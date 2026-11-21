#include <math.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "regs.h"
#include "variables.h"
#include "z64quake.h"
#include "z64rumble.h"
#include "2s2h/ModApi/Player/PlayerHookTypes.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define PENDANT_KEY "nei.equip.pendant"
#define PENDANT_VISUAL_CHANNEL "nei.pendant.visual"
#define PENDANT_REMOTE_INTERVAL 5u
#define MORTAL_DRAW_RANGE 200.0f
#define MORTAL_DRAW_MIN_RANGE 20.0f
#define MORTAL_DRAW_HITSTOP 10
#define MORTAL_DRAW_RECOVERY 15
#define MORTAL_DRAW_SLASH_FRAMES 12
#define MORTAL_DRAW_DAMAGE 0xFF
#define GPOUND_STALL_FRAMES 4
#define GPOUND_FALL_VELOCITY (-18.0f)
#define GPOUND_FALL_GRAVITY (-2.5f)
#define GPOUND_LANDING_FRAMES 25.0f
#define GPOUND_POSE_FRAME 89.0f
#define GPOUND_SHOCKWAVE_FRAMES 6
#define GPOUND_BOUNCE_VELOCITY 8.0f
#define GPOUND_TIMEOUT 60
#define PARRY_ARC_FRAMES 22
#define PARRY_LAND_DIST 120.0f
#define PARRY_TIMING_FRAMES 6
#define PARRY_HOP_WINDOW 60

// The bit OoT names PLAYER_STATE2_HOPPING: set by the Z-target side hop and backflip.
#define PLAYER_STATE2_HOPPING PLAYER_STATE2_80000
#define PLAYER_STATE3_MIDAIR PLAYER_STATE3_2
#define PLAYER_STATE1_JUMPING PLAYER_STATE1_40000
#define PENDANT_BUSY_STATES                                                                                  \
    (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_1 | PLAYER_STATE1_400 | PLAYER_STATE1_800000 | \
     PLAYER_STATE1_4000000 | PLAYER_STATE1_8000000 | PLAYER_STATE1_10000000 | PLAYER_STATE1_20000000)

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__icon_item_static_yar/gItemIconPendantOfMemoriesTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__item_name_static/gItemNamePendantOfMemoriesENGTex";
static const ALIGN_ASSET(2) char sGetItemDL[] = "__OTR__objects/object_gi_reserve_c_01/gGiPendantOfMemoriesDL";
static const ALIGN_ASSET(2) char sGroundPoundAnim[] = "__OTR__misc/link_animetion/nei_pendant_ground_pound";
static const ALIGN_ASSET(2) char sDrawAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_fighter_power_kiru_start";
static const ALIGN_ASSET(2) char sLandingAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_landing";
static const ALIGN_ASSET(2) char sRollAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_landing_roll";
static const ALIGN_ASSET(2) char sRunJumpAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_run_jump";

PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
void func_80833864(PlayState* play, Player* player, s32 meleeWeaponAnim);
bool Player_IsZTargeting(Player* player);

typedef enum {
    PENDANT_IDLE,
    PENDANT_DRAW_SLASH,
    PENDANT_DRAW_HITSTOP,
    PENDANT_DRAW_RECOVERY,
    PENDANT_GPOUND_STALL,
    PENDANT_GPOUND_FALLING,
    PENDANT_GPOUND_LANDING,
    PENDANT_PARRY_ARC,
    PENDANT_PARRY_LANDING,
} PendantState;

typedef struct {
    int16_t scene;
    uint8_t active;
    uint8_t adult;
    uint8_t flags[8];
    float values[12];
} RemoteVisualState;

static const S2HModApi* sApi;

static bool ReadRemoteVisual(Player* player, RemoteVisualState* visual);

static ColliderCylinder sStrikeCollider;
static ColliderJntSph sParryCollider;
static ColliderJntSphElement sParryElement;
static bool sParryReady;
static bool sSecondHopPending;
static bool sComboLandingPending;
static bool sComboWasAirborne;
static f32 sSavedGravity;

