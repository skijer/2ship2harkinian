/**
 * NotEnoughBottles: four more bottles as items of their own, the Net, and the Bottomless Bottle. The MM grid
 * becomes [1 5][2 6][3 7][4 8][Net][Bottomless]; the four vanilla bottles stay vanilla, the others answer to the
 * engine's bottle code through the item-action and bottle-write hooks it already has.
 */

#include <math.h>
#include <string.h>

#include "z64items.h"
#include "soh/Enhancements/randomizer/randomizerEnums.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "regs.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"
#include "objects/oot_object_link_boy/object_link_boy.h"
#include "objects/oot_object_link_child/object_link_child.h"
#include "objects/object_gi_bottle/object_gi_bottle.h"
#include "variables.h"
#include "textures/oot_item_name_static/item_name_static.h"
#include "z64oot_names.h"

#define NEB_MOD_NAME "not_enough_bottles"
#define NEB_SAVE_FIELD "custom_bottles"
#define NEB_LEGACY_SAVE_FIELD "bottles"
#define NEB_NET_KEY "nei.net"
#define NEB_BOTTOMLESS_KEY "nei.bottomless_bottle"

#define NEB_EXTRA_COUNT 4
#define NEB_BOTTOMLESS NEB_EXTRA_COUNT
#define NEB_BOTTLE_COUNT (NEB_EXTRA_COUNT + 1)
#define NEB_NONE (-1)
#define NEB_VANILLA_COUNT 4
#define NEB_BUTTON_COUNT 9
#define NEB_CONTENT_COUNT (ITEM_OBABA_DRINK - ITEM_BOTTLE + 1)
#define NEB_DIGIT_SIZE 8
#define NEB_DIGIT_GAP 6
#define NEB_CATCH_RANGE 90.0f
#define NEB_CATCH_FIRST_FRAME 2
#define NEB_CATCH_LAST_FRAME 6
#define NEB_NET_GIVE_SCALE 0.65f
#define NEB_NET_HELD_ROT_X (-5.0f)
#define NEB_NET_HELD_ROT_Y (-169.0f)
#define NEB_NET_HELD_ROT_Z 64.0f
#define NEB_NET_HELD_OFFSET_X 3.2f
#define NEB_NET_HELD_OFFSET_Y 4.3f
#define NEB_NET_HELD_OFFSET_Z (-1.0f)
#define NEB_NET_HELD_SCALE 0.49f
#define NEB_DEFAULT_USES 1
#define NEB_LEGACY_WHEEL_SIZE 4
#define NEB_LEGACY_BOTTLE_COUNT 8
#define NEB_LEGACY_EMPTY 0xFF

typedef struct {
    uint8_t content[NEB_BOTTLE_COUNT];
    uint8_t bottomlessUses;
    uint16_t hotWaterFrames[NEB_BOTTLE_COUNT];
} NebSave;

typedef struct {
    uint8_t bottles[NEB_LEGACY_BOTTLE_COUNT];
    uint8_t active[2];
    uint8_t bottomlessContent;
    uint8_t bottomlessUses;
} NebLegacySave;

// A vanilla slot lent to one of the mod's bottles while an actor asks the vanilla inventory a question.
typedef struct {
    int8_t slot;
    int8_t bottle;
    uint8_t original;
    uint8_t lent;
} NebLoan;

typedef struct {
    uint8_t item;
    uint8_t uses;
} NebContentUses;

typedef struct {
    int16_t actorId;
    uint8_t item;
} NebCatch;

typedef struct {
    int16_t x;
    int16_t y;
} NebButtonPos;

static const char* const sBottleKeys[NEB_BOTTLE_COUNT] = { "nei.bottle_5", "nei.bottle_6", "nei.bottle_7",
                                                           "nei.bottle_8", NEB_BOTTOMLESS_KEY };
// Odd bottles share the first bottle's cell and even ones the second's, each after the vanilla pair.
static const uint8_t sExtraCells[NEB_EXTRA_COUNT] = { SLOT_BOTTLE_1, SLOT_BOTTLE_2, SLOT_BOTTLE_3, SLOT_BOTTLE_4 };
static const int32_t sExtraPriorities[NEB_EXTRA_COUNT] = { -1, -1, -1, -1 };

// Where the D-pad buttons take their ammo, the eleven pixels below the icon included; B and the C
// buttons read theirs from the registers instead, like Interface_DrawAmmoCount does.

static const ALIGN_ASSET(2) char sNetIconTex[] = "__OTR__textures/icon_item_custom/gItemIconNetTex";
static const ALIGN_ASSET(2) char sNetNameTex[] = "__OTR__textures/item_name_custom/gNetNameTex";
static const ALIGN_ASSET(2) char sNetXluDL[]="__OTR__objects/object_nei_net/g_net_xlu_dl";
static const ALIGN_ASSET(2) char sNetDL[] = "__OTR__objects/object_nei_net/g_net_dl";
static const ALIGN_ASSET(2) char sBottomlessIconTex[] =
    "__OTR__textures/icon_item_custom/gItemIconBottomlessBottleTex";
static const ALIGN_ASSET(2) char sBottomlessNameTex[] =
    "__OTR__textures/item_name_custom/gBottomlessBottleNameTex";
static const ALIGN_ASSET(2) char sSwingAnim[] = "__OTR__objects/oot_gameplay_keep/gPlayerAnim_link_bottle_bug_miss";
static const ALIGN_ASSET(2) char sSwingWaterAnim[] = "__OTR__objects/oot_gameplay_keep/gPlayerAnim_link_bottle_fish_miss";

