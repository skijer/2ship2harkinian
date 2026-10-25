/**
 * Kite Shield: press R in mid air and the shield goes under Link's feet. Downhill builds speed with no cap, uphill
 * bleeds it; a narrow strip of floor becomes a grind rail he is pinned to. A hops, B spins, R + B gets off, and
 * C items keep working.
 */

#include <math.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_link_child/object_link_child.h"

#define KITE_SHIELD_KEY "nei.equip.kite_shield"
#define DEG_TO_BINANG 182.04f
// z_player.c's own name for the linear stick-to-speed mode of Player_GetMovementSpeedAndYaw.
#define SPEED_MODE_LINEAR 0.0f
// MM gates the action function with state flag 4 where Ship has a named flag.
#define PLAYER_STATE3_PAUSE_ACTION_FUNC PLAYER_STATE3_4

// The shield in hand and on the back is modelled in the shield limb's own space, about 6000 units across.
#define SHIELD_SCALE 44.2f
#define SHIELD_ROT_X (-95.0f * (M_PIf / 180.0f))
#define SHIELD_ROT_Y (-27.0f * (M_PIf / 180.0f))
#define SHIELD_ROT_Z (-99.0f * (M_PIf / 180.0f))
#define SHIELD_OFF_X (-508.0f)
#define SHIELD_OFF_Y (-372.0f)
#define SHIELD_OFF_Z (-5.0f)

// The board hangs off the ROOT limb, measured in human Link's limb space.
#define BOARD_SCALE 46.95f
#define BOARD_ROT_X 51.81f
#define BOARD_ROT_Y 47.84f
#define BOARD_ROT_Z 13.16f
#define BOARD_OFF_X (-126.82f)
#define BOARD_OFF_Y (-885.38f)
#define BOARD_OFF_Z (-225.91f)

// The stance on top of the frozen slope-slide pose, in degrees.
#define CROUCH_DEG (-13.31f)
#define UPPER_ROT_X 9.08f
#define UPPER_ROT_Y 10.15f
#define UPPER_ROT_Z (-11.22f)
#define LOWER_ROT_X (-15.49f)
#define LOWER_ROT_Y 30.44f
#define LOWER_ROT_Z (-15.49f)
#define UPPER_LEAN_DEG 5.0f
// The lower body swings with the carve; the sign is part of the tuning, this limb's space does not map as expected.
#define LOWER_TURN_X 30.0f
#define LOWER_TURN_Y 0.0f
#define LOWER_TURN_Z 0.0f

#define MOUNT_FRAMES 8
#define SLOPE_ACCEL 18.28f
#define FRICTION 0.1f
#define STICK_ACCEL 0.09f
#define TURN_MAX 1310.98f
#define TURN_MIN 250.0f
#define STOP_SPEED 0.6f
#define STOP_FRAMES 10
#define HOP_VELOCITY 9.0f
#define SPIN_FRAMES 24
#define SHUVIT_CHANCE 0.552f
#define SHUVIT_RATE 3000
#define REMOUNT_LOCKOUT 25
#define BONK_YAW 0x2000
#define TURN_FULL 0x0A00
#define TURN_SMOOTH 0.15f
#define LEAN_SCALE 0.75f

// Rail sizes are measured off the Hyrule Field fences: strips 20 to 40 units wide, 40 or more above the ground.
#define RAIL_PROBE 100.27f
#define RAIL_MAX_WIDTH 47.03f
#define RAIL_EDGE_DROP 2.0f
#define RAIL_AHEAD 10.0f
#define RAIL_AHEAD_MAX 90.0f
#define RAIL_PROBE_UP 30.0f
#define RAIL_SAMPLES 8
#define RAIL_RING 16
// Must stay above half of RAIL_MAX_WIDTH or nothing is recognised.
#define RAIL_RING_RADIUS 26.23f
#define RAIL_RING_MAX_HITS 9
// Entering stays fussy so a strip crossing his path does not yank him onto it; following allows real corners.
#define RAIL_MAX_APPROACH 0x4000
#define RAIL_MAX_TURN 0x5800
#define RAIL_GAP_STEPS 3
#define RAIL_ATTRACT 90.0f
#define RAIL_ATTRACT_DIRS 8
#define RAIL_ATTRACT_RISE 30.0f
#define RAIL_ATTRACT_PULL 6.0f
#define RAIL_SPEED 8.27f
#define RAIL_SNAP 60.0f
#define RAIL_GRACE 4
#define RAIL_TURN 12000.0f
#define RAIL_DETACH_FRAMES 30

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconKiteShieldTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gKiteShieldNameTex";
static const ALIGN_ASSET(2) char sShieldDL[] = "__OTR__objects/object_nei_kite_shield/g_kite_shield_dl";
static const ALIGN_ASSET(2) char sPoseAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_normal_down_slope_slip";

