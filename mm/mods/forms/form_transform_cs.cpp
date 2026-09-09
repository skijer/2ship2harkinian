/*
 * form_transform_cs.cpp - MM's mask transformation cutscene (func_808388B8 -> Player_Action_86 ->
 * actor re-init -> Player_Action_87) reused by the custom forms. A form only overrides the Human
 * slot, so gSaveContext.save.playerForm never changes and its toggle must land at the flash peak.
 */
#include "forms_internal.h"

extern "C" {
void func_808388B8(PlayState* play, Player* player, PlayerTransformation playerForm);
void Player_Action_86(Player* player, PlayState* play);
// Worn-mask display lists, indexed by PlayerMask - 1 (z_player_lib.c). OTR path strings in this port.
extern const char* D_801C0B20[];
}

// The Gerudo has no MM mask: hers is OoT's face-fitted one, from the companion oot.o2r.
#define GERUDO_MASK_DL_CHILD "__OTR__objects/object_link_child/gLinkChildGerudoMaskDL"
#define GERUDO_MASK_DL_ADULT "__OTR__objects/object_link_boy/gLinkAdultGerudoMaskDL"

// Frame of cl_setmask from which MM starts drawing the mask on the face.
#define TRANSFORM_MASK_FIRST_FRAME 12.0f

static const s32 sFormMaskId[CUSTOM_FORM_MAX] = {
    PLAYER_MASK_KAFEIS_MASK, // CUSTOM_FORM_KAFEI
    PLAYER_MASK_KEATON,      // CUSTOM_FORM_KEATON
    PLAYER_MASK_NONE,        // CUSTOM_FORM_GERUDO
    PLAYER_MASK_GARO,        // CUSTOM_FORM_GARO
};

static s32 sPendingForm = CUSTOM_FORM_NONE;
static u8 sHasPending = 0;
static s32 sMaskForm = CUSTOM_FORM_NONE;
static u8 sCsActive = 0;

static Gfx* GetTransformMaskDL(s32 form) {
    if (form == CUSTOM_FORM_GERUDO) {
        static Gfx* sGerudoMaskDL[2] = { NULL, NULL };
        u8 adult = Forms_IsAdultAge();

        if (sGerudoMaskDL[adult] == NULL) {
            sGerudoMaskDL[adult] = (Gfx*)OotAssets_LoadGfxDirect(adult ? GERUDO_MASK_DL_ADULT : GERUDO_MASK_DL_CHILD);
        }
        return sGerudoMaskDL[adult];
    }
    if (sFormMaskId[form] == PLAYER_MASK_NONE) {
        return NULL;
    }
    return (Gfx*)D_801C0B20[sFormMaskId[form] - 1];
}

// The transformation ends with the actor re-inited: a leftover prevMask would play cl_maskoff there
// instead of the cutscene's own exit, and the vanilla mask under the form is gone anyway.
static void StartTransformCs(PlayState* play, Player* player, s32 targetForm) {
    sPendingForm = targetForm;
    sMaskForm = (targetForm != CUSTOM_FORM_NONE) ? targetForm : CustomForms_WornForm();
    sHasPending = 1;
    sCsActive = 1;
    player->prevMask = PLAYER_MASK_NONE;
    func_808388B8(play, player, PLAYER_FORM_HUMAN);
    SPDLOG_INFO("[CustomForms] transformation CS -> form {}", targetForm);
}

// Player_ActionHandler_13's mask branch, where MM picks between the short mask anim and this
// cutscene. Reached because CustomForms_ConsidersLinkHuman answered false for this mask.
extern "C" u8 CustomForms_StartMaskTransform(PlayState* play, Player* player, s32 maskId) {
    s32 form = CustomForms_FormForMask(maskId);

    // Transformed, this branch is MM's own way back to Human — let it run and take the form on the
    // next press, or the form would be worn under a Deku/Goron/Zora body.
    if ((form == CUSTOM_FORM_NONE) || (player->transformation != PLAYER_FORM_HUMAN)) {
        return 0;
    }
    StartTransformCs(play, player, (CustomForms_WornForm() == form) ? CUSTOM_FORM_NONE : form);
    return 1;
}

// The Gerudo has no MM item id, so her press arrives at the top of Player_UseItem with none of the
// state MM validates for a mask: keep the instant toggle for the cases the cutscene cannot run in.
extern "C" void CustomForms_StartFormTransform(PlayState* play, Player* player, s32 targetForm) {
    if (!Forms_CanAct(player) || !Forms_OnGround(player)) {
        CustomForms_SetActive(targetForm);
        func_8082E1F0(player, NA_SE_PL_CHANGE_ARMS);
        return;
    }
    StartTransformCs(play, player, targetForm);
}

// Player_Action_86 calls this at the flash peak, in place of its OOB-for-Human week-event stamp.
extern "C" u8 CustomForms_ApplyPendingTransform(void) {
    if (!sHasPending) {
        return 0;
    }
    CustomForms_SetActive(sPendingForm);
    sHasPending = 0;
    sCsActive = 0;
    return 1;
}

// Death, a scene cutscene or a void out can tear the player out of Player_Action_86 mid-way.
extern "C" void CustomForms_TickTransform(Player* player, PlayState* play) {
    if (!sCsActive || (player->actionFunc == Player_Action_86) || (player->actor.update == func_8012301C)) {
        return;
    }
    CustomForms_SetActive(sPendingForm);
    sHasPending = 0;
    sCsActive = 0;
}

extern "C" u8 CustomForms_TransformCsActive(void) {
    return sCsActive;
}

// Head limb, on MM's own gate and squash (func_80855218 ramps unk_B10[2]/[3] as the hands press).
extern "C" void CustomForms_DrawTransformMask(PlayState* play, Player* player) {
    if (!sCsActive || (sMaskForm == CUSTOM_FORM_NONE) || (player->skelAnime.curFrame < TRANSFORM_MASK_FIRST_FRAME)) {
        return;
    }

    Gfx* maskDL = GetTransformMaskDL(sMaskForm);

    if (maskDL == NULL) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Matrix_Scale(1.0f, 1.0f - player->unk_B10[3], 1.0f - player->unk_B10[2], MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    Matrix_Pop();

    gSPDisplayList(POLY_OPA_DISP++, maskDL);

    CLOSE_DISPS(play->state.gfxCtx);
}
