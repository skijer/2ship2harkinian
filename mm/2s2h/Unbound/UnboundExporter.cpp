#include "UnboundExporter.h"
#include "UnboundCollisionFields.h"
#include "2s2h/resource/type/TextMM.h"
#include "2s2h/resource/type/CollisionHeader.h"
#include "2s2h/resource/type/Path.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <spdlog/spdlog.h>

#include "2s2h/BenPort.h"
#include "2s2h/resource/type/Scene.h"
#include "2s2h/resource/type/scenecommand/SetActorCutsceneList.h"
#include "2s2h/resource/type/scenecommand/SetActorList.h"
#include "2s2h/resource/type/scenecommand/SetAlternateHeaders.h"
#include "2s2h/resource/type/scenecommand/SetAnimatedMaterialList.h"
#include "2s2h/resource/type/scenecommand/SetCollisionHeader.h"
#include "2s2h/resource/type/scenecommand/SetCsCamera.h"
#include "2s2h/resource/type/scenecommand/SetCutscenes.h"
#include "2s2h/resource/type/scenecommand/SetEchoSettings.h"
#include "2s2h/resource/type/scenecommand/SetEntranceList.h"
#include "2s2h/resource/type/scenecommand/SetExitList.h"
#include "2s2h/resource/type/scenecommand/SetLightList.h"
#include "2s2h/resource/type/scenecommand/SetLightingSettings.h"
#include "2s2h/resource/type/scenecommand/SetMesh.h"
#include "2s2h/resource/type/scenecommand/SetMinimapChests.h"
#include "2s2h/resource/type/scenecommand/SetMinimapList.h"
#include "2s2h/resource/type/scenecommand/SetObjectList.h"
#include "2s2h/resource/type/scenecommand/SetPathways.h"
#include "2s2h/resource/type/scenecommand/SetRoomBehavior.h"
#include "2s2h/resource/type/scenecommand/SetRoomList.h"
#include "2s2h/resource/type/scenecommand/SetSkyboxModifier.h"
#include "2s2h/resource/type/scenecommand/SetSkyboxSettings.h"
#include "2s2h/resource/type/scenecommand/SetSoundSettings.h"
#include "2s2h/resource/type/scenecommand/SetSpecialObjects.h"
#include "2s2h/resource/type/scenecommand/SetStartPositionList.h"
#include "2s2h/resource/type/scenecommand/SetTimeSettings.h"
#include "2s2h/resource/type/scenecommand/SetTransitionActorList.h"
#include "2s2h/resource/type/scenecommand/SetWindSettings.h"
#include "UnboundJson.h"
#include "UnboundSchema.h"

extern "C" {
#include "variables.h"
#include "build.h"
}

namespace S2H::Unbound {

namespace {

namespace K = Schema;
using SOH::SceneCommandID;

// Bump whenever the converter's output changes: every existing base is then converted again.
constexpr int kConverterRevision = 2;
constexpr const char* kOtrPrefix = "__OTR__";

// Stored-entry zip, deterministic (fixed 1980-01-01 timestamps), no ZIP64: past 65 535 entries or 4 GB it refuses.
class ZipWriter {
  public:
    bool Open(const std::string& path) {
        mOut.open(path, std::ios::binary | std::ios::trunc);
        return mOut.good();
    }

    bool Add(const std::string& name, const std::string& text) {
        if (!mError.empty()) {
            return false;
        }
        const uint64_t offset = (uint64_t)mOut.tellp();
        if (mEntries.size() >= 0xFFFF || offset + text.size() + name.size() + 30 > 0xFFFFFFFFull) {
            mError = "zip limit exceeded at entry " + name;
            return false;
        }
        Entry entry = { name, Crc32((const uint8_t*)text.data(), text.size()), (uint32_t)text.size(),
                        (uint32_t)offset };
        WriteLocalHeader(entry);
        mOut.write(name.data(), name.size());
        mOut.write(text.data(), text.size());
        mEntries.push_back(std::move(entry));
        return true;
    }

    bool Close() {
        const uint32_t directoryOffset = (uint32_t)mOut.tellp();
        for (const Entry& entry : mEntries) {
            WriteDirectoryEntry(entry);
        }
        const uint32_t directorySize = (uint32_t)mOut.tellp() - directoryOffset;
        Put32(0x06054b50);
        Put16(0);
        Put16(0);
        Put16((uint16_t)mEntries.size());
        Put16((uint16_t)mEntries.size());
        Put32(directorySize);
        Put32(directoryOffset);
        Put16(0);
        mOut.close();
        if (mOut.fail() && mError.empty()) {
            mError = "write failed";
        }
        return mError.empty();
    }

    const std::string& Error() const {
        return mError;
    }

  private:
    struct Entry {
        std::string name;
        uint32_t crc;
        uint32_t size;
        uint32_t offset;
    };