// Imported by address: vanilla compares actionFunc against the real function, never against a mod's thunk.
extern HOST_DATA void Player_Action_Idle(Player* player, PlayState* play);
extern PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
s32 Player_SetAction(PlayState* play, Player* player, PlayerActionFunc actionFunc, s32 arg3);
void func_8082DC38(Player* player);
void func_80833864(PlayState* play, Player* player, s32 meleeWeaponAnim);
bool Player_CanUpdateItems(Player* player);
void Player_UpdateItems(Player* player, PlayState* play);
s32 Player_GetMovementSpeedAndYaw(Player* player, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                  PlayState* play);
s32 Player_ActionHandler_1(Player* player, PlayState* play);
s32 Player_ActionHandler_2(Player* player, PlayState* play);
s32 Player_ActionHandler_12(Player* player, PlayState* play);
s32 Player_ActionHandler_Talk(Player* player, PlayState* play);
void Player_Anim_PlayOnceMorph(PlayState* play, Player* player, PlayerAnimationHeader* anim);
u16 Player_GetFloorSfx(Player* player, u16 sfxId);

typedef enum {
    SURF_OFF,
    SURF_MOUNT,
    SURF_RIDE,
    SURF_RAIL,
    SURF_DISMOUNT,
} SurfState;

static const S2HModApi* sApi;

static struct {
    SurfState state;
    s16 timer;
    s16 stopFrames;
    s16 spinFrames;
    s16 leanPitch;
    s16 leanRoll;
    s16 upperLean;
    f32 turn;
    s16 railAxisYaw;
    s16 railMiss;
    s16 railDetach;
    s16 boardSpin;
    s16 boardSpinRate;
    s16 remountLockout;
    PlayerActionFunc ownedAction;
    Vec3s poseBackup[3];
    bool isPoseApplied;
    s8 hiddenVanillaShield;
} sSurf;

static bool IsWorn(void) {
    return sApi->IsEquipmentWorn(KITE_SHIELD_KEY);
}

static bool IsRiding(void) {
    return sSurf.state == SURF_RIDE || sSurf.state == SURF_RAIL;
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static s16 Degrees(f32 degrees) {
    return (s16)(degrees * DEG_TO_BINANG);
}

// The swing is cancelled and Link is parked on Player_Action_Idle, a known action for the takeover check. Never
// meleeWeaponAnimation = -1: the melee action indexes its table with it and reads garbage.
static void KillMeleeState(Player* player, PlayState* play) {
    func_8082DC38(player);
    player->stateFlags1 &= ~PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    player->unk_B08 = 0.0f;
    Collider_ResetQuadAT(play, &player->meleeWeaponQuads[0].base);
    Collider_ResetQuadAT(play, &player->meleeWeaponQuads[1].base);
    // Flag 1 keeps the shield up: he is standing on one.
    Player_SetAction(play, player, Player_Action_Idle, 1);
}

// The frozen first frame of the vanilla downhill slide: bent knees, low weight.
static void HoldPose(Player* player, PlayState* play) {
    PlayerAnimationHeader* pose = ResourceMgr_LoadPlayerAnimAsHeader(sPoseAnim);

    if (pose != NULL && player->skelAnime.animation != pose) {
        PlayerAnimation_Change(play, &player->skelAnime, pose, 0.0f, 0.0f, 0.0f, ANIMMODE_ONCE, -4.0f);
    }
    player->skelAnime.playSpeed = 0.0f;
}

static void ClearRide(void) {
    sSurf.state = SURF_OFF;
    sSurf.timer = 0;
    sSurf.stopFrames = 0;
    sSurf.spinFrames = 0;
    sSurf.railMiss = 0;
    sSurf.railDetach = 0;
    sSurf.boardSpin = 0;
    sSurf.boardSpinRate = 0;
    sSurf.leanPitch = 0;
    sSurf.leanRoll = 0;
    sSurf.upperLean = 0;
    sSurf.turn = 0.0f;
    sSurf.ownedAction = NULL;
}

static void Abort(Player* player) {
    if (sSurf.state == SURF_OFF) {
        return;
    }
    ClearRide();
    sSurf.remountLockout = REMOUNT_LOCKOUT;
    if (player != NULL) {
        player->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
        player->skelAnime.playSpeed = 1.0f;
        player->actor.shape.rot.x = 0;
    }
}

// Entry is always in mid air, so the pause is taken this very frame or the fall action steals him.
static void StartSurf(Player* player, PlayState* play) {
    PlayerAnimationHeader* pose = ResourceMgr_LoadPlayerAnimAsHeader(sPoseAnim);

    KillMeleeState(player, play);
    player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    ClearRide();
    sSurf.state = SURF_MOUNT;
    sSurf.ownedAction = player->actionFunc;
    if (pose != NULL) {
        Player_Anim_PlayOnceMorph(play, player, pose);
    }
    Player_PlaySfx(player, NA_SE_IT_SHIELD_SWING);
}

// Only MM's named state flags: the rest of that enum is still numeric, and the cutscene and water cases are covered
// by the idioms the engine tests itself. The board is measured on human Link's skeleton.
static bool IsAllowed(Player* player) {
    return player->transformation == PLAYER_FORM_HUMAN &&
           !(player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_CARRYING_ACTOR)) &&
           player->csAction == PLAYER_CSACTION_NONE && !(player->actor.bgCheckFlags & BGCHECKFLAG_WATER);
}

