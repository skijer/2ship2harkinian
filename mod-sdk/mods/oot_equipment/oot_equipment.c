#include "z64items.h"
#include "z64equipment.h"
#include "z64logic.h"
#include "2s2h/ModApi/Layout/ModLayout.h"
#include "2s2h/ModApi/ModMenu/ModMenu.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <stdio.h>
#include <string.h>

#define ICON(name) "__OTR__textures/oot_icon_item_static/" name
#define NAME(name) "__OTR__textures/oot_item_name_static/" name
#define LINK_CHILD(name) "__OTR__objects/oot_object_link_child/" name
#define LINK_ADULT(name) "__OTR__objects/oot_object_link_boy/" name
#define OOT_EQUIPMENT_PAGE_KEY "oot.equipment"
#define KOKIRI_SWORD_KEY "oot.equip.kokiri_sword"
#define MM_KOKIRI_SWORD_ICON "__OTR__icon_item_static_yar/gItemIconKokiriSwordTex"
#define MM_KOKIRI_SWORD_NAME "__OTR__item_name_static/gItemNameKokiriSwordENGTex"
#define MM_KOKIRI_SWORD_TEXT "%rKokiri Sword&%wA sword from the forest. The smiths in Clock Town can sharpen it."
#define MASTER_SWORD_KEY "oot.equip.master_sword"
#define START_WITHOUT_SWORD_CVAR "gMods.OotEquipment.StartWithoutSword"
#define START_WITHOUT_SHIELD_CVAR "gMods.OotEquipment.StartWithoutShield"
#define COSMETIC_TUNIC_COLOR_CVAR "gCosmetic.Player.HumanTunic.Color"
#define COSMETIC_TUNIC_CHANGED_CVAR "gCosmetic.Player.HumanTunic.Changed"
#define BIGGORON_SWORD_KEY "oot.equip.biggoron_sword"
#define GIANTS_KNIFE_KEY "nei.equip.giants_knife"
#define DEKU_SHIELD_KEY "oot.equip.deku_shield"
#define HYLIAN_SHIELD_KEY "oot.equip.hylian_shield"
#define MIRROR_SHIELD_KEY "oot.equip.mirror_shield"
#define KOKIRI_TUNIC_KEY "oot.equip.kokiri_tunic"
#define KOKIRI_BOOTS_KEY "oot.equip.kokiri_boots"
#define GORON_TUNIC_KEY "oot.equip.goron_tunic"
#define ZORA_TUNIC_KEY "oot.equip.zora_tunic"
#define IRON_BOOTS_KEY "oot.equip.iron_boots"
#define HOVER_BOOTS_KEY "oot.equip.hover_boots"
// Shipwright AgeDependentEquipment: instruction 79 of the adult BGS hand starts the baked adult fist.
#define BGS_HAND_FIST_INSTRUCTION 79
// Read from the extracted DL: the adult fist's gauntlet loads at 105, after the pipe sync at 104.
#define MASTER_HAND_FIST_INSTRUCTION 104
// Read from the extracted DL: the adult hand's skin texture loads at 66, the mirror design resumes at 146.
#define MIRROR_HAND_FIRST_INSTRUCTION 65
#define MIRROR_HAND_END_INSTRUCTION 146
// Read from the extracted DL: the empty sheath and its black mouth sit between the handle (64) and the design (113).
#define MIRROR_SHEATH_FIRST_INSTRUCTION 65
#define MIRROR_SHEATH_END_INSTRUCTION 113
// OoT child Link stands about three quarters of adult Link's height (ageProperties 40 vs 56 eye height).
#define ADULT_TO_CHILD_SCALE 0.75f
// Read from the extracted DLs: every adult boot loads its foot matrix (segment 0x0D) at instruction 2.
#define BOOT_SKIN_MATRIX_INSTRUCTION 2

static const char* const sCursorCorners[4] = {
    "__OTR__textures/oot_equipment/gPauseMenuCursorTopLeftTex",
    "__OTR__textures/oot_equipment/gPauseMenuCursorTopRightTex",
    "__OTR__textures/oot_equipment/gPauseMenuCursorBottomLeftTex",
    "__OTR__textures/oot_equipment/gPauseMenuCursorBottomRightTex",
};
static const S2HModApi* sApi;
static const char* const sHooks[] = { "OnGameFrameUpdate", "OnPlayerUpdate", "OnCollisionResolveDamage",
    "OnPlayerShieldBlocked", "OnActorDrawEnd", "OnPlayerPostLimbDraw", "OnPlayerResolveLimbDraw", "OnLoadFile", "OnSaveInit",
    "AfterKaleidoDrawPage" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sHooks, ARRAY_COUNT(sHooks) };
// OoT z_kaleido_scope_PAL.c: equipment x positions -114,12,44,76 plus 2;
// four rows start at 58 minus 2, with a 32-pixel stride and 28-pixel icons.
static const S2HLayoutCell sEquipmentCells[16] = {
    { -112, 56, 28, 28 }, { 14, 56, 28, 28 }, { 46, 56, 28, 28 }, { 78, 56, 28, 28 },
    { -112, 24, 28, 28 }, { 14, 24, 28, 28 }, { 46, 24, 28, 28 }, { 78, 24, 28, 28 },
    { -112, -8, 28, 28 }, { 14, -8, 28, 28 }, { 46, -8, 28, 28 }, { 78, -8, 28, 28 },
    { -112, -40, 28, 28 }, { 14, -40, 28, 28 }, { 46, -40, 28, 28 }, { 78, -40, 28, 28 },
};
static const char* const sEquipmentBackground[15] = {
    "__OTR__textures/oot_icon_item_static/gPauseEquipment00Tex",
    "__OTR__textures/oot_icon_item_static/gPauseEquipment01Tex",
    "__OTR__textures/oot_icon_item_static/gPauseEquipment02Tex",
    "__OTR__textures/oot_icon_item_static/gPauseEquipment03Tex",
    "__OTR__textures/oot_icon_item_static/gPauseEquipment04Tex",
    "__OTR__textures/oot_equipment/gPauseEquipment10ENGTex",
    "__OTR__textures/oot_icon_item_static/gPauseEquipment11Tex",
    "__OTR__textures/oot_icon_item_static/gPauseEquipment12Tex",
    "__OTR__textures/oot_icon_item_static/gPauseEquipment13Tex",
    "__OTR__textures/oot_icon_item_static/gPauseEquipment14Tex",
    "__OTR__textures/oot_icon_item_static/gPauseEquipment20Tex",
    "__OTR__textures/oot_icon_item_static/gPauseEquipment21Tex",
    "__OTR__textures/oot_icon_item_static/gPauseEquipment22Tex",
    "__OTR__textures/oot_icon_item_static/gPauseEquipment23Tex",
    "__OTR__textures/oot_icon_item_static/gPauseEquipment24Tex"
};


