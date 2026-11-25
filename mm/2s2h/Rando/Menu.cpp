#include "Rando/Rando.h"
#include "Rando/Spoiler/Spoiler.h"
#include "2s2h/BenGui/UIWidgets.hpp"
#include <ship/window/gui/IconsFontAwesome4.h>
#include "Rando/CheckTracker/CheckTracker.h"
#include "Rando/MiscBehavior/ClockShuffle.h"
#include "build.h"
#include "2s2h/BenGui/BenMenu.h"
#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/Fleet/FleetRando.h"
#include "2s2h/Enhancements/Modes/EnemyRandomizer.h"

extern "C" {
#include "overlays/actors/ovl_En_Sth/z_en_sth.h"
}

#include <fast/Fast3dGui.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <map>

// TODO: This block should come from elsewhere, tied to data in Rando::StaticData::Options
std::unordered_map<int32_t, const char*> logicOptions = {
    { RO_LOGIC_GLITCHLESS, "Glitchless" },
    { RO_LOGIC_NO_LOGIC, "No Logic" },
    { RO_LOGIC_NEARLY_NO_LOGIC, "Nearly No Logic" },
    { RO_LOGIC_VANILLA, "Vanilla" },
};

std::unordered_map<int32_t, const char*> accessDungeonOptions = {
    { RO_ACCESS_DUNGEONS_FORM_AND_SONG, "Requires Transformation & Song" },
    { RO_ACCESS_DUNGEONS_FORM_OR_SONG, "Requires Transformation or Song" },
    { RO_ACCESS_DUNGEONS_FORM_ONLY, "Requires Only Transformation" },
    { RO_ACCESS_DUNGEONS_SONG_ONLY, "Requires Only Song" },
    { RO_ACCESS_DUNGEONS_OPEN, "Open" },
};

std::unordered_map<int32_t, const char*> accessTrialsOptions = {
    { RO_ACCESS_TRIALS_20_MASKS, "2-6-12-20 Masks" },
    { RO_ACCESS_TRIALS_REMAINS, "Requires Associated Remains" },
    { RO_ACCESS_TRIALS_FORMS, "Requires Associated Transformation" },
    { RO_ACCESS_TRIALS_OPEN, "Open" },
};

std::unordered_map<int32_t, const char*> junkItemsOptions = {
    { 0, "Default (Cycle)" },
    { 1, "Static" },
};

std::unordered_map<int32_t, const char*> trapItemsOptions = {
    { 0, "Default (Dynamic)" },
    { 1, "Static" },
};

std::unordered_map<int32_t, const char*> dungeonItemPlacementOptions = {
    { RO_DUNGEON_ITEM_ANYWHERE, "Anywhere" },       { RO_DUNGEON_ITEM_OWN_DUNGEON, "Own Dungeon" },
    { RO_DUNGEON_ITEM_START_WITH, "Start With" },   { RO_DUNGEON_ITEM_VANILLA, "Vanilla" },
    { RO_DUNGEON_ITEM_ANY_DUNGEON, "Any Dungeon" }, { RO_DUNGEON_ITEM_OVERWORLD, "Overworld" },
};

std::unordered_map<int32_t, const char*> majoraSoulOptions = {
    { RO_MAJORA_SOUL_START_WITH, "Start With" },   { RO_MAJORA_SOUL_ANY_DUNGEON, "Any Dungeon" },
    { RO_MAJORA_SOUL_OVERWORLD, "Overworld" },     { RO_MAJORA_SOUL_ANYWHERE, "Anywhere" },
    { RO_MAJORA_SOUL_STONES, "Trigger-Stones" },   { RO_MAJORA_SOUL_MEDALLIONS, "Trigger-Medallions" },
    { RO_MAJORA_SOUL_REWARDS, "Trigger-Rewards" }, { RO_MAJORA_SOUL_DUNGEONS, "Trigger-Dungeons" },
    { RO_MAJORA_SOUL_TOKENS, "Trigger-Tokens" },   { RO_MAJORA_SOUL_TRIFORCE_PIECES, "Trigger-Triforce Pieces" },
};

// clang-format off
std::vector<int32_t> incompatibleWithVanilla = {
    RO_SHUFFLE_BOSS_SOULS,
    RO_SHUFFLE_SWIM,
    RO_SHUFFLE_ENEMY_SOULS,
    RO_SHUFFLE_OCARINA_BUTTONS,
    RO_PLENTIFUL_ITEMS,
    RO_CLOCK_SHUFFLE,
    RO_SHUFFLE_TYCOON_WALLET,
};
// clang-format on

std::vector<RandoCheckId> checkExclusionList;

namespace BenGui {
extern std::shared_ptr<Rando::CheckTracker::CheckTrackerWindow> mRandoCheckTrackerWindow;
extern std::shared_ptr<Rando::CheckTracker::SettingsWindow> mRandoCheckTrackerSettingsWindow;
extern std::shared_ptr<BenMenu> mBenMenu;
} // namespace BenGui

using namespace BenGui;
using namespace UIWidgets;

extern "C" {
#include "archives/icon_item_24_static/icon_item_24_static_yar.h"
}

// Clock UI rendering constants
static const ImVec4 CLOCK_DAY_TINT = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
static const ImVec4 CLOCK_NIGHT_TINT = ImVec4(0.3f, 0.5f, 1.0f, 1.0f);
static const float DISABLED_ITEM_ALPHA = 0.3f;
static const char* CLOCK_PROGRESSIVE_TOOLTIP =
    "\n\nTime items are not compatible with Progressive Time modes.\nSwitch to Random mode to use starting time.";

static const ImVec4 DIM_TEXT_COLOR = ImVec4(1.0f, 1.0f, 1.0f, 0.5f);
static const ImVec4 COUNT_TEXT_COLOR = ImVec4(1.0f, 1.0f, 1.0f, 0.35f);

// Apply clock-specific rendering (tint colors and tooltips) based on progressive mode
static void ApplyClockItemRendering(RandoItemId item, ImVec4& tintColor, std::string& tooltipText,
                                    bool isProgressiveMode) {
    using namespace Rando::ClockItems;

    if (!IsClockItem(item)) {
        return; // Not a clock item, no special handling needed
    }

    // Apply day/night color tint
    if (IsDayClock(item)) {
        tintColor = CLOCK_DAY_TINT;
    } else {
        tintColor = CLOCK_NIGHT_TINT;
    }

    // Grey out and add tooltip if progressive mode is active
    if (item != RI_TIME_PROGRESSIVE && isProgressiveMode) {
        tintColor.w *= DISABLED_ITEM_ALPHA;
        tooltipText += CLOCK_PROGRESSIVE_TOOLTIP;
    }
}

void ClearIncompatibleSetting() {
    int32_t currentLogicSetting =
        CVarGetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, Rando::StaticData::Options[RO_LOGIC].defaultValue);
    switch (currentLogicSetting) {
        // Vanilla can't add items without corresponding checks
        case RO_LOGIC_VANILLA:
            CVarClear(Rando::StaticData::Options[RO_PLENTIFUL_ITEMS].cvar);
            CVarClear(Rando::StaticData::Options[RO_SHUFFLE_BOSS_SOULS].cvar);
            CVarClear(Rando::StaticData::Options[RO_SHUFFLE_SWIM].cvar);
            CVarClear(Rando::StaticData::Options[RO_CLOCK_SHUFFLE].cvar);
            CVarClear(Rando::StaticData::Options[RO_SHUFFLE_TYCOON_WALLET].cvar);
            break;
        default:
            break;
    }
}

