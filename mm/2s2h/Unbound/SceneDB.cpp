#include "SceneDB.h"

#include <deque>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <spdlog/spdlog.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/SaveManager/SaveSections.h"
#include "UnboundJson.h"
#include "UnboundSchema.h"

extern "C" {
#include "variables.h"
}

namespace {

using namespace S2H::Unbound;

constexpr u32 kSpawnsPerSlot = 32;
constexpr u16 kDefaultTransition = 2;
constexpr u16 kContinueBgmFlag = 0x8000;
constexpr const char* kFlagsSection = "unboundSceneFlags";

struct PersistentFlags {
    u32 chest = 0;
    u32 switch0 = 0;
    u32 switch1 = 0;
    u32 collectible = 0;
};

struct CustomScene {
    std::string key;
    std::string name;
    std::string scenePath;
    SceneTableEntry entry = {};
    PermanentSceneFlags permanentFlags = {};
    CycleSceneFlags cycleFlags = {};
    PersistentFlags persistent;
};

// One Majora's Mask entrance slot (the bits above 9) per registry entry that declares entrances.
struct EntranceSlot {
    std::string sceneKey;
    std::vector<std::string> names;
    std::vector<EntranceTableEntry> entries;
};

// Deques: the engine keeps SceneTableEntry and flag pointers, so loading more must never move them.
std::deque<CustomScene> sCustomScenes;
std::vector<EntranceSlot> sSlots;
std::unordered_map<std::string, u32> sEntrancesByName;
std::unordered_map<std::string, s16> sSceneIdsByKey;
std::deque<std::string> sScenePaths;
std::unordered_map<s16, const char*> sScenePathById;
bool sBaseMounted = false;

struct RoomFlags {
    std::set<s32> visited;
    std::set<s32> cleared;
    std::set<s32> temporary;
};
std::unordered_map<std::string, RoomFlags> sRoomFlags;

Json WriteRoomFlags() {
    Json section = Json::object();
    for (const auto& [key, flags] : sRoomFlags) {
        section[key] = { { "visited", flags.visited }, { "cleared", flags.cleared } };
    }
    return section;
}

void ReadRoomFlags(const Json& section) {
    sRoomFlags.clear();
    if (!section.is_object()) {
        return;
    }
    for (const auto& [key, saved] : section.items()) {
        auto& flags = sRoomFlags[key];
        for (const char* field : { "visited", "cleared" }) {
            for (const auto& entry : SubArray(saved, field)) {
                const int64_t room = ToInt(entry, -1);
                if (room >= 32 && room <= INT16_MAX) {
                    (field[0] == 'v' ? flags.visited : flags.cleared).insert((s32)room);
                }
            }
        }
    }
}

EntranceTableEntry sMissingEntrance = { SCENE_CLOCKTOWER, 0, (kDefaultTransition << 7) | kDefaultTransition };
PermanentSceneFlags sScratchPermanentFlags;
CycleSceneFlags sScratchCycleFlags;

std::shared_ptr<Ship::ArchiveManager> Archives() {
    return Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
}

bool IsVanillaScene(s16 sceneId) {
    return sceneId >= 0 && sceneId < SCENE_MAX && gSceneTable[sceneId].segment.fileName != nullptr;
}

CustomScene* FindCustomScene(s16 sceneId) {
    const s32 index = sceneId - SCENE_MAX;
    return index >= 0 && index < (s32)sCustomScenes.size() ? &sCustomScenes[index] : nullptr;
}

bool ReadBaseManifest() {
    const Json manifest = LoadMergedJson(Schema::kManifestPath);
    if (!manifest.is_object() || manifest.value(Schema::kFormatName, "") != Schema::kFormatValue) {
        return false;
    }
    for (const auto& feature : SubArray(manifest, Schema::kFeatures)) {
        if (feature.is_string() && feature.get<std::string>() == Schema::kFeatureScenes) {
            return true;
        }
    }
    return false;
}

std::string VanillaScenePath(const char* fileName) {
    const std::string unboundPath = std::string("scenes/") + fileName + "/scene.json";
    if (sBaseMounted && Archives()->HasFile(unboundPath)) {
        return unboundPath;
    }
    return std::string("scenes/nonmq/") + fileName + "/" + fileName;
}

const char* StorePath(s16 sceneId, const std::string& path) {
    const char* stored = sScenePaths.emplace_back(path).c_str();
    sScenePathById[sceneId] = stored;
    return stored;
}

u16 EntranceFlags(const Json& entrance) {
    const u16 start = (u16)Field(entrance, Schema::kStartTransition, kDefaultTransition) & 0x7F;
    const u16 end = (u16)Field(entrance, Schema::kEndTransition, kDefaultTransition) & 0x7F;
    const bool continueBgm = Field(entrance, Schema::kContinueBgm, 0) != 0;
    return (continueBgm ? kContinueBgmFlag : 0) | (start << 7) | end;
}

void AddEntrances(const std::string& sceneKey, s16 sceneId, const Json& entrances) {
    const std::vector<std::string> keys = ListKeys(entrances);
    if (keys.empty()) {
        return;
    }
    if (keys.size() > kSpawnsPerSlot) {
        SPDLOG_ERROR("[Unbound] Scene '{}' declares {} entrances; only the first {} are kept", sceneKey, keys.size(),
                     kSpawnsPerSlot);
    }
    const u32 slotIndex = (u32)sSlots.size();
    EntranceSlot& slot = sSlots.emplace_back();
    slot.sceneKey = sceneKey;
    for (size_t i = 0; i < keys.size() && i < kSpawnsPerSlot; i++) {
        const Json& entrance = entrances[keys[i]];
        const bool titleCard = Field(entrance, Schema::kTitleCard, 1) != 0;
        const s16 spawn = (s16)Field(entrance, Schema::kSpawn, 0);
        slot.names.push_back(keys[i]);
        slot.entries.push_back({ (s16)(titleCard ? sceneId : -sceneId), (s8)spawn, EntranceFlags(entrance) });
        const u32 code = Entrance_Create(ENTR_SCENE_MAX + slotIndex, (s32)i, 0);
        sEntrancesByName[sceneKey + "/" + keys[i]] = code;
    }
}

std::optional<s16> VanillaSceneId(const std::string& key) {
    for (s16 sceneId = 0; sceneId < SCENE_MAX; sceneId++) {
        if (IsVanillaScene(sceneId) && key == gSceneTable[sceneId].segment.fileName) {
            return sceneId;
        }
    }
    return std::nullopt;
}

PersistentFlags ReadPersistentFlags(const Json& entry) {
    const Json& persistent = Sub(entry, Schema::kPersistentFlags);
    return { (u32)Field(persistent, Schema::kChest, 0), (u32)Field(persistent, Schema::kSwitch0, 0),
             (u32)Field(persistent, Schema::kSwitch1, 0), (u32)Field(persistent, Schema::kCollectible, 0) };
}

bool AddCustomScene(const std::string& key, const Json& entry) {
    const std::string scenePath = PathField(entry, Schema::kScene);
    if (scenePath.empty()) {
        SPDLOG_ERROR("[Unbound] Registry scene '{}' has no '{}' path; skipped", key, Schema::kScene);
        return false;
    }
    const s16 sceneId = (s16)(SCENE_MAX + sCustomScenes.size());
    CustomScene& scene = sCustomScenes.emplace_back();
    scene.key = key;
    scene.name = entry.value(Schema::kName, key);
    scene.scenePath = scenePath;
    scene.persistent = ReadPersistentFlags(entry);
    scene.entry.segment.fileName = (char*)scene.key.c_str();
    scene.entry.titleTextId = (u16)Field(entry, Schema::kTitleTextId, 0);
    scene.entry.drawConfig = (u8)Field(entry, Schema::kDrawConfig, SCENE_DRAW_CFG_DEFAULT);
    sSceneIdsByKey[key] = sceneId;
    StorePath(sceneId, scenePath);
    AddEntrances(key, sceneId, Sub(entry, Schema::kEntrances));
    return true;
}

bool AddRegistryEntry(const std::string& key, const Json& entry) {
    if (key.find('/') != std::string::npos) {
        SPDLOG_ERROR("[Unbound] Registry scene id '{}' contains '/'; skipped", key);
        return false;
    }
    if (auto vanilla = VanillaSceneId(key)) {
        AddEntrances(key, *vanilla, Sub(entry, Schema::kEntrances));
        return true;
    }
    return AddCustomScene(key, entry);
}

void CopyCycle(CycleSceneFlags& cycle, const PermanentSceneFlags& permanent) {
    cycle = { permanent.chest, permanent.switch0, permanent.switch1, permanent.clearedRoom, permanent.collectible };
}

Json WriteFlags(const CustomScene& scene) {
    const CycleSceneFlags& cycle = scene.cycleFlags;
    return { { "chest", cycle.chest },
             { "switch0", cycle.switch0 },
             { "switch1", cycle.switch1 },
             { "clearedRoom", cycle.clearedRoom },
             { "collectible", cycle.collectible },
             { "unk_14", scene.permanentFlags.unk_14 },
             { "rooms", scene.permanentFlags.rooms } };
}

Json WriteFlagsSection() {
    Json section = Json::object();
    for (const CustomScene& scene : sCustomScenes) {
        section[scene.key] = WriteFlags(scene);
    }
    return section;
}

// The save keeps what the cycle reached, as vanilla saves copy the cycle flags into the permanent ones.
void ReadFlagsSection(const Json& section) {
    for (CustomScene& scene : sCustomScenes) {
        const Json& flags = Sub(section, scene.key.c_str());
        scene.permanentFlags = { (u32)Field(flags, "chest"),       (u32)Field(flags, "switch0"),
                                 (u32)Field(flags, "switch1"),     (u32)Field(flags, "clearedRoom"),
                                 (u32)Field(flags, "collectible"), (u32)Field(flags, "unk_14"),
                                 (u32)Field(flags, "rooms") };
        CopyCycle(scene.cycleFlags, scene.permanentFlags);
    }
}

void ResetCycleFlagsForNewCycle() {
    for (auto& [key, flags] : sRoomFlags) {
        flags.cleared.clear();
        flags.temporary.clear();
    }
    for (CustomScene& scene : sCustomScenes) {
        CycleSceneFlags& cycle = scene.cycleFlags;
        cycle.chest &= scene.persistent.chest;
        cycle.switch0 &= scene.persistent.switch0;
        cycle.switch1 &= scene.persistent.switch1;
        cycle.collectible &= scene.persistent.collectible;
        cycle.clearedRoom = 0;
    }
}

void LogMissingEntrance(u32 entrance) {
    static std::unordered_set<u32> sReported;
    if (sReported.insert(entrance).second) {
        SPDLOG_ERROR("[Unbound] Entrance 0x{:X} is not registered by any mounted archive", entrance);
    }
}

} // namespace

