#include <math.h>
#include <string.h>

#include "z64items.h"
void Player_AnimSfx_PlayVoice(Player* player, u16 sfxId);
#include "z64aiming.h"
s32 Player_GetMovementSpeedAndYaw(Player* player, f32* speed, s16* yaw, f32 mode, PlayState* play);

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_Obj_Ice_Poly/z_obj_ice_poly.h"
#include "overlays/actors/ovl_Bg_Icicle/z_bg_icicle.h"
#include "overlays/actors/ovl_Obj_Bigicicle/z_obj_bigicicle.h"
#include "overlays/actors/ovl_Obj_Iceblock/z_obj_iceblock.h"
#include "overlays/actors/ovl_En_Fz/z_en_fz.h"
#include "overlays/actors/ovl_En_Snowman/z_en_snowman.h"
void EnSnowman_SetupMelt(EnSnowman* snowman);

#define BALL_KEY "nei.ball_and_chain"
#define BALL_BUTTON_COUNT S2H_ITEM_BUTTON_COUNT
#define BALL_DAMAGE 8
#define BALL_COLLIDER_RADIUS 20
#define BALL_COLLIDER_HEIGHT 20
#define BALL_RADIUS 12.0f
#define BALL_WALL_HEIGHT 20.0f
#define BALL_ICE_REACH 140.0f
#define BALL_BIGICE_REACH 200.0f
#define BALL_HOLD_WALK_SCALE 0.5f
#define BALL_SPIN_WALK_SCALE 0.35f
#define BALL_GUARD_RADIUS_MIN 45.0f
#define BALL_GUARD_RADIUS_MAX 70.0f
#define BALL_GUARD_HEIGHT 30.0f
#define BALL_GOLDEN_DAMAGE 16
#define BALL_HOLD_DROP 5.0f
#define BALL_CHARGE_MAX 60
// The ball has to have built up a swing before it can be aimed or hurled; a tap only lifts it.
#define BALL_CHARGE_MIN 15
#define BALL_SPIN_RADIUS 20.0f
#define BALL_SPIN_HEIGHT_MIN 50.0f
#define BALL_SPIN_HEIGHT_MAX 55.0f
#define BALL_SPIN_SPEED_MIN 0x1000
#define BALL_SPIN_SPEED_MAX 0x2000
#define BALL_STICK_DEADZONE 5.0f
#define BALL_LEAN_TILT 40.0f
#define BALL_THROW_LEAN 3000
// The swing pulls Link 1 degree off balance at rest and 5 at full charge, in the ball's own direction.
#define BALL_LEAN_MIN 182.0f
#define BALL_LEAN_MAX 910.0f
#define BALL_HOLD_POSE_FRAME 8
#define BALL_SPIN_POSE_FRAME 0
#define BALL_LAUNCH_SPEED_MIN 22.0f
#define BALL_LAUNCH_SPEED_MAX 32.0f
#define BALL_LAUNCH_LIFT 7.0f
#define BALL_GRAVITY -1.5f
#define BALL_TERMINAL_FALL 30.0f
#define BALL_RETRACT_SPEED 34.0f
#define BALL_BOUNCE_KEEP_Y 0.45f
#define BALL_BOUNCE_KEEP_XZ 0.65f
#define BALL_WALL_KEEP 0.55f
#define BALL_MAX_BOUNCES 2
#define BALL_REST_FRAMES 12
#define BALL_CHAIN_LENGTH 380.0f
#define BALL_RETURN_DISTANCE 65.0f
#define BALL_THROW_TIMEOUT 200
#define BALL_HELD_SCALE 0.06f
#define BALL_SWUNG_SCALE 0.1f
// The chain model is one strand 100 units long at scale 1.0, the length the hookshot stretches it by.
#define BALL_CHAIN_STRAND_SPAN 100.0f
#define BALL_CHAIN_STRAND_MAX 8
#define BALL_CHAIN_THICKNESS 0.02f
#define BALL_CHAIN_STRETCH 0.01f

typedef enum {
    BALL_IDLE,
    BALL_HOLDING,
    BALL_SPINNING,
    BALL_AIMING,
    BALL_FLYING,
    BALL_RESTING,
    BALL_RETRACTING,
} BallPhase;

// Majora's Mask has no gauntlet upgrades; each form's own strength takes their place: the Human lifts and
// throws forward, the Zora frees feet and aim, the Goron swings it one-handed in a circle that guards Link.

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconBallAndChainTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gBallAndChainNameTex";
static const ALIGN_ASSET(2) char sBallDL[] = "__OTR__objects/object_nei_ball_and_chain/gBallDL";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_ball_and_chain/g_ball_and_chain_dl";
static const ALIGN_ASSET(2) char sChainLinkDL[] = "__OTR__objects/gameplay_keep/gHookshotChainDL";

// Vanilla carries: the ball rides in front of Link while he holds it, and overhead once he swings it.
static const ALIGN_ASSET(2) char sHoldPoseAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_carryB_free";
static const ALIGN_ASSET(2) char sSpinPoseAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_carryB_wait";

static const u32 sItemButtons[BALL_BUTTON_COUNT] = { BTN_B,      BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                     BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };

static const u8 sArmLimbs[] = { PLAYER_LIMB_LEFT_SHOULDER,  PLAYER_LIMB_LEFT_FOREARM,  PLAYER_LIMB_LEFT_HAND,
                                PLAYER_LIMB_RIGHT_SHOULDER, PLAYER_LIMB_RIGHT_FOREARM, PLAYER_LIMB_RIGHT_HAND };

static ColliderCylinderInit sBallColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_PLAYER | AT_TYPE_OTHER,
        AC_ON | AC_TYPE_ENEMY | AC_HARD,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK2,
        { DMG_GORON_PUNCH, 0x00, BALL_DAMAGE },
        { 0xFFFFFFFF, 0, 0 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_NONE,
    },
    { BALL_COLLIDER_RADIUS, BALL_COLLIDER_HEIGHT, 0, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = { "OnPlayerUpdate",
                                              "ShouldActorDraw",
                                              "OnActorDrawEnd",
                                              "OnPlayerFilterInput",
                                              "OnPlayerResolveMotionScale",
                                              "OnPlayerResolveItemActionInit" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static ColliderCylinder sBallCollider;
static bool sIsColliderReady;
static u8 sPhase;
static s16 sCharge;
static s16 sSpinAngle;
static s16 sThrowYaw;
static s16 sThrownFrames;
static s16 sRestTimer;
static u8 sBounces;
static f32 sStickLeanX;
static f32 sStickLeanY;
static Vec3f sBallPos;
static Vec3f sBallVelocity;
static Vec3s sPoseJoints[PLAYER_LIMB_MAX];
static s32 sTrailIndex = -1;

bool Player_UpdateUpperBody(Player* player, PlayState* play);
bool Player_IsZTargeting(Player* player);
s32 Player_SetAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 flags);
void func_8083B1A0(Player* player, PlayState* play);

static u32 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < BALL_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, BALL_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static bool IsButtonHeld(PlayState* play) {
    return (play->state.input[0].cur.button & FindEquippedButtonMask()) != 0;
}

static bool IsBallAway(void) {
    return sPhase >= BALL_FLYING;
}

static s32 Strength(void) {
    return Player_GetStrength();
}

static bool CanLiftBall(Player* player, PlayState* play) {
    return Strength() >= PLAYER_STRENGTH_HUMAN && !(player->stateFlags1 & PLAYER_STATE1_8000000);
}

static bool CanWalkWithBall(void) {
    return Strength() >= PLAYER_STRENGTH_ZORA;
}

static bool CanAimBall(void) {
    return Strength() >= PLAYER_STRENGTH_ZORA;
}

static bool IsOneHanded(void) {
    return Strength() >= PLAYER_STRENGTH_GORON;
}

static void StartTrail(PlayState* play) {
    EffectBlureInit2 init = {
        0,
        8,
        0,
        { 255, 255, 255, 255 },
        { 200, 220, 255, 128 },
        { 255, 255, 255, 0 },
        { 200, 220, 255, 0 },
        4,
        0,
        2,
        0,
        { 235, 235, 245, 200 },
        { 180, 190, 210, 96 },
    };

    Effect_Add(play, &sTrailIndex, EFFECT_BLURE2, 0, 0, &init);
}

static void FeedTrail(PlayState* play) {
    Vec3f top = { sBallPos.x, sBallPos.y + 14.0f, sBallPos.z };
    Vec3f bottom = { sBallPos.x, sBallPos.y - 14.0f, sBallPos.z };

    if (sTrailIndex < 0) {
        StartTrail(play);
    }
    EffectBlure_AddVertex((EffectBlure*)Effect_GetByIndex(sTrailIndex), &top, &bottom);
}

static void EndTrail(PlayState* play) {
    if (sTrailIndex >= 0) {
        Effect_Destroy(play, sTrailIndex);
        sTrailIndex = -1;
    }
}

// Ground-level strikes count as the Goron's pound, which is what floor switches and stakes answer to.
static void StrikeWithBall(Player* player, PlayState* play) {
    bool isAtFootLevel = sBallPos.y - player->actor.world.pos.y < 30.0f;

    sBallCollider.elem.atDmgInfo.dmgFlags = isAtFootLevel ? DMG_GORON_POUND : DMG_GORON_PUNCH;
    sBallCollider.elem.atDmgInfo.damage = Strength() >= PLAYER_STRENGTH_GORON ? BALL_GOLDEN_DAMAGE : BALL_DAMAGE;
    sBallCollider.dim.pos.x = sBallPos.x;
    sBallCollider.dim.pos.y = sBallPos.y - BALL_COLLIDER_HEIGHT / 2;
    sBallCollider.dim.pos.z = sBallPos.z;
    sBallCollider.base.atFlags |= AT_ON | AT_TYPE_PLAYER | AT_TYPE_OTHER;
    if (sBallCollider.base.atFlags & AT_HIT) {
        sBallCollider.base.atFlags &= ~AT_HIT;
        Audio_PlaySfx_AtPos(&sBallPos, NA_SE_IT_HAMMER_HIT);
    }
    CollisionCheck_SetAT(play, &play->colChkCtx, &sBallCollider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &sBallCollider.base);
}

static void BurstIce(PlayState* play, Vec3f* pos) {
    EffectSsIcePiece_SpawnBurst(play, pos, 1.5f);
    Audio_PlaySfx_AtPos(pos, NA_SE_EV_ICE_BROKEN);
}

// Red meltable ice melts on a fire hit or when its switch flag is set: setting the flag drives its own native
// shatter, which also opens the path it gates. Decorative ice with no flag has nothing to gate.
static void MeltRedIce(PlayState* play, Actor* actor) {
    ObjIcePoly* ice = (ObjIcePoly*)actor;

    if (ice->switchFlag != OBJICEPOLY_SWITCH_FLAG_NONE) {
        Flags_SetSwitch(play, ice->switchFlag);
        return;
    }
    BurstIce(play, &actor->world.pos);
    Actor_Kill(actor);
}

static bool IsBallNear(Actor* actor, f32 reach) {
    return Math_Vec3f_DistXYZ(&sBallPos, &actor->world.pos) < reach;
}

static void BreakIceActor(PlayState* play, Actor* actor) {
    switch (actor->id) {
        case ACTOR_OBJ_ICE_POLY:
            if (IsBallNear(actor, BALL_BIGICE_REACH)) {
                MeltRedIce(play, actor);
            }
            break;
        case ACTOR_BG_ICICLE:
            if (IsBallNear(actor, BALL_ICE_REACH)) {
                ((BgIcicle*)actor)->collider.base.acFlags |= AC_HIT;
            }
            break;
        case ACTOR_OBJ_BIGICICLE:
            if (IsBallNear(actor, BALL_BIGICE_REACH)) {
                ((ObjBigicicle*)actor)->collider1.base.acFlags |= AC_HIT;
            }
            break;
        case ACTOR_OBJ_ICEBLOCK: {
            ObjIceblock* block = (ObjIceblock*)actor;

            // The iceberg variant is a platform you stand on; only a block that is not melting yet is tripped.
            if (IsBallNear(actor, BALL_BIGICE_REACH) && !ICEBLOCK_GET_ICEBERG(actor) && block->meltTimer > 0) {
                block->meltTimer = 0;
            }
            break;
        }
        case ACTOR_EN_FZ: {
            EnFz* freezard = (EnFz*)actor;

            // A non-zero unk_BD8 marks one that is already dying.
            if (IsBallNear(actor, BALL_ICE_REACH) && freezard->unk_BD8 == 0) {
                freezard->actor.colChkInfo.damageEffect = 2;
                freezard->collider1.base.acFlags |= AC_HIT;
            }
            break;
        }
        case ACTOR_EN_SNOWMAN:
            // AC_ON is cleared by the melt setup, so this only fires once per Eeno.
            if (IsBallNear(actor, BALL_ICE_REACH) && (((EnSnowman*)actor)->collider.base.acFlags & AC_ON)) {
                EnSnowman_SetupMelt((EnSnowman*)actor);
            }
            break;
        default:
            break;
    }
}

// A fast ball tunnels through thin ice between frames, so it melts on proximity, not on contact.
static void BreakWhatTheBallReaches(PlayState* play) {
    static const u8 sCategories[] = { ACTORCAT_PROP, ACTORCAT_BG, ACTORCAT_ITEMACTION, ACTORCAT_ENEMY };

    for (u32 i = 0; i < ARRAY_COUNT(sCategories); i++) {
        Actor* next;

        for (Actor* actor = play->actorCtx.actorLists[sCategories[i]].first; actor != NULL; actor = next) {
            next = actor->next;
            BreakIceActor(play, actor);
        }
    }
}

static void CrushArmoredFoes(void) {
    Actor* hit = sBallCollider.base.at;

    if (!(sBallCollider.base.atFlags & AT_HIT) || hit == NULL || hit->update == NULL) {
        return;
    }
    if ((hit->id == ACTOR_EN_ST || hit->id == ACTOR_EN_FZ) && hit->colChkInfo.health > 0) {
        hit->colChkInfo.health = MAX(hit->colChkInfo.health - BALL_DAMAGE, 0);
    }
}

static void StrikeAndBreak(Player* player, PlayState* play) {
    CrushArmoredFoes();
    StrikeWithBall(player, play);
    BreakWhatTheBallReaches(play);
}

// Goron strength holds the ball in the throwing hand alone; weaker arms need both.
static void HoldBallInHands(Player* player) {
    Vec3f* left = &player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND];
    Vec3f* right = &player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND];

    if (IsOneHanded()) {
        sBallPos = *right;
        sBallPos.y += BALL_HOLD_DROP;
        return;
    }
    sBallPos.x = (left->x + right->x) * 0.5f;
    sBallPos.y = (left->y + right->y) * 0.5f + BALL_HOLD_DROP;
    sBallPos.z = (left->z + right->z) * 0.5f;
}