// Indexed by content - ITEM_BOTTLE, one column per language, the order vanilla's name table uses.
static const char* const sContentNames[NEB_CONTENT_COUNT] = {
    "__OTR__item_name_static/gItemNameEmptyBottleENGTex",
    "__OTR__item_name_static/gItemNameRedPotionENGTex",
    "__OTR__item_name_static/gItemNameGreenPotionENGTex",
    "__OTR__item_name_static/gItemNameBluePotionENGTex",
    "__OTR__item_name_static/gItemNameFairyENGTex",
    "__OTR__item_name_static/gItemNameDekuPrincessENGTex",
    "__OTR__item_name_static/gItemNameFullMilkENGTex",
    "__OTR__item_name_static/gItemNameHalfMilkENGTex",
    "__OTR__item_name_static/gItemNameFishENGTex",
    "__OTR__item_name_static/gItemNameBugENGTex",
    "__OTR__item_name_static/gItemNameBlueFireENGTex",
    "__OTR__item_name_static/gItemNamePoeENGTex",
    "__OTR__item_name_static/gItemNameBigPoeENGTex",
    "__OTR__item_name_static/gItemNameSpringWaterENGTex",
    "__OTR__item_name_static/gItemNameHotSpringWaterENGTex",
    "__OTR__item_name_static/gItemNameZoraEggENGTex",
    "__OTR__item_name_static/gItemNameGoldDustENGTex",
    "__OTR__item_name_static/gItemNameMagicalMushroomENGTex",
    "__OTR__item_name_static/gItemNameSeaHorseENGTex",
    "__OTR__item_name_static/gItemNameChateauRomaniENGTex",
    "__OTR__item_name_static/gItemNameHylianLoachJPNTex",
    "__OTR__item_name_static/gItemNameObabasDrinkJPNTex",
};

// How often the Bottomless Bottle gives the same content back before it runs dry.
static const NebContentUses sBottomlessUses[] = {
    { ITEM_POTION_RED, 5 }, { ITEM_POTION_GREEN, 5 }, { ITEM_POTION_BLUE, 5 }, { ITEM_MILK_BOTTLE, 5 },
    { ITEM_FISH, 6 },       { ITEM_BUG, 9 },          { ITEM_FAIRY, 3 },       { ITEM_BLUE_FIRE, 7 },
    { ITEM_POE, 3 },        { ITEM_BIG_POE, 3 }, { ITEM_CHATEAU, 5 },
};

static const NebCatch sNetCatch[] = {
    { ACTOR_EN_FISH, ITEM_FISH }, { ACTOR_EN_INSECT, ITEM_BUG },
};

// The actors whose update asks Inventory_HasEmptyBottle before selling or catching into a bottle.
static const int16_t sEmptyBottleAskers[] = {
    ACTOR_EN_AKINDONUTS, ACTOR_EN_COW, ACTOR_EN_GK, ACTOR_EN_GIRLA, ACTOR_EN_IN,
    ACTOR_EN_TAB, ACTOR_EN_TRT, ACTOR_EN_TRT2, ACTOR_EN_TSN,
};
// The beggar buys any of these, and asks Inventory_HasSpecificBottle for each.
static const uint8_t sBeggarWants[] = { ITEM_BLUE_FIRE, ITEM_BUG, ITEM_FISH };

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate", "OnPlayerUpdate",   "OnPlayerResolveItemAction", "OnResolveItemGive",
                                              "OnKaleidoResolveName", "OnInterfaceResolveButtonIcon", "OnKaleidoItemDraw", "OnInterfaceDrawEnd",   "OnLoadFile",
                                              "ShouldActorUpdate", "OnActorUpdate",
                                              "OnGameFrameUpdate", "OnPlayerResolveLimbDraw", "OnPlayerPostLimbDraw" };
static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const SOHModApi* sApi;
#define REMOTE_VISUAL_INTERVAL 5u

typedef struct {
    int16_t scene;
    uint8_t active;
    uint8_t adult;
    uint8_t flags[8];
    float values[12];
} RemoteVisualState;

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual);
static void InitRemoteVisuals(void);
static uint32_t CaptureRemoteLimbEnv(PlayState* play);
static void RestoreRemoteLimbEnv(PlayState* play, uint32_t color);
static NebSave sSave;

static NebLoan sLoan = { NEB_NONE, NEB_NONE, ITEM_NONE, ITEM_NONE };
static uint16_t sButtonsToRestore;
static bool sIsCatchResolved;
static int sNetCaptureFrames;
static const char* sCatchMessage;

void Player_SetupAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void Player_ZeroSpeedXZ(Player* player);
void Player_PlayVoiceSfx(Player* player, u16 sfxId);
void func_80839FFC(Player* player, PlayState* play);
PlayerItemAction Player_ItemToItemAction(Player* player, ItemId item);
GetItemEntry ItemTable_RetrieveEntry(s16 modIndex, s16 getItemID);
int16_t OTRGetRectDimensionFromRightEdge(float v);
float OTRGetDimensionFromRightEdge(float v);

static bool IsBottleContent(uint8_t item) {
    return item >= ITEM_BOTTLE && item <= ITEM_OBABA_DRINK;
}

static bool IsOwned(int8_t bottle) {
    return sApi->IsCustomItemOwned(sBottleKeys[bottle]);
}

static bool IsEmpty(int8_t bottle) {
    return sSave.content[bottle] == ITEM_BOTTLE;
}

static void StoreBottles(void) {
    sApi->StorageSet(NEB_MOD_NAME, NEB_SAVE_FIELD, &sSave, sizeof(sSave));
}

static int8_t BottleForKey(const char* key) {
    if (key == NULL) {
        return NEB_NONE;
    }
    for (int8_t bottle = 0; bottle < NEB_BOTTLE_COUNT; bottle++) {
        if (strcmp(key, sBottleKeys[bottle]) == 0) {
            return bottle;
        }
    }
    return NEB_NONE;
}

static uint8_t* BottleButtonItem(int32_t button) {
    static const uint8_t slots[] = { EQUIP_SLOT_D_RIGHT, EQUIP_SLOT_D_LEFT, EQUIP_SLOT_D_DOWN, EQUIP_SLOT_D_UP };
    if (button >= S2H_ITEM_BUTTON_DPAD_FIRST) {
        return &DPAD_BUTTON_ITEM_EQUIP(0, slots[button - S2H_ITEM_BUTTON_DPAD_FIRST]);
    }
    return &BUTTON_ITEM_EQUIP(button == EQUIP_SLOT_B ? CUR_FORM : 0, button);
}