bool IncompatibleWithLogicSetting(int32_t option) {
    int32_t currentLogicSetting =
        CVarGetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, Rando::StaticData::Options[RO_LOGIC].defaultValue);
    switch (currentLogicSetting) {
        case RO_LOGIC_VANILLA:
            if (std::find(incompatibleWithVanilla.begin(), incompatibleWithVanilla.end(), option) !=
                incompatibleWithVanilla.end()) {
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

void SortExcludedChecks() {
    std::sort(checkExclusionList.begin(), checkExclusionList.end());
}

void SaveExcludedChecks() {
    SortExcludedChecks();
    Rando::SetExcludedChecksInConfig(checkExclusionList);
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    ShipInit::Init("gRando.ExcludedChecks");
}

void LoadExcludedChecks() {
    checkExclusionList = Rando::GetExcludedChecksFromConfig();
}
static RegisterShipInitFunc loadExcludedChecksInit(LoadExcludedChecks, { "gRando.ExcludedChecks" });

struct CuratedCheckGroup {
    const char* label;
    const char* description;
    std::vector<std::pair<RandoCheckId, RandoCheckId>> ranges;
};

// clang-format off
std::vector<CuratedCheckGroup> curatedCheckGroups = {
    {
        "Cow Grotto Grass",
        "Every blade of grass in the Great Bay Coast and Termina Field cow grottos.",
        {
            { RC_GREAT_BAY_COAST_COW_GROTTO_GRASS_01, RC_GREAT_BAY_COAST_COW_GROTTO_GRASS_72 },
            { RC_TERMINA_FIELD_COW_GROTTO_GRASS_01, RC_TERMINA_FIELD_COW_GROTTO_GRASS_72 },
        },
    },
    {
        "Termina Field Grass",
        "Every blade of grass in Termina Field itself. The grottos within it are unaffected.",
        {
            { RC_TERMINA_FIELD_GRASS_01, RC_TERMINA_FIELD_GRASS_216 },
        },
    },
    {
        "Common Exclusions",
        "Minigames and side content that take a lot of time or a lot of luck to finish:",
        {
            { RC_BENEATH_THE_GRAVEYARD_DAMPE_CHEST, RC_BENEATH_THE_GRAVEYARD_DAMPE_CHEST },
            { RC_CLOCK_TOWN_EAST_HONEY_DARLING_ALL_DAYS, RC_CLOCK_TOWN_EAST_HONEY_DARLING_ALL_DAYS },
            { RC_CLOCK_TOWN_EAST_SHOOTING_GALLERY_PERFECT_SCORE, RC_CLOCK_TOWN_EAST_SHOOTING_GALLERY_PERFECT_SCORE },
            { RC_SWAMP_SHOOTING_GALLERY_PERFECT_SCORE, RC_SWAMP_SHOOTING_GALLERY_PERFECT_SCORE },
            { RC_DEKU_PLAYGROUND_ALL_DAYS, RC_DEKU_PLAYGROUND_ALL_DAYS },
            { RC_DEKU_SHRINE_MASK_OF_SCENTS, RC_DEKU_SHRINE_MASK_OF_SCENTS },
            { RC_GREAT_BAY_COAST_FISHERMAN_MINIGAME, RC_GREAT_BAY_COAST_FISHERMAN_MINIGAME },
            { RC_MOON_FIERCE_DEITY_MASK, RC_MOON_FIERCE_DEITY_MASK },
            { RC_MOUNTAIN_VILLAGE_FROG_CHOIR, RC_MOUNTAIN_VILLAGE_FROG_CHOIR },
            { RC_STOCK_POT_INN_COUPLES_MASK, RC_STOCK_POT_INN_COUPLES_MASK },
            { RC_WATERFALL_RAPIDS_BEAVER_RACE_02, RC_WATERFALL_RAPIDS_BEAVER_RACE_02 },
            { RC_PINNACLE_ROCK_REUNITE_SEAHORSE, RC_PINNACLE_ROCK_REUNITE_SEAHORSE },
        },
    },
};
// clang-format on

static std::map<RandoCheckType, const char*> checkTypeNames = {
    { RCTYPE_BARREL, "Barrels" },
    { RCTYPE_BEEHIVE, "Beehives" },
    { RCTYPE_BUTTERFLY, "Butterflies" },
    { RCTYPE_CHEST, "Chests" },
    { RCTYPE_COW, "Cows" },
    { RCTYPE_CRATE, "Crates" },
    { RCTYPE_ENEMY_DROP, "Enemy Drops" },
    { RCTYPE_FREESTANDING, "Freestanding Items" },
    { RCTYPE_FROG, "Frogs" },
    { RCTYPE_GRASS, "Grass" },
    { RCTYPE_HEART, "Heart Pieces & Containers" },
    { RCTYPE_MINIGAME, "Minigames" },
    { RCTYPE_NPC, "NPCs" },
    { RCTYPE_OWL, "Owl Statues" },
    { RCTYPE_POT, "Pots" },
    { RCTYPE_REMAINS, "Boss Remains" },
    { RCTYPE_SHOP, "Shop Items" },
    { RCTYPE_SKULL_TOKEN, "Skulltula Tokens" },
    { RCTYPE_SNOWBALL, "Snowballs" },
    { RCTYPE_SONG, "Songs" },
    { RCTYPE_STRAY_FAIRY, "Stray Fairies" },
    { RCTYPE_TINGLE_SHOP, "Tingle Maps" },
    { RCTYPE_TREE, "Trees" },
    { RCTYPE_WONDER_ITEM, "Wonder Items" },
};

struct CheckFilterGroup {
    std::string label;
    std::string tooltip;
    std::vector<RandoCheckId> checks;
};

static const std::vector<CheckFilterGroup>& GetCheckFilterGroups() {
    static std::vector<CheckFilterGroup> checkFilterGroups;
    if (!checkFilterGroups.empty()) {
        return checkFilterGroups;
    }

    std::map<RandoCheckType, std::vector<RandoCheckId>> checksByType;
    for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
        if (randoCheckId == RC_UNKNOWN) {
            continue;
        }
        checksByType[randoStaticCheck.randoCheckType].push_back(randoCheckId);
    }

    checkFilterGroups.push_back({ "All Checks", "", {} });

    for (auto& curatedCheckGroup : curatedCheckGroups) {
        std::vector<RandoCheckId> groupChecks;
        for (auto& [firstCheckId, lastCheckId] : curatedCheckGroup.ranges) {
            for (int32_t checkId = firstCheckId; checkId <= lastCheckId; checkId++) {
                if (Rando::StaticData::Checks.contains((RandoCheckId)checkId)) {
                    groupChecks.push_back((RandoCheckId)checkId);
                }
            }
        }
        std::sort(groupChecks.begin(), groupChecks.end());

        std::string tooltip = curatedCheckGroup.description;
        if (groupChecks.size() <= 12) {
            for (RandoCheckId randoCheckId : groupChecks) {
                tooltip += "\n- " + Rando::StaticData::CheckNames[randoCheckId];
            }
        }

        checkFilterGroups.push_back({ curatedCheckGroup.label, tooltip, groupChecks });
    }

    // The check types are listed after the curated groups, sorted by name instead of by enum order
    size_t firstCheckTypeGroup = checkFilterGroups.size();
    for (auto& [randoCheckType, checkTypeName] : checkTypeNames) {
        auto& typeChecks = checksByType[randoCheckType];
        if (typeChecks.empty()) {
            continue;
        }
        std::sort(typeChecks.begin(), typeChecks.end());
        checkFilterGroups.push_back({ checkTypeName, "", typeChecks });
    }
    std::sort(checkFilterGroups.begin() + firstCheckTypeGroup, checkFilterGroups.end(),
              [](const CheckFilterGroup& a, const CheckFilterGroup& b) { return a.label < b.label; });

    return checkFilterGroups;
}

static int32_t selectedCheckFilterGroup = 0;

static bool PassesCheckGroupFilter(RandoCheckId randoCheckId) {
    auto& groupChecks = GetCheckFilterGroups()[selectedCheckFilterGroup].checks;
    return groupChecks.empty() || std::binary_search(groupChecks.begin(), groupChecks.end(), randoCheckId);
}

static const float POOL_BALANCE_UNLIKELY_RATIO = 0.85f;
static const float POOL_BALANCE_LIMIT_RATIO = 0.9f;

static int checksInPool = 0;
static int itemsInPool = 0;
static int junkInPool = 0;
static int balanceStatus = 0; // 0 = Able to balance, 1 = Unlikely to balance, 2 = Unable to balance
static std::set<RandoItemId> setOfItemsInPool;
static std::set<RandoCheckId> setOfChecksInPool;
static uint32_t checkPoolGeneration = 0;
void RefreshMetrics() {
    setOfItemsInPool.clear();
    setOfChecksInPool.clear();
    RandoSaveInfo randoSaveInfo;
    std::vector<RandoCheckId> checkPool;
    std::vector<RandoItemId> itemPool;

    // Load options from CVars
    for (auto& [randoOptionId, randoStaticOption] : Rando::StaticData::Options) {
        randoSaveInfo.randoSaveOptions[randoOptionId] =
            (uint32_t)CVarGetInteger(randoStaticOption.cvar, randoStaticOption.defaultValue);
    }
    auto startingItems = Rando::GetStartingItemsFromConfig();
    Rando::SetStartingItemsInSave(randoSaveInfo, startingItems);

    Rando::Logic::GeneratePools(randoSaveInfo, checkPool, itemPool);

    checksInPool = checkPool.size();
    itemsInPool = itemPool.size();
    junkInPool = 0;
    for (auto& check : checkPool) {
        setOfChecksInPool.insert(check);
    }
    for (auto& item : itemPool) {
        setOfItemsInPool.insert(item);
        if (Rando::StaticData::Items[item].randoItemType == RITYPE_JUNK) {
            junkInPool++;
        }
    }
    for (auto& item : startingItems) {
        setOfItemsInPool.insert(item);
    }
    // Handle weird edge case with Random shuffle time option missing one time because it's computed given
    if (randoSaveInfo.randoSaveOptions[RO_CLOCK_SHUFFLE] &&
        randoSaveInfo.randoSaveOptions[RO_CLOCK_SHUFFLE_PROGRESSIVE] == RO_CLOCK_SHUFFLE_RANDOM) {
        setOfItemsInPool.insert(RI_TIME_DAY_1);
        setOfItemsInPool.insert(RI_TIME_NIGHT_1);
        setOfItemsInPool.insert(RI_TIME_DAY_2);
        setOfItemsInPool.insert(RI_TIME_NIGHT_2);
        setOfItemsInPool.insert(RI_TIME_DAY_3);
        setOfItemsInPool.insert(RI_TIME_NIGHT_3);
    }
    // If there are less checks than non-junk items, we can't balance
    if (checksInPool * POOL_BALANCE_LIMIT_RATIO < itemsInPool - junkInPool) {
        balanceStatus = 2;
        // If there are only slightly more checks than non-junk items, balancing is unlikely
    } else if (checksInPool * POOL_BALANCE_UNLIKELY_RATIO < itemsInPool - junkInPool) {
        balanceStatus = 1;
    } else {
        balanceStatus = 0;
    }
    checkPoolGeneration++;
}

static RegisterShipInitFunc refreshMetricsInit(RefreshMetrics, {
                                                                   // I Don't love this, but it works...
                                                                   "gRando.ExcludedChecks",
                                                                   "gRando.Options.RO_ACCESS_DUNGEONS",
                                                                   "gRando.Options.RO_ACCESS_MAJORA_MASKS_COUNT",
                                                                   "gRando.Options.RO_ACCESS_MAJORA_REMAINS_COUNT",
                                                                   "gRando.Options.RO_ACCESS_MOON_MASKS_COUNT",
                                                                   "gRando.Options.RO_ACCESS_MOON_REMAINS_COUNT",
                                                                   "gRando.Options.RO_ACCESS_TRIALS",
                                                                   "gRando.Options.RO_CLOCK_SHUFFLE_PROGRESSIVE",
                                                                   "gRando.Options.RO_CLOCK_SHUFFLE",
                                                                   "gRando.Options.RO_HINTS_BOSS_REMAINS",
                                                                   "gRando.Options.RO_HINTS_GOSSIP_STONE_STRENGTH",
                                                                   "gRando.Options.RO_HINTS_GOSSIP_STONES",
                                                                   "gRando.Options.RO_HINTS_HOOKSHOT",
                                                                   "gRando.Options.RO_HINTS_OATH_TO_ORDER",
                                                                   "gRando.Options.RO_HINTS_PURCHASEABLE",
                                                                   "gRando.Options.RO_HINTS_SPIDER_HOUSES",
                                                                   "gRando.Options.RO_LOGIC",
                                                                   "gRando.Options.RO_PLACEMENT_BOSS_KEYS",
                                                                   "gRando.Options.RO_PLACEMENT_SMALL_KEYS",
                                                                   "gRando.Options.RO_PLACEMENT_STRAY_FAIRIES",
                                                                   "gRando.Options.RO_PLENTIFUL_ITEMS",
                                                                   "gRando.Options.RO_SHUFFLE_BARREL_DROPS",
                                                                   "gRando.Options.RO_SHUFFLE_BOSS_REMAINS",
                                                                   "gRando.Options.RO_SHUFFLE_BOSS_SOULS",
                                                                   "gRando.Options.RO_SHUFFLE_BUTTERFLIES",
                                                                   "gRando.Options.RO_SHUFFLE_COWS",
                                                                   "gRando.Options.RO_SHUFFLE_CRATE_DROPS",
                                                                   "gRando.Options.RO_SHUFFLE_ENEMY_DROPS",
                                                                   "gRando.Options.RO_SHUFFLE_ENEMY_SOULS",
                                                                   "gRando.Options.RO_SHUFFLE_FREESTANDING_ITEMS",
                                                                   "gRando.Options.RO_SHUFFLE_FROGS",
                                                                   "gRando.Options.RO_SHUFFLE_GOLD_SKULLTULAS",
                                                                   "gRando.Options.RO_SHUFFLE_GRASS_DROPS",
                                                                   "gRando.Options.RO_SHUFFLE_HIVE_DROPS",
                                                                   "gRando.Options.RO_SHUFFLE_MAJORA_SOUL",
                                                                   "gRando.Options.RO_SHUFFLE_OCARINA_BUTTONS",
                                                                   "gRando.Options.RO_SHUFFLE_OWL_STATUES",
                                                                   "gRando.Options.RO_SHUFFLE_POT_DROPS",
                                                                   "gRando.Options.RO_SHUFFLE_SHOPS",
                                                                   "gRando.Options.RO_SHUFFLE_SKELETON_KEY",
                                                                   "gRando.Options.RO_SHUFFLE_SNOWBALL_DROPS",
                                                                   "gRando.Options.RO_SHUFFLE_SONG_DOUBLE_TIME",
                                                                   "gRando.Options.RO_SHUFFLE_SONG_INVERTED_TIME",
                                                                   "gRando.Options.RO_SHUFFLE_SONG_SARIA",
                                                                   "gRando.Options.RO_SHUFFLE_SONG_SUN",
                                                                   "gRando.Options.RO_SHUFFLE_SWIM",
                                                                   "gRando.Options.RO_SHUFFLE_TINGLE_SHOPS",
                                                                   "gRando.Options.RO_SHUFFLE_TRAPS",
                                                                   "gRando.Options.RO_SHUFFLE_TREE_DROPS",
                                                                   "gRando.Options.RO_SHUFFLE_TRIFORCE_PIECES",
                                                                   "gRando.Options.RO_SHUFFLE_TYCOON_WALLET",
                                                                   "gRando.Options.RO_SHUFFLE_WONDER_ITEMS",
                                                                   "gRando.Options.RO_SKULLTULA_SHUFFLED",
                                                                   "gRando.Options.RO_SKULLTULA_TOKENS_REQUIRED",
                                                                   "gRando.Options.RO_STARTING_CONSUMABLES",
                                                                   "gRando.Options.RO_STARTING_HEALTH",
                                                                   "gRando.Options.RO_STARTING_MAPS_AND_COMPASSES",
                                                                   "gRando.Options.RO_STARTING_RUPEES",
                                                                   "gRando.Options.RO_STRAY_FAIRIES_MAX",
                                                                   "gRando.Options.RO_STRAY_FAIRIES_REQUIRED",
                                                                   "gRando.Options.RO_TRAP_AMOUNT",
                                                                   "gRando.Options.RO_TRIFORCE_PIECES_MAX",
                                                                   "gRando.Options.RO_TRIFORCE_PIECES_REQUIRED",
                                                               });

// Shop checks that Rando::Logic::GeneratePools always shuffles even with Shops off
static constexpr RandoCheckId ALWAYS_SHUFFLED_SHOP_CHECKS[] = {
    RC_CURIOSITY_SHOP_SPECIAL_ITEM,
    RC_BOMB_SHOP_ITEM_03,
    RC_BOMB_SHOP_ITEM_04_OR_CURIOSITY_SHOP_ITEM,
};

// Must mirror the check-skipping rules in Rando::Logic::GeneratePools
static const std::map<RandoCheckType, int>& GetCheckCountsByType() {
    static const std::map<RandoCheckType, int> counts = [] {
        std::map<RandoCheckType, int> result;
        std::set<RandoCheckId> seen;
        for (auto& [randoRegionId, randoRegion] : Rando::Logic::Regions) {
            for (auto& [randoCheckId, _] : randoRegion.checks) {
                auto& randoStaticCheck = Rando::StaticData::Checks[randoCheckId];
                if (randoStaticCheck.randoCheckId == RC_UNKNOWN || randoStaticCheck.sceneId == SCENE_LAST_BS) {
                    continue;
                }
                if (!seen.insert(randoCheckId).second) {
                    continue;
                }
                result[randoStaticCheck.randoCheckType]++;
            }
        }
        result[RCTYPE_SHOP] = std::max(0, result[RCTYPE_SHOP] - (int)std::size(ALWAYS_SHUFFLED_SHOP_CHECKS));
        return result;
    }();
    return counts;
}

static void PoolCountSuffix(int count) {
    if (count <= 0) {
        return;
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(COUNT_TEXT_COLOR, "+%d", count);
}

static bool IsDrawnByFleet(RandoOptionId optionId) {
    return FleetRando_IsOptionDrawnByFleet(Rando::StaticData::Options[optionId].cvar);
}

static bool CheckPoolCheckbox(const char* label, RandoOptionId optionId, RandoCheckType checkType, const char* tooltip,
                              int countOverride = -1) {
    if (IsDrawnByFleet(optionId)) {
        return false;
    }
    bool changed =
        CVarCheckbox(label, Rando::StaticData::Options[optionId].cvar, CheckboxOptions({ { .tooltip = tooltip } }));
    if (countOverride >= 0) {
        PoolCountSuffix(countOverride);
        return changed;
    }
    auto& counts = GetCheckCountsByType();
    auto it = counts.find(checkType);
    PoolCountSuffix(it != counts.end() ? it->second : 0);
    return changed;
}

static void ClampRequiredToMax(RandoOptionId requiredOptionId, RandoOptionId maxOptionId, int32_t fallback) {
    const char* requiredCvar = Rando::StaticData::Options[requiredOptionId].cvar;
    int32_t maxValue = CVarGetInteger(Rando::StaticData::Options[maxOptionId].cvar, fallback);
    if (CVarGetInteger(requiredCvar, fallback) > maxValue) {
        CVarSetInteger(requiredCvar, maxValue);
    }
}

static void DrawSearchFilter(ImGuiTextFilter& filter, const char* id, const char* hint, UIWidgets::Colors color) {
    f32 startX = ImGui::GetCursorPosX();
    UIWidgets::PushStyleCombobox(color);
    filter.Draw(id, ImGui::GetContentRegionAvail().x);
    UIWidgets::PopStyleCombobox();
    if (!filter.IsActive()) {
        ImGui::SameLine(startX + 18.0f);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "%s", hint);
    }
}

static const char* SEED_HEALTH_TOOLTIP =
    "Junk (rupees, arrows, single bombs, etc.) automatically fills every check that important items don't use.\n\n"
    "If important items need more than ~90% of the checks (the white marker), seed generation can fail. To make "
    "room:\n"
    "- Enable more locations on the Check Pool page\n"
    "- Remove extra items on the Item Pool page\n"
    "- Restore checks on the Check Exclusions page";

static void DrawSeedHealthStrip() {
    if (FleetRando_IsOptionDrawnByFleet(FLEET_HIDDEN_SEED_HEALTH)) {
        return;
    }
    int importantItems = itemsInPool - junkInPool;
    float usage = 0.0f;
    if (checksInPool > 0) {
        usage = (float)importantItems / (float)checksInPool;
    } else if (importantItems > 0) {
        usage = 1.0f;
    }

    UIWidgets::Colors statusColor;
    const char* statusText;
    if (balanceStatus == 0) {
        statusColor = Colors::Green;
        statusText = "Balanced";
    } else if (balanceStatus == 1) {
        statusColor = Colors::Orange;
        statusText = "Tight Fit";
    } else {
        statusColor = Colors::Red;
        statusText = "Overfilled";
    }
    const ImVec4& statusBarColor = ColorValues.at(statusColor);
    // TODO: light text variants of the palette's dark green/red belong in UIWidgets::ColorValues
    auto brighten = [](float channel) { return std::min(1.0f, channel + 0.25f); };
    ImVec4 statusTextColor =
        ImVec4(brighten(statusBarColor.x), brighten(statusBarColor.y), brighten(statusBarColor.z), statusBarColor.w);

    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(DIM_TEXT_COLOR, "Seed Health:");
    ImGui::SameLine();
    ImGui::TextColored(statusTextColor, ICON_FA_CIRCLE " %s", statusText);
    ImGui::SameLine(0.0f, 30.0f);
    ImGui::TextColored(DIM_TEXT_COLOR, "%d checks  |  %d important items  |  %d junk fill  |  %d excluded",
                       checksInPool, importantItems, junkInPool, (int)checkExclusionList.size());

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, statusBarColor);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ColorValues.at(Colors::DarkGray));
    ImGui::ProgressBar(std::min(usage, 1.0f), ImVec2(-FLT_MIN, 8.0f), "");
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    ImVec2 barMin = ImGui::GetItemRectMin();
    ImVec2 barMax = ImGui::GetItemRectMax();
    float thresholdX = barMin.x + (barMax.x - barMin.x) * POOL_BALANCE_LIMIT_RATIO;
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(thresholdX - 1.0f, barMin.y), ImVec2(thresholdX + 1.0f, barMax.y),
                                              IM_COL32(255, 255, 255, 160));
    ImGui::EndGroup();
    Tooltip(SEED_HEALTH_TOOLTIP);
}