// A rail is a narrow, long piece of floor. Every probe is a floor raycast looking for where the ground ends.
static f32 FloorAt(PlayState* play, Player* player, f32 x, f32 z, CollisionPoly** outPoly) {
    Vec3f probe = { x, player->actor.world.pos.y + RAIL_PROBE_UP, z };
    s32 bgId = BGCHECK_SCENE;

    *outPoly = NULL;
    return BgCheck_EntityRaycastFloor3(&play->colCtx, outPoly, &bgId, &probe);
}

// Measured against the plane under his feet, not a flat height: a ramped rail reads as zero deviation. Ground
// well above the plane is an edge too, so a sunken channel or a path hugging a wall counts.
static bool IsOnStrip(PlayState* play, Player* player, f32 x, f32 z, f32 drop) {
    CollisionPoly* ignored;
    f32 y = FloorAt(play, player, x, z, &ignored);
    Vec3f point = { x, y, z };

    if (y <= BGCHECK_Y_MIN) {
        return false;
    }
    if (player->actor.floorPoly == NULL) {
        return true;
    }
    return fabsf(CollisionPoly_GetPointDistanceFromPlane(player->actor.floorPoly, &point)) <= drop;
}

static bool IsOnStripAt(PlayState* play, Player* player, s16 yaw, f32 dist, f32 drop) {
    return IsOnStrip(play, player, player->actor.world.pos.x + dist * Math_SinS(yaw),
                     player->actor.world.pos.z + dist * Math_CosS(yaw), drop);
}

static bool IsOffPlane(PlayState* play, Player* player, CollisionPoly* poly, f32 x, f32 z, f32 drop) {
    CollisionPoly* ignored;
    f32 y = FloorAt(play, player, x, z, &ignored);
    Vec3f point = { x, y, z };

    if (y <= BGCHECK_Y_MIN) {
        return true;
    }
    return fabsf(CollisionPoly_GetPointDistanceFromPlane(poly, &point)) > drop;
}

// The magnet: a rail one step to the side is invisible to the detection, so this drags him across onto it.
// Horizontal only, and only for rails at his own height; hauling him up a fence side would launch him through it.
static bool AttractToRail(Player* player, PlayState* play) {
    s32 stepAngle = 0x10000 / RAIL_ATTRACT_DIRS;
    s16 heading = player->actor.shape.rot.y;
    s32 bestDiff = 0x7FFFFFFF;
    f32 bestX = 0.0f;
    f32 bestZ = 0.0f;
    bool isFound = false;

    for (s32 k = 0; k < RAIL_ATTRACT_DIRS; k++) {
        s16 dir = (s16)(k * stepAngle);
        s16 across = dir + 0x4000;
        f32 cx = player->actor.world.pos.x + RAIL_ATTRACT * Math_SinS(dir);
        f32 cz = player->actor.world.pos.z + RAIL_ATTRACT * Math_CosS(dir);
        CollisionPoly* poly;
        f32 cy = FloorAt(play, player, cx, cz, &poly);
        s32 diff;

        if (cy <= BGCHECK_Y_MIN || poly == NULL || fabsf(cy - player->actor.floorHeight) > RAIL_ATTRACT_RISE) {
            continue;
        }
        if (!IsOffPlane(play, player, poly, cx + RAIL_RING_RADIUS * Math_SinS(across),
                        cz + RAIL_RING_RADIUS * Math_CosS(across), RAIL_EDGE_DROP) ||
            !IsOffPlane(play, player, poly, cx - RAIL_RING_RADIUS * Math_SinS(across),
                        cz - RAIL_RING_RADIUS * Math_CosS(across), RAIL_EDGE_DROP)) {
            continue;
        }
        diff = ABS((s16)(dir - heading));
        if (diff < bestDiff) {
            bestDiff = diff;
            bestX = cx;
            bestZ = cz;
            isFound = true;
        }
    }
    if (!isFound) {
        return false;
    }
    Math_StepToF(&player->actor.world.pos.x, bestX, RAIL_ATTRACT_PULL);
    Math_StepToF(&player->actor.world.pos.z, bestZ, RAIL_ATTRACT_PULL);
    return true;
}

// How far out to one side the ground lasts before it ends; 0 when it never does, which is not a strip.
static f32 GetEdgeDistance(PlayState* play, Player* player, s16 yaw, bool isRight) {
    f32 step = RAIL_PROBE / RAIL_SAMPLES;
    s16 side = yaw + (isRight ? 0x4000 : -0x4000);

    for (s32 i = 1; i <= RAIL_SAMPLES; i++) {
        if (!IsOnStripAt(play, player, side, step * i, RAIL_EDGE_DROP)) {
            return step * i;
        }
    }
    return 0.0f;
}

