#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "regs.h"
#include "2s2h/ModApi/Player/PlayerHookTypes.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_dy_obj/object_dy_obj.h"
#include "objects/object_link_child/object_link_child.h"
#include "overlays/actors/ovl_Boss_01/z_boss_01.h"
#include "overlays/actors/ovl_En_Okuta/z_en_okuta.h"

#define BYRNA_KEY "nei.equip.byrna"
#define BYRNA_VISUAL_CHANNEL "nei.byrna.visual"
#define BYRNA_SAMPLES 4
#define BYRNA_PLAY_SPEED 2.1f
#define BYRNA_NORMAL_DAMAGE 1
#define BYRNA_BOSS_DAMAGE 4
#define BYRNA_AIR_GRAVITY (-0.4f)
#define BYRNA_NORMAL_GRAVITY (-1.2f)
#define BYRNA_AIR_SPEED 18.0f
#define BYRNA_REBOUND_IMPULSE 6.0f
#define BYRNA_CYL_SEGMENTS 12
#define BYRNA_CYL_TRIS (BYRNA_CYL_SEGMENTS * 4)
// 3 root translation values, 21 limb rotations and the face word: the raw layout of a player animation resource.
#define ANIM_VALUES_PER_FRAME 67
#define CANE_POS_X (-887.74f)
#define CANE_POS_Y 988.20f
#define CANE_POS_Z 54.18f
#define CANE_ROT_X 1.0f
#define CANE_ROT_Z 49.7f
#define CANE_SCALE_X 7.76f
#define CANE_SCALE_Y 10.45f
#define CANE_SCALE_Z 7.71f
#define CANE_AXIS_MIN (-416.0f)
#define CANE_AXIS_MAX 223.0f
#define DEG_TO_BINANG 182.04f
#define REMOTE_VISUAL_INTERVAL 5u

typedef enum {
    CLIP_SHEATHE,
    CLIP_DRAW,
    CLIP_DASH,
    CLIP_SLASH,
    CLIP_AIR_B,
    CLIP_AIR_A,
    CLIP_REBOUND,
    CLIP_LAUNCH,
    CLIP_POUND,
    CLIP_HOP_RIGHT,
    CLIP_HOP_LEFT,
    CLIP_AIR_IDLE,
    CLIP_IDLE,
    CLIP_BACKFLIP,
    CLIP_WALK,
    CLIP_RUN,
    CLIP_MAX
} ClipId;

typedef struct {
    const char* path;
    bool ground;
} ClipSource;

#define BYRNA_CLIP(name) "__OTR__misc/link_animetion/nei_byrna_" name

static const ClipSource sClipSources[CLIP_MAX] = {
    { BYRNA_CLIP("ForwardMultiHitAerialStaffStrike"), true },
    { BYRNA_CLIP("ForwardRisingDoubleChargedStaffCombo"), true },
    { BYRNA_CLIP("ForwardRisingMultiHitAdvancingStaffSweep_Variant10"), false },
    { BYRNA_CLIP("ForwardRisingTripleAerialStaffStrike_Variant12"), true },
    { BYRNA_CLIP("LeftHighAerialMultiHitSilkbindStaffStrike"), false },
    { BYRNA_CLIP("ForwardSingleStaffTransition_Variant05"), false },
    { BYRNA_CLIP("LeftRisingDoubleChargedStaffCombo"), false },
    { BYRNA_CLIP("ForwardHighAerialTripleSilkbindStaffStrike_Variant12"), false },
    { BYRNA_CLIP("ForwardHighAerialMultiHitSilkbindStaffStrike_Variant20"), false },
    { BYRNA_CLIP("LeftMultiHitSilkbindVault"), false },
    { BYRNA_CLIP("RightRisingTripleSilkbindVault"), false },
    { BYRNA_CLIP("StationaryStaffReadyIdle_Variant02"), false },
    { BYRNA_CLIP("StationaryStaffReadyIdle_Variant14"), true },
    { BYRNA_CLIP("BackwardHighAerialSingleSilkbindStaffStrike"), false },
    { BYRNA_CLIP("ForwardStaffRun"), true },
    { BYRNA_CLIP("ForwardStaffRun"), true },
};