static uint8_t* BottleButtonSlot(int32_t buttonMinusOne) {
    static const uint8_t slots[] = { EQUIP_SLOT_D_RIGHT, EQUIP_SLOT_D_LEFT, EQUIP_SLOT_D_DOWN, EQUIP_SLOT_D_UP };
    if (buttonMinusOne >= S2H_ITEM_BUTTON_DPAD_FIRST - 1) {
        return &DPAD_SLOT_EQUIP(0, slots[buttonMinusOne - (S2H_ITEM_BUTTON_DPAD_FIRST - 1)]);
    }
    return &C_SLOT_EQUIP(0, buttonMinusOne + 1);
}

static int8_t BottleOnButton(int32_t button) {
    if (button < 0 || button >= NEB_BUTTON_COUNT || button == 4 || (*BottleButtonItem(button)) != ITEM_CUSTOM) {
        return NEB_NONE;
    }
    return BottleForKey(sApi->GetEquippedCustomItem((uint8_t)button));
}

// The icon is the content's, so the bottle reads like a vanilla one; only an empty Bottomless keeps its own.
static void ShowContent(int8_t bottle) {
    const char* icon = bottle == NEB_BOTTOMLESS && IsEmpty(bottle) ? sBottomlessIconTex
                                                                   : (const char*)gItemIcons[sSave.content[bottle]];

    CustomItemRegistry_SetIconPath(sBottleKeys[bottle], icon);
}

static uint8_t BottomlessUsesFor(uint8_t item) {
    for (uint8_t index = 0; index < ARRAY_COUNT(sBottomlessUses); index++) {
        if (sBottomlessUses[index].item == item) {
            return sBottomlessUses[index].uses;
        }
    }
    return NEB_DEFAULT_USES;
}

// Emptying the Bottomless Bottle spends one use instead of the bottle; filling it restarts the count.
static void SetContent(int8_t bottle, uint8_t item) {
    if (bottle == NEB_BOTTOMLESS && !IsEmpty(bottle) && (item == ITEM_BOTTLE || item == ITEM_MILK_HALF) &&
        sSave.bottomlessUses > 1) {
        sSave.bottomlessUses--;
    } else {
        sSave.content[bottle] = item;
        if (bottle == NEB_BOTTOMLESS) {
            sSave.bottomlessUses = item == ITEM_BOTTLE ? 0 : BottomlessUsesFor(item);
        }
    }
    if (sSave.content[bottle] == ITEM_HOT_SPRING_WATER) {
        if (!sSave.hotWaterFrames[bottle]) sSave.hotWaterFrames[bottle] = 60 * 20;
    } else sSave.hotWaterFrames[bottle] = 0;
    StoreBottles();
    ShowContent(bottle);
}

static void Acquire(int8_t bottle, uint8_t content) {
    sApi->SetCustomItemOwned(sBottleKeys[bottle], true);
    SetContent(bottle, content);
}

static int8_t FindUnownedExtra(void) {
    for (int8_t bottle = 0; bottle < NEB_EXTRA_COUNT; bottle++) {
        if (!IsOwned(bottle)) {
            return bottle;
        }
    }
    return NEB_NONE;
}

// The Bottomless first: an empty one is the bottle that most wants filling.
static int8_t FindOwnedHolding(uint8_t content) {
    if (IsOwned(NEB_BOTTOMLESS) && sSave.content[NEB_BOTTOMLESS] == content) {
        return NEB_BOTTOMLESS;
    }
    for (int8_t bottle = 0; bottle < NEB_EXTRA_COUNT; bottle++) {
        if (IsOwned(bottle) && sSave.content[bottle] == content) {
            return bottle;
        }
    }
    return NEB_NONE;
}

static int8_t FindVanillaSlot(uint8_t content) {
    for (int8_t i = 0; i < NEB_VANILLA_COUNT; i++) {
        if (gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1 + i] == content) {
            return (int8_t)(SLOT_BOTTLE_1 + i);
        }
    }
    return NEB_NONE;
}

static bool FillChateauBottle(PlayState* play) {
    int8_t bottle = FindOwnedHolding(ITEM_BOTTLE);
    if (bottle != NEB_NONE) {
        SetContent(bottle, ITEM_CHATEAU);
        return true;
    }
    if (FindVanillaSlot(ITEM_BOTTLE) == NEB_NONE) return false;
    Item_Give(play, ITEM_CHATEAU);
    return true;
}

static bool HasEmptyBottle(void) {
    return FindVanillaSlot(ITEM_BOTTLE) != NEB_NONE || FindOwnedHolding(ITEM_BOTTLE) != NEB_NONE;
}

// A write on a button holding one of these bottles lands here instead of the inventory, which has no slot for
// it. Vanilla then overwrites the button with a plain item id; it gets its custom item back before anyone
// reads the save or the button list.
static void CatchBottleWrite(bool* should, va_list args) {
    int32_t button = va_arg(args, int32_t);
    int32_t item = va_arg(args, int32_t);
    int8_t bottle = BottleOnButton(button);

    if (bottle == NEB_NONE) {
        return;
    }
    // Vanilla's half-milk test read past the inventory for this button: redo it against the real content.
    if (sSave.content[bottle] == ITEM_MILK_BOTTLE && item == ITEM_BOTTLE) {
        item = ITEM_MILK_HALF;
    } else if (sSave.content[bottle] != ITEM_MILK_BOTTLE && item == ITEM_MILK_HALF) {
        item = ITEM_BOTTLE;
    }
    SetContent(bottle, (uint8_t)item);
    sButtonsToRestore |= 1 << button;
    *should = false;
}

static void RestoreButtons(void) {
    if (sButtonsToRestore == 0) {
        return;
    }
    for (uint8_t button = 0; button < NEB_BUTTON_COUNT; button++) {
        if (button != 4 && (sButtonsToRestore & (1 << button))) {
            (*BottleButtonItem(button)) = ITEM_CUSTOM;
        }
    }
    sButtonsToRestore = 0;
}

