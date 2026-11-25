#include "FleetRuntime.h"
#include "FleetRuntimeBridge.h"
#include "FleetGame.h"
#include "FleetForeignItems.h"

#include <filesystem>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <fleet/rando/CrossDungeonRuntime.h>
#include <spdlog/spdlog.h>

#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/RandoItems/RandoItems.h"
#include "2s2h/ModApi/ProgressiveItems/ProgressiveItems.h"
#include "2s2h/ModApi/RandoInfCustom/RandoInfCustom.h"
#include "2s2h/ModApi/RandoOptions/RandoOptions.h"
#include "2s2h/ModApi/ModApi.h"
#include "2s2h/SaveManager/SaveManager.h"
#include "2s2h/SaveManager/SaveSections.h"
#include "2s2h/BenJsonConversions.hpp"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/BenGui/Notification.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

#include "CrossEntranceMmGrottos.h"

namespace {

using namespace ComboRando::CrossDungeon;
constexpr const char* kGame = "mm";
nlohmann::json sSession;
std::set<std::string> sReported;
RuntimeState sRoutes(1);
std::optional<RuntimeState::Transition> sDeparture;
bool sTransitionComplete = false;
int32_t sCheckpoint = -1;
bool sSkipTransition = false;

// What this game last exchanged with the host, so an unchanged state is not sent or applied again.
struct ExchangedState {
    std::string reportedBottles;
    std::string appliedBottles;
    std::string reportedLive;
    std::string appliedLive;
    std::string markedTiers;
};
ExchangedState sExchanged;

bool IsKnownCheckpoint(int32_t entrance) {
    if (sRoutes.IsRecoveryEntrance(entrance)) {
        return true;
    }
    const auto cross = sSession.at("seed").value("crossDungeonEntrances", nlohmann::json{});
    if (!cross.is_object()) {
        return false;
    }
    const auto dungeons = cross.value("dungeons", nlohmann::json::array());
    for (const auto& entry : dungeons) {
        const auto dungeon = ParseDungeon(entry);
        if (dungeon.returnTo == Endpoint{ 1, entrance }) {
            return true;
        }
    }
    return false;
}

template <class Function> Function Host(const char* name) {
    return reinterpret_cast<Function>(FleetGame_GetHostFunction(name));
}

bool IsPlaying() {
    return gPlayState != nullptr && gGameState == &gPlayState->state && gGameState->running &&
           gSaveContext.gameMode == GAMEMODE_NORMAL && IS_RANDO && gSaveContext.fileNum == FleetRuntime_Slot();
}

using FleetGoalReachedFn = int (*)(const char* game);

void DecideEndingCredits(bool* should) {
    const auto goalReached = Host<FleetGoalReachedFn>("GoalReached");
    if (goalReached == nullptr || !IsPlaying() || goalReached(kGame) != 0) {
        return;
    }
    *should = false;
    Notification::Emit({ .message = "Majora's Mask cleared! Finish Ocarina of Time to win." });
}

void RegisterEndingHook() {
    REGISTER_VB_SHOULD(VB_PLAY_ENDING_CREDITS, { DecideEndingCredits(should); });
}

static RegisterShipInitFunc sEndingInit(RegisterEndingHook, {});

bool IsTextboxOpen() {
    const uint8_t mode = gPlayState->msgCtx.msgMode;
    return mode != MSGMODE_NONE &&
           (mode < MSGMODE_SCENE_TITLE_CARD_FADE_IN_BACKGROUND || mode > MSGMODE_SCENE_TITLE_CARD_FADE_OUT_BACKGROUND);
}

RandoItemId FindItemNamed(const std::string& item) {
    static std::unordered_map<std::string, RandoItemId> sFound;
    if (const auto known = sFound.find(item); known != sFound.end()) {
        return known->second;
    }
    for (const auto& [id, definition] : Rando::StaticData::Items) {
        if (Rando::StaticData::GetItemDisplayName(id) == item ||
            (definition.spoilerName != nullptr && item == definition.spoilerName)) {
            sFound[item] = id;
            return id;
        }
    }
    return RI_UNKNOWN;
}

std::string DeliveredItem(const nlohmann::json& delivery) {
    return delivery.value("trap", false) ? "Knockoff Item" : delivery.at("item").get<std::string>();
}

// A delivery for an item this game does not know (its mod was removed after the seed was made) waits instead of
// stopping the game, and arrives once the mod is back.
bool IsUnknownDelivery(const nlohmann::json& delivery) {
    static std::set<std::string> sReported;
    const std::string item = DeliveredItem(delivery);
    if (FindItemNamed(item) != RI_UNKNOWN) {
        return false;
    }
    if (sReported.insert(item).second) {
        SPDLOG_WARN("[Fleet] {} is not an item of this game; its delivery waits", item);
    }
    return true;
}

void Grant(const nlohmann::json& delivery) {
    const std::string item = DeliveredItem(delivery);
    RandoItemId itemId = FindItemNamed(item);
    if (itemId == RI_UNKNOWN) {
        throw std::runtime_error("unknown MM delivery: " + item);
    }
    itemId = Rando::ConvertItem(itemId);
    if (itemId == RI_JUNK) {
        uint32_t hash = FleetRuntime_Seed().at("masterSeed").get<uint32_t>();
        for (unsigned char character : delivery.value("id", item)) {
            hash = (hash ^ character) * 16777619u;
        }
        constexpr RandoItemId junk[] = { RI_RUPEE_GREEN, RI_RUPEE_BLUE, RI_RUPEE_RED };
        itemId = junk[hash % 3];
    }
    Rando::GiveItem(itemId);
}

struct SharedTierGrant {
    RandoItemId item = RI_UNKNOWN;
    const char* key = nullptr;
    s16 upgrade = -1;
    u8 upgradeValue = 0;
};

constexpr const char* kInfiniteOption = "oot.infinite_upgrades";

bool IsModOptionOn(const char* key) {
    return RandoOptions_Get(key) != 0;
}

std::vector<SharedTierGrant> SharedTierGrants(const std::string& key) {
    std::vector<SharedTierGrant> levels;
    const char* infinite = nullptr;
    if (key == "wallets") {
        levels = { { .key = "nei.bag.child_wallet" }, { .item = RI_WALLET_ADULT }, { .item = RI_WALLET_GIANT } };
        if (RANDO_SAVE_OPTIONS[RO_SHUFFLE_TYCOON_WALLET] == RO_GENERIC_YES) {
            levels.push_back({ .item = RI_WALLET_TYCOON });
        }
        infinite = "nei.infinite.wallet";
    } else if (key == "bows") {
        levels = { { .item = RI_BOW }, { .item = RI_QUIVER_40 }, { .item = RI_QUIVER_50 } };
        infinite = "nei.infinite.quiver";
    } else if (key == "bombBags") {
        levels = { { .item = RI_BOMB_BAG_20 }, { .item = RI_BOMB_BAG_30 }, { .item = RI_BOMB_BAG_40 } };
        infinite = "nei.infinite.bomb_bag";
    } else if (key == "magic") {
        levels = { { .item = RI_SINGLE_MAGIC }, { .item = RI_DOUBLE_MAGIC } };
        infinite = "nei.infinite.magic";
    } else if (key == "dekuStickBag") {
        levels = { { .key = "nei.bag.deku_sticks" },
                   { .upgrade = UPG_DEKU_STICKS, .upgradeValue = 2 },
                   { .upgrade = UPG_DEKU_STICKS, .upgradeValue = 3 } };
        infinite = "nei.infinite.deku_sticks";
    } else if (key == "dekuNutBag") {
        levels = { { .key = "nei.bag.deku_nuts" },
                   { .upgrade = UPG_DEKU_NUTS, .upgradeValue = 2 },
                   { .upgrade = UPG_DEKU_NUTS, .upgradeValue = 3 } };
        infinite = "nei.infinite.deku_nuts";
    } else if (key == "bombchuBags") {
        levels = { { .key = "nei.bag.bombchus" }, { .key = "nei.bag.bombchus_30" }, { .key = "nei.bag.bombchus_50" } };
        infinite = "nei.infinite.bombchus";
    }
    if (infinite != nullptr && IsModOptionOn(kInfiniteOption)) {
        levels.push_back({ .key = infinite });
    }
    return levels;
}

void GiveUpgradeFilled(s16 upgrade, u8 value) {
    constexpr ItemId kHeldItems[] = { ITEM_DEKU_STICK, ITEM_DEKU_NUT };
    const ItemId held = kHeldItems[upgrade == UPG_DEKU_STICKS ? 0 : 1];
    Inventory_ChangeUpgrade(upgrade, value);
    if (INV_CONTENT(held) == held) {
        AMMO(held) = CUR_CAPACITY(upgrade);
    }
}

uint16_t TierReceipt(const std::string& key, int level) {
    return RandoInfCustom_Register(("fleet:tier:" + key + ":" + std::to_string(level)).c_str());
}

bool GrantLevelOnce(const std::string& key, int level, const nlohmann::json& item) {
    const uint16_t receipt = TierReceipt(key, level);
    if (receipt == 0 || RandoInfCustom_Get(S2H_RANDO_INF_SAVE, receipt)) {
        return false;
    }
    Grant({ { "item", item } });
    RandoInfCustom_Set(S2H_RANDO_INF_SAVE, receipt, true);
    return true;
}

bool GrantSharedTier(const std::string& key, int current, const nlohmann::json& item) {
    const std::vector<SharedTierGrant> levels = SharedTierGrants(key);
    if (levels.empty()) {
        return GrantLevelOnce(key, current, item);
    }
    if (current < 0 || static_cast<size_t>(current) >= levels.size()) {
        return false;
    }
    const SharedTierGrant& grant = levels[current];
    if (grant.key != nullptr) {
        CustomItemRegistry_ReceiveGetItem(grant.key);
    } else if (grant.upgrade >= 0) {
        GiveUpgradeFilled(grant.upgrade, grant.upgradeValue);
    } else {
        Rando::GiveItem(grant.item);
    }
    return true;
}

int LevelOrInfinite(const char* infiniteKey, int infiniteLevel, int level) {
    return CustomItemRegistry_IsOwned(infiniteKey) ? infiniteLevel : level;
}

int BagLevel(const char* option, const char* bagKey, int level) {
    return IsModOptionOn(option) && !CustomItemRegistry_IsOwned(bagKey) ? 0 : level;
}

int BombchuBagLevel() {
    const int bag = INV_CONTENT(ITEM_BOMBCHU) != ITEM_NONE || CustomItemRegistry_IsOwned("nei.bag.bombchus");
    const int bigger = CustomItemRegistry_IsOwned("nei.bag.bombchus_50")   ? 2
                       : CustomItemRegistry_IsOwned("nei.bag.bombchus_30") ? 1
                                                                           : 0;
    return bag + bigger;
}

int GoldenRedQuarters() {
    using ReadQuarters = int32_t (*)();
    const auto read = reinterpret_cast<ReadQuarters>(ModApi_Get()->FindService("nei.heart.golden_red_quarters"));
    return read != nullptr ? read() : 0;
}

int HeartQuarters() {
    constexpr int kHealthPerHeart = 0x10;
    const int hearts = gSaveContext.save.saveInfo.playerData.healthCapacity / kHealthPerHeart;
    return hearts * 4 + static_cast<int>(GET_QUEST_HEART_PIECE_COUNT) - GoldenRedQuarters();
}

nlohmann::json ReadSharedTiers() {
    constexpr int kInfiniteWallet = 5;
    return { { "bows", LevelOrInfinite("nei.infinite.quiver", 4, CUR_UPG_VALUE(UPG_QUIVER)) },
             { "bombBags", LevelOrInfinite("nei.infinite.bomb_bag", 4, CUR_UPG_VALUE(UPG_BOMB_BAG)) },
             { "bombchuBags", LevelOrInfinite("nei.infinite.bombchus", 4, BombchuBagLevel()) },
             { "dekuStickBag", LevelOrInfinite("nei.infinite.deku_sticks", 4,
                                               BagLevel("oot.shuffle_deku_stick_bag", "nei.bag.deku_sticks",
                                                        CUR_UPG_VALUE(UPG_DEKU_STICKS))) },
             { "dekuNutBag", LevelOrInfinite("nei.infinite.deku_nuts", 4,
                                             BagLevel("oot.shuffle_deku_nut_bag", "nei.bag.deku_nuts",
                                                      CUR_UPG_VALUE(UPG_DEKU_NUTS))) },
             { "magic", LevelOrInfinite("nei.infinite.magic", 3,
                                        gSaveContext.save.saveInfo.playerData.isMagicAcquired +
                                            gSaveContext.save.saveInfo.playerData.isDoubleMagicAcquired) },
             { "wallets", LevelOrInfinite("nei.infinite.wallet", kInfiniteWallet,
                                          BagLevel("oot.shuffle_child_wallet", "nei.bag.child_wallet",
                                                   1 + CUR_UPG_VALUE(UPG_WALLET))) },
             { "fireArrows", static_cast<int>(INV_CONTENT(ITEM_ARROW_FIRE) == ITEM_ARROW_FIRE) },
             { "iceArrows", static_cast<int>(INV_CONTENT(ITEM_ARROW_ICE) == ITEM_ARROW_ICE) },
             { "lightArrows", static_cast<int>(INV_CONTENT(ITEM_ARROW_LIGHT) == ITEM_ARROW_LIGHT) },
             { "eponasSong", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_EPONA) != 0) },
             { "songOfStorms", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_STORMS) != 0) },
             { "hookshot", static_cast<int>(INV_CONTENT(ITEM_HOOKSHOT) != ITEM_NONE) },
             { "lens", static_cast<int>(INV_CONTENT(ITEM_LENS_OF_TRUTH) != ITEM_NONE) },
             { "goronMask", static_cast<int>(INV_CONTENT(ITEM_MASK_GORON) != ITEM_NONE) },
             { "zoraMask", static_cast<int>(INV_CONTENT(ITEM_MASK_ZORA) != ITEM_NONE) },
             { "keatonMask", static_cast<int>(INV_CONTENT(ITEM_MASK_KEATON) != ITEM_NONE) },
             { "bunnyHood", static_cast<int>(INV_CONTENT(ITEM_MASK_BUNNY) != ITEM_NONE) },
             { "maskOfTruth", static_cast<int>(INV_CONTENT(ITEM_MASK_TRUTH) != ITEM_NONE) },
             { "ocarina", static_cast<int>(INV_CONTENT(ITEM_OCARINA_OF_TIME) != ITEM_NONE) },
             { "songOfTime", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_TIME) != 0) },
             { "sunsSong", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_SUN) != 0) },
             { "sariasSong", static_cast<int>(CHECK_QUEST_ITEM(QUEST_SONG_SARIA) != 0) },
             { "swords", static_cast<int>(GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD)) },
             { "strength", static_cast<int>(ProgressiveItems_GetLevel("oot.strength", S2H_RANDO_INF_SAVE)) },
             { "heartQuarters", HeartQuarters() } };
}