typedef struct {
    const char* key;
    const char* icon;
    const char* name;
    const char* pauseText;
    const char* getItemText;
    u8 slot;
    u8 column;
    u16 vanillaBase;
    u8 meleeWeapon;
    Z64EquipmentRole role;
    bool isStartingPiece;
    bool isVanillaStandIn;
} OotPiece;

// Swords take B through their melee weapon and empty MM's sword slot while worn, so they never pass for MM's
// own; shields stand for MM's by vanillaBase; tunics and boots act through z64equipment.
static const OotPiece sPieces[] = {
    { KOKIRI_SWORD_KEY, MM_KOKIRI_SWORD_ICON, MM_KOKIRI_SWORD_NAME, MM_KOKIRI_SWORD_TEXT, "",
      S2H_EQUIP_SLOT_SWORD, 1, 0, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_ROLE_NONE, false, true },
    { MASTER_SWORD_KEY, ICON("gItemIconSwordMasterTex"), NAME("gMasterSwordItemNameENGTex"),
      "%rMaster Sword&%wThe Blade of Evil's Bane. Its edge never dulls.",
      "You got the %rMaster Sword%w!&The sacred blade that seals the darkness. Unlike the Razor Sword, it never "
      "loses its edge.",
      S2H_EQUIP_SLOT_SWORD, 2, EQUIP_VALUE_SWORD_NONE, PLAYER_MELEEWEAPON_SWORD_RAZOR, Z64EQUIPMENT_ROLE_NONE },
    { BIGGORON_SWORD_KEY, ICON("gItemIconSwordBiggoronTex"), NAME("gBiggoronsSwordItemNameENGTex"),
      "%rBiggoron's Sword&%wSwing it with %y\xB1%w using both hands. No shield can be held with it.",
      "You got %rBiggoron's Sword%w!&A masterpiece forged by the giant Goron smith. Its two-handed swing hits "
      "harder than any blade.",
      S2H_EQUIP_SLOT_SWORD, 3, EQUIP_VALUE_SWORD_NONE, PLAYER_MELEEWEAPON_SWORD_TWO_HANDED,
      Z64EQUIPMENT_ROLE_NONE },
    { KOKIRI_TUNIC_KEY, ICON("gItemIconTunicKokiriTex"), NAME("gKokiriTunicItemNameENGTex"),
      "%rKokiri Tunic&%wThe green clothes of the forest children.", "",
      S2H_EQUIP_SLOT_TUNIC, 1, 0, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_ROLE_NONE, true },
    { KOKIRI_BOOTS_KEY, ICON("gItemIconBootsKokiriTex"), NAME("gKokiriBootsItemNameENGTex"),
      "%rKokiri Boots&%wComfortable boots for running anywhere.", "",
      S2H_EQUIP_SLOT_BOOTS, 1, 0, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_ROLE_NONE, true },
    { DEKU_SHIELD_KEY, ICON("gItemIconShieldDekuTex"), NAME("gDekuShieldItemNameENGTex"),
      "%rDeku Shield&%wHold %y\xB4%w to block. It burns if it blocks fire.",
      "You got a %rDeku Shield%w!&A wooden shield from the forest. Fire will turn it to ash.",
      S2H_EQUIP_SLOT_SHIELD, 1, EQUIP_VALUE_SHIELD_HERO, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_SHIELD_WOOD },
    { HYLIAN_SHIELD_KEY, ICON("gItemIconShieldHylianTex"), NAME("gHylianShieldItemNameENGTex"),
      "%rHylian Shield&%wThe Hero's Shield, made in Hyrule. Hold %y\xB4%w to block.", "",
      S2H_EQUIP_SLOT_SHIELD, 2, 0, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_ROLE_NONE, false, true },
    { MIRROR_SHIELD_KEY, ICON("gItemIconShieldMirrorTex"), NAME("gMirrorShieldItemNameENGTex"),
      "%rMirror Shield&%wHold %y\xB4%w to block and reflect light.",
      "You got the %rMirror Shield%w!&The polished shield of the Spirit Temple reflects light and magic.",
      S2H_EQUIP_SLOT_SHIELD, 3, EQUIP_VALUE_SHIELD_MIRROR, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_ROLE_NONE },
    { GORON_TUNIC_KEY, ICON("gItemIconTunicGoronTex"), NAME("gGoronTunicItemNameENGTex"),
      "%rGoron Tunic&%wFire, burning floors and lava cannot hurt you.",
      "You got a %rGoron Tunic%w!&Made by the Gorons to withstand heat. Flames and lava will not harm you while "
      "you wear it.",
      S2H_EQUIP_SLOT_TUNIC, 2, 0, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_TUNIC_HEAT },
    { ZORA_TUNIC_KEY, ICON("gItemIconTunicZoraTex"), NAME("gZoraTunicItemNameENGTex"),
      "%rZora Tunic&%wBreathe underwater. Press %y\xB0%w while swimming to speed ahead.",
      "You got a %rZora Tunic%w!&Wear it and you can breathe underwater and swim like a Zora.",
      S2H_EQUIP_SLOT_TUNIC, 3, 0, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_TUNIC_WATER },
    { IRON_BOOTS_KEY, ICON("gItemIconBootsIronTex"), NAME("gIronBootsItemNameENGTex"),
      "%rIron Boots&%wSo heavy you sink in water and walk along the bottom. Currents cannot push you.",
      "You got the %rIron Boots%w!&Too heavy to run in, but they will take you to the bottom of deep water.",
      S2H_EQUIP_SLOT_BOOTS, 2, 0, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_BOOTS_IRON },
    { HOVER_BOOTS_KEY, ICON("gItemIconBootsHoverTex"), NAME("gHoverBootsItemNameENGTex"),
      "%rHover Boots&%wWalk off a ledge to float for a moment. The ground is slippery in them.",
      "You got the %rHover Boots%w!&Their magic lets you walk on air for a short while, but they grip nothing.",
      S2H_EQUIP_SLOT_BOOTS, 3, 0, PLAYER_MELEEWEAPON_NONE, Z64EQUIPMENT_BOOTS_HOVER },
};