static void DrawSeedGeneration() {
    ImGui::SeparatorText("Seed Generation");
    UIWidgets::CVarCheckbox("Enable Rando (Randomizes new files upon creation)", "gRando.Enabled");

    if (UIWidgets::CVarCombobox("Seed", "gRando.SpoilerFileIndex", Rando::Spoiler::spoilerOptions)) {
        Rando::Spoiler::SelectSpoiler(CVarGetInteger("gRando.SpoilerFileIndex", 0));
    }

    if (CVarGetInteger("gRando.SpoilerFileIndex", 0) == 0) {
        UIWidgets::PushStyleSlider();
        static char seed[256];
        std::string stringSeed = CVarGetString("gRando.InputSeed", "");
        strcpy(seed, stringSeed.c_str());
        ImGui::InputText("##Seed", seed, sizeof(seed), ImGuiInputTextFlags_CallbackAlways,
                         [](ImGuiInputTextCallbackData* data) {
                             CVarSetString("gRando.InputSeed", data->Buf);
                             return 0;
                         });
        if (stringSeed.length() < 1) {
            ImGui::SameLine(17.0f);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "Leave blank for random seed");
        }
        UIWidgets::PopStyleSlider();

        UIWidgets::CVarCheckbox("Generate Spoiler File", "gRando.GenerateSpoiler",
                                CheckboxOptions().DefaultValue(true));
    }

    ImGui::SeparatorText("Seed Health");
    DrawSeedHealthStrip();
}

static void DrawGeneralIntro() {
    ImGui::PushStyleColor(ImGuiCol_Text, DIM_TEXT_COLOR);
    ImGui::TextWrapped(
        "Explore the menus for various enhancements and time savers; most are not enabled by default in Rando.");
    ImGui::PopStyleColor();
}

static void DrawLiveOptionsHeader() {
    ImGui::SeparatorText("Live Options");
    ImGui::TextWrapped("These options can be changed on the fly, and are not tied to the seed generation.");
}

static void DrawContainerStyleRow() {
    if (FleetRando_IsOptionDrawnByFleet("gRando.CSMC")) {
        return;
    }
    UIWidgets::CVarCheckbox(
        "Container Style Matches Contents", "gRando.CSMC",
        UIWidgets::CheckboxOptions().Tooltip("This will make the contents of a container match the container itself. "
                                             "Eg chests, pots, crates, grass, etc."));
}

static void DrawJunkItemsRow() {
    UIWidgets::CVarCombobox(
        "Junk Items", "gRando.JunkItems", &junkItemsOptions,
        UIWidgets::ComboboxOptions()
            .ComponentAlignment(UIWidgets::ComponentAlignment::Right)
            .LabelPosition(UIWidgets::LabelPosition::Near)
            .Tooltip(
                "Note: For both Options, junk items will be randomly rolled from a pool of obtainable "
                "items.\n\nDefault (Cycle): Junk items will cycle every few seconds, allowing you to choose which item "
                "to pick up\n\nStatic: Junk items will be static, only changing when obtainability status changes."));
}

static void DrawLogicCombobox() {
    if (UIWidgets::CVarCombobox("Logic", Rando::StaticData::Options[RO_LOGIC].cvar, &logicOptions)) {
        ClearIncompatibleSetting();
    }
    UIWidgets::Tooltip(
        "Glitchless - The items are shuffled in a way that guarantees the seed is beatable without "
        "glitches. With this setting, \"Save Game on Moon Crash\" is automatically enabled.\n\n"
        "No Logic - The items are shuffled completely randomly, this can result in unbeatable seeds, and "
        "will require heavy use of glitches.\n\n"
        "Nearly No Logic - The items are shuffled completely randomly, with the following exceptions:\n"
        "- Oath to Order and Remains cannot be placed on the Moon.\n"
        "- Deku Mask, Zora Mask, Sonata, and Bossa Nova cannot be placed in their respective Temples or on "
        "the Moon.\n\n"
        "Vanilla - The items are not shuffled.\n"
        "Not compatible with settings that add items to the pool, like Boss Souls or Plentiful Items.");
}

static void DrawDungeonAccessRow() {
    UIWidgets::CVarCombobox("Dungeon Access", Rando::StaticData::Options[RO_ACCESS_DUNGEONS].cvar,
                            &accessDungeonOptions);
    UIWidgets::Tooltip("Dungeon access requirements:\n\n"
                       "Requires Transformation & Song - Requires both the correct form and the song (Vanilla).\n\n"
                       "Requires Transformation or Song - Requires either the correct form or the song.\n\n"
                       "Requires Only Transformation - Requires only the correct form.\n\n"
                       "Requires Only Song - Requires only the correct song.\n\n"
                       "Open - Dungeons will be open with no requirements.");
}

static void DrawTrialsAccessRow() {
    UIWidgets::CVarCombobox("Trials Access", Rando::StaticData::Options[RO_ACCESS_TRIALS].cvar, &accessTrialsOptions);
}

static void DrawAccessCountRow(const char* label, RandoOptionId optionId, int32_t max, int32_t defaultValue) {
    UIWidgets::CVarSliderInt(label, Rando::StaticData::Options[optionId].cvar,
                             IntSliderOptions().Min(0).Max(max).DefaultValue(defaultValue));
}

static void DrawMoonTriforceRow() {
    UIWidgets::CVarSliderInt(
        "Moon Access Triforce Pieces Required", Rando::StaticData::Options[RO_ACCESS_MOON_TRIFORCE_COUNT].cvar,
        IntSliderOptions().Min(0).Max(100).DefaultValue(0).Tooltip(
            "Triforce Pieces needed, on top of the other requirements, to reach the Moon. 0 turns it off.\n"
            "Only applies with Triforce Hunt; inside Fleet both games' pieces count."));
}

static f32 sTwoColumnWidth = 0.0f;

static void BeginFirstColumn(const char* id) {
    sTwoColumnWidth = ImGui::GetContentRegionAvail().x / 2 - (ImGui::GetStyle().ItemSpacing.x * 2);
    ImGui::BeginChild(id, ImVec2(sTwoColumnWidth, 0));
}

static void BeginSecondColumn(const char* id) {
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild(id, ImVec2(sTwoColumnWidth, 0));
}

struct CheckPoolRow {
    const char* label;
    RandoOptionId optionId;
    RandoCheckType checkType;
    const char* tooltip;
};

static const CheckPoolRow CHECK_POOL_ROWS[] = {
    { "Owl Statues", RO_SHUFFLE_OWL_STATUES, RCTYPE_OWL,
      "Activating an owl statue is a check. Song of Soaring destinations are unaffected." },
    { "Shops", RO_SHUFFLE_SHOPS, RCTYPE_SHOP,
      "Items sold in shops are checks, with randomized prices.\n\nThe Curiosity Shop special item "
      "and the Bomb Shop's bomb bags are always shuffled, even with this off." },
    { "Tingle Maps", RO_SHUFFLE_TINGLE_SHOPS, RCTYPE_TINGLE_SHOP,
      "Maps sold by Tingle are checks, with randomized prices." },
    { "Boss Remains", RO_SHUFFLE_BOSS_REMAINS, RCTYPE_REMAINS,
      "Defeating each temple boss rewards a shuffled item instead of that boss's remains." },
    { "Cows", RO_SHUFFLE_COWS, RCTYPE_COW, "Playing Epona's Song to a cow is a check." },
    { "Enemy Drops", RO_SHUFFLE_ENEMY_DROPS, RCTYPE_ENEMY_DROP, "The first drop from each non-boss enemy is a check." },
    { "Frogs", RO_SHUFFLE_FROGS, RCTYPE_FROG,
      "Talking to each of the four scattered choir frogs while wearing Don Gero's Mask is a check." },
    { "Butterflies", RO_SHUFFLE_BUTTERFLIES, RCTYPE_BUTTERFLY, "Butterfly swarms in the overworld are checks." },
    { "Freestanding Items", RO_SHUFFLE_FREESTANDING_ITEMS, RCTYPE_FREESTANDING,
      "Items sitting out in the world (hearts, rupees, arrows, etc.) are checks." },
    { "Wonder Items", RO_SHUFFLE_WONDER_ITEMS, RCTYPE_WONDER_ITEM,
      "Invisible touch/shoot triggers scattered around the world are checks." },
};

static constexpr size_t CHECK_POOL_WORLD_ROW_COUNT = 5;

static const CheckPoolRow SCENERY_DROP_GROUPS[] = {
    { "Pot Drops", RO_SHUFFLE_POT_DROPS, RCTYPE_POT, "Breaking each pot is a check." },
    { "Crate Drops", RO_SHUFFLE_CRATE_DROPS, RCTYPE_CRATE, "Breaking each crate is a check." },
    { "Barrel Drops", RO_SHUFFLE_BARREL_DROPS, RCTYPE_BARREL, "Breaking each barrel is a check." },
    { "Snowball Drops", RO_SHUFFLE_SNOWBALL_DROPS, RCTYPE_SNOWBALL, "Breaking each large snowball is a check." },
    { "Grass Drops", RO_SHUFFLE_GRASS_DROPS, RCTYPE_GRASS, "Cutting each clump of grass is a check." },
    { "Tree Drops", RO_SHUFFLE_TREE_DROPS, RCTYPE_TREE,
      "Bonking into each tree that shakes is a check. Trees that don't shake have no drops." },
    { "Beehive Drops", RO_SHUFFLE_HIVE_DROPS, RCTYPE_BEEHIVE, "Knocking down each beehive is a check." },
};

static void SetSceneryDropOptions(int32_t value) {
    for (const CheckPoolRow& sceneryDropGroup : SCENERY_DROP_GROUPS) {
        const char* cvar = Rando::StaticData::Options[sceneryDropGroup.optionId].cvar;
        CVarSetInteger(cvar, value);
        ShipInit::Init(cvar);
    }
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

static const UIWidgets::CardLayoutOptions POOL_CARD_LAYOUT = { .columnsPerRow = 3,
                                                               .minColumnWidth = 400.0f,
                                                               .childFlags = ImGuiChildFlags_AutoResizeY };

static bool IsGoldSkulltulaShuffleOn() {
    return CVarGetInteger(Rando::StaticData::Options[RO_SHUFFLE_GOLD_SKULLTULAS].cvar, RO_GENERIC_OFF) != 0;
}

static void DrawSongsPlaceholderRow() {
    CVarCheckbox("Songs", "gPlaceholderBool",
                 CheckboxOptions({ { .disabled = true,
                                     .disabledTooltip = "Songs are currently always shuffled. The option to "
                                                        "disable this is coming soon." } })
                     .DefaultValue(true));
}

static void DrawGoldSkulltulasRow() {
    int32_t skulltulasShuffled =
        CVarGetInteger(Rando::StaticData::Options[RO_SKULLTULA_SHUFFLED].cvar, SPIDER_HOUSE_TOKENS_REQUIRED);
    CheckPoolCheckbox("Gold Skulltulas", RO_SHUFFLE_GOLD_SKULLTULAS, RCTYPE_SKULL_TOKEN,
                      "Gold Skulltulas in the two Spider Houses are checks.", skulltulasShuffled * 2);
}

static void DrawRequiredSkulltulaTokensRow() {
    if (!IsGoldSkulltulaShuffleOn()) {
        return;
    }
    CVarSliderInt("Required Gold Skulltula Tokens", Rando::StaticData::Options[RO_SKULLTULA_TOKENS_REQUIRED].cvar,
                  IntSliderOptions()
                      .Tooltip("Minimum Gold Skulltula tokens needed to obtain the Spider House checks.")
                      .LabelPosition(UIWidgets::LabelPosition::None)
                      .Min(1)
                      .Format("%d Tokens Required")
                      .Max(SPIDER_HOUSE_TOKENS_REQUIRED)
                      .DefaultValue(SPIDER_HOUSE_TOKENS_REQUIRED));
}

static void DrawShuffledSkulltulasRow() {
    if (!IsGoldSkulltulaShuffleOn()) {
        return;
    }
    CVarSliderInt("Shuffled Gold Skulltulas", Rando::StaticData::Options[RO_SKULLTULA_SHUFFLED].cvar,
                  IntSliderOptions()
                      .Tooltip("How many Gold Skulltulas in each Spider House are randomized checks. The rest "
                               "contain their associated token. The spiders are chosen at random each seed.")
                      .LabelPosition(UIWidgets::LabelPosition::None)
                      .Min(1)
                      .Format("%d of 30 Shuffled Per House")
                      .Max(SPIDER_HOUSE_TOKENS_REQUIRED)
                      .DefaultValue(SPIDER_HOUSE_TOKENS_REQUIRED));
}

static void DrawSceneryDropsToggles() {
    ImGui::PushStyleColor(ImGuiCol_Text, DIM_TEXT_COLOR);
    ImGui::TextWrapped("Hundreds of junk-heavy checks per group. Enable these to make room for a bigger item pool, "
                       "or for full \"allsanity\" seeds.");
    ImGui::PopStyleColor();
    if (Button("All On", ButtonOptions({ { .tooltip = "Enable every scenery drop group" } })
                             .Size(UIWidgets::Sizes::Inline)
                             .Color(Colors::Green))) {
        SetSceneryDropOptions(RO_GENERIC_ON);
    }
    ImGui::SameLine();
    if (Button("All Off", ButtonOptions({ { .tooltip = "Disable every scenery drop group" } })
                              .Size(UIWidgets::Sizes::Inline)
                              .Color(Colors::Red))) {
        SetSceneryDropOptions(RO_GENERIC_OFF);
    }
}

static constexpr int SARIA_MAX_PRIORITY_ITEMS = 16;
static constexpr float PRIORITY_BUTTON_SIZE = 24.0f;

static void PushPriorityListChildStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
}

static ButtonOptions PriorityRowButtonOptions(bool disabled = false) {
    return ButtonOptions({ { .disabled = disabled } })
        .Size(ImVec2(PRIORITY_BUTTON_SIZE, PRIORITY_BUTTON_SIZE))
        .Padding(ImVec2(4.0f, 4.0f));
}

static void DrawPrioritySwapButton(const char* strId, const char* icon, bool disabled,
                                   std::vector<RandoItemId>& priorityItemsList, size_t a, size_t b) {
    if (IconButton(strId, icon, PriorityRowButtonOptions(disabled))) {
        std::swap(priorityItemsList[a], priorityItemsList[b]);
        Rando::SetSariaPriorityItemsInConfig(priorityItemsList);
    }
}