    void WriteLocalHeader(const Entry& entry) {
        Put32(0x04034b50);
        Put16(20);
        Put16(0x0800);
        Put16(0);
        Put16(0);
        Put16(0x21);
        Put32(entry.crc);
        Put32(entry.size);
        Put32(entry.size);
        Put16((uint16_t)entry.name.size());
        Put16(0);
    }

    void WriteDirectoryEntry(const Entry& entry) {
        Put32(0x02014b50);
        Put16(20);
        Put16(20);
        Put16(0x0800);
        Put16(0);
        Put16(0);
        Put16(0x21);
        Put32(entry.crc);
        Put32(entry.size);
        Put32(entry.size);
        Put16((uint16_t)entry.name.size());
        Put16(0);
        Put16(0);
        Put16(0);
        Put16(0);
        Put32(0);
        Put32(entry.offset);
        mOut.write(entry.name.data(), entry.name.size());
    }

    void Put16(uint16_t value) {
        const char bytes[2] = { (char)(value & 0xFF), (char)(value >> 8) };
        mOut.write(bytes, 2);
    }

    void Put32(uint32_t value) {
        const char bytes[4] = { (char)(value & 0xFF), (char)((value >> 8) & 0xFF), (char)((value >> 16) & 0xFF),
                                (char)(value >> 24) };
        mOut.write(bytes, 4);
    }

    static uint32_t Crc32(const uint8_t* data, size_t size) {
        static uint32_t table[256];
        static bool initialized = false;
        if (!initialized) {
            for (uint32_t i = 0; i < 256; i++) {
                uint32_t c = i;
                for (int k = 0; k < 8; k++) {
                    c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                }
                table[i] = c;
            }
            initialized = true;
        }
        uint32_t c = 0xFFFFFFFFu;
        for (size_t i = 0; i < size; i++) {
            c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
        }
        return c ^ 0xFFFFFFFFu;
    }