// Native OoT pickup recipes from z_draw.c: collars precede cloth; rivets and mirror symbol use XLU.
static const ALIGN_ASSET(2) char sGiMaster[] = "__OTR__objects/nei_get_item/oot_equipment/object_toki_objects/object_toki_objects_DL_001BD0";
static const ALIGN_ASSET(2) char sGiBiggoron[] = "__OTR__objects/nei_get_item/oot_equipment/object_gi_longsword/gGiBiggoronSwordDL";
static const ALIGN_ASSET(2) char sGiDekuShield[] = "__OTR__objects/nei_get_item/oot_equipment/object_gi_shield_1/gGiDekuShieldDL";
static const ALIGN_ASSET(2) char sGiMirror[] = "__OTR__objects/nei_get_item/oot_equipment/object_gi_shield_3/gGiMirrorShieldDL";
static const ALIGN_ASSET(2) char sGiMirrorSymbol[] = "__OTR__objects/nei_get_item/oot_equipment/object_gi_shield_3/gGiMirrorShieldSymbolDL";
static const ALIGN_ASSET(2) char sGiCollar[] = "__OTR__objects/nei_get_item/oot_equipment/object_gi_clothes/gGiTunicCollarDL";
static const ALIGN_ASSET(2) char sGiTunic[] = "__OTR__objects/nei_get_item/oot_equipment/object_gi_clothes/gGiTunicDL";
static const ALIGN_ASSET(2) char sGiGoronCollar[] = "__OTR__objects/nei_get_item/oot_equipment/object_gi_clothes/gGiGoronCollarColorDL";
static const ALIGN_ASSET(2) char sGiGoronTunic[] = "__OTR__objects/nei_get_item/oot_equipment/object_gi_clothes/gGiGoronTunicColorDL";
static const ALIGN_ASSET(2) char sGiZoraCollar[] = "__OTR__objects/nei_get_item/oot_equipment/object_gi_clothes/gGiZoraCollarColorDL";
static const ALIGN_ASSET(2) char sGiZoraTunic[] = "__OTR__objects/nei_get_item/oot_equipment/object_gi_clothes/gGiZoraTunicColorDL";
static const ALIGN_ASSET(2) char sGiIron[] = "__OTR__objects/nei_get_item/oot_equipment/object_gi_boots_2/gGiIronBootsDL";
static const ALIGN_ASSET(2) char sGiRivets[] = "__OTR__objects/nei_get_item/oot_equipment/object_gi_boots_2/gGiIronBootsRivetsDL";
static const ALIGN_ASSET(2) char sGiHover[] = "__OTR__objects/nei_get_item/oot_equipment/object_gi_hoverboots/gGiHoverBootsDL";

static void DrawPickup(PlayState* play, const char* a, const char* b, const char* c, const char* d,
                       const char* translucent) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    if (a) gSPDisplayList(POLY_OPA_DISP++, (Gfx*)a);
    if (b) gSPDisplayList(POLY_OPA_DISP++, (Gfx*)b);
    if (c) gSPDisplayList(POLY_OPA_DISP++, (Gfx*)c);
    if (d) gSPDisplayList(POLY_OPA_DISP++, (Gfx*)d);
    if (translucent) {
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)translucent);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
static void DrawMasterPickup(PlayState* play, S2HGetItemEntry* entry) {
    // Shipwright Randomizer_DrawMasterSword's pickup transform and animated material.
    Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);
    Matrix_RotateZF(2.1f, MTXMODE_APPLY);
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx,
        0, play->state.frames, 0, 32, 32, 1, 0, 0, 32, 32));
    CLOSE_DISPS(play->state.gfxCtx);
    DrawPickup(play, sGiMaster, NULL, NULL, NULL, NULL);
}
static void DrawBiggoronPickup(PlayState* play, S2HGetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx,
        0, play->state.frames, 0, 32, 32, 1, 0, 0, 32, 32));
    CLOSE_DISPS(play->state.gfxCtx);
    DrawPickup(play, sGiBiggoron, NULL, NULL, NULL, NULL);
}
static void DrawDekuPickup(PlayState* play, S2HGetItemEntry* entry) {
    DrawPickup(play, sGiDekuShield, NULL, NULL, NULL, NULL);
}
static void DrawMirrorPickup(PlayState* play, S2HGetItemEntry* entry) {
    DrawPickup(play, sGiMirror, NULL, NULL, NULL, sGiMirrorSymbol);
}
static void DrawGoronPickup(PlayState* play, S2HGetItemEntry* entry) {
    DrawPickup(play, sGiGoronCollar, sGiCollar, sGiGoronTunic, sGiTunic, NULL);
}
static void DrawZoraPickup(PlayState* play, S2HGetItemEntry* entry) {
    DrawPickup(play, sGiZoraCollar, sGiCollar, sGiZoraTunic, sGiTunic, NULL);
}
static void DrawIronPickup(PlayState* play, S2HGetItemEntry* entry) {
    DrawPickup(play, sGiIron, NULL, NULL, NULL, sGiRivets);
}
static void DrawHoverPickup(PlayState* play, S2HGetItemEntry* entry) {
    DrawPickup(play, sGiHover, NULL, NULL, NULL, NULL);
}

static void RegisterWithRole(const S2HCustomEquipDefinition* definition, const OotPiece* piece) {
    if (CustomEquipRegistry_Register(definition) && piece->role != Z64EQUIPMENT_ROLE_NONE) {
        Z64Equipment_SetRole(piece->key, piece->role);
    }
}