bool SynchronizeBottles() {
    using ReadBottles = const char* (*)(void);
    using WriteBottles = void (*)(const char*);
    const auto get = Host<const char* (*)(const char*)>("GetBottles");
    const auto report = Host<bool (*)(const char*, const char*)>("ReportBottles");
    const auto read = reinterpret_cast<ReadBottles>(ModApi_Get()->FindService("nei.bottles.read"));
    const auto write = reinterpret_cast<WriteBottles>(ModApi_Get()->FindService("nei.bottles.write"));
    if (get == nullptr || report == nullptr || read == nullptr || write == nullptr) {
        return false;
    }
    bool isRewritten = false;
    const char* remote = get(kGame);
    if (remote != nullptr && remote[0] != '\0' && sExchanged.appliedBottles != remote) {
        if (std::string(remote) != read()) {
            write(remote);
            isRewritten = true;
        }
        sExchanged.appliedBottles = remote;
    }
    const std::string local = read();
    if (local != sExchanged.reportedBottles && report(kGame, local.c_str())) {
        sExchanged.reportedBottles = local;
    }
    return isRewritten;
}

struct SharedEquip {
    const char* key;
    u8 slot;
    u8 buttonItem = ITEM_NONE;
};

constexpr SharedEquip kSharedEquips[] = {
    { "fireArrows", SLOT_ARROW_FIRE, ITEM_BOW_FIRE },
    { "iceArrows", SLOT_ARROW_ICE, ITEM_BOW_ICE },
    { "lightArrows", SLOT_ARROW_LIGHT, ITEM_BOW_LIGHT },
    { "bow", SLOT_BOW },
    { "ocarina", SLOT_OCARINA },
    { "bombs", SLOT_BOMB },
    { "bombchus", SLOT_BOMBCHU },
    { "dekuSticks", SLOT_DEKU_STICK },
    { "dekuNuts", SLOT_DEKU_NUT },
    { "hookshot", SLOT_HOOKSHOT },
    { "lens", SLOT_LENS_OF_TRUTH },
    { "magicBeans", SLOT_MAGIC_BEANS },
    { "bottle1", SLOT_BOTTLE_1 },
    { "bottle2", SLOT_BOTTLE_2 },
    { "bottle3", SLOT_BOTTLE_3 },
    { "bottle4", SLOT_BOTTLE_4 },
};
constexpr size_t kSharedButtons = 7;
constexpr size_t kSharedCButtons = 3;