// The engine's own speed resolution, so Link keeps his turning and his animations and only loses pace.
static void WeighDownPlayer(Player* player, s32 kind, f32* scale) {
    if (sPhase == BALL_IDLE || IsBallAway()) {
        return;
    }
    if (!CanWalkWithBall()) {
        *scale = 0.0f;
        return;
    }
    *scale *= sPhase == BALL_SPINNING ? BALL_SPIN_WALK_SCALE : BALL_HOLD_WALK_SCALE;
}

static void FaceThrowDirection(Player* player) {
    player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = sThrowYaw;
}

static void TurnBallInPlace(Player* player, PlayState* play) {
    if (!CanWalkWithBall() && !Player_IsZTargeting(player)) {
        f32 speed;
        s16 yaw;
        if (Player_GetMovementSpeedAndYaw(player, &speed, &yaw, 1.0f, play)) {
            Math_ScaledStepToS(&player->actor.shape.rot.y, yaw, 0x7D0);
            player->yaw = player->actor.world.rot.y = player->actor.shape.rot.y;
        }
        Player_StopHorizontalMovement(player);
    }
}

static void OrbitBallOverhead(Player* player) {
    f32 chargeRatio = (f32)sCharge / BALL_CHARGE_MAX;
    f32 orbitX = Math_SinS(sSpinAngle) * BALL_SPIN_RADIUS;
    f32 orbitZ = Math_CosS(sSpinAngle) * BALL_SPIN_RADIUS;
    f32 orbitY = BALL_SPIN_HEIGHT_MIN + (BALL_SPIN_HEIGHT_MAX - BALL_SPIN_HEIGHT_MIN) * chargeRatio -
                 Math_CosS(sSpinAngle) * sStickLeanY * BALL_LEAN_TILT -
                 Math_SinS(sSpinAngle) * sStickLeanX * BALL_LEAN_TILT;
    s16 yaw = player->actor.shape.rot.y;

    sSpinAngle += (s16)(BALL_SPIN_SPEED_MIN + (BALL_SPIN_SPEED_MAX - BALL_SPIN_SPEED_MIN) * chargeRatio);
    sBallPos.x = player->actor.world.pos.x + orbitX * Math_CosS(yaw) + orbitZ * Math_SinS(yaw);
    sBallPos.y = player->actor.world.pos.y + orbitY;
    sBallPos.z = player->actor.world.pos.z - orbitX * Math_SinS(yaw) + orbitZ * Math_CosS(yaw);
}