static ColliderJntSphElementInit sParryElementInit = {
    { ELEM_MATERIAL_UNK2,
      { DMG_SWORD, 0x00, 0x08 },
      { 0, 0, 0 },
      ATELEM_ON | ATELEM_NEAREST | ATELEM_SFX_NORMAL,
      ACELEM_NONE,
      OCELEM_NONE },
    { 0, { { 0, 0, 0 }, 36 }, 100 },
};

static ColliderJntSphInit sParryInit = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_JNTSPH },
    1,
    &sParryElementInit,
};

static ColliderCylinderInit sStrikeColliderInit = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK2,
      { DMG_SWORD, 0x00, 0x08 },
      { 0, 0, 0 },
      ATELEM_ON | ATELEM_SFX_NORMAL,
      ACELEM_NONE,
      OCELEM_NONE },
    { 80, 80, 0, { 0, 0, 0 } },
};

static struct {
    PendantState state;
    s16 timer;
    Actor* target;
    Vec3f arcStart;
    Vec3f arcEnd;
    bool hasParryHit;
    bool isColliderReady;
    u8 hops;
    s16 hopTimer;
    bool isLeapArmed;
} sPendant;

static bool IsActive(void) {
    return sApi->IsEquipmentOwned(PENDANT_KEY) && sApi->IsEquipmentToggleOn(PENDANT_KEY) &&
           !sApi->IsEquipmentWorn("nei.equip.trident") && !sApi->IsEquipmentWorn("nei.equip.byrna");
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static bool IsPlayerBusy(PlayState* play, Player* player) {
    return Player_InCsMode(play) || (player->stateFlags1 & PENDANT_BUSY_STATES) != 0;
}

static bool IsSwordOut(Player* player) {
    return Player_GetMeleeWeaponHeld(player) != PLAYER_MELEEWEAPON_NONE;
}

static bool IsBPressed(PlayState* play) {
    return CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B);
}

static void PlaySfx(Player* player, u16 sfxId) {
    Audio_PlaySfx_AtPos(&player->actor.projectedPos, sfxId);
}

static void ChangeAnim(PlayState* play, Player* player, const char* path, f32 playSpeed, f32 startFrame, f32 endFrame,
                       u8 mode, f32 morph) {
    PlayerAnimationHeader* anim = ResourceMgr_LoadPlayerAnimAsHeader(path);

    if (anim != NULL) {
        PlayerAnimation_Change(play, &player->skelAnime, anim, playSpeed, startFrame, endFrame, mode, morph);
    }
}

static f32 GetLastFrame(const char* path) {
    PlayerAnimationHeader* anim = ResourceMgr_LoadPlayerAnimAsHeader(path);

    return anim == NULL ? 0.0f : Animation_GetLastFrame(anim);
}

static bool IsActorAlive(PlayState* play, Actor* actor) {
    if (actor == NULL) {
        return false;
    }
    for (s32 category = 0; category < ACTORCAT_MAX; category++) {
        for (Actor* listed = play->actorCtx.actorLists[category].first; listed != NULL; listed = listed->next) {
            if (listed == actor) {
                return actor->update != NULL;
            }
        }
    }
    return false;
}

static void ReturnToIdle(void) {
    sPendant.state = PENDANT_IDLE;
    sPendant.timer = 0;
    sPendant.target = NULL;
}

static void ResetMoves(void) {
    sSecondHopPending = false;
    sComboLandingPending = false;
    sComboWasAirborne = false;
    ReturnToIdle();
    sPendant.hops = 0;
    sPendant.hopTimer = 0;
    sPendant.isLeapArmed = false;
}

static void EnsureStrikeCollider(PlayState* play, Player* player) {
    if (sPendant.isColliderReady) {
        return;
    }
    Collider_InitCylinder(play, &sStrikeCollider);
    Collider_SetCylinder(play, &sStrikeCollider, &player->actor, &sStrikeColliderInit);
    sPendant.isColliderReady = true;
}