// A ring of floor probes finds the strip's own direction without using his heading, which is what lets a rail be
// grabbed from any angle: on a strip the landing probes form two opposite arcs. The arc nearest his heading wins.
// A second pass at half radius catches fences too thin for the first.
static bool FindRailAxis(PlayState* play, Player* player, s16* outYaw) {
    bool hit[RAIL_RING];
    f32 radius = RAIL_RING_RADIUS;
    s32 maxApproach = (sSurf.state == SURF_RAIL) ? RAIL_MAX_TURN : RAIL_MAX_APPROACH;
    s32 stepAngle = 0x10000 / RAIL_RING;
    s16 heading = player->actor.shape.rot.y;
    s32 hits = 0;
    s32 origin = -1;
    s32 bestDiff = 0x7FFFFFFF;
    bool isFound = false;
    s32 k;

    for (s32 pass = 0; pass < 2; pass++) {
        hits = 0;
        for (k = 0; k < RAIL_RING; k++) {
            hit[k] = IsOnStripAt(play, player, (s16)(k * stepAngle), radius, RAIL_EDGE_DROP);
            hits += hit[k];
        }
        if (hits != 0) {
            break;
        }
        radius *= 0.5f;
    }
    if (hits == 0 || hits > RAIL_RING_MAX_HITS) {
        return false;
    }
    // Walking from the start of an arc sees an arc that straddles index 0 whole.
    for (k = 0; k < RAIL_RING; k++) {
        if (hit[k] && !hit[(k + RAIL_RING - 1) % RAIL_RING]) {
            origin = k;
            break;
        }
    }
    if (origin < 0) {
        return false;
    }
    k = 0;
    while (k < RAIL_RING) {
        s32 length = 0;
        s32 angle;
        s32 diff;

        if (!hit[(origin + k) % RAIL_RING]) {
            k++;
            continue;
        }
        while (length < RAIL_RING && hit[(origin + k + length) % RAIL_RING]) {
            length++;
        }
        angle = (s32)((origin + k + (length - 1.0f) * 0.5f) * stepAngle);
        diff = ABS((s16)((s16)angle - heading));
        if (diff < bestDiff) {
            bestDiff = diff;
            *outYaw = (s16)angle;
            isFound = true;
        }
        k += length;
    }
    return isFound && bestDiff <= maxApproach;
}

static bool DoesStripContinue(PlayState* play, Player* player, s16 axis) {
    f32 ahead = CLAMP(player->speedXZ * 2.0f, RAIL_AHEAD, RAIL_AHEAD_MAX);

    // Several distances, so a seam or a post between fence segments does not end the run.
    for (s32 i = 1; i <= RAIL_GAP_STEPS; i++) {
        if (IsOnStripAt(play, player, axis, ahead * i / RAIL_GAP_STEPS, RAIL_EDGE_DROP)) {
            return true;
        }
    }
    return false;
}

// The whole lateral error is taken out every frame: a soft pull could always be outrun on a bend.
static bool UpdateRail(Player* player, PlayState* play) {
    s16 axis;
    f32 left;
    f32 right;
    f32 correction;
    s16 side;

    if (!FindRailAxis(play, player, &axis)) {
        return false;
    }
    // Published before the gates: on a corner the width test fails for a frame or two, and the grace window must
    // coast on the new arm, not on the one he came in along.
    if (sSurf.state == SURF_RAIL) {
        sSurf.railAxisYaw = axis;
    }
    left = GetEdgeDistance(play, player, axis, false);
    right = GetEdgeDistance(play, player, axis, true);
    if (left == 0.0f || right == 0.0f || left + right > RAIL_MAX_WIDTH || !DoesStripContinue(play, player, axis)) {
        return false;
    }
    sSurf.railAxisYaw = axis;
    correction = CLAMP((right - left) * 0.5f, -RAIL_SNAP, RAIL_SNAP);
    side = axis + 0x4000;
    player->actor.world.pos.x += correction * Math_SinS(side);
    player->actor.world.pos.z += correction * Math_CosS(side);
    return true;
}

// The player moves himself inside Player_UpdateCommon, before this runs; instead of moving him twice the speed is
// clamped so a frame can never start on the far side of a wall.
static void ClampSpeedToWall(Player* player, PlayState* play) {
    f32 reach = player->speedXZ + 10.0f;
    Vec3f from = { player->actor.world.pos.x, player->actor.world.pos.y + 20.0f, player->actor.world.pos.z };
    Vec3f to = { from.x + Math_SinS(player->actor.shape.rot.y) * reach, from.y,
                 from.z + Math_CosS(player->actor.shape.rot.y) * reach };
    Vec3f hitPos;
    CollisionPoly* poly = NULL;
    s32 bgId = BGCHECK_SCENE;
    s16 wallYaw;

    if (player->speedXZ < 10.0f) {
        return;
    }
    if (!BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hitPos, &poly, true, false, false, true, &bgId) ||
        poly == NULL) {
        return;
    }
    // Only a wall he drives into: a curved rail's own outer side is not an obstacle.
    wallYaw = Math_Atan2S_XY(COLPOLY_GET_NORMAL(poly->normal.z), COLPOLY_GET_NORMAL(poly->normal.x));
    if (Math_CosS(wallYaw - player->actor.shape.rot.y) > -0.5f) {
        return;
    }
    player->speedXZ =
        MIN(player->speedXZ, MAX(sqrtf(SQ(hitPos.x - from.x) + SQ(hitPos.z - from.z)) - 10.0f, 0.0f));
}