static void BallThrownAction(Player* player, PlayState* play);

static void ReturnBallToHands(Player* player, PlayState* play) {
    sPhase = BALL_HOLDING;
    EndTrail(play);
    AudioSfx_StopById(NA_SE_PL_WALK_GROUND);
    if (player->actionFunc == BallThrownAction) {
        func_8083B1A0(player, play);
    }
}

// Aimed throws lead the arc so gravity lands the ball on the mark; a plain throw only goes where Link faces.
static void LaunchTowards(Vec3f* mark, f32 launchSpeed) {
    f32 dx = mark->x - sBallPos.x;
    f32 dy = mark->y - sBallPos.y;
    f32 dz = mark->z - sBallPos.z;
    f32 flightFrames = MAX(sqrtf(SQ(dx) + SQ(dz)) / launchSpeed, 1.0f);

    sBallVelocity.x = dx / flightFrames;
    sBallVelocity.y = dy / flightFrames - 0.5f * BALL_GRAVITY * flightFrames;
    sBallVelocity.z = dz / flightFrames;
    sThrowYaw = Math_Vec3f_Yaw(&sBallPos, mark);
}

static void LaunchAlong(s16 yaw, s16 pitch, f32 launchSpeed) {
    sThrowYaw = yaw;
    sBallVelocity.x = Math_SinS(yaw) * Math_CosS(pitch) * launchSpeed;
    sBallVelocity.y = BALL_LAUNCH_LIFT - Math_SinS(pitch) * launchSpeed;
    sBallVelocity.z = Math_CosS(yaw) * Math_CosS(pitch) * launchSpeed;
}

static void AimLaunch(Player* player, f32 launchSpeed) {
    Actor* target = player->focusActor;

    if (sPhase == BALL_AIMING) {
        s16 yaw;
        s16 pitch;

        Z64Aiming_GetDirection(player, &yaw, &pitch);
        LaunchAlong(yaw, pitch, launchSpeed);
        return;
    }
    if (CanAimBall() && Player_IsZTargeting(player) && target != NULL && target->update != NULL) {
        LaunchTowards(&target->focus.pos, launchSpeed);
        return;
    }
    LaunchAlong(player->actor.shape.rot.y, 0, launchSpeed);
}

static void ThrowBall(Player* player, PlayState* play) {
    f32 launchSpeed =
        BALL_LAUNCH_SPEED_MIN + (BALL_LAUNCH_SPEED_MAX - BALL_LAUNCH_SPEED_MIN) * sCharge / BALL_CHARGE_MAX;

    sBallPos.x = player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * 20.0f;
    sBallPos.y = player->actor.world.pos.y + 45.0f;
    sBallPos.z = player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * 20.0f;
    AimLaunch(player, launchSpeed);
    sPhase = BALL_FLYING;
    sThrownFrames = 0;
    sBounces = 0;
    Player_SetAction(play, player, BallThrownAction, 1);
    Player_StopHorizontalMovement(player);
    Player_AnimSfx_PlayVoice(player, NA_SE_VO_LI_SWORD_N);
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SWORD_SWING_HARD);
}