    std::ofstream mOut;
    std::vector<Entry> mEntries;
    std::string mError;
};

struct ExportContext {
    ZipWriter zip;
    ExportReport report;
    std::string sceneDir;
    std::map<std::string, std::string> roomDocuments;
    std::map<std::string, std::string> resourceDocuments;
};

std::shared_ptr<Ship::ArchiveManager> Archives() {
    return Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
}

std::string StripOtrPrefix(const void* path) {
    if (path == nullptr) {
        return "";
    }
    const std::string text = (const char*)path;
    return text.rfind(kOtrPrefix, 0) == 0 ? text.substr(7) : text;
}

std::string Leaf(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string Hex(uint32_t value) {
    char text[16];
    snprintf(text, sizeof(text), "%08X", value);
    return text;
}

Json Vec(s16 x, s16 y, s16 z) {
    return Json::array({ x, y, z });
}

Json Coordinate(f32 value) {
    const f32 whole = std::round(value);
    if (whole == value) {
        return (int64_t)whole;
    }
    return value;
}

Json Pos(const Vec3f& pos) {
    return Json::array({ Coordinate(pos.x), Coordinate(pos.y), Coordinate(pos.z) });
}

template <typename T> Json Rgb(const T* values) {
    return Json::array({ values[0], values[1], values[2] });
}

template <typename T> std::shared_ptr<T> As(const std::shared_ptr<SOH::ISceneCommand>& command) {
    return std::static_pointer_cast<T>(command);
}

Json ActorsJson(const std::vector<SOH::ActorEntry>& actors) {
    Json list = Json::object();
    for (size_t i = 0; i < actors.size(); i++) {
        const SOH::ActorEntry& actor = actors[i];
        list[std::to_string(i)] = { { K::kId, actor.id },
                                    { K::kPos, Pos(actor.pos) },
                                    { K::kRot, Vec(actor.rot.x, actor.rot.y, actor.rot.z) },
                                    { K::kParams, (u16)actor.params } };
    }
    return list;
}

Json CsCamerasJson(const SOH::SetCsCamera& command) {
    Json list = Json::object();
    for (size_t i = 0; i < command.csCamera.size(); i++) {
        const SOH::ActorCsCamInfoData& camera = command.csCamera[i];
        Json points = Json::array();
        for (s16 p = 0; p < camera.count; p++) {
            const SOH::z64Vec3s& point = camera.actorCsCamFuncData[p];
            points.push_back(Vec(point.x, point.y, point.z));
        }
        list[std::to_string(i)] = { { K::kSetting, camera.setting }, { K::kPoints, points } };
    }
    return list;
}

Json MeshEntriesJson(const SOH::SetMesh& mesh) {
    Json entries = Json::object();
    if (mesh.meshHeader.base.type == 2) {
        for (size_t i = 0; i < mesh.dlists2.size(); i++) {
            const SOH::PolygonDlist2& dlist = mesh.dlists2[i];
            entries[std::to_string(i)] = { { K::kPos, Pos(dlist.pos) },
                                           { K::kRadius, dlist.unk_06 },
                                           { K::kOpa, StripOtrPrefix(dlist.opa) },
                                           { K::kXlu, StripOtrPrefix(dlist.xlu) } };
        }
        return entries;
    }
    for (size_t i = 0; i < mesh.dlists.size(); i++) {
        entries[std::to_string(i)] = { { K::kOpa, StripOtrPrefix(mesh.dlists[i].opa) },
                                       { K::kXlu, StripOtrPrefix(mesh.dlists[i].xlu) } };
    }
    return entries;
}

Json BgImageJson(const SOH::BgImage& image) {
    return { { K::kUnk00, image.unk_00 }, { K::kId, image.id },         { K::kSource, StripOtrPrefix(image.source) },
             { K::kUnk0C, image.unk_0C }, { K::kTlut, image.tlut },     { K::kWidth, image.width },
             { K::kHeight, image.height }, { K::kFmt, image.fmt },      { K::kSiz, image.siz },
             { K::kMode0, image.mode0 },  { K::kTlutCount, image.tlutCount } };
}

Json PrerenderedMeshJson(const SOH::SetMesh& mesh) {
    const auto& polygon = mesh.meshHeader.polygon1;
    Json images = Json::object();
    if (polygon.format == 1) {
        SOH::BgImage single = {};
        single.source = polygon.single.source;
        single.unk_0C = polygon.single.unk_0C;
        single.tlut = (u32)(uintptr_t)polygon.single.tlut;
        single.width = polygon.single.width;
        single.height = polygon.single.height;
        single.fmt = polygon.single.fmt;
        single.siz = polygon.single.siz;
        single.mode0 = polygon.single.mode0;
        single.tlutCount = polygon.single.tlutCount;
        images["0"] = BgImageJson(single);
    } else {
        for (size_t i = 0; i < mesh.images.size(); i++) {
            images[std::to_string(i)] = BgImageJson(mesh.images[i]);
        }
    }
    const bool hasDlist = !mesh.dlists.empty();
    return { { K::kType, 1 },
             { K::kFormat, polygon.format },
             { K::kImages, images },
             { K::kOpa, hasDlist ? StripOtrPrefix(mesh.dlists[0].opa) : "" },
             { K::kXlu, hasDlist ? StripOtrPrefix(mesh.dlists[0].xlu) : "" } };
}

Json MeshJson(const SOH::SetMesh& mesh) {
    if (mesh.meshHeader.base.type == 1) {
        return PrerenderedMeshJson(mesh);
    }
    return { { K::kType, mesh.meshHeader.base.type }, { K::kEntries, MeshEntriesJson(mesh) } };
}

Json LightsJson(const SOH::SetLightList& command) {
    Json list = Json::object();
    for (size_t i = 0; i < command.lightList.size(); i++) {
        const auto& point = command.lightList[i].params.point;
        list[std::to_string(i)] = { { K::kType, command.lightList[i].type },
                                    { K::kPos, Pos(SOH::LightInfo_GetCoords(command.lightList[i])) },
                                    { K::kColor, Rgb(point.color) },
                                    { K::kGlow, point.drawGlow },
                                    { K::kRadius, point.radius } };
    }
    return list;
}

Json TransitionActorsJson(const SOH::SetTransitionActorList& command) {
    Json list = Json::object();
    for (size_t i = 0; i < command.transitionActorList.size(); i++) {
        const SOH::TransitionActorEntry& actor = command.transitionActorList[i];
        list[std::to_string(i)] = {
            { K::kId, actor.id },
            { K::kPos, Pos(actor.pos) },
            { K::kRotY, actor.rotY },
            { K::kParams, (u16)actor.params },
            { K::kFront, { { K::kRoom, actor.sides[0].room }, { K::kEffects, actor.sides[0].effects } } },
            { K::kBack, { { K::kRoom, actor.sides[1].room }, { K::kEffects, actor.sides[1].effects } } },
        };
    }
    return list;
}

Json LightingJson(const SOH::SetLightingSettings& command) {
    Json list = Json::object();
    for (size_t i = 0; i < command.settings.size(); i++) {
        const SOH::EnvLightSettings& light = command.settings[i];
        list[std::to_string(i)] = { { K::kAmbient, Rgb(light.ambientColor) },
                                    { K::kLight1Dir, Rgb(light.light1Dir) },
                                    { K::kLight1Color, Rgb(light.light1Color) },
                                    { K::kLight2Dir, Rgb(light.light2Dir) },
                                    { K::kLight2Color, Rgb(light.light2Color) },
                                    { K::kFogColor, Rgb(light.fogColor) },
                                    { K::kFogNear, light.fogNear },
                                    { K::kFogFar, light.fogFar } };
    }
    return list;
}

Json CutscenesJson(const SOH::SetCutscenesMM& command) {
    Json list = Json::object();
    for (size_t i = 0; i < command.entries.size(); i++) {
        const SOH::CutsceneScriptEntry& script = command.entries[i];
        list[std::to_string(i)] = { { K::kScript, i < command.scriptPaths.size() ? command.scriptPaths[i] : "" },
                                    { K::kExit, script.exit },
                                    { K::kEntrance, script.entrance },
                                    { K::kFlag, script.flag } };
    }
    return list;
}

Json ActorCutscenesJson(const SOH::SetActorCutsceneList& command) {
    Json list = Json::object();
    for (size_t i = 0; i < command.entries.size(); i++) {
        const SOH::CutsceneEntry& cutscene = command.entries[i];
        list[std::to_string(i)] = { { K::kPriority, cutscene.priority },
                                    { K::kLength, cutscene.length },
                                    { K::kCsCamId, cutscene.csCamId },
                                    { K::kScriptIndex, cutscene.scriptIndex },
                                    { K::kAdditionalCsId, cutscene.additionalCsId },
                                    { K::kEndSfx, cutscene.endSfx },
                                    { K::kCustomValue, cutscene.customValue },
                                    { K::kHudVisibility, cutscene.hudVisibility },
                                    { K::kEndCam, cutscene.endCam },
                                    { K::kLetterboxSize, cutscene.letterboxSize } };
    }
    return list;
}

Json MinimapJson(const SOH::SetMinimapList& command) {
    Json entries = Json::object();
    for (size_t i = 0; i < command.entries.size(); i++) {
        const SOH::MinimapEntryData& entry = command.entries[i];
        entries[std::to_string(i)] = { { K::kMapId, entry.mapId }, { K::kUnk2, entry.unk2 }, { K::kUnk4, entry.unk4 },
                                       { K::kUnk6, entry.unk6 },   { K::kUnk8, entry.unk8 } };
    }
    return { { K::kScale, command.list.scale }, { K::kEntries, entries } };
}

Json MinimapChestsJson(const SOH::SetMinimapChests& command) {
    Json list = Json::object();
    for (size_t i = 0; i < command.chests.size(); i++) {
        const SOH::MinimapChestData& chest = command.chests[i];
        list[std::to_string(i)] = { { K::kUnk0, chest.unk_0 }, { K::kUnk2, chest.unk_2 }, { K::kUnk4, chest.unk_4 },
                                    { K::kUnk6, chest.unk_6 }, { K::kUnk8, chest.unk_8 } };
    }
    return list;
}

bool ConvertRoom(ExportContext& ctx, const std::string& roomPath, std::string& documentPath);

std::string ConvertCollision(ExportContext& ctx, const std::string& source) {
    const std::string key = "collision:" + source;
    if (ctx.resourceDocuments.contains(key)) { return ctx.resourceDocuments[key]; }
    const auto resource = std::dynamic_pointer_cast<SOH::CollisionHeader>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(source));
    if (!resource) { ctx.report.failures++; return source; }
    const std::string path = ctx.sceneDir + "/collision-" + std::to_string(ctx.resourceDocuments.size());
    std::string bulk;
    auto word = [&](uint32_t value, size_t size) {
        for (size_t i = 0; i < size; i++) { bulk.push_back((value >> (8 * i)) & 255); }
    };
    for (const auto& vertex : resource->vertices) { word(vertex.x, 4); word(vertex.y, 4); word(vertex.z, 4); }
    for (const auto& polygon : resource->polygons) {
        word(polygon.type, 2); word(0, 2);
        word(polygon.flags_vIA, 4); word(polygon.flags_vIB, 4); word(polygon.vIC, 4);
        word(polygon.normal.x, 2); word(polygon.normal.y, 2); word(polygon.normal.z, 2); word(0, 2);
        word(polygon.dist, 4);
    }
    Json document = { { "$schema", "2ship-unbound/collision/1" },
        { "bulk", { { "file", path + ".bin" }, { "vertices", resource->vertices.size() }, { "polys", resource->polygons.size() } } },
        { "surfaceTypes", Json::object() }, { "cameraPositions", Json::object() },
        { "cameras", Json::object() }, { "waterBoxes", Json::object() } };
    const auto& bounds = resource->collisionHeaderData;
    document["bounds"] = {
        { "min", Json::array({ bounds.minBounds.x, bounds.minBounds.y, bounds.minBounds.z }) },
        { "max", Json::array({ bounds.maxBounds.x, bounds.maxBounds.y, bounds.maxBounds.z }) }
    };
    for (size_t i = 0; i < resource->surfaceTypes.size(); i++) {
        const auto& surface = resource->surfaceTypes[i];
        document["surfaceTypes"][std::to_string(i)] = { { "raw", Json::array({ surface.data[0], surface.data[1] }) } };
        for (const PackedField& field : kSurfaceFields) {
            document["surfaceTypes"][std::to_string(i)][field.key] = (surface.data[field.word] >> field.shift) & field.mask;
        }
    }
    for (size_t i = 0; i < resource->camPosData.size(); i++) {
        const auto& position = resource->camPosData[i];
        document["cameraPositions"][std::to_string(i)] = Json::array({ position.x, position.y, position.z });
    }
    for (size_t i = 0; i < resource->camData.size(); i++) {
        const auto& camera = resource->camData[i];
        const uintptr_t pointer = (uintptr_t)camera.camPosData;
        const uintptr_t start = (uintptr_t)resource->camPosData.data();
        const int64_t index = pointer >= start && pointer < start + resource->camPosData.size() * sizeof(Vec3s) ?
            (pointer - start) / sizeof(Vec3s) : -1;
        document["cameras"][std::to_string(i)] = { { "setting", camera.cameraSType }, { "count", camera.numCameras }, { "positionIndex", index } };
    }
    for (size_t i = 0; i < resource->waterBoxes.size(); i++) {
        const auto& water = resource->waterBoxes[i];
        document["waterBoxes"][std::to_string(i)] = {
            { "xMin", water.xMin }, { "ySurface", water.ySurface }, { "zMin", water.zMin },
            { "xLength", water.xLength }, { "zLength", water.zLength }, { "room", water.room },
            { "camera", water.properties & 255 }, { "lightSetting", (water.properties >> 8) & 31 },
            { "notSwimmable", (water.properties >> 19) & 1 }
        };
    }
    if (!ctx.zip.Add(path + ".bin", bulk) || !ctx.zip.Add(path + ".json", document.dump(2))) { ctx.report.failures++; }
    return ctx.resourceDocuments[key] = path + ".json";
}
Json ConvertPaths(ExportContext& ctx, const std::vector<std::string>& sources) {
    Json paths = Json::array();
    for (const auto& source : sources) {
        const std::string key = "path:" + source;
        if (ctx.resourceDocuments.contains(key)) { paths.push_back(ctx.resourceDocuments[key]); continue; }
        const auto resource = std::dynamic_pointer_cast<SOH::PathMM>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(source));
        if (!resource) { ctx.report.failures++; paths.push_back(source); continue; }
        Json document = { { "$schema", "2ship-unbound/path/1" }, { "paths", Json::object() } };
        for (size_t i = 0; i < resource->pathData.size(); i++) {
            const auto& path = resource->pathData[i];
            Json points = Json::array();
            for (size_t j = 0; j < path.count; j++) { points.push_back(Json::array({ path.points[j].x, path.points[j].y, path.points[j].z })); }
            document["paths"][std::to_string(i)] = { { "points", points }, { "additionalPathIndex", path.additionalPathIndex }, { "customValue", path.customValue } };
        }
        const std::string path = ctx.sceneDir + "/paths-" + std::to_string(ctx.resourceDocuments.size()) + ".json";
        if (!ctx.zip.Add(path, document.dump(2))) { ctx.report.failures++; }
        paths.push_back(path);
        ctx.resourceDocuments[key] = path;
    }
    return paths;
}
Json RoomsJson(ExportContext& ctx, const SOH::SetRoomList& command) {
    Json list = Json::object();
    for (size_t i = 0; i < command.fileNames.size(); i++) {
        std::string documentPath;
        if (ConvertRoom(ctx, command.fileNames[i], documentPath)) {
            list[std::to_string(i)] = documentPath;
        }
    }
    return list;
}

void AddCommand(ExportContext& ctx, Json& setup, const std::shared_ptr<SOH::ISceneCommand>& command) {
    switch (command->cmdId) {
        case SceneCommandID::SetStartPositionList:
            setup[K::kSpawns] = ActorsJson(As<SOH::SetStartPositionList>(command)->startPositions);
            break;
        case SceneCommandID::SetActorList:
            setup[K::kActors] = ActorsJson(As<SOH::SetActorList>(command)->actorList);
            break;
        case SceneCommandID::SetCsCamera:
            setup[K::kActorCsCameras] = CsCamerasJson(*As<SOH::SetCsCamera>(command));
            break;
        case SceneCommandID::SetCollisionHeader:
            setup[K::kCollision] = ConvertCollision(ctx, As<SOH::SetCollisionHeader>(command)->fileName);
            break;
        case SceneCommandID::SetRoomList:
            setup[K::kRooms] = RoomsJson(ctx, *As<SOH::SetRoomList>(command));
            break;
        case SceneCommandID::SetWind: {
            const auto& wind = As<SOH::SetWindSettings>(command)->settings;
            setup[K::kWind] = { { K::kWest, wind.windWest },
                                { K::kVertical, wind.windVertical },
                                { K::kSouth, wind.windSouth },
                                { K::kSpeed, wind.windSpeed } };
            break;
        }
        case SceneCommandID::SetEntranceList: {
            Json list = Json::object();
            const auto& entrances = As<SOH::SetEntranceList>(command)->entrances;
            for (size_t i = 0; i < entrances.size(); i++) {
                list[std::to_string(i)] = { { K::kSpawn, entrances[i].spawn }, { K::kRoom, entrances[i].room } };
            }
            setup[K::kEntrances] = list;
            break;
        }
        case SceneCommandID::SetSpecialObjects: {
            const auto& special = As<SOH::SetSpecialObjects>(command)->specialObjects;
            setup[K::kSpecialObjects] = { { K::kElfMessage, special.elfMessage },
                                          { K::kGlobalObject, special.globalObject } };
            break;
        }
        case SceneCommandID::SetRoomBehavior: {
            const auto& behavior = As<SOH::SetRoomBehaviorMM>(command)->roomBehavior;
            setup[K::kBehavior] = { { K::kGameplayFlags, behavior.gameplayFlags },
                                    { K::kCurrRoomUnk2, behavior.currRoomUnk2 },
                                    { K::kCurrRoomUnk5, behavior.currRoomUnk5 },
                                    { K::kMsgCtxUnk, behavior.msgCtxUnk },
                                    { K::kEnablePointLights, behavior.enablePointLights },
                                    { K::kKankyoContextUnkE2, behavior.kankyoContextUnkE2 } };
            break;
        }
        case SceneCommandID::SetMesh:
            setup[K::kMesh] = MeshJson(*As<SOH::SetMesh>(command));
            break;
        case SceneCommandID::SetObjectList:
            setup[K::kObjects] = As<SOH::SetObjectList>(command)->objects;
            break;
        case SceneCommandID::SetLightList:
            setup[K::kLights] = LightsJson(*As<SOH::SetLightList>(command));
            break;
        case SceneCommandID::SetPathways:
            setup[K::kPaths] = ConvertPaths(ctx, As<SOH::SetPathwaysMM>(command)->fileNames);
            break;
        case SceneCommandID::SetTransitionActorList:
            setup[K::kTransitionActors] = TransitionActorsJson(*As<SOH::SetTransitionActorList>(command));
            break;
        case SceneCommandID::SetLightingSettings:
            setup[K::kLighting] = LightingJson(*As<SOH::SetLightingSettings>(command));
            break;
        case SceneCommandID::SetTimeSettings: {
            const auto& time = As<SOH::SetTimeSettings>(command)->settings;
            setup[K::kTime] = { { K::kHour, time.hour }, { K::kMinute, time.minute }, { K::kIncrement, time.timeIncrement } };
            break;
        }
        case SceneCommandID::SetSkyboxSettings: {
            const auto& skybox = As<SOH::SetSkyboxSettings>(command)->settings;
            setup[K::kSkybox] = { { K::kUnk, skybox.unk },
                                  { K::kId, skybox.skyboxId },
                                  { K::kWeather, skybox.weather },
                                  { K::kIndoors, skybox.indoors } };
            break;
        }
        case SceneCommandID::SetSkyboxModifier: {
            const auto& modifier = As<SOH::SetSkyboxModifier>(command)->modifier;
            setup[K::kSkyboxModifier] = { { K::kSkyboxDisabled, modifier.skyboxDisabled },
                                          { K::kSunMoonDisabled, modifier.sunMoonDisabled } };
            break;
        }
        case SceneCommandID::SetExitList:
            setup[K::kExits] = As<SOH::SetExitList>(command)->exits;
            break;
        case SceneCommandID::SetSoundSettings: {
            const auto& sound = As<SOH::SetSoundSettings>(command)->settings;
            setup[K::kSound] = { { K::kSeq, sound.seqId },
                                 { K::kNatureAmbience, sound.natureAmbienceId },
                                 { K::kReverb, sound.reverb } };
            break;
        }
        case SceneCommandID::SetEchoSettings:
            setup[K::kEcho] = As<SOH::SetEchoSettings>(command)->settings.echo;
            break;
        case SceneCommandID::SetCutscenesMM:
            setup[K::kCutscenes] = CutscenesJson(*As<SOH::SetCutscenesMM>(command));
            break;
        case SceneCommandID::SetWorldMapVisited:
            setup[K::kRegionVisited] = true;
            break;
        case SceneCommandID::SetAnimatedMaterialList:
            setup[K::kAnimatedMaterials] = As<SOH::SetAnimatedMaterialList>(command)->fileName;
            break;
        case SceneCommandID::SetActorCutsceneList:
            setup[K::kActorCutscenes] = ActorCutscenesJson(*As<SOH::SetActorCutsceneList>(command));
            break;
        case SceneCommandID::SetMinimapList:
            setup[K::kMinimap] = MinimapJson(*As<SOH::SetMinimapList>(command));
            break;
        case SceneCommandID::SetMinimapChests:
            setup[K::kMinimapChests] = MinimapChestsJson(*As<SOH::SetMinimapChests>(command));
            break;
        case SceneCommandID::SetAlternateHeaders:
        case SceneCommandID::EndMarker:
            break;
        default:
            SPDLOG_WARN("[Unbound export] {}: scene command 0x{:X} has no JSON form; dropped", ctx.sceneDir,
                        (int)command->cmdId);
            break;
    }
}

// Each command adds at most one key; the keys in the order they appear keep the engine's command order.
Json SetupJson(ExportContext& ctx, const SOH::Scene& scene) {
    Json setup = Json::object();
    Json order = Json::array();
    for (const auto& command : scene.commands) {
        if (command == nullptr) {
            continue;
        }
        const Json before = setup;
        AddCommand(ctx, setup, command);
        for (const auto& [key, value] : setup.items()) {
            if (!before.contains(key)) {
                order.push_back(key);
            }
        }
    }
    setup[K::kCommandOrder] = order;
    return setup;
}

Json SetupsJson(ExportContext& ctx, const SOH::Scene& scene) {
    Json setups = Json::object();
    setups["0"] = SetupJson(ctx, scene);
    for (const auto& command : scene.commands) {
        if (command == nullptr || command->cmdId != SceneCommandID::SetAlternateHeaders) {
            continue;
        }
        const auto& headers = As<SOH::SetAlternateHeaders>(command)->headers;
        for (size_t i = 0; i < headers.size(); i++) {
            if (headers[i] != nullptr) {
                setups[std::to_string(i + 1)] = SetupJson(ctx, *headers[i]);
            }
        }
    }
    return setups;
}

std::shared_ptr<SOH::Scene> LoadVanillaScene(const std::string& path) {
    return std::dynamic_pointer_cast<SOH::Scene>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(path.c_str()));
}