static void EnsureParryCollider(PlayState* play, Player* player) {
    if (sParryReady) {
        return;
    }
    Collider_InitJntSph(play, &sParryCollider);
    Collider_SetJntSph(play, &sParryCollider, &player->actor, &sParryInit, &sParryElement);
    sParryReady = true;
}

static void StrikeAround(PlayState* play, Player* player, Vec3f* center) {
    sStrikeCollider.dim.pos.x = center->x;
    sStrikeCollider.dim.pos.y = center->y;
    sStrikeCollider.dim.pos.z = center->z;
    sStrikeCollider.base.atFlags |= AT_ON;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sStrikeCollider.base);
}

static bool ConsumeStrikeHit(void) {
    if (!(sStrikeCollider.base.atFlags & AT_HIT)) {
        return false;
    }
    sStrikeCollider.base.atFlags &= ~AT_HIT;
    return true;
}

static void ShakeCamera(PlayState* play, s16 speed, s16 y, s16 x, s16 zoom, s16 frames) {
    s16 quake = Quake_Request(Play_GetCamera(play, CAM_ID_MAIN), QUAKE_TYPE_3);

    Quake_SetSpeed(quake, speed);
    Quake_SetPerturbations(quake, y, x, zoom, 0);
    Quake_SetDuration(quake, frames);
}

static Actor* FindMortalDrawTarget(Player* player, PlayState* play) {
    Actor* best = NULL;
    f32 closest = MORTAL_DRAW_RANGE;

    for (Actor* enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].first; enemy != NULL; enemy = enemy->next) {
        f32 dist = Actor_WorldDistXZToActor(&player->actor, enemy);

        if (dist < closest && dist > MORTAL_DRAW_MIN_RANGE) {
            closest = dist;
            best = enemy;
        }
    }
    return best;
}

static bool CanMortalDraw(Player* player, PlayState* play) {
    return IsBPressed(play) && !IsSwordOut(player) && !Player_IsZTargeting(player) && IsGrounded(player) &&
           player->speedXZ <= 1.0f;
}

static void StartMortalDraw(Player* player, PlayState* play, Actor* target) {
    s16 yaw = Actor_WorldYawTowardActor(&player->actor, target);

    sPendant.state = PENDANT_DRAW_SLASH;
    sPendant.timer = 0;
    sPendant.target = target;
    player->actor.shape.rot.y = yaw;
    player->yaw = yaw;
    player->speedXZ = 0.0f;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.z = 0.0f;
    func_80833864(play, player, PLAYER_MWA_JUMPSLASH_START);
    ChangeAnim(play, player, sDrawAnim, 2.0f, 0.0f, GetLastFrame(sDrawAnim), ANIMMODE_ONCE, -3.0f);
    player->meleeWeaponQuads[0].elem.atDmgInfo.damage = MORTAL_DRAW_DAMAGE;
    player->meleeWeaponQuads[1].elem.atDmgInfo.damage = MORTAL_DRAW_DAMAGE;
    PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
}

static void UpdateMortalDraw(Player* player, PlayState* play) {
    bool hasHit =
        (player->meleeWeaponQuads[0].base.atFlags & AT_HIT) || (player->meleeWeaponQuads[1].base.atFlags & AT_HIT);

    sPendant.timer++;
    player->speedXZ = 0.0f;
    if (hasHit) {
        sPendant.state = PENDANT_DRAW_HITSTOP;
        sPendant.timer = 0;
        ShakeCamera(play, 20000, 8, 0, 0, MORTAL_DRAW_HITSTOP);
        Rumble_Request(0.0f, 180, 14, 100);
        PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
        return;
    }
    if (sPendant.timer > MORTAL_DRAW_SLASH_FRAMES) {
        sPendant.state = PENDANT_DRAW_RECOVERY;
        sPendant.timer = 0;
    }
}