// Goron strength swings the ball one-handed and wide, so it circles Link at chest height and guards him
// instead of riding over his head.
static void OrbitBallAround(Player* player) {
    f32 chargeRatio = (f32)sCharge / BALL_CHARGE_MAX;
    f32 radius = BALL_GUARD_RADIUS_MIN + (BALL_GUARD_RADIUS_MAX - BALL_GUARD_RADIUS_MIN) * chargeRatio;

    sSpinAngle += (s16)(BALL_SPIN_SPEED_MIN + (BALL_SPIN_SPEED_MAX - BALL_SPIN_SPEED_MIN) * chargeRatio);
    sBallPos.x = player->actor.world.pos.x + Math_SinS(sSpinAngle) * radius;
    sBallPos.y = player->actor.world.pos.y + BALL_GUARD_HEIGHT;
    sBallPos.z = player->actor.world.pos.z + Math_CosS(sSpinAngle) * radius;
}

static void SwingBall(Player* player) {
    if (IsOneHanded()) {
        OrbitBallAround(player);
        return;
    }
    OrbitBallOverhead(player);
}

static void StartAiming(Player* player, PlayState* play);

static bool IsBallSwungUp(void) {
    return sCharge >= BALL_CHARGE_MIN;
}

static void UpdateSpinning(Player* player, PlayState* play) {
    if (play->state.input[0].press.button & (BTN_A | BTN_B)) {
        sPhase = BALL_HOLDING;
        return;
    }
    if (CanAimBall() && IsBallSwungUp() && (play->state.input[0].press.button & BTN_CUP)) {
        StartAiming(player, play);
        return;
    }
    sStickLeanX = sStickLeanY = 0.0f;
    TurnBallInPlace(player, play);
    sCharge = MIN(sCharge + 1, BALL_CHARGE_MAX);
    SwingBall(player);
    StrikeAndBreak(player, play);
    FeedTrail(play);
    Actor_PlaySfx_Flagged2(&player->actor, NA_SE_IT_HOOKSHOT_CHAIN - SFX_FLAG);
    if (IsButtonHeld(play)) {
        return;
    }
    if (!IsBallSwungUp()) {
        sPhase = BALL_HOLDING;
        return;
    }
    ThrowBall(player, play);
}

// The return value is Link's "upper body is busy" flag: only the spin claims it, or the ball merely carried
// in hand would block his rolls and his other item buttons.
static int32_t UpdateBallInHand(Player* player, PlayState* play) {
    if ((player->stateFlags1 & PLAYER_STATE1_8000000) || player->invincibilityTimer < 0) {
        sPhase = BALL_HOLDING;
    }
    if (sPhase == BALL_HOLDING) {
        TurnBallInPlace(player, play);
        HoldBallInHands(player);
        sBallCollider.dim.pos.x = sBallPos.x;
        sBallCollider.dim.pos.y = sBallPos.y - BALL_COLLIDER_HEIGHT / 2;
        sBallCollider.dim.pos.z = sBallPos.z;
        if (sBallCollider.base.acFlags & AC_HIT) {
            sBallCollider.base.acFlags &= ~AC_HIT;
            Audio_PlaySfx_AtPos(&sBallPos, NA_SE_IT_SHIELD_BOUND);
        }
        CollisionCheck_SetAC(play, &play->colChkCtx, &sBallCollider.base);
        EndTrail(play);
        return 0;
    }
    if (sPhase == BALL_SPINNING) {
        UpdateSpinning(player, play);
        return 1;
    }
    return 0;
}

static void BounceOffWall(PlayState* play, Vec3f* previousPos) {
    CollisionPoly* wall = NULL;
    Vec3f resolved = sBallPos;

    if (!BgCheck_EntitySphVsWall1(&play->colCtx, &resolved, &sBallPos, previousPos, BALL_COLLIDER_RADIUS, &wall,
                                  BALL_WALL_HEIGHT)) {
        return;
    }
    sBallPos = resolved;
    if (fabsf(sBallVelocity.x) + fabsf(sBallVelocity.z) > 1.0f) {
        Audio_PlaySfx_AtPos(&sBallPos, NA_SE_IT_SHIELD_BOUND);
    }
    if (wall == NULL) {
        sBallVelocity.x = sBallVelocity.z = 0.0f;
        return;
    }
    f32 normalX = COLPOLY_GET_NORMAL(wall->normal.x);
    f32 normalZ = COLPOLY_GET_NORMAL(wall->normal.z);
    f32 intoWall = sBallVelocity.x * normalX + sBallVelocity.z * normalZ;

    sBallVelocity.x = (sBallVelocity.x - 2.0f * intoWall * normalX) * BALL_WALL_KEEP;
    sBallVelocity.z = (sBallVelocity.z - 2.0f * intoWall * normalZ) * BALL_WALL_KEEP;
}