static void RegisterPiece(const OotPiece* piece) {
    S2HCustomEquipDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = piece->key;
    definition.iconPath = piece->icon;
    definition.namePath = piece->name;
    definition.pauseText = piece->pauseText;
    definition.getItemText = piece->getItemText;
    definition.slot = piece->slot;
    definition.page = 1;
    definition.row = piece->slot;
    definition.column = piece->column;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.vanillaBase = piece->vanillaBase;
    definition.meleeWeapon = piece->meleeWeapon;
    // Equipment has no implicit randomizer contribution in MM.
    const S2HCustomItemRandomizer randomizer = {
        .structSize = sizeof(S2HCustomItemRandomizer),
        .flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT,
        .type = S2H_CUSTOM_ITEM_TYPE_EQUIP,
        .poolCount = 1,
        .logicKey = piece->key,
        .hintKey = piece->key,
    };
    // Starting pieces and the stand-in for MM's own sword never enter the pool.
    definition.randomizer = (piece->isStartingPiece || piece->isVanillaStandIn) ? NULL : &randomizer;
    definition.presentationFlags = piece->isVanillaStandIn ? S2H_EQUIP_VANILLA_STAND_IN : 0;
    if (piece->isVanillaStandIn) {
        RegisterWithRole(&definition, piece);
        return;
    }
    switch (piece->slot) {
        case S2H_EQUIP_SLOT_SWORD:
            definition.getItemEntry.drawFunc = piece->column == 2 ? DrawMasterPickup : DrawBiggoronPickup;
            break;
        case S2H_EQUIP_SLOT_SHIELD:
            definition.getItemEntry.drawFunc = piece->column == 1 ? DrawDekuPickup : DrawMirrorPickup;
            break;
        case S2H_EQUIP_SLOT_TUNIC:
            definition.getItemEntry.drawFunc = piece->column == 2 ? DrawGoronPickup : DrawZoraPickup;
            break;
        case S2H_EQUIP_SLOT_BOOTS:
            definition.getItemEntry.drawFunc = piece->column == 2 ? DrawIronPickup : DrawHoverPickup;
            break;
        default:
            break;
    }
    RegisterWithRole(&definition, piece);
}

// The Kokiri Sword is MM's sword at any upgrade; the Hylian Shield is MM's Hero's Shield, which the Mirror replaces.
static bool HasVanillaPiece(u8 slot) {
    const u16 value = CustomEquipRegistry_GetVanillaBase(slot);
    if (slot == S2H_EQUIP_SLOT_SWORD) {
        return value != EQUIP_VALUE_SWORD_NONE;
    }
    return value == EQUIP_VALUE_SHIELD_HERO;
}

// A stand-in cell is owned while Link has MM's own piece, and worn while no other piece is.
static void SyncStandIn(const char* key, u8 slot) {
    const bool hasVanillaPiece = HasVanillaPiece(slot);
    if (CustomEquipRegistry_IsOwned(key) != hasVanillaPiece) {
        CustomEquipRegistry_SetOwned(key, hasVanillaPiece);
    }
    if (hasVanillaPiece && CustomEquipRegistry_GetWorn(slot) == NULL) {
        CustomEquipRegistry_Wear(key);
    }
}

typedef struct {
    const char* icon;
    const char* name;
    const char* pauseText;
} SwordLevel;

// Indexed by MM's sword value from Kokiri on: the cell is MM's own progressive sword, so it wears MM's art.
static const SwordLevel sSwordLevels[] = {
    { MM_KOKIRI_SWORD_ICON, MM_KOKIRI_SWORD_NAME, MM_KOKIRI_SWORD_TEXT },
    { "__OTR__icon_item_static_yar/gItemIconRazorSwordTex", "__OTR__item_name_static/gItemNameRazorSwordENGTex",
      "%rRazor Sword&%wSharpened by the smiths. Its edge dulls after many swings." },
    { "__OTR__icon_item_static_yar/gItemIconGildedSwordTex", "__OTR__item_name_static/gItemNameGildedSwordENGTex",
      "%rGilded Sword&%wForged with gold dust. Its edge never dulls." },
};

static void ShowVanillaSwordLevel(void) {
    static u16 sShownValue = EQUIP_VALUE_SWORD_KOKIRI;
    const u16 value = CustomEquipRegistry_GetVanillaBase(S2H_EQUIP_SLOT_SWORD);
    if (value == sShownValue || value < EQUIP_VALUE_SWORD_KOKIRI || value > EQUIP_VALUE_SWORD_GILDED) {
        return;
    }
    sShownValue = value;
    const SwordLevel* level = &sSwordLevels[value - EQUIP_VALUE_SWORD_KOKIRI];
    sApi->SetCustomItemIconPath(KOKIRI_SWORD_KEY, level->icon);
    sApi->SetCustomItemText(KOKIRI_SWORD_KEY, S2H_ITEM_TEXT_PAUSE, level->pauseText);
    if (S2H_MOD_API_HAS(sApi, SetCustomItemNamePath)) {
        sApi->SetCustomItemNamePath(KOKIRI_SWORD_KEY, level->name);
    }
}

// As in OoT, Link always wears a tunic and boots: the Kokiri pair comes back whenever the slot empties.
static void WearKokiriWhenBare(const char* key, u8 slot) {
    if (CustomEquipRegistry_IsOwned(key) && CustomEquipRegistry_GetWorn(slot) == NULL) {
        CustomEquipRegistry_Wear(key);
    }
}

static void SyncStandIns(void) {
    if (gPlayState == NULL) {
        return;
    }
    SyncStandIn(KOKIRI_SWORD_KEY, S2H_EQUIP_SLOT_SWORD);
    SyncStandIn(HYLIAN_SHIELD_KEY, S2H_EQUIP_SLOT_SHIELD);
    WearKokiriWhenBare(KOKIRI_TUNIC_KEY, S2H_EQUIP_SLOT_TUNIC);
    WearKokiriWhenBare(KOKIRI_BOOTS_KEY, S2H_EQUIP_SLOT_BOOTS);
    ShowVanillaSwordLevel();
}

// A randomizer file takes its sword and shield from its own starting items instead.
static void ApplyStartingEquipment(s16 fileNum) {
    if (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) {
        return;
    }
    if (CVarGetInteger(START_WITHOUT_SWORD_CVAR, 0)) {
        SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
        BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = ITEM_NONE;
    }
    if (CVarGetInteger(START_WITHOUT_SHIELD_CVAR, 0)) {
        SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE);
    }
}