static void UpdateHitstop(Player* player) {
    player->skelAnime.playSpeed = 0.0f;
    player->speedXZ = 0.0f;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.z = 0.0f;
    if (++sPendant.timer >= MORTAL_DRAW_HITSTOP) {
        sPendant.state = PENDANT_DRAW_RECOVERY;
        sPendant.timer = 0;
        player->skelAnime.playSpeed = 1.0f;
    }
}

static void UpdateDrawRecovery(Player* player) {
    player->speedXZ = 0.0f;
    if (++sPendant.timer >= MORTAL_DRAW_RECOVERY) {
        ReturnToIdle();
    }
}

static bool CanGroundPound(Player* player, PlayState* play) {
    return !IsGrounded(player) && IsSwordOut(player) && IsBPressed(play);
}

static void HoldStill(Player* player) {
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
    player->actor.gravity = 0.0f;
    player->speedXZ = 0.0f;
}

// A negative speed plays the clip back from the planted pose to its first frame.
static void PlantSwordPose(Player* player, PlayState* play, f32 playSpeed, f32 morph) {
    bool isRising = playSpeed < 0.0f;

    ChangeAnim(play, player, sGroundPoundAnim, playSpeed, GPOUND_POSE_FRAME, isRising ? 0.0f : GPOUND_POSE_FRAME,
               isRising ? ANIMMODE_ONCE : ANIMMODE_LOOP, morph);
}

static void StartGroundPound(Player* player, PlayState* play) {
    sPendant.state = PENDANT_GPOUND_STALL;
    sPendant.timer = 0;
    sSavedGravity = player->actor.gravity;
    HoldStill(player);
    PlantSwordPose(player, play, 0.0f, -3.0f);
    EnsureStrikeCollider(play, player);
    PlaySfx(player, NA_SE_IT_SWORD_SWING_HARD);
}

static void UpdateGroundPoundStall(Player* player, PlayState* play) {
    HoldStill(player);
    PlayerAnimation_Update(play, &player->skelAnime);
    if (++sPendant.timer < GPOUND_STALL_FRAMES) {
        return;
    }
    sPendant.state = PENDANT_GPOUND_FALLING;
    sPendant.timer = 0;
    player->actor.velocity.y = GPOUND_FALL_VELOCITY;
    player->actor.gravity = GPOUND_FALL_GRAVITY;
    PlantSwordPose(player, play, 0.0f, 0.0f);
}

static void LandGroundPound(Player* player, PlayState* play) {
    sPendant.state = PENDANT_GPOUND_LANDING;
    sPendant.timer = 0;
    StrikeAround(play, player, &player->actor.world.pos);
    Rumble_Request(0.0f, 255, 20, 150);
    ShakeCamera(play, 28000, 14, 2, 100, 16);
    PlaySfx(player, NA_SE_IT_HAMMER_HIT);
    PlantSwordPose(player, play, -GPOUND_POSE_FRAME / GPOUND_LANDING_FRAMES, -3.0f);
    player->actor.gravity = sSavedGravity;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->stateFlags1 &= ~PLAYER_STATE1_JUMPING;
}

static void UpdateGroundPoundFall(Player* player, PlayState* play) {
    sPendant.timer++;
    player->actor.gravity = GPOUND_FALL_GRAVITY;
    player->speedXZ = 0.0f;
    PlantSwordPose(player, play, 0.0f, 0.0f);
    PlayerAnimation_Update(play, &player->skelAnime);
    if (ConsumeStrikeHit()) {
        player->actor.velocity.y = GPOUND_BOUNCE_VELOCITY;
        player->actor.gravity = sSavedGravity;
        sStrikeCollider.base.atFlags &= ~AT_ON;
        PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
        ReturnToIdle();
        return;
    }
    StrikeAround(play, player, &player->actor.world.pos);
    if (IsGrounded(player)) {
        LandGroundPound(player, play);
    }
    if (sPendant.timer > GPOUND_TIMEOUT) {
        player->actor.gravity = sSavedGravity;
        ReturnToIdle();
    }
}

