#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/RandoOptions/RandoOptions.h"
#include "2s2h/ModApi/ModApi.h"
#include "2s2h/Enhancements/ModMenu/ModMenu.h"
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "FleetRando.h"
#include "FleetForeignItems.h"
#include "FleetItemDraw.h"
#include "FleetRuntime.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/BenGui/BenMenu.h"

namespace BenGui {
extern std::shared_ptr<BenMenu> mBenMenu;
}

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
#include <cctype>

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

#include "2s2h/BenGui/Notification.h"

#include "2s2h/CustomMessage/CustomMessage.h"

extern "C" {
#include "functions.h"
#include "message_data_fmt_nes.h"
#include "message_data_static.h"
#include "sfx.h"
#include "variables.h"
extern MessageTableEntry* sMessageTableNES;
void OTRMessage_Init();
}

extern std::unordered_map<int32_t, const char*> logicOptions;
extern std::unordered_map<int32_t, const char*> accessDungeonOptions;
extern std::unordered_map<int32_t, const char*> accessTrialsOptions;
extern std::unordered_map<int32_t, const char*> dungeonItemPlacementOptions;
extern std::unordered_map<int32_t, const char*> majoraSoulOptions;

namespace {

using Rando::StaticData::RandoStaticItem;

uint32_t sSeed = 0;

struct ComboOptions {
    bool hunt = false;
    uint32_t required = 0;
    int pieces = -1;
    int totalPieces = -1;
};
ComboOptions sComboOptions;

// The game's own save while the oracle borrows gSaveContext; restored by the "Finish" request.
std::unique_ptr<SaveContext> sLiveSave;
std::unique_ptr<SaveContext> sSeedSave;
std::vector<RandoItemId> sOracleModItems;

char* CopyString(const std::string& text) {
    char* copy = static_cast<char*>(std::malloc(text.size() + 1));
    std::memcpy(copy, text.c_str(), text.size() + 1);
    return copy;
}

const char* CheckName(RandoCheckId id) {
    return Rando::StaticData::GetCheckDisplayName(id).c_str();
}

const char* ItemName(RandoItemId id) {
    return Rando::StaticData::GetItemDisplayName(id).c_str();
}

bool IsNamed(const char* name) {
    return name != nullptr && name[0] != '\0';
}

std::string DisplayName(RandoItemId id, const RandoStaticItem& item) {
    const std::string name = CustomItemRegistry_GetDisplayName(RandoItems_GetKey(id));
    if (!name.empty()) {
        return name;
    }
    return IsNamed(item.name) ? item.name : ItemName(id);
}

bool IsAdvancement(const RandoStaticItem& item) {
    if (const char* key = RandoItems_GetKey(item.randoItemId); key != nullptr) {
        const auto* definition = CustomItemRegistry_Find(key);
        if (definition != nullptr && definition->randomizer != nullptr) {
            return (definition->randomizer->flags & S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT) != 0;
        }
    }
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

bool IsAbility(RandoItemId item) {
    switch (item) {
        case RI_ABILITY_SWIM:
        case RI_OCARINA_BUTTON_A:
        case RI_OCARINA_BUTTON_C_DOWN:
        case RI_OCARINA_BUTTON_C_RIGHT:
        case RI_OCARINA_BUTTON_C_LEFT:
        case RI_OCARINA_BUTTON_C_UP:
            return true;
        default:
            return false;
    }
}

bool IsDungeonMap(RandoItemId item) {
    switch (item) {
        case RI_WOODFALL_MAP:
        case RI_WOODFALL_COMPASS:
        case RI_SNOWHEAD_MAP:
        case RI_SNOWHEAD_COMPASS:
        case RI_GREAT_BAY_MAP:
        case RI_GREAT_BAY_COMPASS:
        case RI_STONE_TOWER_MAP:
        case RI_STONE_TOWER_COMPASS:
            return true;
        default:
            return false;
    }
}

const char* ReplicaKey(RandoItemId item) {
    static const std::unordered_map<RandoItemId, const char*> keys = {
        { RI_MASK_POSTMAN, "mm.mask.postman" },
        { RI_MASK_ALL_NIGHT, "mm.mask.all_night" },
        { RI_MASK_BLAST, "mm.mask.blast" },
        { RI_MASK_STONE, "mm.mask.stone" },
        { RI_MASK_GREAT_FAIRY, "mm.mask.great_fairy" },
        { RI_MASK_BREMEN, "mm.mask.bremen" },
        { RI_MASK_DON_GERO, "mm.mask.don_gero" },
        { RI_MASK_SCENTS, "mm.mask.scents" },
        { RI_MASK_ROMANI, "mm.mask.romani" },
        { RI_MASK_CIRCUS_LEADER, "mm.mask.circus_leader" },
        { RI_MASK_COUPLE, "mm.mask.couple" },
        { RI_MASK_KAMARO, "mm.mask.kamaro" },
        { RI_MASK_GIBDO, "mm.mask.gibdo" },
        { RI_MASK_CAPTAIN, "mm.mask.captain" },
        { RI_MASK_GIANT, "mm.mask.giant" },
        { RI_MASK_DEKU, "nei.deku_form_mask" },
        { RI_MASK_FIERCE_DEITY, "nei.fierce_deity_mask" },
        { RI_MASK_GORON, "nei.goron_form_mask" },
        { RI_MASK_ZORA, "nei.zora_form_mask" },
        { RI_MASK_KEATON, "nei.keaton_form_mask" },
        { RI_MASK_GARO, "nei.garo_form_mask" },
        { RI_MASK_KAFEIS_MASK, "nei.mask_kafei" },
        { RI_SONG_HEALING, "mm.song.healing" },
        { RI_SONG_SOARING, "mm.song.soaring" },
        { RI_SONG_SONATA, "mm.song.sonata" },
        { RI_PROGRESSIVE_LULLABY, "mm.song.goron_lullaby" },
        { RI_SONG_NOVA, "mm.song.new_wave" },
        { RI_SONG_ELEGY, "mm.song.elegy" },
        { RI_SONG_OATH, "mm.song.oath" },
        { RI_REMAINS_ODOLWA, "mm.remains.odolwa" },
        { RI_REMAINS_GOHT, "mm.remains.goht" },
        { RI_REMAINS_GYORG, "mm.remains.gyorg" },
        { RI_REMAINS_TWINMOLD, "mm.remains.twinmold" },
        { RI_PICTOGRAPH_BOX, "mm.picto_box" },
        { RI_POWDER_KEG, "mm.power_keg" },
        { RI_SHIELD_MIRROR, "mm.mirror_shield" },
        { RI_BOTTLE_EMPTY, "fleet.bottle.empty" },
        { RI_BOTTLE_MILK, "fleet.bottle.milk" },
        { RI_BOTTLE_RED_POTION, "fleet.bottle.red_potion" },
    };
    const auto found = keys.find(item);
    return found != keys.end() ? found->second : nullptr;
}

const char* PoolGroup(const RandoStaticItem& item) {
    if (item.randoItemId == RI_TRAP) {
        return "traps";
    }
    if (IsAbility(item.randoItemId)) {
        return "abilities";
    }
    if (IsDungeonMap(item.randoItemId)) {
        return "keys";
    }
    switch (item.randoItemType) {
        case RITYPE_BOSS_KEY:
        case RITYPE_SMALL_KEY:
        case RITYPE_SKULLTULA_TOKEN:
        case RITYPE_STRAY_FAIRY:
            return "keys";
        case RITYPE_HEALTH:
            return "health";
        case RITYPE_JUNK:
            return "consumables";
        case RITYPE_LESSER:
            return "useful";
        case RITYPE_MAJOR:
        case RITYPE_MASK:
            return "progression";
        default:
            SPDLOG_WARN("[Fleet] Item {} has no pool group", static_cast<int>(item.randoItemId));
            return "useful";
    }
}

nlohmann::json PoolEntry(RandoItemId item) {
    const RandoStaticItem& staticItem = Rando::StaticData::Items[item];
    return { { "name", ItemName(item) },
             { "advancement", IsAdvancement(staticItem) },
             { "category", CategoryName(staticItem) },
             { "group", PoolGroup(staticItem) } };
}

void AddPickupVariant(nlohmann::json& entry, RandoItemId item) {
    const char* key = RandoItems_GetKey(item);
    const S2HCustomPickupVariant* variant = key != nullptr ? CustomItemRegistry_GetPickupVariant(key) : nullptr;
    if (variant == nullptr || variant->itemKey == nullptr) {
        return;
    }
    const S2HCustomItemDefinition* owner = CustomItemRegistry_Find(variant->itemKey);
    entry["variantOf"] = variant->itemKey;
    entry["variantOfName"] = owner != nullptr && owner->displayName != nullptr ? owner->displayName : variant->itemKey;
}

bool IsNativeSong(RandoItemId item) {
    switch (item) {
        case RI_PROGRESSIVE_LULLABY:
        case RI_SONG_DOUBLE_TIME:
        case RI_SONG_ELEGY:
        case RI_SONG_EPONA:
        case RI_SONG_HEALING:
        case RI_SONG_INVERTED_TIME:
        case RI_SONG_LULLABY_INTRO:
        case RI_SONG_LULLABY:
        case RI_SONG_NOVA:
        case RI_SONG_OATH:
        case RI_SONG_SARIA:
        case RI_SONG_SOARING:
        case RI_SONG_SONATA:
        case RI_SONG_STORMS:
        case RI_SONG_SUN:
        case RI_SONG_TIME:
            return true;
        default:
            return false;
    }
}

bool IsSong(RandoItemId item) {
    const char* key = RandoItems_GetKey(item);
    return key != nullptr ? std::strstr(key, ".song.") != nullptr : IsNativeSong(item);
}

const char* CheckType(const Rando::StaticData::RandoStaticCheck& check) {
    switch (check.randoCheckType) {
        case RCTYPE_SONG:
            return "song";
        case RCTYPE_REMAINS:
            return "dungeonReward";
        default:
            return "";
    }
}

nlohmann::json CheckEntry(RandoCheckId check) {
    nlohmann::json entry = { { "name", CheckName(check) }, { "type", "" }, { "area", "" }, { "vanilla", "" } };
    const auto staticCheck = Rando::StaticData::Checks.find(check);
    if (staticCheck == Rando::StaticData::Checks.end()) {
        return entry;
    }
    entry["type"] = CheckType(staticCheck->second);
    entry["area"] = Ship_GetSceneName(staticCheck->second.sceneId);
    entry["vanilla"] = ItemName(staticCheck->second.randoItemId);
    return entry;
}

std::string ReadableText(const std::string& bytes) {
    static const char* const kButtons[] = { "A",    "B",      "C",      "L",       "R",        "Z",
                                            "C-Up", "C-Down", "C-Left", "C-Right", "Z-Target", "Control Stick" };
    std::string text;
    for (size_t i = 0; i < bytes.size(); i++) {
        const uint8_t byte = static_cast<uint8_t>(bytes[i]);
        if (byte == MESSAGE_END) {
            break;
        }
        if (byte == MESSAGE_NEWLINE || byte == MESSAGE_BOX_BREAK || byte == MESSAGE_BOX_BREAK2) {
            text += ' ';
        } else if (byte == MESSAGE_SHIFT) {
            i += 1;
        } else if (byte >= MESSAGE_BOX_BREAK_DELAYED && byte <= MESSAGE_DELAY) {
            i += 2;
        } else if (byte >= MESSAGE_BTN_A && byte <= MESSAGE_CONTROL_PAD) {
            text += kButtons[byte - MESSAGE_BTN_A];
        } else if (byte >= ' ' && byte < 0x7F) {
            text += static_cast<char>(byte);
        }
    }
    std::string collapsed;
    for (char character : text) {
        if (character != ' ' || (!collapsed.empty() && collapsed.back() != ' ')) {
            collapsed += character;
        }
    }
    while (!collapsed.empty() && collapsed.back() == ' ') {
        collapsed.pop_back();
    }
    return collapsed;
}

std::string NativeDescription(const RandoStaticItem& item) {
    if (item.itemId == ITEM_NONE || sMessageTableNES == nullptr) {
        return "";
    }
    const u16 textId = 0x1700 + item.itemId;
    const MessageTableEntry* entry = OTRMessage_Find(sMessageTableNES, textId);
    if (entry == nullptr || entry->textId != textId || entry->msgSize <= MESSAGE_HEADER_SIZE) {
        return "";
    }
    std::string bytes(entry->segment + MESSAGE_HEADER_SIZE, entry->msgSize - MESSAGE_HEADER_SIZE);
    const bool startsWithName = static_cast<uint8_t>(bytes.front()) > MESSAGE_COLOR_DEFAULT &&
                                static_cast<uint8_t>(bytes.front()) <= MESSAGE_COLOR_ORANGE;
    const size_t nameEnd = bytes.find(static_cast<char>(MESSAGE_NEWLINE));
    if (startsWithName && nameEnd != std::string::npos) {
        bytes.erase(0, nameEnd + 1);
    }
    return ReadableText(bytes);
}

std::string Description(RandoItemId item, const RandoStaticItem& staticItem) {
    const char* key = RandoItems_GetKey(item);
    if (key == nullptr) {
        return NativeDescription(staticItem);
    }
    const auto* definition = CustomItemRegistry_Find(key);
    if (definition == nullptr || definition->pauseText == nullptr) {
        return "";
    }
    std::string text = CustomMessage::RemoveColorCodes(definition->pauseText);
    std::replace(text.begin(), text.end(), '&', ' ');
    return ReadableText(text);
}

uint8_t MaxStartingCount(RandoItemId item) {
    const auto found = Rando::StaticData::MaxStartingItemsMap.find(item);
    return found != Rando::StaticData::MaxStartingItemsMap.end() ? found->second : 1;
}

nlohmann::json StartingItems() {
    const std::vector<RandoItemId> chosen = Rando::GetStartingItemsFromConfig();
    nlohmann::json items = nlohmann::json::array();
    for (const auto& [category, categoryItems] : Rando::StaticData::StartingItemsMap) {
        for (RandoItemId item : categoryItems) {
            items.push_back({ { "name", ItemName(item) },
                              { "group", PoolGroup(Rando::StaticData::Items[item]) },
                              { "max", MaxStartingCount(item) },
                              { "count", std::count(chosen.begin(), chosen.end(), item) } });
        }
    }
    return items;
}

void SetStartingItem(const nlohmann::json& input) {
    if (!input.is_object()) {
        return;
    }
    const std::string name = input.value("name", std::string{});
    const auto found = std::find_if(Rando::StaticData::Items.begin(), Rando::StaticData::Items.end(),
                                    [&](const auto& entry) { return name == ItemName(entry.first); });
    if (found == Rando::StaticData::Items.end()) {
        SPDLOG_WARN("[Fleet] 2Ship cannot start with unknown item '{}'", name);
        return;
    }
    const RandoItemId item = found->first;
    const int count = std::clamp(input.value("count", 0), 0, static_cast<int>(MaxStartingCount(item)));
    std::vector<RandoItemId> chosen = Rando::GetStartingItemsFromConfig();
    std::erase(chosen, item);
    chosen.insert(chosen.end(), count, item);
    Rando::SetStartingItemsInConfig(chosen);
}

nlohmann::json FixedEntry(RandoCheckId check, RandoItemId item, bool hintable) {
    nlohmann::json entry = PoolEntry(item);
    entry.erase("name");
    entry["check"] = CheckName(check);
    entry["item"] = ItemName(item);
    entry["hintable"] = hintable;
    return entry;
}

void ApplyComboOptions() {
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONG_SUN] = RO_GENERIC_YES;
    const bool arePiecesShuffled = sComboOptions.hunt || sComboOptions.totalPieces > 0;
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_TRIFORCE_PIECES] = arePiecesShuffled ? RO_GENERIC_YES : RO_GENERIC_NO;
    RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_REQUIRED] = sComboOptions.hunt ? sComboOptions.required : 0;
    if (sComboOptions.pieces < 0) {
        return;
    }
    RANDO_SAVE_OPTIONS[RO_TRIFORCE_PIECES_MAX] = static_cast<uint32_t>(sComboOptions.pieces);
    const uint32_t countable =
        static_cast<uint32_t>(std::max(sComboOptions.hunt ? sComboOptions.pieces : sComboOptions.totalPieces, 0));
    for (RandoOptionId option : { RO_ACCESS_MOON_TRIFORCE_COUNT, RO_MAJORA_SOUL_TRIFORCE_COUNT }) {
        RANDO_SAVE_OPTIONS[option] = std::min(RANDO_SAVE_OPTIONS[option], countable);
    }
}

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
    while (itemPool.size() < checkPool.size()) {
        itemPool.push_back(RI_JUNK);
    }
    sSeedSave = std::make_unique<SaveContext>(gSaveContext);
}

