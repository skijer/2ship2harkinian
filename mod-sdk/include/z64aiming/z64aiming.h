#ifndef S2H_Z64AIMING_H
#define S2H_Z64AIMING_H

// Shipwright's z64aiming API on Majora's Mask: the first-person camera and the native aiming rotation stand in for
// OoT's Player_Action first-person handlers. The host keeps Link still while a custom item is aimed.
#include "2s2h/ModApi/ModApi.h"
#include "align_asset_macro.h"
#include <libultraship/bridge/consolevariablebridge.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "functions.h"
#include "macros.h"
void Player_UpdateCustomAimingRotation(PlayState* play, Player* player, Input* input);
Gfx* ResourceMgr_LoadGfxByName(const char* path);
#ifdef __cplusplus
}
#endif

void Player_StopHorizontalMovement(Player* player);

static bool sZ64Aiming = false;
static s16 sZ64AimingPreviousSetting = CAM_SET_NORMAL0;
static bool sZ64AimingWasFirstPerson;
static s16 sZ64AimingYaw;
static s16 sZ64AimingPitch;
static u32 sZ64AimingFrame = UINT32_MAX;

typedef enum {
    Z64AIMING_TOGGLE_HOOKSHOT,
    Z64AIMING_TOGGLE_BOW,
    Z64AIMING_TOGGLE_BOOMERANG,
} Z64AimingReticleToggle;

typedef enum {
    Z64AIMING_TINT_LAUNCH,
    Z64AIMING_TINT_CONTROL,
    Z64AIMING_TINT_ATTRACT,
    Z64AIMING_TINT_MAX,
} Z64AimingReticleTint;

typedef struct {
    Z64AimingReticleToggle toggle;
    Z64AimingReticleTint tint;
} Z64AimingReticleStyle;

static const u8 sZ64AimingTintColors[Z64AIMING_TINT_MAX][3] = {
    { 255, 0, 0 },
    { 0, 255, 0 },
    { 0, 120, 255 },
};

static inline void Z64Aiming_HoldState(Player* player) {
    player->stateFlags1 |= PLAYER_STATE1_100000;
}

static inline void Z64AimingImpl_HoldCamera(PlayState* play) {
    Camera_ChangeMode(Play_GetCamera(play, CAM_ID_MAIN), CAM_MODE_FIRSTPERSON);
}

static inline bool Z64Aiming_Request(Player* player, PlayState* play) {
    if (player == NULL || play == NULL) {
        return false;
    }
    if (!sZ64Aiming) {
        sZ64AimingPreviousSetting = Play_GetCamera(play, CAM_ID_MAIN)->setting;
        sZ64AimingWasFirstPerson = (player->stateFlags1 & PLAYER_STATE1_100000) != 0;
        sZ64AimingYaw = player->actor.shape.rot.y;
        sZ64AimingPitch = 0;
        sZ64AimingFrame = UINT32_MAX;
    }
    sZ64Aiming = true;
    player->actor.focus.rot.x = 0;
    player->actor.focus.rot.y = player->actor.shape.rot.y;
    Z64Aiming_HoldState(player);
    Player_StopHorizontalMovement(player);
    Z64AimingImpl_HoldCamera(play);
    Audio_PlaySfx(NA_SE_SY_CAMERA_ZOOM_UP);
    return true;
}

// Same inversion, sensitivity, gyro and limits as the native bow; once per frame however often it is called.
static inline void Z64Aiming_Update(Player* player, PlayState* play) {
    if (!sZ64Aiming || player == NULL || play == NULL) {
        return;
    }
    if (sZ64AimingFrame != play->state.frames) {
        player->actor.focus.rot.x = sZ64AimingPitch;
        player->actor.focus.rot.y = sZ64AimingYaw;
        Player_UpdateCustomAimingRotation(play, player, &play->state.input[0]);
        sZ64AimingPitch = player->actor.focus.rot.x;
        sZ64AimingYaw = player->actor.focus.rot.y;
        sZ64AimingFrame = play->state.frames;
    }
    // The subject camera reads focus after Player_UpdateCommon, which would otherwise reset it.
    player->actor.focus.rot.x = sZ64AimingPitch;
    player->actor.focus.rot.y = sZ64AimingYaw;
    Z64Aiming_HoldState(player);
    Player_StopHorizontalMovement(player);
    Z64AimingImpl_HoldCamera(play);
}

static inline void Z64Aiming_Release(Player* player, PlayState* play) {
    if (!sZ64Aiming) {
        return;
    }
    sZ64Aiming = false;
    if (player == NULL || play == NULL) {
        return;
    }
    if (!sZ64AimingWasFirstPerson) {
        player->stateFlags1 &= ~PLAYER_STATE1_100000;
    }
    Camera_ChangeSetting(Play_GetCamera(play, CAM_ID_MAIN), sZ64AimingPreviousSetting);
    Camera_ChangeMode(Play_GetCamera(play, CAM_ID_MAIN), CAM_MODE_NORMAL);
    Audio_PlaySfx(NA_SE_SY_CAMERA_ZOOM_DOWN);
}