static void KeepChainTaut(Player* player) {
    f32 dx = sBallPos.x - player->actor.world.pos.x;
    f32 dz = sBallPos.z - player->actor.world.pos.z;
    f32 reach = sqrtf(SQ(dx) + SQ(dz));

    if (reach > BALL_CHAIN_LENGTH) {
        sBallPos.x = player->actor.world.pos.x + dx * (BALL_CHAIN_LENGTH / reach);
        sBallPos.z = player->actor.world.pos.z + dz * (BALL_CHAIN_LENGTH / reach);
        sBallVelocity.x = sBallVelocity.z = 0.0f;
    }
}

static void LandOnFloor(Player* player, PlayState* play) {
    CollisionPoly* floor = NULL;
    s32 floorBgId;
    Vec3f probe = { sBallPos.x, sBallPos.y + 20.0f, sBallPos.z };
    f32 floorY = BgCheck_EntityRaycastFloor5(&play->colCtx, &floor, &floorBgId, &player->actor, &probe);

    if (floorY <= BGCHECK_Y_MIN || sBallPos.y - BALL_RADIUS > floorY || sBallVelocity.y > 0.0f) {
        if (sBallPos.y < player->actor.world.pos.y - 500.0f) {
            sPhase = BALL_RETRACTING;
        }
        return;
    }
    sBallPos.y = floorY + BALL_RADIUS;
    Audio_PlaySfx_AtPos(&sBallPos, NA_SE_IT_HAMMER_HIT);
    if (++sBounces >= BALL_MAX_BOUNCES || fabsf(sBallVelocity.y) < 3.0f) {
        sBallVelocity.x = sBallVelocity.y = sBallVelocity.z = 0.0f;
        sPhase = BALL_RESTING;
        sRestTimer = BALL_REST_FRAMES;
        return;
    }
    sBallVelocity.y = -sBallVelocity.y * BALL_BOUNCE_KEEP_Y;
    sBallVelocity.x *= BALL_BOUNCE_KEEP_XZ;
    sBallVelocity.z *= BALL_BOUNCE_KEEP_XZ;
}

static void FlyBall(Player* player, PlayState* play) {
    Vec3f previousPos = sBallPos;

    sBallVelocity.y = MAX(sBallVelocity.y + BALL_GRAVITY, -BALL_TERMINAL_FALL);
    sBallPos.x += sBallVelocity.x;
    sBallPos.y += sBallVelocity.y;
    sBallPos.z += sBallVelocity.z;
    KeepChainTaut(player);
    BounceOffWall(play, &previousPos);
    LandOnFloor(player, play);
}

static void ReelBallIn(Player* player, PlayState* play) {
    Vec3f hands = { player->actor.world.pos.x, player->actor.world.pos.y + 45.0f, player->actor.world.pos.z };
    f32 distance = Math_Vec3f_DistXYZ(&sBallPos, &player->actor.world.pos);

    if (distance <= BALL_RETURN_DISTANCE) {
        ReturnBallToHands(player, play);
        return;
    }
    f32 step = BALL_RETRACT_SPEED / MAX(Math_Vec3f_DistXYZ(&sBallPos, &hands), 0.1f);

    sBallPos.x += (hands.x - sBallPos.x) * step;
    sBallPos.y += (hands.y - sBallPos.y) * step;
    sBallPos.z += (hands.z - sBallPos.z) * step;
    Actor_PlaySfx_Flagged2(&player->actor, NA_SE_PL_WALK_GROUND);
}

static void BallThrownAction(Player* player, PlayState* play) {
    Player_UpdateUpperBody(player, play);
    Player_StopHorizontalMovement(player);
    FaceThrowDirection(player);
    if (!IsBallAway() || ++sThrownFrames > BALL_THROW_TIMEOUT) {
        ReturnBallToHands(player, play);
        return;
    }
    switch (sPhase) {
        case BALL_FLYING:
            FlyBall(player, play);
            break;
        case BALL_RESTING:
            if (--sRestTimer <= 0) {
                sPhase = BALL_RETRACTING;
            }
            break;
        case BALL_RETRACTING:
            ReelBallIn(player, play);
            break;
        default:
            ReturnBallToHands(player, play);
            return;
    }
    if (sPhase == BALL_HOLDING) {
        return;
    }
    StrikeAndBreak(player, play);
    if (sPhase != BALL_RESTING) {
        FeedTrail(play);
    }
}

static void TakeOutBall(PlayState* play, Player* player) {
    if (!sIsColliderReady) {
        Collider_InitCylinder(play, &sBallCollider);
        sIsColliderReady = true;
    }
    Collider_SetCylinder(play, &sBallCollider, &player->actor, &sBallColliderInit);
    sPhase = BALL_HOLDING;
    sTrailIndex = -1;
    HoldBallInHands(player);
}

static void StartSpinning(Player* player, PlayState* play) {
    if (sPhase != BALL_HOLDING) {
        return;
    }
    sPhase = BALL_SPINNING;
    sCharge = 0;
    sThrowYaw = player->actor.shape.rot.y;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SWORD_SWING_HARD);
}

static void BallAimAction(Player* player, PlayState* play);

static void StartAiming(Player* player, PlayState* play) {
    if (!Z64Aiming_Request(player, play)) {
        return;
    }
    sPhase = BALL_AIMING;
    Player_SetAction(play, player, BallAimAction, 1);
    Player_StopHorizontalMovement(player);
}