// The engine resolves a custom item to its own action; one of these bottles answers with its content's.
static void ResolveBottleAction(int32_t item, int8_t* itemAction) {
    if (item != ITEM_CUSTOM || gPlayState == NULL) {
        return;
    }
    int8_t bottle = BottleOnButton(GET_PLAYER(gPlayState)->heldItemButton);

    if (bottle != NEB_NONE) {
        *itemAction = Player_ItemToItemAction(GET_PLAYER(gPlayState), sSave.content[bottle]);
    }
}

uint8_t KaleidoItemManager_GetVanillaSlot(uint8_t cell);

// Pause pages name cells; a rearranged page puts any bottle slot in any cell.


static void ResolveBottleName(PlayState* play, uint16_t item, const char** namePath) {
    const SOHCustomItemDefinition* selected = CustomItemRegistry_GetPauseItem();
    int8_t bottle = item == ITEM_CUSTOM && selected != NULL ? BottleForKey(selected->key) : NEB_NONE;

    if (bottle == NEB_NONE || (bottle == NEB_BOTTOMLESS && IsEmpty(bottle))) {
        return;
    }
    uint8_t language = gSaveContext.options.language <= LANGUAGE_FRA ? gSaveContext.options.language : LANGUAGE_ENG;

    *namePath = sContentNames[sSave.content[bottle] - ITEM_BOTTLE];
}

// Vanilla fills its own four first; only what they cannot hold goes to the mod's bottles.
static void GiveIntoBottles(PlayState* play, uint8_t* item, bool* handled) {
    uint8_t given = *item;
    bool newBottle = given == ITEM_BOTTLE || given == ITEM_MILK_BOTTLE || given == ITEM_POE ||
                     given == ITEM_GOLD_DUST || given == ITEM_CHATEAU || given == ITEM_HYLIAN_LOACH ||
                     given == ITEM_LONGSHOT;
    int8_t bottle;

    if (*handled) {
        return;
    }
    switch (given) {
        case ITEM_LONGSHOT: given = ITEM_POTION_RED; break;
        case ITEM_MILK: given = ITEM_MILK_BOTTLE; break;
        case ITEM_CHATEAU_2: given = ITEM_CHATEAU; break;
        case ITEM_GOLD_DUST_2: given = ITEM_GOLD_DUST; break;
        case ITEM_HYLIAN_LOACH_2: given = ITEM_HYLIAN_LOACH; break;
        case ITEM_SEAHORSE_CAUGHT: given = ITEM_SEAHORSE; break;
    }
    if (newBottle) {
        bottle = FindVanillaSlot(ITEM_NONE) == NEB_NONE ? FindUnownedExtra() : NEB_NONE;
        if (bottle != NEB_NONE) {
            Acquire(bottle, given);
            *handled = true;
        }
        return;
    }
    if (!IsBottleContent(given) || FindVanillaSlot(ITEM_BOTTLE) != NEB_NONE) {
        return;
    }
    bottle = FindOwnedHolding(ITEM_BOTTLE);
    if (bottle != NEB_NONE) {
        SetContent(bottle, given);
        *handled = true;
    }
}

// Buttons that point at a lent slot were rewritten along with it by Item_Give; they go back with the slot.
static void ResyncSlotButtons(uint8_t slot) {
    for (uint8_t button = 1; button < NEB_BUTTON_COUNT; button++) {
        if (button == 4) continue;
        if ((*BottleButtonSlot(button - 1)) != slot ||
            (*BottleButtonItem(button)) == ITEM_CUSTOM) {
            continue;
        }
        (*BottleButtonItem(button)) = gSaveContext.save.saveInfo.inventory.items[slot];
        if (gPlayState != NULL) {
            if (button >= S2H_ITEM_BUTTON_DPAD_FIRST) {
                Interface_Dpad_LoadItemIconImpl(gPlayState, HELD_ITEM_TO_DPAD(button));
            } else {
                Interface_LoadItemIconImpl(gPlayState, button);
            }
        }
    }
}

static void SettleLoan(void) {
    if (sLoan.slot == NEB_NONE) {
        return;
    }
    uint8_t now = gSaveContext.save.saveInfo.inventory.items[sLoan.slot];

    if (now != sLoan.lent) {
        SetContent(sLoan.bottle, now);
    }
    gSaveContext.save.saveInfo.inventory.items[sLoan.slot] = sLoan.original;
    ResyncSlotButtons((uint8_t)sLoan.slot);
    sLoan.slot = NEB_NONE;
}

// Only a vanilla inventory without the content borrows: a free slot if there is one, the first bottle if not.
static void LendContent(uint8_t content) {
    int8_t bottle;
    int8_t slot;

    SettleLoan();
    if (FindVanillaSlot(content) != NEB_NONE) {
        return;
    }
    bottle = FindOwnedHolding(content);
    if (bottle == NEB_NONE) {
        return;
    }
    slot = FindVanillaSlot(ITEM_NONE);
    sLoan.slot = slot != NEB_NONE ? slot : (int8_t)SLOT_BOTTLE_1;
    sLoan.bottle = bottle;
    sLoan.original = gSaveContext.save.saveInfo.inventory.items[sLoan.slot];
    sLoan.lent = content;
    gSaveContext.save.saveInfo.inventory.items[sLoan.slot] = content;
}

static void LendForEmptyBottleAsker(void* actor, bool* result) {
    if (*result) {
        LendContent(ITEM_BOTTLE);
    }
}


// Link at zero hearts is about to ask Inventory_ConsumeFairy, which only looks in the vanilla slots.
static void LendFairyToFallingLink(void* actor, bool* result) {
    if (*result && gSaveContext.save.saveInfo.playerData.health <= 0) {
        LendContent(ITEM_FAIRY);
    }
}

static void CloseActorWindow(void* actor) {
    SettleLoan();
    RestoreButtons();
}