static bool HitsWallHeadOn(Player* player) {
    s16 yawDiff = player->yaw - (s16)(player->actor.wallYaw + 0x8000);

    return (player->actor.bgCheckFlags & BGCHECKFLAG_PLAYER_WALL_INTERACT) && ABS(yawDiff) < BONK_YAW &&
           player->speedXZ > 4.0f;
}

static void Dismount(void) {
    sSurf.state = SURF_DISMOUNT;
    sSurf.timer = 0;
}

// MM reaches Player_UpdateItems only from inside the paused action, and its wrapper keeps the gates that keep
// the Pictograph Box from firing mid-fall, so the predicate is asked first.
static void RunItemButtons(Player* player, PlayState* play) {
    PlayerActionFunc before = player->actionFunc;

    if (!Player_CanUpdateItems(player)) {
        return;
    }
    Player_UpdateItems(player, play);
    // Whatever the items started has to play out: re-pausing next frame would freeze it half done and start it again.
    if (player->actionFunc != before) {
        sSurf.spinFrames = SPIN_FRAMES;
        sSurf.ownedAction = player->actionFunc;
    }
}

// B alone spins, or draws the sword if none is out; R + B gets off.
static void HandleB(Player* player, PlayState* play, Input* input, bool isGrounded) {
    if (CHECK_BTN_ALL(input->cur.button, BTN_R)) {
        Dismount();
        return;
    }
    if (Player_GetMeleeWeaponHeld(player) == PLAYER_MELEEWEAPON_NONE) {
        RunItemButtons(player, play);
        return;
    }
    // MM's ground slash refuses to start in mid air, so an airborne press would re-arm every frame.
    if (sSurf.spinFrames != 0 || !isGrounded) {
        return;
    }
    func_80833864(play, player,
                  Player_IsHoldingTwoHandedWeapon(player) ? PLAYER_MWA_SPIN_ATTACK_2H : PLAYER_MWA_SPIN_ATTACK_1H);
    sSurf.spinFrames = SPIN_FRAMES;
    // The swing changes the action; owned again so the takeover check does not read it as a steal.
    sSurf.ownedAction = player->actionFunc;
}

// A hops; on a rail it also lets go, and the detach window stops the next frame grabbing it straight back.
static bool HandleHop(Player* player, Input* input, bool isGrounded) {
    if (!CHECK_BTN_ALL(input->press.button, BTN_A) || (!isGrounded && sSurf.state != SURF_RAIL)) {
        return isGrounded;
    }
    if (sSurf.state == SURF_RAIL) {
        sSurf.state = SURF_RIDE;
        sSurf.railDetach = RAIL_DETACH_FRAMES;
    }
    player->actor.velocity.y = HOP_VELOCITY;
    player->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    Player_PlaySfx(player, NA_SE_PL_SKIP);
    if (sSurf.boardSpinRate == 0 && Rand_ZeroOne() < SHUVIT_CHANCE) {
        sSurf.boardSpinRate = (Rand_ZeroOne() < 0.5f) ? SHUVIT_RATE : -SHUVIT_RATE;
    }
    return false;
}

// The shuvit is cosmetic, so the board always lands square under him.
static void SpinBoard(bool isGrounded) {
    if (isGrounded) {
        sSurf.boardSpinRate = 0;
    }
    if (sSurf.boardSpinRate != 0) {
        sSurf.boardSpin += sSurf.boardSpinRate;
    } else if (sSurf.boardSpin != 0) {
        Math_ScaledStepToS(&sSurf.boardSpin, 0, 4000);
    }
}

static void UpdateRailState(Player* player, PlayState* play, bool isGrounded) {
    if (sSurf.railDetach > 0) {
        sSurf.railDetach--;
    }
    if (isGrounded && sSurf.railDetach == 0 && UpdateRail(player, play)) {
        sSurf.state = SURF_RAIL;
        sSurf.railMiss = 0;
    } else if (isGrounded && sSurf.railDetach == 0 && sSurf.state != SURF_RAIL && AttractToRail(player, play)) {
        // Dragged across; once over the strip the branch above pins him.
    } else if (sSurf.state == SURF_RAIL && ++sSurf.railMiss >= RAIL_GRACE) {
        sSurf.state = SURF_RIDE;
    }
}

// The turn rate grows with the error, so a gentle bend stays gentle and a corner is taken in about a frame.
static void FollowRail(Player* player) {
    s16 delta = sSurf.railAxisYaw - player->actor.shape.rot.y;

    Math_SmoothStepToS(&player->actor.shape.rot.y, sSurf.railAxisYaw, 2, (s16)(RAIL_TURN + (ABS(delta) >> 1)), 100);
    player->speedXZ = RAIL_SPEED;
}

static void SteerFreely(Player* player, s16 yawTarget) {
    f32 turnMax = TURN_MAX;

    if (player->speedXZ > 1.0f) {
        turnMax /= player->speedXZ;
    }
    Math_SmoothStepToS(&player->actor.shape.rot.y, yawTarget, 6, (s16)MAX(turnMax, TURN_MIN), 100);
}