static void RegisterStartOption(const char* label, const char* cvar, const char* tooltip) {
    S2HModMenuWidget widget = { 0 };
    widget.structSize = sizeof(widget);
    widget.section = "Mods";
    widget.sidebar = "Equipment";
    widget.type = S2H_MOD_MENU_CHECKBOX;
    widget.label = label;
    widget.cvar = cvar;
    widget.tooltip = tooltip;
    sApi->RegisterMenuWidget(&widget);
}

static void RegisterMenu(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterMenuSidebar) || !S2H_MOD_API_HAS(sApi, RegisterMenuWidget)) {
        return;
    }
    sApi->RegisterMenuSidebar("Mods", "Equipment", "gMods.Menu.Page", 1);
    RegisterStartOption("Start without a sword", START_WITHOUT_SWORD_CVAR,
                        "New files start without the Kokiri Sword. Randomizer files use their starting items.");
    RegisterStartOption("Start without a shield", START_WITHOUT_SHIELD_CVAR,
                        "New files start without the Hero's Shield. Randomizer files use their starting items.");
}

// The Goron Tunic lends the Goron's guard: while R is held, a hit from any side is blocked like a shield's.
static void GuardLikeGoron(Actor* victim, ColliderElement* attack, f32* damage) {
    if (gPlayState == NULL || attack == NULL) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (victim != &player->actor || !(player->stateFlags1 & PLAYER_STATE1_400000) ||
        !Z64Equipment_IsRoleWorn(player, Z64EQUIPMENT_TUNIC_HEAT)) {
        return;
    }
    *damage = 0.0f;
    player->actor.colChkInfo.damage = 0;
    player->actor.colChkInfo.acHitEffect = 0;
    player->cylinder.base.acFlags &= ~AC_HIT;
    Actor_PlaySfx(&player->actor, NA_SE_IT_SHIELD_BOUND);
}

// MM's logic reads its own sword and shield slots, which a worn OoT piece no longer fills. Worn, the OoT Mirror
// is MM's own mirror value, so it reflects every native light beam.
static void GrantEquipmentLogic(void) {
    static const Z64LogicSource sSwords[] = { { MASTER_SWORD_KEY }, { BIGGORON_SWORD_KEY }, { GIANTS_KNIFE_KEY } };
    static const Z64LogicSource sDekuShield = { DEKU_SHIELD_KEY };
    static const Z64LogicSource sMirrorShield = { MIRROR_SHIELD_KEY };
    static const char* const sSlashing[] = { "CanUseHumanSword", "CanHitSwitch", "CanAttack", "CanDamage", NULL };
    static const char* const sBlocking[] = { "CanShield", NULL };
    static const char* const sReflecting[] = { "CanShield", "CanReflectLight", NULL };

    if (!Z64Logic_Init(sApi)) {
        return;
    }
    for (u32 i = 0; i < ARRAY_COUNT(sSwords); i++) {
        Z64Logic_GrantCapabilities(&sSwords[i], sSlashing);
    }
    Z64Logic_GrantCapabilities(&sDekuShield, sBlocking);
    Z64Logic_GrantCapabilities(&sMirrorShield, sReflecting);
}

static void OwnStartingPieces(s32 fileNum) {
    for (u32 i = 0; i < ARRAY_COUNT(sPieces); i++) {
        if (sPieces[i].isStartingPiece && !CustomEquipRegistry_IsOwned(sPieces[i].key)) {
            CustomEquipRegistry_SetOwned(sPieces[i].key, true);
        }
    }
}

// OoT sTunicColors: Goron {100, 20, 0}, Zora {0, 60, 100}. The Kokiri Tunic is Link's own, so it keeps the
// colour the cosmetics editor gave him, else the host's default green.
static void TintOotTunic(bool* should, va_list args) {
    va_arg(args, Player*);
    Color_RGB8* color = va_arg(args, Color_RGB8*);
    if (CustomEquipRegistry_IsWorn(GORON_TUNIC_KEY)) {
        *color = (Color_RGB8){ 100, 20, 0 };
    } else if (CustomEquipRegistry_IsWorn(ZORA_TUNIC_KEY)) {
        *color = (Color_RGB8){ 0, 60, 100 };
    } else if (CustomEquipRegistry_IsWorn(KOKIRI_TUNIC_KEY) && CVarGetInteger(COSMETIC_TUNIC_CHANGED_CVAR, 0)) {
        const Color_RGBA8 cosmetic = CVarGetColor(COSMETIC_TUNIC_COLOR_CVAR, (Color_RGBA8){ 30, 105, 27, 255 });
        *color = (Color_RGB8){ cosmetic.r, cosmetic.g, cosmetic.b };
    }
}

static bool IsLocalHumanLink(Player* player) {
    return gPlayState != NULL && player == GET_PLAYER(gPlayState) && player->transformation == PLAYER_FORM_HUMAN;
}

static const char* GetBootModel(bool isLeftFoot) {
    if (CustomEquipRegistry_IsWorn(IRON_BOOTS_KEY)) {
        return isLeftFoot ? LINK_ADULT("gLinkAdultLeftIronBootDL") : LINK_ADULT("gLinkAdultRightIronBootDL");
    }
    if (CustomEquipRegistry_IsWorn(HOVER_BOOTS_KEY)) {
        return isLeftFoot ? LINK_ADULT("gLinkAdultLeftHoverBootDL") : LINK_ADULT("gLinkAdultRightHoverBootDL");
    }
    return NULL;
}

// Each boot reloads its foot's skin matrix from segment 0x0D at this instruction, which would drop the scale.
static void KeepBootOnLoadedMatrix(const char* model) {
    static const char* sPatchedModels[4];
    static u32 sPatchedCount;
    for (u32 i = 0; i < sPatchedCount; i++) {
        if (sPatchedModels[i] == model) {
            return;
        }
    }
    if (sPatchedCount >= ARRAY_COUNT(sPatchedModels)) {
        return;
    }
    Gfx skip = gsDPNoOp();
    ResourceMgr_PatchGfxByName(model, "ootEquipmentBootMatrix", BOOT_SKIN_MATRIX_INSTRUCTION, skip);
    sPatchedModels[sPatchedCount++] = model;
}