static bool NetHeld(void);
static void ResolveNetCatch(Player* player,PlayState* play);
static void CloseFrame(void) {
    if(gPlayState && NetHeld()) {
        Player* player=GET_PLAYER(gPlayState);
        for(int i=0;i<ARRAY_COUNT(player->meleeWeaponQuads);++i)player->meleeWeaponQuads[i].base.atFlags&=~AT_ON;
        if(gPlayState->state.input[0].press.button&BTN_B)sNetCaptureFrames=12;
        if(player->meleeWeaponState!=0 || sNetCaptureFrames>0)ResolveNetCatch(player,gPlayState);
        if(sNetCaptureFrames>0)--sNetCaptureFrames;
    }
    SettleLoan();
    RestoreButtons();
}

static int32_t GetBottomlessUses(const char* key) {
    return IsEmpty(NEB_BOTTOMLESS) ? 0 : sSave.bottomlessUses;
}

static void ForgetBottles(void) {
    memset(&sSave, 0, sizeof(sSave));
    for (int8_t bottle = 0; bottle < NEB_BOTTLE_COUNT; bottle++) {
        sSave.content[bottle] = ITEM_BOTTLE;
    }
    sSave.bottomlessUses = 0;
}

// The first version kept eight bottles of its own and projected the shown ones into slots 1, 2 and 4: the
// hidden ones move to free vanilla slots, then to the new bottles.
static void AdoptLegacyBottles(void) {
    NebLegacySave legacy;

    if (!sApi->StorageHas(NEB_MOD_NAME, NEB_LEGACY_SAVE_FIELD) ||
        sApi->StorageGet(NEB_MOD_NAME, NEB_LEGACY_SAVE_FIELD, &legacy, sizeof(legacy)) != sizeof(legacy)) {
        return;
    }
    if (IsOwned(NEB_BOTTOMLESS)) {
        gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_4] = ITEM_NONE;
        sSave.content[NEB_BOTTOMLESS] = IsBottleContent(legacy.bottomlessContent) ? legacy.bottomlessContent
                                                                                  : ITEM_BOTTLE;
        sSave.bottomlessUses = legacy.bottomlessUses;
    }
    for (uint8_t index = 0; index < NEB_LEGACY_BOTTLE_COUNT; index++) {
        uint8_t content = legacy.bottles[index];
        bool isShown = index % NEB_LEGACY_WHEEL_SIZE == legacy.active[index / NEB_LEGACY_WHEEL_SIZE];

        if (content == NEB_LEGACY_EMPTY || isShown) {
            continue;
        }
        int8_t slot = FindVanillaSlot(ITEM_NONE);
        int8_t bottle = slot == NEB_NONE ? FindUnownedExtra() : NEB_NONE;

        if (slot != NEB_NONE) {
            gSaveContext.save.saveInfo.inventory.items[slot] = content;
        } else if (bottle != NEB_NONE) {
            Acquire(bottle, content);
        }
    }
    sApi->StorageRemove(NEB_MOD_NAME, NEB_LEGACY_SAVE_FIELD);
    StoreBottles();
}

static void LoadBottles(int32_t fileNum) {
    ForgetBottles();
    sLoan.slot = NEB_NONE;
    sButtonsToRestore = 0;
    sApi->StorageGet(NEB_MOD_NAME, NEB_SAVE_FIELD, &sSave, sizeof(sSave));
    for (int8_t bottle = 0; bottle < NEB_BOTTLE_COUNT; bottle++) {
        if (!IsBottleContent(sSave.content[bottle])) {
            sSave.content[bottle] = ITEM_BOTTLE;
        }
    }
    AdoptLegacyBottles();
    for (uint8_t slot = SLOT_BOTTLE_5; slot <= SLOT_BOTTLE_6; ++slot) {
        uint8_t content = gSaveContext.save.saveInfo.inventory.items[slot];
        int8_t bottle = FindUnownedExtra();
        if (IsBottleContent(content) && bottle != NEB_NONE) {
            Acquire(bottle, content);
            gSaveContext.save.saveInfo.inventory.items[slot] = ITEM_NONE;
            for (uint8_t button = 1; button < NEB_BUTTON_COUNT; ++button) {
                if (button == 4 || *BottleButtonSlot(button - 1) != slot) continue;
                sApi->EquipCustomItem(button, sBottleKeys[bottle]);
            }
        }
    }
    for (int8_t bottle = 0; bottle < NEB_BOTTLE_COUNT; bottle++) {
        ShowContent(bottle);
    }
}

static uint8_t NetContentOf(Actor* actor) {
    if (actor->id == ACTOR_EN_ELF) {
        int type = FAIRY_GET_TYPE(actor);
        return type == FAIRY_TYPE_2 || type == FAIRY_TYPE_6 ? ITEM_FAIRY : ITEM_NONE;
    }
    if(actor->id==ACTOR_EN_BUTTE)return ITEM_FAIRY;
    for (uint8_t index = 0; index < ARRAY_COUNT(sNetCatch); index++) {
        if (sNetCatch[index].actorId == actor->id) {
            return sNetCatch[index].item;
        }
    }
    return ITEM_NONE;
}

static Vec3f sNetCatchPoints[9];
static bool sNetCatchPointsValid;
static bool IsInFrontOfPlayer(Player* player, Actor* actor) {
    if(Math_Vec3f_DistXZ(&player->actor.world.pos,&actor->world.pos)<160.0f &&
       fabsf(player->actor.world.pos.y-actor->world.pos.y)<120.0f)return true;
    if(sNetCatchPointsValid) {
        for(int i=0;i<ARRAY_COUNT(sNetCatchPoints);++i)
            if(Math_Vec3f_DistXYZ(&actor->world.pos,&sNetCatchPoints[i])<100.0f)return true;
        return false;
    }
    f32 deltaX = actor->world.pos.x - player->actor.world.pos.x;
    f32 deltaZ = actor->world.pos.z - player->actor.world.pos.z;
    s32 angleDiff = ABS((s16)(Math_Atan2S(deltaX, deltaZ) - player->actor.shape.rot.y));

    if (SQ(deltaX) + SQ(deltaZ) >= SQ(NEB_CATCH_RANGE)) {
        return false;
    }
    return angleDiff < 0x4000;
}