const char* const kExcludedChecksKey = "gRando.ExcludedChecks";
const char* const kStartingItemsKey = "gRando.StartingItems";
const char* const kFleetStartingItemsKey = "fleet.startingItems";
std::vector<std::string> sFleetStartingItems;
std::optional<nlohmann::json> sPlayerSettings;

void GiveOracleItem(const std::string& name) {
    const RandoItemId item = Rando::StaticData::GetItemIdFromDisplayName(name);
    if (RandoItems_IsModItem(item)) {
        Rando::GiveKeyRingKeys(item);
        sOracleModItems.push_back(item);
    } else if (item != RI_UNKNOWN && item != RI_TRAP) {
        Rando::GiveItem(Rando::ConvertItem(item));
    }
}

void RemoveFleetStartingItems(nlohmann::json& pool) {
    for (const std::string& name : sFleetStartingItems) {
        const auto found = std::find_if(pool.begin(), pool.end(),
                                        [&](const nlohmann::json& entry) { return entry.value("name", "") == name; });
        if (found != pool.end()) {
            pool.erase(found);
        }
    }
}

nlohmann::json DumpSettings() {
    nlohmann::json settings = nlohmann::json::object();
    for (const auto& [id, option] : Rando::StaticData::Options) {
        if (IsNamed(option.cvar)) {
            settings[option.cvar] = CVarGetInteger(option.cvar, option.defaultValue);
        }
    }
    for (const std::string& cvar : RandoOptions_GetCvars()) {
        settings[cvar] = CVarGetInteger(cvar.c_str(), 0);
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
    settings[kFleetStartingItemsKey] = sFleetStartingItems;
    return { { "settings", std::move(settings) } };
}

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
    sFleetStartingItems.clear();
    for (const nlohmann::json& name : settings.value(kFleetStartingItemsKey, nlohmann::json::array())) {
        if (name.is_string()) {
            sFleetStartingItems.push_back(name.get<std::string>());
        }
    }
    for (const auto& [cvar, value] : settings.items()) {
        if (value.is_number_integer()) {
            CVarSetInteger(cvar.c_str(), value.get<int32_t>());
        }
    }
}

