#include "FleetRando.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "FleetGame.h"
#include "2s2h/ModApi/RandoLogic/RandoLogic.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/Rando/MiscBehavior/MiscBehavior.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/ShipUtils.h"

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace {

using Rando::StaticData::RandoStaticItem;

uint32_t sSeed = 0;
// The game's own save while the oracle borrows gSaveContext; restored by the "Finish" request.
std::unique_ptr<SaveContext> sLiveSave;
// The prepared seed before any query: options, starting items, prices and confined placements.
std::unique_ptr<SaveContext> sSeedSave;

char* CopyString(const std::string& text) {
    char* copy = static_cast<char*>(std::malloc(text.size() + 1));
    std::memcpy(copy, text.c_str(), text.size() + 1);
    return copy;
}

// Checks travel by their readable name ("Moon Majora Pot 01"): the combined engine keys its win condition on it.
const std::unordered_map<std::string, RandoCheckId>& CheckIdsByName() {
    static const std::unordered_map<std::string, RandoCheckId> ids = [] {
        std::unordered_map<std::string, RandoCheckId> map;
        for (const auto& [id, name] : Rando::StaticData::CheckNames) {
            if (!name.empty()) {
                map.emplace(name, id);
            }
        }
        return map;
    }();
    return ids;
}

const std::unordered_map<std::string, RandoItemId>& ItemIdsByName() {
    static const std::unordered_map<std::string, RandoItemId> ids = [] {
        std::unordered_map<std::string, RandoItemId> map;
        for (const auto& [id, item] : Rando::StaticData::Items) {
            if (item.spoilerName != nullptr && item.spoilerName[0] != '\0') {
                map.emplace(item.spoilerName, id);
            }
        }
        return map;
    }();
    return ids;
}

const char* CheckName(RandoCheckId id) {
    auto name = Rando::StaticData::CheckNames.find(id);
    return name == Rando::StaticData::CheckNames.end() ? nullptr : name->second.c_str();
}

const char* ItemName(RandoItemId id) {
    auto item = Rando::StaticData::Items.find(id);
    return item == Rando::StaticData::Items.end() ? nullptr : item->second.spoilerName;
}

bool IsNamed(const char* name) {
    return name != nullptr && name[0] != '\0';
}

// Traps never gate logic, so they fill like junk.
bool IsAdvancement(const RandoStaticItem& item) {
    return item.randoItemType != RITYPE_JUNK && item.randoItemType != RITYPE_HEALTH && item.randoItemId != RI_TRAP;
}

const char* CategoryName(const RandoStaticItem& item) {
    switch (item.randoItemType) {
        case RITYPE_JUNK:
            return "junk";
        case RITYPE_LESSER:
            return "lesser";
        case RITYPE_HEALTH:
            return "health";
        case RITYPE_BOSS_KEY:
            return "bossKey";
        case RITYPE_SMALL_KEY:
            return "smallKey";
        case RITYPE_SKULLTULA_TOKEN:
            return "token";
        case RITYPE_MASK:
            return "mask";
        case RITYPE_STRAY_FAIRY:
            return "strayFairy";
        default:
            return "major";
    }
}

nlohmann::json FixedEntry(RandoCheckId check, RandoItemId item, bool hintable) {
    const RandoStaticItem& staticItem = Rando::StaticData::Items[item];
    return { { "check", CheckName(check) },
             { "item", staticItem.spoilerName },
             { "advancement", IsAdvancement(staticItem) },
             { "hintable", hintable } };
}

// The native generator's preparation up to its free fill, in a fresh save. Leaves the free pools behind.
void PrepareSeed(std::vector<RandoCheckId>& checkPool, std::vector<RandoItemId>& itemPool,
                 std::vector<RandoCheckId>& confinedChecks) {
    if (sLiveSave == nullptr) {
        sLiveSave = std::make_unique<SaveContext>(gSaveContext);
    }
    RandoSeedGenerationScope modApiScope;
    Sram_InitNewSave();
    Rando::MiscBehavior::PrepareRandoSave();
    Ship_Random_Seed(sSeed);
    Rando::MiscBehavior::PersistSeedOptions(sSeed);
    Rando::Logic::GeneratePools(gSaveContext.save.shipSaveInfo.rando, checkPool, itemPool);
    Rando::GrantStartingItems();

    const std::vector<RandoCheckId> allChecks = checkPool;
    Rando::Logic::PreplaceConfinedItems(checkPool, itemPool);
    const std::set<RandoCheckId> stillFree(checkPool.begin(), checkPool.end());
    for (RandoCheckId check : allChecks) {
        if (!stillFree.contains(check)) {
            confinedChecks.push_back(check);
        }
    }
    // GeneratePools leaves fewer items than checks; the native generator pads with junk the same way.
    while (itemPool.size() < checkPool.size()) {
        itemPool.push_back(RI_JUNK);
    }
    sSeedSave = std::make_unique<SaveContext>(gSaveContext);
}

const char* const kExcludedChecksKey = "gRando.ExcludedChecks";
const char* const kStartingItemsKey = "gRando.StartingItems";

// Every option lives in its CVar; excluded checks and starting items are config lists named by their tokens.
nlohmann::json DumpSettings() {
    nlohmann::json settings = nlohmann::json::object();
    for (const auto& [id, option] : Rando::StaticData::Options) {
        if (IsNamed(option.cvar)) {
            settings[option.cvar] = CVarGetInteger(option.cvar, option.defaultValue);
        }
    }
    nlohmann::json excluded = nlohmann::json::array();
    for (RandoCheckId check : Rando::GetExcludedChecksFromConfig()) {
        auto staticCheck = Rando::StaticData::Checks.find(check);
        if (staticCheck != Rando::StaticData::Checks.end() && IsNamed(staticCheck->second.name)) {
            excluded.push_back(staticCheck->second.name);
        }
    }
    settings[kExcludedChecksKey] = std::move(excluded);
    nlohmann::json startingItems = nlohmann::json::array();
    for (RandoItemId item : Rando::GetStartingItemsFromConfig()) {
        if (IsNamed(ItemName(item))) {
            startingItems.push_back(ItemName(item));
        }
    }
    settings[kStartingItemsKey] = std::move(startingItems);
    return { { "settings", std::move(settings) } };
}

// The snapshot is authoritative: an absent exclusion list clears the local one. The lists stay in memory,
// so the player's own config file is never rewritten.
void RestoreSettings(const nlohmann::json& snapshot) {
    const nlohmann::json settings = snapshot.value("settings", nlohmann::json::object());
    std::vector<RandoCheckId> excluded;
    for (const nlohmann::json& name : settings.value(kExcludedChecksKey, nlohmann::json::array())) {
        const RandoCheckId check =
            name.is_string() ? Rando::StaticData::GetCheckIdFromName(name.get<std::string>().c_str()) : RC_UNKNOWN;
        if (check != RC_UNKNOWN) {
            excluded.push_back(check);
        }
    }
    Rando::OverrideExcludedChecks(excluded);
    if (settings.contains(kStartingItemsKey)) {
        std::vector<RandoItemId> startingItems;
        for (const nlohmann::json& name : settings[kStartingItemsKey]) {
            auto item = name.is_string() ? ItemIdsByName().find(name.get<std::string>()) : ItemIdsByName().end();
            if (item != ItemIdsByName().end()) {
                startingItems.push_back(item->second);
            }
        }
        Rando::OverrideStartingItems(startingItems);
    }
    for (const auto& [cvar, value] : settings.items()) {
        if (value.is_number_integer()) {
            CVarSetInteger(cvar.c_str(), value.get<int32_t>());
        }
    }
}

void InitRandoHeadless() {
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;
    FleetGame_JoinHostContext();
    ShipInit::Init(Rando::Logic::RegionGraphPath);
    Rando::StaticData::PopulateCheckNames();
}

} // namespace

