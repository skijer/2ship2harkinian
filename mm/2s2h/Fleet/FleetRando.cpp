#include "FleetRando.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "FleetCrossDungeon.h"
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

// The combined Triforce Hunt. pieces is this game's share, -1 = unset; the required count is combined.
struct ComboOptions {
    bool hunt = false;
    uint32_t required = 0;
    int pieces = -1;
};
ComboOptions sComboOptions;

// The game's own save while the oracle borrows gSaveContext; restored by the "Finish" request.
std::unique_ptr<SaveContext> sLiveSave;
// The prepared seed before any query: options, starting items, prices and confined placements.
std::unique_ptr<SaveContext> sSeedSave;

char* CopyString(const std::string& text) {
    char* copy = static_cast<char*>(std::malloc(text.size() + 1));
    std::memcpy(copy, text.c_str(), text.size() + 1);
    return copy;
}

// Checks and items travel by their unique readable names ("Moon Majora Pot 01", "Piece of the Triforce"):
// the combined engine keys its win conditions on them.
const char* CheckName(RandoCheckId id) {
    return Rando::StaticData::GetCheckDisplayName(id).c_str();
}

const char* ItemName(RandoItemId id) {
    return Rando::StaticData::GetItemDisplayName(id).c_str();
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
             { "item", ItemName(item) },
             { "advancement", IsAdvancement(staticItem) },
             { "hintable", hintable } };
}