struct EquipButton {
    u8* item;
    u8* slot;
};

constexpr u8 kSharedDpadButtons[] = { EQUIP_SLOT_D_UP, EQUIP_SLOT_D_DOWN, EQUIP_SLOT_D_LEFT, EQUIP_SLOT_D_RIGHT };

EquipButton SharedButton(size_t index) {
    if (index < kSharedCButtons) {
        const size_t button = EQUIP_SLOT_C_LEFT + index;
        return { &BUTTON_ITEM_EQUIP(0, button), &C_SLOT_EQUIP(0, button) };
    }
    const u8 button = kSharedDpadButtons[index - kSharedCButtons];
    return { &DPAD_BUTTON_ITEM_EQUIP(0, button), &DPAD_SLOT_EQUIP(0, button) };
}

const SharedEquip* FindEquipOnButton(const EquipButton& button) {
    if (*button.item == ITEM_NONE) {
        return nullptr;
    }
    for (const auto& equip : kSharedEquips) {
        const bool isHeld =
            equip.buttonItem != ITEM_NONE ? *button.item == equip.buttonItem : *button.slot == equip.slot;
        if (isHeld) {
            return &equip;
        }
    }
    return nullptr;
}

nlohmann::json ReadSharedEquips() {
    auto equips = nlohmann::json::array();
    for (size_t index = 0; index < kSharedButtons; index++) {
        const SharedEquip* equip = FindEquipOnButton(SharedButton(index));
        equips.push_back(equip != nullptr ? equip->key : "");
    }
    return equips;
}

