// MM's pause hooks select the bow element without fixed inventory positions.
#include "sw97_mod.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"

static const ALIGN_ASSET(2) char sIcons[SW97_ELEM_COUNT][80] = {
    "__OTR__icon_item_static_yar/gItemIconBowTex",
    "__OTR__textures/icon_item_custom/gItemIconMedallionFireTex",
    "__OTR__textures/icon_item_custom/gItemIconMedallionWaterTex",
    "__OTR__textures/icon_item_custom/gItemIconMedallionLightTex",
    "__OTR__textures/icon_item_custom/gItemIconMedallionShadowTex",
    "__OTR__textures/icon_item_custom/gItemIconMedallionSpiritTex",
    "__OTR__textures/icon_item_custom/gItemIconMedallionForestTex",
};
static const ALIGN_ASSET(2) char sBowIcons[SW97_ELEM_COUNT][80] = {
    "__OTR__icon_item_static_yar/gItemIconBowTex",
    "__OTR__textures/icon_item_custom/gItemIconBowSw97FireTex",
    "__OTR__textures/icon_item_custom/gItemIconBowSw97IceTex",
    "__OTR__textures/icon_item_custom/gItemIconBowSw97LightTex",
    "__OTR__textures/icon_item_custom/gItemIconBowSw97DarkTex",
    "__OTR__textures/icon_item_custom/gItemIconBowSw97SoulTex",
    "__OTR__textures/icon_item_custom/gItemIconBowSw97WindTex",
};

static void UpdateButtonIcons(void) {
    if (gPlayState == NULL) return;
    Sw97Element element = Sw97_GetElement(false);
    for (u8 button = 0; button < EQUIP_SLOT_MAX; button++) {
        if (GET_CUR_FORM_BTN_ITEM(button) == ITEM_BOW) gPlayState->interfaceCtx.iconItemSegment[button] = (char*)sBowIcons[element];
    }
    for (u8 button = 0; button < 4; button++) {
        if (DPAD_GET_CUR_FORM_BTN_ITEM(button) == ITEM_BOW) gPlayState->interfaceCtx.iconItemSegment[DPAD_BUTTON(button) + EQUIP_SLOT_MAX] = (char*)sBowIcons[element];
    }
}

static void DrawElements(PauseContext* pause, u16 page) {
    PlayState* play = gPlayState;
    if (play == NULL || page != PAUSE_ITEM || pause->pageIndex != PAUSE_ITEM ||
        pause->state != PAUSE_STATE_MAIN || pause->cursorSpecialPos != 0 ||
        pause->cursorItem[PAUSE_ITEM] != ITEM_BOW) return;
    static u32 lastSelectionFrame = (u32)-1;
    if ((play->state.input[0].press.button & BTN_A) && lastSelectionFrame != play->state.frames) {
        lastSelectionFrame = play->state.frames;
        /* Selection is handled by the pause A action below, not the C-Up description hook. */
        Sw97Element current=Sw97_GetElement(false);
        u8 count=Sw97_ElementCount();
        for(u8 i=0;i<count;i++) if(Sw97_ElementAt(i)==current) {
            Sw97_SetElement(false,Sw97_ElementAt((i+1)%count));
            Audio_PlaySfx(NA_SE_SY_CURSOR);break;
        }
    }
    s32 cell = pause->cursorPoint[PAUSE_ITEM];
    if (cell < 0 || cell >= ITEM_NUM_SLOTS) return;
    const s16 x = pause->itemVtx[cell * 4].v.ob[0];
    const s16 y = pause->itemVtx[cell * 4].v.ob[1];
    const u8 count = Sw97_ElementCount();
    OPEN_DISPS(play->state.gfxCtx);
    for (u8 i = 0; i < count; i++) {
        Sw97Element element = Sw97_ElementAt(i);
        Vtx* vertices = GRAPH_ALLOC(play->state.gfxCtx, 4 * sizeof(Vtx));
        s16 left = x + (i - count / 2) * 18;
        s16 top = y - 35;
        Vtx quad[4] = { VTX(left, top, 0, 0, 0, 255, 255, 255, 255), VTX(left + 16, top, 0, 32 << 5, 0, 255, 255, 255, 255),
            VTX(left, top - 16, 0, 0, 32 << 5, 255, 255, 255, 255), VTX(left + 16, top - 16, 0, 32 << 5, 32 << 5, 255, 255, 255, 255) };
        for (u8 corner = 0; corner < 4; corner++) vertices[corner] = quad[corner];
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, element == Sw97_GetElement(false) ? pause->alpha : pause->alpha / 2);
        gSPVertex(POLY_OPA_DISP++, (uintptr_t)vertices, 4, 0);
        KaleidoScope_DrawTexQuadRGBA32(play->state.gfxCtx, (char*)sIcons[element], 32, 32, 0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void Sw97_RegisterArrowUi(void) {

    S2H_REGISTER_HOOK(gSw97Api, OnInterfaceDrawStart, UpdateButtonIcons);
    S2H_REGISTER_HOOK(gSw97Api, AfterKaleidoDrawPage, DrawElements);
}
