#include "DungeonRegistry.h"
#include "LogicExpr.h"
#include <algorithm>
#include <deque>
#include <set>
#include <random>
#include <stdexcept>
#include <spdlog/spdlog.h>
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/ModApi/RandoOptions/RandoOptions.h"
#include "2s2h/SaveManager/SaveSections.h"
#include "2s2h/Unbound/SceneDB.h"
#include "2s2h/Unbound/UnboundJson.h"

namespace {
using S2H::Unbound::Json;
using S2H::Unbound::Field;
std::string StringField(const Json& object, const char* key, const std::string& fallback = "") {
    const auto value = object.find(key);
    return value != object.end() && value->is_string() ? value->get<std::string>() : fallback;
}
constexpr int KeySlot = 9;
struct Dungeon {
    std::string key, name;
    std::vector<s16> scenes;
    std::array<int32_t, 4> items = {};
    std::array<int32_t, 4> placement = {};
    std::array<int32_t, 4> poolCounts = {};
    std::array<std::string, 4> itemNames;
    std::vector<int32_t> checks;
    int32_t keys = 0, receivedKeys = 0, logicKeys = 0;
    uint8_t equipment = 0, logicEquipment = 0;
    int32_t outside = -1;
    uint32_t entrance = 0;
    Rando::Logic::RandoRegionExit entranceRule;
};
std::deque<Dungeon> dungeons;
std::deque<std::string> names;
std::map<int32_t, RandoSaveCheck> checkSaves;
std::map<int32_t, uint8_t> eventStates;
std::map<std::string, int32_t> namedEvents, namedRegions;
std::map<RandoRegionId, Rando::Logic::RandoRegion> registeredRegions;
std::map<int32_t, std::pair<int32_t, int32_t>> itemOwners;
int32_t nextCheck = RC_MAX + 1, nextRegion = RR_MAX + 1, nextEvent = RE_MAX + 1;
int32_t activeDungeon = -1;
struct EntrancePair { std::string key; uint32_t entry, exit; };
std::vector<EntrancePair> entrancePairs;
std::map<uint32_t, uint32_t> entranceOverrides;
std::map<std::string, std::string> entranceAssignment;
int8_t backupKeys = -1;
uint8_t backupItems = 0;
int32_t backupMapIndex = 0;
int32_t backupSharedIndex = 0;

const char* KeepName(const std::string& name) {
    names.push_back(name);
    return names.back().c_str();
}
bool Simulating() { return RandoInfCustom_GetActiveBank() == S2H_RANDO_INF_LOGIC; }
void CaptureKeys() {
    if (activeDungeon < 0) { return; }
    auto& dungeon = dungeons[activeDungeon];
    dungeon.keys = std::max(0, (int)gSaveContext.save.saveInfo.inventory.dungeonKeys[KeySlot]);
    dungeon.equipment = gSaveContext.save.saveInfo.inventory.dungeonItems[KeySlot];
}
void RestoreKeys() {
    if (activeDungeon < 0) { return; }
    CaptureKeys();
    gSaveContext.save.saveInfo.inventory.dungeonKeys[KeySlot] = backupKeys;
    gSaveContext.save.saveInfo.inventory.dungeonItems[KeySlot] = backupItems;
    if (gSaveContext.mapIndex == KeySlot) { gSaveContext.mapIndex = backupMapIndex; }
    if (gSaveContext.dungeonSceneSharedIndex == KeySlot) { gSaveContext.dungeonSceneSharedIndex = backupSharedIndex; }
    activeDungeon = -1;
}
void EnterScene(s16 scene) {
    RestoreKeys();
    activeDungeon = DungeonRegistry_FindScene(scene);
    if (activeDungeon < 0) { return; }
    backupKeys = gSaveContext.save.saveInfo.inventory.dungeonKeys[KeySlot];
    backupItems = gSaveContext.save.saveInfo.inventory.dungeonItems[KeySlot];
    backupMapIndex = gSaveContext.mapIndex;
    backupSharedIndex = gSaveContext.dungeonSceneSharedIndex;
    const auto& dungeon = dungeons[activeDungeon];
    gSaveContext.save.saveInfo.inventory.dungeonKeys[KeySlot] = dungeon.keys;
    gSaveContext.save.saveInfo.inventory.dungeonItems[KeySlot] = dungeon.equipment;
    gSaveContext.mapIndex = KeySlot;
    gSaveContext.dungeonSceneSharedIndex = KeySlot;
}
std::function<bool()> Condition(const std::string& text, int32_t dungeon) {
    auto expression = std::make_shared<LogicExpr>();
    std::string error;
    if (!expression->Compile(text, error, dungeon)) { throw std::runtime_error(error); }
    return [expression]() { return expression->Evaluate(); };
}
s16 Scene(const std::string& key) {
    const auto scene = SceneDB_FindScene(key);
    if (!scene) { throw std::runtime_error("unknown scene '" + key + "'"); }
    return *scene;
}
int32_t ParseItem(int32_t dungeon, const std::string& item) {
    static const char* local[] = { "@smallkey", "@bosskey", "@map", "@compass" };
    for (int i = 0; i < 4; i++) {
        if (item == local[i]) {
            if (!dungeons[dungeon].items[i]) { throw std::runtime_error("undeclared dungeon item '" + item + "'"); }
            return dungeons[dungeon].items[i];
        }
    }
    const int32_t symbol = DungeonRegistry_FindSymbol(item);
    if (symbol >= 0 && item.starts_with("RI_")) { return symbol; }
    const int32_t custom = RandoItems_GetRandoGet(item.c_str());
    if (custom) { return custom; }
    for (const auto& [id, row] : Rando::StaticData::Items) {
        if ((row.name && item == row.name) || (row.spoilerName && item == row.spoilerName)) { return id; }
    }
    throw std::runtime_error("unknown item '" + item + "'");
}
void AddItems(int32_t index, const Json& definition) {
    auto& dungeon = dungeons[index];
    const char* keys[] = { "smallKeys", "bossKey", "map", "compass" };
    const char* labels[] = { " Small Key", " Boss Key", " Map", " Compass" };
    const RandoItemType types[] = { RITYPE_SMALL_KEY, RITYPE_BOSS_KEY, RITYPE_LESSER, RITYPE_LESSER };
    for (int kind = 0; kind < 4; kind++) {
        const int32_t count = Field(definition, keys[kind]);
        if (count < 0 || count > 127 || (kind != 0 && count > 1)) { throw std::runtime_error("invalid dungeon item count"); }
        if (!count) { continue; }
        dungeon.poolCounts[kind] = count;
        const std::string key = dungeon.key + "/" + keys[kind];
        const int32_t item = RandoItems_Register(key.c_str());
        if (!item) { throw std::runtime_error("dungeon item registry is full"); }
        dungeon.items[kind] = item;
        static const char* placementFields[] = { "smallKeyPlacement", "bossKeyPlacement", "mapPlacement", "compassPlacement" };
        dungeon.placement[kind] = Field(definition, placementFields[kind], kind < 2 ? -1 : RO_DUNGEON_ITEM_OWN_DUNGEON);
        if (dungeon.placement[kind] < -2 || dungeon.placement[kind] > 4) {
            throw std::runtime_error("invalid dungeon item placement mode");
        }
        dungeon.itemNames[kind] = dungeon.name + labels[kind];
        itemOwners[item] = { index, kind };
        const char* name = dungeon.itemNames[kind].c_str();
        Rando::StaticData::Items[(RandoItemId)item] = {
            (RandoItemId)item, name, "a", name, types[kind],
            kind == 0 ? ITEM_KEY_SMALL : kind == 1 ? ITEM_KEY_BOSS : kind == 2 ? ITEM_DUNGEON_MAP : ITEM_COMPASS,
            kind == 0 ? GI_KEY_SMALL : kind == 1 ? GI_KEY_BOSS : kind == 2 ? GI_MAP : GI_COMPASS,
            kind == 0 ? GID_KEY_SMALL : kind == 1 ? GID_KEY_BOSS : kind == 2 ? GID_DUNGEON_MAP : GID_COMPASS
        };
    }
}
void LoadDungeon(const std::string& key, const Json& definition) {
    if (!definition.is_object()) { throw std::runtime_error("dungeon must be an object"); }
    Dungeon dungeon;
    dungeon.key = key;
    dungeon.name = StringField(definition, "name", key);
    for (const auto& value : definition.at("scenes")) { dungeon.scenes.push_back(Scene(value.get<std::string>())); }
    if (dungeon.scenes.empty()) { throw std::runtime_error("dungeon has no scenes"); }
    for (s16 scene : dungeon.scenes) {
        if (DungeonRegistry_FindScene(scene) >= 0) { throw std::runtime_error("scene belongs to another dungeon"); }
    }
    const auto entrance = SceneDB_FindEntrance(definition.at("entrance").get<std::string>());
    const auto exit = SceneDB_FindEntrance(definition.at("exit").get<std::string>());
    if (!entrance || !exit) { throw std::runtime_error("unknown dungeon entrance/exit"); }
    const int32_t outside = DungeonRegistry_FindRegion(definition.at("overworldRegion").get<std::string>());
    if (outside < 0 || !Rando::Logic::Regions.contains((RandoRegionId)outside)) { throw std::runtime_error("unknown overworld region"); }
    const int32_t index = dungeons.size();
    dungeons.push_back(std::move(dungeon));
    AddItems(index, definition);
    const Json& regions = definition.at("regions");
    for (const auto& [name, region] : regions.items()) {
        const int32_t id = nextRegion++;
        namedRegions[key + "/" + name] = id;
        auto& row = Rando::Logic::Regions[(RandoRegionId)id];
        row.name = KeepName(dungeons[index].name + " " + name);
        row.sceneId = (SceneId)(region.contains("scene") ? Scene(region.at("scene").get<std::string>()) : dungeons[index].scenes.front());
        if (region.contains("events")) {
            for (const auto& [event, _] : region.at("events").items()) {
                namedEvents[key + "/" + event] = nextEvent++;
            }
        }
    }
    auto resolve = [&](const std::string& name) {
        if (name == "@outside") { return outside; }
        const auto found = namedRegions.find(key + "/" + name);
        if (found == namedRegions.end()) { throw std::runtime_error("unknown region '" + name + "'"); }
        return found->second;
    };
    for (const auto& [name, definition] : regions.items()) {
        auto& region = Rando::Logic::Regions[(RandoRegionId)resolve(name)];
        if (definition.contains("events")) {
            for (const auto& [name, text] : definition.at("events").items()) {
                region.events.push_back({ (RandoEvent)namedEvents.at(key + "/" + name), Condition(text.get<std::string>(), index) });
            }
        }
        if (definition.contains("exits")) {
            for (const auto& [to, text] : definition.at("exits").items()) {
                if (to == "@outside") {
                    region.exits[(s32)*exit] = { (s32)*entrance, Condition(text.get<std::string>(), index), text.get<std::string>() };
                    continue;
                }
                region.connections[(RandoRegionId)resolve(to)] = { Condition(text.get<std::string>(), index), text.get<std::string>() };
            }
        }
        if (definition.contains("checks")) {
            for (const auto& [name, check] : definition.at("checks").items()) {
                if (nextCheck > INT16_MAX) { throw std::runtime_error("check id does not fit MM's pickup parameter"); }
                const int32_t id = nextCheck++;
                const std::string type = StringField(check, "type", "chest");
                if (type != "chest" && type != "collectible") { throw std::runtime_error("unsupported check type"); }
                const int32_t flag = Field(check, "flag", -1);
                if (flag < 0 || flag >= (type == "chest" ? 32 : 128)) { throw std::runtime_error("invalid check flag"); }
                const s16 scene = check.contains("scene") ? Scene(check.at("scene").get<std::string>()) : region.sceneId;
                const char* label = KeepName(dungeons[index].name + " " + name);
                const std::string text = StringField(check, "logic", "true");
                const auto predicate = Condition(text, index);
                const auto item = (RandoItemId)ParseItem(index, StringField(check, "vanilla", "RI_JUNK"));
                Rando::StaticData::Checks[(RandoCheckId)id] = {
                    (RandoCheckId)id, label, type == "chest" ? RCTYPE_CHEST : RCTYPE_FREESTANDING,
                    (SceneId)scene, type == "chest" ? FLAG_CYCL_SCENE_CHEST : FLAG_CYCL_SCENE_COLLECTIBLE,
                    flag, item
                };
                Rando::StaticData::CheckNames[(RandoCheckId)id] = label;
                region.checks[(RandoCheckId)id] = { predicate, text };
                dungeons[index].checks.push_back(id);
            }
        }
    }
    const int32_t entry = resolve(definition.at("entryRegion").get<std::string>());
    auto& entryRegion = Rando::Logic::Regions[(RandoRegionId)entry];
    entryRegion.exits[(s32)*exit] = { (s32)*entrance, []() { return true; }, "true" };
    Rando::Logic::Regions[(RandoRegionId)outside].exits[(s32)*entrance] = {
        (s32)*exit, Condition(StringField(definition, "entranceLogic", "true"), index), StringField(definition, "entranceLogic", "true")
    };
    auto& registered = dungeons[index];
    registered.outside = outside;
    registered.entrance = *entrance;
    registered.entranceRule = Rando::Logic::Regions[(RandoRegionId)outside].exits[(s32)*entrance];
    for (const auto& [name, _] : regions.items()) {
        const auto id = (RandoRegionId)resolve(name);
        registeredRegions[id] = Rando::Logic::Regions[id];
    }
    entrancePairs.push_back({ key, *entrance, *exit });
}
Json WriteState() {
    CaptureKeys();
    Json state = Json::object();
    state["$entrances"] = entranceAssignment;
    for (const auto& dungeon : dungeons) {
        auto& row = state[dungeon.key];
        row["keys"] = dungeon.keys;
        row["receivedKeys"] = dungeon.receivedKeys;
        row["items"] = dungeon.equipment;
        for (int32_t check : dungeon.checks) {
            const auto& saved = checkSaves[check];
            const auto& item = Rando::StaticData::Items[saved.randoItemId];
            const char* key = RandoItems_GetKey(saved.randoItemId);
            row["checks"][Rando::StaticData::Checks[(RandoCheckId)check].name] = {
                { "item", key ? key : (item.spoilerName ? item.spoilerName : "") },
                { "shuffled", saved.shuffled }, { "obtained", saved.obtained },
                { "cycleObtained", saved.cycleObtained }, { "skipped", saved.skipped }, { "price", saved.price }
            };
        }
    }
    return state;
}
void ReadState(const Json& state) {
    RestoreKeys();
    checkSaves.clear();
    eventStates.clear();
    entranceAssignment.clear();
    entranceOverrides.clear();
    if (state.is_object() && state.contains("$entrances") && state["$entrances"].is_object()) {
        std::set<std::string> used;
        bool valid = true;
        for (const auto& source : entrancePairs) {
            const auto targetKey = state["$entrances"].find(source.key);
            if (targetKey == state["$entrances"].end() || !targetKey->is_string()) { valid = false; break; }
            const auto target = std::find_if(entrancePairs.begin(), entrancePairs.end(), [&](const auto& pair) { return pair.key == targetKey->get<std::string>(); });
            if (target == entrancePairs.end() || !used.insert(target->key).second) { valid = false; break; }
            entranceAssignment[source.key] = target->key;
            entranceOverrides[source.entry] = target->entry;
            entranceOverrides[target->exit] = source.exit;
        }
        if (!valid) {
            SPDLOG_WARN("[DungeonRegistry] Saved entrance assignment is incomplete; using original entrances");
            entranceAssignment.clear();
            entranceOverrides.clear();
        }
    }
    for (int32_t index = 0; index < dungeons.size(); index++) {
        auto& dungeon = dungeons[index];
        dungeon.keys = dungeon.receivedKeys = dungeon.equipment = 0;
        dungeon.logicKeys = dungeon.logicEquipment = 0;
        const Json row = state.is_object() ? state.value(dungeon.key, Json::object()) : Json::object();
        if (!row.is_object()) { continue; }
        dungeon.keys = std::clamp((int)Field(row, "keys"), 0, 127);
        dungeon.receivedKeys = std::clamp((int)Field(row, "receivedKeys", dungeon.keys), 0, 127);
        dungeon.equipment = Field(row, "items") & 7;
        const auto checks = row.find("checks");
        for (int32_t id : dungeon.checks) {
            auto& saved = checkSaves[id];
            saved.randoItemId = Rando::StaticData::Checks[(RandoCheckId)id].randoItemId;
            if (checks == row.end() || !checks->is_object()) { continue; }
            const auto check = checks->find(Rando::StaticData::Checks[(RandoCheckId)id].name);
            if (check == checks->end() || !check->is_object()) { continue; }
            try { saved.randoItemId = (RandoItemId)ParseItem(index, StringField(*check, "item")); }
            catch (const std::exception& error) { SPDLOG_WARN("[DungeonRegistry] {}", error.what()); }
            saved.shuffled = Field(*check, "shuffled");
            saved.obtained = Field(*check, "obtained");
            saved.cycleObtained = Field(*check, "cycleObtained");
            saved.skipped = Field(*check, "skipped");
            saved.price = std::clamp((int)Field(*check, "price"), 0, 65535);
        }
    }
}
}

