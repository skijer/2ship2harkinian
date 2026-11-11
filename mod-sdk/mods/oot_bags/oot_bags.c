/**
 * Ship's bag shuffles in Majora's Mask. Without its bag an ammo item is refused by ShouldItemGive, which the
 * seed's logic simulation also runs, so logic stops counting sticks, nuts or bombchus until the bag is found.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "z64items.h"
#include "z64logic.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "2s2h/Rando/Types.h"
#include "2s2h/ModApi/RandoInfCustom/RandoInfCustom.h"

#define CHILD_WALLET_OPTION "oot.shuffle_child_wallet"
#define STICK_BAG_OPTION "oot.shuffle_deku_stick_bag"
#define NUT_BAG_OPTION "oot.shuffle_deku_nut_bag"
#define BOMBCHU_BAG_OPTION "oot.bombchu_bag"
#define INFINITE_OPTION "oot.infinite_upgrades"
// Fleet's shared families (SharedItems.h) name MM's copy of each bag by these keys.
#define CHILD_WALLET_KEY "nei.bag.child_wallet"
#define STICK_BAG_KEY "nei.bag.deku_sticks"
#define NUT_BAG_KEY "nei.bag.deku_nuts"
#define BOMBCHU_BAG_KEY "nei.bag.bombchus"

static const char* const sRequiredHooks[] = { "ShouldItemGive", "OnCustomItemReceive", "OnLoadFile",
                                              "OnGameFrameUpdate" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;

static bool IsRandoSave(void) {
    return gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO;
}

static bool IsOptionOn(const char* key) {
    return sApi->GetRandoOption(key) != 0;
}

// The seed's simulation keeps mod items in the logic bank, not in the save.
static bool Owns(const char* key) {
    if (RandoInfCustom_IsLogicSimulation()) {
        return Z64Logic_HasKey(key);
    }
    return sApi->IsCustomItemOwned(key);
}

static bool IsLocked(const char* option, const char* bag) {
    return IsRandoSave() && IsOptionOn(option) && !Owns(bag);
}

static bool IsRupee(u8 item) {
    return item >= ITEM_RUPEE_GREEN && item <= ITEM_RUPEE_HUGE;
}

static bool IsDekuStick(u8 item) {
    return item == ITEM_DEKU_STICK || item == ITEM_DEKU_STICKS_5 || item == ITEM_DEKU_STICKS_10;
}

static bool IsDekuNut(u8 item) {
    return item == ITEM_DEKU_NUT || item == ITEM_DEKU_NUTS_5 || item == ITEM_DEKU_NUTS_10;
}

static bool IsBombchu(u8 item) {
    return item == ITEM_BOMBCHU || (item >= ITEM_BOMBCHUS_20 && item <= ITEM_BOMBCHUS_5);
}

static void RefuseWithoutBag(u8 item, bool* should) {
    if ((IsRupee(item) && IsLocked(CHILD_WALLET_OPTION, CHILD_WALLET_KEY)) ||
        (IsDekuStick(item) && IsLocked(STICK_BAG_OPTION, STICK_BAG_KEY)) ||
        (IsDekuNut(item) && IsLocked(NUT_BAG_OPTION, NUT_BAG_KEY)) ||
        (IsBombchu(item) && IsLocked(BOMBCHU_BAG_OPTION, BOMBCHU_BAG_KEY))) {
        *should = false;
    }
}

// A bag arrives filled, written straight to the save so the refusal above never sees it.
static void FillBag(s16 upgrade, ItemId item) {
    if (upgrade >= 0 && CUR_UPG_VALUE(upgrade) == 0) {
        Inventory_ChangeUpgrade(upgrade, 1);
    }
    INV_CONTENT(item) = item;
    const s16 capacity = upgrade >= 0 ? CUR_CAPACITY(upgrade) : CUR_CAPACITY(UPG_BOMB_BAG);
    AMMO(item) = MAX(AMMO(item), capacity);
}

static void ReceiveBag(const char* key) {
    if (strcmp(key, STICK_BAG_KEY) == 0) {
        FillBag(UPG_DEKU_STICKS, ITEM_DEKU_STICK);
    } else if (strcmp(key, NUT_BAG_KEY) == 0) {
        FillBag(UPG_DEKU_NUTS, ITEM_DEKU_NUT);
    } else if (strcmp(key, BOMBCHU_BAG_KEY) == 0) {
        FillBag(-1, ITEM_BOMBCHU);
    }
}

// A bagless file holds no sticks or nuts: capacity 0 is MM's own "no upgrade" row.
static void EmptyMissingBags(s32 fileNum) {
    if (IsLocked(STICK_BAG_OPTION, STICK_BAG_KEY)) {
        Inventory_ChangeUpgrade(UPG_DEKU_STICKS, 0);
    }
    if (IsLocked(NUT_BAG_OPTION, NUT_BAG_KEY)) {
        Inventory_ChangeUpgrade(UPG_DEKU_NUTS, 0);
    }
}

typedef struct {
    const char* key;
    const char* name;
    const char* icon;
    ItemId item;
    s16 upgrade;
} InfiniteUpgrade;

#define ICON(name) "__OTR__textures/icon_item_custom/" name

static const InfiniteUpgrade sInfiniteUpgrades[] = {
    { "nei.infinite.quiver", "Infinite Quiver", ICON("gItemIconInfiniteQuiverTex"), ITEM_BOW, UPG_QUIVER },
    { "nei.infinite.bomb_bag", "Infinite Bomb Bag", ICON("gItemIconInfiniteBombBagTex"), ITEM_BOMB, UPG_BOMB_BAG },
    { "nei.infinite.bombchus", "Infinite Bombchus", ICON("gItemIconInfiniteBombchusTex"), ITEM_BOMBCHU,
      UPG_BOMB_BAG },
    { "nei.infinite.deku_sticks", "Infinite Deku Sticks", ICON("gItemIconInfiniteDekuSticksTex"), ITEM_DEKU_STICK,
      UPG_DEKU_STICKS },
    { "nei.infinite.deku_nuts", "Infinite Deku Nuts", ICON("gItemIconInfiniteDekuNutsTex"), ITEM_DEKU_NUT,
      UPG_DEKU_NUTS },
};

#define INFINITE_WALLET_KEY "nei.infinite.wallet"
#define INFINITE_MAGIC_KEY "nei.infinite.magic"

static void KeepInfiniteUpgradesFull(void) {
    if (!IsRandoSave() || gPlayState == NULL) {
        return;
    }
    for (u32 i = 0; i < ARRAY_COUNT(sInfiniteUpgrades); i++) {
        const InfiniteUpgrade* upgrade = &sInfiniteUpgrades[i];
        if (sApi->IsCustomItemOwned(upgrade->key) && INV_CONTENT(upgrade->item) == upgrade->item) {
            AMMO(upgrade->item) = CUR_CAPACITY(upgrade->upgrade);
        }
    }
    if (sApi->IsCustomItemOwned(INFINITE_WALLET_KEY)) {
        gSaveContext.save.saveInfo.playerData.rupees = CUR_CAPACITY(UPG_WALLET);
    }
    if (sApi->IsCustomItemOwned(INFINITE_MAGIC_KEY) && gSaveContext.save.saveInfo.playerData.isMagicAcquired) {
        gSaveContext.save.saveInfo.playerData.magic = gSaveContext.magicCapacity;
    }
}

// Rewards paid through Rupees_ChangeBy skip ShouldItemGive, so a walletless Link drops them every frame.
static void EmptyWalletlessPurse(void) {
    if (gPlayState == NULL || !IsLocked(CHILD_WALLET_OPTION, CHILD_WALLET_KEY)) {
        return;
    }
    gSaveContext.save.saveInfo.playerData.rupees = 0;
    gSaveContext.rupeeAccumulator = 0;
}

static void UpdateEachFrame(void) {
    EmptyWalletlessPurse();
    KeepInfiniteUpgradesFull();
}

static bool HasChildWalletInLogic(void* userData) {
    return !IsOptionOn(CHILD_WALLET_OPTION) || Z64Logic_HasKey(CHILD_WALLET_KEY);
}

// Every check paid for in rupees besides shops and Tingle, which their check types select.
static const int32_t sPaidChecks[] = {
    RC_CLOCK_TOWN_EAST_SHOOTING_GALLERY_HIGH_SCORE,
    RC_CLOCK_TOWN_EAST_SHOOTING_GALLERY_PERFECT_SCORE,
    RC_CLOCK_TOWN_EAST_TREASURE_CHEST_GAME_DEKU,
    RC_CLOCK_TOWN_EAST_TREASURE_CHEST_GAME_GORON,
    RC_CLOCK_TOWN_EAST_TREASURE_CHEST_GAME_HUMAN,
    RC_CLOCK_TOWN_EAST_TREASURE_CHEST_GAME_ZORA,
    RC_CLOCK_TOWN_WEST_BANK_ADULTS_WALLET,
    RC_CLOCK_TOWN_WEST_BANK_INTEREST,
    RC_CLOCK_TOWN_WEST_BANK_PIECE_OF_HEART,
    RC_CLOCK_TOWN_WEST_LOTTERY,
    RC_DEKU_PLAYGROUND_ALL_DAYS,
    RC_DEKU_PLAYGROUND_ANY_DAY,
    RC_DOGGY_RACETRACK_PIECE_OF_HEART,
    RC_GORMAN_MILK_PURCHASE,
    RC_GORON_VILLAGE_SCRUB_BOMB_BAG,
    RC_IKANA_CANYON_SCRUB_POTION_REFILL,
    RC_MILK_BAR_PURCHASE_CHATEAU,
    RC_MILK_BAR_PURCHASE_MILK,
    RC_SOUTHERN_SWAMP_SCRUB_BEANS,
    RC_SWAMP_SHOOTING_GALLERY_HIGH_SCORE,
    RC_SWAMP_SHOOTING_GALLERY_PERFECT_SCORE,
    RC_SWORDSMAN_SCHOOL_PIECE_OF_HEART,
    RC_ZORA_HALL_SCRUB_POTION_REFILL,
};

static void RequireChildWallet(const char* name, const S2HLogicSelector* selector) {
    S2HLogicRule rule = { sizeof(S2HLogicRule) };
    rule.name = name;
    rule.effect = S2H_LOGIC_REQUIRE;
    rule.condition = HasChildWalletInLogic;
    rule.include = selector;
    rule.includeCount = 1;
    sApi->RegisterLogicRule(&rule);
}

// The host copies selectors and replaces a rule registered under a name it already holds.
static void RegisterWalletRules(void) {
    S2HLogicSelector shops = Z64Logic_SelectAny();
    shops.targets = S2H_LOGIC_TARGET_LOCATIONS;
    shops.checkType = RCTYPE_SHOP;
    RequireChildWallet("oot_bags.child_wallet_shops", &shops);

    S2HLogicSelector tingle = shops;
    tingle.checkType = RCTYPE_TINGLE_SHOP;
    RequireChildWallet("oot_bags.child_wallet_tingle", &tingle);

    S2HLogicSelector paid = Z64Logic_SelectAny();
    paid.targets = S2H_LOGIC_TARGET_LOCATIONS;
    paid.checks = sPaidChecks;
    paid.checkCount = ARRAY_COUNT(sPaidChecks);
    RequireChildWallet("oot_bags.child_wallet_paid", &paid);
}

typedef struct {
    const char* key;
    const char* name;
    const char* icon;
    const char* model;
    const char* text;
    const char* poolOption;
    uint32_t flags;
} BagItem;

static bool RegisterBagItem(const BagItem* bag) {
    static S2HCustomItemRandomizer pools[16];
    static uint32_t poolCount;
    if (poolCount >= ARRAY_COUNT(pools)) {
        return false;
    }
    S2HCustomItemRandomizer* pool = &pools[poolCount++];
    *pool = (S2HCustomItemRandomizer){ .structSize = sizeof(S2HCustomItemRandomizer),
                                       .flags = bag->flags,
                                       .type = S2H_CUSTOM_ITEM_TYPE_ITEM,
                                       .poolCount = 1,
                                       .logicKey = bag->key,
                                       .hintKey = bag->key,
                                       .poolModeOption = bag->poolOption,
                                       .poolModeValue = 1 };

    S2HCustomItemDefinition definition = Z64Items_Define(bag->key, bag->icon, NULL);
    definition.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO;
    definition.displayName = bag->name;
    definition.getItemScale = 1.0f;
    Z64Items_SetTextbox(&definition, bag->text);
    if (bag->model != NULL) {
        Z64Items_SetGetItemModel(&definition, bag->model);
    }
    Z64Items_SetLogic(&definition, pool);
    return Z64Items_Register(sApi, &definition);
}

static const BagItem sBags[] = {
    { CHILD_WALLET_KEY, "Child Wallet", ICON("gItemIconChildWalletTex"),
      "__OTR__objects/object_gi_purse/gGiWalletDL", "You got a %rChild Wallet%w!&Now you can carry rupees.",
      CHILD_WALLET_OPTION, S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT },
    { STICK_BAG_KEY, "Deku Stick Bag", ICON("gItemIconDekuStickBagTex"),
      "__OTR__objects/object_gi_stick/gGiStickDL", "You got a %rDeku Stick Bag%w!&Now you can carry Deku Sticks.",
      STICK_BAG_OPTION, S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT },
    { NUT_BAG_KEY, "Deku Nut Bag", ICON("gItemIconDekuNutBagTex"),
      "__OTR__objects/object_gi_nuts/gGiNutDL", "You got a %rDeku Nut Bag%w!&Now you can carry Deku Nuts.",
      NUT_BAG_OPTION, S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT },
    { BOMBCHU_BAG_KEY, "Bombchu Bag", ICON("gItemIconBombchuBagTex"),
      "__OTR__objects/object_gi_bomb_2/gGiBombchuDL", "You got a %rBombchu Bag%w!&Now you can carry Bombchus.",
      BOMBCHU_BAG_OPTION, S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT },
};

static void RegisterInfiniteUpgrades(void) {
    static char texts[ARRAY_COUNT(sInfiniteUpgrades) + 2][96];
    for (u32 i = 0; i < ARRAY_COUNT(sInfiniteUpgrades); i++) {
        snprintf(texts[i], sizeof(texts[i]), "You got the %%r%s%%w!&It never runs out.",
                 sInfiniteUpgrades[i].name);
        const BagItem item = { sInfiniteUpgrades[i].key, sInfiniteUpgrades[i].name, sInfiniteUpgrades[i].icon, NULL,
                               texts[i], INFINITE_OPTION, 0 };
        RegisterBagItem(&item);
    }
    const BagItem wallet = { INFINITE_WALLET_KEY, "Infinite Wallet", ICON("gItemIconInfiniteWalletTex"), NULL,
                             "You got the %rInfinite Wallet%w!&It never runs out.", INFINITE_OPTION, 0 };
    const BagItem magic = { INFINITE_MAGIC_KEY, "Infinite Magic", ICON("gItemIconInfiniteMagicTex"), NULL,
                            "You got %rInfinite Magic%w!&It never runs out.", INFINITE_OPTION, 0 };
    RegisterBagItem(&wallet);
    RegisterBagItem(&magic);
}

static const char* const sOffOn[] = { "Off", "On" };

static const S2HModRandoOption sOptions[] = {
    { sizeof(S2HModRandoOption), CHILD_WALLET_OPTION, "Shuffle Child Wallet",
      "Link cannot carry rupees until the Child Wallet is found.", sOffOn, 2, 0, S2H_MOD_RANDO_WIDGET_COMBOBOX },
    { sizeof(S2HModRandoOption), STICK_BAG_OPTION, "Shuffle Deku Stick Bag",
      "Link cannot carry Deku Sticks until the Deku Stick Bag is found.", sOffOn, 2, 0,
      S2H_MOD_RANDO_WIDGET_COMBOBOX },
    { sizeof(S2HModRandoOption), NUT_BAG_OPTION, "Shuffle Deku Nut Bag",
      "Link cannot carry Deku Nuts until the Deku Nut Bag is found.", sOffOn, 2, 0, S2H_MOD_RANDO_WIDGET_COMBOBOX },
    { sizeof(S2HModRandoOption), BOMBCHU_BAG_OPTION, "Bombchu Bag",
      "Link cannot carry Bombchus until the Bombchu Bag is found.", sOffOn, 2, 0, S2H_MOD_RANDO_WIDGET_COMBOBOX },
    { sizeof(S2HModRandoOption), INFINITE_OPTION, "Infinite Upgrades",
      "Adds an upgrade past each maximum that keeps that ammo, the wallet or magic always full.", sOffOn, 2, 0,
      S2H_MOD_RANDO_WIDGET_COMBOBOX },
};

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterRandoOption) || !S2H_MOD_API_HAS(sApi, RegisterLogicRule) ||
        !Z64Logic_Init(sApi)) {
        fprintf(stderr, "[oot_bags] The host lacks the randomizer API; the mod stays off\n");
        return;
    }
    for (u32 i = 0; i < ARRAY_COUNT(sOptions); i++) {
        sApi->RegisterRandoOption(&sOptions[i]);
    }
    for (u32 i = 0; i < ARRAY_COUNT(sBags); i++) {
        RegisterBagItem(&sBags[i]);
    }
    RegisterInfiniteUpgrades();
    RegisterWalletRules();
    S2H_REGISTER_HOOK(sApi, ShouldItemGive, RefuseWithoutBag);
    S2H_REGISTER_HOOK(sApi, OnCustomItemReceive, ReceiveBag);
    S2H_REGISTER_HOOK(sApi, OnLoadFile, EmptyMissingBags);
    S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, UpdateEachFrame);
}