char* FleetRando_DumpStaticData(void) {
    std::vector<RandoCheckId> checkPool;
    std::vector<RandoItemId> itemPool;
    std::vector<RandoCheckId> confinedChecks;
    PrepareSeed(checkPool, itemPool, confinedChecks);

    nlohmann::json checks = nlohmann::json::array();
    for (RandoCheckId check : checkPool) {
        if (IsNamed(CheckName(check))) {
            checks.push_back({ { "name", CheckName(check) } });
        }
    }

    nlohmann::json pool = nlohmann::json::array();
    for (RandoItemId item : itemPool) {
        const RandoStaticItem& staticItem = Rando::StaticData::Items[item];
        if (IsNamed(staticItem.spoilerName)) {
            pool.push_back({ { "name", staticItem.spoilerName },
                             { "advancement", IsAdvancement(staticItem) },
                             { "category", CategoryName(staticItem) } });
        }
    }

    nlohmann::json fixed = nlohmann::json::array();
    std::set<RandoCheckId> fixedChecks(confinedChecks.begin(), confinedChecks.end());
    for (RandoCheckId check : confinedChecks) {
        const RandoItemId item = RANDO_SAVE_CHECKS[check].randoItemId;
        if (IsNamed(CheckName(check)) && IsNamed(ItemName(item))) {
            fixed.push_back(FixedEntry(check, item, true));
        }
    }
    const std::set<RandoCheckId> freeChecks(checkPool.begin(), checkPool.end());
    for (const auto& [id, check] : Rando::StaticData::Checks) {
        if (fixedChecks.contains(id) || freeChecks.contains(id) || !IsNamed(CheckName(id))) {
            continue;
        }
        // Unshuffled remains still gate the Moon, so the fill must credit them where they stand.
        const bool vanillaRemains =
            check.randoCheckType == RCTYPE_REMAINS && RANDO_SAVE_OPTIONS[RO_SHUFFLE_BOSS_REMAINS] == RO_GENERIC_NO;
        if (vanillaRemains && IsNamed(ItemName(check.randoItemId))) {
            fixed.push_back(FixedEntry(id, check.randoItemId, false));
        } else if (RANDO_SAVE_CHECKS[id].shuffled && IsNamed(ItemName(RANDO_SAVE_CHECKS[id].randoItemId))) {
            fixed.push_back(FixedEntry(id, RANDO_SAVE_CHECKS[id].randoItemId, true));
        }
    }

    nlohmann::json prices = nlohmann::json::object();
    for (const auto& [id, check] : Rando::StaticData::Checks) {
        const bool sold = check.randoCheckType == RCTYPE_SHOP || check.randoCheckType == RCTYPE_TINGLE_SHOP;
        if (sold && IsNamed(CheckName(id)) && RANDO_SAVE_CHECKS[id].price != 0) {
            prices[CheckName(id)] = RANDO_SAVE_CHECKS[id].price;
        }
    }

    nlohmann::json items = nlohmann::json::array();
    for (const auto& [id, item] : Rando::StaticData::Items) {
        if (!IsNamed(item.spoilerName)) {
            continue;
        }
        nlohmann::json entry = { { "name", item.spoilerName },
                                 { "token", item.spoilerName },
                                 { "advancement", IsAdvancement(item) },
                                 { "trap", id == RI_TRAP },
                                 { "trickNames", nlohmann::json::array() } };
        if (IsNamed(item.name)) {
            entry["displayName"] = item.name;
        }
        items.push_back(std::move(entry));
    }

    const nlohmann::json dump = { { "checks", std::move(checks) }, { "pool", std::move(pool) },
                                  { "fixed", std::move(fixed) },   { "items", std::move(items) },
                                  { "prices", std::move(prices) } };
    return CopyString(dump.dump());
}