// Every incline feeds speed, not only the floors vanilla slides on.
static void ApplySlope(Player* player, bool isGrounded, bool hasStick, f32 speedTarget) {
    Vec3f slopeNormal;
    s16 downhillYaw;

    if (!isGrounded || player->actor.floorPoly == NULL) {
        Math_SmoothStepToS(&sSurf.leanPitch, 0, 3, 0x300, 0x40);
        player->actor.shape.rot.x = sSurf.leanPitch;
        return;
    }
    Actor_GetSlopeDirection(player->actor.floorPoly, &slopeNormal, &downhillYaw);
    if (sSurf.state != SURF_RAIL) {
        player->speedXZ += SLOPE_ACCEL * (1.0f - slopeNormal.y) * Math_CosS(downhillYaw - player->actor.shape.rot.y);
        Math_StepToF(&player->speedXZ, 0.0f, FRICTION);
        if (hasStick) {
            player->speedXZ += STICK_ACCEL * (speedTarget / 9.0f);
        }
    }
    Math_SmoothStepToS(&sSurf.leanPitch, (s16)(player->floorPitch * LEAN_SCALE), 3, 0x300, 0x40);
    player->actor.shape.rot.x = sSurf.leanPitch;
}

// yaw still holds last frame's heading here, so the difference is exactly how hard he is turning now.
static void LeanIntoTurn(Player* player) {
    s16 want = (s16)(-(s16)(player->actor.shape.rot.y - player->yaw) * 2);
    s16 upperMax = Degrees(UPPER_LEAN_DEG);

    Math_SmoothStepToS(&sSurf.leanRoll, CLAMP(want, -0x0A00, 0x0A00), 4, 0x200, 0x20);
    Math_SmoothStepToS(&sSurf.upperLean, CLAMP(want, -upperMax, upperMax), 4, 0x200, 0x20);
    sSurf.turn += ((f32)CLAMP(want, -TURN_FULL, TURN_FULL) / TURN_FULL - sSurf.turn) * TURN_SMOOTH;
}

static void CheckStop(Player* player, bool isGrounded) {
    if (HitsWallHeadOn(player)) {
        Dismount();
        return;
    }
    if (!isGrounded || player->speedXZ >= STOP_SPEED) {
        sSurf.stopFrames = 0;
        return;
    }
    if (++sSurf.stopFrames >= STOP_FRAMES) {
        Dismount();
    }
}

static void Ride(Player* player, PlayState* play) {
    Input* input = &play->state.input[0];
    bool isGrounded = IsGrounded(player);
    bool isBPressed = CHECK_BTN_ALL(input->press.button, BTN_B);
    f32 speedTarget = 0.0f;
    s16 yawTarget = player->actor.shape.rot.y;
    bool hasStick = Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, SPEED_MODE_LINEAR, play);

    if (isBPressed) {
        HandleB(player, play, input, isGrounded);
        if (sSurf.state == SURF_DISMOUNT) {
            return;
        }
    }
    isGrounded = HandleHop(player, input, isGrounded);
    SpinBoard(isGrounded);
    // The pause would swallow the C buttons; during a spin the vanilla action already reads them.
    if (!isBPressed && sSurf.spinFrames == 0) {
        RunItemButtons(player, play);
    }
    UpdateRailState(player, play, isGrounded);
    if (sSurf.state == SURF_RAIL) {
        FollowRail(player);
    } else if (hasStick) {
        SteerFreely(player, yawTarget);
    }
    ApplySlope(player, isGrounded, hasStick, speedTarget);
    LeanIntoTurn(player);
    player->speedXZ = MAX(player->speedXZ, 0.0f);
    ClampSpeedToWall(player, play);
    player->actor.speed = player->speedXZ;
    player->yaw = player->actor.shape.rot.y;
    if (isGrounded && player->speedXZ > 1.0f) {
        Audio_PlaySfx_AtPosWithSyncedFreqAndVolume(&player->actor.projectedPos,
                                                   Player_GetFloorSfx(player, NA_SE_PL_SLIP_LEVEL - SFX_FLAG),
                                                   player->actor.speed);
    }
    CheckStop(player, isGrounded);
}

// The spin owns the action and the animation: the pause stays off and he keeps his speed through it. The window
// is a ceiling; the swing ending early hands control back at once.
static void RideThroughSpin(Player* player, PlayState* play) {
    sSurf.spinFrames--;
    if (sSurf.spinFrames == 0 ||
        (sSurf.spinFrames < SPIN_FRAMES - 4 && player->meleeWeaponState == PLAYER_MELEE_WEAPON_STATE_0)) {
        sSurf.spinFrames = 0;
        sSurf.ownedAction = player->actionFunc;
    }
    Ride(player, play);
}

static void TryMount(Player* player, PlayState* play) {
    // A dismount over a pit must not re-arm halfway down the fall.
    if (sSurf.remountLockout > 0) {
        if (IsGrounded(player)) {
            sSurf.remountLockout--;
        }
        return;
    }
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_R) && !IsGrounded(player) &&
        !Player_InBlockingCsMode(play, player)) {
        StartSurf(player, play);
    }
}