static void StopAiming(Player* player, PlayState* play) {
    Z64Aiming_Release(player, play);
    if (sPhase == BALL_AIMING) {
        sPhase = BALL_SPINNING;
    }
}

static void BallAimAction(Player* player, PlayState* play) {
    Z64Aiming_Update(player, play);
    Player_StopHorizontalMovement(player);
    Player_UpdateUpperBody(player, play);
    if (sPhase != BALL_AIMING) {
        return;
    }
    sCharge = MIN(sCharge + 1, BALL_CHARGE_MAX);
    SwingBall(player);
    StrikeAndBreak(player, play);
    FeedTrail(play);
    if (!IsButtonHeld(play)) {
        ThrowBall(player, play);
        StopAiming(player, play);
        return;
    }
    if (play->state.input[0].press.button & (BTN_CUP | BTN_A | BTN_B)) {
        StopAiming(player, play);
        func_8083B1A0(player, play);
    }
}

static void PutBallAway(Player* player, PlayState* play) {
    if (sPhase == BALL_AIMING) {
        StopAiming(player, play);
    }
    if (player->actionFunc == BallThrownAction || player->actionFunc == BallAimAction) {
        func_8083B1A0(player, play);
    }
    EndTrail(play);
    sBallCollider.base.atFlags &= ~(AT_ON | AT_HIT);
    sPhase = BALL_IDLE;
    player->upperLimbRot.x = player->upperLimbRot.y = player->upperLimbRot.z = 0;
    AudioSfx_StopById(NA_SE_IT_SWORD_SWING_HARD);
    AudioSfx_StopById(NA_SE_IT_HOOKSHOT_CHAIN - SFX_FLAG);
    AudioSfx_StopById(NA_SE_PL_WALK_GROUND);
}

// The queued frame lands in sPoseJoints when the animation queue runs, which is after every actor update and
// before any actor is drawn: posing an arm during the update itself is undone by Link's own animation.
static void RequestArmPose(PlayState* play) {
    bool isHeld = sPhase == BALL_HOLDING;

    AnimTaskQueue_AddLoadPlayerFrame(play, (PlayerAnimationHeader*)(isHeld ? sHoldPoseAnim : sSpinPoseAnim),
                                     isHeld ? BALL_HOLD_POSE_FRAME : BALL_SPIN_POSE_FRAME, PLAYER_LIMB_MAX,
                                     sPoseJoints);
}

// Goron strength swings it with the throwing arm alone, so the left arm keeps whatever Link is animating.
static void ApplyArmPose(Player* player) {
    u32 first = IsOneHanded() ? ARRAY_COUNT(sArmLimbs) / 2 : 0;

    for (u32 i = first; i < ARRAY_COUNT(sArmLimbs); i++) {
        player->skelAnime.jointTable[sArmLimbs[i]] = sPoseJoints[sArmLimbs[i]];
    }
}

// Iron Boots are what vanilla checks before a fan or a current may push Link: the ball weighs at least as much.
static void AnchorAgainstWind(Player* player) {
    player->pushedSpeed = 0.0f;
}

// The overhead spin orbits in Link's own frame, the one-handed guard orbits in world space.
static s16 GetBallBearing(Player* player) {
    return IsOneHanded() ? (s16)(sSpinAngle - player->actor.shape.rot.y) : sSpinAngle;
}

// The ball drags Link a few degrees after it, harder the faster it goes. Weaker arms take the pull in the
// torso; Goron strength holds its ground and only the swinging shoulder gives.
static void LeanIntoSwing(Player* player) {
    s16 bearing = GetBallBearing(player);
    f32 pull = BALL_LEAN_MIN + (BALL_LEAN_MAX - BALL_LEAN_MIN) * sCharge / BALL_CHARGE_MAX;
    s16 forwards = (s16)(Math_CosS(bearing) * pull);
    s16 sideways = (s16)(-Math_SinS(bearing) * pull);

    if (IsOneHanded()) {
        Vec3s* shoulder = &player->skelAnime.jointTable[PLAYER_LIMB_RIGHT_SHOULDER];

        shoulder->y -= forwards;
        shoulder->z += sideways;
        player->upperLimbRot.x = player->upperLimbRot.y = player->upperLimbRot.z = 0;
        return;
    }
    player->upperLimbRot.x = 0;
    player->upperLimbRot.z = 0;
    player->upperLimbRot.y = 0;
}

// Runs just before Link is drawn, the only point in the frame where the joint table is his final pose.
static void PoseArmsAroundBall(Actor* actor, bool* should) {
    Player* player = (Player*)actor;

    if (sPhase == BALL_IDLE) {
        return;
    }
    ApplyArmPose(player);
    if (sPhase == BALL_HOLDING) {
        player->upperLimbRot.x = player->upperLimbRot.y = player->upperLimbRot.z = 0;
        return;
    }
    if (IsBallAway()) {
        player->upperLimbRot.x = BALL_THROW_LEAN;
        player->upperLimbRot.y = player->upperLimbRot.z = 0;
        return;
    }
    LeanIntoSwing(player);
}