void FinishRequest() {
    if (sLiveSave != nullptr) {
        gSaveContext = *sLiveSave;
        sLiveSave.reset();
    }
    sSeedSave.reset();
    if (sPlayerSettings.has_value()) {
        RestoreSettings(*sPlayerSettings);
        sPlayerSettings.reset();
    }
    Rando::OverrideExcludedChecks(std::nullopt);
    Rando::OverrideStartingItems(std::nullopt);
    sSeed = 0;
    sComboOptions = {};
    sOracleModItems.clear();
    sFleetStartingItems.clear();
    FleetCrossDungeon_Reset();
}

void InitRandoHeadless() {
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;
    // Item effects such as the Tingle maps write debug registers, which only a running game allocates.
    static RegEditor sHeadlessRegisters = {};
    if (gRegEditor == nullptr) {
        gRegEditor = &sHeadlessRegisters;
    }
    FleetGame_JoinHostContext();
    auto resources = FleetGame_CreateResources(Ship::Context::LocateFileAcrossAppDirs("2ship.o2r"));
    for (const char* file : { "mm.o2r", "mm-unbound.o2r" }) {
        const auto archive = Ship::Context::LocateFileAcrossAppDirs(file);
        if (!archive.empty()) {
            resources->GetArchiveManager()->AddArchive(archive);
        }
    }
    GameInteractor::Instance = new GameInteractor();
    ModMenu_LoadArchives();
    OTRMessage_Init();
    ShipInit::Init(Rando::Logic::RegionGraphPath);
    ModApi_Init();
    Rando::StaticData::PopulateCheckNames();
}

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
    if (request == "GetCrossBossDefeated") {
        return nlohmann::json{ { "defeated", FleetCrossDungeon_GetBossDefeated() } };
    }
    if (request == "CanEnterCrossEntrance") {
        return nlohmann::json{ { "allowed", FleetCrossDungeon_CanEnterArrival(input.value("entrance", -1)) } };
    }
    return std::nullopt;
}