void SceneDB_Init() {
    sBaseMounted = ReadBaseManifest();
    const size_t accepted = ForEachRegistryEntry(Schema::kRegistryPath, "scene", AddRegistryEntry);
    SaveSections_Register(kFlagsSection, WriteFlagsSection, ReadFlagsSection);
    SaveSections_Register("unboundRoomFlags", WriteRoomFlags, ReadRoomFlags);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s16 sceneId, s8 spawn) {
        for (auto& [key, flags] : sRoomFlags) {
            flags.temporary.clear();
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::BeforeEndOfCycleSave>(ResetCycleFlagsForNewCycle);
    SPDLOG_INFO("[Unbound] Base {}; {} registry entries, {} custom scenes, {} custom entrances",
                sBaseMounted ? "mounted" : "not mounted", accepted, sCustomScenes.size(), sEntrancesByName.size());
}

bool SceneDB_IsBaseMounted() {
    return sBaseMounted;
}

extern "C" SceneTableEntry* SceneDB_GetSceneEntry(s16 sceneId) {
    if (IsVanillaScene(sceneId)) {
        return &gSceneTable[sceneId];
    }
    CustomScene* scene = FindCustomScene(sceneId);
    return scene != nullptr ? &scene->entry : &gSceneTable[SCENE_CLOCKTOWER];
}

extern "C" const char* SceneDB_GetScenePath(s16 sceneId) {
    auto cached = sScenePathById.find(sceneId);
    if (cached != sScenePathById.end()) {
        return cached->second;
    }
    return StorePath(sceneId, VanillaScenePath(SceneDB_GetSceneEntry(sceneId)->segment.fileName));
}

extern "C" EntranceTableEntry* SceneDB_GetCustomEntrance(u32 entrance) {
    const u32 slotIndex = (entrance >> 9) - ENTR_SCENE_MAX;
    const u32 spawnIndex = (entrance >> 4) & 0x1F;
    if (slotIndex >= sSlots.size() || spawnIndex >= sSlots[slotIndex].entries.size()) {
        LogMissingEntrance(entrance);
        return &sMissingEntrance;
    }
    return &sSlots[slotIndex].entries[spawnIndex];
}

extern "C" PermanentSceneFlags* SceneFlags_GetPermanent(s16 sceneId) {
    if (sceneId >= 0 && sceneId < ARRAY_COUNT(gSaveContext.save.saveInfo.permanentSceneFlags)) {
        return &gSaveContext.save.saveInfo.permanentSceneFlags[sceneId];
    }
    CustomScene* scene = FindCustomScene(sceneId);
    return scene != nullptr ? &scene->permanentFlags : &sScratchPermanentFlags;
}

extern "C" CycleSceneFlags* SceneFlags_GetCycle(s16 sceneId) {
    if (sceneId >= 0 && sceneId < ARRAY_COUNT(gSaveContext.cycleSceneFlags)) {
        return &gSaveContext.cycleSceneFlags[sceneId];
    }
    CustomScene* scene = FindCustomScene(sceneId);
    return scene != nullptr ? &scene->cycleFlags : &sScratchCycleFlags;
}

std::optional<u32> SceneDB_FindEntrance(const std::string& name) {
    auto entrance = sEntrancesByName.find(name);
    return entrance == sEntrancesByName.end() ? std::nullopt : std::optional<u32>(entrance->second);
}

std::string SceneDB_GetEntranceName(u32 entrance) {
    const u32 slotIndex = (entrance >> 9) - ENTR_SCENE_MAX;
    const u32 spawnIndex = (entrance >> 4) & 0x1F;
    if ((entrance >> 9) < ENTR_SCENE_MAX || slotIndex >= sSlots.size() ||
        spawnIndex >= sSlots[slotIndex].names.size()) {
        return "";
    }
    return sSlots[slotIndex].sceneKey + "/" + sSlots[slotIndex].names[spawnIndex];
}

std::optional<s16> SceneDB_FindScene(const std::string& key) {
    auto custom = sSceneIdsByKey.find(key);
    if (custom != sSceneIdsByKey.end()) {
        return custom->second;
    }
    return VanillaSceneId(key);
}

std::string SceneDB_GetSceneKey(s16 sceneId) {
    if (IsVanillaScene(sceneId)) {
        return gSceneTable[sceneId].segment.fileName;
    }
    CustomScene* scene = FindCustomScene(sceneId);
    return scene != nullptr ? scene->key : "";
}

extern "C" s32 SceneFlags_GetRoomVisited(s16 sceneId, s32 room) {
    if (room < 0 || room > INT16_MAX) {
        return false;
    }
    if (room < 32) {
        return (SceneFlags_GetPermanent(sceneId)->rooms & (1u << room)) != 0;
    }
    const auto flags = sRoomFlags.find(SceneDB_GetSceneKey(sceneId));
    return flags != sRoomFlags.end() && flags->second.visited.count(room) != 0;
}

extern "C" void SceneFlags_SetRoomVisited(s16 sceneId, s32 room) {
    if (room < 0 || room > INT16_MAX) {
        return;
    }
    if (room < 32) {
        SceneFlags_GetPermanent(sceneId)->rooms |= 1u << room;
        return;
    }
    const std::string key = SceneDB_GetSceneKey(sceneId);
    if (!key.empty()) {
        sRoomFlags[key].visited.insert(room);
    }
}

extern "C" void SceneFlags_ClearRoomVisited(s16 sceneId, s32 room) {
    if (room < 0 || room > INT16_MAX) return;
    if (room < 32) { SceneFlags_GetPermanent(sceneId)->rooms &= ~(1u << room); return; }
    const std::string key = SceneDB_GetSceneKey(sceneId);
    auto found = sRoomFlags.find(key);
    if (found != sRoomFlags.end()) found->second.visited.erase(room);
}

extern "C" s32 SceneFlags_GetRoomClear(s16 sceneId, s32 room, s32 temporary) {
    const auto found = sRoomFlags.find(SceneDB_GetSceneKey(sceneId));
    if (room < 32 || room > INT16_MAX || found == sRoomFlags.end()) {
        return false;
    }
    const auto& rooms = temporary ? found->second.temporary : found->second.cleared;
    return rooms.count(room) != 0;
}

extern "C" void SceneFlags_SetRoomClear(s16 sceneId, s32 room, s32 temporary, s32 value) {
    const std::string key = SceneDB_GetSceneKey(sceneId);
    if (room < 32 || room > INT16_MAX || key.empty()) {
        return;
    }
    auto& flags = sRoomFlags[key];
    auto& rooms = temporary ? flags.temporary : flags.cleared;
    if (value) {
        rooms.insert(room);
    } else {
        rooms.erase(room);
    }
}
