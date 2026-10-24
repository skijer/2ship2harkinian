#include "2s2h/resource/importer/PathFactory.h"
#include "2s2h/resource/type/Path.h"
#include "2s2h/Unbound/UnboundJson.h"
#include <spdlog/spdlog.h>

namespace SOH {
std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryPathMMV0::ReadResource(std::shared_ptr<Ship::File> file,
                                            std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto path = std::make_shared<PathMM>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    path->numPaths = reader->ReadUInt32();
    path->paths.reserve(path->numPaths);
    for (uint32_t k = 0; k < path->numPaths; k++) {
        std::vector<Vec3f> points;
        uint32_t pointCount = reader->ReadUInt32();

        uint8_t additionalPathIndex = reader->ReadUByte();
        int16_t customValue = reader->ReadInt16();

        for (uint32_t i = 0; i < pointCount; i++) {
            Vec3f point;
            point.x = reader->ReadInt16();
            point.y = reader->ReadInt16();
            point.z = reader->ReadInt16();

            points.push_back(point);
        }

        PathDataMM pathDataEntry;
        pathDataEntry.count = pointCount;
        pathDataEntry.additionalPathIndex = additionalPathIndex;
        pathDataEntry.customValue = customValue;

        path->paths.push_back(points);
        pathDataEntry.points = path->paths.back().data();

        path->pathData.push_back(pathDataEntry);
    }

    return path;
}
std::shared_ptr<Ship::IResource>
ResourceFactoryJsonPathMMV1::ReadResource(std::shared_ptr<Ship::File> file,
                                         std::shared_ptr<Ship::ResourceInitData> initData) {
    using namespace S2H::Unbound;
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }
    try {
        const Json doc = LoadMergedJson(initData->Path);
        if (doc.value("$schema", "") != "2ship-unbound/path/1") {
            throw DocumentError("unsupported path schema");
        }
        const Json& paths = Sub(doc, "paths");
        const auto keys = PositionalKeys(paths, initData->Path + ".paths");
        if (keys.size() > 255) {
            throw DocumentError("at most 255 paths per resource");
        }
        auto resource = std::make_shared<PathMM>(initData);
        resource->numPaths = keys.size();
        resource->paths.reserve(keys.size());
        resource->pathData.reserve(keys.size());
        for (const auto& key : keys) {
            const Json& definition = paths[key];
            const auto& points = SubArray(definition, "points");
            if (points.size() > 255) {
                throw DocumentError("at most 255 points per MM path");
            }
            auto& positions = resource->paths.emplace_back();
            positions.reserve(points.size());
            for (const auto& point : points) {
                positions.push_back(ReadVec3f(point));
            }
            const int64_t additional = Field(definition, "additionalPathIndex", 255);
            if (additional < 0 || additional > 255 ||
                (additional != 255 && additional >= keys.size())) {
                throw DocumentError("additionalPathIndex is outside the path list");
            }
            resource->pathData.push_back({ static_cast<u8>(positions.size()), static_cast<u8>(additional),
                                          static_cast<s16>(Field(definition, "customValue")), positions.data() });
        }
        return resource;
    } catch (const std::exception& error) {
        SPDLOG_ERROR("[Unbound] Path '{}': {}", initData->Path, error.what());
        return nullptr;
    }
}
} // namespace SOH