std::set<std::string> sOptionsDrawnByFleet;

} // namespace

bool FleetRando_IsOptionDrawnByFleet(const char* cvar) {
    return sOptionsDrawnByFleet.contains(cvar);
}

char* FleetRando_DumpStaticData(void) {
    std::vector<RandoCheckId> checkPool;
    std::vector<RandoItemId> itemPool;
    std::vector<RandoCheckId> confinedChecks;
    PrepareSeed(checkPool, itemPool, confinedChecks);

    nlohmann::json checks = nlohmann::json::array();
    for (RandoCheckId check : checkPool) {
        if (!IsNamed(CheckName(check))) {
            continue;
        }
        checks.push_back(CheckEntry(check));
    }

    nlohmann::json pool = nlohmann::json::array();
    for (RandoItemId item : itemPool) {
        if (IsNamed(ItemName(item))) {
            pool.push_back(PoolEntry(item));
        }
    }
    RemoveFleetStartingItems(pool);

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
        const bool vanillaRemains =
            check.randoCheckType == RCTYPE_REMAINS && RANDO_SAVE_OPTIONS[RO_SHUFFLE_BOSS_REMAINS] == RO_GENERIC_NO;
        const bool vanillaCollection = !RANDO_SAVE_CHECKS[id].shuffled && (check.randoCheckType == RCTYPE_SKULL_TOKEN ||
                                                                           check.randoCheckType == RCTYPE_FROG);
        if ((vanillaRemains || vanillaCollection) && IsNamed(ItemName(check.randoItemId))) {
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
        entry["displayName"] = DisplayName(id, item);
        if (IsSong(id)) {
            entry["song"] = true;
        }
        AddPickupVariant(entry, id);
        if (const std::string description = Description(id, item); !description.empty()) {
            entry["description"] = description;
        }
        if (const char* replica = ReplicaKey(id); replica != nullptr) {
            entry["pairKey"] = replica;
        }
        if (const char* key = RandoItems_GetKey(id); key != nullptr) {
            entry["textKey"] = key;
            entry["group"] = PoolGroup(item);
        }
        items.push_back(std::move(entry));
    }

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

    const nlohmann::json dump = { { "checks", std::move(checks) }, { "pool", std::move(pool) },
                                  { "fixed", std::move(fixed) },   { "items", std::move(items) },
                                  { "prices", std::move(prices) }, { "locationHints", std::move(locationHints) },
                                  { "options", options },          { "junkPool", std::move(junkPool) } };
    return CopyString(dump.dump());
}