#undef BYRNA_CLIP

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconCaneOfByrnaTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gCaneOfByrnaNameTex";
static const ALIGN_ASSET(2) char sCaneDL[] = "__OTR__objects/object_byrna/g_byrna_cane_dl";
static const ALIGN_ASSET(2) char sCaneGiveDL[] = "__OTR__objects/object_byrna/g_byrna_cane_give_dl";
static const S2HModApi* sApi;

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
PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
s32 Player_SetAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_8083B1A0(Player* player, PlayState* play);
void func_80123C58(Player* player);
bool Player_IsZTargeting(Player* player);
PlayerItemAction Player_ItemToItemAction(Player* player, ItemId item);
static void ApplyCaneMatrix(void);
#include "byrna_runtime.inc.c"
static void StunByrnaBoss(Actor* actor);
#include "byrna_damage.inc.c"
#include "byrna_boss_stun.inc.c"

static void WieldWithBothHands(Player* player, s32 itemAction, s32* modelGroup) {
    if (IsWorn() && IsSwordAction(itemAction)) {
        *modelGroup = PLAYER_MODELGROUP_TWO_HAND_SWORD;
    }
}

static void GripCane(Player* player, s32 limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (limbIndex != PLAYER_LIMB_LEFT_HAND || gPlayState == NULL) {
        return;
    }
    if (player != GET_PLAYER(gPlayState)) {
        RemoteVisualState visual;
        if (ReadRemoteVisual(player, &visual)) {
            *dList = (Gfx*)gLinkHumanLeftHandOpenDL;
        }
        return;
    }
    if (IsWielding(player)) {
        *dList = (Gfx*)gLinkHumanLeftHandOpenDL;
    }
}

static void ApplyCaneMatrix(void) {
    Matrix_Translate(CANE_POS_X, CANE_POS_Y, CANE_POS_Z, MTXMODE_APPLY);
    Matrix_RotateZYX((s16)(CANE_ROT_X * DEG_TO_BINANG), 0, (s16)(CANE_ROT_Z * DEG_TO_BINANG), MTXMODE_APPLY);
}

static void DrawGetItem(PlayState* play, S2HGetItemEntry* entry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(0.20f, 0.20f, 0.20f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sCaneGiveDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

#include "byrna_visuals.inc.c"

static bool sRemoteVisualReady;
static bool sRemoteVisualSent;
static uint32_t sRemoteVisualFrame;
static RemoteVisualState sLastRemoteVisual;

static void ReceiveRemoteVisual(const S2HNetMessage* message) {
    (void)message;
}

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual) {
    if (!sRemoteVisualReady || gPlayState == NULL || player == GET_PLAYER(gPlayState)) {
        return false;
    }
    uint32_t peerId = sApi->GetNetPeerIdForActor(&player->actor);
    if (peerId == 0 ||
        sApi->GetNetPeerState(peerId, BYRNA_VISUAL_CHANNEL, visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    if (visual->active != 1 || visual->scene != gPlayState->sceneId ||
        (player->stateFlags2 & PLAYER_STATE2_20000000)) {
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
        visual.scene = play->sceneId;
        visual.active = IsWielding(GET_PLAYER(play));
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (sRemoteVisualSent && memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) == 0) {
        return;
    }
    if (sApi->SetNetPeerState(BYRNA_VISUAL_CHANNEL, &visual, sizeof(visual))) {
        sLastRemoteVisual = visual;
        sRemoteVisualSent = true;
        sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
    }
}

static void InitRemoteVisuals(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterNetChannel) || !S2H_MOD_API_HAS(sApi, SetNetPeerState) ||
        !S2H_MOD_API_HAS(sApi, GetNetPeerState) || !S2H_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel(BYRNA_VISUAL_CHANNEL, 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
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
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, color >> 24, (color >> 16) & 255, (color >> 8) & 255, color & 255);
    gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING | G_SHADING_SMOOTH);
    CLOSE_DISPS(play->state.gfxCtx);
}

#include "byrna_register.inc.c"