bool ConvertRoom(ExportContext& ctx, const std::string& roomPath, std::string& documentPath) {
    auto converted = ctx.roomDocuments.find(roomPath);
    if (converted != ctx.roomDocuments.end()) {
        documentPath = converted->second;
        return true;
    }
    auto room = LoadVanillaScene(roomPath);
    if (room == nullptr) {
        SPDLOG_ERROR("[Unbound export] room {} could not be loaded", roomPath);
        ctx.report.failures++;
        return false;
    }
    documentPath = ctx.sceneDir + "/rooms/" + Leaf(roomPath) + ".json";
    ctx.roomDocuments[roomPath] = documentPath;
    const Json doc = { { K::kSchema, K::kRoomV1 }, { K::kSetups, SetupsJson(ctx, *room) } };
    ctx.zip.Add(documentPath, doc.dump(2));
    ctx.report.rooms++;
    return true;
}

void ConvertScene(ExportContext& ctx, const char* fileName) {
    const std::string vanillaPath = std::string("scenes/nonmq/") + fileName + "/" + fileName;
    auto scene = LoadVanillaScene(vanillaPath);
    if (scene == nullptr) {
        SPDLOG_ERROR("[Unbound export] scene {} could not be loaded", vanillaPath);
        ctx.report.failures++;
        return;
    }
    ctx.sceneDir = std::string("scenes/") + fileName;
    ctx.roomDocuments.clear();
    const Json doc = { { K::kSchema, K::kSceneV1 }, { K::kSetups, SetupsJson(ctx, *scene) } };
    ctx.zip.Add(ctx.sceneDir + "/scene.json", doc.dump(2));
    ctx.report.scenes++;
}