static void UpdateGroundPoundLanding(Player* player, PlayState* play) {
    player->speedXZ = 0.0f;
    if (++sPendant.timer < GPOUND_SHOCKWAVE_FRAMES) {
        StrikeAround(play, player, &player->actor.world.pos);
        if (ConsumeStrikeHit()) {
            PlaySfx(player, NA_SE_IT_SWORD_STRIKE);
        }
    }
    if (PlayerAnimation_Update(play, &player->skelAnime) || sPendant.timer >= GPOUND_LANDING_FRAMES) {
        player->actor.gravity = sSavedGravity;
        ReturnToIdle();
    }
}

#include "pendant_combo.inc.c"

static void StartMoveFromIdle(Player* player, PlayState* play) {
    TrackSideHops(player);
    if (CanMortalDraw(player, play)) {
        Actor* drawTarget = FindMortalDrawTarget(player, play);

        if (drawTarget != NULL) {
            StartMortalDraw(player, play, drawTarget);
            return;
        }
    }
    if (CanGroundPound(player, play)) {
        StartGroundPound(player, play);
    }
}

static bool OwnsPendantAction(void) {
    return sPendant.state >= PENDANT_GPOUND_STALL;
}

static void ReleasePendantAction(Player* player) {
    if (!OwnsPendantAction()) {
        return;
    }
    player->actor.gravity = sSavedGravity;
    player->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
    player->stateFlags2 &= ~PLAYER_STATE2_HOPPING;
    player->cylinder.base.ocFlags1 |= OC1_ON;
    player->skelAnime.playSpeed = 1.0f;
    if (sParryReady) {
        sParryCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    }
    if (sPendant.isColliderReady) {
        sStrikeCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    }
}

static void TickPendant(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || player == NULL || player != GET_PLAYER(play)) {
        return;
    }
    if (!IsActive() || IsPlayerBusy(play, player)) {
        ReleasePendantAction(player);
        if (sPendant.state != PENDANT_IDLE) {
            player->cylinder.base.ocFlags1 |= OC1_ON;
            player->skelAnime.playSpeed = 1.0f;
        }
        ResetMoves();
        return;
    }
    if (sSecondHopPending && sPendant.state == PENDANT_IDLE) {
        ConfigureSecondHopAnimation(player);
    }
    switch (sPendant.state) {
        case PENDANT_IDLE:
            StartMoveFromIdle(player, play);
            break;
        case PENDANT_DRAW_SLASH:
            UpdateMortalDraw(player, play);
            break;
        case PENDANT_DRAW_HITSTOP:
            UpdateHitstop(player);
            break;
        case PENDANT_DRAW_RECOVERY:
            UpdateDrawRecovery(player);
            break;
        case PENDANT_GPOUND_STALL:
        case PENDANT_GPOUND_FALLING:
        case PENDANT_GPOUND_LANDING:
        case PENDANT_PARRY_ARC:
        case PENDANT_PARRY_LANDING:
            break;
        default:
            ReturnToIdle();
            break;
    }
}

// Replaces the vanilla action function while a pendant move owns Link's body.
static void DrivePendantAction(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);
    PlayState* play = gPlayState;

    if (play == NULL || player != GET_PLAYER(play) || !OwnsPendantAction()) {
        return;
    }
    if (!IsActive() || IsPlayerBusy(play, player)) {
        ReleasePendantAction(player);
        ResetMoves();
        return;
    }
    *should = false;
    SilenceSwordQuads(player);
    switch (sPendant.state) {
        case PENDANT_GPOUND_STALL:
            UpdateGroundPoundStall(player, play);
            break;
        case PENDANT_GPOUND_FALLING:
            UpdateGroundPoundFall(player, play);
            break;
        case PENDANT_GPOUND_LANDING:
            UpdateGroundPoundLanding(player, play);
            break;
        case PENDANT_PARRY_ARC:
            UpdateParryArc(player, play);
            break;
        case PENDANT_PARRY_LANDING:
            PlayerAnimation_Update(play, &player->skelAnime);
            UpdateParryLanding(player);
            break;
        default:
            ReturnToIdle();
            break;
    }
}