static Actor* FindCatchableActor(Player* player, PlayState* play, uint8_t* content) {
    static const uint8_t categories[] = { ACTORCAT_SWITCH,ACTORCAT_BG,ACTORCAT_PLAYER,ACTORCAT_EXPLOSIVE,ACTORCAT_NPC,ACTORCAT_ENEMY,ACTORCAT_PROP,ACTORCAT_ITEMACTION,ACTORCAT_MISC,ACTORCAT_BOSS,ACTORCAT_DOOR,ACTORCAT_CHEST };

    for (uint8_t index = 0; index < ARRAY_COUNT(categories); index++) {
        for (Actor* actor = play->actorCtx.actorLists[categories[index]].first; actor != NULL; actor = actor->next) {
            uint8_t caught = actor->update == NULL ? ITEM_NONE : NetContentOf(actor);

            if (caught != ITEM_NONE && IsInFrontOfPlayer(player, actor)) {
                *content = caught;
                return actor;
            }
        }
    }
    return NULL;
}

// Item_Give puts it where a vanilla catch would go, and past the vanilla four into the mod's bottles.
static void ResolveNetCatch(Player* player, PlayState* play) {
    uint8_t content = ITEM_NONE;
    Actor* caught = FindCatchableActor(player, play, &content);

    if (caught == NULL) {
        return;
    }
    if (!HasEmptyBottle()) {
        sCatchMessage = "The net comes up full, and there&is no empty bottle to hold what&it caught.";
        return;
    }
    if(caught->id==ACTOR_EN_BUTTE) {
        Vec3f at=caught->world.pos;
        Actor_Spawn(&play->actorCtx,play,ACTOR_EN_ELF,at.x,at.y,at.z,0,0,0,FAIRY_PARAMS(FAIRY_TYPE_2, false, 0));
        Actor_Kill(caught);sCatchMessage="The butterfly becomes a fairy!";return;
    }
    Item_Give(play, content);
    Actor_Kill(caught);
    Audio_PlayFanfare(NA_BGM_ITEM_GET | 0x900);
    sCatchMessage = "The net scoops it up, and it lands&in an empty bottle.";
}

s32 Player_UpperAction_1(Player* player,PlayState* play);
static int32_t HoldNet(Player* player,PlayState* play) {
    int result=Player_UpperAction_1(player,play);
    for(int i=0;i<ARRAY_COUNT(player->meleeWeaponQuads);++i)player->meleeWeaponQuads[i].base.atFlags&=~AT_ON;
    if(player->meleeWeaponState) {
        if(!sIsCatchResolved){sIsCatchResolved=true;ResolveNetCatch(player,play);}
    } else sIsCatchResolved=false;
    return result;
}
static bool NetHeld(void) {
    const char* key=CustomItemRegistry_GetHeldKey();return key && !strcmp(key,NEB_NET_KEY);
}
static void NetSwordHand(Player* player,int32_t limb,Gfx** dl,Gfx* original,Vec3f* pos) {
    if (player != GET_PLAYER(gPlayState)) {
        RemoteVisualState visual;
        if (limb == PLAYER_LIMB_L_HAND && ReadRemoteVisual(player, &visual)) {
            *dl = (Gfx*)(visual.adult ? gLinkAdultLeftHandClosedNearDL : gLinkChildLeftFistNearDL);
        }
        return;
    }

    if (gPlayState == NULL || player != GET_PLAYER(gPlayState)) {
        return;
    }

    if(NetHeld() && limb==PLAYER_LIMB_L_HAND)*dl=(Gfx*)(LINK_IS_ADULT?gLinkAdultLeftHandClosedNearDL:gLinkChildLeftFistNearDL);
}
static void DrawNetInHand(PlayState* play,Player* player,int32_t limb) {
    RemoteVisualState visual;
    bool local = player == GET_PLAYER(play);
    bool held = local ? NetHeld() : ReadRemoteVisual(player, &visual);
    if (!held || limb != PLAYER_LIMB_L_HAND) {
        return;
    }
    Matrix_Push();float scale=player->actor.scale.x!=0?1/player->actor.scale.x:1;
    Matrix_Scale(scale,scale,scale,MTXMODE_APPLY);
    Matrix_RotateX(DEG_TO_RAD(NEB_NET_HELD_ROT_X), MTXMODE_APPLY);
    Matrix_RotateY(DEG_TO_RAD(NEB_NET_HELD_ROT_Y), MTXMODE_APPLY);
    Matrix_RotateZ(DEG_TO_RAD(NEB_NET_HELD_ROT_Z), MTXMODE_APPLY);
    Matrix_Translate(NEB_NET_HELD_OFFSET_X, NEB_NET_HELD_OFFSET_Y, NEB_NET_HELD_OFFSET_Z, MTXMODE_APPLY);
    Matrix_Scale(NEB_NET_HELD_SCALE, NEB_NET_HELD_SCALE, NEB_NET_HELD_SCALE, MTXMODE_APPLY);
    if (local) {
        for (int i = 0; i < ARRAY_COUNT(sNetCatchPoints); i++) {
            float t = (float)i / (ARRAY_COUNT(sNetCatchPoints) - 1);
            Vec3f sample = { -57 * t, 55 * t, 0 };
            Matrix_MultVec3f(&sample, &sNetCatchPoints[i]);
        }
        sNetCatchPointsValid = true;
    }
    uint32_t savedRemoteEnv = CaptureRemoteLimbEnv(play);
    OPEN_DISPS(play->state.gfxCtx);Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_MODELVIEW|G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++,(Gfx*)sNetDL);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_MODELVIEW|G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++,(Gfx*)sNetXluDL);
    CLOSE_DISPS(play->state.gfxCtx);Matrix_Pop();

    if (player != GET_PLAYER(play)) {
        RestoreRemoteLimbEnv(play, savedRemoteEnv);
    }
}

static void DrawNetInHand_Mm(Player* player, s32 limbIndex) {
    DrawNetInHand(gPlayState, player, limbIndex);
}