// Damage or a cutscene swaps the action straight through the pause; a door, an NPC, a grab or a ledge must be able
// to complete, or the surf is a softlock.
static bool LostControl(Player* player, PlayState* play) {
    if (sSurf.state != SURF_MOUNT && sSurf.spinFrames == 0 && sSurf.ownedAction != NULL &&
        player->actionFunc != sSurf.ownedAction) {
        return true;
    }
    return Player_ActionHandler_1(player, play) || Player_ActionHandler_Talk(player, play) ||
           Player_ActionHandler_2(player, play) || Player_ActionHandler_12(player, play);
}

// Runs after the player's own update, where the speed and animation written here are the last word.
static void TickSurf(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || player == NULL || player != GET_PLAYER(play)) {
        return;
    }
    if (!IsWorn()) {
        Abort(player);
        return;
    }
    if (!IsAllowed(player)) {
        Abort(player);
        return;
    }
    if (sSurf.state == SURF_OFF) {
        TryMount(player, play);
        return;
    }
    if (LostControl(player, play)) {
        Abort(player);
        return;
    }
    switch (sSurf.state) {
        case SURF_MOUNT:
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            PlayerAnimation_Update(play, &player->skelAnime);
            if (++sSurf.timer >= MOUNT_FRAMES) {
                sSurf.state = SURF_RIDE;
                sSurf.timer = 0;
                sSurf.ownedAction = player->actionFunc;
            }
            break;
        case SURF_RIDE:
        case SURF_RAIL:
            if (sSurf.spinFrames > 0) {
                RideThroughSpin(player, play);
                break;
            }
            player->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
            HoldPose(player, play);
            PlayerAnimation_Update(play, &player->skelAnime);
            Ride(player, play);
            break;
        case SURF_DISMOUNT:
            KillMeleeState(player, play);
            Player_PlaySfx(player, NA_SE_IT_SHIELD_SWING);
            Abort(player);
            break;
        default:
            Abort(player);
            break;
    }
}

// The joint table is final by the time the player draws; the stance is added there and taken back after, so the
// next animation load never inherits it.
static void AddStance(Vec3s* joints) {
    joints[PLAYER_LIMB_WAIST].x += Degrees(CROUCH_DEG) + sSurf.leanPitch / 2;
    joints[PLAYER_LIMB_WAIST].z += sSurf.leanRoll;
    joints[PLAYER_LIMB_LOWER_ROOT].x += Degrees(LOWER_ROT_X + LOWER_TURN_X * sSurf.turn);
    joints[PLAYER_LIMB_LOWER_ROOT].y += Degrees(LOWER_ROT_Y + LOWER_TURN_Y * sSurf.turn);
    joints[PLAYER_LIMB_LOWER_ROOT].z += Degrees(LOWER_ROT_Z + LOWER_TURN_Z * sSurf.turn);
    joints[PLAYER_LIMB_UPPER_ROOT].x += Degrees(UPPER_ROT_X);
    joints[PLAYER_LIMB_UPPER_ROOT].y += Degrees(UPPER_ROT_Y);
    joints[PLAYER_LIMB_UPPER_ROOT].z += Degrees(UPPER_ROT_Z) + sSurf.upperLean;
}

static void PoseBeforeDraw(Actor* actor) {
    Vec3s* joints = ((Player*)actor)->skelAnime.jointTable;

    if (!IsRiding() || sSurf.spinFrames != 0) {
        return;
    }
    sSurf.poseBackup[0] = joints[PLAYER_LIMB_WAIST];
    sSurf.poseBackup[1] = joints[PLAYER_LIMB_LOWER_ROOT];
    sSurf.poseBackup[2] = joints[PLAYER_LIMB_UPPER_ROOT];
    AddStance(joints);
    sSurf.isPoseApplied = true;
}

static void RestorePoseAfterDraw(Actor* actor, PlayState* play) {
    Vec3s* joints = ((Player*)actor)->skelAnime.jointTable;

    if (!sSurf.isPoseApplied) {
        return;
    }
    sSurf.isPoseApplied = false;
    joints[PLAYER_LIMB_WAIST] = sSurf.poseBackup[0];
    joints[PLAYER_LIMB_LOWER_ROOT] = sSurf.poseBackup[1];
    joints[PLAYER_LIMB_UPPER_ROOT] = sSurf.poseBackup[2];
}

static bool IsShieldOnBack(Player* player) {
    return player->sheathType == PLAYER_MODELTYPE_SHEATH_14 || player->sheathType == PLAYER_MODELTYPE_SHEATH_15;
}

static bool IsShieldInHand(Player* player) {
    return player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD;
}

// The hand model carries the shield, so it is swapped for the bare hand. The back shield is drawn by the sheath
// limb's post draw from currentShield, so that is cleared until DrawOnLimb restores it.
static void HideVanillaShield(Player* player, int32_t limbIndex, Gfx** dList, Gfx* limbDList, Vec3f* pos) {
    if (!IsWorn() || player->transformation != PLAYER_FORM_HUMAN || player->currentShield == PLAYER_SHIELD_NONE) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_RIGHT_HAND && IsShieldInHand(player)) {
        *dList = (Gfx*)gLinkHumanRightHandOpenDL;
    } else if (limbIndex == PLAYER_LIMB_SHEATH && IsShieldOnBack(player)) {
        sSurf.hiddenVanillaShield = player->currentShield;
        player->currentShield = PLAYER_SHIELD_NONE;
    }
}