void ConvertAllScenes(ExportContext& ctx) {
    for (s16 sceneId = 0; sceneId < SCENE_MAX; sceneId++) {
        const char* fileName = gSceneTable[sceneId].segment.fileName;
        if (fileName != nullptr) {
            ConvertScene(ctx, fileName);
        }
    }
}

Json CurrentProvenance() {
    std::vector<uint32_t> versions = Archives()->GetGameVersions();
    std::sort(versions.begin(), versions.end());
    versions.erase(std::unique(versions.begin(), versions.end()), versions.end());
    Json hashes = Json::array();
    for (uint32_t version : versions) {
        hashes.push_back(Hex(version));
    }
    return { { K::kRomHashes, hashes }, { K::kConverter, ConverterName() } };
}

void WriteManifest(ExportContext& ctx) {
    const Json doc = { { K::kFormatName, K::kFormatValue },
                       { K::kFormatVersion, K::kCurrentFormatVersion },
                       { K::kGame, "mm" },
                       { K::kFeatures, Json::array({ K::kFeatureScenes, "collision", "paths", "text" }) },
                       { K::kSourceInfo, CurrentProvenance() } };
    ctx.zip.Add(K::kManifestPath, doc.dump(2));
}

Json MountedManifestSource() {
    return Sub(LoadMergedJson(K::kManifestPath), K::kSourceInfo);
}