u8 OwnedButtonItem(const SharedEquip& equip) {
    const u8 owned = gSaveContext.save.saveInfo.inventory.items[equip.slot];
    if (owned == ITEM_NONE || equip.buttonItem == ITEM_NONE) {
        return owned;
    }
    return gSaveContext.save.saveInfo.inventory.items[SLOT_BOW] != ITEM_NONE ? equip.buttonItem : ITEM_NONE;
}

bool CanEquipSharedButton(size_t index) {
    return index < kSharedCButtons || CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0) != 0;
}

void EquipSharedItem(size_t index, const SharedEquip& equip) {
    const u8 item = OwnedButtonItem(equip);
    const EquipButton target = SharedButton(index);
    // A custom item's key lives in the item registry; overwriting or swapping its button here would orphan it.
    if (item == ITEM_NONE || !CanEquipSharedButton(index) || *target.item == ITEM_CUSTOM ||
        FindEquipOnButton(target) == &equip) {
        return;
    }
    for (size_t other = 0; other < kSharedButtons; other++) {
        const EquipButton holder = SharedButton(other);
        if (FindEquipOnButton(holder) == &equip) {
            *holder.item = *target.item;
            *holder.slot = *target.slot;
        }
    }
    *target.item = item;
    *target.slot = equip.slot;
}

void ReloadSharedButtonIcons() {
    for (size_t index = 0; index < kSharedCButtons; index++) {
        Interface_LoadItemIconImpl(gPlayState, static_cast<u8>(EQUIP_SLOT_C_LEFT + index));
    }
    for (const u8 button : kSharedDpadButtons) {
        Interface_Dpad_LoadItemIcon(gPlayState, button);
    }
}

void ApplySharedEquips(const nlohmann::json& equips) {
    if (!equips.is_array() || equips.size() != kSharedButtons) {
        return;
    }
    for (size_t index = 0; index < kSharedButtons; index++) {
        const std::string key = equips[index].is_string() ? equips[index].get<std::string>() : std::string{};
        const auto equip = std::find_if(std::begin(kSharedEquips), std::end(kSharedEquips),
                                        [&](const SharedEquip& candidate) { return key == candidate.key; });
        if (equip != std::end(kSharedEquips)) {
            EquipSharedItem(index, *equip);
        }
    }
    ReloadSharedButtonIcons();
}

nlohmann::json ReadLiveState() {
    const auto& player = gSaveContext.save.saveInfo.playerData;
    return { { "rupees", player.rupees },
             { "health", player.health },
             { "magic", player.magic },
             { "chateau", CHECK_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI) != 0 },
             { "equips", ReadSharedEquips() } };
}