void FleetRando_Reset(void) {
    // Assumes DumpStaticData prepared the seed first.
    assert(sSeedSave != nullptr);
    gSaveContext = *sSeedSave;
}

void FleetRando_SetOwnedItems(const char* itemsJson) {
    if (itemsJson == nullptr) {
        return;
    }
    const nlohmann::json items = nlohmann::json::parse(itemsJson, nullptr, false);
    if (!items.is_array()) {
        return;
    }
    const auto& ids = ItemIdsByName();
    for (const nlohmann::json& name : items) {
        auto item = ids.find(name.get<std::string>());
        if (item != ids.end() && item->second != RI_TRAP) {
            Rando::GiveItem(Rando::ConvertItem(item->second));
        }
    }
}

// GlitchlessLogic's fixpoint without placing anything: regions and events unlock each other until both settle.
char* FleetRando_GetReachableChecks(void) {
    std::set<RandoRegionId> reachable = { RR_MAX };
    auto timeStates = Rando::Logic::InitializeRegionTimeStates(RR_MAX);
    std::set<std::pair<RandoEvent, std::function<bool()>>*> applied;
    bool changed = true;
    while (changed) {
        const size_t previousSize = reachable.size();
        for (RandoRegionId region : std::set<RandoRegionId>(reachable)) {
            Rando::Logic::FindReachableRegions(region, reachable, timeStates);
        }
        changed = reachable.size() != previousSize;
        for (RandoRegionId regionId : reachable) {
            Rando::Logic::SetCurrentRegionTime(timeStates, regionId);
            for (auto& event : Rando::Logic::Regions[regionId].events) {
                if (!applied.contains(&event) && event.second()) {
                    RANDO_EVENTS[event.first]++;
                    applied.insert(&event);
                    changed = true;
                }
            }
        }
    }

    nlohmann::json names = nlohmann::json::array();
    for (RandoRegionId regionId : reachable) {
        Rando::Logic::SetCurrentRegionTime(timeStates, regionId);
        for (auto& [checkId, checkLogic] : Rando::Logic::Regions[regionId].checks) {
            if (checkLogic.first() && IsNamed(CheckName(checkId))) {
                names.push_back(CheckName(checkId));
            }
        }
    }
    return CopyString(names.dump());
}