bool IsCurrent(const Json& source) {
    const Json current = CurrentProvenance();
    return PathField(source, K::kConverter) == PathField(current, K::kConverter) &&
           SubArray(source, K::kRomHashes) == SubArray(current, K::kRomHashes);
}

std::string BaseArchivePath() {
    const std::string gameArchive = Ship::Context::LocateFileAcrossAppDirs("mm.o2r", appShortName);
    const std::filesystem::path directory = std::filesystem::path(gameArchive).parent_path();
    return (directory / kBaseArchiveName).string();
}

bool MountExisting(const std::string& path) {
    if (!std::filesystem::exists(path) || Archives()->AddArchive(path) == nullptr) {
        return false;
    }
    if (IsCurrent(MountedManifestSource())) {
        SPDLOG_INFO("[Unbound] {} is current; mounted", path);
        return true;
    }
    Archives()->RemoveArchive(path);
    SPDLOG_INFO("[Unbound] {} was made by another build or from another ROM; converting again", path);
    return false;
}

} // namespace

std::string ConverterName() {
    return std::string("2ship ") + (const char*)gBuildVersion + " unbound r" + std::to_string(kConverterRevision);
}

ExportReport ExportArchive(const std::string& outPath) {
    ExportContext ctx;
    if (!ctx.zip.Open(outPath)) {
        ctx.report.error = "cannot open " + outPath + " for writing";
        return ctx.report;
    }
    ConvertAllScenes(ctx);
    for (const auto& language : { std::pair{ "eng", "text/message_data_static/message_data_static" },
                                 std::pair{ "staff", "text/staff_message_data_static/staff_message_data_static" } }) {
        const auto resource = std::dynamic_pointer_cast<SOH::TextMM>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(language.second));
        if (!resource) { continue; }
        Json document = { { "$schema", "2ship-unbound/text/1" }, { "messages", Json::object() } };
        for (const auto& message : resource->messages) {
            if (message.id == 0xFFFF) { continue; }
            std::string text;
            for (uint8_t byte : message.msg) {
                if (byte < 128) { text.push_back(byte); }
                else { text.push_back(0xC0 | (byte >> 6)); text.push_back(0x80 | (byte & 63)); }
            }
            document["messages"][std::to_string(message.id)] = {
                { "box", message.textboxType }, { "ypos", message.textboxYPos }, { "icon", message.icon },
                { "nextMessage", message.nextMessageID }, { "firstItemCost", message.firstItemCost },
                { "secondItemCost", message.secondItemCost }, { "text", text }
            };
        }
        if (!ctx.zip.Add(std::string("text/") + language.first + "/messages.json", document.dump(2))) {
            ctx.report.failures++;
        }
    }
    const char* baseScene = gSceneTable[SCENE_CLOCKTOWER].segment.fileName;
    const std::string basePath = std::string("scenes/nonmq/") + baseScene + "/" + baseScene;
    const auto base = Archives()->GetArchiveFromFile(basePath);
    if (base) {
        std::vector<std::string> names;
        for (const auto& [hash, name] : *base->ListFiles()) { if (name != K::kManifestPath) { names.push_back(name); } }
        std::sort(names.begin(), names.end());
        for (const auto& name : names) {
            const auto file = base->LoadFile(name);
            const std::string bytes = file && file->Buffer ? std::string(file->Buffer->begin(), file->Buffer->end()) : "";
            if (!ctx.zip.Add(name, bytes)) { ctx.report.failures++; }
            ctx.report.copied++;
        }
    }
    WriteManifest(ctx);
    if (!ctx.zip.Close()) {
        ctx.report.error = "failed to finish writing " + outPath + ": " + ctx.zip.Error();
        return ctx.report;
    }
    SPDLOG_INFO("[Unbound export] {} scenes, {} rooms, {} copied resources, {} failure(s) -> {}", ctx.report.scenes,
                ctx.report.rooms, ctx.report.copied, ctx.report.failures, outPath);
    ctx.report.ok = ctx.report.failures == 0;
    if (!ctx.report.ok) {
        ctx.report.error = std::to_string(ctx.report.failures) + " resource(s) failed to convert; see the log";
    }
    return ctx.report;
}

bool EnsureBaseArchive() {
    if (Archives()->GetGameVersions().empty()) {
        return false;
    }
    const std::string path = BaseArchivePath();
    if (MountExisting(path)) {
        return true;
    }
    std::error_code error;
    std::filesystem::remove(path, error);
    ExportReport report;
    try {
        report = ExportArchive(path);
    } catch (const std::exception& exception) {
        report.error = std::string("the converter threw: ") + exception.what();
    }
    if (!report.ok) {
        SPDLOG_ERROR("[Unbound] base archive not written: {}; vanilla scenes stay in the vanilla format", report.error);
        std::filesystem::remove(path, error);
        return false;
    }
    const bool mounted = Archives()->AddArchive(path) != nullptr;
    SPDLOG_INFO("[Unbound] {} converted and {}", path, mounted ? "mounted" : "NOT mounted");
    return mounted;
}

} // namespace S2H::Unbound