void ApplyLiveState(const nlohmann::json& state) {
    auto& player = gSaveContext.save.saveInfo.playerData;
    if (state.contains("rupees")) {
        player.rupees =
            static_cast<s16>(std::clamp(state.at("rupees").get<int>(), 0, static_cast<int>(CUR_CAPACITY(UPG_WALLET))));
    }
    if (state.contains("health") && state.at("health").get<int>() > 0) {
        player.health =
            static_cast<s16>(std::min(state.at("health").get<int>(), static_cast<int>(player.healthCapacity)));
    }
    if (state.contains("magic") && player.isMagicAcquired) {
        player.magic =
            static_cast<s8>(std::clamp(state.at("magic").get<int>(), 0, static_cast<int>(gSaveContext.magicCapacity)));
    }
    if (state.contains("chateau")) {
        if (state.at("chateau").get<bool>()) {
            SET_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI);
        } else {
            CLEAR_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI);
        }
    }
    if (state.contains("equips")) {
        ApplySharedEquips(state.at("equips"));
    }
}

void SynchronizeLiveState() {
    const auto get = Host<const char* (*)(const char*)>("GetLiveState");
    const auto report = Host<bool (*)(const char*, const char*)>("ReportLiveState");
    if (get == nullptr || report == nullptr) {
        return;
    }
    const char* remote = get(kGame);
    if (remote != nullptr && remote[0] != '\0' && sExchanged.appliedLive != remote) {
        const auto state = nlohmann::json::parse(remote, nullptr, false);
        if (state.is_object()) {
            ApplyLiveState(state);
        }
        sExchanged.appliedLive = remote;
    }
    const std::string local = ReadLiveState().dump();
    if (local != sExchanged.reportedLive && report(kGame, local.c_str())) {
        sExchanged.reportedLive = local;
    }
}

bool OwnsItemNamed(const std::string& item) {
    const RandoItemId itemId = FindItemNamed(item);
    return itemId != RI_UNKNOWN && (RandoItems_IsOwned(itemId, S2H_RANDO_INF_SAVE) || !Rando::IsItemObtainable(itemId));
}

nlohmann::json ReadTiers(const nlohmann::json& shared) {
    nlohmann::json tiers = ReadSharedTiers();
    for (const auto& entry : shared) {
        const std::string key = entry.at("key");
        if (entry.value("owned", false) && !tiers.contains(key)) {
            tiers[key] = static_cast<int>(OwnsItemNamed(entry.at("item").get<std::string>()));
        }
    }
    return tiers;
}

void MarkHeldLevels(const nlohmann::json& tiers) {
    for (const auto& [key, level] : tiers.items()) {
        for (int held = 0; held < level.get<int>(); held++) {
            const uint16_t receipt = TierReceipt(key, held);
            if (receipt != 0) {
                RandoInfCustom_Set(S2H_RANDO_INF_SAVE, receipt, true);
            }
        }
    }
}

void SynchronizeSharedTiers() {
    const auto report = Host<bool (*)(const char*, const char*)>("ReportSharedTiers");
    const auto get = Host<const char* (*)(const char*)>("GetSharedTiers");
    const char* listed = report != nullptr && get != nullptr ? get(kGame) : nullptr;
    if (listed == nullptr) {
        return;
    }
    const nlohmann::json held = ReadTiers(nlohmann::json::parse(listed));
    const std::string native = held.dump();
    if (!report(kGame, native.c_str())) {
        return;
    }
    if (native != sExchanged.markedTiers) {
        MarkHeldLevels(held);
        sExchanged.markedTiers = native;
    }
    const char* raw = get(kGame);
    if (raw == nullptr) {
        return;
    }
    const auto shared = nlohmann::json::parse(raw);
    bool changed = false;
    for (const auto& entry : shared) {
        const std::string key = entry.at("key");
        const int desired = entry.at("tier");
        if (held.value(key, 0) >= desired) {
            continue;
        }
        for (int attempt = 0; attempt < 16; attempt++) {
            const int previous = ReadTiers(shared).value(key, 0);
            if (previous >= desired || !GrantSharedTier(key, previous, entry.at("item"))) {
                break;
            }
            changed = true;
            if (ReadTiers(shared).value(key, 0) <= previous) {
                break;
            }
        }
    }
    if (changed) {
        FleetRuntime_Save();
    }
}

void ReceiveItems() {
    const auto get = Host<const char* (*)(const char*)>("GetDeliveries");
    const char* raw = get != nullptr ? get(kGame) : nullptr;
    if (raw == nullptr) {
        return;
    }
    const auto deliveries = nlohmann::json::parse(raw);
    bool changed = false;
    for (const auto& delivery : deliveries) {
        const std::string key = "fleet:received:" + delivery.at("id").get<std::string>();
        const uint16_t flag = RandoInfCustom_Register(key.c_str());
        if (flag == 0) {
            throw std::runtime_error("could not register Fleet receipt");
        }
        if (RandoInfCustom_Get(S2H_RANDO_INF_SAVE, flag) || IsUnknownDelivery(delivery)) {
            continue;
        }
        SPDLOG_INFO("[Fleet] Receiving {} from {}", delivery.at("item").get<std::string>(),
                    delivery.at("id").get<std::string>());
        Grant(delivery);
        RandoInfCustom_Set(S2H_RANDO_INF_SAVE, flag, true);
        changed = true;
    }
    if (changed) {
        FleetRuntime_Save();
    }
}