static void RestoreVanillaShield(Player* player) {
    if (sSurf.hiddenVanillaShield != PLAYER_SHIELD_NONE) {
        player->currentShield = sSurf.hiddenVanillaShield;
        sSurf.hiddenVanillaShield = PLAYER_SHIELD_NONE;
    }
}

// XLU on purpose: the model leaves its combiner on the pipe, and on OPA that blacks out every limb after it.
static void DrawShieldModel(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(SHIELD_OFF_X, SHIELD_OFF_Y, SHIELD_OFF_Z, MTXMODE_APPLY);
    Matrix_RotateXF(SHIELD_ROT_X, MTXMODE_APPLY);
    Matrix_RotateYF(SHIELD_ROT_Y, MTXMODE_APPLY);
    Matrix_RotateZF(SHIELD_ROT_Z, MTXMODE_APPLY);
    Matrix_Scale(SHIELD_SCALE, SHIELD_SCALE, SHIELD_SCALE, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sShieldDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// The shuvit turns after the translate and before the placement, so the board spins about its own centre.
static void DrawBoard(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(BOARD_OFF_X, BOARD_OFF_Y, BOARD_OFF_Z, MTXMODE_APPLY);
    if (sSurf.boardSpin != 0) {
        Matrix_RotateYF(sSurf.boardSpin * (M_PIf / 32768.0f), MTXMODE_APPLY);
    }
    Matrix_RotateZYX(Degrees(BOARD_ROT_X), Degrees(BOARD_ROT_Y), Degrees(BOARD_ROT_Z), MTXMODE_APPLY);
    Matrix_Scale(BOARD_SCALE, BOARD_SCALE, BOARD_SCALE, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sShieldDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// While riding the board is under his feet, so neither the hand nor the back copy draws. The vanilla base is the
// Hero's Shield, which collides as metal; this one is wood.
static void DrawOnLimb(Player* player, int32_t limbIndex) {
    PlayState* play = gPlayState;

    if (limbIndex == PLAYER_LIMB_SHEATH) {
        RestoreVanillaShield(player);
    }
    if (play == NULL || !IsWorn() || player != GET_PLAYER(play) || player->transformation != PLAYER_FORM_HUMAN) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_ROOT && IsRiding()) {
        DrawBoard(play);
        return;
    }
    if (IsRiding() || player->currentShield == PLAYER_SHIELD_NONE) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_RIGHT_HAND && IsShieldInHand(player)) {
        player->shieldQuad.base.colMaterial = COL_MATERIAL_WOOD;
        DrawShieldModel(play);
    } else if (limbIndex == PLAYER_LIMB_SHEATH && IsShieldOnBack(player) && !IsShieldInHand(player) &&
               player->rightHandType != PLAYER_MODELTYPE_RH_FF) {
        DrawShieldModel(play);
    }
}

static void CancelFallDamage(bool* should, va_list args) {
    if (sSurf.state != SURF_OFF) {
        *should = false;
    }
}

static void AbortOnSceneChange(s16 sceneId, s8 spawnNum) {
    Abort(NULL);
    sSurf.remountLockout = 0;
}

static void HandBackPlayer(const char* key) {
    Abort(gPlayState != NULL ? GET_PLAYER(gPlayState) : NULL);
    sSurf.remountLockout = 0;
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate",       "OnPlayerResolveLimbDraw", "OnPlayerPostLimbDraw",
                                              "OnActorDraw",          "OnActorDrawEnd",          "OnSceneInit" };

static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    S2HCustomEquipDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = KITE_SHIELD_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rKite Shield\n%wPress %yR%w in mid air to ride it. %yA%w hops, %yB%w spins, "
                           "%yR%w + %yB%w gets off.";
    definition.getItemText = "You got the %rKite Shield%w!\nLight enough to ride. Press %yR%w in mid air and it goes "
                             "under your feet: slopes speed you up, and narrow ledges become rails.";
    definition.slot = S2H_EQUIP_SLOT_SHIELD;
    definition.page = 0;
    definition.row = S2H_EQUIP_SLOT_SHIELD;
    definition.column = 2;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.vanillaBase = PLAYER_SHIELD_HEROS_SHIELD;
    definition.toggles = 1;
    definition.onUnequip = HandBackPlayer;
    definition.getItemModelPath = sShieldDL;
    if (!sApi->RegisterCustomEquipment(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickSurf);
    S2H_REGISTER_HOOK(sApi, OnPlayerResolveLimbDraw, HideVanillaShield);
    S2H_REGISTER_HOOK(sApi, OnPlayerPostLimbDraw, DrawOnLimb);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, AbortOnSceneChange);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDraw, ACTOR_PLAYER, PoseBeforeDraw);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorDrawEnd, ACTOR_PLAYER, RestorePoseAfterDraw);
    sApi->RegisterVB(VB_RECIEVE_FALL_DAMAGE, CancelFallDamage);
}