static inline void Z64Aiming_Drop(void) {
    sZ64Aiming = false;
}

static inline bool Z64Aiming_IsAiming(void) {
    return sZ64Aiming;
}

// Positive pitch looks down, matching a projectile's y -= sin(pitch).
static inline void Z64Aiming_GetDirection(Player* player, s16* yaw, s16* pitch) {
    if (player == NULL) {
        return;
    }
    if (yaw != NULL) {
        *yaw = player->actor.focus.rot.y;
    }
    if (pitch != NULL) {
        *pitch = player->actor.focus.rot.x;
    }
}

// The hookshot's reticle has no toggle; MM has a single Bow Reticle option, which the boomerang style follows too.
static inline bool Z64Aiming_IsReticleEnabled(Z64AimingReticleToggle toggle) {
    switch (toggle) {
        case Z64AIMING_TOGGLE_BOW:
        case Z64AIMING_TOGGLE_BOOMERANG:
            return CVarGetInteger("gEnhancements.Graphics.BowReticle", 0) != 0;
        case Z64AIMING_TOGGLE_HOOKSHOT:
        default:
            return true;
    }
}

static const ALIGN_ASSET(2) char sZ64AimingReticleDL[] = "__OTR__objects/gameplay_keep/gHookshotReticleDL";

#define Z64AIMING_RETICLE_DL_MAX 32
#define Z64AIMING_RETICLE_VANILLA_PRIM 0xFF0000FF

// gHookshotReticleDL sets its own red; a copy without that command takes the style's tint.
static inline Gfx* Z64AimingImpl_GetUntintedReticleDL(void) {
    static Gfx sUntinted[Z64AIMING_RETICLE_DL_MAX];
    static bool sIsCopied = false;

    if (sIsCopied) {
        return sUntinted;
    }
    Gfx* vanilla = ResourceMgr_LoadGfxByName(sZ64AimingReticleDL);
    if (vanilla == NULL) {
        return NULL;
    }
    for (s32 i = 0; i < Z64AIMING_RETICLE_DL_MAX; i++) {
        u32 opcode = (u32)(vanilla[i].words.w0 >> 24) & 0xFF;

        sUntinted[i] = vanilla[i];
        if (opcode == G_SETPRIMCOLOR && (u32)vanilla[i].words.w1 == Z64AIMING_RETICLE_VANILLA_PRIM) {
            gDPNoOp(&sUntinted[i]);
        }
        if (opcode == G_ENDDL && vanilla[i].words.w1 == 0) {
            sIsCopied = true;
            return sUntinted;
        }
    }
    return NULL;
}

// Shown only on a surface within range, scaled like Player_DrawHookshotReticle.
static inline void Z64Aiming_DrawReticle(PlayState* play, Player* player, f32 range, Z64AimingReticleStyle style) {
    if (play == NULL || player == NULL || !Z64Aiming_IsReticleEnabled(style.toggle)) {
        return;
    }
    Gfx* reticleDL = Z64AimingImpl_GetUntintedReticleDL();
    if (reticleDL == NULL) {
        return;
    }
    s16 yaw;
    s16 pitch;
    Z64Aiming_GetDirection(player, &yaw, &pitch);
    f32 cosPitch = Math_CosS(pitch);
    Vec3f start = player->actor.focus.pos;
    Vec3f end = { start.x + Math_SinS(yaw) * cosPitch * range, start.y - Math_SinS(pitch) * range,
                  start.z + Math_CosS(yaw) * cosPitch * range };
    Vec3f hit;
    CollisionPoly* poly;
    s32 bgId;
    if (!BgCheck_AnyLineTest3(&play->colCtx, &start, &end, &hit, &poly, true, true, true, true, &bgId)) {
        return;
    }
    Vec3f projected;
    f32 depth;
    SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &hit, &projected, &depth);
    f32 scale = (depth < 200.0f) ? 0.08f : (depth / 200.0f) * 0.08f;
    const u8* color = sZ64AimingTintColors[style.tint % Z64AIMING_TINT_MAX];

    OPEN_DISPS(play->state.gfxCtx);
    OVERLAY_DISP = Gfx_SetupDL(OVERLAY_DISP, SETUPDL_7);
    Matrix_Push();
    Matrix_Translate(hit.x, hit.y, hit.z, MTXMODE_NEW);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(OVERLAY_DISP++, play->state.gfxCtx);
    Matrix_Pop();
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, color[0], color[1], color[2], 255);
    gSPDisplayList(OVERLAY_DISP++, reticleDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

#endif