void ReportChecks() {
    const auto report = Host<bool (*)(const char*, const char*)>("ReportObtained");
    if (report == nullptr) {
        return;
    }
    const auto& placements = FleetRuntime_Seed().at(kGame).at("placements");
    static std::unordered_map<std::string, RandoCheckId> checkIds;
    if (checkIds.empty()) {
        for (const auto& [id, definition] : Rando::StaticData::Checks) {
            checkIds[Rando::StaticData::GetCheckDisplayName(id)] = id;
        }
    }
    bool saved = false;
    for (const auto& [check, item] : placements.items()) {
        if (sReported.contains(check)) {
            continue;
        }
        const auto found = checkIds.find(check);
        const RandoCheckId checkId = found != checkIds.end() ? found->second : RC_UNKNOWN;
        const bool obtained = checkId != RC_UNKNOWN && RANDO_SAVE_CHECKS[checkId].obtained;
        if (!obtained) {
            continue;
        }
        if (!saved) {
            FleetRuntime_Save();
            saved = true;
        }
        if (report(kGame, check.c_str())) {
            sReported.insert(check);
        }
    }
}

// A later session in the same process reuses these: the registry keeps an item for good.
void RegisterSessionItemsOnce() {
    static bool isRegistered = false;
    if (isRegistered) {
        return;
    }
    isRegistered = true;
    S2HCustomItemRandomizer randomizer = {};
    randomizer.structSize = sizeof(randomizer);
    S2HCustomItemDefinition definition = {};
    definition.structSize = sizeof(definition);
    definition.key = "fleet:foreign";
    definition.randomizer = &randomizer;
    definition.presentationFlags = S2H_ITEM_HIDE_FROM_SAVE_EDITOR | S2H_ITEM_HIDE_FROM_KALEIDO;
    definition.getItemText = "You found an item for the other game!";
    definition.displayName = "Cross-game item";

    if (!CustomItemRegistry_Register(&definition)) {
        throw std::runtime_error("could not register Fleet foreign item");
    }
    SaveSections_Register(
        "fleet",
        [] {
            return nlohmann::json{ { "checkpoint", sCheckpoint } };
        },
        [](const nlohmann::json& section) {
            sCheckpoint = section.is_object() ? section.value("checkpoint", -1) : -1;
            if (!IsKnownCheckpoint(sCheckpoint)) {
                sCheckpoint = -1;
            }
        });
}

} // namespace

void FleetRuntime_Initialize() {
    if (!sSession.is_null()) {
        return;
    }
    const auto get = Host<const char* (*)()>("GetSession");
    const char* raw = get != nullptr ? get() : nullptr;
    if (raw == nullptr) {
        return;
    }
    sSession = nlohmann::json::parse(raw);
    RegisterSessionItemsOnce();
    FleetForeignItems_Register(FleetRuntime_Seed());
    FleetRuntime_InstallRoutes();
}

// Pending writes land before the host restores the committed save; nothing new is written.
void FleetRuntime_EndSession() {
    SaveManager_WaitForFleetSaves();
    const std::string error = SaveManager_TakeFleetSaveError();
    if (!error.empty()) {
        SPDLOG_ERROR("[Fleet] The last MM save before the reset failed: {}", error);
    }
    sSession = nullptr;
    sReported.clear();
    sRoutes = RuntimeState(1);
    sDeparture.reset();
    sTransitionComplete = false;
    sCheckpoint = -1;
    sSkipTransition = false;
    sExchanged = {};
}

bool FleetRuntime_HasSession() {
    FleetRuntime_Initialize();
    return sSession.is_object();
}

const nlohmann::json& FleetRuntime_Seed() {
    return sSession.at("seed");
}

int FleetRuntime_Slot() {
    return sSession.is_object() ? sSession.at("slot").get<int>() : -1;
}

static double sSlowestSaveMilliseconds = 0.0;

static void QueueFleetSave();

void FleetRuntime_Save() {
    const auto start = std::chrono::steady_clock::now();
    QueueFleetSave();
    const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;
    sSlowestSaveMilliseconds = std::max(sSlowestSaveMilliseconds, elapsed.count());
}

void FleetRuntime_FinishSaving() {
    SaveManager_WaitForFleetSaves();
    const std::string error = SaveManager_TakeFleetSaveError();
    if (!error.empty()) {
        throw std::runtime_error(error);
    }
}

double FleetRuntime_SlowestSaveMilliseconds() {
    return sSlowestSaveMilliseconds;
}

static void MergeSections(nlohmann::json& saveFile, const char* variant, const nlohmann::json& sections) {
    for (const auto& [name, section] : sections.items()) {
        saveFile["sections"][variant][name] = section;
    }
}

// Snapshots the state on the game thread; the save thread reads the file, serializes and writes it.
static void QueueFleetSave() {
    const std::string pastError = SaveManager_TakeFleetSaveError();
    if (!pastError.empty()) {
        throw std::runtime_error(pastError);
    }
    if (IsPlaying()) {
        auto& rupees = gSaveContext.save.saveInfo.playerData.rupees;
        rupees =
            static_cast<int16_t>(std::clamp<int>(rupees + gSaveContext.rupeeAccumulator, 0, CUR_CAPACITY(UPG_WALLET)));
        gSaveContext.rupeeAccumulator = 0;
    }
    auto owlContext = std::make_shared<SaveContext>(gSaveContext);
    owlContext->save.isOwlSave = true;
    owlContext->save.shipSaveInfo.pauseSaveEntrance = owlContext->save.entrance;
    auto cycleSave = std::make_shared<Save>(gSaveContext.save);
    nlohmann::json written;
    SaveSections_Write(written, true);
    auto sections = std::make_shared<nlohmann::json>(
        written.value(nlohmann::json::json_pointer("/sections/owl"), nlohmann::json::object()));
    SaveManager_QueueFleetSave(SaveManager_GetFileName(gSaveContext.fileNum + 1),
                               [owlContext, cycleSave, sections](nlohmann::json& saveFile) {
                                   saveFile["version"] = 7;
                                   saveFile["type"] = "2S2H_SAVE";
                                   saveFile["owlSave"] = *owlContext;
                                   MergeSections(saveFile, "owl", *sections);
                                   if (!saveFile.contains("newCycleSave")) {
                                       saveFile["newCycleSave"]["save"] = *cycleSave;
                                       MergeSections(saveFile, "newCycle", *sections);
                                   }
                               });
}