void func_80833864(PlayState* play,Player* player,s32 meleeWeaponAnim);
static void SwingNetRequest(Player* player,PlayState* play) {
    sNetCaptureFrames=12;
    func_80833864(play,player,PLAYER_MWA_FORWARD_SLASH_1H);
}
static void TakeOutNet(PlayState* play, Player* player) {
    sNetCatchPointsValid=false;
    sIsCatchResolved = false;
    sCatchMessage = NULL;
}

static void DrawNetGetItem(PlayState* play, GetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_RotateY((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    Matrix_Scale(NEB_NET_GIVE_SCALE, NEB_NET_GIVE_SCALE, NEB_NET_GIVE_SCALE, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sNetDL);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++,MATRIX_NEWMTX(play->state.gfxCtx),G_MTX_MODELVIEW|G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++,(Gfx*)sNetXluDL);
    CLOSE_DISPS(play->state.gfxCtx);
}



// The spot Interface_DrawAmmoCount would use for this button, without its reposition cosmetics.

// The uses left are what tells the Bottomless Bottle apart from an ordinary one, drawn where vanilla
// would put its ammo.


// Each bottle tool is shuffled once by the randomizer; no vanilla logic needs them.
static const SOHCustomItemRandomizer sNetRandomizer = {
    sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, NEB_NET_KEY, NEB_NET_KEY, "",
};
static const SOHCustomItemRandomizer sBottomlessRandomizer = {
    sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, 1, NEB_BOTTOMLESS_KEY, NEB_BOTTOMLESS_KEY, "",
};

static bool sRemoteVisualReady;
static bool sRemoteVisualSent;
static uint32_t sRemoteVisualFrame;
static RemoteVisualState sLastRemoteVisual;

static void ReceiveRemoteVisual(const SOHNetMessage* message) {
    (void)message;
}

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual) {
    if (!sRemoteVisualReady || gPlayState == NULL || player == GET_PLAYER(gPlayState)) {
        return false;
    }
    uint32_t peerId = sApi->GetNetPeerIdForActor(&player->actor);
    if (peerId == 0 || sApi->GetNetPeerState(peerId, "nei.not_enough_bottles.visual", visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    if (visual->active != 1 || visual->adult > 1 || visual->scene != gPlayState->sceneId ||
        visual->flags[0] > 255 || (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW)) {
        return false;
    }
    for (uint32_t i = 0; i < ARRAY_COUNT(visual->values); i++) {
        if (!isfinite(visual->values[i]) || fabsf(visual->values[i]) > 10000000.0f) {
            return false;
        }
    }
    return true;
}

static void PublishRemoteVisual(void) {
    if (!sRemoteVisualReady) {
        return;
    }
    RemoteVisualState visual = { 0 };
    PlayState* play = gPlayState;
    visual.scene = -1;
    if (play != NULL) {
        Player* player = GET_PLAYER(play);
        visual.scene = play->sceneId;
        visual.adult = LINK_IS_ADULT;
        visual.active = (NetHeld());
        if (visual.active) {

        }
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (!sRemoteVisualSent || memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) != 0) {
        if (sApi->SetNetPeerState("nei.not_enough_bottles.visual", &visual, sizeof(visual))) {
            sLastRemoteVisual = visual;
            sRemoteVisualSent = true;
            sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
        }
    }
}



static void InitRemoteVisuals(void) {
    if (!SOH_MOD_API_HAS(sApi, RegisterNetChannel) || !SOH_MOD_API_HAS(sApi, SetNetPeerState) ||
        !SOH_MOD_API_HAS(sApi, GetNetPeerState) || !SOH_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel("nei.not_enough_bottles.visual", 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
    }
}




static uint32_t CaptureRemoteLimbEnv(PlayState* play) {
    Gfx* begin = play->state.gfxCtx->polyOpa.start;
    Gfx* cursor = play->state.gfxCtx->polyOpa.p;
    while (cursor > begin) {
        cursor--;
        if (((cursor->words.w0 >> 24) & 255) == G_SETENVCOLOR) {
            return (uint32_t)cursor->words.w1;
        }
    }
    return UINT32_MAX;
}

static void RestoreRemoteLimbEnv(PlayState* play, uint32_t color) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, color >> 24, (color >> 16) & 255, (color >> 8) & 255, color & 255);
    gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING | G_SHADING_SMOOTH);
    CLOSE_DISPS(play->state.gfxCtx);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

// Same opaque stopper and translucent glass passes, matrices and scale as vanilla.
static void DrawBottleModel(PlayState* play, bool black) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGiEmptyBottleCorkDL);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    if (black) {
        gDPSetGrayscaleColor(POLY_XLU_DISP++, 28, 28, 36, 255);
        gSPGrayscale(POLY_XLU_DISP++, true);
    }
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGiEmptyBottleGlassDL);
    if (black) { gSPGrayscale(POLY_XLU_DISP++, false); }
    CLOSE_DISPS(play->state.gfxCtx);
}
static void DrawExtraBottle(PlayState* play, GetItemEntry* entry) { DrawBottleModel(play, false); }
static void DrawBottomlessBottle(PlayState* play, GetItemEntry* entry) { DrawBottleModel(play, true); }

static bool RegisterNet(void) {
    SOHCustomItemDefinition net = Z64Items_Define(NEB_NET_KEY, sNetIconTex, sNetNameTex);

    Z64Items_SetButtons(&net, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&net, SOH_ITEM_PAGE_VANILLA, SLOT_BOTTLE_5, 0);
    Z64Items_SetTextbox(&net, "You got the %rNet%w! A&long-handled net that scoops&up what a bottle would.^"
                              "Swing it with %y\xA1%w at a fairy, a&fish, a bug or a flame and it&"
                              "lands in an empty bottle.");
    Z64Items_SetPauseText(&net, "%rNet&%wSwing it with %y\xA1%w to catch something&into an empty bottle.");
    Z64Items_SetAction(&net, TakeOutNet, HoldNet);
    Z64Items_SetMeleeWeapon(&net,2);
    Z64Items_SetHeldCallbacks(&net,SwingNetRequest,NULL,NULL);
    net.modelGroup=PLAYER_MODELGROUP_SWORD_AND_SHIELD;
    net.getItemEntry.drawFunc = DrawNetGetItem;
    Z64Items_SetLogic(&net, &sNetRandomizer);
    return Z64Items_Register(sApi, &net);
}