void FleetRando_Reset(void) {
    // Assumes DumpStaticData prepared the seed first.
    assert(sSeedSave != nullptr);
    gSaveContext = *sSeedSave;
    sOracleModItems.clear();
    for (const std::string& name : sFleetStartingItems) {
        GiveOracleItem(name);
    }
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
        if (name.is_string()) {
            GiveOracleItem(name.get<std::string>());
        }
    }
}

char* FleetRando_GetReachableChecks(void) {
    RandoSeedGenerationScope modApiScope;
    for (const RandoItemId item : sOracleModItems) {
        RandoItems_Give(item);
    }
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
    if (request == "RuntimeStatus") {
        return CopyString(FleetRuntime_Status().dump());
    }
    if (request == "RuntimeCycleSmoke") {
        return CopyString(FleetRuntime_CycleSmoke().dump());
    }
    if (request == "CatchUpParked") {
        FleetRuntime_CatchUpParked();
        return CopyString("{}");
    }
    if (request == "DumpSettingDefinitions") {
        auto definitions = nlohmann::json::array();
        for (const auto& [id, option] : Rando::StaticData::Options) {
            if (!IsNamed(option.cvar) || id == RO_SHUFFLE_TRIFORCE_PIECES || id == RO_TRIFORCE_PIECES_MAX ||
                id == RO_TRIFORCE_PIECES_REQUIRED) {
                continue;
            }
            std::string name = option.name + 3;
            bool upper = true;
            for (char& character : name) {
                if (character == '_') {
                    character = ' ';
                    upper = true;
                } else {
                    character = static_cast<char>(upper ? std::toupper(static_cast<unsigned char>(character))
                                                        : std::tolower(static_cast<unsigned char>(character)));
                    upper = false;
                }
            }
            nlohmann::json entry = { { "key", option.cvar }, { "name", name }, { "min", 0 }, { "max", 1 } };
            const std::unordered_map<int32_t, const char*>* choices = nullptr;
            switch (id) {
                case RO_LOGIC:
                    choices = &logicOptions;
                    break;
                case RO_ACCESS_DUNGEONS:
                    choices = &accessDungeonOptions;
                    break;
                case RO_ACCESS_TRIALS:
                    choices = &accessTrialsOptions;
                    break;
                case RO_PLACEMENT_BOSS_KEYS:
                case RO_PLACEMENT_SMALL_KEYS:
                case RO_PLACEMENT_STRAY_FAIRIES:
                    choices = &dungeonItemPlacementOptions;
                    break;
                case RO_CLOCK_SHUFFLE_PROGRESSIVE:
                    entry["choices"] = nlohmann::json::array(
                        { { { "value", RO_CLOCK_SHUFFLE_RANDOM }, { "name", "Random" } },
                          { { "value", RO_CLOCK_SHUFFLE_ASCENDING }, { "name", "Ascending" } },
                          { { "value", RO_CLOCK_SHUFFLE_DESCENDING }, { "name", "Descending" } } });
                    break;
                case RO_ACCESS_MAJORA_MASKS_COUNT:
                case RO_ACCESS_MOON_MASKS_COUNT:
                    entry["max"] = 20;
                    break;
                case RO_ACCESS_MAJORA_REMAINS_COUNT:
                case RO_ACCESS_MOON_REMAINS_COUNT:
                    entry["max"] = 4;
                    break;
                case RO_ACCESS_MOON_TRIFORCE_COUNT:
                case RO_MAJORA_SOUL_TRIFORCE_COUNT:
                    entry["max"] = 100;
                    break;
                case RO_SHUFFLE_MAJORA_SOUL:
                    choices = &majoraSoulOptions;
                    break;
                case RO_MAJORA_SOUL_STONE_COUNT:
                    entry["max"] = 3;
                    break;
                case RO_MAJORA_SOUL_MEDALLION_COUNT:
                    entry["max"] = 6;
                    break;
                case RO_MAJORA_SOUL_REWARD_COUNT:
                    entry["max"] = 13;
                    break;
                case RO_MAJORA_SOUL_DUNGEON_COUNT:
                    entry["max"] = 4;
                    break;
                case RO_MAJORA_SOUL_TOKEN_COUNT:
                    entry["max"] = 60;
                    break;
                case RO_STARTING_HEALTH:
                    entry["min"] = 1;
                    entry["max"] = 20;
                    break;
                case RO_SKULLTULA_SHUFFLED:
                case RO_SKULLTULA_TOKENS_REQUIRED:
                    entry["min"] = 1;
                    entry["max"] = 30;
                    break;
                case RO_STRAY_FAIRIES_MAX:
                case RO_STRAY_FAIRIES_REQUIRED:
                    entry["min"] = 1;
                    entry["max"] = 15;
                    break;
                case RO_TRAP_AMOUNT:
                    entry["min"] = 1;
                    entry["max"] = 100;
                    break;
                case RO_HINTS_GOSSIP_STONE_STRENGTH:
                    entry["max"] = 100;
                    break;
                case RO_CLOCK_TERMINAL_TIME:
                    entry["max"] = 359;
                    break;
                default:
                    break;
            }
            if (choices != nullptr) {
                entry["choices"] = nlohmann::json::array();
                for (const auto& [value, label] : std::map<int32_t, const char*>(choices->begin(), choices->end())) {
                    entry["choices"].push_back({ { "value", value }, { "name", label } });
                }
            }
            definitions.push_back(std::move(entry));
        }
        return CopyString(definitions.dump());
    }
    if (request == "RuntimeCollectCheck") {
        const auto input = nlohmann::json::parse(json != nullptr ? json : "{}", nullptr, false);
        return CopyString(nlohmann::json{
            { "collected", input.is_object() && FleetRuntime_CollectForSmoke(input.value("check", "")) } }
                              .dump());
    }
    const nlohmann::json input = nlohmann::json::parse(json != nullptr ? json : "{}", nullptr, false);
    if (request == "MenuModel") {
        return CopyString(BenGui::mBenMenu ? BenGui::mBenMenu->FleetMenuModel() : "null");
    }
    if (request == "ApplySetting") {
        ShipInit::Init(input.at("key").get<std::string>());
        return CopyString("{}");
    }
    if (request == "HideRandoOptions") {
        sOptionsDrawnByFleet = input.is_array() ? input.get<std::set<std::string>>() : std::set<std::string>{};
        return CopyString("{}");
    }
    if (request == "MenuDrawWidget") {
        if (BenGui::mBenMenu && input.is_object()) {
            BenGui::mBenMenu->FleetDrawWidget(input.at("section"), input.at("sidebar"), input.at("column"),
                                              input.at("index"));
        }
        return CopyString("{}");
    }
    if (request == "Notify") {
        Notification::Emit({ .message = input.value("message", std::string{}) });
        Audio_PlaySfx(NA_SE_SY_QUIZ_CORRECT);
        return CopyString("{}");
    }
    if (request == "ListItems") {
        nlohmann::json items = nlohmann::json::array();
        for (const auto& [id, item] : Rando::StaticData::Items) {
            const bool isPlaceholder =
                id == RI_UNKNOWN || id == RI_JUNK || id == RI_NONE || id == RI_TRIFORCE_PIECE_PREVIOUS;
            const char* key = RandoItems_GetKey(id);
            if (!isPlaceholder && IsNamed(ItemName(id)) && (key == nullptr || std::strncmp(key, "fleet:", 6) != 0)) {
                items.push_back({ { "name", ItemName(id) }, { "label", DisplayName(id, item) } });
            }
        }
        return CopyString(items.dump());
    }
    if (request == "MenuDrawPage") {
        if (BenGui::mBenMenu && input.is_object()) {
            BenGui::mBenMenu->FleetDrawPage(input.at("section"), input.at("sidebar"));
        }
        return CopyString("{}");
    }
    if (request == "InitRandoHeadless") {
        InitRandoHeadless();
        return CopyString("{}");
    }
    if (request == "SetSeed") {
        sSeed = input.value("seed", 0u);
        return CopyString("{}");
    }
    if (request == "SetComboOptions") {
        const int pieces = std::clamp(input.value("pieces", -1), -1, 100);
        sComboOptions = { .hunt = input.value("hunt", false),
                          .required = static_cast<uint32_t>(std::clamp(input.value("required", 0), 0, 100)),
                          .pieces = pieces,
                          .totalPieces = std::clamp(input.value("totalPieces", pieces), -1, 100) };
        return CopyString("{}");
    }
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
    if (request == "StartingItems") {
        return CopyString(StartingItems().dump());
    }
    if (request == "SetStartingItem") {
        SetStartingItem(input);
        return CopyString("{}");
    }
    if (request == "RestoreSettings") {
        if (!sPlayerSettings.has_value()) {
            sPlayerSettings = DumpSettings();
        }
        RestoreSettings(input.is_object() ? input : nlohmann::json::object());
        return CopyString("{}");
    }
    if (request == "ItemTextboxIcon") {
        const auto item = input.find("item");
        const bool isNamed = item != input.end() && item->is_string();
        const char* path =
            isNamed ? FleetItemDraw_TextboxIconPath(item->get_ref<const std::string&>().c_str()) : nullptr;
        return CopyString(nlohmann::json{ { "path", path != nullptr ? path : "" } }.dump());
    }
    if (request == "Finish") {
        FinishRequest();
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

void FleetRando_ApplyCombinedSave(const nlohmann::json& seed) {
    RandoSeedGenerationScope modApiScope;
    struct RestorePlayerSettings {
        nlohmann::json snapshot = DumpSettings();
        ~RestorePlayerSettings() {
            RestoreSettings(snapshot);
            Rando::OverrideExcludedChecks(std::nullopt);
            Rando::OverrideStartingItems(std::nullopt);
        }
    } restorePlayerSettings;
    RestoreSettings(seed.at("mm"));
    sSeed = seed.at("masterSeed").get<uint32_t>();
    const auto goal = seed.at("goal");
    sComboOptions.hunt = goal.value("type", "bosses") == "triforceHunt";
    sComboOptions.required = goal.value("requiredPieces", 0u);
    const int totalPieces = sComboOptions.hunt ? goal.value("totalPieces", 0) : goal.value("pieces", 0);
    sComboOptions.pieces = totalPieces / 2;
    sComboOptions.totalPieces = totalPieces;
    Rando::MiscBehavior::PersistSeedOptions(sSeed);
    ApplyComboOptions();
    Ship_Random_Seed(sSeed);
    std::vector<RandoCheckId> checks;
    std::vector<RandoItemId> items;
    Rando::Logic::GeneratePools(gSaveContext.save.shipSaveInfo.rando, checks, items);
    Rando::GrantStartingItems();
    Rando::Logic::PreplaceConfinedItems(checks, items);
    std::unordered_map<std::string, RandoCheckId> checkIds;
    std::unordered_map<std::string, RandoItemId> itemIds;
    for (const auto& [id, check] : Rando::StaticData::Checks) {
        checkIds[CheckName(id)] = id;
    }
    for (const auto& [id, item] : Rando::StaticData::Items) {
        itemIds[ItemName(id)] = id;
        if (item.spoilerName != nullptr) {
            itemIds[item.spoilerName] = id;
        }
    }
    FleetForeignItems_Register(seed);
    nlohmann::json placements = seed.at("mm").at("placements");
    for (const auto& foreign : seed.at("foreign")) {
        if (foreign.at("checkGame") == "mm") {
            const std::string key = FleetForeignItems_Key(foreign);
            placements[foreign.at("checkName").get<std::string>()] = key;
            itemIds[key] = static_cast<RandoItemId>(RandoItems_GetRandoGet(key.c_str()));
        }
    }
    for (const auto& [check, placed] : placements.items()) {
        std::string item = placed.get<std::string>();
        for (const char* suffix : { " (OOT)", " (MM)" }) {
            const size_t size = std::strlen(suffix);
            if (item.size() >= size && item.compare(item.size() - size, size, suffix) == 0) {
                item.resize(item.size() - size);
            }
        }
        if (!checkIds.contains(check) || !itemIds.contains(item)) {
            throw std::runtime_error("unknown MM placement: " + check + " -> " + item);
        }
        auto& saveCheck = RANDO_SAVE_CHECKS[checkIds.at(check)];
        saveCheck.randoItemId = itemIds.at(item);
        saveCheck.shuffled =
            saveCheck.shuffled || saveCheck.randoItemId != Rando::StaticData::Checks.at(checkIds.at(check)).randoItemId;
    }
    const auto prices = seed.at("mm").value("prices", nlohmann::json::object());
    for (const auto& [check, price] : prices.items()) {
        RANDO_SAVE_CHECKS[checkIds.at(check)].price = price.get<uint16_t>();
    }
    RANDO_SAVE_CHECKS[RC_STARTING_ITEM_DEKU_MASK].eligible = true;
    RANDO_SAVE_CHECKS[RC_STARTING_ITEM_SONG_OF_HEALING].eligible = true;
}

void FleetRando_FreeString(char* text) {
    std::free(text);
}