static void KeepBallStateSane(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || sPhase == BALL_IDLE) {
        return;
    }
    AnchorAgainstWind(player);
    if (sPhase == BALL_AIMING && player->actionFunc != BallAimAction) {
        StopAiming(player, play);
    }
    if (IsBallAway() && player->actionFunc != BallThrownAction) {
        ReturnBallToHands(player, play);
    }
    RequestArmPose(play);
}

// A and B stop the swing, and vanilla would read the same press as "roll" or "draw the sword". The mod reads
// the raw input, so taking them off Link's copy for those frames costs it nothing.
static void KeepSwingButtons(Player* player, Input* input) {
    if (sPhase == BALL_IDLE || sPhase == BALL_HOLDING) {
        return;
    }
    input->cur.button &= ~(BTN_A | BTN_B | BTN_R);
    input->press.button &= ~(BTN_A | BTN_B | BTN_R);
}

// Whole strands laid end to end keep the links their own size and leave no gap between them.
static void DrawChain(PlayState* play, Vec3f* from, Vec3f* to) {
    f32 dx = to->x - from->x;
    f32 dy = to->y - from->y;
    f32 dz = to->z - from->z;
    f32 span = sqrtf(SQ(dx) + SQ(dy) + SQ(dz));
    f32 yaw = Math_Atan2F_XY(dz, dx);
    f32 pitch = Math_Atan2F_XY(sqrtf(SQ(dx) + SQ(dz)), -dy);
    s32 strands = CLAMP((s32)(span / BALL_CHAIN_STRAND_SPAN) + 1, 1, BALL_CHAIN_STRAND_MAX);
    f32 strandSpan = span / strands;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 85, 85, 85, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 40, 40, 40, 255);
    for (s32 i = 0; i < strands; i++) {
        f32 t = (f32)i / strands;

        Matrix_Translate(from->x + dx * t, from->y + dy * t, from->z + dz * t, MTXMODE_NEW);
        Matrix_RotateYF(yaw, MTXMODE_APPLY);
        Matrix_RotateXF(pitch, MTXMODE_APPLY);
        Matrix_Scale(BALL_CHAIN_THICKNESS, BALL_CHAIN_THICKNESS, strandSpan * BALL_CHAIN_STRETCH, MTXMODE_APPLY);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sChainLinkDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawBall(PlayState* play) {
    bool isSwung = sPhase != BALL_HOLDING;
    f32 scale = isSwung ? BALL_SWUNG_SCALE : BALL_HELD_SCALE;
    s16 tumble = (s16)(play->gameplayFrames * 0x400);

    OPEN_DISPS(play->state.gfxCtx);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BACK | G_LIGHTING);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_SHADE | G_SHADING_SMOOTH | G_CULL_BACK | G_LIGHTING | G_ZBUFFER);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_SHADE, G_CC_SHADE);
    Matrix_Translate(sBallPos.x, sBallPos.y, sBallPos.z, MTXMODE_NEW);
    if (isSwung) {
        Matrix_RotateYF(BINANG_TO_RAD(tumble), MTXMODE_APPLY);
        Matrix_RotateXF(BINANG_TO_RAD(tumble / 2), MTXMODE_APPLY);
    }
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBallDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawBallAndChain(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (sPhase == BALL_IDLE || (player->stateFlags2 & PLAYER_STATE2_20000000)) {
        return;
    }
    DrawChain(play, &player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND], &player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND]);
    DrawChain(play, &player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND], &sBallPos);
    DrawBall(play);
}

// Shuffled once by the randomizer; no vanilla logic needs it.
static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, BALL_KEY, BALL_KEY, "",
};

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }

    S2HCustomItemDefinition ball = Z64Items_Define(BALL_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&ball, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&ball, 0, 9, 0);
    Z64Items_SetTextbox(&ball, "You got the %rBall and Chain%w!&A massive iron ball that only the&strongest arms "
                               "can swing.^Press a %yC button%w again and hold it to spin&the ball, then let go to "
                               "%rhurl%w it.^Its weight roots you where %ywind%w&would move anyone else, and&it "
                               "crushes %yarmor%w and shatters %cice%w.^Stronger forms carry it "
                               "farther:&the %gZora%w frees your feet&and your aim, and the %gGoron%w swings it "
                               "one-handed to %rguard%w you.");
    Z64Items_SetPauseText(&ball, "%rBall and Chain&%wHold a %yC button%w to spin, release to throw.&%gZ%w or "
                                 "%yC-Up%w to aim.");
    Z64Items_SetCanUse(&ball, CanLiftBall);
    Z64Items_SetAction(&ball, TakeOutBall, UpdateBallInHand);
    Z64Items_SetHeldCallbacks(&ball, StartSpinning, PutBallAway, NULL);
    Z64Items_SetGetItemModel(&ball, sGiveDL);
    ball.getItemScale = 0.13333333f;
    ball.modelGroup = PLAYER_MODELGROUP_DEFAULT;

    Z64Items_SetLogic(&ball, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &ball)) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, KeepBallStateSane);
    S2H_REGISTER_HOOK(sApi, OnPlayerFilterInput, KeepSwingButtons);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveMotionScale, WeighDownPlayer);
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorDraw, ACTOR_PLAYER, PoseArmsAroundBall);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, DrawBallAndChain);
}