// OoT appends the boots after the skeleton; MM has no such point, so each boot seats on its live foot matrix.
static void DrawOotBoots(Player* player, int32_t limbIndex) {
    if (!IsLocalHumanLink(player) || (limbIndex != PLAYER_LIMB_LEFT_FOOT && limbIndex != PLAYER_LIMB_RIGHT_FOOT)) {
        return;
    }
    const char* model = GetBootModel(limbIndex == PLAYER_LIMB_LEFT_FOOT);
    if (model == NULL || !sApi->HasResource(model)) {
        return;
    }
    KeepBootOnLoadedMatrix(model);
    OPEN_DISPS(gPlayState->state.gfxCtx);
    Matrix_Push();
    Matrix_Scale(ADULT_TO_CHILD_SCALE, ADULT_TO_CHILD_SCALE, ADULT_TO_CHILD_SCALE, MTXMODE_APPLY);
    gDPPipeSync(POLY_OPA_DISP++);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)model);
    Matrix_Pop();
    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

static const ALIGN_ASSET(2) char sMasterSwordHandDL[] = LINK_ADULT("gLinkAdultLeftHandHoldingMasterSwordNearDL");
static const ALIGN_ASSET(2) char sBgsHandDL[] = LINK_ADULT("gLinkAdultLeftHandHoldingBgsNearDL");
static const ALIGN_ASSET(2) char sHumanLeftFistDL[] = "__OTR__objects/object_link_child/gLinkHumanLeftHandClosedDL";
static const ALIGN_ASSET(2) char sDekuShieldHandDL[] = LINK_CHILD("gLinkChildRightFistAndDekuShieldNearDL");

// Only an adult BGS hand exists; MM's own fist replaces the baked adult one.
static bool PatchBgsHand(void) {
    static bool sIsPatched;
    if (sIsPatched) {
        return true;
    }
    if (!sApi->HasResource(sBgsHandDL)) {
        return false;
    }
    Gfx fist = gsSPDisplayListOTRFilePath(sHumanLeftFistDL);
    Gfx end = gsSPEndDisplayList();
    ResourceMgr_PatchGfxByName(sBgsHandDL, "ootEquipmentBgsFist", BGS_HAND_FIST_INSTRUCTION, fist);
    ResourceMgr_PatchGfxByName(sBgsHandDL, "ootEquipmentBgsEnd", BGS_HAND_FIST_INSTRUCTION + 1, end);
    sIsPatched = true;
    return true;
}

static const ALIGN_ASSET(2) char sMirrorShieldHandDL[] = LINK_ADULT("gLinkAdultRightHandHoldingMirrorShieldNearDL");
static const ALIGN_ASSET(2) char sHumanRightFistDL[] = "__OTR__objects/object_link_child/gLinkHumanRightHandClosedDL";
static const ALIGN_ASSET(2) char sHeroShieldOnBackDL[] = "__OTR__objects/object_link_child/gLinkHumanHerosShieldWithMtxDL";
static const ALIGN_ASSET(2) char sDekuShieldOnBackDL[] = LINK_CHILD("gLinkChildDekuShieldWithMatrixDL");
static const ALIGN_ASSET(2) char sMmMirrorShieldOnBackDL[] = "__OTR__objects/object_link_child/gLinkHumanMirrorShieldWithMtxDL";
static const ALIGN_ASSET(2) char sMirrorShieldOnBackDL[] = LINK_ADULT("gLinkAdultMirrorShieldAndSheathNearDL");
static const ALIGN_ASSET(2) char sMasterSwordSheathedDL[] = LINK_ADULT("gLinkAdultMasterSwordAndSheathNearDL");
static const ALIGN_ASSET(2) char sEmptySheathDL[] = LINK_ADULT("gLinkAdultSheathNearDL");
static Gfx sHiddenLimbDL[] = { gsSPEndDisplayList() };

typedef char PatchName[32];

// Patch names are kept by the resource manager, so the caller's storage must outlive the patch.
static void SkipInstructions(const char* displayList, const char* prefix, s32 first, s32 end, PatchName* names) {
    for (s32 i = first; i < end; i++) {
        char* name = names[i - first];
        snprintf(name, sizeof(PatchName), "%s%d", prefix, i);
        Gfx skip = gsDPNoOp();
        ResourceMgr_PatchGfxByName(displayList, name, i, skip);
    }
}

// The adult hand lies between the shield face (0-64) and its mirror design (146 on); MM's fist is drawn apart.
static bool PatchMirrorShieldHand(void) {
    static bool sIsPatched;
    static PatchName sSkipNames[MIRROR_HAND_END_INSTRUCTION - MIRROR_HAND_FIRST_INSTRUCTION];
    if (sIsPatched) {
        return true;
    }
    if (!sApi->HasResource(sMirrorShieldHandDL)) {
        return false;
    }
    SkipInstructions(sMirrorShieldHandDL, "ootEquipmentMirrorSkip", MIRROR_HAND_FIRST_INSTRUCTION,
                     MIRROR_HAND_END_INSTRUCTION, sSkipNames);
    sIsPatched = true;
    return true;
}

// The adult fist follows the blade from 104 on; MM's fist is drawn apart.
static bool PatchMasterSwordHand(void) {
    static bool sIsPatched;
    if (sIsPatched) {
        return true;
    }
    if (!sApi->HasResource(sMasterSwordHandDL)) {
        return false;
    }
    Gfx end = gsSPEndDisplayList();
    ResourceMgr_PatchGfxByName(sMasterSwordHandDL, "ootEquipmentMasterEnd", MASTER_HAND_FIST_INSTRUCTION, end);
    sIsPatched = true;
    return true;
}

typedef struct {
    Gfx commands[5];
    bool isBuilt;
} HeldAtChildScale;