bool FleetRando_PlaceItem(const char* check, const char* item) {
    if (check == nullptr || item == nullptr) {
        return false;
    }
    auto checkId = CheckIdsByName().find(check);
    auto itemId = ItemIdsByName().find(item);
    if (checkId == CheckIdsByName().end() || itemId == ItemIdsByName().end()) {
        return false;
    }
    RANDO_SAVE_CHECKS[checkId->second].randoItemId = itemId->second;
    RANDO_SAVE_CHECKS[checkId->second].shuffled = true;
    return true;
}

char* FleetRando_Request(const char* op, const char* json) {
    const std::string request = op != nullptr ? op : "";
    const nlohmann::json input = nlohmann::json::parse(json != nullptr ? json : "{}", nullptr, false);
    if (request == "InitRandoHeadless") {
        InitRandoHeadless();
        return CopyString("{}");
    }
    if (request == "SetSeed") {
        sSeed = input.value("seed", 0u);
        return CopyString("{}");
    }
    if (request == "DumpSettings") {
        return CopyString(DumpSettings().dump());
    }
    if (request == "RestoreSettings") {
        RestoreSettings(input.is_object() ? input : nlohmann::json::object());
        return CopyString("{}");
    }
    if (request == "Finish") {
        if (sLiveSave != nullptr) {
            gSaveContext = *sLiveSave;
            sLiveSave.reset();
        }
        sSeedSave.reset();
        Rando::OverrideExcludedChecks(std::nullopt);
        Rando::OverrideStartingItems(std::nullopt);
        return CopyString("{}");
    }
    SPDLOG_WARN("[Fleet] 2Ship has no randomizer service '{}'", request);
    return nullptr;
}

void FleetRando_FreeString(char* text) {
    std::free(text);
}
