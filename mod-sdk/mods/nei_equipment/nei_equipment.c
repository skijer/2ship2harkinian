#include <string.h>
#include "z64items.h"
#include "2s2h/ModApi/Layout/ModLayout.h"
#include "2s2h/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include "2s2h/BenPort.h"
#include "variables.h"

static const char* const sCursorCorners[4] = {
    "__OTR__textures/nei_equipment/gPauseMenuCursorTopLeftTex",
    "__OTR__textures/nei_equipment/gPauseMenuCursorTopRightTex",
    "__OTR__textures/nei_equipment/gPauseMenuCursorBottomLeftTex",
    "__OTR__textures/nei_equipment/gPauseMenuCursorBottomRightTex",
};
static const S2HModApi* sApi;
static uint32_t sKnownCount;
#define NEI_EQUIPMENT_PAGE_KEY "nei.equipment"
static const char* const sHooks[] = { "OnGameFrameUpdate", "OnInterfaceResolveButtonIcon", "AfterKaleidoDrawPage" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sHooks, sizeof(sHooks) / sizeof(sHooks[0]) };
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
    "__OTR__textures/nei_equipment/gPauseEquipment10ENGTex",
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

static void PlaceEquipment(void) {
    uint32_t count = CustomEquipRegistry_GetCount();
    if (sKnownCount == count) return;
    for (uint32_t i = 0; i < count; ++i) {
        const S2HCustomEquipDefinition* piece = CustomEquipRegistry_GetAt(i);
        if (strncmp(piece->key, "nei.equip.", 10) || piece->row >= 4 || piece->column >= 4) continue;
        sApi->PlaceLayoutItem(piece->key, NEI_EQUIPMENT_PAGE_KEY, piece->row * 4 + piece->column);
    }
    sKnownCount = count;
}
// The native sword value is an animation/behavior base, not the equipment's identity.
// Resolve at draw time so taking off gear, changing forms and temporary B items
// immediately return to their own icons without rewriting the native icon cache.
static void ResolveEquipmentButtonIcon(PlayState* play, uint8_t button, uint16_t item, const char** texture) {
    if (!play || !texture || button != S2H_ITEM_BUTTON_B) return;
    const char* key = NULL;
    if (item == ITEM_CUSTOM) {
        key = CustomItemRegistry_GetEquippedKey(button);
    } else if (GET_PLAYER(play)->transformation == PLAYER_FORM_HUMAN &&
               item >= ITEM_SWORD_KOKIRI && item <= ITEM_SWORD_GILDED) {
        key = CustomEquipRegistry_GetWorn(S2H_EQUIP_SLOT_SWORD);
    }
    const char* icon = CustomItemRegistry_ResolveTexture(key, S2H_ITEM_ICON_B_BUTTON);
    if (icon && ResourceMgr_FileExists(icon)) *texture = icon;
}

// OoT's pause Link quad (equipVtx at page x -64, top y 50, 64x112) in screen space, which is page space + (160, 120).
static void ShowLinkInPanel(PauseContext* pauseCtx, u16 pauseIndex) {
    static const S2HPlayerPreview sPanel = { sizeof(S2HPlayerPreview), 96, 70, 64, 112 };
    if (gPlayState == NULL || pauseIndex != PAUSE_MASK || pauseIndex != pauseCtx->pageIndex ||
        !S2H_MOD_API_HAS(sApi, DrawPlayerPreview)) {
        return;
    }
    const char* shown = sApi->GetShownLayoutPage();
    if (shown != NULL && strcmp(shown, NEI_EQUIPMENT_PAGE_KEY) == 0) {
        sApi->DrawPlayerPreview(gPlayState, &sPanel);
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) { sApi = api; }
S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) { return &sRequirements; }
S2H_MOD_EXPORT void ModInit(void) {
    const S2HLayoutPage page = { sizeof(S2HLayoutPage), NEI_EQUIPMENT_PAGE_KEY, "NEI Equipment",
        S2H_LAYOUT_EQUIPMENT, 4, 4, 16, sEquipmentCells, sEquipmentBackground,
        S2H_ITEM_PAGE_EQUIPMENT_FIRST, S2H_LAYOUT_FIXED_PAGE | S2H_LAYOUT_EQUIP_CONTROLS, sCursorCorners };
    S2H_REGISTER_HOOK(sApi, OnInterfaceResolveButtonIcon, ResolveEquipmentButtonIcon);
    if (!sApi->RegisterLayoutPage(&page)) return;
    S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, PlaceEquipment);
    S2H_REGISTER_HOOK(sApi, AfterKaleidoDrawPage, ShowLinkInPanel);
}
