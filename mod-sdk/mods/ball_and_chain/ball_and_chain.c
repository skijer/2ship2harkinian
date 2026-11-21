#include <math.h>
#include <string.h>

#include "z64items.h"
#include "z64logic.h"
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
#include "overlays/actors/ovl_En_Go/z_en_go.h"
#include "overlays/actors/ovl_Bg_Dblue_Waterfall/z_bg_dblue_waterfall.h"
#include "overlays/actors/ovl_Boss_Hakugin/z_boss_hakugin.h"

#define BALL_KEY "nei.ball_and_chain"
#define BALL_BUTTON_COUNT S2H_ITEM_BUTTON_COUNT
#define BALL_DAMAGE 8
#define BALL_COLLIDER_RADIUS 20
#define BALL_COLLIDER_HEIGHT 20
#define BALL_RADIUS 12.0f
#define BALL_WALL_HEIGHT 20.0f
#define BALL_ICE_REACH 140.0f
#define BALL_BIGICE_REACH 200.0f
#define BALL_FIRE_DAMAGE 2
// Fire arrow damage effects, private to each overlay: ENGO_DMGEFF_FIRE, EN_SNOWMAN_DMGEFF_MELT and En_Fz's case 2.
#define BALL_GORON_DMGEFF_FIRE 2
#define BALL_EENO_DMGEFF_MELT 2
#define BALL_FREEZARD_DMGEFF_FIRE 2
#define BALL_HOLD_WALK_SCALE 0.5f
#define BALL_SPIN_WALK_SCALE 0.35f
#define BALL_GUARD_RADIUS_MIN 45.0f
#define BALL_GUARD_RADIUS_MAX 70.0f
#define BALL_GUARD_HEIGHT 30.0f
#define BALL_GOLDEN_DAMAGE 16
#define BALL_HOLD_DROP 5.0f
#define BALL_CHARGE_MAX 60
#define BALL_CHARGE_MIN 15
#define BALL_CHARGE_TAP 5
#define BALL_SPIN_RADIUS 20.0f
#define BALL_SPIN_HEIGHT_MIN 50.0f
#define BALL_SPIN_HEIGHT_MAX 55.0f
#define BALL_SPIN_SPEED_MIN 0x1000
#define BALL_SPIN_SPEED_MAX 0x2000
#define BALL_QUICK_SPIN_MULTIPLIER 2.5f
#define BALL_STICK_DEADZONE 5.0f
#define BALL_LEAN_TILT 40.0f
#define BALL_WEAK_LEAN_SCALE 0.2f
#define BALL_THROW_LEAN 3000
#define BALL_LEAN_MIN 182.0f
#define BALL_LEAN_MAX 910.0f
#define BALL_HOLD_POSE_FRAME 8
#define BALL_SPIN_POSE_FRAME 0
#define BALL_LAUNCH_SPEED_SHORT 10.0f
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
#define BALL_CHAIN_STRAND_SPAN 100.0f
#define BALL_CHAIN_STRAND_MAX 8
#define BALL_CHAIN_THICKNESS 0.02f
#define BALL_CHAIN_STRETCH 0.01f
#define BALL_CHAIN_ARC_SEGMENTS 8
#define BALL_CHAIN_ARC_HEIGHT 35.0f
#define BALL_GIVE_SCALE 0.25f

typedef enum {
    BALL_IDLE,
    BALL_HOLDING,
    BALL_SPINNING,
    BALL_AIMING,
    BALL_FLYING,
    BALL_RESTING,
    BALL_RETRACTING,
} BallPhase;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconBallAndChainTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gBallAndChainNameTex";
static const ALIGN_ASSET(2) char sBallDL[] = "__OTR__objects/object_nei_ball_and_chain/gBallDL";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_ball_and_chain/g_ball_and_chain_dl";
static const ALIGN_ASSET(2) char sChainLinkDL[] = "__OTR__objects/gameplay_keep/gHookshotChainDL";

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