// The adult piece shrinks to the size it has on the back; MM's own fist around it keeps its size.
static Gfx* GetHeldAtChildScale(HeldAtChildScale* held, const char* model, const char* fist) {
    static Mtx sChildScale;
    static bool sIsScaleReady;
    if (!sIsScaleReady) {
        guScale(&sChildScale, ADULT_TO_CHILD_SCALE, ADULT_TO_CHILD_SCALE, ADULT_TO_CHILD_SCALE);
        sIsScaleReady = true;
    }
    if (held->isBuilt) {
        return held->commands;
    }
    Gfx push = gsSPMatrix(&sChildScale, G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    Gfx piece = gsSPDisplayListOTRFilePath(model);
    Gfx pop = gsSPPopMatrix(G_MTX_MODELVIEW);
    Gfx hand = gsSPDisplayListOTRFilePath(fist);
    Gfx end = gsSPEndDisplayList();
    held->commands[0] = push;
    held->commands[1] = piece;
    held->commands[2] = pop;
    held->commands[3] = hand;
    held->commands[4] = end;
    held->isBuilt = true;
    return held->commands;
}

// OoT only bakes the Mirror on the back together with a sheath; without it, MM's own sheath stays the only one.
static bool PatchMirrorShieldOnBack(void) {
    static bool sIsPatched;
    static PatchName sSkipNames[MIRROR_SHEATH_END_INSTRUCTION - MIRROR_SHEATH_FIRST_INSTRUCTION];
    if (sIsPatched) {
        return true;
    }
    if (!sApi->HasResource(sMirrorShieldOnBackDL)) {
        return false;
    }
    SkipInstructions(sMirrorShieldOnBackDL, "ootEquipmentMirrorBackSkip", MIRROR_SHEATH_FIRST_INSTRUCTION,
                     MIRROR_SHEATH_END_INSTRUCTION, sSkipNames);
    sIsPatched = true;
    return true;
}

static Gfx* GetShieldInHandModel(void) {
    static HeldAtChildScale sMirrorHeld;
    if (CustomEquipRegistry_IsWorn(DEKU_SHIELD_KEY) && sApi->HasResource(sDekuShieldHandDL)) {
        return (Gfx*)sDekuShieldHandDL;
    }
    if (CustomEquipRegistry_IsWorn(MIRROR_SHIELD_KEY) && PatchMirrorShieldHand()) {
        return GetHeldAtChildScale(&sMirrorHeld, sMirrorShieldHandDL, sHumanRightFistDL);
    }
    return NULL;
}

static bool IsOotMirrorOnBack(void) {
    return CustomEquipRegistry_IsWorn(MIRROR_SHIELD_KEY) && PatchMirrorShieldOnBack();
}

// MM draws the shield on Link's back from its own Hero's Shield list; the Deku Shield borrows that call.
static void UpdateDekuShieldOnBack(void) {
    static bool sIsDekuOnBack;
    const bool shouldShowDeku = CustomEquipRegistry_IsWorn(DEKU_SHIELD_KEY) && sApi->HasResource(sDekuShieldOnBackDL);
    if (shouldShowDeku == sIsDekuOnBack) {
        return;
    }
    sIsDekuOnBack = shouldShowDeku;
    if (!shouldShowDeku) {
        ResourceMgr_UnpatchGfxByName(sHeroShieldOnBackDL, "ootEquipmentDekuBack");
        ResourceMgr_UnpatchGfxByName(sHeroShieldOnBackDL, "ootEquipmentDekuBackEnd");
        return;
    }
    Gfx deku = gsSPDisplayListOTRFilePath(sDekuShieldOnBackDL);
    Gfx end = gsSPEndDisplayList();
    ResourceMgr_PatchGfxByName(sHeroShieldOnBackDL, "ootEquipmentDekuBack", 0, deku);
    ResourceMgr_PatchGfxByName(sHeroShieldOnBackDL, "ootEquipmentDekuBackEnd", 1, end);
}

// MM's own mirror on the back is emptied while the OoT one is drawn at the sheath limb in its place.
static void UpdateMmMirrorOnBack(void) {
    static bool sIsHidden;
    const bool shouldHide = IsOotMirrorOnBack();
    if (shouldHide == sIsHidden) {
        return;
    }
    sIsHidden = shouldHide;
    if (!shouldHide) {
        ResourceMgr_UnpatchGfxByName(sMmMirrorShieldOnBackDL, "ootEquipmentMirrorBackHide");
        return;
    }
    Gfx end = gsSPEndDisplayList();
    ResourceMgr_PatchGfxByName(sMmMirrorShieldOnBackDL, "ootEquipmentMirrorBackHide", 0, end);
}

static void UpdateShieldOnBack(void) {
    UpdateDekuShieldOnBack();
    UpdateMmMirrorOnBack();
}

static bool IsHoldingKey(Player* player, const char* key) {
    const char* held = player->heldItemAction == PLAYER_IA_CUSTOM ? CustomItemRegistry_GetHeldKey() : NULL;
    return held != NULL && strcmp(held, key) == 0;
}

// Before the hook, MM's sheath post-limb moves the matrix onto the back shield (630, 100, -30, Z 0x7FFF) unless the
// shield is in hand; the inverse brings it back to the sheath limb itself.
static void ReturnToSheathLimbMatrix(Player* player) {
    if (player->actor.scale.y < 0.0f || player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD ||
        player->rightHandType == PLAYER_MODELTYPE_RH_FF) {
        return;
    }
    Matrix_RotateZS(-0x7FFF, MTXMODE_APPLY);
    Matrix_Translate(-630.0f, -100.0f, 30.0f, MTXMODE_APPLY);
}

// Only adult models exist for these, so they are shrunk to child Link's size on his own sheath limb.
static void DrawAdultModelAtChildScale(Player* player, const char* model) {
    OPEN_DISPS(gPlayState->state.gfxCtx);
    Matrix_Push();
    ReturnToSheathLimbMatrix(player);
    Matrix_Scale(ADULT_TO_CHILD_SCALE, ADULT_TO_CHILD_SCALE, ADULT_TO_CHILD_SCALE, MTXMODE_APPLY);
    gDPPipeSync(POLY_OPA_DISP++);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gPlayState->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)model);
    Matrix_Pop();
    CLOSE_DISPS(gPlayState->state.gfxCtx);
}

static void DrawMasterSwordSheath(Player* player, int32_t limbIndex) {
    if (!IsLocalHumanLink(player) || limbIndex != PLAYER_LIMB_SHEATH || !CustomEquipRegistry_IsWorn(MASTER_SWORD_KEY)) {
        return;
    }
    const char* model = IsHoldingKey(player, MASTER_SWORD_KEY) ? sEmptySheathDL : sMasterSwordSheathedDL;
    if (sApi->HasResource(model)) {
        DrawAdultModelAtChildScale(player, model);
    }
}