// The combined goal replaces the player's own hunt settings in the seed's options.
void ApplyComboOptions() {
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_TRIFORCE_PIECES] = sComboOptions.hunt ? RO_GENERIC_YES : RO_GENERIC_NO;
    if (!sComboOptions.hunt) {
        return;
    }
    RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_REQUIRED] = sComboOptions.required;
    if (sComboOptions.pieces >= 0) {
        RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_MAX] = static_cast<uint32_t>(sComboOptions.pieces);
    }
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
    ApplyComboOptions();
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
        if (IsNamed(Rando::StaticData::Items[item].spoilerName)) {
            startingItems.push_back(Rando::StaticData::Items[item].spoilerName);
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
            const RandoItemId item =
                name.is_string() ? Rando::StaticData::GetItemIdFromName(name.get<std::string>().c_str()) : RI_UNKNOWN;
            if (item != RI_UNKNOWN) {
                startingItems.push_back(item);
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

// The cross-game entrance services; nullopt for any other request.
std::optional<nlohmann::json> HandleCrossDungeonRequest(const std::string& request, const nlohmann::json& input) {
    if (request == "DumpCrossDungeonCatalogue") {
        return FleetCrossDungeon_DumpCatalogue(input);
    }
    if (request == "DumpCrossDungeonCatalogueForSeed") {
        return FleetCrossDungeon_DumpCatalogueForSeed(input);
    }
    if (request == "SetCrossDungeonSeed") {
        FleetCrossDungeon_SetSeed(input);
        return nlohmann::json::object();
    }
    if (request == "SetCrossDungeonAccess") {
        FleetCrossDungeon_SetAccess(input);
        return nlohmann::json::object();
    }
    if (request == "GetCrossDungeonExits") {
        return FleetCrossDungeon_GetExits();
    }
    if (request == "CanEnterCrossEntrance") {
        return nlohmann::json{ { "allowed", FleetCrossDungeon_CanEnterArrival(input.value("entrance", -1)) } };
    }
    return std::nullopt;
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
        if (IsNamed(ItemName(item))) {
            pool.push_back({ { "name", ItemName(item) },
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
        if (!IsNamed(ItemName(id))) {
            continue;
        }
        nlohmann::json entry = { { "name", ItemName(id) },
                                 { "token", item.spoilerName },
                                 { "advancement", IsAdvancement(item) },
                                 { "trap", id == RI_TRAP },
                                 { "trickNames", Rando::StaticData::GetTrickNames(id) },
                                 { "weightClass", Rando::GetItemHintWeight(id) } };
        if (IsNamed(item.name)) {
            entry["displayName"] = item.name;
        }
        items.push_back(std::move(entry));
    }

    // Cross-game hints phrase MM checks the way MM's own gossip stones do.
    nlohmann::json locationHints = nlohmann::json::object();
    for (const auto& [id, check] : Rando::StaticData::Checks) {
        if (IsNamed(CheckName(id))) {
            locationHints[CheckName(id)] = Rando::StaticData::GetLocationNameForHint(id, false);
        }
    }
    const nlohmann::json options = { { "RO_HINTS_GOSSIP_STONES", RANDO_SAVE_OPTIONS[RO_HINTS_GOSSIP_STONES] },
                                     { "RO_HINTS_PURCHASEABLE", RANDO_SAVE_OPTIONS[RO_HINTS_PURCHASEABLE] } };
    nlohmann::json junkPool = nlohmann::json::array();
    for (RandoItemId item : Rando::JunkRotation()) {
        if (item != RI_NONE) {
            junkPool.push_back(ItemName(item));
        }
    }

    const nlohmann::json dump = { { "checks", std::move(checks) },     { "pool", std::move(pool) },
                                  { "fixed", std::move(fixed) },       { "items", std::move(items) },
                                  { "prices", std::move(prices) },     { "locationHints", std::move(locationHints) },
                                  { "options", options },              { "junkPool", std::move(junkPool) } };
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
    for (const nlohmann::json& name : items) {
        const RandoItemId item =
            name.is_string() ? Rando::StaticData::GetItemIdFromDisplayName(name.get<std::string>()) : RI_UNKNOWN;
        if (item != RI_UNKNOWN && item != RI_TRAP) {
            Rando::GiveItem(Rando::ConvertItem(item));
        }
    }
}

// GlitchlessLogic's fixpoint without placing anything: regions and events unlock each other until both settle.
char* FleetRando_GetReachableChecks(void) {
    FleetCrossDungeonSearch crossRoutes;
    std::set<RandoRegionId> reachable;
    std::unordered_map<RandoRegionId, Rando::Logic::RegionTimeState> timeStates;
    crossRoutes.Roots(reachable, timeStates);
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
    crossRoutes.Remember(reachable, timeStates);
    return CopyString(names.dump());
}

bool FleetRando_PlaceItem(const char* check, const char* item) {
    if (check == nullptr || item == nullptr) {
        return false;
    }
    const RandoCheckId checkId = Rando::StaticData::GetCheckIdFromDisplayName(check);
    const RandoItemId itemId = Rando::StaticData::GetItemIdFromDisplayName(item);
    if (checkId == RC_UNKNOWN || itemId == RI_UNKNOWN) {
        return false;
    }
    RANDO_SAVE_CHECKS[checkId].randoItemId = itemId;
    RANDO_SAVE_CHECKS[checkId].shuffled = true;
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
    if (request == "SetComboOptions") {
        sComboOptions = { .hunt = input.value("hunt", false),
                          .required = static_cast<uint32_t>(std::clamp(input.value("required", 0), 0, 100)),
                          .pieces = std::clamp(input.value("pieces", -1), -1, 100) };
        return CopyString("{}");
    }
    // A saved seed's prices win over this preparation's rolls, so wallet logic judges what the player pays.
    if (request == "SetCheckPrices") {
        if (sSeedSave == nullptr) {
            return nullptr;
        }
        for (const auto& [name, price] : input.items()) {
            const RandoCheckId check = Rando::StaticData::GetCheckIdFromDisplayName(name);
            if (check != RC_UNKNOWN && price.is_number_integer()) {
                DungeonRegistry_SaveCheck(sSeedSave->save.shipSaveInfo.rando, check).price = price.get<uint16_t>();
            }
        }
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
    try {
        if (const std::optional<nlohmann::json> reply = HandleCrossDungeonRequest(request, input)) {
            return CopyString(reply->dump());
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Fleet] 2Ship rejected '{}': {}", request, e.what());
        return nullptr;
    }
    SPDLOG_WARN("[Fleet] 2Ship has no randomizer service '{}'", request);
    return nullptr;
}

void FleetRando_FreeString(char* text) {
    std::free(text);
}
