#include "UnboundSceneFactory.h"

#include <algorithm>

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <spdlog/spdlog.h>

#include "2s2h/ModApi/ActorRegistry/ActorRegistry.h"
#include "2s2h/resource/importer/PathFactory.h"
#include "2s2h/resource/type/2shResourceType.h"
#include "2s2h/resource/type/Cutscene.h"
#include "2s2h/resource/type/Scene.h"
#include "2s2h/resource/type/scenecommand/EndMarker.h"
#include "2s2h/resource/type/scenecommand/SetActorCutsceneList.h"
#include "2s2h/resource/type/scenecommand/SetActorList.h"
#include "2s2h/resource/type/scenecommand/SetAlternateHeaders.h"
#include "2s2h/resource/type/scenecommand/SetAnimatedMaterialList.h"
#include "2s2h/resource/type/scenecommand/SetCameraSettings.h"
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
#include "SceneDB.h"
#include "UnboundCollisionFactory.h"
#include "UnboundJson.h"
#include "UnboundSchema.h"

extern "C" {
#include "z64environment.h"
}

namespace S2H::Unbound {

namespace {

constexpr const char* kOtrPrefix = "__OTR__";
constexpr u32 kFirstCustomEntrance = ENTR_SCENE_MAX << 9;

class CommandBuilder {
  public:
    explicit CommandBuilder(std::shared_ptr<Ship::ResourceInitData> sceneInitData) : mSceneInitData(sceneInitData) {
    }

    template <typename T> std::shared_ptr<T> Make(SOH::SceneCommandID id) {
        auto initData = std::make_shared<Ship::ResourceInitData>();
        initData->Id = mSceneInitData->Id;
        initData->Type = static_cast<uint32_t>(SOH::ResourceType::SOH_SceneCommand);
        initData->Path = mSceneInitData->Path + "/SceneCommand" + std::to_string(mCount++);
        initData->ResourceVersion = mSceneInitData->ResourceVersion;
        auto command = std::make_shared<T>(initData);
        command->cmdId = id;
        return command;
    }

    std::shared_ptr<Ship::ResourceInitData> ChildInitData(const std::string& suffix) const {
        auto initData = std::make_shared<Ship::ResourceInitData>(*mSceneInitData);
        initData->Path = mSceneInitData->Path + suffix;
        return initData;
    }

  private:
    std::shared_ptr<Ship::ResourceInitData> mSceneInitData;
    uint32_t mCount = 0;
};

template <typename T> std::shared_ptr<T> LoadBulkResource(const std::string& path) {
    return std::static_pointer_cast<T>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(path.c_str()));
}

// Display list and texture paths are kept as "__OTR__" strings, resolved when drawn so mods can swap them.
const char* KeepOtrPath(std::vector<std::string>& storage, const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }
    storage.push_back(kOtrPrefix + path);
    return storage.back().c_str();
}

s16 ReadActorId(const Json& value, const std::string& where) {
    auto numericId = [&](int64_t id) -> s16 {
        if (id < INT16_MIN || id > UINT16_MAX) { return -1; }
        const uint16_t packedId = (uint16_t)id;
        // MM packs spawn flags into the upper three bits of scene actor IDs.
        if ((packedId & 0x1FFF) >= ACTOR_ID_MAX) {
            SPDLOG_ERROR("[Unbound] {}: actor id {} is outside the vanilla table; use registered actor names", where, id);
            return -1;
        }
        return (s16)packedId;
    };
    if (!value.is_string()) {
        return numericId(ToInt(value));
    }
    int64_t numeric = 0;
    if (ParseIntString(value.get<std::string>(), numeric)) {
        return numericId(numeric);
    }
    const s16 actorId = ActorRegistry_GetId(value.get<std::string>().c_str());
    if (actorId < 0) {
        SPDLOG_ERROR("[Unbound] {}: actor '{}' is not a vanilla or registered actor", where, value.get<std::string>());
    }
    return actorId;
}

SOH::ActorEntry ReadActorEntry(const Json& entry, s16 actorId) {
    SOH::ActorEntry actor = {};
    actor.id = actorId;
    actor.pos = ReadVec3f(entry.value(Schema::kPos, Json()));
    actor.rot = ReadVec3s(entry.value(Schema::kRot, Json()));
    actor.params = (s16)Field(entry, Schema::kParams);
    return actor;
}

std::vector<SOH::ActorEntry> ReadActors(const Json& list, const std::string& where) {
    std::vector<SOH::ActorEntry> actors;
    for (const auto& key : PositionalKeys(list, where)) {
        const Json& entry = list[key];
        const s16 actorId = ReadActorId(entry.value(Schema::kId, Json()), where);
        if (actorId != -1) {
            actors.push_back(ReadActorEntry(entry, actorId));
        }
    }
    return actors;
}