// Same gate as MM's own shield on the back (Player_PostLimbDrawGameplay, sheath limb).
static void DrawMirrorShieldOnBack(Player* player, int32_t limbIndex) {
    if (!IsLocalHumanLink(player) || limbIndex != PLAYER_LIMB_SHEATH || player->currentShield == PLAYER_SHIELD_NONE ||
        (player->sheathType != PLAYER_MODELTYPE_SHEATH_14 && player->sheathType != PLAYER_MODELTYPE_SHEATH_15) ||
        !IsOotMirrorOnBack()) {
        return;
    }
    DrawAdultModelAtChildScale(player, sMirrorShieldOnBackDL);
}

// The child-only Master Sword hand is the pedestal grip, blade backwards; the adult one points it forward.
static Gfx* GetHeldSwordModel(Player* player) {
    static HeldAtChildScale sMasterHeld;
    if (IsHoldingKey(player, MASTER_SWORD_KEY) && PatchMasterSwordHand()) {
        return GetHeldAtChildScale(&sMasterHeld, sMasterSwordHandDL, sHumanLeftFistDL);
    }
    if ((IsHoldingKey(player, BIGGORON_SWORD_KEY) || IsHoldingKey(player, GIANTS_KNIFE_KEY)) && PatchBgsHand()) {
        return (Gfx*)sBgsHandDL;
    }
    return NULL;
}

static void DrawOotWeapons(Player* player, int32_t limbIndex, Gfx** displayList, Gfx* original, Vec3f* position) {
    if (!IsLocalHumanLink(player)) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_LEFT_HAND) {
        Gfx* sword = GetHeldSwordModel(player);
        if (sword != NULL) {
            *displayList = sword;
        }
    } else if (limbIndex == PLAYER_LIMB_RIGHT_HAND && player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD) {
        Gfx* shield = GetShieldInHandModel();
        if (shield != NULL) {
            *displayList = shield;
        }
    } else if (limbIndex == PLAYER_LIMB_SHEATH && CustomEquipRegistry_IsWorn(MASTER_SWORD_KEY) &&
               sApi->HasResource(sMasterSwordSheathedDL)) {
        // Not NULL: MM only draws the shield on Link's back while the sheath limb has a list.
        *displayList = sHiddenLimbDL;
    }
}

// Native post-limb code resets the collider material from the Hero/Mirror base.
// Apply the wooden material afterwards, without changing native reflection rules.
static void SetDekuShieldMaterial(Player* player, int32_t limbIndex) {
    if (gPlayState && player == GET_PLAYER(gPlayState) &&
        player->transformation == PLAYER_FORM_HUMAN &&
        CustomEquipRegistry_IsWorn(DEKU_SHIELD_KEY)) {
        player->shieldQuad.base.colMaterial = COL_MATERIAL_WOOD;
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

// OoT's pause Link quad (equipVtx at page x -64, top y 50, 64x112) in screen space, which is page space + (160, 120).
static void ShowLinkInPanel(PauseContext* pauseCtx, u16 pauseIndex) {
    static const S2HPlayerPreview sPanel = { sizeof(S2HPlayerPreview), 96, 70, 64, 112 };
    if (gPlayState == NULL || pauseIndex != PAUSE_MASK || pauseIndex != pauseCtx->pageIndex ||
        !S2H_MOD_API_HAS(sApi, DrawPlayerPreview)) {
        return;
    }
    const char* shown = sApi->GetShownLayoutPage();
    if (shown != NULL && strcmp(shown, OOT_EQUIPMENT_PAGE_KEY) == 0) {
        sApi->DrawPlayerPreview(gPlayState, &sPanel);
    }
}

static void PlaceEquipment(void) {
    static uint32_t knownCount;
    uint32_t count = CustomEquipRegistry_GetCount();
    if (knownCount == count) return;
    for (uint32_t i = 0; i < count; ++i) {
        const S2HCustomEquipDefinition* piece = CustomEquipRegistry_GetAt(i);
        if ((strncmp(piece->key, "oot.equip.", 10) && strcmp(piece->key, "nei.equip.giants_knife")) || piece->row >= 4) continue;
        uint8_t column = piece->column > 3 ? 3 : piece->column;
        sApi->PlaceLayoutItem(piece->key, OOT_EQUIPMENT_PAGE_KEY, piece->row * 4 + column);
    }
    knownCount = count;
}

S2H_MOD_EXPORT void ModInit(void) {
    const S2HLayoutPage page = { sizeof(S2HLayoutPage), OOT_EQUIPMENT_PAGE_KEY, "OoT Equipment",
        S2H_LAYOUT_EQUIPMENT, 4, 4, 16, sEquipmentCells, sEquipmentBackground,
        S2H_ITEM_PAGE_EQUIPMENT_FIRST + 1, S2H_LAYOUT_FIXED_PAGE | S2H_LAYOUT_EQUIP_CONTROLS, sCursorCorners };
    if (sApi->RegisterLayoutPage(&page)) {
        S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, PlaceEquipment);
        S2H_REGISTER_HOOK(sApi, AfterKaleidoDrawPage, ShowLinkInPanel);
    }
    for (u32 i = 0; i < ARRAY_COUNT(sPieces); i++) {
        RegisterPiece(&sPieces[i]);
    }
    Z64Equipment_Install(sApi);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, SetDekuShieldMaterial);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawOotBoots);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, DrawOotWeapons);
    S2H_REGISTER_HOOK(sApi, OnLoadFile, OwnStartingPieces);
    S2H_REGISTER_HOOK(sApi, OnSaveInit, ApplyStartingEquipment);
    S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, SyncStandIns);
    S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, UpdateShieldOnBack);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawMasterSwordSheath);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawMirrorShieldOnBack);
    RegisterMenu();
    GrantEquipmentLogic();
    S2H_REGISTER_HOOK(sApi, OnCollisionResolveDamage, GuardLikeGoron);
    sApi->RegisterVB(VB_APPLY_TUNIC_COLOR, TintOotTunic);
}