static void DrawGetItem(PlayState* play, S2HGetItemEntry* entry) {
    (void)entry;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(0.9f, 0.9f, 0.9f, MTXMODE_APPLY);
    Matrix_RotateYF((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGetItemDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void ForgetSceneState(s16 sceneId, s8 spawnNum) {
    ResetMoves();
    sPendant.isColliderReady = false;
    sParryReady = false;
    sBoundsActor = NULL;
}

static void TurnOff(const char* key) {
    if (gPlayState != NULL) {
        ReleasePendantAction(GET_PLAYER(gPlayState));
    }
    ResetMoves();
}

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate",       "OnPlayerUpdate",       "OnSceneInit",
                                              "OnPlayerResolveAnimSite", "OnPlayerFilterInput",  "OnActorUpdate",
                                              "OnPlayerPostLimbDraw",    "OnActorResolveMotionScale" };

static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

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
        sApi->GetNetPeerState(peerId, PENDANT_VISUAL_CHANNEL, visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    return visual->active == 1 && visual->scene == gPlayState->sceneId &&
           !(player->stateFlags2 & PLAYER_STATE2_20000000);
}

static void PublishRemoteVisual(void) {
    PlayState* play = gPlayState;
    RemoteVisualState visual = { 0 };

    if (!sRemoteVisualReady) {
        return;
    }
    visual.scene = -1;
    if (play != NULL) {
        visual.scene = play->sceneId;
        visual.active = IsParryCueShown(GET_PLAYER(play));
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < PENDANT_REMOTE_INTERVAL) {
            return;
        }
    }
    if (sRemoteVisualSent && memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) == 0) {
        return;
    }
    if (sApi->SetNetPeerState(PENDANT_VISUAL_CHANNEL, &visual, sizeof(visual))) {
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
    sRemoteVisualReady = sApi->RegisterNetChannel(PENDANT_VISUAL_CHANNEL, 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    S2HCustomEquipDefinition definition = { 0 };

    InitRemoteVisuals();
    definition.structSize = sizeof(definition);
    definition.key = PENDANT_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rPendant of Memories&%w%y\xB1%w near an enemy with your sword&"
                           "sheathed draws and strikes. %y\xB1%w in&mid air slams down.^"
                           "Locked on: %y\xB0%w, %y\xB0%w, %y\xB0%w.&Hop, roll, hop. At the Hey and&"
                           "sword glow, tap %y\xB1%w. No bosses.^"
                           "The timing window is brief.&%y\xB0%w on this slot turns it off.";
    definition.getItemText = "You got the %rPendant of&Memories%w! A keepsake from&Kafei, proof of his promise.^"
                             "It remembers the moves of&heroes from other tales, and&they answer to %y\xB1%w.";
    definition.slot = S2H_EQUIP_SLOT_UPGRADE;
    definition.page = 0;
    definition.row = 1;
    definition.column = 0;
    definition.toggles = 1;
    definition.onUnequip = TurnOff;
    definition.getItemEntry.drawFunc = DrawGetItem;
    definition.getItemModelPath = sGetItemDL;
    static const S2HCustomItemRandomizer randomizer = {
        .structSize = sizeof(S2HCustomItemRandomizer),
        .type = S2H_CUSTOM_ITEM_TYPE_EQUIP,
        .poolCount = 1,
        .logicKey = PENDANT_KEY,
        .hintKey = PENDANT_KEY,
    };
    definition.randomizer = &randomizer;
    if (!sApi->RegisterCustomEquipment(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickPendant);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveAnimSite, ResolveComboHop);
    S2H_REGISTER_HOOK(sApi, OnActorResolveMotionScale, ExtendComboHopTravel);
    S2H_REGISTER_HOOK(sApi, OnPlayerFilterInput, ReadParryInput);
    S2H_REGISTER_HOOK(sApi, OnActorUpdate, SampleTargetBounds);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawParryCue);
    sApi->RegisterVB(VB_EXECUTE_PLAYER_ACTION_FUNC, DrivePendantAction);
}