std::shared_ptr<SOH::ISceneCommand> BuildSpawns(CommandBuilder& builder, const Json& list, const std::string& where) {
    auto command = builder.Make<SOH::SetStartPositionList>(SOH::SceneCommandID::SetStartPositionList);
    command->startPositions = ReadActors(list, where + "." + Schema::kSpawns);
    command->numStartPositions = (uint32_t)command->startPositions.size();
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildActors(CommandBuilder& builder, const Json& list, const std::string& where) {
    auto command = builder.Make<SOH::SetActorList>(SOH::SceneCommandID::SetActorList);
    command->actorList = ReadActors(list, where + "." + Schema::kActors);
    command->numActors = (uint32_t)command->actorList.size();
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildActorCsCameras(CommandBuilder& builder, const Json& list,
                                                        const std::string& where) {
    auto command = builder.Make<SOH::SetCsCamera>(SOH::SceneCommandID::SetCsCamera);
    for (const auto& key : PositionalKeys(list, where + "." + Schema::kActorCsCameras)) {
        const Json& entry = list[key];
        const Json& points = SubArray(entry, Schema::kPoints);
        SOH::ActorCsCamInfoData camera = {};
        camera.setting = (s16)Field(entry, Schema::kSetting);
        camera.count = (s16)points.size();
        camera.actorCsCamFuncData = camera.count > 0 ? new SOH::z64Vec3s[camera.count] : nullptr;
        for (s16 i = 0; i < camera.count; i++) {
            const Vec3s point = ReadVec3s(points[i]);
            camera.actorCsCamFuncData[i] = { point.x, point.y, point.z };
        }
        command->csCamera.push_back(camera);
    }
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildCollision(CommandBuilder& builder, const std::string& path) {
    auto command = builder.Make<SOH::SetCollisionHeader>(SOH::SceneCommandID::SetCollisionHeader);
    command->fileName = path;
    command->collisionHeader = LoadBulkResource<SOH::CollisionHeader>(path);
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildRooms(CommandBuilder& builder, const Json& list, const std::string& where) {
    auto command = builder.Make<SOH::SetRoomList>(SOH::SceneCommandID::SetRoomList);
    const auto keys = PositionalKeys(list, where + "." + Schema::kRooms);
    if (keys.size() > 32768) { throw DocumentError(where + ": at most 32768 rooms"); }
    command->fileNames.reserve(keys.size());
    for (const auto& key : keys) {
        command->fileNames.push_back(list[key].is_string() ? list[key].get<std::string>() : "");
    }
    for (auto& fileName : command->fileNames) {
        command->rooms.push_back({ 0, 0, (char*)fileName.c_str() });
    }
    command->numRooms = (uint32_t)command->rooms.size();
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildWind(CommandBuilder& builder, const Json& wind) {
    auto command = builder.Make<SOH::SetWindSettings>(SOH::SceneCommandID::SetWind);
    command->settings = { (int8_t)Field(wind, Schema::kWest), (int8_t)Field(wind, Schema::kVertical),
                          (int8_t)Field(wind, Schema::kSouth), (uint8_t)Field(wind, Schema::kSpeed) };
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildEntrances(CommandBuilder& builder, const Json& list,
                                                   const std::string& where) {
    auto command = builder.Make<SOH::SetEntranceList>(SOH::SceneCommandID::SetEntranceList);
    for (const auto& key : PositionalKeys(list, where + "." + Schema::kEntrances)) {
        command->entrances.push_back({ (u8)Field(list[key], Schema::kSpawn), (u16)Field(list[key], Schema::kRoom) });
    }
    command->numEntrances = (uint32_t)command->entrances.size();
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildSpecialObjects(CommandBuilder& builder, const Json& special) {
    auto command = builder.Make<SOH::SetSpecialObjects>(SOH::SceneCommandID::SetSpecialObjects);
    command->specialObjects = { (int8_t)Field(special, Schema::kElfMessage),
                                (int16_t)Field(special, Schema::kGlobalObject) };
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildBehavior(CommandBuilder& builder, const Json& behavior) {
    auto command = builder.Make<SOH::SetRoomBehaviorMM>(SOH::SceneCommandID::SetRoomBehavior);
    command->roomBehavior.gameplayFlags = (int8_t)Field(behavior, Schema::kGameplayFlags);
    command->roomBehavior.currRoomUnk2 = (int8_t)Field(behavior, Schema::kCurrRoomUnk2);
    command->roomBehavior.currRoomUnk5 = (int8_t)Field(behavior, Schema::kCurrRoomUnk5);
    command->roomBehavior.msgCtxUnk = (int8_t)Field(behavior, Schema::kMsgCtxUnk);
    command->roomBehavior.enablePointLights = (int8_t)Field(behavior, Schema::kEnablePointLights);
    command->roomBehavior.kankyoContextUnkE2 = (int8_t)Field(behavior, Schema::kKankyoContextUnkE2);
    return command;
}

void ReadMeshEntries(SOH::SetMesh& mesh, const Json& entries, const std::string& where) {
    const auto keys = PositionalKeys(entries, where);
    mesh.opaPaths.reserve(keys.size());
    mesh.xluPaths.reserve(keys.size());
    for (const auto& key : keys) {
        const Json& entry = entries[key];
        Gfx* opa = (Gfx*)KeepOtrPath(mesh.opaPaths, PathField(entry, Schema::kOpa));
        Gfx* xlu = (Gfx*)KeepOtrPath(mesh.xluPaths, PathField(entry, Schema::kXlu));
        if (mesh.meshHeader.base.type == 2) {
            SOH::PolygonDlist2 dlist = {};
            dlist.pos = ReadVec3f(entry.value(Schema::kPos, Json()));
            dlist.unk_06 = (s16)Field(entry, Schema::kRadius);
            dlist.opa = opa;
            dlist.xlu = xlu;
            mesh.dlists2.push_back(dlist);
        } else {
            mesh.dlists.push_back({ opa, xlu });
        }
    }
}

SOH::BgImage ReadBgImage(SOH::SetMesh& mesh, const Json& image) {
    SOH::BgImage bgImage = {};
    bgImage.unk_00 = (u16)Field(image, Schema::kUnk00);
    bgImage.id = (u8)Field(image, Schema::kId);
    bgImage.source = (void*)KeepOtrPath(mesh.imagePaths, PathField(image, Schema::kSource));
    bgImage.unk_0C = (u32)Field(image, Schema::kUnk0C);
    bgImage.tlut = (u32)Field(image, Schema::kTlut);
    bgImage.width = (u16)Field(image, Schema::kWidth);
    bgImage.height = (u16)Field(image, Schema::kHeight);
    bgImage.fmt = (u8)Field(image, Schema::kFmt);
    bgImage.siz = (u8)Field(image, Schema::kSiz);
    bgImage.mode0 = (u16)Field(image, Schema::kMode0);
    bgImage.tlutCount = (u16)Field(image, Schema::kTlutCount);
    return bgImage;
}

void ReadPrerenderedMesh(SOH::SetMesh& mesh, const Json& shape, const std::string& where) {
    auto& polygon = mesh.meshHeader.polygon1;
    polygon.format = (u8)Field(shape, Schema::kFormat, 1);
    const Json& images = Sub(shape, Schema::kImages);
    const auto keys = PositionalKeys(images, where + "." + Schema::kImages);
    mesh.imagePaths.reserve(keys.size());
    for (const auto& key : keys) {
        mesh.images.push_back(ReadBgImage(mesh, images[key]));
    }
    if (polygon.format == 1 && !mesh.images.empty()) {
        const SOH::BgImage& image = mesh.images.front();
        polygon.single = { image.source, image.unk_0C, (void*)(uintptr_t)image.tlut, image.width,
                           image.height, image.fmt,    image.siz,                    image.mode0,
                           image.tlutCount };
        mesh.images.clear();
    } else {
        polygon.multi.count = (u8)mesh.images.size();
        polygon.multi.list = mesh.images.data();
    }
    mesh.opaPaths.reserve(1);
    mesh.xluPaths.reserve(1);
    Gfx* opa = (Gfx*)KeepOtrPath(mesh.opaPaths, PathField(shape, Schema::kOpa));
    Gfx* xlu = (Gfx*)KeepOtrPath(mesh.xluPaths, PathField(shape, Schema::kXlu));
    mesh.dlists.push_back({ opa, xlu });
    polygon.dlist = (Gfx*)mesh.dlists.data();
}

std::shared_ptr<SOH::ISceneCommand> BuildMesh(CommandBuilder& builder, const Json& shape, const std::string& where) {
    auto command = builder.Make<SOH::SetMesh>(SOH::SceneCommandID::SetMesh);
    command->meshHeader.base.type = (u8)Field(shape, Schema::kType);
    const std::string meshWhere = where + "." + Schema::kMesh;
    switch (command->meshHeader.base.type) {
        case 0:
            ReadMeshEntries(*command, Sub(shape, Schema::kEntries), meshWhere);
            command->meshHeader.polygon0.num = (u32)command->dlists.size();
            command->meshHeader.polygon0.start = command->dlists.data();
            break;
        case 1:
            ReadPrerenderedMesh(*command, shape, meshWhere);
            break;
        case 2:
            ReadMeshEntries(*command, Sub(shape, Schema::kEntries), meshWhere);
            command->meshHeader.polygon2.num = (u32)command->dlists2.size();
            command->meshHeader.polygon2.start = command->dlists2.data();
            break;
        default:
            throw DocumentError(meshWhere + ": unknown room shape type " +
                                std::to_string(command->meshHeader.base.type));
    }
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildObjects(CommandBuilder& builder, const Json& objects) {
    auto command = builder.Make<SOH::SetObjectList>(SOH::SceneCommandID::SetObjectList);
    for (const auto& objectId : objects) {
        command->objects.push_back((int16_t)ToInt(objectId));
    }
    command->numObjects = (uint32_t)command->objects.size();
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildLights(CommandBuilder& builder, const Json& list, const std::string& where) {
    auto command = builder.Make<SOH::SetLightList>(SOH::SceneCommandID::SetLightList);
    for (const auto& key : PositionalKeys(list, where + "." + Schema::kLights)) {
        const Json& entry = list[key];
        SOH::LightInfo light = {};
        const Vec3f pos = ReadVec3f(entry.value(Schema::kPos, Json()));
        light.type = (u8)Field(entry, Schema::kType);
        SOH::LightInfo_SetCoords(light, pos.x, pos.y, pos.z);
        ReadRgb(entry.value(Schema::kColor, Json()), light.params.point.color);
        light.params.point.drawGlow = (u8)Field(entry, Schema::kGlow);
        light.params.point.radius = (s16)Field(entry, Schema::kRadius);
        command->lightList.push_back(light);
    }
    command->numLights = (uint32_t)command->lightList.size();
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildPaths(CommandBuilder& builder, const Json& paths) {
    auto command = builder.Make<SOH::SetPathwaysMM>(SOH::SceneCommandID::SetPathways);
    for (const auto& path : paths) {
        if (path.is_string()) {
            command->paths.push_back(LoadBulkResource<SOH::PathMM>(path.get<std::string>())->GetPointer());
            command->fileNames.push_back(path.get<std::string>());
        }
    }
    command->numPaths = (uint32_t)command->paths.size();
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildTransitionActors(CommandBuilder& builder, const Json& list,
                                                          const std::string& where) {
    auto command = builder.Make<SOH::SetTransitionActorList>(SOH::SceneCommandID::SetTransitionActorList);
    for (const auto& key : PositionalKeys(list, where + "." + Schema::kTransitionActors)) {
        const Json& entry = list[key];
        const s16 actorId = ReadActorId(entry.value(Schema::kId, Json()), where);
        if (actorId == -1) {
            continue;
        }
        SOH::TransitionActorEntry actor = {};
        const Json& front = Sub(entry, Schema::kFront);
        const Json& back = Sub(entry, Schema::kBack);
        actor.sides[0] = { (s16)Field(front, Schema::kRoom, -1), (s8)Field(front, Schema::kEffects) };
        actor.sides[1] = { (s16)Field(back, Schema::kRoom, -1), (s8)Field(back, Schema::kEffects) };
        actor.id = actorId;
        actor.pos = ReadVec3f(entry.value(Schema::kPos, Json()));
        actor.rotY = (s16)Field(entry, Schema::kRotY);
        actor.params = (s16)Field(entry, Schema::kParams);
        command->transitionActorList.push_back(actor);
    }
    command->numTransitionActors = (uint32_t)command->transitionActorList.size();
    return command;
}

// World fog is on when any of its keys is present; an absent one takes the entry's vanilla equivalent.
void ReadWorldFog(SOH::EnvLightSettings& light, const Json& entry) {
    const bool hasWorldFog = entry.contains(Schema::kFogStart) || entry.contains(Schema::kFogEnd) ||
                             entry.contains(Schema::kDrawDistance) || entry.contains(Schema::kNearPlane);
    if (!hasWorldFog) {
        return;
    }
    const f32 legacyStart = Environment_GetLegacyFogStart(ENV_LIGHT_SETTINGS_FOG_NEAR(light.fogNear), light.fogFar);
    const f32 drawDistance = (f32)NumberField(entry, Schema::kDrawDistance, light.fogFar);
    light.hasWorldFog = true;
    light.worldFog.fogStart = (f32)NumberField(entry, Schema::kFogStart, legacyStart);
    light.worldFog.fogEnd = (f32)NumberField(entry, Schema::kFogEnd, drawDistance);
    light.worldFog.drawDistance = drawDistance;
    light.worldFog.nearPlane = (f32)NumberField(entry, Schema::kNearPlane, 0.0);
}

SOH::EnvLightSettings ReadLightSetting(const Json& entry) {
    SOH::EnvLightSettings light = {};
    ReadRgb(entry.value(Schema::kAmbient, Json()), light.ambientColor);
    ReadRgb(entry.value(Schema::kLight1Dir, Json()), light.light1Dir);
    ReadRgb(entry.value(Schema::kLight1Color, Json()), light.light1Color);
    ReadRgb(entry.value(Schema::kLight2Dir, Json()), light.light2Dir);
    ReadRgb(entry.value(Schema::kLight2Color, Json()), light.light2Color);
    ReadRgb(entry.value(Schema::kFogColor, Json()), light.fogColor);
    light.fogNear = (s16)Field(entry, Schema::kFogNear);
    light.fogFar = (s16)Field(entry, Schema::kFogFar);
    ReadWorldFog(light, entry);
    return light;
}

std::shared_ptr<SOH::ISceneCommand> BuildLighting(CommandBuilder& builder, const Json& list,
                                                  const std::string& where) {
    auto command = builder.Make<SOH::SetLightingSettings>(SOH::SceneCommandID::SetLightingSettings);
    for (const auto& key : PositionalKeys(list, where + "." + Schema::kLighting)) {
        command->settings.push_back(ReadLightSetting(list[key]));
    }
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildTime(CommandBuilder& builder, const Json& time) {
    auto command = builder.Make<SOH::SetTimeSettings>(SOH::SceneCommandID::SetTimeSettings);
    command->settings = { (uint8_t)Field(time, Schema::kHour, 255), (uint8_t)Field(time, Schema::kMinute, 255),
                          (uint8_t)Field(time, Schema::kIncrement, 255) };
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildSkybox(CommandBuilder& builder, const Json& skybox) {
    auto command = builder.Make<SOH::SetSkyboxSettings>(SOH::SceneCommandID::SetSkyboxSettings);
    command->settings = { (uint8_t)Field(skybox, Schema::kUnk), (uint8_t)Field(skybox, Schema::kId),
                          (uint8_t)Field(skybox, Schema::kWeather), (uint8_t)Field(skybox, Schema::kIndoors) };
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildSkyboxModifier(CommandBuilder& builder, const Json& modifier) {
    auto command = builder.Make<SOH::SetSkyboxModifier>(SOH::SceneCommandID::SetSkyboxModifier);
    command->modifier = { (uint8_t)Field(modifier, Schema::kSkyboxDisabled),
                          (uint8_t)Field(modifier, Schema::kSunMoonDisabled) };
    return command;
}

u32 ReadExit(const Json& value, const std::string& where) {
    if (value.is_string()) {
        const std::string name = value.get<std::string>();
        int64_t numeric = 0;
        if (ParseIntString(name, numeric) && numeric >= 0 &&
            (numeric < kFirstCustomEntrance || numeric == UINT16_MAX)) {
            return (u32)numeric;
        }
        if (auto entrance = SceneDB_FindEntrance(name)) {
            return *entrance;
        }
        throw DocumentError(where + ": exit '" + name + "' is not a registered entrance");
    }
    if (!value.is_number_integer() || value.get<int64_t>() < 0) {
        throw DocumentError(where + ": an exit is a vanilla entrance number or a registered entrance name");
    }
    const int64_t entrance = value.get<int64_t>();
    if (entrance >= kFirstCustomEntrance && entrance != UINT16_MAX) {
        throw DocumentError(where + ": exit " + std::to_string(entrance) +
                            " is in the custom entrance range; name the entrance instead");
    }
    return (u32)entrance;
}

std::shared_ptr<SOH::ISceneCommand> BuildExits(CommandBuilder& builder, const Json& exits, const std::string& where) {
    auto command = builder.Make<SOH::SetExitList>(SOH::SceneCommandID::SetExitList);
    for (const auto& exit : exits) {
        command->exits.push_back(ReadExit(exit, where + "." + Schema::kExits));
    }
    command->numExits = (uint32_t)command->exits.size();
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildSound(CommandBuilder& builder, const Json& sound) {
    auto command = builder.Make<SOH::SetSoundSettings>(SOH::SceneCommandID::SetSoundSettings);
    command->settings = { (uint8_t)Field(sound, Schema::kSeq), (uint8_t)Field(sound, Schema::kNatureAmbience),
                          (uint8_t)Field(sound, Schema::kReverb) };
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildEcho(CommandBuilder& builder, const Json& echo) {
    auto command = builder.Make<SOH::SetEchoSettings>(SOH::SceneCommandID::SetEchoSettings);
    command->settings.echo = (int8_t)ToInt(echo);
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildCutscenes(CommandBuilder& builder, const Json& list,
                                                   const std::string& where) {
    auto command = builder.Make<SOH::SetCutscenesMM>(SOH::SceneCommandID::SetCutscenesMM);
    for (const auto& key : PositionalKeys(list, where + "." + Schema::kCutscenes)) {
        const Json& entry = list[key];
        SOH::CutsceneScriptEntry script = {};
        const std::string scriptPath = PathField(entry, Schema::kScript);
        script.data = LoadBulkResource<SOH::Cutscene>(scriptPath)->GetPointer();
        command->scriptPaths.push_back(scriptPath);
        script.exit = (uint16_t)Field(entry, Schema::kExit);
        script.entrance = (uint8_t)Field(entry, Schema::kEntrance);
        script.flag = (uint8_t)Field(entry, Schema::kFlag);
        command->entries.push_back(script);
    }
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildRegionVisited(CommandBuilder& builder) {
    return builder.Make<SOH::SetCameraSettings>(SOH::SceneCommandID::SetWorldMapVisited);
}

std::shared_ptr<SOH::ISceneCommand> BuildAnimatedMaterials(CommandBuilder& builder, const std::string& path) {
    auto command = builder.Make<SOH::SetAnimatedMaterialList>(SOH::SceneCommandID::SetAnimatedMaterialList);
    command->mat = LoadBulkResource<SOH::TextureAnimation>(path)->GetPointer();
    command->fileName = path;
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildActorCutscenes(CommandBuilder& builder, const Json& list,
                                                        const std::string& where) {
    auto command = builder.Make<SOH::SetActorCutsceneList>(SOH::SceneCommandID::SetActorCutsceneList);
    for (const auto& key : PositionalKeys(list, where + "." + Schema::kActorCutscenes)) {
        const Json& entry = list[key];
        SOH::CutsceneEntry cutscene = {};
        cutscene.priority = (int16_t)Field(entry, Schema::kPriority);
        cutscene.length = (int16_t)Field(entry, Schema::kLength);
        cutscene.csCamId = (int16_t)Field(entry, Schema::kCsCamId);
        cutscene.scriptIndex = (int16_t)Field(entry, Schema::kScriptIndex);
        cutscene.additionalCsId = (int16_t)Field(entry, Schema::kAdditionalCsId);
        cutscene.endSfx = (uint8_t)Field(entry, Schema::kEndSfx);
        cutscene.customValue = (uint8_t)Field(entry, Schema::kCustomValue);
        cutscene.hudVisibility = (int16_t)Field(entry, Schema::kHudVisibility);
        cutscene.endCam = (uint8_t)Field(entry, Schema::kEndCam);
        cutscene.letterboxSize = (uint8_t)Field(entry, Schema::kLetterboxSize);
        command->entries.push_back(cutscene);
    }
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildMinimap(CommandBuilder& builder, const Json& minimap,
                                                 const std::string& where) {
    auto command = builder.Make<SOH::SetMinimapList>(SOH::SceneCommandID::SetMinimapList);
    const Json& entries = Sub(minimap, Schema::kEntries);
    for (const auto& key : PositionalKeys(entries, where + "." + Schema::kMinimap)) {
        const Json& entry = entries[key];
        command->entries.push_back({ (u16)Field(entry, Schema::kMapId), (s16)Field(entry, Schema::kUnk2),
                                     (s16)Field(entry, Schema::kUnk4), (s16)Field(entry, Schema::kUnk6),
                                     (u16)Field(entry, Schema::kUnk8) });
    }
    command->list.scale = (s16)Field(minimap, Schema::kScale);
    command->list.entry = command->entries.data();
    return command;
}

std::shared_ptr<SOH::ISceneCommand> BuildMinimapChests(CommandBuilder& builder, const Json& list,
                                                       const std::string& where) {
    auto command = builder.Make<SOH::SetMinimapChests>(SOH::SceneCommandID::SetMinimapChests);
    for (const auto& key : PositionalKeys(list, where + "." + Schema::kMinimapChests)) {
        const Json& entry = list[key];
        command->chests.push_back({ (int16_t)Field(entry, Schema::kUnk0), (int16_t)Field(entry, Schema::kUnk2),
                                    (int16_t)Field(entry, Schema::kUnk4), (int16_t)Field(entry, Schema::kUnk6),
                                    (int16_t)Field(entry, Schema::kUnk8) });
    }
    return command;
}

using SetupCommandBuilder = std::shared_ptr<SOH::ISceneCommand> (*)(CommandBuilder& builder, const Json& setup,
                                                                    const std::string& where);

struct SetupCommand {
    const char* key;
    SetupCommandBuilder build;
};

// The canonical order a setup without "order" runs in: entrances before spawns, which index them.
const SetupCommand kSetupCommands[] = {
    { Schema::kSound, [](CommandBuilder& b, const Json& s, const std::string&) { return BuildSound(b, Sub(s, Schema::kSound)); } },
    { Schema::kEntrances, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildEntrances(b, Sub(s, Schema::kEntrances), w); } },
    { Schema::kSpawns, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildSpawns(b, Sub(s, Schema::kSpawns), w); } },
    { Schema::kSpecialObjects, [](CommandBuilder& b, const Json& s, const std::string&) { return BuildSpecialObjects(b, Sub(s, Schema::kSpecialObjects)); } },
    { Schema::kCollision, [](CommandBuilder& b, const Json& s, const std::string&) { return BuildCollision(b, PathField(s, Schema::kCollision)); } },
    { Schema::kRooms, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildRooms(b, Sub(s, Schema::kRooms), w); } },
    { Schema::kSkybox, [](CommandBuilder& b, const Json& s, const std::string&) { return BuildSkybox(b, Sub(s, Schema::kSkybox)); } },
    { Schema::kExits, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildExits(b, SubArray(s, Schema::kExits), w); } },
    { Schema::kLighting, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildLighting(b, Sub(s, Schema::kLighting), w); } },
    { Schema::kPaths, [](CommandBuilder& b, const Json& s, const std::string&) { return BuildPaths(b, SubArray(s, Schema::kPaths)); } },
    { Schema::kTransitionActors, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildTransitionActors(b, Sub(s, Schema::kTransitionActors), w); } },
    { Schema::kCutscenes, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildCutscenes(b, Sub(s, Schema::kCutscenes), w); } },
    { Schema::kActorCutscenes, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildActorCutscenes(b, Sub(s, Schema::kActorCutscenes), w); } },
    { Schema::kActorCsCameras, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildActorCsCameras(b, Sub(s, Schema::kActorCsCameras), w); } },
    { Schema::kMinimap, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildMinimap(b, Sub(s, Schema::kMinimap), w); } },
    { Schema::kMinimapChests, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildMinimapChests(b, Sub(s, Schema::kMinimapChests), w); } },
    { Schema::kRegionVisited, [](CommandBuilder& b, const Json& s, const std::string&) { return BuildRegionVisited(b); } },
    { Schema::kAnimatedMaterials, [](CommandBuilder& b, const Json& s, const std::string&) { return BuildAnimatedMaterials(b, PathField(s, Schema::kAnimatedMaterials)); } },
    { Schema::kWind, [](CommandBuilder& b, const Json& s, const std::string&) { return BuildWind(b, Sub(s, Schema::kWind)); } },
    { Schema::kBehavior, [](CommandBuilder& b, const Json& s, const std::string&) { return BuildBehavior(b, Sub(s, Schema::kBehavior)); } },
    { Schema::kMesh, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildMesh(b, Sub(s, Schema::kMesh), w); } },
    { Schema::kObjects, [](CommandBuilder& b, const Json& s, const std::string&) { return BuildObjects(b, SubArray(s, Schema::kObjects)); } },
    { Schema::kActors, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildActors(b, Sub(s, Schema::kActors), w); } },
    { Schema::kLights, [](CommandBuilder& b, const Json& s, const std::string& w) { return BuildLights(b, Sub(s, Schema::kLights), w); } },
    { Schema::kTime, [](CommandBuilder& b, const Json& s, const std::string&) { return BuildTime(b, Sub(s, Schema::kTime)); } },
    { Schema::kSkyboxModifier, [](CommandBuilder& b, const Json& s, const std::string&) { return BuildSkyboxModifier(b, Sub(s, Schema::kSkyboxModifier)); } },
    { Schema::kEcho, [](CommandBuilder& b, const Json& s, const std::string&) { return BuildEcho(b, s[Schema::kEcho]); } },
};

const SetupCommand* FindSetupCommand(const std::string& key) {
    for (const SetupCommand& command : kSetupCommands) {
        if (key == command.key) {
            return &command;
        }
    }
    return nullptr;
}

bool IsPresent(const Json& setup, const SetupCommand& command) {
    if (std::string(command.key) == Schema::kRegionVisited) {
        return Field(setup, Schema::kRegionVisited) != 0;
    }
    return setup.contains(command.key);
}

// Engine order matters (the spawn list reads the entrance list, persistent objects take slots in turn): "order" keeps
// the converted scene's own order, and keys it does not list follow in kSetupCommands order.
std::vector<const SetupCommand*> OrderedCommands(const Json& setup) {
    std::vector<const SetupCommand*> ordered;
    for (const auto& key : SubArray(setup, Schema::kCommandOrder)) {
        const SetupCommand* command = key.is_string() ? FindSetupCommand(key.get<std::string>()) : nullptr;
        if (command != nullptr && IsPresent(setup, *command) &&
            std::find(ordered.begin(), ordered.end(), command) == ordered.end()) {
            ordered.push_back(command);
        }
    }
    for (const SetupCommand& command : kSetupCommands) {
        if (IsPresent(setup, command) && std::find(ordered.begin(), ordered.end(), &command) == ordered.end()) {
            ordered.push_back(&command);
        }
    }
    return ordered;
}

void BuildSetupCommands(CommandBuilder& builder, SOH::Scene& scene, const Json& setup, const std::string& where) {
    for (const SetupCommand* command : OrderedCommands(setup)) {
        scene.commands.push_back(command->build(builder, setup, where));
    }
    scene.commands.push_back(builder.Make<SOH::EndMarker>(SOH::SceneCommandID::EndMarker));
}

std::shared_ptr<SOH::Scene> BuildSetup(CommandBuilder& builder, std::shared_ptr<Ship::ResourceInitData> initData,
                                       const Json& setup, const std::string& where) {
    auto scene = std::make_shared<SOH::Scene>(initData);
    BuildSetupCommands(builder, *scene, setup, where);
    return scene;
}

// Layers past "0" run as vanilla alternate headers: the list comes first and a missing layer falls back to "0".
std::shared_ptr<SOH::ISceneCommand> BuildAlternateHeaders(CommandBuilder& builder, const Json& setups,
                                                          int64_t highestLayer, const std::string& where) {
    auto command = builder.Make<SOH::SetAlternateHeaders>(SOH::SceneCommandID::SetAlternateHeaders);
    for (int64_t layer = 1; layer <= highestLayer; layer++) {
        const std::string key = std::to_string(layer);
        if (!setups.contains(key) || !setups[key].is_object()) {
            command->headers.push_back(nullptr);
            continue;
        }
        CommandBuilder layerBuilder(builder.ChildInitData("/Setup" + key));
        command->headers.push_back(BuildSetup(layerBuilder, builder.ChildInitData("/Setup" + key), setups[key],
                                              where + ".setups." + key));
    }
    command->numHeaders = (uint32_t)command->headers.size();
    return command;
}

int64_t HighestLayer(const Json& setups) {
    int64_t highest = 0;
    for (const auto& key : ListKeys(setups)) {
        int64_t layer = 0;
        if (ParseIntString(key, layer) && layer > highest) {
            highest = layer;
        }
    }
    return highest;
}

void CheckSchema(const Json& doc, const std::string& path) {
    std::string type;
    int version = 0;
    if (!ParseSchema(SchemaOf(doc), type, version) ||
        (type != Schema::kSceneType && type != Schema::kRoomType)) {
        throw DocumentError(path + ": '" + SchemaOf(doc) + "' is not a 2Ship Unbound scene or room schema");
    }
    if (version != Schema::kSceneVersion) {
        throw DocumentError(path + ": schema version " + std::to_string(version) + " is not supported");
    }
}

std::shared_ptr<SOH::Scene> BuildScene(std::shared_ptr<Ship::ResourceInitData> initData, const Json& doc) {
    const std::string& path = initData->Path;
    CheckSchema(doc, path);
    const Json& setups = Sub(doc, Schema::kSetups);
    if (!setups.contains("0") || !setups["0"].is_object()) {
        throw DocumentError(path + ": setup \"0\" is required");
    }
    CommandBuilder builder(initData);
    auto scene = std::make_shared<SOH::Scene>(initData);
    const int64_t highestLayer = HighestLayer(setups);
    if (highestLayer > 0) {
        scene->commands.push_back(BuildAlternateHeaders(builder, setups, highestLayer, path));
    }
    BuildSetupCommands(builder, *scene, setups["0"], path + ".setups.0");
    return scene;
}

} // namespace

std::shared_ptr<Ship::IResource> ResourceFactoryJsonSceneV1::ReadResource(
    std::shared_ptr<Ship::File> file, std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }
    try {
        return BuildScene(initData, LoadMergedJson(initData->Path));
    } catch (const std::exception& error) {
        SPDLOG_ERROR("[Unbound] {}", error.what());
        return nullptr;
    }
}

void RegisterResourceFactories(const std::shared_ptr<Ship::ResourceLoader>& loader) {
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryJsonPathMMV1>(), RESOURCE_FORMAT_JSON,
                                    "2ship-unbound/path", static_cast<uint32_t>(SOH::ResourceType::SOH_Path), 1);
    const uint32_t roomType = static_cast<uint32_t>(SOH::ResourceType::SOH_Room);
    loader->RegisterResourceFactory(std::make_shared<ResourceFactoryJsonSceneV1>(), RESOURCE_FORMAT_JSON,
                                    Schema::kSceneType, roomType, Schema::kSceneVersion);
    loader->RegisterResourceFactory(std::make_shared<ResourceFactoryJsonSceneV1>(), RESOURCE_FORMAT_JSON,
                                    Schema::kRoomType, roomType, Schema::kSceneVersion);
    loader->RegisterResourceFactory(std::make_shared<ResourceFactoryJsonCollisionV1>(), RESOURCE_FORMAT_JSON,
                                    Schema::kCollisionType,
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_CollisionHeader),
                                    Schema::kCollisionVersion);
}

} // namespace S2H::Unbound