static ColliderElement sFireArrowTouch = { { DMG_FIRE_ARROW, 0x00, BALL_FIRE_DAMAGE } };

static const char* const sRequiredHooks[] = { "OnGameFrameUpdate",
                                              "OnPlayerUpdate",
                                              "ShouldActorDraw",
                                              "OnActorDrawEnd",
                                              "OnPlayerFilterInput",
                                              "OnPlayerResolveMotionScale",
                                              "OnPlayerResolveItemActionInit" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;

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

static ColliderCylinder sBallCollider;
static bool sIsColliderReady;
static u8 sPhase;
static s16 sCharge;
static s16 sSpinAngle;
static s16 sThrowYaw;
static s16 sQuickSpinFrames;
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
void EnGo_Frozen(EnGo* goron, PlayState* play);
void BossHakugin_FrozenBeforeFight(BossHakugin* goht, PlayState* play);
s32 func_80B83D04(BgDblueWaterfall* waterfall, PlayState* play);

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

static bool HasOnlyLiftingStrength(void) {
    return Strength() == PLAYER_STRENGTH_HUMAN;
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

static bool CanMeltIce(void) {
    return Strength() >= PLAYER_STRENGTH_GORON;
}

static void StartTrail(PlayState* play) {
    EffectBlureInit2 init = {
        0, 8, 0, { 255, 255, 255, 255 }, { 200, 220, 255, 128 }, { 255, 255, 255, 0 }, { 200, 220, 255, 0 }, 4,
        0, 2, 0, { 235, 235, 245, 200 }, { 180, 190, 210, 96 },
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

static void StrikeWithBall(Player* player, PlayState* play) {
    bool isAtFootLevel = sBallPos.y - player->actor.world.pos.y < 30.0f;

    sBallCollider.elem.atDmgInfo.dmgFlags = isAtFootLevel ? DMG_GORON_POUND : DMG_GORON_PUNCH;
    sBallCollider.elem.atDmgInfo.damage = IsOneHanded() ? BALL_GOLDEN_DAMAGE : BALL_DAMAGE;
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

static bool IsBallNear(Actor* actor, f32 reach) {
    return Math_Vec3f_DistXYZ(&sBallPos, &actor->world.pos) < reach;
}

static bool IsBallInCylinder(ColliderCylinder* cylinder) {
    f32 dx = sBallPos.x - cylinder->dim.pos.x;
    f32 dz = sBallPos.z - cylinder->dim.pos.z;
    f32 bottom = cylinder->dim.pos.y + cylinder->dim.yShift;

    return sqrtf(SQ(dx) + SQ(dz)) < cylinder->dim.radius + BALL_COLLIDER_RADIUS &&
           sBallPos.y > bottom - BALL_COLLIDER_RADIUS && sBallPos.y < bottom + cylinder->dim.height + BALL_COLLIDER_RADIUS;
}

// Ice melts through the fire arrow branch of its own update: a forged AC hit is read before its next SetAC reset.
static void TouchWithFire(ColliderCylinder* cylinder, Player* player) {
    cylinder->base.acFlags |= AC_HIT;
    cylinder->base.ac = &player->actor;
    cylinder->elem.acHitElem = &sFireArrowTouch;
}

static void BurnWithFire(ColliderCylinder* cylinder, Actor* target, Player* player, u8 damageEffect) {
    TouchWithFire(cylinder, player);
    target->colChkInfo.damageEffect = damageEffect;
    target->colChkInfo.damage = BALL_FIRE_DAMAGE;
}

static bool IsStandingOn(Player* player, PlayState* play, Actor* actor) {
    return DynaPoly_GetActor(&play->colCtx, player->actor.floorBgId) == (DynaPolyActor*)actor;
}

static void MeltIcePoly(Player* player, ObjIcePoly* ice) {
    for (u32 i = 0; i < ARRAY_COUNT(ice->colliders2); i++) {
        if (IsBallInCylinder(&ice->colliders2[i])) {
            TouchWithFire(&ice->colliders2[i], player);
            return;
        }
    }
}

static void MeltIceBlock(Player* player, PlayState* play, ObjIceblock* block) {
    if (IsStandingOn(player, play, &block->dyna.actor) || !IsBallInCylinder(&block->collider)) {
        return;
    }
    TouchWithFire(&block->collider, player);
}

static void ThawGoron(Player* player, EnGo* goron) {
    if (goron->actionFunc != EnGo_Frozen || !IsBallInCylinder(&goron->colliderCylinder)) {
        return;
    }
    BurnWithFire(&goron->colliderCylinder, &goron->actor, player, BALL_GORON_DMGEFF_FIRE);
}

static void ThawWaterfall(Player* player, PlayState* play, BgDblueWaterfall* waterfall) {
    if (!func_80B83D04(waterfall, play) || !IsBallInCylinder(&waterfall->collider)) {
        return;
    }
    TouchWithFire(&waterfall->collider, player);
}

static void ThawGoht(Player* player, BossHakugin* goht) {
    if (goht->actionFunc != BossHakugin_FrozenBeforeFight || !IsBallInCylinder(&goht->iceCollider)) {
        return;
    }
    TouchWithFire(&goht->iceCollider, player);
}

static void MeltFreezard(Player* player, EnFz* freezard) {
    if (freezard->unk_BD8 != 0 || !IsBallNear(&freezard->actor, BALL_ICE_REACH)) {
        return;
    }
    BurnWithFire(&freezard->collider1, &freezard->actor, player, BALL_FREEZARD_DMGEFF_FIRE);
}

static void MeltEeno(Player* player, EnSnowman* snowman) {
    if (!(snowman->collider.base.acFlags & AC_ON) || !IsBallNear(&snowman->actor, BALL_ICE_REACH)) {
        return;
    }
    BurnWithFire(&snowman->collider, &snowman->actor, player, BALL_EENO_DMGEFF_MELT);
}

static void MeltLikeFireArrow(Player* player, PlayState* play, Actor* actor) {
    switch (actor->id) {
        case ACTOR_OBJ_ICE_POLY:
            MeltIcePoly(player, (ObjIcePoly*)actor);
            break;
        case ACTOR_OBJ_ICEBLOCK:
            MeltIceBlock(player, play, (ObjIceblock*)actor);
            break;
        case ACTOR_EN_GO:
            ThawGoron(player, (EnGo*)actor);
            break;
        case ACTOR_BG_DBLUE_WATERFALL:
            ThawWaterfall(player, play, (BgDblueWaterfall*)actor);
            break;
        case ACTOR_BOSS_HAKUGIN:
            ThawGoht(player, (BossHakugin*)actor);
            break;
        case ACTOR_EN_FZ:
            MeltFreezard(player, (EnFz*)actor);
            break;
        case ACTOR_EN_SNOWMAN:
            MeltEeno(player, (EnSnowman*)actor);
            break;
        default:
            break;
    }
}

static void ShatterIcicle(Actor* actor) {
    switch (actor->id) {
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
        default:
            break;
    }
}

static void BreakWhatTheBallReaches(Player* player, PlayState* play) {
    static const u8 sCategories[] = { ACTORCAT_BG,   ACTORCAT_NPC,        ACTORCAT_ENEMY,
                                      ACTORCAT_PROP, ACTORCAT_ITEMACTION, ACTORCAT_BOSS };
    bool canMelt = CanMeltIce();

    for (u32 i = 0; i < ARRAY_COUNT(sCategories); i++) {
        Actor* next;

        for (Actor* actor = play->actorCtx.actorLists[sCategories[i]].first; actor != NULL; actor = next) {
            next = actor->next;
            ShatterIcicle(actor);
            if (canMelt) {
                MeltLikeFireArrow(player, play, actor);
            }
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
    BreakWhatTheBallReaches(player, play);
}

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
    player->actor.shape.rot.y = sThrowYaw;
    player->actor.world.rot.y = sThrowYaw;
    player->yaw = sThrowYaw;
}

// A zero motion scale also stops vanilla turning, so arms that cannot walk with the ball turn on the spot here.
static void TurnBallInPlace(Player* player, PlayState* play) {
    if (CanWalkWithBall() || Player_IsZTargeting(player)) {
        return;
    }
    f32 speed;
    s16 yaw;

    if (Player_GetMovementSpeedAndYaw(player, &speed, &yaw, 1.0f, play)) {
        Math_ScaledStepToS(&player->actor.shape.rot.y, yaw, 0x7D0);
        player->actor.world.rot.y = player->actor.shape.rot.y;
        player->yaw = player->actor.shape.rot.y;
    }
    Player_StopHorizontalMovement(player);
}

static void ReadStickLean(Player* player, PlayState* play) {
    s8 stickX = play->state.input[0].cur.stick_x;
    s8 stickY = play->state.input[0].cur.stick_y;

    sStickLeanX = 0.0f;
    sStickLeanY = 0.0f;
    if (!Player_IsZTargeting(player) && sqrtf(SQ(stickX) + SQ(stickY)) > BALL_STICK_DEADZONE) {
        sStickLeanX = stickX / 127.0f;
        sStickLeanY = stickY / 127.0f;
    }
}

static s16 SpinStep(void) {
    f32 speed = BALL_SPIN_SPEED_MIN + (BALL_SPIN_SPEED_MAX - BALL_SPIN_SPEED_MIN) * (f32)sCharge / BALL_CHARGE_MAX;
    return (s16)(speed * (sQuickSpinFrames > 0 ? BALL_QUICK_SPIN_MULTIPLIER : 1.0f));
}

static void OrbitBallOverhead(Player* player) {
    f32 chargeRatio = (f32)sCharge / BALL_CHARGE_MAX;
    f32 tilt = HasOnlyLiftingStrength() ? BALL_LEAN_TILT * BALL_WEAK_LEAN_SCALE : BALL_LEAN_TILT;
    f32 orbitX = Math_SinS(sSpinAngle) * BALL_SPIN_RADIUS;
    f32 orbitZ = Math_CosS(sSpinAngle) * BALL_SPIN_RADIUS;
    f32 orbitY = BALL_SPIN_HEIGHT_MIN + (BALL_SPIN_HEIGHT_MAX - BALL_SPIN_HEIGHT_MIN) * chargeRatio -
                 Math_CosS(sSpinAngle) * sStickLeanY * tilt - Math_SinS(sSpinAngle) * sStickLeanX * tilt;
    s16 yaw = player->actor.shape.rot.y;

    sSpinAngle += SpinStep();
    sBallPos.x = player->actor.world.pos.x + orbitX * Math_CosS(yaw) + orbitZ * Math_SinS(yaw);
    sBallPos.y = player->actor.world.pos.y + orbitY;
    sBallPos.z = player->actor.world.pos.z - orbitX * Math_SinS(yaw) + orbitZ * Math_CosS(yaw);
}

static void BallThrownAction(Player* player, PlayState* play);

static void ReturnBallToHands(Player* player, PlayState* play) {
    sPhase = BALL_HOLDING;
    EndTrail(play);
    AudioSfx_StopById(NA_SE_PL_WALK_GROUND - SFX_FLAG);
    if (player->actionFunc == BallThrownAction) {
        func_8083B1A0(player, play);
    }
}

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

static f32 GetLaunchSpeed(void) {
    if (sCharge < BALL_CHARGE_MIN) {
        f32 windUp = (f32)(sCharge - BALL_CHARGE_TAP) / (BALL_CHARGE_MIN - BALL_CHARGE_TAP);
        return BALL_LAUNCH_SPEED_SHORT + (BALL_LAUNCH_SPEED_MIN - BALL_LAUNCH_SPEED_SHORT) * windUp;
    }
    return BALL_LAUNCH_SPEED_MIN + (BALL_LAUNCH_SPEED_MAX - BALL_LAUNCH_SPEED_MIN) * sCharge / BALL_CHARGE_MAX;
}

static void ThrowBall(Player* player, PlayState* play) {
    f32 launchSpeed = GetLaunchSpeed();

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
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_HAMMER_SWING);
}

static void OrbitBallAround(Player* player) {
    f32 chargeRatio = (f32)sCharge / BALL_CHARGE_MAX;
    f32 radius = BALL_GUARD_RADIUS_MIN + (BALL_GUARD_RADIUS_MAX - BALL_GUARD_RADIUS_MIN) * chargeRatio;

    sSpinAngle += SpinStep();
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

static bool IsBallWoundUp(void) {
    return sCharge >= BALL_CHARGE_TAP;
}

// The counter is raised first so SpinStep measures the quick step; the turn lasts one full revolution.
static void StartQuickSpin(void) {
    sQuickSpinFrames = 1;
    s32 step = SpinStep();

    sQuickSpinFrames = (s16)((0x10000 + step - 1) / step);
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
    if (HasOnlyLiftingStrength()) {
        FaceThrowDirection(player);
        player->upperLimbYawSecondary = 0;
    }
    bool isHeld = IsButtonHeld(play);

    if (sQuickSpinFrames == 0 && !isHeld && !IsBallWoundUp() && CanAimBall()) {
        StartQuickSpin();
    }
    ReadStickLean(player, play);
    if (sQuickSpinFrames == 0) {
        sCharge = MIN(sCharge + 1, BALL_CHARGE_MAX);
    }
    SwingBall(player);
    StrikeAndBreak(player, play);
    FeedTrail(play);
    Actor_PlaySfx_Flagged(&player->actor, NA_SE_IT_SWORD_SWING - SFX_FLAG);
    if (sQuickSpinFrames > 0) {
        sQuickSpinFrames--;
        if (sQuickSpinFrames == 0) {
            sPhase = BALL_HOLDING;
        }
        return;
    }
    if (isHeld) {
        return;
    }
    if (!IsBallWoundUp()) {
        sPhase = BALL_HOLDING;
        return;
    }
    ThrowBall(player, play);
}

static void GuardWithHeldBall(PlayState* play) {
    sBallCollider.dim.pos.x = sBallPos.x;
    sBallCollider.dim.pos.y = sBallPos.y - BALL_COLLIDER_HEIGHT / 2;
    sBallCollider.dim.pos.z = sBallPos.z;
    if (sBallCollider.base.acFlags & AC_HIT) {
        sBallCollider.base.acFlags &= ~AC_HIT;
        Audio_PlaySfx_AtPos(&sBallPos, NA_SE_IT_SHIELD_BOUND);
    }
    CollisionCheck_SetAC(play, &play->colChkCtx, &sBallCollider.base);
}

static int32_t UpdateBallInHand(Player* player, PlayState* play) {
    if ((player->stateFlags1 & PLAYER_STATE1_8000000) || player->invincibilityTimer < 0) {
        sPhase = BALL_HOLDING;
    }
    if (sPhase == BALL_HOLDING) {
        TurnBallInPlace(player, play);
        HoldBallInHands(player);
        GuardWithHeldBall(play);
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
        sBallVelocity.x = 0.0f;
        sBallVelocity.z = 0.0f;
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
        sBallVelocity.x = 0.0f;
        sBallVelocity.z = 0.0f;
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
    sBounces++;
    if (sBounces >= BALL_MAX_BOUNCES || fabsf(sBallVelocity.y) < 3.0f) {
        sBallVelocity = gZeroVec3f;
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
    Actor_PlaySfx_Flagged(&player->actor, NA_SE_PL_WALK_GROUND - SFX_FLAG);
}

static void BallThrownAction(Player* player, PlayState* play) {
    Player_UpdateUpperBody(player, play);
    Player_StopHorizontalMovement(player);
    FaceThrowDirection(player);
    sThrownFrames++;
    if (!IsBallAway() || sThrownFrames > BALL_THROW_TIMEOUT) {
        ReturnBallToHands(player, play);
        return;
    }
    switch (sPhase) {
        case BALL_FLYING:
            FlyBall(player, play);
            break;
        case BALL_RESTING:
            sRestTimer--;
            if (sRestTimer <= 0) {
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
    sQuickSpinFrames = 0;
    sThrowYaw = player->actor.shape.rot.y;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_HAMMER_SWING);
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

static void ClearUpperLimbRot(Player* player) {
    player->upperLimbRot.x = 0;
    player->upperLimbRot.y = 0;
    player->upperLimbRot.z = 0;
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
    ClearUpperLimbRot(player);
    AudioSfx_StopById(NA_SE_IT_SWORD_SWING - SFX_FLAG);
    AudioSfx_StopById(NA_SE_PL_WALK_GROUND - SFX_FLAG);
}

// Posing an arm during the update is undone by Link's own animation; the queued frame lands before any draw.
static void RequestArmPose(PlayState* play) {
    bool isHeld = sPhase == BALL_HOLDING;

    AnimTaskQueue_AddLoadPlayerFrame(play, (PlayerAnimationHeader*)(isHeld ? sHoldPoseAnim : sSpinPoseAnim),
                                     isHeld ? BALL_HOLD_POSE_FRAME : BALL_SPIN_POSE_FRAME, PLAYER_LIMB_MAX,
                                     sPoseJoints);
}

static void ApplyArmPose(Player* player) {
    u32 first = IsOneHanded() ? ARRAY_COUNT(sArmLimbs) / 2 : 0;

    for (u32 i = first; i < ARRAY_COUNT(sArmLimbs); i++) {
        player->skelAnime.jointTable[sArmLimbs[i]] = sPoseJoints[sArmLimbs[i]];
    }
}

static void AnchorAgainstWind(Player* player) {
    player->pushedSpeed = 0.0f;
}

static s16 GetBallBearing(Player* player) {
    return IsOneHanded() ? (s16)(sSpinAngle - player->actor.shape.rot.y) : sSpinAngle;
}

static void LeanIntoSwing(Player* player) {
    s16 bearing = GetBallBearing(player);
    f32 pull = BALL_LEAN_MIN + (BALL_LEAN_MAX - BALL_LEAN_MIN) * sCharge / BALL_CHARGE_MAX;

    if (HasOnlyLiftingStrength()) {
        pull *= BALL_WEAK_LEAN_SCALE;
    }
    s16 forwards = (s16)(Math_CosS(bearing) * pull);
    s16 sideways = (s16)(-Math_SinS(bearing) * pull);

    if (IsOneHanded()) {
        Vec3s* shoulder = &player->skelAnime.jointTable[PLAYER_LIMB_RIGHT_SHOULDER];

        shoulder->y -= forwards;
        shoulder->z += sideways;
        ClearUpperLimbRot(player);
        return;
    }
    player->upperLimbRot.x = forwards;
    player->upperLimbRot.y = 0;
    player->upperLimbRot.z = sideways;
}

static void PoseArmsAroundBall(Actor* actor, bool* should) {
    Player* player = (Player*)actor;

    if (gPlayState == NULL || actor != &GET_PLAYER(gPlayState)->actor || sPhase == BALL_IDLE) {
        return;
    }
    ApplyArmPose(player);
    if (HasOnlyLiftingStrength() && sPhase == BALL_SPINNING) {
        FaceThrowDirection(player);
        player->upperLimbYawSecondary = 0;
    }
    if (sPhase == BALL_HOLDING) {
        ClearUpperLimbRot(player);
        return;
    }
    if (IsBallAway()) {
        ClearUpperLimbRot(player);
        player->upperLimbRot.x = BALL_THROW_LEAN;
        return;
    }
    LeanIntoSwing(player);
}

static void KeepBallStateSane(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || player != GET_PLAYER(play) || sPhase == BALL_IDLE) {
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

static void KeepSwingButtons(Player* player, Input* input) {
    if (sPhase == BALL_IDLE || sPhase == BALL_HOLDING) {
        return;
    }
    input->cur.button &= ~(BTN_A | BTN_B | BTN_R);
    input->press.button &= ~(BTN_A | BTN_B | BTN_R);
}

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

static void DrawOverheadChain(PlayState* play, Vec3f* from, Vec3f* to) {
    Vec3f previous = *from;

    for (s32 i = 1; i <= BALL_CHAIN_ARC_SEGMENTS; i++) {
        f32 t = (f32)i / BALL_CHAIN_ARC_SEGMENTS;
        Vec3f point = { from->x + (to->x - from->x) * t,
                        from->y + (to->y - from->y) * t + 4.0f * t * (1.0f - t) * BALL_CHAIN_ARC_HEIGHT,
                        from->z + (to->z - from->z) * t };

        DrawChain(play, &previous, &point);
        previous = point;
    }
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

static bool IsChainArched(void) {
    return IsOneHanded() && (sPhase == BALL_SPINNING || sPhase == BALL_AIMING);
}

static void DrawChainAndBall(PlayState* play, Player* player, bool isArched) {
    Vec3f* left = &player->bodyPartsPos[PLAYER_BODYPART_LEFT_HAND];
    Vec3f* right = &player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND];

    DrawChain(play, left, right);
    if (isArched) {
        DrawOverheadChain(play, right, &sBallPos);
    } else {
        DrawChain(play, right, &sBallPos);
    }
    DrawBall(play);
}

static void DrawRemoteBallAndChain(Player* player, PlayState* play) {
    RemoteVisualState visual;

    if (!ReadRemoteVisual(player, &visual)) {
        return;
    }
    Vec3f savedPosition = sBallPos;
    u8 savedPhase = sPhase;

    sBallPos.x = visual.values[0];
    sBallPos.y = visual.values[1];
    sBallPos.z = visual.values[2];
    sPhase = visual.flags[0] ? BALL_SPINNING : BALL_HOLDING;
    DrawChainAndBall(play, player, visual.flags[1]);
    sPhase = savedPhase;
    sBallPos = savedPosition;
}

static void DrawBallAndChain(Actor* actor, PlayState* play) {
    Player* player = (Player*)actor;

    if (player != GET_PLAYER(play)) {
        DrawRemoteBallAndChain(player, play);
        return;
    }
    if (sPhase == BALL_AIMING) {
        Z64Aiming_DrawReticle(play, player, BALL_CHAIN_LENGTH,
                              (Z64AimingReticleStyle){ Z64AIMING_TOGGLE_HOOKSHOT, Z64AIMING_TINT_LAUNCH });
    }
    if (sPhase == BALL_IDLE || (player->stateFlags2 & PLAYER_STATE2_20000000)) {
        return;
    }
    DrawChainAndBall(play, player, IsChainArched());
}

static void DrawGetItem(PlayState* play, S2HGetItemEntry* getItemEntry) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(BALL_GIVE_SCALE, BALL_GIVE_SCALE, BALL_GIVE_SCALE, MTXMODE_APPLY);
    Matrix_RotateYF((s16)(play->gameplayFrames * 2) * 0.01f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sGiveDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, BALL_KEY, BALL_KEY, "",
};

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
        sApi->GetNetPeerState(peerId, "nei.ball_and_chain.visual", visual, sizeof(*visual)) != sizeof(*visual)) {
        return false;
    }
    if (visual->active != 1 || visual->adult > 1 || visual->scene != gPlayState->sceneId || visual->flags[0] > 1 ||
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
        visual.active = (sPhase != BALL_IDLE);
        if (visual.active) {
            visual.flags[0] = sPhase != BALL_HOLDING;
            visual.flags[1] = IsChainArched();
            visual.values[0] = sBallPos.x;
            visual.values[1] = sBallPos.y;
            visual.values[2] = sBallPos.z;
        }
        if (sRemoteVisualSent && (uint32_t)(play->gameplayFrames - sRemoteVisualFrame) < REMOTE_VISUAL_INTERVAL) {
            return;
        }
    }
    if (sRemoteVisualSent && memcmp(&visual, &sLastRemoteVisual, sizeof(visual)) == 0) {
        return;
    }
    if (sApi->SetNetPeerState("nei.ball_and_chain.visual", &visual, sizeof(visual))) {
        sLastRemoteVisual = visual;
        sRemoteVisualSent = true;
        sRemoteVisualFrame = play != NULL ? play->gameplayFrames : 0;
    }
}

static void DrawRemotePlayer(Actor* actor, PlayState* play) {
    if (actor->id == ACTOR_PLAYER || !sRemoteVisualReady || sApi->GetNetPeerIdForActor(actor) == 0) {
        return;
    }
    DrawBallAndChain(actor, play);
}

static void InitRemoteVisuals(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterNetChannel) || !S2H_MOD_API_HAS(sApi, SetNetPeerState) ||
        !S2H_MOD_API_HAS(sApi, GetNetPeerState) || !S2H_MOD_API_HAS(sApi, GetNetPeerIdForActor)) {
        return;
    }
    sRemoteVisualReady = sApi->RegisterNetChannel("nei.ball_and_chain.visual", 1, ReceiveRemoteVisual);
    if (sRemoteVisualReady) {
        S2H_REGISTER_HOOK(sApi, OnGameFrameUpdate, PublishRemoteVisual);
        S2H_REGISTER_HOOK(sApi, OnActorDrawEnd, DrawRemotePlayer);
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

static void GrantBallLogic(void) {
    static const Z64LogicSource sBall = { BALL_KEY };
    static const char* const sSmashing[] = { "BlastOrSmash", "CanHitSwitch", "CanAttack", "CanDamage", NULL };

    if (Z64Logic_Init(sApi)) {
        Z64Logic_GrantCapabilities(&sBall, sSmashing);
        Z64Logic_GrantBoulderSmashing(&sBall);
    }
}

S2H_MOD_EXPORT void ModInit(void) {
    GrantBallLogic();
    InitRemoteVisuals();
    if (!S2H_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }

    S2HCustomItemDefinition ball = Z64Items_Define(BALL_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&ball, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&ball, 0, 9, 0);
    Z64Items_SetTextbox(&ball, "You got the %rBall and Chain%w! A&massive iron ball that only the&"
                               "strongest arms can swing.^"
                               "Press %y\xB2%w again and hold it to&spin the ball, then let go to&%rhurl%w it.^"
                               "Its weight roots you where&%ywind%w would move anyone else,&"
                               "and it crushes %yarmor%w and&shatters %bice%w.^"
                               "Stronger forms carry it&farther: the %gZora%w frees your&feet and your aim,^"
                               "and the %gGoron%w swings it&one-handed to %rguard%w you,&"
                               "hard enough to melt %bice%w&like a %rfire arrow%w.");
    Z64Items_SetPauseText(&ball, "%rBall and Chain&%wHold %y\xB2%w to spin, release to throw. %g\xB5%w&"
                                 "or %y\xB6%w to aim.");
    Z64Items_SetCanUse(&ball, CanLiftBall);
    Z64Items_SetAction(&ball, TakeOutBall, UpdateBallInHand);
    Z64Items_SetHeldCallbacks(&ball, StartSpinning, PutBallAway, NULL);
    ball.modelGroup = PLAYER_MODELGROUP_DEFAULT;
    ball.getItemEntry.drawFunc = DrawGetItem;

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