static void DrawPriorityItemsPopup() {
    static std::vector<RandoItemId> priorityItemsList;
    if (ImGui::IsWindowAppearing()) {
        priorityItemsList = Rando::GetSariaPriorityItemsFromConfig();
    }

    std::string headerLabel = "Priority Items (" + std::to_string(priorityItemsList.size()) + "/" +
                              std::to_string(SARIA_MAX_PRIORITY_ITEMS) + ")";
    ImGui::SeparatorText(headerLabel.c_str());
    ImGui::TextWrapped("Saria's Song hints whichever of these is reachable and not yet found, checked in the "
                       "order listed below.");
    if (Button(
            ICON_FA_UNDO " Reset to Default",
            ButtonOptions({ { .tooltip = "Replace this list with the default priority items" } }).Size(ImVec2(0, 0)))) {
        priorityItemsList = Rando::GetDefaultSariaPriorityItems();
        Rando::SetSariaPriorityItemsInConfig(priorityItemsList);
    }

    PushPriorityListChildStyle();
    if (ImGui::BeginChild("priorityItemsCurrentList", ImVec2(0, 180.0f))) {
        if (priorityItemsList.empty()) {
            ImGui::TextColored(ColorValues.at(Colors::Gray), "No priority items configured.");
        } else if (ImGui::BeginTable("priorityItemsTable", 5, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("icon", ImGuiTableColumnFlags_WidthFixed, PRIORITY_BUTTON_SIZE);
            ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("up", ImGuiTableColumnFlags_WidthFixed, 32.0f);
            ImGui::TableSetupColumn("down", ImGuiTableColumnFlags_WidthFixed, 32.0f);
            ImGui::TableSetupColumn("remove", ImGuiTableColumnFlags_WidthFixed, 32.0f);

            for (size_t index = 0; index < priorityItemsList.size(); index++) {
                RandoItemId itemId = priorityItemsList[index];
                Rando::StaticData::RandoStaticItem randoStaticItem = Rando::StaticData::Items[itemId];
                ImGui::PushID((int)index);
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                const char* texturePath = Rando::StaticData::GetIconTexturePath(itemId);
                ImTextureID textureId =
                    std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui())
                        ->GetTextureByName(texturePath);
                float iconOffsetY = (ImGui::GetFrameHeight() - PRIORITY_BUTTON_SIZE) * 0.5f;
                if (iconOffsetY > 0.0f) {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + iconOffsetY);
                }
                ImGui::Image(textureId, ImVec2(PRIORITY_BUTTON_SIZE, PRIORITY_BUTTON_SIZE), ImVec2(0, 0), ImVec2(1, 1),
                             Ship_GetItemColorTint(randoStaticItem.itemId), ImVec4(0, 0, 0, 0));

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(randoStaticItem.name);

                ImGui::TableNextColumn();
                DrawPrioritySwapButton("##up", ICON_FA_CHEVRON_UP, index == 0, priorityItemsList, index, index - 1);

                ImGui::TableNextColumn();
                DrawPrioritySwapButton("##down", ICON_FA_CHEVRON_DOWN, index + 1 == priorityItemsList.size(),
                                       priorityItemsList, index, index + 1);

                ImGui::TableNextColumn();
                if (IconButton("##remove", ICON_FA_TIMES, PriorityRowButtonOptions().Color(Colors::Red))) {
                    priorityItemsList.erase(priorityItemsList.begin() + index);
                    Rando::SetSariaPriorityItemsInConfig(priorityItemsList);
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::Spacing();
    ImGui::SeparatorText("Add Item");

    static ImGuiTextFilter addItemFilter;
    DrawSearchFilter(addItemFilter, "##priorityItemFilter", "Search", UIWidgets::Colors::LightBlue);

    bool atCap = priorityItemsList.size() >= SARIA_MAX_PRIORITY_ITEMS;
    PushPriorityListChildStyle();
    if (ImGui::BeginChild("priorityItemsAddList", ImVec2(0, 0))) {
        if (atCap) {
            ImGui::PushStyleColor(ImGuiCol_Text, ColorValues.at(Colors::Orange));
            ImGui::TextWrapped("Priority list is full (%d/%d). Remove an item above to add a different one.",
                               SARIA_MAX_PRIORITY_ITEMS, SARIA_MAX_PRIORITY_ITEMS);
            ImGui::PopStyleColor();
        } else {
            for (RandoItemId candidateId : Rando::GetSariaPriorityItemCandidates()) {
                if (setOfItemsInPool.count(candidateId) == 0) {
                    continue;
                }
                if (std::find(priorityItemsList.begin(), priorityItemsList.end(), candidateId) !=
                    priorityItemsList.end()) {
                    continue;
                }

                const char* name = Rando::StaticData::Items[candidateId].name;
                if (!addItemFilter.PassFilter(name)) {
                    continue;
                }

                if (ImGui::Selectable(name)) {
                    priorityItemsList.push_back(candidateId);
                    Rando::SetSariaPriorityItemsInConfig(priorityItemsList);
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

static bool ItemPoolCheckbox(const char* label, RandoOptionId optionId, const char* tooltip, int itemCount = 0,
                             const char* disabledReason = nullptr) {
    if (IsDrawnByFleet(optionId)) {
        return false;
    }
    if (disabledReason == nullptr && IncompatibleWithLogicSetting(optionId)) {
        disabledReason = "Incompatible with current Logic Setting";
    }
    bool changed = CVarCheckbox(
        label, Rando::StaticData::Options[optionId].cvar,
        CheckboxOptions(
            { { .tooltip = tooltip, .disabled = disabledReason != nullptr, .disabledTooltip = disabledReason } }));
    PoolCountSuffix(itemCount);
    return changed;
}

static void DrawComingSoonItems() {
    ImGui::PushStyleColor(ImGuiCol_Text, DIM_TEXT_COLOR);
    ImGui::SeparatorText("Coming Soon");
    ImGui::PopStyleColor();
    for (const char* label : { "Deku Stick Bag", "Deku Nut Bag", "Child Wallet", "Infinite Upgrades" }) {
        CVarCheckbox(label, "gPlaceholderBool",
                     CheckboxOptions({ { .disabled = true, .disabledTooltip = "Coming Soon" } }));
    }
}

struct ItemPoolRow {
    const char* label;
    RandoOptionId optionId;
    const char* tooltip;
    int itemCount;
};

static const ItemPoolRow ITEM_POOL_ROWS[] = {
    { "Swim", RO_SHUFFLE_SWIM,
      "Shuffles the ability to Swim, entering the Swim state or submerging\n"
      "into deep water will respawn Link.",
      1 },
    { "Ocarina Buttons", RO_SHUFFLE_OCARINA_BUTTONS,
      "Shuffles the Buttons used to play Ocarina Notes.\n"
      "You will be unable to play a song until you find all\n"
      "notes for the given melody.",
      RI_OCARINA_BUTTON_C_UP - RI_OCARINA_BUTTON_A + 1 },
    { "Tycoon's Wallet", RO_SHUFFLE_TYCOON_WALLET,
      "Adds the Tycoon's Wallet (5,000 rupees) to the item pool\n"
      "as a third progressive wallet upgrade.",
      1 },
    { "Sun's Song", RO_SHUFFLE_SONG_SUN,
      "Adds the Sun's Song to the item pool. Playing it sets the time of day to 06:00 or 18:00, "
      "whichever comes first.",
      1 },
    { "Song of Double Time", RO_SHUFFLE_SONG_DOUBLE_TIME, "Adds the Song of Double Time to the item pool.", 1 },
    { "Inverted Song of Time", RO_SHUFFLE_SONG_INVERTED_TIME, "Adds the Inverted Song of Time to the item pool.", 1 },
    { "Boss Souls", RO_SHUFFLE_BOSS_SOULS,
      "Adds the \"souls\" of the five bosses to the item pool. Boss Souls are items "
      "that must be found in order for their corresponding boss to spawn.\n\n"
      "Majora's soul only joins them while Majora's Soul is set to Start With.",
      RI_SOUL_BOSS_TWINMOLD - RI_SOUL_BOSS_GOHT + 1 },
    { "Enemy Souls", RO_SHUFFLE_ENEMY_SOULS,
      "Adds the \"souls\" of regular enemies to the item pool. An enemy will be "
      "immune to damage until its corresponding soul has been obtained.",
      RI_SOUL_ENEMY_WOLFOS - RI_SOUL_ENEMY_ALIEN + 1 },
    { "Shuffle Time", RO_CLOCK_SHUFFLE,
      "Breaks the 3-day cycle into 6 separate half-days (Day 1 Day/Night, "
      "Day 2 Day/Night, Day 3 Day/Night) that must be unlocked as items. "
      "Players can only access time periods they've obtained. Attempting to "
      "access unowned time redirects to the next owned half-day.",
      Rando::ClockItems::HALF_COUNT },
    { "Plentiful Items", RO_PLENTIFUL_ITEMS,
      "Major items, masks, and keys will have an extra copy added to the item pool. \n"
      "Lesser items, stray fairies, and skulltula tokens will have a chance for an "
      "extra copy to be added to the item pool.",
      0 },
};

static void DrawItemPoolRow(WidgetInfo& info) {
    for (const ItemPoolRow& row : ITEM_POOL_ROWS) {
        if (info.name == row.label) {
            ItemPoolCheckbox(row.label, row.optionId, row.tooltip, row.itemCount);
            return;
        }
    }
    SPDLOG_ERROR("Item Pool row '{}' has no definition", info.name);
}

static bool IsOptionOn(RandoOptionId optionId) {
    return CVarGetInteger(Rando::StaticData::Options[optionId].cvar, RO_GENERIC_OFF) != 0;
}

static const char* MAJORA_SOUL_HELP =
    "Majora cannot be fought until its soul is obtained.\n\n"
    "Start With - You begin with Majora's soul. With Boss Souls on, it is shuffled with the other souls "
    "instead.\n\n"
    "Any Dungeon - Majora's soul can only appear inside of the four temples.\n\n"
    "Overworld - Majora's soul can only appear outside of the four temples.\n\n"
    "Anywhere - Majora's soul can appear anywhere in the world.\n\n"
    "Trigger - These settings keep Majora's soul out of the item pool, granting it once the requirements are "
    "met:\n"
    "- Stones: Obtain the specified amount of Spiritual Stones.\n"
    "- Medallions: Obtain the specified amount of Medallions.\n"
    "- Dungeon rewards: Obtain the specified total sum of Spiritual Stones, Medallions or boss remains.\n"
    "- Dungeons: Complete the specified amount of temples. Temples are considered complete after stepping into "
    "the blue warp after the boss.\n"
    "- Tokens: Obtain the specified amount of Gold Skulltula tokens.\n"
    "- Triforce Pieces: Obtain the specified amount of Triforce Pieces (requires Triforce Hunt).\n\n"
    "Spiritual Stones and Medallions only exist with their mods installed.";

static void DrawMajoraSoulRow() {
    if (IsDrawnByFleet(RO_SHUFFLE_MAJORA_SOUL)) {
        return;
    }
    UIWidgets::CVarCombobox("Majora's Soul", Rando::StaticData::Options[RO_SHUFFLE_MAJORA_SOUL].cvar,
                            &majoraSoulOptions, UIWidgets::ComboboxOptions().Tooltip(MAJORA_SOUL_HELP));
}

struct MajoraSoulCountRow {
    const char* label;
    RandoOptionMajoraSoul trigger;
    RandoOptionId optionId;
    int32_t max;
    const char* tooltip;
};

static const MajoraSoulCountRow MAJORA_SOUL_COUNT_ROWS[] = {
    { "Majora's Soul Stone Count", RO_MAJORA_SOUL_STONES, RO_MAJORA_SOUL_STONE_COUNT,
      (int32_t)std::size(Rando::Logic::SPIRITUAL_STONE_KEYS), "Spiritual Stones needed to receive Majora's soul." },
    { "Majora's Soul Medallion Count", RO_MAJORA_SOUL_MEDALLIONS, RO_MAJORA_SOUL_MEDALLION_COUNT,
      (int32_t)std::size(Rando::Logic::MEDALLION_KEYS), "Medallions needed to receive Majora's soul." },
    { "Majora's Soul Reward Count", RO_MAJORA_SOUL_REWARDS, RO_MAJORA_SOUL_REWARD_COUNT,
      (int32_t)(std::size(Rando::Logic::SPIRITUAL_STONE_KEYS) + std::size(Rando::Logic::MEDALLION_KEYS)) +
          (QUEST_REMAINS_TWINMOLD - QUEST_REMAINS_ODOLWA + 1),
      "Spiritual Stones, Medallions and boss remains, in any mix, needed to receive Majora's soul." },
    { "Majora's Soul Dungeon Count", RO_MAJORA_SOUL_DUNGEONS, RO_MAJORA_SOUL_DUNGEON_COUNT,
      (int32_t)std::size(Rando::Logic::TEMPLE_CLEARS), "Temples to complete before receiving Majora's soul." },
    { "Majora's Soul Token Count", RO_MAJORA_SOUL_TOKENS, RO_MAJORA_SOUL_TOKEN_COUNT, SPIDER_HOUSE_TOKENS_REQUIRED * 2,
      "Gold Skulltula tokens, from both Spider Houses, needed to receive "
      "Majora's soul." },
    { "Majora's Soul Triforce Piece Count", RO_MAJORA_SOUL_TRIFORCE_PIECES, RO_MAJORA_SOUL_TRIFORCE_COUNT, 100,
      "Triforce Pieces needed to receive Majora's soul." },
};

static void DrawMajoraSoulCountRow(WidgetInfo& info) {
    const int32_t selectedTrigger =
        CVarGetInteger(Rando::StaticData::Options[RO_SHUFFLE_MAJORA_SOUL].cvar, RO_MAJORA_SOUL_START_WITH);
    for (const MajoraSoulCountRow& row : MAJORA_SOUL_COUNT_ROWS) {
        if (info.name != row.label) {
            continue;
        }
        if (row.trigger == selectedTrigger && !IsDrawnByFleet(row.optionId)) {
            const auto& option = Rando::StaticData::Options[row.optionId];
            CVarSliderInt(
                row.label, option.cvar,
                IntSliderOptions().Min(0).Max(row.max).DefaultValue((int32_t)option.defaultValue).Tooltip(row.tooltip));
        }
        return;
    }
    SPDLOG_ERROR("Majora's Soul row '{}' has no definition", info.name);
}

static void DrawSkeletonKeyRow() {
    const char* skeletonKeyDisabledReason =
        CVarGetInteger(Rando::StaticData::Options[RO_PLACEMENT_SMALL_KEYS].cvar, RO_DUNGEON_ITEM_ANYWHERE) ==
                RO_DUNGEON_ITEM_START_WITH
            ? "Small Keys are set to Start With, so the Skeleton Key would have nothing to unlock"
            : nullptr;
    ItemPoolCheckbox("Skeleton Key", RO_SHUFFLE_SKELETON_KEY,
                     "Adds the Skeleton Key to the item pool. Collecting it immediately grants "
                     "the maximum number of Small Keys for every dungeon.",
                     1, skeletonKeyDisabledReason);
}

static void DrawRepeatableRupeesRow() {
    CVarCheckbox("Repeatable Rupee Purchases", Rando::StaticData::Options[RO_PURCHASE_INFINITE_RUPEES].cvar,
                 CheckboxOptions({ { .tooltip = "Shops can end up selling rupees as their randomized stock (e.g. a "
                                                "Silver Rupee for 1 rupee). When enabled, those rupees can be "
                                                "purchased any number of times per cycle; otherwise only once "
                                                "per cycle." } }));
}

static void DrawComingSoonRow() {
    if (!FleetRando_IsOptionDrawnByFleet(FLEET_HIDDEN_COMING_SOON)) {
        DrawComingSoonItems();
    }
}

static void DrawSariasSongRow() {
    ItemPoolCheckbox("Saria's Song", RO_SHUFFLE_SONG_SARIA,
                     "Adds Saria's Song to the item pool, playing it will give you a hint to a reachable "
                     "item, preferring items from your Priority Items list (configurable via the button to "
                     "the right) in order, and falling back to a random reachable major item or mask if none "
                     "of your priority items are currently available. The song is one time use, you will "
                     "lose it after using it.",
                     1);
    if (!IsOptionOn(RO_SHUFFLE_SONG_SARIA)) {
        return;
    }
    ImGui::SameLine();
    if (Button(ICON_FA_COG,
               ButtonOptions({ { .tooltip = "Configure the Priority Items list used by the Saria's Song hint" } })
                   .Size(ImVec2(0, 0)))) {
        ImGui::OpenPopup("PriorityItemsPopup");
    }
    ImGui::SetNextWindowSize(ImVec2(400.0f, 520.0f), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 6.0f);
    if (ImGui::BeginPopup("PriorityItemsPopup")) {
        DrawPriorityItemsPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

static void DrawTimeProgressionRow() {
    if (!IsOptionOn(RO_CLOCK_SHUFFLE)) {
        return;
    }
    static std::unordered_map<int32_t, const char*> clockModeOptions = {
        { RO_CLOCK_SHUFFLE_RANDOM, "Random" },
        { RO_CLOCK_SHUFFLE_ASCENDING, "Progressive: Ascending" },
        { RO_CLOCK_SHUFFLE_DESCENDING, "Progressive: Descending" },
    };
    UIWidgets::CVarCombobox(
        "Time Progression Mode", Rando::StaticData::Options[RO_CLOCK_SHUFFLE_PROGRESSIVE].cvar, &clockModeOptions,
        UIWidgets::ComboboxOptions().Tooltip(
            "Random: All 6 half-days shuffled randomly. Player starts with one random half-day.\n\n"
            "Progressive Ascending: Unlocks half-days in order (D1, N1, D2, N2, D3, N3).\n\n"
            "Progressive Descending: Unlocks half-days in reverse order (N3, D3, N2, D2, N1, D1)."));
}

static void DrawFinalHoursRow() {
    if (!IsOptionOn(RO_CLOCK_SHUFFLE)) {
        return;
    }
    int32_t terminalMinutes = CVarGetInteger(Rando::StaticData::Options[RO_CLOCK_TERMINAL_TIME].cvar, 350);
    int hours = terminalMinutes / 60;
    int minutes = terminalMinutes % 60;

    ImGui::Spacing();
    ImGui::Text("Final Hours Start Time: %02d:%02d", hours, minutes);
    ImGui::Spacing();
    UIWidgets::CVarSliderInt("Final Hours Start Time", Rando::StaticData::Options[RO_CLOCK_TERMINAL_TIME].cvar,
                             UIWidgets::IntSliderOptions()
                                 .Min(0)
                                 .Max(359)
                                 .DefaultValue(350)
                                 .LabelPosition(UIWidgets::LabelPosition::None)
                                 .Tooltip("Controls when the final hours countdown begins (00:00 to 05:59). "
                                          "When you run out of owned half-days, this allows the player control over "
                                          "how much time is left before the moon crash.\n\n"
                                          "This setting is baked into the seed and cannot be changed after "
                                          "generation."));
}

static void DrawTriforceHuntRow() {
    if (IsDrawnByFleet(RO_SHUFFLE_TRIFORCE_PIECES)) {
        return;
    }
    CVarCheckbox("Triforce Hunt", Rando::StaticData::Options[RO_SHUFFLE_TRIFORCE_PIECES].cvar,
                 CheckboxOptions({ { .tooltip = "Scatters Triforce pieces into the item pool. Collect the required "
                                                "amount to complete the hunt." } }));
}

static void DrawRequiredTriforcePiecesRow() {
    if (IsDrawnByFleet(RO_TRIFORCE_PIECES_REQUIRED) || !IsOptionOn(RO_SHUFFLE_TRIFORCE_PIECES)) {
        return;
    }
    CVarSliderInt(
        "Required Triforce Pieces", Rando::StaticData::Options[RO_TRIFORCE_PIECES_REQUIRED].cvar,
        IntSliderOptions()
            .Format("%d Pieces Required")
            .LabelPosition(UIWidgets::LabelPosition::None)
            .Min(1)
            .Max(CVarGetInteger(Rando::StaticData::Options[RO_TRIFORCE_PIECES_MAX].cvar, DEFAULT_TRIFORCE_PIECES_MAX))
            .DefaultValue(DEFAULT_TRIFORCE_PIECES_MAX));
}

static void DrawShuffledTriforcePiecesRow() {
    if (IsDrawnByFleet(RO_TRIFORCE_PIECES_MAX) || !IsOptionOn(RO_SHUFFLE_TRIFORCE_PIECES)) {
        return;
    }
    if (CVarSliderInt("Shuffled Triforce Pieces", Rando::StaticData::Options[RO_TRIFORCE_PIECES_MAX].cvar,
                      IntSliderOptions()
                          .Format("%d Pieces in Pool")
                          .LabelPosition(UIWidgets::LabelPosition::None)
                          .Min(1)
                          .Max(1000)
                          .DefaultValue(DEFAULT_TRIFORCE_PIECES_MAX)
                          .Tooltip("If the maximum amount of placeable pieces exceeds what will allow the seed to "
                                   "generate, the amount will be adjusted automatically."))) {
        ClampRequiredToMax(RO_TRIFORCE_PIECES_REQUIRED, RO_TRIFORCE_PIECES_MAX, DEFAULT_TRIFORCE_PIECES_MAX);
    }
}

static const char* DUNGEON_ITEM_PLACEMENT_HELP =
    "Anywhere - Anywhere in the world.\n\n"
    "Own Dungeon - Only within their own dungeon.\n\n"
    "Start With - You begin with them, and none are added to the item pool.\n\n"
    "Vanilla - They stay where the unrandomized game puts them.\n\n"
    "Any Dungeon - Within any of the four temples.\n\n"
    "Overworld - Anywhere outside the four temples.";

static void DrawDungeonItemPlacementRow(const char* label, RandoOptionId optionId, const char* subject) {
    if (IsDrawnByFleet(optionId)) {
        return;
    }
    const std::string tooltip = std::string("Where ") + subject + " may be placed.\n\n" + DUNGEON_ITEM_PLACEMENT_HELP;
    UIWidgets::CVarCombobox(label, Rando::StaticData::Options[optionId].cvar, &dungeonItemPlacementOptions,
                            UIWidgets::ComboboxOptions().Tooltip(tooltip.c_str()));
}

static void DrawSmallKeysRow() {
    DrawDungeonItemPlacementRow("Small Keys", RO_PLACEMENT_SMALL_KEYS, "each dungeon's small keys");
}

static void DrawBossKeysRow() {
    DrawDungeonItemPlacementRow("Boss Keys", RO_PLACEMENT_BOSS_KEYS, "each dungeon's boss key");
}

static void DrawStrayFairiesRow() {
    DrawDungeonItemPlacementRow("Stray Fairies", RO_PLACEMENT_STRAY_FAIRIES,
                                "each dungeon's Stray Fairies (not the Clock Town one)");
}

static bool AreStrayFairiesInPool() {
    return CVarGetInteger(Rando::StaticData::Options[RO_PLACEMENT_STRAY_FAIRIES].cvar, RO_DUNGEON_ITEM_ANYWHERE) !=
           RO_DUNGEON_ITEM_START_WITH;
}

static void DrawRequiredStrayFairiesRow() {
    if (!AreStrayFairiesInPool()) {
        return;
    }
    CVarSliderInt(
        "Required Stray Fairies", Rando::StaticData::Options[RO_STRAY_FAIRIES_REQUIRED].cvar,
        IntSliderOptions()
            .Tooltip("Minimum Stray Fairies needed to obtain the corresponding Great Fairy check.\n"
                     "Does not affect the Clock Town fairy.")
            .LabelPosition(UIWidgets::LabelPosition::None)
            .Min(1)
            .Format("%d Fairies Required")
            .Max(CVarGetInteger(Rando::StaticData::Options[RO_STRAY_FAIRIES_MAX].cvar, STRAY_FAIRY_SCATTERED_TOTAL))
            .DefaultValue(STRAY_FAIRY_SCATTERED_TOTAL));
}

static void DrawStrayFairiesInPoolRow() {
    if (!AreStrayFairiesInPool()) {
        return;
    }
    if (CVarSliderInt("Stray Fairies in Pool", Rando::StaticData::Options[RO_STRAY_FAIRIES_MAX].cvar,
                      IntSliderOptions()
                          .Tooltip("Maximum Stray Fairies that can appear in the item pool, per dungeon.")
                          .LabelPosition(UIWidgets::LabelPosition::None)
                          .Min(1)
                          .Format("%d Fairies in Pool")
                          .Max(STRAY_FAIRY_SCATTERED_TOTAL)
                          .DefaultValue(STRAY_FAIRY_SCATTERED_TOTAL))) {
        ClampRequiredToMax(RO_STRAY_FAIRIES_REQUIRED, RO_STRAY_FAIRIES_MAX, STRAY_FAIRY_SCATTERED_TOTAL);
    }
}

static void DrawTrapsRow() {
    if (IsDrawnByFleet(RO_SHUFFLE_TRAPS)) {
        return;
    }
    CVarCheckbox("Traps", Rando::StaticData::Options[RO_SHUFFLE_TRAPS].cvar,
                 CheckboxOptions({ { .tooltip = "Add trapped items to the pool. Traps disguise themselves as "
                                                "items you have not obtained yet." } }));
}

static void DrawTrapAmountRow() {
    if (IsDrawnByFleet(RO_TRAP_AMOUNT) || !IsOptionOn(RO_SHUFFLE_TRAPS)) {
        return;
    }
    CVarSliderInt("##trapcount", Rando::StaticData::Options[RO_TRAP_AMOUNT].cvar,
                  IntSliderOptions({ { .tooltip = "How many Traps are shuffled into the Item Pool." } })
                      .LabelPosition(LabelPosition::None)
                      .Color(UIWidgets::Colors(CVarGetInteger("gSettings.Menu.Theme", 5)))
                      .Format("Traps: %i")
                      .Min(1)
                      .Max(100)
                      .DefaultValue(5));
}

static void DrawTrapBehaviorRow() {
    if (!IsOptionOn(RO_SHUFFLE_TRAPS)) {
        return;
    }
    UIWidgets::CVarCombobox(
        "Trap Behavior", "gRando.TrapItems", &trapItemsOptions,
        UIWidgets::ComboboxOptions(
            { { .tooltip = "Default (Dynamic): Trap items will change dynamically as you progress, ensuring they "
                           "are an item you have not obtained yet for maximum trickery.\n\nStatic: Trap items "
                           "will be static, according to the randomizer seed.\n\nNot tied to the seed - can be "
                           "changed at any time." } }));
}

static void DrawTrapTypesRow() {
    if (!IsOptionOn(RO_SHUFFLE_TRAPS)) {
        return;
    }
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, DIM_TEXT_COLOR);
    ImGui::SeparatorText("Trap Types");
    ImGui::PopStyleColor();
    struct TrapType {
        const char* label;
        const char* cvar;
        const char* tooltip;
    };
    static const TrapType trapTypes[] = {
        { "Freeze", "gRando.Traps.Freeze", "Freezes Link in place." },
        { "Blast", "gRando.Traps.Blast", "Link explodes with Powder Keg force." },
        { "Shock", "gRando.Traps.Shock", "Shocks Link for a few seconds." },
        { "Jinx", "gRando.Traps.Jinx", "Afflicts Link with Jinx." },
        { "Wallet", "gRando.Traps.Wallet", "Links rupees scatter around him." },
        // This only spawns a Like Like, more enemies may be added in the future but each would need fine tuning
        { "Like Like", "gRando.Traps.Enemy", "Spawns a Like Like on top of Link." },
        { "Time", "gRando.Traps.Time", "Advances Time 90 Minutes (Game Time)." },
        { "Fire", "gRando.Traps.Fire", "Deals Fire to Link." },
        { "Knockback", "gRando.Traps.Knockback", "Knocks Link back." },
    };
    if (ImGui::BeginTable("trapTypesTable", 2)) {
        for (const TrapType& trapType : trapTypes) {
            if (FleetRando_IsOptionDrawnByFleet(trapType.cvar)) {
                continue;
            }
            ImGui::TableNextColumn();
            CVarCheckbox(trapType.label, trapType.cvar, CheckboxOptions({ { .tooltip = trapType.tooltip } }));
        }
        ImGui::EndTable();
    }
}

static bool sIsInStartingOptionsTable = false;
static f32 sStartingItemsListHeight = 0.0f;

static void BeginStartingOptions() {
    sStartingItemsListHeight = ImGui::GetContentRegionAvail().y / 4 - (ImGui::GetStyle().ItemSpacing.y * 4);
    ImGui::BeginChild("randoStartingOptions", ImVec2(0, 120.0f));
    ImGui::SeparatorText("Starting Options");
    sIsInStartingOptionsTable = ImGui::BeginTable("Starting Options", 3);
}

static void EndStartingOptions() {
    if (sIsInStartingOptionsTable) {
        ImGui::EndTable();
        sIsInStartingOptionsTable = false;
    }
    ImGui::EndChild();
}

static void NextStartingOptionCell() {
    if (sIsInStartingOptionsTable) {
        ImGui::TableNextColumn();
    }
}

static void DrawStartingCheckboxRow(const char* label, RandoOptionId optionId, const char* tooltip) {
    if (IsDrawnByFleet(optionId)) {
        return;
    }
    NextStartingOptionCell();
    CVarCheckbox(label, Rando::StaticData::Options[optionId].cvar, CheckboxOptions({ { .tooltip = tooltip } }));
}

static void DrawStartingHealthRow() {
    if (IsDrawnByFleet(RO_STARTING_HEALTH)) {
        return;
    }
    NextStartingOptionCell();
    CVarSliderInt("Health", Rando::StaticData::Options[RO_STARTING_HEALTH].cvar,
                  IntSliderOptions()
                      .Min(1)
                      .Max(20)
                      .DefaultValue(3)
                      .LabelPosition(LabelPosition::None)
                      .Format("%d Hearts")
                      .Color(Colors::Red));
}

static bool IsStartingItemsListDrawnByFleet() {
    return FleetRando_IsOptionDrawnByFleet(FLEET_HIDDEN_STARTING_ITEMS);
}

static void DrawStartingItemsList() {
    if (IsStartingItemsListDrawnByFleet()) {
        return;
    }
    ImGui::BeginChild("randoStartingItems1", ImVec2(0, sStartingItemsListHeight));
    ImGui::SeparatorText("Starting Items");
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(15, 15));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));

    auto setStartingItemsList = Rando::GetStartingItemsFromConfig();

    uint32_t listIndex = 0;
    for (auto& startingItem : setStartingItemsList) {
        ImGui::PushID(listIndex);
        ImVec2 imageSize = ImVec2(42.0f, 42.0f);
        if ((startingItem >= RI_SONG_DOUBLE_TIME && startingItem <= RI_SONG_TIME) ||
            startingItem == RI_PROGRESSIVE_LULLABY) {
            imageSize.x /= 1.5f;
        }

        Rando::StaticData::RandoStaticItem randoStaticItem = Rando::StaticData::Items[startingItem];
        const char* texturePath = Rando::StaticData::GetIconTexturePath(startingItem);
        ImTextureID textureId =
            std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui())
                ->GetTextureByName(texturePath);

        ImVec4 tintColor =
            Ship_GetItemColorTint(startingItem == RI_PROGRESSIVE_LULLABY ? ITEM_SONG_LULLABY : randoStaticItem.itemId);
        std::string tooltipText = randoStaticItem.name;
        bool isProgressiveMode = CVarGetInteger(Rando::StaticData::Options[RO_CLOCK_SHUFFLE_PROGRESSIVE].cvar,
                                                RO_CLOCK_SHUFFLE_RANDOM) != RO_CLOCK_SHUFFLE_RANDOM;
        ApplyClockItemRendering(startingItem, tintColor, tooltipText, isProgressiveMode);

        if (ImGui::ImageButton(std::to_string(listIndex).c_str(), textureId, imageSize, ImVec2(0, 0), ImVec2(1, 1),
                               ImVec4(0, 0, 0, 0), tintColor)) {
            setStartingItemsList.erase(setStartingItemsList.begin() + listIndex);
            Rando::SetStartingItemsInConfig(setStartingItemsList);
            RefreshMetrics();
        }
        UIWidgets::Tooltip(tooltipText.c_str());
        listIndex++;

        if ((listIndex + 1) % 15 != 0) {
            ImGui::SameLine();
        }
        ImGui::PopID();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    ImGui::EndChild();
}

static void DrawAvailableStartingItems() {
    if (IsStartingItemsListDrawnByFleet()) {
        return;
    }
    auto setStartingItemsList = Rando::GetStartingItemsFromConfig();
    int tableColumns = 0;
    ImGui::BeginChild("randoStartingItems2", ImVec2(0, 0));
    ImGui::SeparatorText("Available Items");

    for (auto& category : Rando::StaticData::StartingItemsMap) {
        tableColumns = 5;
        if (category.first == STARTING_ITEMS_MASK) {
            tableColumns++;
        } else if (category.first == STARTING_ITEMS_MISC) {
            tableColumns = 6; // Need 6 columns for the 6 time items on their own row
        }
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        if (ImGui::BeginChild(std::to_string(category.first).c_str(), ImVec2(0, 0),
                              ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX |
                                  ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));

            if (ImGui::BeginTable(std::to_string(category.first).c_str(), tableColumns)) {
                for (int i = 0; i < tableColumns; i++) {
                    ImGui::TableSetupColumn("item", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                }
                for (auto& item : category.second) {
                    if (setOfItemsInPool.count(item) == 0) {
                        // Skip items that are not in the item pool
                        continue;
                    }

                    ImVec2 imageSize = ImVec2(42.0f, 42.0f);
                    if ((item >= RI_SONG_DOUBLE_TIME && item <= RI_SONG_TIME) || item == RI_PROGRESSIVE_LULLABY) {
                        imageSize.x /= 1.5f;
                    }

                    Rando::StaticData::RandoStaticItem randoStaticItem = Rando::StaticData::Items[item];
                    const char* texturePath = Rando::StaticData::GetIconTexturePath(item);
                    ImTextureID textureId = std::dynamic_pointer_cast<Fast::Fast3dGui>(
                                                Ship::Context::GetRawInstance()->GetWindow()->GetGui())
                                                ->GetTextureByName(texturePath);

                    // Force new row for Song of Time, first frog, and first time item
                    if (item == RI_SONG_TIME || item == RI_FROG_BLUE || item == RI_TIME_DAY_1) {
                        ImGui::TableNextRow();
                    }
                    ImGui::TableNextColumn();

                    ImVec4 tintColor = Ship_GetItemColorTint(item == RI_PROGRESSIVE_LULLABY ? ITEM_SONG_LULLABY
                                                                                            : randoStaticItem.itemId);
                    std::string tooltipText = randoStaticItem.name;
                    bool isProgressiveMode =
                        CVarGetInteger(Rando::StaticData::Options[RO_CLOCK_SHUFFLE_PROGRESSIVE].cvar,
                                       RO_CLOCK_SHUFFLE_RANDOM) != RO_CLOCK_SHUFFLE_RANDOM;
                    ApplyClockItemRendering(item, tintColor, tooltipText, isProgressiveMode);

                    if (ImGui::ImageButton(std::to_string(item).c_str(), textureId, imageSize, ImVec2(0, 0),
                                           ImVec2(1, 1), ImVec4(0, 0, 0, 0), tintColor)) {
                        u8 maxCount = Rando::StaticData::MaxStartingItemsMap.count(item)
                                          ? Rando::StaticData::MaxStartingItemsMap[item]
                                          : 1;
                        if (item == RI_PROGRESSIVE_WALLET &&
                            CVarGetInteger(Rando::StaticData::Options[RO_SHUFFLE_TYCOON_WALLET].cvar, 0)) {
                            maxCount = 3;
                        }
                        if (std::count(setStartingItemsList.begin(), setStartingItemsList.end(), item) < maxCount) {

                            setStartingItemsList.push_back(item);
                            Rando::SetStartingItemsInConfig(setStartingItemsList);
                            RefreshMetrics();
                        }
                    }
                    UIWidgets::Tooltip(tooltipText.c_str());
                }
                ImGui::EndTable();
            }

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
            ImGui::EndChild();
        }
        ImGui::PopStyleColor(1);
        if (category.first != STARTING_ITEMS_QUEST) {
            ImGui::SameLine();
        }
    }
    ImGui::EndChild();
}

static void BulkExcludeChecks(const std::vector<RandoCheckId>& checks) {
    for (RandoCheckId randoCheckId : checks) {
        auto it = std::lower_bound(checkExclusionList.begin(), checkExclusionList.end(), randoCheckId);
        if (it == checkExclusionList.end() || *it != randoCheckId) {
            checkExclusionList.insert(it, randoCheckId);
        }
    }
    SaveExcludedChecks();
}

static void BulkRestoreChecks(const std::vector<RandoCheckId>& checks) {
    std::set<RandoCheckId> checkSet(checks.begin(), checks.end());
    std::erase_if(checkExclusionList, [&](const RandoCheckId& checkId) { return checkSet.contains(checkId); });
    SaveExcludedChecks();
}

static const std::vector<std::string>& GetCheckFilterGroupLabels() {
    static std::vector<std::string> labels;
    static uint32_t builtGeneration = 0;
    if (!labels.empty() && builtGeneration == checkPoolGeneration) {
        return labels;
    }

    labels.clear();
    builtGeneration = checkPoolGeneration;
    for (auto& checkFilterGroup : GetCheckFilterGroups()) {
        size_t count;
        if (checkFilterGroup.checks.empty()) {
            count = setOfChecksInPool.size() + checkExclusionList.size();
        } else {
            count = std::count_if(
                checkFilterGroup.checks.begin(), checkFilterGroup.checks.end(), [](RandoCheckId randoCheckId) {
                    return setOfChecksInPool.contains(randoCheckId) ||
                           std::binary_search(checkExclusionList.begin(), checkExclusionList.end(), randoCheckId);
                });
        }
        labels.push_back(checkFilterGroup.label + " (" + std::to_string(count) + ")");
    }
    return labels;
}

static void DrawCheckGroupFilter(UIWidgets::Colors menuThemeColor) {
    const auto& checkFilterGroups = GetCheckFilterGroups();
    const auto& labels = GetCheckFilterGroupLabels();

    f32 comboWidth = 0.0f;
    for (auto& label : labels) {
        comboWidth = std::max(comboWidth, UIWidgets::CalcComboWidth(label.c_str(), 0));
    }

    ImGui::BeginGroup();
    UIWidgets::PushStyleCombobox(menuThemeColor);
    ImGui::SetNextItemWidth(comboWidth);
    if (ImGui::BeginCombo("##checkGroupFilter", labels[selectedCheckFilterGroup].c_str())) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
        for (int32_t i = 0; i < (int32_t)checkFilterGroups.size(); i++) {
            if (ImGui::Selectable(labels[i].c_str(), i == selectedCheckFilterGroup)) {
                selectedCheckFilterGroup = i;
            }
            if (!checkFilterGroups[i].tooltip.empty()) {
                UIWidgets::Tooltip(checkFilterGroups[i].tooltip.c_str());
            }
        }
        ImGui::PopStyleVar();
        ImGui::EndCombo();
    }
    UIWidgets::PopStyleCombobox();
    ImGui::EndGroup();
    UIWidgets::Tooltip("Only show checks from a curated group or check type.\n"
                       "Bulk actions apply to the checks currently shown.");
}

static bool IsCheckShownAsShuffled(RandoCheckId randoCheckId, ImGuiTextFilter& filter) {
    return randoCheckId != RC_UNKNOWN && setOfChecksInPool.contains(randoCheckId) &&
           PassesCheckGroupFilter(randoCheckId) &&
           filter.PassFilter(Rando::StaticData::CheckNames[randoCheckId].c_str());
}

static bool IsCheckShownAsExcluded(RandoCheckId randoCheckId, ImGuiTextFilter& filter) {
    return PassesCheckGroupFilter(randoCheckId) &&
           filter.PassFilter(Rando::StaticData::CheckNames[randoCheckId].c_str());
}

static RandoCheckId DrawCheckList(const char* id, const char* title, const char* rowTooltip,
                                  const std::vector<RandoCheckId>& shownChecks, const char* bulkVerb,
                                  UIWidgets::Colors bulkColor, bool& bulkActionFlag, f32 columnWidth) {
    RandoCheckId clickedCheck = RC_UNKNOWN;

    ImGui::PushID(id);
    ImGui::BeginChild("CheckListPane", ImVec2(columnWidth, ImGui::GetContentRegionAvail().y), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    char label[64];
    snprintf(label, sizeof(label), "%s (%d shown)", title, (int)shownChecks.size());
    ImGui::SeparatorText(label);

    snprintf(label, sizeof(label), "%s All %d Shown", bulkVerb, (int)shownChecks.size());
    if (Button(label, ButtonOptions({ { .disabled = shownChecks.empty() } })
                          .Size(UIWidgets::Sizes::Inline)
                          .Padding(ImVec2(16.0f, 6.0f))
                          .Color(bulkColor))) {
        bulkActionFlag = true;
    }

    ImGui::BeginChild("CheckListRows", ImVec2(0, 0));
    ImGuiListClipper clipper;
    clipper.Begin((int)shownChecks.size());
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            RandoCheckId randoCheckId = shownChecks[i];
            if (ImGui::Selectable(Rando::StaticData::CheckNames[randoCheckId].c_str())) {
                clickedCheck = randoCheckId;
            }
            UIWidgets::Tooltip(rowTooltip);
        }
    }
    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::PopID();

    return clickedCheck;
}

static void DrawCheckExclusionsTab() {
    if (CVarGetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, RO_LOGIC_GLITCHLESS) >= RO_LOGIC_VANILLA) {
        ImGui::TextColored(UIWidgets::ColorValues.at(UIWidgets::Colors::Red),
                           "This setting is not compatible with Vanilla Logic.");
        return;
    }

    auto menuThemeColor = UIWidgets::Colors(CVarGetInteger("gSettings.Menu.Theme", LightBlue));
    bool excludeShownChecks = false;
    bool restoreShownChecks = false;
    static ImGuiTextFilter checkSearchFilter;
    static std::vector<RandoCheckId> shownShuffledChecks;
    static std::vector<RandoCheckId> shownExcludedChecks;

    DrawSeedHealthStrip();

    DrawCheckGroupFilter(menuThemeColor);
    ImGui::SameLine();
    DrawSearchFilter(checkSearchFilter, "##checkSearchFilter", "Search both lists...", menuThemeColor);

    // Filtering every check is costly; only rebuild when the pool, group filter, or search text changes
    static uint32_t shownListsGeneration = 0;
    static int32_t shownListsGroup = -1;
    static char shownListsSearch[sizeof(checkSearchFilter.InputBuf)] = "";
    if (shownListsGeneration != checkPoolGeneration || shownListsGroup != selectedCheckFilterGroup ||
        strcmp(shownListsSearch, checkSearchFilter.InputBuf) != 0) {
        shownListsGeneration = checkPoolGeneration;
        shownListsGroup = selectedCheckFilterGroup;
        strcpy(shownListsSearch, checkSearchFilter.InputBuf);

        shownShuffledChecks.clear();
        for (auto& includedChecks : Rando::StaticData::Checks) {
            if (IsCheckShownAsShuffled(includedChecks.first, checkSearchFilter)) {
                shownShuffledChecks.push_back(includedChecks.first);
            }
        }
        shownExcludedChecks.clear();
        for (RandoCheckId randoCheckId : checkExclusionList) {
            if (IsCheckShownAsExcluded(randoCheckId, checkSearchFilter)) {
                shownExcludedChecks.push_back(randoCheckId);
            }
        }
    }

    f32 columnWidth = ImGui::GetContentRegionAvail().x / 2 - (ImGui::GetStyle().ItemSpacing.x * 2);

    RandoCheckId checkToExclude =
        DrawCheckList("randoIncludedChecks", "In Pool", "Click to exclude", shownShuffledChecks, "Exclude",
                      UIWidgets::Colors::Red, excludeShownChecks, columnWidth);
    ImGui::SameLine();
    RandoCheckId checkToRestore =
        DrawCheckList("randoExcludedChecks", "Excluded", "Click to restore", shownExcludedChecks, "Restore",
                      UIWidgets::Colors::Green, restoreShownChecks, columnWidth);

    if (checkToExclude != RC_UNKNOWN) {
        auto it = std::lower_bound(checkExclusionList.begin(), checkExclusionList.end(), checkToExclude);
        if (it == checkExclusionList.end() || *it != checkToExclude) {
            checkExclusionList.insert(it, checkToExclude);
            SaveExcludedChecks();
        }
    }

    if (checkToRestore != RC_UNKNOWN) {
        std::erase(checkExclusionList, checkToRestore);
        SaveExcludedChecks();
    }

    if (excludeShownChecks) {
        BulkExcludeChecks(shownShuffledChecks);
    }

    if (restoreShownChecks) {
        BulkRestoreChecks(shownExcludedChecks);
    }
}

static void BeginHintsColumn() {
    f32 columnWidth = ImGui::GetContentRegionAvail().x / 3 - (ImGui::GetStyle().ItemSpacing.x * 2);
    ImGui::BeginChild("randoHintsColumn1", ImVec2(columnWidth, 0));
}

struct HintRow {
    const char* label;
    RandoOptionId optionId;
    const char* tooltip;
};

static const HintRow HINT_ROWS[] = {
    { "Spider House", RO_HINTS_SPIDER_HOUSES,
      "Swamp Spider House: Hinted at his normal location within the Swamp Spider House\n\nOcean Spider "
      "House: Hinted in South Clock Town day 1, by the main standing on the scaffolding." },
    { "Gossip Stone Static Hint", RO_HINTS_GOSSIP_STONES,
      "Each gossip stone will give a static hint about the contents of a random location." },
    { "Gossip Stone Purchaseable", RO_HINTS_PURCHASEABLE,
      "Gossip stones will offer a hint for a scaling rupee cost. This cost ranges "
      "from 10-250 rupees depending on how many checks are remaining in your seed. "
      "The hint will guaranteed be a check you have not obtained yet." },
    { "Boss Remains", RO_HINTS_BOSS_REMAINS,
      "Lists the location of the Boss remains on the guard recruitment posters around Clock Town" },
    { "Oath to Order", RO_HINTS_OATH_TO_ORDER,
      "Once you have the Moon Access Requirements, talking to Skull Kid on "
      "the Clock Tower Rooftop will hint the location of Oath to Order" },
    { "Transformation Masks", RO_HINTS_TRANSFORMATIONS,
      "Checking the sign near the Business Scrub in South Clock Town "
      "will reveal the location of Transformation Masks.\n"
      "Note: This excludes Fierce Deity." },
    { "Song of Soaring", RO_HINTS_SONG_OF_SOARING,
      "Hints the location of the Song of Soaring at it's vanilla location" },
    { "Hookshot Location", RO_HINTS_HOOKSHOT,
      "The Zora in Great Bay Coast, near Pirates Fortress, will hint the location of the Hookshot." },
    { "Bank Reward", RO_HINTS_BANK_SIGN,
      "The sign next to the Bank in West Clock Town will describe a "
      "promotion for the Piece of Heart Check." },
};

static void DrawHintRow(WidgetInfo& info) {
    for (const HintRow& row : HINT_ROWS) {
        if (info.name != row.label) {
            continue;
        }
        if (!IsDrawnByFleet(row.optionId)) {
            CVarCheckbox(row.label, Rando::StaticData::Options[row.optionId].cvar,
                         CheckboxOptions({ { .tooltip = row.tooltip } }));
        }
        return;
    }
    SPDLOG_ERROR("Hints row '{}' has no definition", info.name);
}

static void DrawGossipStrengthRow() {
    CVarSliderInt("Gossip Stone Hint Strength", Rando::StaticData::Options[RO_HINTS_GOSSIP_STONE_STRENGTH].cvar,
                  IntSliderOptions().Min(0).Max(100).DefaultValue(50).Tooltip(
                      "Controls how strongly gossip stone hints are weighted toward important items.\n"
                      "At 0 all checks are equally likely. At 100 the full weights below apply.\n"
                      "\n"
                      "Item weights (higher = more likely to be hinted):\n"
                      "  Majora's Soul              13\n"
                      "  Deku / Goron / Zora Masks  12\n"
                      "  Blast / Fierce Deity Masks 11\n"
                      "  Boss Souls & Remains       10\n"
                      "\n"
                      "Check weights (overrides item weight):\n"
                      "  Seahorse Reunion           10\n"
                      "  New Wave Bossa Nova        10\n"
                      "  Frog Choir                 10\n"
                      "  Couple's Mask              10\n"
                      "  Romani Ranch Aliens        10\n"
                      "  Beaver Race 1 & 2           8\n"
                      "  Keaton Quiz                 8\n"
                      "  Curiosity Shop Special Item 8\n"
                      "  Deku Playground All Days    8\n"
                      "  Moon Trial Hearts (x3)      6\n"
                      "\n"
                      "Item type weights (fallback):\n"
                      "  Major / Mask                9\n"
                      "  Boss Key                    8\n"
                      "  Lesser                      6\n"
                      "  Small Key                   5\n"
                      "  Skulltula / Stray Fairy     3\n"
                      "  Health / Junk               2"));
}

static void DrawGeneralActorHintsRow() {
    CVarCheckbox(
        "General Actor Hints", "gPlaceholderBool",
        CheckboxOptions({ { .disabled = true,
                            .disabledTooltip = "Soon you will be able to disable these. Currently hinted:\n- Bomb Shop "
                                               "4th Item\n- Lottery\n- Great Fairy Fountains\n- Mountain Smithy" } })
            .DefaultValue(true));
}

static constexpr const char* SECTION_PREFIX = "Section: ";

static const char* OptionCvar(RandoOptionId optionId) {
    return Rando::StaticData::Options[optionId].cvar;
}

static void AddRow(WidgetPath& path, const char* name, const char* cvar, WidgetFunc draw) {
    mBenMenu->AddWidget(path, name, WIDGET_CUSTOM).CVar(cvar).CustomFunction(draw);
}

static void AddLayout(WidgetPath& path, const char* name, WidgetFunc draw) {
    mBenMenu->AddWidget(path, name, WIDGET_CUSTOM).CustomFunction(draw).HideInSearch(true);
}

static void BeginSection(WidgetInfo& info) {
    UIWidgets::BeginCard(info.name.c_str());
    ImGui::SeparatorText(info.name.c_str() + strlen(SECTION_PREFIX));
}

static void EndSection(WidgetInfo& info) {
    UIWidgets::EndCard();
}

static void AddSection(WidgetPath& path, const std::string& title) {
    AddLayout(path, (SECTION_PREFIX + title).c_str(), BeginSection);
}

static void AddSectionEnd(WidgetPath& path, const std::string& title) {
    AddLayout(path, ("Section End: " + title).c_str(), EndSection);
}

static void DrawCheckPoolRow(WidgetInfo& info) {
    for (const CheckPoolRow& row : CHECK_POOL_ROWS) {
        if (info.name == row.label) {
            CheckPoolCheckbox(row.label, row.optionId, row.checkType, row.tooltip);
            return;
        }
    }
    for (const CheckPoolRow& row : SCENERY_DROP_GROUPS) {
        if (info.name == row.label) {
            CheckPoolCheckbox(row.label, row.optionId, row.checkType, row.tooltip);
            return;
        }
    }
    SPDLOG_ERROR("Check Pool row '{}' has no definition", info.name);
}

static void AddItemPoolRow(WidgetPath& path, const char* label) {
    for (const ItemPoolRow& row : ITEM_POOL_ROWS) {
        if (strcmp(label, row.label) == 0) {
            AddRow(path, label, OptionCvar(row.optionId), DrawItemPoolRow);
            return;
        }
    }
    SPDLOG_ERROR("Item Pool row '{}' has no definition", label);
}

static void RegisterGeneralRows(WidgetPath& path) {
    AddLayout(path, "Layout: General", [](WidgetInfo& info) { ImGui::BeginChild("randoSettings"); });
    AddRow(path, "Intro", nullptr, [](WidgetInfo& info) { DrawGeneralIntro(); });
    AddRow(path, "Seed Generation", nullptr, [](WidgetInfo& info) {
        if (!FleetRando_IsOptionDrawnByFleet(FLEET_HIDDEN_SEED_GENERATION)) {
            DrawSeedGeneration();
        }
    });
    AddRow(path, "Live Options", nullptr, [](WidgetInfo& info) { DrawLiveOptionsHeader(); });
    AddRow(path, "Container Style Matches Contents", "gRando.CSMC", [](WidgetInfo& info) { DrawContainerStyleRow(); });
    AddRow(path, "Junk Items", "gRando.JunkItems", [](WidgetInfo& info) { DrawJunkItemsRow(); });
    AddLayout(path, "Layout End: General", [](WidgetInfo& info) {
        ImGui::EndChild();
        ImGui::SameLine();
    });
}

static void RegisterLogicRows(WidgetPath& path) {
    AddLayout(path, "Layout: Logic Column 1", [](WidgetInfo& info) { BeginFirstColumn("randoLogicColumn1"); });
    AddRow(path, "Logic", OptionCvar(RO_LOGIC), [](WidgetInfo& info) {
        if (!IsDrawnByFleet(RO_LOGIC)) {
            DrawLogicCombobox();
        }
    });
    AddRow(path, "Dungeon Access", OptionCvar(RO_ACCESS_DUNGEONS), [](WidgetInfo& info) { DrawDungeonAccessRow(); });
    AddRow(path, "Trials Access", OptionCvar(RO_ACCESS_TRIALS), [](WidgetInfo& info) { DrawTrialsAccessRow(); });
    AddLayout(path, "Layout: Logic Column 2", [](WidgetInfo& info) { BeginSecondColumn("randoLogicColumn2"); });
    AddRow(path, "Majora Access Remains Required", OptionCvar(RO_ACCESS_MAJORA_REMAINS_COUNT), [](WidgetInfo& info) {
        DrawAccessCountRow("Majora Access Remains Required", RO_ACCESS_MAJORA_REMAINS_COUNT, 4, 0);
    });
    AddRow(path, "Majora Access Masks Required", OptionCvar(RO_ACCESS_MAJORA_MASKS_COUNT), [](WidgetInfo& info) {
        DrawAccessCountRow("Majora Access Masks Required", RO_ACCESS_MAJORA_MASKS_COUNT, 20, 0);
    });
    AddRow(path, "Moon Access Remains Required", OptionCvar(RO_ACCESS_MOON_REMAINS_COUNT), [](WidgetInfo& info) {
        DrawAccessCountRow("Moon Access Remains Required", RO_ACCESS_MOON_REMAINS_COUNT, 4, 4);
    });
    AddRow(path, "Moon Access Masks Required", OptionCvar(RO_ACCESS_MOON_MASKS_COUNT), [](WidgetInfo& info) {
        DrawAccessCountRow("Moon Access Masks Required", RO_ACCESS_MOON_MASKS_COUNT, 20, 0);
    });
    AddRow(path, "Moon Access Triforce Pieces Required", OptionCvar(RO_ACCESS_MOON_TRIFORCE_COUNT),
           [](WidgetInfo& info) { DrawMoonTriforceRow(); });
    AddLayout(path, "Layout End: Logic", [](WidgetInfo& info) { ImGui::EndChild(); });
}

static void AddCheckPoolRows(WidgetPath& path, const CheckPoolRow* rows, size_t count) {
    for (size_t i = 0; i < count; i++) {
        AddRow(path, rows[i].label, OptionCvar(rows[i].optionId), DrawCheckPoolRow);
    }
}

static void RegisterCheckPoolRows(WidgetPath& path) {
    AddRow(path, "Seed Health", nullptr, [](WidgetInfo& info) { DrawSeedHealthStrip(); });
    AddLayout(path, "Layout: Check Pool", [](WidgetInfo& info) { UIWidgets::BeginCardLayout(POOL_CARD_LAYOUT); });
    AddSection(path, "World & NPCs");
    AddRow(path, "Songs", nullptr, [](WidgetInfo& info) { DrawSongsPlaceholderRow(); });
    AddCheckPoolRows(path, CHECK_POOL_ROWS, CHECK_POOL_WORLD_ROW_COUNT);
    AddSectionEnd(path, "World & NPCs");
    AddSection(path, "Creatures & Collectibles");
    AddCheckPoolRows(path, CHECK_POOL_ROWS + CHECK_POOL_WORLD_ROW_COUNT,
                     std::size(CHECK_POOL_ROWS) - CHECK_POOL_WORLD_ROW_COUNT);
    AddRow(path, "Gold Skulltulas", OptionCvar(RO_SHUFFLE_GOLD_SKULLTULAS),
           [](WidgetInfo& info) { DrawGoldSkulltulasRow(); });
    AddRow(path, "Required Gold Skulltula Tokens", OptionCvar(RO_SKULLTULA_TOKENS_REQUIRED),
           [](WidgetInfo& info) { DrawRequiredSkulltulaTokensRow(); });
    AddRow(path, "Shuffled Gold Skulltulas", OptionCvar(RO_SKULLTULA_SHUFFLED),
           [](WidgetInfo& info) { DrawShuffledSkulltulasRow(); });
    AddSectionEnd(path, "Creatures & Collectibles");
    AddSection(path, "Scenery Drops");
    AddRow(path, "Scenery Drops Toggles", nullptr, [](WidgetInfo& info) { DrawSceneryDropsToggles(); });
    AddCheckPoolRows(path, SCENERY_DROP_GROUPS, std::size(SCENERY_DROP_GROUPS));
    AddSectionEnd(path, "Scenery Drops");
    AddLayout(path, "Layout End: Check Pool", [](WidgetInfo& info) { UIWidgets::EndCardLayout(); });
}

static void RegisterItemPoolRows(WidgetPath& path) {
    AddRow(path, "Seed Health", nullptr, [](WidgetInfo& info) { DrawSeedHealthStrip(); });
    AddLayout(path, "Layout: Item Pool", [](WidgetInfo& info) { UIWidgets::BeginCardLayout(POOL_CARD_LAYOUT); });
    AddSection(path, "Abilities & Upgrades");
    AddItemPoolRow(path, "Swim");
    AddItemPoolRow(path, "Ocarina Buttons");
    AddRow(path, "Skeleton Key", OptionCvar(RO_SHUFFLE_SKELETON_KEY), [](WidgetInfo& info) { DrawSkeletonKeyRow(); });
    AddItemPoolRow(path, "Tycoon's Wallet");
    AddRow(path, "Repeatable Rupee Purchases", OptionCvar(RO_PURCHASE_INFINITE_RUPEES),
           [](WidgetInfo& info) { DrawRepeatableRupeesRow(); });
    AddRow(path, "Coming Soon", nullptr, [](WidgetInfo& info) { DrawComingSoonRow(); });
    AddSectionEnd(path, "Abilities & Upgrades");
    AddSection(path, "Extra Songs");
    AddItemPoolRow(path, "Sun's Song");
    AddItemPoolRow(path, "Song of Double Time");
    AddItemPoolRow(path, "Inverted Song of Time");
    AddRow(path, "Saria's Song", OptionCvar(RO_SHUFFLE_SONG_SARIA), [](WidgetInfo& info) { DrawSariasSongRow(); });
    AddSectionEnd(path, "Extra Songs");
    AddSection(path, "Souls");
    AddItemPoolRow(path, "Boss Souls");
    AddRow(path, "Majora's Soul", OptionCvar(RO_SHUFFLE_MAJORA_SOUL), [](WidgetInfo& info) { DrawMajoraSoulRow(); });
    for (const MajoraSoulCountRow& row : MAJORA_SOUL_COUNT_ROWS) {
        AddRow(path, row.label, OptionCvar(row.optionId), DrawMajoraSoulCountRow);
    }
    AddItemPoolRow(path, "Enemy Souls");
    AddSectionEnd(path, "Souls");
    AddSection(path, "Shuffle Time");
    AddItemPoolRow(path, "Shuffle Time");
    AddRow(path, "Time Progression Mode", OptionCvar(RO_CLOCK_SHUFFLE_PROGRESSIVE),
           [](WidgetInfo& info) { DrawTimeProgressionRow(); });
    AddRow(path, "Final Hours Start Time", OptionCvar(RO_CLOCK_TERMINAL_TIME),
           [](WidgetInfo& info) { DrawFinalHoursRow(); });
    AddSectionEnd(path, "Shuffle Time");
    AddSection(path, "Goals & Collectibles");
    AddRow(path, "Triforce Hunt", OptionCvar(RO_SHUFFLE_TRIFORCE_PIECES),
           [](WidgetInfo& info) { DrawTriforceHuntRow(); });
    AddRow(path, "Required Triforce Pieces", OptionCvar(RO_TRIFORCE_PIECES_REQUIRED),
           [](WidgetInfo& info) { DrawRequiredTriforcePiecesRow(); });
    AddRow(path, "Shuffled Triforce Pieces", OptionCvar(RO_TRIFORCE_PIECES_MAX),
           [](WidgetInfo& info) { DrawShuffledTriforcePiecesRow(); });
    AddSectionEnd(path, "Goals & Collectibles");
    AddSection(path, "Dungeon Items");
    AddRow(path, "Small Keys", OptionCvar(RO_PLACEMENT_SMALL_KEYS), [](WidgetInfo& info) { DrawSmallKeysRow(); });
    AddRow(path, "Boss Keys", OptionCvar(RO_PLACEMENT_BOSS_KEYS), [](WidgetInfo& info) { DrawBossKeysRow(); });
    AddRow(path, "Stray Fairies", OptionCvar(RO_PLACEMENT_STRAY_FAIRIES),
           [](WidgetInfo& info) { DrawStrayFairiesRow(); });
    AddRow(path, "Required Stray Fairies", OptionCvar(RO_STRAY_FAIRIES_REQUIRED),
           [](WidgetInfo& info) { DrawRequiredStrayFairiesRow(); });
    AddRow(path, "Stray Fairies in Pool", OptionCvar(RO_STRAY_FAIRIES_MAX),
           [](WidgetInfo& info) { DrawStrayFairiesInPoolRow(); });
    AddSectionEnd(path, "Dungeon Items");
    AddSection(path, "Pool Modifiers");
    AddItemPoolRow(path, "Plentiful Items");
    AddRow(path, "Traps", OptionCvar(RO_SHUFFLE_TRAPS), [](WidgetInfo& info) { DrawTrapsRow(); });
    AddRow(path, "Trap Amount", OptionCvar(RO_TRAP_AMOUNT), [](WidgetInfo& info) { DrawTrapAmountRow(); });
    AddRow(path, "Trap Behavior", "gRando.TrapItems", [](WidgetInfo& info) { DrawTrapBehaviorRow(); });
    AddRow(path, "Trap Types", nullptr, [](WidgetInfo& info) { DrawTrapTypesRow(); });
    AddSectionEnd(path, "Pool Modifiers");
    AddLayout(path, "Layout End: Item Pool", [](WidgetInfo& info) { UIWidgets::EndCardLayout(); });
}

static void RegisterStartingItemsRows(WidgetPath& path) {
    AddLayout(path, "Layout: Starting Options", [](WidgetInfo& info) { BeginStartingOptions(); });
    AddRow(path, "Wallet Full", OptionCvar(RO_STARTING_RUPEES), [](WidgetInfo& info) {
        DrawStartingCheckboxRow("Wallet Full", RO_STARTING_RUPEES, "Start with a full wallet");
    });
    AddRow(path, "Consumables Full", OptionCvar(RO_STARTING_CONSUMABLES), [](WidgetInfo& info) {
        DrawStartingCheckboxRow("Consumables Full", RO_STARTING_CONSUMABLES,
                                "Start with full Deku Sticks and Deku Nuts");
    });
    AddRow(path, "Maps and Compasses", OptionCvar(RO_STARTING_MAPS_AND_COMPASSES), [](WidgetInfo& info) {
        DrawStartingCheckboxRow("Maps and Compasses", RO_STARTING_MAPS_AND_COMPASSES,
                                "Enables maps and compasses everywhere");
    });
    AddRow(path, "Health", OptionCvar(RO_STARTING_HEALTH), [](WidgetInfo& info) { DrawStartingHealthRow(); });
    AddLayout(path, "Layout End: Starting Options", [](WidgetInfo& info) { EndStartingOptions(); });
    AddRow(path, "Starting Items", FLEET_HIDDEN_STARTING_ITEMS, [](WidgetInfo& info) { DrawStartingItemsList(); });
    AddRow(path, "Available Items", FLEET_HIDDEN_STARTING_ITEMS,
           [](WidgetInfo& info) { DrawAvailableStartingItems(); });
}

static const std::unordered_map<int32_t, const char*> enemyMinibossOptions = {
    { RO_ENEMY_MINIBOSSES_VANILLA, "Vanilla" },
    { RO_ENEMY_MINIBOSSES_AMONG_THEMSELVES, "Among Themselves" },
    { RO_ENEMY_MINIBOSSES_ANYWHERE, "Anywhere" },
};

static const std::unordered_map<int32_t, const char*> enemySoulBehaviourOptions = {
    { RO_ENEMY_SOUL_INVULNERABLE, "Invulnerable" },
    { RO_ENEMY_SOUL_NO_SPAWN, "No Spawn" },
};

static void DrawEnemyRandomizerRow() {
    if (IsDrawnByFleet(RO_ENEMY_RANDOMIZER)) {
        return;
    }
    CVarCheckbox("Enemy Randomizer", OptionCvar(RO_ENEMY_RANDOMIZER),
                 CheckboxOptions({ { .tooltip = "Each kind of enemy in a scene becomes another enemy from the pool, "
                                                "decided when the seed is generated. Logic knows the result." } }));
}

static void DrawEnemyMinibossesRow() {
    if (IsDrawnByFleet(RO_ENEMY_RANDOMIZER_MINIBOSSES) || !IsOptionOn(RO_ENEMY_RANDOMIZER)) {
        return;
    }
    UIWidgets::CVarCombobox("Minibosses", OptionCvar(RO_ENEMY_RANDOMIZER_MINIBOSSES), &enemyMinibossOptions,
                            UIWidgets::ComboboxOptions().Tooltip(
                                "Vanilla - Minibosses stay and never replace other enemies.\n\n"
                                "Among Themselves - Minibosses only replace and are replaced by minibosses.\n\n"
                                "Anywhere - Minibosses and regular enemies replace each other freely."));
}

static void DrawEnemySoulBehaviourRow() {
    if (IsDrawnByFleet(RO_ENEMY_SOUL_BEHAVIOUR) || !IsOptionOn(RO_SHUFFLE_ENEMY_SOULS)) {
        return;
    }
    UIWidgets::CVarCombobox("Soulless Enemies", OptionCvar(RO_ENEMY_SOUL_BEHAVIOUR), &enemySoulBehaviourOptions,
                            UIWidgets::ComboboxOptions().Tooltip(
                                "Invulnerable - Enemies without their soul cannot be hurt.\n\n"
                                "No Spawn - Enemies without their soul vanish as they appear. Rooms that wait for "
                                "their enemies still open, but logic still requires the soul to kill them."));
}

static bool ContainsIgnoringCase(const std::string& text, const char* fragment) {
    std::string_view needle(fragment);
    auto found = std::search(text.begin(), text.end(), needle.begin(), needle.end(),
                             [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
    return found != text.end();
}

static void DrawPoolBulkButtons(const char* cvar, const std::vector<const EnemyRandomizer::PoolEntry*>& shown) {
    if (Button("All", ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(THEME_COLOR))) {
        for (const EnemyRandomizer::PoolEntry* entry : shown) {
            EnemyRandomizer::SetKeyExcluded(cvar, entry->key, false);
        }
    }
    ImGui::SameLine();
    if (Button("None", ButtonOptions().Size(UIWidgets::Sizes::Inline).Color(THEME_COLOR))) {
        for (const EnemyRandomizer::PoolEntry* entry : shown) {
            EnemyRandomizer::SetKeyExcluded(cvar, entry->key, true);
        }
    }
}

static void DrawPoolList(const char* title, const char* cvar, const std::vector<EnemyRandomizer::PoolEntry>& entries) {
    if (FleetRando_IsOptionDrawnByFleet(cvar)) {
        return;
    }
    static std::map<std::string, std::array<char, 64>> filters;
    std::array<char, 64>& filter = filters[title];
    ImGui::PushID(title);
    ImGui::SeparatorText(title);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
    ImGui::InputTextWithHint("##search", "Search", filter.data(), filter.size());

    std::vector<const EnemyRandomizer::PoolEntry*> shown;
    for (const EnemyRandomizer::PoolEntry& entry : entries) {
        if (ContainsIgnoringCase(entry.name, filter.data())) {
            shown.push_back(&entry);
        }
    }
    ImGui::SameLine();
    DrawPoolBulkButtons(cvar, shown);

    if (ImGui::BeginTable("##pool", 2)) {
        for (const EnemyRandomizer::PoolEntry* entry : shown) {
            ImGui::TableNextColumn();
            bool included = !EnemyRandomizer::IsKeyExcluded(cvar, entry->key);
            std::string label = entry->name + "##" + entry->key;
            if (UIWidgets::Checkbox(label.c_str(), &included, CheckboxOptions().Color(THEME_COLOR))) {
                EnemyRandomizer::SetKeyExcluded(cvar, entry->key, !included);
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopID();
}

static void DrawEnemyPoolRow() {
    if (!IsOptionOn(RO_ENEMY_RANDOMIZER)) {
        return;
    }
    DrawPoolList("Enemy Pool", ENEMY_POOL_EXCLUDED_CVAR, EnemyRandomizer::ListEnemyPool(false));
}

static void DrawMinibossPoolRow() {
    const bool areMinibossesShuffled = CVarGetInteger(OptionCvar(RO_ENEMY_RANDOMIZER_MINIBOSSES),
                                                      RO_ENEMY_MINIBOSSES_VANILLA) != RO_ENEMY_MINIBOSSES_VANILLA;
    if (!IsOptionOn(RO_ENEMY_RANDOMIZER) || !areMinibossesShuffled) {
        return;
    }
    DrawPoolList("Miniboss Pool", ENEMY_POOL_EXCLUDED_CVAR, EnemyRandomizer::ListEnemyPool(true));
}

static void DrawSoulPoolRow() {
    if (!IsOptionOn(RO_SHUFFLE_ENEMY_SOULS)) {
        return;
    }
    DrawPoolList("Soul Pool", SOUL_POOL_EXCLUDED_CVAR, EnemyRandomizer::ListSoulPool());
}

static void RegisterEnemyRows(WidgetPath& path) {
    AddRow(path, "Enemy Randomizer", OptionCvar(RO_ENEMY_RANDOMIZER),
           [](WidgetInfo& info) { DrawEnemyRandomizerRow(); });
    AddRow(path, "Minibosses", OptionCvar(RO_ENEMY_RANDOMIZER_MINIBOSSES),
           [](WidgetInfo& info) { DrawEnemyMinibossesRow(); });
    AddRow(path, "Soulless Enemies", OptionCvar(RO_ENEMY_SOUL_BEHAVIOUR),
           [](WidgetInfo& info) { DrawEnemySoulBehaviourRow(); });
    AddRow(path, "Enemy Pool", ENEMY_POOL_EXCLUDED_CVAR, [](WidgetInfo& info) { DrawEnemyPoolRow(); });
    AddRow(path, "Miniboss Pool", ENEMY_POOL_EXCLUDED_CVAR, [](WidgetInfo& info) { DrawMinibossPoolRow(); });
    AddRow(path, "Soul Pool", SOUL_POOL_EXCLUDED_CVAR, [](WidgetInfo& info) { DrawSoulPoolRow(); });
}

static void RegisterHintRows(WidgetPath& path) {
    AddLayout(path, "Layout: Hints", [](WidgetInfo& info) { BeginHintsColumn(); });
    for (const HintRow& row : HINT_ROWS) {
        AddRow(path, row.label, OptionCvar(row.optionId), DrawHintRow);
        if (row.optionId == RO_HINTS_GOSSIP_STONES) {
            AddRow(path, "Gossip Stone Hint Strength", OptionCvar(RO_HINTS_GOSSIP_STONE_STRENGTH),
                   [](WidgetInfo& info) { DrawGossipStrengthRow(); });
        } else if (row.optionId == RO_HINTS_TRANSFORMATIONS) {
            AddRow(path, "General Actor Hints", nullptr, [](WidgetInfo& info) { DrawGeneralActorHintsRow(); });
        }
    }
    AddLayout(path, "Layout End: Hints", [](WidgetInfo& info) { ImGui::EndChild(); });
}

void Rando::RegisterMenu() {
    mBenMenu->AddMenuEntry("Rando", "gSettings.Menu.RandoSidebarSection");
    mBenMenu->AddSidebarEntry("Rando", "General", 1);
    WidgetPath path = { "Rando", "General", SECTION_COLUMN_1 };
    RegisterGeneralRows(path);
    mBenMenu->AddSidebarEntry("Rando", "Logic/Conditions", 1);
    path.sidebarName = "Logic/Conditions";
    RegisterLogicRows(path);

    mBenMenu->AddSidebarEntry("Rando", "Check Pool", 1);
    path.sidebarName = "Check Pool";
    RegisterCheckPoolRows(path);

    mBenMenu->AddSidebarEntry("Rando", "Check Exclusions", 1);
    path.sidebarName = "Check Exclusions";
    mBenMenu->AddWidget(path, "Check Exclusions", WIDGET_CUSTOM)
        .CustomFunction([](WidgetInfo& info) { DrawCheckExclusionsTab(); })
        .HideInSearch(true);

    mBenMenu->AddSidebarEntry("Rando", "Item Pool", 1);
    path.sidebarName = "Item Pool";
    RegisterItemPoolRows(path);

    mBenMenu->AddSidebarEntry("Rando", "Starting Items", 1);
    path.sidebarName = "Starting Items";
    RegisterStartingItemsRows(path);
    mBenMenu->AddSidebarEntry("Rando", "Hints", 1);
    path.sidebarName = "Hints";
    RegisterHintRows(path);

    mBenMenu->AddSidebarEntry("Rando", "Enemies", 1);
    path.sidebarName = "Enemies";
    RegisterEnemyRows(path);

    mBenMenu->AddSidebarEntry("Rando", "Item Tracker", 1);
    path.sidebarName = "Item Tracker";
    mBenMenu->AddWidget(path, "Popout Settings", WIDGET_WINDOW_BUTTON)
        .CVar("gWindows.ItemTrackerSettings")
        .WindowName("Item Tracker Settings");

    mBenMenu->AddSidebarEntry("Rando", "Check Tracker", 1);
    path.sidebarName = "Check Tracker";
    mBenMenu->AddWidget(path, "Popout Settings", WIDGET_WINDOW_BUTTON)
        .CVar("gWindows.CheckTrackerSettings")
        .WindowName("Check Tracker Settings");
}

static RegisterMenuInitFunc initFunc(Rando::RegisterMenu);