int32_t DungeonRegistry_FindScene(int32_t scene) {
    for (int32_t index = 0; index < dungeons.size(); index++) {
        const auto& scenes = dungeons[index].scenes;
        if (std::find(scenes.begin(), scenes.end(), scene) != scenes.end()) { return index; }
    }
    return -1;
}
extern "C" int32_t DungeonRegistry_KeyIndex(int32_t scene) { return DungeonRegistry_FindScene(scene) < 0 ? -1 : KeySlot; }
extern "C" uint32_t DungeonRegistry_ResolveEntrance(uint32_t entrance) {
    if (!IS_RANDO) { return entrance; }
    const auto found = entranceOverrides.find(entrance & ~15u);
    return found == entranceOverrides.end() ? entrance : found->second | (entrance & 15u);
}
void DungeonRegistry_AssignEntrances(uint32_t seed) {
    entranceAssignment.clear();
    entranceOverrides.clear();
    if (!RandoOptions_GetForSeed("unbound.dungeonShuffle")) { return; }
    std::vector<size_t> order;
    for (size_t i = 0; i < entrancePairs.size(); i++) { order.push_back(i); }
    std::mt19937 random(seed);
    for (size_t i = order.size(); i > 1; i--) { std::swap(order[i - 1], order[random() % i]); }
    for (size_t i = 0; i < order.size(); i++) {
        const auto& source = entrancePairs[i];
        const auto& target = entrancePairs[order[i]];
        entranceAssignment[source.key] = target.key;
        entranceOverrides[source.entry] = target.entry;
        entranceOverrides[target.exit] = source.exit;
    }
}
int32_t DungeonRegistry_FindItem(int32_t item) {
    const auto found = itemOwners.find(item);
    return found == itemOwners.end() ? -1 : found->second.first;
}
namespace {
int32_t Placement(int32_t index, int32_t kind) {
    if (dungeons[index].placement[kind] != -1) { return dungeons[index].placement[kind]; }
    return kind == 0 ? RANDO_SAVE_OPTIONS[RO_PLACEMENT_SMALL_KEYS] :
           kind == 1 ? RANDO_SAVE_OPTIONS[RO_PLACEMENT_BOSS_KEYS] : dungeons[index].placement[kind];
}
}
int32_t DungeonRegistry_ConfinedItem(int32_t item) {
    const auto owner = itemOwners.find(item);
    if (owner == itemOwners.end()) { return -1; }
    const auto [index, kind] = owner->second;
    return Placement(index, kind) == RO_DUNGEON_ITEM_OWN_DUNGEON ? index + 9 : -1;
}
bool DungeonRegistry_ItemAllowed(int32_t item, bool dungeonCheck) {
    const auto owner = itemOwners.find(item);
    if (owner == itemOwners.end()) { return true; }
    const int32_t placement = Placement(owner->second.first, owner->second.second);
    return placement == 3 ? dungeonCheck : placement == 4 ? !dungeonCheck : true;
}
void DungeonRegistry_AdjustPool(std::vector<RandoItemId>& items) {
    for (int32_t index = 0; index < dungeons.size(); index++) {
        for (int32_t kind = 0; kind < 4; kind++) {
            auto& dungeon = dungeons[index];
            if (!dungeon.items[kind]) { continue; }
            const int32_t placement = Placement(index, kind);
            std::erase(items, (RandoItemId)dungeon.items[kind]);
            if (placement == RO_DUNGEON_ITEM_START_WITH) {
                if (kind == 0) { dungeon.keys = dungeon.receivedKeys = dungeon.poolCounts[kind]; }
                else { dungeon.equipment |= kind == 1 ? 1 : kind == 2 ? 4 : 2; }
            }
            if (placement != -2 && placement != RO_DUNGEON_ITEM_START_WITH) {
                items.insert(items.end(), dungeon.poolCounts[kind], (RandoItemId)dungeon.items[kind]);
            }
        }
    }
}
bool DungeonRegistry_AddCheckToPool(RandoSaveInfo& save, RandoCheckId check,
                                   std::vector<RandoCheckId>& checks, std::vector<RandoItemId>& items) {
    if (check <= RC_MAX) { return false; }
    if (std::find(checks.begin(), checks.end(), check) != checks.end()) { return true; }
    auto& saved = DungeonRegistry_SaveCheck(save, check);
    const RandoItemId item = Rando::StaticData::Checks[check].randoItemId;
    saved.randoItemId = item;
    const auto owner = itemOwners.find(item);
    if (owner != itemOwners.end()) {
        const auto [index, kind] = owner->second;
        const int32_t placement = Placement(index, kind);
        if (placement == -2) { return true; }
        if (placement == RO_DUNGEON_ITEM_START_WITH) {
            auto& dungeon = dungeons[index];
            if (kind == 0) { dungeon.keys = dungeon.receivedKeys = std::min(127, dungeon.keys + 1); }
            else { dungeon.equipment |= kind == 1 ? 1 : kind == 2 ? 4 : 2; }
            saved.shuffled = true;
            checks.push_back(check);
            items.push_back(RI_JUNK);
            return true;
        }
    }
    saved.shuffled = true;
    checks.push_back(check);
    items.push_back(item);
    return true;
}
bool DungeonRegistry_HasItem(int32_t item) {
    const auto owner = itemOwners.find(item);
    if (owner == itemOwners.end()) { return false; }
    const auto [index, kind] = owner->second;
    if (!Simulating()) { CaptureKeys(); }
    const auto& dungeon = dungeons[index];
    if (kind == 0) { return (Simulating() ? std::max(dungeon.logicKeys, dungeon.keys) : dungeon.keys) > 0; }
    return ((Simulating() ? dungeon.logicEquipment | dungeon.equipment : dungeon.equipment) & (kind == 1 ? 1 : kind == 2 ? 4 : 2)) != 0;
}
bool DungeonRegistry_GrantItem(int32_t item, bool remove) {
    const auto owner = itemOwners.find(item);
    if (owner == itemOwners.end()) { return false; }
    const auto [index, kind] = owner->second;
    auto& dungeon = dungeons[index];
    if (!Simulating()) { CaptureKeys(); }
    int32_t& keys = Simulating() ? dungeon.logicKeys : dungeon.keys;
    uint8_t& equipment = Simulating() ? dungeon.logicEquipment : dungeon.equipment;
    if (kind == 0) {
        keys = std::clamp(keys + (remove ? -1 : 1), 0, 127);
        if (!Simulating()) { dungeon.receivedKeys = std::clamp(dungeon.receivedKeys + (remove ? -1 : 1), 0, 127); }
    }
    else {
        const uint8_t mask = kind == 1 ? 1 : (kind == 2 ? 4 : 2);
        equipment = remove ? equipment & ~mask : equipment | mask;
    }
    if (!Simulating() && activeDungeon == index) {
        gSaveContext.save.saveInfo.inventory.dungeonKeys[KeySlot] = keys;
        gSaveContext.save.saveInfo.inventory.dungeonItems[KeySlot] = equipment;
    }
    return true;
}
bool DungeonRegistry_HasKeys(int32_t dungeon, int32_t count) {
    if (!Simulating()) { CaptureKeys(); }
    return dungeon >= 0 && dungeon < dungeons.size() &&
           std::max(dungeons[dungeon].keys, dungeons[dungeon].logicKeys) >= count;
}
bool DungeonRegistry_HasBossKey(int32_t dungeon) {
    if (!Simulating()) { CaptureKeys(); }
    return dungeon >= 0 && dungeon < dungeons.size() &&
           (!dungeons[dungeon].items[1] || ((dungeons[dungeon].logicEquipment | dungeons[dungeon].equipment) & 1));
}
bool DungeonRegistry_GetEvent(int32_t dungeon, const std::string& name) {
    if (dungeon < 0 || dungeon >= dungeons.size()) { return false; }
    const auto event = namedEvents.find(dungeons[dungeon].key + "/" + name);
    return event != namedEvents.end() && eventStates[event->second];
}
RandoSaveCheck& DungeonRegistry_SaveCheck(RandoSaveInfo& save, int32_t check) {
    if (check >= 0 && check < RC_MAX) { return save.randoSaveChecks[check]; }
    if (check <= RC_MAX || check >= nextCheck) { throw std::out_of_range("unknown randomizer check"); }
    return checkSaves[check];
}
uint8_t& DungeonRegistry_LogicEvent(int32_t event) {
    if (event >= 0 && event < RE_MAX) { return gSaveContext.save.shipSaveInfo.rando.randoEvents[event]; }
    if (event <= RE_MAX || event >= nextEvent) { throw std::out_of_range("unknown randomizer event"); }
    return eventStates[event];
}
void DungeonRegistry_BeginSeed() {
    for (const auto& [id, region] : registeredRegions) { Rando::Logic::Regions[id] = region; }
    for (const auto& dungeon : dungeons) {
        Rando::Logic::Regions[(RandoRegionId)dungeon.outside].exits[(s32)dungeon.entrance] = dungeon.entranceRule;
    }
    checkSaves.clear();
    eventStates.clear();
    for (auto& dungeon : dungeons) { dungeon.logicKeys = dungeon.logicEquipment = dungeon.keys = dungeon.receivedKeys = dungeon.equipment = 0; }
}
void DungeonRegistry_EndSeed() {
    eventStates.clear();
    for (auto& dungeon : dungeons) { dungeon.logicKeys = dungeon.logicEquipment = 0; }
}
void DungeonRegistry_WriteSpoiler(Json& spoiler) { spoiler["unboundDungeons"] = WriteState(); }
void DungeonRegistry_ReadSpoiler(const Json& spoiler) { ReadState(spoiler.value("unboundDungeons", Json())); }
void DungeonRegistry_Init() {
    entrancePairs = {
        { "vanilla.woodfall", ENTRANCE(WOODFALL_TEMPLE, 0), ENTRANCE(WOODFALL, 1) },
        { "vanilla.snowhead", ENTRANCE(SNOWHEAD_TEMPLE, 0), ENTRANCE(SNOWHEAD, 1) },
        { "vanilla.greatBay", ENTRANCE(GREAT_BAY_TEMPLE, 0), ENTRANCE(ZORA_CAPE, 7) },
        { "vanilla.stoneTower", ENTRANCE(STONE_TOWER_TEMPLE, 0), ENTRANCE(STONE_TOWER, 2) }
    };
    static const char* choices[] = { "Off", "On" };
    S2HModRandoOption option = { sizeof(option), "unbound.dungeonShuffle", "Dungeon entrance shuffle",
        "Shuffle the four MM dungeon doors and registered mod dungeon doors as coupled entrance/exit pairs.", choices, 2, 0, S2H_MOD_RANDO_WIDGET_COMBOBOX };
    RandoOptions_Register(&option);
    SaveSections_Register("unboundDungeons", WriteState, ReadState);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSaveInit>([](s16) {
        if (!IS_RANDO) {
            DungeonRegistry_BeginSeed();
            entranceOverrides.clear();
            entranceAssignment.clear();
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s16 scene, s8) { EnterScene(scene); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDestroy>([]() { RestoreKeys(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::BeforeEndOfCycleSave>([]() {
        RestoreKeys();
        for (auto& [_, check] : checkSaves) { check.cycleObtained = false; }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::AfterEndOfCycleSave>([]() {
        for (auto& dungeon : dungeons) {
            dungeon.keys = IS_RANDO ? dungeon.receivedKeys : 0;
            if (!IS_RANDO) { dungeon.equipment = 0; }
        }
    });
}
void DungeonRegistry_Load() {
    const auto registry = S2H::Unbound::LoadMergedJson("unbound/dungeons.json");
    if (!registry.is_object()) { return; }
    for (const auto& key : S2H::Unbound::ListKeys(registry)) {
        const int32_t firstCheck = nextCheck, firstRegion = nextRegion, firstEvent = nextEvent;
        const size_t firstDungeon = dungeons.size(), firstName = names.size(), firstPair = entrancePairs.size();
        try { LoadDungeon(key, registry.at(key)); }
        catch (const std::exception& error) {
            SPDLOG_ERROR("[DungeonRegistry] '{}': {}; entry rejected", key, error.what());
            std::erase_if(Rando::StaticData::Checks, [&](const auto& row) { return row.first >= firstCheck; });
            std::erase_if(Rando::StaticData::CheckNames, [&](const auto& row) { return row.first >= firstCheck; });
            std::erase_if(Rando::Logic::Regions, [&](const auto& row) { return row.first >= firstRegion; });
            std::erase_if(registeredRegions, [&](const auto& row) { return row.first >= firstRegion; });
            std::erase_if(namedRegions, [&](const auto& row) { return row.second >= firstRegion; });
            std::erase_if(namedEvents, [&](const auto& row) { return row.second >= firstEvent; });
            std::erase_if(itemOwners, [&](const auto& row) {
                if (row.second.first < firstDungeon) { return false; }
                Rando::StaticData::Items.erase((RandoItemId)row.first);
                return true;
            });
            while (dungeons.size() > firstDungeon) { dungeons.pop_back(); }
            while (names.size() > firstName) { names.pop_back(); }
            entrancePairs.resize(firstPair);
            nextCheck = firstCheck; nextRegion = firstRegion; nextEvent = firstEvent;
        }
    }
}
int32_t DungeonRegistry_FindSymbol(const std::string& name) {
    static const std::map<std::string, int32_t> symbols = {
#include "DungeonSymbols.inc"
    };
    const auto symbol = symbols.find(name);
    return symbol == symbols.end() ? -1 : symbol->second;
}
int32_t DungeonRegistry_FindRegion(const std::string& name) {
    const auto custom = namedRegions.find(name);
    return custom == namedRegions.end() ? DungeonRegistry_FindSymbol(name) : custom->second;
}