static bool RegisterBottomless(void) {
    SOHCustomItemDefinition bottomless = Z64Items_Define(NEB_BOTTOMLESS_KEY, sBottomlessIconTex, sBottomlessNameTex);

    Z64Items_SetButtons(&bottomless, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    bottomless.flags |= SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&bottomless, SOH_ITEM_PAGE_VANILLA, SLOT_BOTTLE_6, 0);
    Z64Items_SetAmmo(&bottomless, GetBottomlessUses);
    Z64Items_SetTextbox(&bottomless,
                        "You got the %rBottomless Bottle%w!&A bottle that never empties at&once.^"
                        "Whatever you put in it lasts&for several uses: the number&on its button counts them&"
                        "down.");
    Z64Items_SetPauseText(&bottomless, "%rBottomless Bottle&%wIt holds one content for several&"
                                       "uses. The number counts them&down.");
    bottomless.getItemEntry.drawFunc = DrawBottomlessBottle;
    Z64Items_SetLogic(&bottomless, &sBottomlessRandomizer);
    return Z64Items_Register(sApi, &bottomless);
}

// MM supplies six bottles; two custom pool entries bring the total to eight.
static bool RegisterExtraBottle(int8_t bottle) {
    SOHCustomItemDefinition extra =
        Z64Items_Define(sBottleKeys[bottle], (const char*)gItemIcons[ITEM_BOTTLE], gEmptyBottleItemNameENGTex);

    Z64Items_SetButtons(&extra, SOH_CUSTOM_ITEM_C_BUTTON | SOH_CUSTOM_ITEM_DPAD);
    extra.flags |= SOH_CUSTOM_ITEM_WEARABLE;
    Z64Items_SetPlacement(&extra, SOH_ITEM_PAGE_VANILLA, sExtraCells[bottle], sExtraPriorities[bottle]);
    extra.getItemEntry = ItemTable_RetrieveEntry(MOD_NONE, GI_BOTTLE);
    extra.getItemEntry.textId = 0;
    extra.getItemEntry.drawFunc = DrawExtraBottle;
    Z64Items_SetTextbox(&extra, "You got an %rEmpty Bottle%w! You&can put something in this&bottle.");
    Z64Items_SetPauseText(&extra, "%rBottle&%wWhatever it holds, %y\xA1%w uses it like&any other bottle.");
    const SOHCustomItemRandomizer randomizer = {
        sizeof(SOHCustomItemRandomizer), 0, SOH_CUSTOM_ITEM_TYPE_ITEM, 0, bottle >= 2 ? 1 : 0, sBottleKeys[bottle], sBottleKeys[bottle], "",
    };
    Z64Items_SetLogic(&extra, &randomizer);
    return Z64Items_Register(sApi, &extra);
}

// Bottles 3 and 4 join the first two cells, so the row reads 1-3-5-7, 2-4-6-8, Net, Bottomless.
static void GroupVanillaBottles(void) {
    if (!SOH_MOD_API_HAS(sApi, PlaceLayoutItem)) {
        return;
    }
}

static void TickBottles(void) {
    CloseFrame();
    if (!gPlayState || gPlayState->pauseCtx.state != 0) return;
    for (int8_t bottle = 0; bottle < NEB_BOTTLE_COUNT; ++bottle) {
        if (!IsOwned(bottle) || sSave.content[bottle] != ITEM_HOT_SPRING_WATER) continue;
        if (!sSave.hotWaterFrames[bottle]) sSave.hotWaterFrames[bottle] = 60 * 20;
        if (--sSave.hotWaterFrames[bottle] == 0) SetContent(bottle, ITEM_SPRING_WATER);
        else if (sSave.hotWaterFrames[bottle] % 20 == 0) StoreBottles();
    }
}
static void ClosePlayerFrame(Player* player) { CloseFrame(); }

SOH_MOD_EXPORT void ModInit(void) {
    InitRemoteVisuals();
    if (!SOH_MOD_API_HAS(sApi, StorageRemove) || !SOH_MOD_API_HAS(sApi, KeepCustomItemHeld)) {
        return;
    }
    if (!RegisterNet() || !RegisterBottomless()) {
        return;
    }
    for (int8_t bottle = 0; bottle < NEB_EXTRA_COUNT; bottle++) {
        if (!RegisterExtraBottle(bottle)) {
            return;
        }
    }
    if(SOH_MOD_API_HAS(sApi,PublishService))sApi->PublishService("nei.bottles.fill_chateau",(void*)FillChateauBottle);
    GroupVanillaBottles();
    SOH_REGISTER_HOOK(sApi,OnPlayerResolveLimbDraw,NetSwordHand);
    SOH_REGISTER_HOOK(sApi,OnPlayerPostLimbDraw,DrawNetInHand_Mm);
    sApi->RegisterVB(VB_UPDATE_BOTTLE_ITEM, CatchBottleWrite);
    SOH_REGISTER_HOOK(sApi, OnPlayerResolveItemAction, ResolveBottleAction);
    SOH_REGISTER_HOOK(sApi, OnResolveItemGive, GiveIntoBottles);
    SOH_REGISTER_HOOK(sApi, OnKaleidoResolveName, ResolveBottleName);
    SOH_REGISTER_HOOK(sApi, OnPlayerUpdate, ClosePlayerFrame);
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, TickBottles);
    SOH_REGISTER_HOOK(sApi, OnLoadFile, LoadBottles);
    SOH_REGISTER_HOOK_FOR_ID(sApi, ShouldActorUpdate, ACTOR_PLAYER, LendFairyToFallingLink);
    for (uint8_t i = 0; i < ARRAY_COUNT(sEmptyBottleAskers); i++) {
        SOH_REGISTER_HOOK_FOR_ID(sApi, ShouldActorUpdate, sEmptyBottleAskers[i], LendForEmptyBottleAsker);
        SOH_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, sEmptyBottleAskers[i], CloseActorWindow);
    }
    LoadBottles(0);
}
