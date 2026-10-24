#include "UnboundCollisionFactory.h"
#include "UnboundCollisionFields.h"

#include <algorithm>
#include <climits>
#include <spdlog/spdlog.h>

#include "2s2h/resource/type/CollisionHeader.h"
#include "UnboundJson.h"
#include "UnboundSchema.h"

namespace S2H::Unbound {

namespace {

// Bulk layout shared with SoH: Unbound's collision.bin v3, little-endian, vertices then polygons.
constexpr size_t kBulkVertexSize = 12;
constexpr size_t kBulkPolySize = 28;
constexpr u32 kBulkIndexMask = 0x1FFFFFFF;
constexpr u32 kBulkFlagShift = 29;

// Runtime words retain the bulk format's 29-bit index and 3 flag bits.
constexpr u32 kMaxVertexCount = kBulkIndexMask + 1;
constexpr u32 kVertexFlagShift = 29;

constexpr u32 kWaterBoxAllRooms = 0x3F;

u32 PackField(const Json& entry, const PackedField& field) {
    return ((u32)Field(entry, field.key) & field.mask) << field.shift;
}

const uint8_t* BulkBytes(const std::vector<char>& bulk, size_t offset) {
    return reinterpret_cast<const uint8_t*>(bulk.data() + offset);
}

u16 ReadU16(const std::vector<char>& bulk, size_t offset) {
    const uint8_t* bytes = BulkBytes(bulk, offset);
    return (u16)(bytes[0] | (bytes[1] << 8));
}

u32 ReadU32(const std::vector<char>& bulk, size_t offset) {
    const uint8_t* bytes = BulkBytes(bulk, offset);
    return (u32)bytes[0] | ((u32)bytes[1] << 8) | ((u32)bytes[2] << 16) | ((u32)bytes[3] << 24);
}

u32 ToVertexWord(u32 bulkWord, u32 numVertices, const std::string& where) {
    const u32 index = bulkWord & kBulkIndexMask;
    if (index >= numVertices) {
        throw DocumentError(where + ": polygon vertex " + std::to_string(index) + " is past the vertex list");
    }
    return (u32)(((bulkWord >> kBulkFlagShift) << kVertexFlagShift) | index);
}

SOH::CollisionPoly ReadPoly(const std::vector<char>& bulk, size_t offset, u32 numVertices, const std::string& where) {
    SOH::CollisionPoly poly = {};
    poly.type = ReadU16(bulk, offset);
    poly.flags_vIA = ToVertexWord(ReadU32(bulk, offset + 4), numVertices, where);
    poly.flags_vIB = ToVertexWord(ReadU32(bulk, offset + 8), numVertices, where);
    poly.vIC = ToVertexWord(ReadU32(bulk, offset + 12) & kBulkIndexMask, numVertices, where);
    poly.normal.x = (s16)ReadU16(bulk, offset + 16);
    poly.normal.y = (s16)ReadU16(bulk, offset + 18);
    poly.normal.z = (s16)ReadU16(bulk, offset + 20);
    poly.dist = (s32)ReadU32(bulk, offset + 24);
    return poly;
}

void ReadBulk(SOH::CollisionHeader& header, const Json& bulkInfo, const std::string& where) {
    const std::string file = PathField(bulkInfo, Schema::kFile);
    const int64_t numVertices = Field(bulkInfo, Schema::kVertices);
    const int64_t numPolys = Field(bulkInfo, Schema::kPolys);
    if (numVertices < 0 || numVertices > kMaxVertexCount || numPolys < 0 || numPolys > INT32_MAX) {
        throw DocumentError(where + ": collision counts exceed the 32-bit index range");
    }
    const std::vector<char> bulk = LoadBulk(file);
    const size_t polysOffset = (size_t)numVertices * kBulkVertexSize;
    if (bulk.size() < polysOffset + (size_t)numPolys * kBulkPolySize) {
        throw DocumentError(where + ": bulk file '" + file + "' is shorter than its declared counts");
    }
    header.vertices.reserve((size_t)numVertices);
    for (size_t offset = 0; offset < polysOffset; offset += kBulkVertexSize) {
        header.vertices.push_back(
            { (s32)ReadU32(bulk, offset), (s32)ReadU32(bulk, offset + 4), (s32)ReadU32(bulk, offset + 8) });
    }
    header.polygons.reserve((size_t)numPolys);
    for (int64_t i = 0; i < numPolys; i++) {
        header.polygons.push_back(ReadPoly(bulk, polysOffset + (size_t)i * kBulkPolySize, (u32)numVertices, where));
    }
}

void ReadSurfaceTypes(SOH::CollisionHeader& header, const Json& list, const std::string& where) {
    for (const auto& key : PositionalKeys(list, where + "." + Schema::kSurfaceTypes)) {
        SOH::SurfaceType surface = {};
        const Json& raw = SubArray(list[key], "raw");
        if (raw.size() == 2) { surface.data[0] = ToInt(raw[0]); surface.data[1] = ToInt(raw[1]); }
        for (const PackedField& field : kSurfaceFields) {
            if (list[key].contains(field.key)) {
                surface.data[field.word] = (surface.data[field.word] & ~(field.mask << field.shift)) | PackField(list[key], field);
            }
        }
        header.surfaceTypes.push_back(surface);
    }
}

// Positions are filled first: each camera keeps a pointer into them.
void ReadCameras(SOH::CollisionHeader& header, const Json& doc, const std::string& where) {
    for (const auto& key : PositionalKeys(Sub(doc, Schema::kCameraPositions), where + "." + Schema::kCameraPositions)) {
        header.camPosData.push_back(ReadVec3s(Sub(doc, Schema::kCameraPositions)[key]));
    }
    header.camPosDataZero = { 0, 0, 0 };
    const Json& cameras = Sub(doc, Schema::kCameras);
    for (const auto& key : PositionalKeys(cameras, where + "." + Schema::kCameras)) {
        const Json& entry = cameras[key];
        const int64_t positionIndex = Field(entry, Schema::kPositionIndex, -1);
        const bool hasPosition = positionIndex >= 0 && positionIndex < (int64_t)header.camPosData.size();
        SOH::CamData camera = {};
        camera.cameraSType = (u16)Field(entry, Schema::kSetting);
        camera.numCameras = (s16)Field(entry, Schema::kCount);
        camera.camPosData = hasPosition ? &header.camPosData[positionIndex] : &header.camPosDataZero;
        header.camData.push_back(camera);
    }
}

u32 WaterBoxProperties(const Json& entry) {
    const int64_t room = Field(entry, Schema::kRoom, -1);
    const u32 roomBits = room < 0 ? kWaterBoxAllRooms : (u32)room & kWaterBoxAllRooms;
    return ((u32)Field(entry, Schema::kCamera) & 0xFF) | (((u32)Field(entry, Schema::kLightSetting) & 0x1F) << 8) |
           (roomBits << 13) | (((u32)Field(entry, Schema::kNotSwimmable) & 1) << 19);
}

void ReadWaterBoxes(SOH::CollisionHeader& header, const Json& list, const std::string& where) {
    const std::vector<std::string> keys = PositionalKeys(list, where + "." + Schema::kWaterBoxes);
    if (keys.size() > INT32_MAX) {
        throw DocumentError(where + ": too many water boxes");
    }
    for (const auto& key : keys) {
        const Json& entry = list[key];
        const int64_t room = Field(entry, Schema::kRoom, -1);
        if (room < -1 || room > INT16_MAX) {
            throw DocumentError(where + ": invalid water box room");
        }
        header.waterBoxes.push_back({ IntegralField(entry, Schema::kXMin), IntegralField(entry, Schema::kYSurface),
                                      IntegralField(entry, Schema::kZMin), IntegralField(entry, Schema::kXLength),
                                      IntegralField(entry, Schema::kZLength), WaterBoxProperties(entry), (s32)Field(entry, Schema::kRoom, -1) });
    }
}

void ComputeBounds(SOH::CollisionHeaderData& data, const std::vector<Vec3i>& vertices) {
    data.minBounds = { INT_MAX, INT_MAX, INT_MAX };
    data.maxBounds = { INT_MIN, INT_MIN, INT_MIN };
    for (const Vec3i& vertex : vertices) {
        data.minBounds = { std::min(data.minBounds.x, vertex.x), std::min(data.minBounds.y, vertex.y),
                           std::min(data.minBounds.z, vertex.z) };
        data.maxBounds = { std::max(data.maxBounds.x, vertex.x), std::max(data.maxBounds.y, vertex.y),
                           std::max(data.maxBounds.z, vertex.z) };
    }
}

void ReadBounds(SOH::CollisionHeader& header, const Json& doc) {
    const Json& bounds = Sub(doc, Schema::kBounds);
    if (bounds.empty() && !header.vertices.empty()) {
        ComputeBounds(header.collisionHeaderData, header.vertices);
        return;
    }
    header.collisionHeaderData.minBounds = ReadVec3i(bounds.value(Schema::kMin, Json()));
    header.collisionHeaderData.maxBounds = ReadVec3i(bounds.value(Schema::kMax, Json()));
}

void PublishLists(SOH::CollisionHeader& header) {
    SOH::CollisionHeaderData& data = header.collisionHeaderData;
    header.surfaceTypesCount = (uint32_t)header.surfaceTypes.size();
    header.camDataCount = (uint32_t)header.camData.size();
    header.camPosCount = (int32_t)header.camPosData.size();
    data.numVertices = (u32)header.vertices.size();
    data.vtxList = header.vertices.data();
    data.numPolygons = (u32)header.polygons.size();
    data.polyList = header.polygons.data();
    data.surfaceTypeList = header.surfaceTypes.data();
    data.cameraDataList = header.camData.data();
    data.cameraDataListLen = header.camDataCount;
    data.numWaterBoxes = (u32)header.waterBoxes.size();
    data.waterBoxes = header.waterBoxes.data();
}

std::shared_ptr<SOH::CollisionHeader> BuildCollision(std::shared_ptr<Ship::ResourceInitData> initData,
                                                     const Json& doc) {
    const std::string& where = initData->Path;
    auto header = std::make_shared<SOH::CollisionHeader>(initData);
    ReadBulk(*header, Sub(doc, Schema::kBulk), where);
    ReadSurfaceTypes(*header, Sub(doc, Schema::kSurfaceTypes), where);
    ReadCameras(*header, doc, where);
    ReadWaterBoxes(*header, Sub(doc, Schema::kWaterBoxes), where);
    ReadBounds(*header, doc);
    PublishLists(*header);
    return header;
}

} // namespace

std::shared_ptr<Ship::IResource>
ResourceFactoryJsonCollisionV1::ReadResource(std::shared_ptr<Ship::File> file,
                                             std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }
    try {
        return BuildCollision(initData, LoadMergedJson(initData->Path));
    } catch (const std::exception& error) {
        SPDLOG_ERROR("[Unbound] {}", error.what());
        return nullptr;
    }
}

} // namespace S2H::Unbound