void FleetRuntime_CatchUpParked() {
    if (!FleetRuntime_HasSession() || !IsPlaying()) {
        return;
    }
    ReceiveItems();
    SynchronizeSharedTiers();
    if (SynchronizeBottles()) {
        FleetRuntime_Save();
    }
}

void FleetRuntime_FrameEnd() {
    if (!FleetRuntime_HasSession() || !IsPlaying() || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    ReportChecks();
    if (!IsTextboxOpen()) {
        ReceiveItems();
        SynchronizeSharedTiers();
        SynchronizeBottles();
        SynchronizeLiveState();
    }
}

void FleetRuntime_InstallRoutes() {
    sRoutes.Install(FleetRuntime_Seed().value("crossDungeonEntrances", nlohmann::json{}));
}

nlohmann::json FleetRuntime_ReadOwnership(const nlohmann::json& items) {
    auto owned = nlohmann::json::object();
    for (const auto& item : items) {
        if (item.is_string()) {
            owned[item.get<std::string>()] = OwnsItemNamed(item.get<std::string>());
        }
    }
    return owned;
}

nlohmann::json FleetRuntime_Status() {
    auto received = nlohmann::json::array();
    if (sSession.is_object() && IsPlaying()) {
        const auto get = Host<const char* (*)(const char*)>("GetDeliveries");
        const char* raw = get != nullptr ? get(kGame) : nullptr;
        if (raw != nullptr) {
            for (const auto& delivery : nlohmann::json::parse(raw)) {
                const std::string id = delivery.at("id");
                const std::string key = "fleet:received:" + id;
                if (RandoInfCustom_Get(S2H_RANDO_INF_SAVE, RandoInfCustom_Find(key.c_str()))) {
                    received.push_back(id);
                }
            }
        }
    }
    return { { "received", received },
             { "sharedTiers", sSession.is_object() && IsPlaying() ? ReadSharedTiers() : nlohmann::json{} },
             { "playing", sSession.is_object() && IsPlaying() },
             { "slot", gSaveContext.fileNum },
             { "scene", sSession.is_object() && IsPlaying() ? static_cast<int>(gPlayState->sceneId) : -1 } };
}

bool FleetRuntime_CollectForSmoke(const std::string& check) {
    if (!sSession.is_object() || !sSession.value("runtimeTests", false) || !IsPlaying() ||
        !FleetRuntime_Seed().at(kGame).at("placements").contains(check)) {
        return false;
    }
    const RandoCheckId id = Rando::StaticData::GetCheckIdFromDisplayName(check);
    const std::string expected = FleetForeignItems_KeyForCheck(FleetRuntime_Seed(), check);
    if (id == RC_UNKNOWN || RANDO_SAVE_CHECKS[id].randoItemId != RandoItems_GetRandoGet(expected.c_str())) {
        return false;
    }
    RANDO_SAVE_CHECKS[id].obtained = true;
    RANDO_SAVE_CHECKS[id].cycleObtained = true;
    RANDO_SAVE_CHECKS[id].eligible = false;
    ReportChecks();
    return true;
}

nlohmann::json FleetRuntime_CycleSmoke() {
    if (!sSession.is_object() || !sSession.value("runtimeTests", false) || !IsPlaying()) {
        return { { "preserved", false } };
    }
    const auto original = std::make_unique<SaveContext>(gSaveContext);
    const auto scene = gPlayState->actorCtx.sceneFlags;
    const auto id = RC_STARTING_ITEM_DEKU_MASK;
    const auto oldCheck = RANDO_SAVE_CHECKS[id];
    gSaveContext.save.saveInfo.weekEventReg[0] |= 0x80;
    gPlayState->actorCtx.sceneFlags.chest |= 0x80000000;
    gPlayState->actorCtx.sceneFlags.switches[0] |= 0x40000000;
    RANDO_SAVE_CHECKS[id].cycleObtained = true;
    gSaveContext.save.day = 3;
    Sram_SaveEndOfCycle(gPlayState);
    const bool preserved = (gSaveContext.save.saveInfo.weekEventReg[0] & 0x80) != 0 &&
                           (gPlayState->actorCtx.sceneFlags.chest & 0x80000000) != 0 &&
                           (gPlayState->actorCtx.sceneFlags.switches[0] & 0x40000000) != 0 &&
                           RANDO_SAVE_CHECKS[id].cycleObtained;
    const bool clockReset = gSaveContext.save.day == 0;
    gSaveContext = *original;
    gPlayState->actorCtx.sceneFlags = scene;
    RANDO_SAVE_CHECKS[id] = oldCheck;
    return { { "preserved", preserved }, { "clockReset", clockReset } };
}

int32_t FleetRuntime_Route(int32_t entrance, bool beforeNative) {
    if (!sSession.is_object() || !IsPlaying()) {
        return -1;
    }
    const int32_t canonicalExit = ComboMmCanonicalGrottoExit(entrance, gPlayState->sceneId);
    const bool grottoExit = canonicalExit != entrance;
    if (gSaveContext.respawnFlag != 0 && !(gSaveContext.respawnFlag == 4 && grottoExit)) {
        return -1;
    }
    const int32_t canonical = grottoExit ? canonicalExit : ComboMmCanonicalGrottoLoad(entrance);
    const auto transition = sRoutes.Lookup(beforeNative ? Phase::BeforeNative : Phase::AfterNative, canonical);
    if (!transition) {
        return -1;
    }
    if (transition->to.game == 1) {
        sCheckpoint = sRoutes.RecoveryForArrival(transition->to.index);
        return FleetRuntime_Materialize(transition->to.index);
    }
    sDeparture = transition;
    sTransitionComplete = false;
    return entrance;
}

bool FleetRuntime_TakeDeparture(FleetDestination& destination, std::string& payload) {
    if (!sDeparture || !sTransitionComplete) {
        return false;
    }
    const auto transition = *sDeparture;
    sCheckpoint = transition.checkpoint.index;
    sDeparture.reset();
    sTransitionComplete = false;
    gPlayState->transitionTrigger = TRANS_TRIGGER_OFF;
    gPlayState->transitionMode = TRANS_MODE_OFF;
    gSaveContext.save.entrance = FleetRuntime_Materialize(transition.checkpoint.index);
    FleetRuntime_Save();
    FleetRuntime_FinishSaving();
    payload = nlohmann::json{
        { "cross", true }, { "slot", FleetRuntime_Slot() }, { "entrance", transition.to.index }
    }.dump();
    destination = { .game = "oot",
                    .entrance = static_cast<uint32_t>(transition.to.index),
                    .payloadJson = payload.c_str() };
    return true;
}

bool FleetRuntime_QueueArrival(int32_t entrance) {
    const bool queued = sRoutes.QueueArrival(entrance);
    if (queued) {
        sSkipTransition = IsPlaying();
        sCheckpoint = sRoutes.RecoveryForArrival(entrance);
    }
    return queued;
}

int32_t FleetRuntime_Materialize(int32_t entrance) {
    return ComboMmMaterializeGrotto(entrance);
}

int32_t FleetRuntime_ConsumeArrival() {
    const int32_t entrance = sRoutes.ConsumeArrival();
    sSkipTransition = false;
    if (entrance < 0) {
        return -1;
    }
    sCheckpoint = sRoutes.RecoveryForArrival(entrance);
    return FleetRuntime_Materialize(entrance);
}

void FleetRuntime_ObserveScene(int32_t scene) {
    const auto* owner = sRoutes.FindEntrance(sCheckpoint);
    if (owner == nullptr) {
        sCheckpoint = -1;
        return;
    }
    bool inside = false;
    if (owner->pool == Pool::Dungeon) {
        inside = scene == SCENE_MITURIN || scene == SCENE_HAKUGIN || scene == SCENE_SEA || scene == SCENE_INISIE_N ||
                 scene == SCENE_INISIE_R || scene == SCENE_MITURIN_BS || scene == SCENE_HAKUGIN_BS ||
                 scene == SCENE_SEA_BS || scene == SCENE_INISIE_BS || scene == SCENE_KYOJINNOMA;
        for (const auto& dungeon : ComboCrossMmCatalogue::ExpandedDungeons()) {
            if (dungeon.entrance == sCheckpoint) {
                inside = inside || Rando::Logic::Regions.at(dungeon.region).sceneId == scene;
            }
        }
    } else if (owner->pool == Pool::Grotto) {
        inside = scene == SCENE_KAKUSIANA;
    } else {
        for (const auto& interior : ComboCrossMmCatalogue::Interiors()) {
            if (interior.entrance == sCheckpoint) {
                inside = Rando::Logic::Regions.at(interior.region).sceneId == scene;
            }
        }
    }
    if (!inside) {
        sCheckpoint = -1;
    }
}

extern "C" int32_t FleetRuntime_RecoveryEntrance(void) {
    return sCheckpoint >= 0 && IsKnownCheckpoint(sCheckpoint) ? FleetRuntime_Materialize(sCheckpoint) : -1;
}

extern "C" int32_t FleetRuntime_ResolveExit(int32_t entrance, int beforeNative) {
    return FleetRuntime_Route(entrance, beforeNative != 0);
}

extern "C" int FleetRuntime_CompleteTransition(void) {
    if (!sDeparture) {
        return 0;
    }
    sTransitionComplete = true;
    return 1;
}

extern "C" void FleetRuntime_CancelTransition(void) {
    sDeparture.reset();
    sTransitionComplete = false;
}

extern "C" int FleetRuntime_EntrancesEnabled(void) {
    return sSession.is_object() && sRoutes.Enabled();
}

void FleetRuntime_SkipNextRoute() {
    sSkipTransition = true;
}

extern "C" int32_t FleetRuntime_ResolveTransition(int32_t entrance) {
    if (sSkipTransition) {
        sSkipTransition = false;
        return entrance;
    }
    int32_t resolved = FleetRuntime_Route(entrance, true);
    if (resolved < 0) {
        resolved = FleetRuntime_Route(entrance, false);
    }
    return resolved >= 0 ? resolved : entrance;
}
