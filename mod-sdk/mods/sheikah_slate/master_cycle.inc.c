// Master Cycle Zero: a real En_Horse with its update and draw replaced, so the player's whole riding stack works on it.
// MM picks the horse type from the high params bits and its Skin may be left uninitialised, so the Skin is never used.
#include "overlays/actors/ovl_En_Horse/z_en_horse.h"
#include "objects/object_horse_link_child/object_horse_link_child.h"
#include "z64horse.h"

void EnHorse_Update(Actor* thisx, PlayState* play2);
void EnHorse_Draw(Actor* thisx, PlayState* play);

typedef struct {
    u32 dmgFlags;
    u8 damage;
    u8 effect;
    f32 radius;
    f32 height;
} CombatColliderConfig;

static void Combat_InitCylinder(PlayState* play, ColliderCylinder* col, Actor* owner, CombatColliderConfig* cfg) {
    ColliderCylinderInit init = {
        { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
        { ELEM_MATERIAL_UNK2,
          { cfg->dmgFlags, cfg->effect, cfg->damage },
          { 0, 0, 0 },
          ATELEM_ON | ATELEM_SFX_NORMAL,
          ACELEM_NONE,
          OCELEM_NONE },
        { (s16)cfg->radius, (s16)cfg->height, 0, { 0, 0, 0 } },
    };
    Collider_InitCylinder(play, col);
    Collider_SetCylinder(play, col, owner, &init);
}

static void Combat_UpdateCylinder(ColliderCylinder* col, Vec3f* pos, CombatColliderConfig* cfg) {
    col->dim.pos.x = (s16)pos->x;
    col->dim.pos.y = (s16)pos->y;
    col->dim.pos.z = (s16)pos->z;
    col->dim.radius = (s16)cfg->radius;
    col->dim.height = (s16)cfg->height;
    col->elem.atDmgInfo.dmgFlags = cfg->dmgFlags;
    col->elem.atDmgInfo.damage = cfg->damage;
    col->elem.atDmgInfo.effect = cfg->effect;
    col->base.atFlags |= AT_ON | AT_TYPE_PLAYER;
}

#define MC_RIDING_FLAG PLAYER_STATE1_800000
#define MC_CUTSCENE_FLAG PLAYER_STATE1_20000000

#define MC_DL_BODY "__OTR__objects/object_master_cycle/gMasterCycleBodyDL"
#define MC_DL_LIGHTS "__OTR__objects/object_master_cycle/gMasterCycleLigthsDL"
#define MC_DL_WHEEL_F "__OTR__objects/object_master_cycle/gMasterCycleFrontWhellDL"
#define MC_DL_WHEEL_B "__OTR__objects/object_master_cycle/gMasterCycleBackWheelDL"
#define MC_HUB_F_Z 35.0f
#define MC_HUB_B_Z -39.0f
#define MC_HUB_Y 15.0f
#define MC_WHEEL_RADIUS 15.0f
#define MC_WHEELBASE (MC_HUB_F_Z - MC_HUB_B_Z)
#define MC_BODY_HALF_WIDTH 13.0f

#define MC_HULL_RADIUS 15.0f
#define MC_HULL_NOSE_Z 42.0f
#define MC_HULL_TAIL_Z -40.0f
#define MC_BODY_TOP 63.0f

// Player_Action_52 places Link at riderPos - 27 in Y, so the seat value is the seat height plus 27.
#define MC_SEAT_X 0.0f
#define MC_SEAT_Y 50.0f
#define MC_SEAT_Z -6.0f
#define MC_MODEL_SCALE 1.06f
#define MC_MODEL_Y_OFFSET -8.0f
#define MC_SHADOW_SCALE 0.248f

#define MKW_FPT 3.0f
#define MKW_BASE_SPEED 81.89f
#define MC_MAX_SPEED 36.0f
#define MC_UNIT_PER_MKW (MC_MAX_SPEED / MKW_BASE_SPEED)

#define MKW_ACC_A0 0.65056f
#define MKW_ACC_A1 0.29360f
#define MKW_ACC_A2 0.37680f
#define MKW_ACC_A3 0.04768f
#define MKW_ACC_T0 0.200f
#define MKW_ACC_T1 0.800f
#define MKW_ACC_T2 0.925f
#define MKW_DRIFT_ACC_A0 2.35f
#define MKW_DRIFT_ACC_A1 0.15f
#define MKW_DRIFT_ACC_T0 0.98f

#define MKW_COAST_FWD 0.98f
#define MKW_COAST_REV 0.95f
#define MKW_BRAKE_ACCEL 3.0f
#define MKW_REVERSE_LIMIT 20.0f
#define MKW_BASE_HANDLING 0.9933f
#define MKW_HANDLING_RESP 0.9025f
#define MKW_DRIFT_RESP 0.98f
#define MKW_DRIFT_TURN_STICK 0.4f
#define MKW_DRIFT_TURN_DIR 0.6f
#define MKW_WHEELIE_TURN 0.35f

#define MKW_MT_BASE 2
#define MKW_MT_INSIDE 3
#define MKW_MT_STICK 0.4f
#define MKW_MT_MAX 270
#define MKW_MT_DURATION 37

#define MC_MT_BONUS 0.08f
#define MC_BOOST_ACCEL 0.9f

#define MKW_WHEELIE_SPEED_BONUS 0.15f
#define MKW_WHEELIE_COOLDOWN 20
#define MKW_WHEELIE_CANCEL_STICK 0.85f
#define MKW_WHEELIE_CANCEL_FRAMES 15
#define MKW_WHEELIE_MIN_RATIO 0.3f
#define MKW_WHEELIE_MAX_FRAMES 180

#define MC_STOPPED 0.35f
#define MC_REVERSE_STICK 0.30f
#define MC_GRAVITY -3.5f
#define MC_MIN_VEL_Y -20.0f
#define MC_STEER_MAX_LOW 0x1C00
#define MC_STEER_MAX_HIGH 0x0500
#define MC_STEER_RATE 0x260
#define MC_STEER_RETURN 0x1A0
#define MC_SLOPE_ACCEL 0.55f
#define MC_LEAN_MAX 0x1400
#define MC_DRIFT_LEAN 0x1900
#define MC_WALL_PROBE_HEIGHT 22.0f
#define MC_WALL_BOUNCE 0.40f
#define MC_WALL_MIN_SFX 12.0f
#define MC_TERRAIN_SLOPE_LIMIT 0.55f

#define MC_SUS_STIFFNESS 0.45f
#define MC_SUS_STIFFNESS_F 0.52f
#define MC_SUS_DAMPING 0.95f
#define MC_SUS_TRAVEL 25.0f

#define MC_MT_FLAME_COUNT 2
#define MC_MT_FLAME_SPREAD 6.0f
#define MC_MT_PIPE_Y (MC_HUB_Y + MC_WHEEL_RADIUS + 2.0f)
#define MC_MT_PIPE_Z (MC_HUB_B_Z - 2.0f)
#define MC_TRAIL_MIN_RATIO 0.35f
#define MC_TRAIL_LIFE 8
#define MC_TRAIL_HALF_Y 5.0f
#define MC_TRAIL_SIDE 6.0f
#define MC_TRAIL_ON_TICKS 7.0f
#define MC_TRAIL_OFF_TICKS 6.0f

#define MC_MT_FLAME_SCALE 0.0008f
#define MC_MT_STAGE1 0.33f
#define MC_MT_STAGE2 0.70f
#define MC_MT_CHARGE_SCALE 0.5f
#define MC_DRIFT_MIN_RATIO 0.30f

#define MC_DRIFT_ANGLE 0x0A00
#define MC_DRIFT_ANGLE_RATE 0x0120
#define MC_DRIFT_ANGLE_DECR 0x01C0
#define MC_DRIFT_TURN_SCALE 0.55f
#define MC_HOP_VEL 10.0f

#define MC_WHEELIE_PITCH 0x1C00
#define MC_WHEELIE_PITCH_STEP 0x300

#define MC_HIT_MIN_SPEED 10.0f
#define MC_HIT_DAMAGE_LOW 1
#define MC_HIT_DAMAGE_HIGH 4

#define MC_MATERIALIZE_TICKS 16
#define MC_DISMISS_TICKS 12
#define MC_SUMMON_AHEAD 90.0f
#define MC_MOUNT_RANGE 55.0f
#define MC_MOUNT_MAX_DY 30.0f
#define MC_DEEP_WATER 28.0f
#define MC_MOUNT_GRACE 18

#define MC_ACTION_PARKED ENHORSE_ACTION_IDLE
#define MC_ACTION_RIDDEN ENHORSE_ACTION_MOUNTED_WALK

typedef enum {
    MC_PHASE_NONE = 0,
    MC_PHASE_MATERIALIZE,
    MC_PHASE_ACTIVE,
    MC_PHASE_DISMISS,
} McPhase;

typedef enum {
    MC_DRIFT_NONE = 0,
    MC_DRIFT_HOP,
    MC_DRIFT_ON,
} McDrift;

typedef struct {
    Actor* actor;
    u8 phase;
    s16 phaseTimer;

    f32 speed;
    s16 steer;
    s16 lean;
    s16 pitch;
    s16 terrainPitch;
    s16 floorPitch;
    f32 wheelSpin;
    s16 lastYaw;

    u8 drift;
    s8 driftDir;
    s16 mtCharge;
    s16 boostTimer;

    s32 trailIdx[2];
    u8 trailsOwned;
    u8 trailOn[2];
    u8 trailWheelie;
    s16 trailTimer[2];

    u8 wheelie;
    s16 wheelieTimer;
    s16 wheelieCooldown;
    s16 wheelieStickTimer;
    s16 wheeliePitch;
    f32 turnSmooth;

    f32 susY[2];
    f32 susVel[2];
    u8 susReady;
    f32 wheelY[2];

    u8 launched;
    u8 wasRiding;
    u8 airborne;
    s16 hopTimer;
    s16 driftAngle;
    s16 mountGrace;
    f32 riderFrame;

    ColliderCylinder hitCol;
    u8 hitColReady;

    s32 mountSide;
    u8 riderTilted;
} MasterCycleState;

static MasterCycleState sMc = { 0 };
static u8 sMcCarry = 0;
static f32 sMcCarrySpeed = 0.0f;
static u8 sMcSceneChanged = 1;

static void MasterCycle_BodyPoint(Actor* actor, f32 lx, f32 ly, f32 lz, Vec3f* out);
static void MasterCycle_Update(Actor* thisx, PlayState* play);
static void MasterCycle_Draw(Actor* thisx, PlayState* play);

static bool MasterCycle_HasModel(void) {
    return ResourceMgr_FileExists(MC_DL_BODY);
}

static u8 MasterCycle_IsRiding(void) {
    Player* player;

    if ((sMc.actor == NULL) || (gPlayState == NULL)) {
        return 0;
    }
    player = GET_PLAYER(gPlayState);
    return (player->stateFlags1 & MC_RIDING_FLAG) && (player->rideActor == sMc.actor);
}

static void MasterCycle_Forget(void) {
    memset(&sMc, 0, sizeof(sMc));
}

static void MasterCycle_Sfx(u16 id) {
    if (sMc.actor != NULL) {
        Audio_PlaySfx_AtPos(&sMc.actor->projectedPos, id);
    }
}

static void MasterCycle_TrailsInit(PlayState* play) {
    static const u8 sP1Start[] = { 190, 235, 255, 190 };
    static const u8 sP2Start[] = { 120, 200, 255, 120 };
    static const u8 sP1End[] = { 120, 190, 255, 60 };
    static const u8 sP2End[] = { 90, 150, 255, 0 };
    EffectBlureInit1 init;

    for (s32 i = 0; i < 4; i++) {
        init.p1StartColor[i] = sP1Start[i];
        init.p2StartColor[i] = sP2Start[i];
        init.p1EndColor[i] = sP1End[i];
        init.p2EndColor[i] = sP2End[i];
    }
    init.elemDuration = MC_TRAIL_LIFE;
    init.unkFlag = false;
    init.calcMode = 2;

    for (s32 i = 0; i < 2; i++) {
        Effect_Add(play, &sMc.trailIdx[i], EFFECT_BLURE1, 0, 0, &init);
    }
    sMc.trailsOwned = 1;
}

static void MasterCycle_TrailsFree(PlayState* play) {
    if (!sMc.trailsOwned) {
        return;
    }
    for (s32 i = 0; i < 2; i++) {
        Effect_Destroy(play, sMc.trailIdx[i]);
    }
    sMc.trailsOwned = 0;
}

static void MasterCycle_Trails(Actor* actor) {
    static const f32 sHubZ[2] = { MC_HUB_F_Z, MC_HUB_B_Z };
    f32 ratio = fabsf(sMc.speed) / MC_MAX_SPEED;

    if (!sMc.trailsOwned) {
        return;
    }
    for (s32 i = 0; i < 2; i++) {
        EffectBlure* blure = Effect_GetByIndex(sMc.trailIdx[i]);
        Vec3f hub;
        Vec3f top;
        Vec3f bottom;
        f32 lx;
        f32 lz;

        if (blure == NULL) {
            continue;
        }
        if (--sMc.trailTimer[i] <= 0) {
            sMc.trailOn[i] = !sMc.trailOn[i];
            sMc.trailTimer[i] =
                (s16)(2.0f + (Rand_ZeroOne() * (sMc.trailOn[i] ? MC_TRAIL_ON_TICKS : MC_TRAIL_OFF_TICKS)));
        }
        // Changing layout teleports the ribbon's mouth, and a blure would join the two points with a quad.
        if (sMc.trailWheelie != (sMc.wheelie ? 1 : 0)) {
            EffectBlure_AddSpace(blure);
            if (i == 1) {
                sMc.trailWheelie = sMc.wheelie ? 1 : 0;
            }
            continue;
        }
        if (sMc.wheelie) {
            lx = (i == 0) ? MC_TRAIL_SIDE : -MC_TRAIL_SIDE;
            lz = MC_HUB_B_Z;
        } else {
            lx = 0.0f;
            lz = sHubZ[i];
            if (!sMc.trailOn[i]) {
                EffectBlure_AddSpace(blure);
                continue;
            }
        }
        if ((ratio < MC_TRAIL_MIN_RATIO) || (sMc.phase != MC_PHASE_ACTIVE)) {
            EffectBlure_AddSpace(blure);
            continue;
        }
        MasterCycle_BodyPoint(actor, lx, MC_HUB_Y, lz, &hub);
        hub.x += actor->world.pos.x;
        hub.y += actor->world.pos.y + MC_MODEL_Y_OFFSET;
        hub.z += actor->world.pos.z;
        bottom.x = top.x = hub.x;
        bottom.z = top.z = hub.z;
        top.y = hub.y + MC_TRAIL_HALF_Y;
        bottom.y = hub.y - MC_TRAIL_HALF_Y;
        EffectBlure_AddVertex(blure, &top, &bottom);
    }
}

// riderPos is relative: Player_Action_52 places Link at rideActor->world.pos + riderPos.
static void MasterCycle_BodyPoint(Actor* actor, f32 lx, f32 ly, f32 lz, Vec3f* out) {
    f32 x = lx;
    f32 y = ly - MC_HUB_Y;
    f32 z = lz - MC_HUB_B_Z;
    f32 sn = Math_SinS(actor->shape.rot.x);
    f32 cs = Math_CosS(actor->shape.rot.x);
    f32 t = (y * cs) - (z * sn);

    z = (y * sn) + (z * cs) + MC_HUB_B_Z;
    y = t + MC_HUB_Y;

    sn = Math_SinS(actor->shape.rot.z);
    cs = Math_CosS(actor->shape.rot.z);
    t = (x * cs) - (y * sn);
    y = (x * sn) + (y * cs);
    x = t;

    sn = Math_SinS(actor->shape.rot.y);
    cs = Math_CosS(actor->shape.rot.y);
    out->x = (x * cs) + (z * sn);
    out->y = y;
    out->z = (-x * sn) + (z * cs);
}

static void MasterCycle_SeatPos(Actor* actor, Vec3f* out) {
    MasterCycle_BodyPoint(actor, MC_SEAT_X, MC_SEAT_Y, MC_SEAT_Z, out);
}

// Published for companions: places a point given relative to Link (x left, y up, z forward) on the cycle, measured from
// where Link actually sits so it follows the lean and the wheelie exactly as he does.
static bool MasterCycle_PlaceBesideRider(Actor* ride, Vec3f* local, Vec3f* pos, Vec3s* rot) {
    Vec3f seat;
    Vec3f point;

    if ((sMc.actor == NULL) || (ride != sMc.actor) || (gPlayState == NULL) || (local == NULL) || (pos == NULL) ||
        (rot == NULL)) {
        return false;
    }
    MasterCycle_SeatPos(ride, &seat);
    MasterCycle_BodyPoint(ride, MC_SEAT_X + local->x, MC_SEAT_Y + local->y, MC_SEAT_Z + local->z, &point);
    *pos = GET_PLAYER(gPlayState)->actor.world.pos;
    pos->x += point.x - seat.x;
    pos->y += point.y - seat.y;
    pos->z += point.z - seat.z;
    *rot = ride->shape.rot;
    return true;
}

static Actor* MasterCycle_Spawn(PlayState* play, Player* player, bool isUnderLink) {
    s16 yaw = player->actor.shape.rot.y;
    f32 ahead = isUnderLink ? 0.0f : MC_SUMMON_AHEAD;
    Vec3f pos = player->actor.world.pos;

    pos.x += Math_SinS(yaw) * ahead;
    pos.z += Math_CosS(yaw) * ahead;
    // Horse_SpawnOverworld's own params: the other horse types kill themselves in Init outside their scenes.
    Actor* actor = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_HORSE, pos.x, pos.y, pos.z, 0, yaw, 0,
                               ENHORSE_PARAMS(ENHORSE_PARAM_4000, ENHORSE_1));

    if ((actor == NULL) || (actor->update == NULL)) {
        return NULL;
    }
    EnHorse* horse = (EnHorse*)actor;

    actor->update = MasterCycle_Update;
    actor->draw = MasterCycle_Draw;
    actor->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    actor->room = -1;
    actor->gravity = MC_GRAVITY;
    actor->terminalVelocity = MC_MIN_VEL_Y;
    actor->speed = 0.0f;
    actor->velocity.x = actor->velocity.y = actor->velocity.z = 0.0f;
    // The shadow is drawn at actor->scale * shadowScale, and the bike is scale 1 where Epona is 0.01.
    ActorShape_Init(&actor->shape, 0.0f, ActorShadow_DrawHorse, MC_SHADOW_SCALE);
    actor->shape.rot.x = actor->shape.rot.z = 0;
    actor->world.rot = actor->shape.rot;

    horse->action = MC_ACTION_PARKED;
    horse->stateFlags = 0;
    horse->animIndex = ENHORSE_ANIM_IDLE;
    horse->curFrame = 0.0f;
    horse->playerControlled = false;

    memset(&sMc, 0, sizeof(sMc));
    sMc.actor = actor;
    sMc.lastYaw = actor->shape.rot.y;
    if (isUnderLink) {
        sMc.phase = MC_PHASE_ACTIVE;
        Actor_SetScale(actor, MC_MODEL_SCALE);
    } else {
        sMc.phase = MC_PHASE_MATERIALIZE;
        sMc.phaseTimer = MC_MATERIALIZE_TICKS;
        Actor_SetScale(actor, 0.001f);
    }
    MasterCycle_SeatPos(actor, &horse->riderPos);
    MasterCycle_TrailsInit(play);
    return actor;
}

static void MasterCycle_ForceDismount(void) {
    EnHorse* horse = (EnHorse*)sMc.actor;

    if (horse != NULL) {
        horse->stateFlags |= ENHORSE_FLAG_6;
    }
}

static void MasterCycle_BeginDismiss(void) {
    if (sMc.actor == NULL) {
        return;
    }
    sMc.phase = MC_PHASE_DISMISS;
    sMc.phaseTimer = MC_DISMISS_TICKS;
    MasterCycle_Sfx(NA_SE_EV_TRE_BOX_APPEAR);
}

static s32 MasterCycle_Cast(PlayState* play, Player* player) {
    if ((sMc.actor != NULL) && (sMc.actor->update == NULL)) {
        MasterCycle_Forget();
    }
    if (sMc.actor != NULL) {
        if (sMc.phase == MC_PHASE_DISMISS) {
            return 0;
        }
        if (MasterCycle_IsRiding()) {
            MasterCycle_ForceDismount();
            sMc.wasRiding = 2;
            return 1;
        }
        MasterCycle_BeginDismiss();
        return 1;
    }
    if (!MasterCycle_HasModel() || (MasterCycle_Spawn(play, player, false) == NULL)) {
        return 0;
    }
    MasterCycle_Sfx(NA_SE_EV_TRE_BOX_APPEAR);
    return 1;
}

// The cycle has no carrot boosts, but the HUD draws Epona's for any ridden horse.
static void MasterCycle_HideCarrots(bool* should, va_list args) {
    if (MasterCycle_IsRiding()) {
        *should = false;
    }
}

// Riding drops the HUD to A/B/minimap; vanilla only raises it again when a button status flips on the dismount.
static void MasterCycle_RestoreHud(void) {
    if ((gSaveContext.hudVisibility != HUD_VISIBILITY_A_B_MINIMAP) &&
        (gSaveContext.hudVisibility != HUD_VISIBILITY_A_HEARTS_MAGIC_MINIMAP_WITH_OVERWRITE)) {
        return;
    }
    Interface_SetHudVisibility(HUD_VISIBILITY_ALL);
}

static void MasterCycle_Tick(PlayState* play, Player* player) {
    static s16 sLastScene = -1;

    if (sMcSceneChanged || (sLastScene != play->sceneId)) {
        sMcSceneChanged = 0;
        sLastScene = play->sceneId;
        MasterCycle_Forget();
        if (!sMcCarry) {
            return;
        }
        sMcCarry = 0;
        Actor* bike = MasterCycle_Spawn(play, player, true);

        if (bike != NULL) {
            Player_MountHorse(play, player, bike);
            sMc.speed = sMcCarrySpeed;
            sMc.lastYaw = bike->shape.rot.y;
        }
        return;
    }
    bool isRiding = MasterCycle_IsRiding();

    if (!isRiding) {
        sMcCarry = 0;
    }
    if (sMc.actor == NULL) {
        return;
    }
    if (sMc.actor->update == NULL) {
        MasterCycle_Forget();
        return;
    }
    // gHorseIsMounted makes Horse_Spawn put Epona under Link in the next scene, so it is cleared every frame of the fade.
    if (isRiding && ((play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->transitionMode != TRANS_MODE_OFF))) {
        gHorseIsMounted = false;
        if (gSaveContext.respawnFlag != -2) {
            sMcCarry = 1;
            sMcCarrySpeed = sMc.speed * 0.6f;
        }
    }
    if ((sMc.wasRiding == 2) && !isRiding && !(player->stateFlags1 & MC_RIDING_FLAG)) {
        sMc.wasRiding = 0;
        MasterCycle_RestoreHud();
        MasterCycle_BeginDismiss();
    }
}

// Offered right before Player_UpdateCommon reads it, after every BG actor (a nearby Epona too) has offered itself.
static void MasterCycle_OfferMount(PlayState* play, Player* player) {
    Actor* actor = sMc.actor;

    if ((actor == NULL) || (actor->update == NULL) || (sMc.phase != MC_PHASE_ACTIVE) ||
        (fabsf(sMc.speed) >= MC_STOPPED) || (player->transformation != PLAYER_FORM_HUMAN) ||
        (player->stateFlags1 & MC_RIDING_FLAG)) {
        return;
    }
    f32 dx = player->actor.world.pos.x - actor->world.pos.x;
    f32 dy = player->actor.world.pos.y - actor->world.pos.y;
    f32 dz = player->actor.world.pos.z - actor->world.pos.z;

    if ((sqrtf(SQ(dx) + SQ(dz)) >= MC_MOUNT_RANGE) || (fabsf(dy) >= MC_MOUNT_MAX_DY)) {
        return;
    }
    f32 localX = (dx * Math_CosS(actor->shape.rot.y)) - (dz * Math_SinS(actor->shape.rot.y));

    sMc.mountSide = (localX >= 0.0f) ? 1 : -1;
    Actor_SetRideActor(play, actor, sMc.mountSide);
}

// B drives the brake and the dismount, so it must not also draw Epona's bow from the saddle.
static void MasterCycle_FilterInput(Player* player, Input* input) {
    if ((sMc.actor == NULL) || (gPlayState == NULL)) {
        return;
    }
    if ((player->stateFlags1 & MC_RIDING_FLAG) && (player->rideActor == sMc.actor)) {
        input->cur.button &= ~BTN_B;
        input->press.button &= ~BTN_B;
        return;
    }
    MasterCycle_OfferMount(gPlayState, player);
}

static f32 MasterCycle_TopSpeed(void) {
    f32 bonus = 0.0f;

    if (sMc.boostTimer > 0) {
        bonus = MC_MT_BONUS;
    } else if (sMc.wheelie) {
        bonus = MKW_WHEELIE_SPEED_BONUS;
    }
    return MC_MAX_SPEED * (1.0f + bonus);
}

static f32 MasterCycle_MkwAccel(f32 ratio, bool isDrifting) {
    if (ratio < 0.0f) {
        return 1.0f;
    }
    if (isDrifting) {
        if (ratio < MKW_DRIFT_ACC_T0) {
            return MKW_DRIFT_ACC_A0 + ((MKW_DRIFT_ACC_A1 - MKW_DRIFT_ACC_A0) * (ratio / MKW_DRIFT_ACC_T0));
        }
        return MKW_DRIFT_ACC_A1;
    }
    if (ratio < MKW_ACC_T0) {
        return MKW_ACC_A0 + ((MKW_ACC_A1 - MKW_ACC_A0) * (ratio / MKW_ACC_T0));
    }
    if (ratio < MKW_ACC_T1) {
        return MKW_ACC_A1 + ((MKW_ACC_A2 - MKW_ACC_A1) * ((ratio - MKW_ACC_T0) / (MKW_ACC_T1 - MKW_ACC_T0)));
    }
    if (ratio < MKW_ACC_T2) {
        return MKW_ACC_A2 + ((MKW_ACC_A3 - MKW_ACC_A2) * ((ratio - MKW_ACC_T1) / (MKW_ACC_T2 - MKW_ACC_T1)));
    }
    return MKW_ACC_A3;
}

static f32 MasterCycle_PerTick(f32 perFrame) {
    return perFrame * perFrame * perFrame;
}

static void MasterCycle_ProbeTerrain(PlayState* play, Actor* actor) {
    CollisionPoly* poly;
    s32 bgId;
    f32 s = Math_SinS(actor->shape.rot.y);
    f32 c = Math_CosS(actor->shape.rot.y);
    Vec3f front = { actor->world.pos.x + (s * MC_HUB_F_Z), actor->world.pos.y + 40.0f,
                    actor->world.pos.z + (c * MC_HUB_F_Z) };
    Vec3f back = { actor->world.pos.x + (s * MC_HUB_B_Z), actor->world.pos.y + 40.0f,
                   actor->world.pos.z + (c * MC_HUB_B_Z) };

    sMc.wheelY[0] = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &front);
    sMc.wheelY[1] = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &back);
    if ((sMc.wheelY[0] <= BGCHECK_Y_MIN) || (sMc.wheelY[1] <= BGCHECK_Y_MIN)) {
        Math_SmoothStepToS(&sMc.terrainPitch, 0, 4, 0x200, 0x10);
        return;
    }
    if (!sMc.airborne) {
        return;
    }
    f32 sp = fabsf(actor->speed);
    s16 want = (sp > 1.0f) ? (s16)(Math_FAtan2F(actor->velocity.y, sp) * (0x8000 / M_PI)) : 0;

    Math_SmoothStepToS(&sMc.terrainPitch, CLAMP(want, -0x2800, 0x2000), 4, 0x300, 0x10);
}

static void MasterCycle_ResolveHull(PlayState* play, Actor* actor) {
    static const f32 sHullZ[2] = { MC_HULL_NOSE_Z, MC_HULL_TAIL_Z };
    f32 sn = Math_SinS(actor->shape.rot.y);
    f32 cs = Math_CosS(actor->shape.rot.y);

    for (s32 i = 0; i < 2; i++) {
        CollisionPoly* poly = NULL;
        s32 bgId = BGCHECK_SCENE;
        f32 reach = sHullZ[i] + ((sHullZ[i] > 0.0f) ? MC_HULL_RADIUS : -MC_HULL_RADIUS);
        Vec3f from = { actor->world.pos.x, actor->world.pos.y + MC_WALL_PROBE_HEIGHT, actor->world.pos.z };
        Vec3f to = { from.x + (sn * reach), from.y, from.z + (cs * reach) };
        Vec3f hit;

        if (!BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, false, false, true, &bgId) ||
            (poly == NULL)) {
            continue;
        }
        f32 nx = COLPOLY_GET_NORMAL(poly->normal.x);
        f32 nz = COLPOLY_GET_NORMAL(poly->normal.z);
        f32 over = ((to.x - hit.x) * nx) + ((to.z - hit.z) * nz);

        if (over > 0.0f) {
            actor->world.pos.x += nx * over;
            actor->world.pos.z += nz * over;
            actor->wallYaw = Math_Atan2S_XY(nz, nx);
            actor->bgCheckFlags |= BGCHECKFLAG_WALL;
        }
    }
}

static void MasterCycle_Move(PlayState* play, Actor* actor) {
    bool hitWall = false;

    // Ground speed is split into cos(pitch) along the floor and sin(pitch) up, so a ramp launches.
    if (!sMc.airborne && (actor->floorPoly != NULL)) {
        f32 nx = COLPOLY_GET_NORMAL(actor->floorPoly->normal.x);
        f32 ny = COLPOLY_GET_NORMAL(actor->floorPoly->normal.y);
        f32 nz = COLPOLY_GET_NORMAL(actor->floorPoly->normal.z);

        if (ny > 0.05f) {
            f32 slope = (-(nx * Math_SinS(actor->world.rot.y)) - (nz * Math_CosS(actor->world.rot.y))) / ny;
            s16 want = Math_Atan2S_XY(1.0f, slope);

            Math_SmoothStepToS(&sMc.floorPitch, (sMc.speed < 0.0f) ? -want : want, 2, 0x600, 0x40);
        }
    } else {
        Math_SmoothStepToS(&sMc.floorPitch, 0, 3, 0x300, 0x40);
    }
    if (!sMc.airborne && !sMc.launched) {
        sMc.speed -= Math_SinS(sMc.floorPitch) * fabsf(MC_GRAVITY) * MC_SLOPE_ACCEL;
    }
    // The floor glue snaps any actor across a gap under 11 units unless it carries this flag.
    actor->bgCheckFlags |= BGCHECKFLAG_PLAYER_800;
    actor->speed = sMc.speed * Math_CosS(sMc.floorPitch);
    if (!sMc.airborne && !sMc.launched && (actor->velocity.y <= 0.0f)) {
        actor->velocity.y = fabsf(sMc.speed) * Math_SinS(sMc.floorPitch);
    }
    Actor_UpdateVelocityWithGravity(actor);

    f32 dist = sqrtf(SQ(actor->velocity.x) + SQ(actor->velocity.y) + SQ(actor->velocity.z));
    s32 substeps = MIN((s32)(dist / 10.0f) + 1, 8);

    for (s32 i = 0; i < substeps; i++) {
        f32 keepY;

        actor->prevPos = actor->world.pos;
        actor->world.pos.x += actor->velocity.x / (f32)substeps;
        actor->world.pos.y += actor->velocity.y / (f32)substeps;
        actor->world.pos.z += actor->velocity.z / (f32)substeps;
        // 0x15 and not Epona's 0x1D, which clamps a grounded body's velocity.y and swallows the ramp launch; the
        // height itself belongs to MasterCycle_Suspension, so the floor snap is undone.
        keepY = actor->world.pos.y;
        Actor_UpdateBgCheckInfo(play, actor, 30.0f, MC_HULL_RADIUS, MC_BODY_TOP, 0x15);
        if (actor->bgCheckFlags & BGCHECKFLAG_GROUND) {
            actor->world.pos.y = keepY;
        }
        MasterCycle_ResolveHull(play, actor);
        if (actor->bgCheckFlags & BGCHECKFLAG_WALL) {
            hitWall = true;
            break;
        }
    }
    if (hitWall && (Math_CosS(actor->wallYaw - actor->world.rot.y) < -0.3f)) {
        if (fabsf(sMc.speed) > MC_WALL_MIN_SFX) {
            MasterCycle_Sfx(NA_SE_EV_BOMB_BOUND);
        }
        sMc.speed *= MC_WALL_BOUNCE;
        sMc.wheelie = 0;
        sMc.drift = MC_DRIFT_NONE;
        sMc.mtCharge = 0;
    }
    sMc.airborne = !(actor->bgCheckFlags & BGCHECKFLAG_GROUND);
}

static void MasterCycle_UpdateThrottle(Input* input, f32 stickMag, f32 ratio) {
    u16 held = input->cur.button;
    f32 top = MasterCycle_TopSpeed();
    f32 accel = MasterCycle_MkwAccel(ratio, sMc.drift == MC_DRIFT_ON) * MC_UNIT_PER_MKW * MKW_FPT;
    f32 brake = MKW_BRAKE_ACCEL * MC_UNIT_PER_MKW * MKW_FPT;

    if (sMc.boostTimer > 0) {
        sMc.boostTimer--;
        sMc.speed = MIN(sMc.speed + (MC_BOOST_ACCEL * MC_UNIT_PER_MKW * MKW_FPT), top);
    } else if (held & BTN_A) {
        sMc.speed += (sMc.speed < 0.0f) ? brake : accel;
    } else if (held & BTN_B) {
        if (sMc.speed > 0.0f) {
            sMc.speed = MAX(sMc.speed - brake, 0.0f);
        } else if (stickMag > MC_REVERSE_STICK) {
            if (!sMc.wheelie) {
                sMc.speed -= accel * 0.6f * stickMag;
            }
        } else if (fabsf(sMc.speed) < MC_STOPPED) {
            MasterCycle_ForceDismount();
        } else {
            sMc.speed *= MasterCycle_PerTick(MKW_COAST_REV);
        }
    } else {
        sMc.speed *= MasterCycle_PerTick((sMc.speed >= 0.0f) ? MKW_COAST_FWD : MKW_COAST_REV);
        if (fabsf(sMc.speed) < 0.05f) {
            sMc.speed = 0.0f;
        }
    }
    if ((sMc.drift != MC_DRIFT_ON) && !sMc.wheelie) {
        f32 r = MIN(fabsf(ratio), 1.0f);
        f32 scrub = MKW_BASE_HANDLING + ((1.0f - MKW_BASE_HANDLING) * (1.0f - (fabsf(sMc.turnSmooth) * r)));

        sMc.speed *= MasterCycle_PerTick(scrub);
    }
    if (sMc.speed > top) {
        sMc.speed = (sMc.speed - top > 0.5f) ? (sMc.speed - 0.5f) : top;
    }
    sMc.speed = MAX(sMc.speed, -(MKW_REVERSE_LIMIT * MC_UNIT_PER_MKW));
}

static void MasterCycle_UpdateWheelie(Input* input, f32 stickX, f32 ratio) {
    if (sMc.wheelieCooldown > 0) {
        sMc.wheelieCooldown--;
    }
    if (!sMc.wheelie) {
        if ((input->press.button & BTN_DUP) && (sMc.wheelieCooldown == 0) && (sMc.drift == MC_DRIFT_NONE) &&
            !sMc.airborne && (ratio >= MKW_WHEELIE_MIN_RATIO)) {
            sMc.wheelie = 1;
            sMc.wheelieTimer = 0;
            sMc.wheelieStickTimer = 0;
            sMc.wheelieCooldown = (s16)(MKW_WHEELIE_COOLDOWN / MKW_FPT);
            MasterCycle_Sfx(NA_SE_EV_HORSE_JUMP);
        }
    } else {
        bool isCancelled = false;

        sMc.wheelieTimer++;
        if (fabsf(stickX) > MKW_WHEELIE_CANCEL_STICK) {
            sMc.wheelieStickTimer++;
            isCancelled = sMc.wheelieStickTimer > (s16)(MKW_WHEELIE_CANCEL_FRAMES / MKW_FPT);
        } else {
            sMc.wheelieStickTimer = 0;
        }
        if ((input->press.button & BTN_DDOWN) || (ratio < MKW_WHEELIE_MIN_RATIO) || (sMc.drift != MC_DRIFT_NONE) ||
            (sMc.wheelieTimer > (s16)(MKW_WHEELIE_MAX_FRAMES / MKW_FPT))) {
            isCancelled = true;
        }
        if (isCancelled) {
            sMc.wheelie = 0;
            sMc.wheelieCooldown = (s16)(MKW_WHEELIE_COOLDOWN / MKW_FPT);
            MasterCycle_Sfx(NA_SE_EV_HORSE_LAND);
        }
    }
    Math_SmoothStepToS(&sMc.wheeliePitch, sMc.wheelie ? MC_WHEELIE_PITCH : 0, 3, MC_WHEELIE_PITCH_STEP, 0x40);
}

static void MasterCycle_UpdateDrift(Actor* actor, Input* input, f32 stickX, f32 ratio) {
    u16 held = input->cur.button;

    if (sMc.hopTimer > 0) {
        sMc.hopTimer--;
    }
    switch (sMc.drift) {
        case MC_DRIFT_NONE:
            if ((input->press.button & BTN_R) && !sMc.airborne && (fabsf(sMc.speed) > 0.5f)) {
                actor->velocity.y = MC_HOP_VEL;
                sMc.drift = MC_DRIFT_HOP;
                sMc.hopTimer = 6;
                sMc.wheelie = 0;
                MasterCycle_Sfx(NA_SE_EV_HORSE_JUMP);
                actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
                sMc.launched = 1;
            }
            break;
        case MC_DRIFT_HOP:
            if (!(held & BTN_R)) {
                sMc.drift = MC_DRIFT_NONE;
            } else if (!sMc.airborne && (sMc.hopTimer <= 3)) {
                sMc.drift = MC_DRIFT_ON;
                sMc.driftDir = (stickX > 0.05f) ? 1 : ((stickX < -0.05f) ? -1 : ((sMc.steer >= 0) ? 1 : -1));
                sMc.mtCharge = 0;
            }
            break;
        case MC_DRIFT_ON:
            if (!(held & BTN_R) || (ratio < MC_DRIFT_MIN_RATIO * 0.6f)) {
                if (sMc.mtCharge >= MKW_MT_MAX) {
                    sMc.boostTimer = (s16)(MKW_MT_DURATION / MKW_FPT);
                    MasterCycle_Sfx(NA_SE_IT_SWORD_SWING_HARD);
                }
                sMc.drift = MC_DRIFT_NONE;
                sMc.mtCharge = 0;
            } else {
                bool isInto = (stickX * (f32)sMc.driftDir) > MKW_MT_STICK;
                s16 before = sMc.mtCharge;

                sMc.mtCharge += (s16)((MKW_MT_BASE + (isInto ? MKW_MT_INSIDE : 0)) * (s32)MKW_FPT * MC_MT_CHARGE_SCALE);
                sMc.mtCharge = MIN(sMc.mtCharge, MKW_MT_MAX);
                if (sMc.mtCharge < MKW_MT_MAX) {
                    // A continuous sound is played minus SFX_FLAG every tick; with the flag it never ends.
                    MasterCycle_Sfx(NA_SE_IT_SWORD_CHARGE - SFX_FLAG);
                } else if (before < MKW_MT_MAX) {
                    MasterCycle_Sfx(NA_SE_IT_HOOKSHOT_READY);
                }
            }
            break;
        default:
            sMc.drift = MC_DRIFT_NONE;
            break;
    }
}

static void MasterCycle_Steer(Actor* actor, f32 stickX, f32 ratio) {
    f32 raw = sMc.wheelie ? (stickX * MKW_WHEELIE_TURN) : stickX;
    f32 maxSteer = (f32)MC_STEER_MAX_LOW + (((f32)MC_STEER_MAX_HIGH - (f32)MC_STEER_MAX_LOW) * MIN(fabsf(ratio), 1.0f));
    f32 resp = 1.0f - MasterCycle_PerTick(1.0f - ((sMc.drift == MC_DRIFT_ON) ? MKW_DRIFT_RESP : MKW_HANDLING_RESP));
    s16 wantAngle;

    if (sMc.drift == MC_DRIFT_ON) {
        f32 t = (MKW_DRIFT_TURN_STICK * sMc.turnSmooth) + (MKW_DRIFT_TURN_DIR * (f32)sMc.driftDir);

        wantAngle = (s16)(CLAMP(t, -1.0f, 1.0f) * maxSteer * MC_DRIFT_TURN_SCALE);
    } else if (sMc.drift == MC_DRIFT_HOP) {
        wantAngle = (s16)(((stickX > 0.2f) ? 1.0f : ((stickX < -0.2f) ? -1.0f : 0.0f)) * maxSteer);
    } else {
        wantAngle = (s16)(raw * maxSteer);
    }
    sMc.turnSmooth = CLAMP(sMc.turnSmooth + ((raw - sMc.turnSmooth) * resp), -1.0f, 1.0f);
    Math_SmoothStepToS(&sMc.steer, wantAngle, 2, (wantAngle == 0) ? MC_STEER_RETURN : MC_STEER_RATE, 0x20);

    f32 yawRad = (sMc.speed / MC_WHEELBASE) * (Math_SinS(sMc.steer) / Math_CosS(sMc.steer));

    if (sMc.airborne) {
        yawRad *= 0.35f;
    }
    // The body pivots on the rear axle, so the tail tracks inside the turn.
    f32 rearX = actor->world.pos.x + (Math_SinS(actor->world.rot.y) * MC_HUB_B_Z);
    f32 rearZ = actor->world.pos.z + (Math_CosS(actor->world.rot.y) * MC_HUB_B_Z);

    actor->world.rot.y += (s16)(yawRad * (0x8000 / M_PI));
    actor->world.pos.x = rearX - (Math_SinS(actor->world.rot.y) * MC_HUB_B_Z);
    actor->world.pos.z = rearZ - (Math_CosS(actor->world.rot.y) * MC_HUB_B_Z);
}

static void MasterCycle_UpdatePose(Actor* actor, f32 stickX) {
    s16 wantDrift = (sMc.drift == MC_DRIFT_ON) ? (s16)(-sMc.driftDir * MC_DRIFT_ANGLE) : 0;

    if (sMc.driftAngle < wantDrift) {
        sMc.driftAngle = MIN(sMc.driftAngle + ((wantDrift > 0) ? MC_DRIFT_ANGLE_RATE : MC_DRIFT_ANGLE_DECR), wantDrift);
    } else if (sMc.driftAngle > wantDrift) {
        sMc.driftAngle = MAX(sMc.driftAngle - ((wantDrift < 0) ? MC_DRIFT_ANGLE_RATE : MC_DRIFT_ANGLE_DECR), wantDrift);
    }
    s16 turnVel = actor->world.rot.y - sMc.lastYaw;
    f32 sp = fabsf(sMc.speed) / MC_MAX_SPEED;
    s16 wantLean = (s16)CLAMP(-(f32)MC_LEAN_MAX * sp * ((f32)turnVel / 0x400), -MC_LEAN_MAX, MC_LEAN_MAX);

    if (sMc.drift == MC_DRIFT_ON) {
        f32 into = MAX(0.6f + (0.4f * stickX * (f32)sMc.driftDir), 0.2f);

        wantLean = (s16)(-(f32)sMc.driftDir * (f32)MC_DRIFT_LEAN * into);
    }
    Math_SmoothStepToS(&sMc.lean, wantLean, 3, 0x280, 0x20);
    sMc.lastYaw = actor->world.rot.y;

    sMc.wheelSpin += sMc.speed / MC_WHEEL_RADIUS;
    if (sMc.wheelSpin > (2.0f * M_PI)) {
        sMc.wheelSpin -= 2.0f * M_PI;
    }
    if (sMc.wheelSpin < 0.0f) {
        sMc.wheelSpin += 2.0f * M_PI;
    }
}

static void MasterCycle_Drive(PlayState* play, Actor* actor, Player* player) {
    Input input = play->state.input[0];
    f32 stickX = CLAMP((f32)input.rel.stick_x / 60.0f, -1.0f, 1.0f);
    f32 stickY = CLAMP((f32)input.rel.stick_y / 60.0f, -1.0f, 1.0f);
    f32 stickMag = sqrtf(SQ(stickX) + SQ(stickY));

    stickX = -stickX;
    if (Player_InCsMode(play) || (player->stateFlags1 & (PLAYER_STATE1_TALKING | MC_CUTSCENE_FLAG))) {
        input.cur.button = 0;
        input.press.button = 0;
        stickX = 0.0f;
        stickMag = 0.0f;
    }
    f32 ratio = MIN(sMc.speed / MC_MAX_SPEED, 1.0f + MC_MT_BONUS);

    MasterCycle_UpdateThrottle(&input, stickMag, ratio);
    MasterCycle_UpdateWheelie(&input, stickX, ratio);
    MasterCycle_UpdateDrift(actor, &input, stickX, ratio);
    MasterCycle_Steer(actor, stickX, ratio);
    MasterCycle_UpdatePose(actor, stickX);
}

// Player_Action_52 reads only animIndex and curFrame off the mount, so both are driven by hand on Epona's clip lengths.
static void MasterCycle_UpdateRiderPose(EnHorse* horse) {
    f32 sp = fabsf(sMc.speed);
    s32 want;
    const char* anim;
    f32 playSpeed;

    if (sMc.airborne) {
        want = ENHORSE_ANIM_LOW_JUMP;
        anim = object_horse_link_child_Anim_0035B0;
        playSpeed = 1.0f;
    } else if (sp < MC_STOPPED) {
        want = ENHORSE_ANIM_IDLE;
        anim = gEponaIdleAnim;
        playSpeed = 1.0f;
    } else if (sp < MC_MAX_SPEED * 0.33f) {
        want = ENHORSE_ANIM_WALK;
        anim = gEponaWalkAnim;
        playSpeed = 0.5f + (sp / (MC_MAX_SPEED * 0.33f));
    } else if (sp < MC_MAX_SPEED * 0.66f) {
        want = ENHORSE_ANIM_TROT;
        anim = gEponaTrotAnim;
        playSpeed = 0.75f + ((sp - (MC_MAX_SPEED * 0.33f)) / (MC_MAX_SPEED * 0.66f));
    } else {
        want = ENHORSE_ANIM_GALLOP;
        anim = gEponaGallopAnim;
        playSpeed = 0.9f + ((sp - (MC_MAX_SPEED * 0.66f)) / (MC_MAX_SPEED * 1.3f));
    }
    if (horse->animIndex != want) {
        horse->animIndex = want;
        sMc.riderFrame = 0.0f;
    }
    f32 last = (f32)Animation_GetLastFrame((void*)anim);

    sMc.riderFrame += playSpeed;
    while ((last > 0.0f) && (sMc.riderFrame > last)) {
        sMc.riderFrame -= last;
    }
    horse->curFrame = sMc.riderFrame;
}

static void MasterCycle_UpdateHitCollider(PlayState* play, Actor* actor) {
    f32 sp = fabsf(sMc.speed);

    if (sp < MC_HIT_MIN_SPEED) {
        return;
    }
    f32 t = MIN((sp - MC_HIT_MIN_SPEED) / (MC_MAX_SPEED * (1.0f + MC_MT_BONUS) - MC_HIT_MIN_SPEED), 1.0f);
    CombatColliderConfig cfg = {
        DMG_HORSE_TRAMPLE,
        (u8)(MC_HIT_DAMAGE_LOW + (s32)((MC_HIT_DAMAGE_HIGH - MC_HIT_DAMAGE_LOW) * t + 0.5f)),
        0,
        MC_WHEELBASE * 0.5f + 8.0f,
        MC_BODY_TOP,
    };

    if (!sMc.hitColReady) {
        Combat_InitCylinder(play, &sMc.hitCol, actor, &cfg);
        sMc.hitColReady = 1;
    }
    Combat_UpdateCylinder(&sMc.hitCol, &actor->world.pos, &cfg);
    CollisionCheck_SetAT(play, &play->colChkCtx, &sMc.hitCol.base);
    if (sMc.hitCol.base.atFlags & AT_HIT) {
        sMc.hitCol.base.atFlags &= ~AT_HIT;
        sMc.speed *= 0.85f;
    }
}

static void MasterCycle_Suspension(Actor* actor) {
    static const f32 sStiffness[2] = { MC_SUS_STIFFNESS_F, MC_SUS_STIFFNESS };
    s32 grounded = 0;

    if (sMc.launched) {
        f32 land = MAX(sMc.wheelY[0], sMc.wheelY[1]);

        sMc.airborne = 1;
        actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        sMc.susReady = 0;
        if ((actor->velocity.y <= 0.0f) && (land > BGCHECK_Y_MIN) && (actor->world.pos.y <= land)) {
            actor->world.pos.y = land;
            actor->velocity.y = 0.0f;
            sMc.launched = 0;
            sMc.airborne = 0;
            actor->bgCheckFlags |= BGCHECKFLAG_GROUND;
        }
        return;
    }
    for (s32 i = 0; i < 2; i++) {
        f32 target = sMc.wheelY[i];

        if (target <= BGCHECK_Y_MIN) {
            sMc.susY[i] = actor->world.pos.y;
            sMc.susVel[i] = 0.0f;
            continue;
        }
        if (!sMc.susReady) {
            sMc.susY[i] = target;
            sMc.susVel[i] = 0.0f;
        }
        f32 err = target - sMc.susY[i];

        if (err < -MC_SUS_TRAVEL) {
            sMc.susY[i] = actor->world.pos.y;
            sMc.susVel[i] = 0.0f;
            continue;
        }
        if (err > MC_SUS_TRAVEL) {
            sMc.susY[i] = target;
            sMc.susVel[i] = 0.0f;
        } else {
            sMc.susVel[i] += (err * sStiffness[i]) - (sMc.susVel[i] * MC_SUS_DAMPING);
            sMc.susY[i] += sMc.susVel[i];
        }
        grounded++;
    }
    sMc.susReady = 1;

    f32 mid = (sMc.susY[0] + sMc.susY[1]) * 0.5f;

    if ((grounded == 0) || ((mid - actor->world.pos.y) < -MC_SUS_TRAVEL)) {
        sMc.airborne = 1;
        actor->bgCheckFlags &= ~BGCHECKFLAG_GROUND;
        return;
    }
    actor->world.pos.y = mid;
    actor->velocity.y = MAX(actor->velocity.y, 0.0f);

    f32 tiltLimit = MC_WHEELBASE * MC_TERRAIN_SLOPE_LIMIT;
    f32 tilt = CLAMP(sMc.susY[0] - sMc.susY[1], -tiltLimit, tiltLimit);

    sMc.terrainPitch = (s16)(Math_FAtan2F(tilt, MC_WHEELBASE) * (0x8000 / M_PI));
    sMc.airborne = 0;
    actor->bgCheckFlags |= BGCHECKFLAG_GROUND;
}

static void MasterCycle_UpdatePhase(Actor* thisx, PlayState* play) {
    EnHorse* horse = (EnHorse*)thisx;

    if (sMc.phase == MC_PHASE_MATERIALIZE) {
        f32 t = 1.0f - ((f32)sMc.phaseTimer / (f32)MC_MATERIALIZE_TICKS);

        Actor_SetScale(thisx, MC_MODEL_SCALE * (0.05f + (0.95f * t)));
        thisx->speed = 0.0f;
        Actor_MoveWithGravity(thisx);
        Actor_UpdateBgCheckInfo(play, thisx, 30.0f, MC_BODY_HALF_WIDTH, MC_BODY_TOP, 0x1D);
        thisx->prevPos = thisx->world.pos;
        if (--sMc.phaseTimer <= 0) {
            sMc.phase = MC_PHASE_ACTIVE;
            Actor_SetScale(thisx, MC_MODEL_SCALE);
        }
        MasterCycle_SeatPos(thisx, &horse->riderPos);
        return;
    }
    f32 t = (f32)sMc.phaseTimer / (f32)MC_DISMISS_TICKS;

    Actor_SetScale(thisx, MC_MODEL_SCALE * (0.05f + (0.95f * t)));
    if (--sMc.phaseTimer <= 0) {
        thisx->bgCheckFlags &= ~BGCHECKFLAG_PLAYER_800;
        MasterCycle_TrailsFree(play);
        Actor_Kill(thisx);
        MasterCycle_Forget();
    }
}

static void MasterCycle_UpdateRider(Actor* thisx, PlayState* play, Player* player, bool isRiding) {
    EnHorse* horse = (EnHorse*)thisx;

    if (isRiding && !sMc.wasRiding) {
        sMc.mountGrace = MC_MOUNT_GRACE;
    }
    if (!isRiding) {
        sMc.mountGrace = 0;
        horse->action = MC_ACTION_PARKED;
        sMc.speed *= 0.9f;
        if (fabsf(sMc.speed) < 0.05f) {
            sMc.speed = 0.0f;
        }
        sMc.wheelie = 0;
        sMc.drift = MC_DRIFT_NONE;
        sMc.boostTimer = 0;
        Math_SmoothStepToS(&sMc.wheeliePitch, 0, 3, MC_WHEELIE_PITCH_STEP, 0x40);
        Math_SmoothStepToS(&sMc.lean, 0, 3, 0x280, 0x20);
        Math_SmoothStepToS(&sMc.steer, 0, 3, MC_STEER_RETURN, 0x40);
    } else if (sMc.mountGrace == 0) {
        // A timer of our own, not av2: in MM that field is shared scratch and flickers to 0 while seated.
        horse->action = MC_ACTION_RIDDEN;
        MasterCycle_Drive(play, thisx, player);
    } else {
        horse->action = MC_ACTION_RIDDEN;
        sMc.mountGrace--;
        sMc.speed = 0.0f;
    }
    if (!isRiding && (horse->stateFlags & ENHORSE_FLAG_6)) {
        horse->stateFlags &= ~ENHORSE_FLAG_6;
    }
    sMc.wasRiding = isRiding ? 1 : ((sMc.wasRiding == 2) ? 2 : 0);
}

static void MasterCycle_UpdateColliders(Actor* thisx, PlayState* play, bool isRiding) {
    EnHorse* horse = (EnHorse*)thisx;
    f32 sn = Math_SinS(thisx->shape.rot.y);
    f32 cs = Math_CosS(thisx->shape.rot.y);

    Collider_UpdateCylinder(thisx, &horse->colliderCylinder1);
    Collider_UpdateCylinder(thisx, &horse->colliderCylinder2);
    horse->colliderCylinder1.dim.pos.x += (s16)(sn * 22.0f);
    horse->colliderCylinder1.dim.pos.z += (s16)(cs * 22.0f);
    horse->colliderCylinder2.dim.pos.x += (s16)(sn * -24.0f);
    horse->colliderCylinder2.dim.pos.z += (s16)(cs * -24.0f);
    CollisionCheck_SetOC(play, &play->colChkCtx, &horse->colliderCylinder1.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &horse->colliderCylinder2.base);
    if (isRiding) {
        MasterCycle_UpdateHitCollider(play, thisx);
    }
}

static void MasterCycle_Update(Actor* thisx, PlayState* play) {
    EnHorse* horse = (EnHorse*)thisx;
    Player* player = GET_PLAYER(play);

    if (thisx != sMc.actor) {
        thisx->update = EnHorse_Update;
        thisx->draw = EnHorse_Draw;
        return;
    }
    if (sMc.phase != MC_PHASE_ACTIVE) {
        MasterCycle_UpdatePhase(thisx, play);
        return;
    }
    // child is the player only while he is genuinely in the saddle: he clears it the frame he starts a dismount.
    bool isRiding = (player->stateFlags1 & MC_RIDING_FLAG) && (player->rideActor == thisx) &&
                    (thisx->child == &player->actor);

    MasterCycle_UpdateRider(thisx, play, player, isRiding);
    MasterCycle_Move(play, thisx);
    MasterCycle_ProbeTerrain(play, thisx);
    MasterCycle_Suspension(thisx);
    MasterCycle_Trails(thisx);

    // The seated poses are the human skeleton's, so deep water and a transformation both put Link off.
    if ((thisx->bgCheckFlags & BGCHECKFLAG_WATER) && (thisx->depthInWater > MC_DEEP_WATER)) {
        sMc.speed = 0.0f;
        if (isRiding) {
            MasterCycle_ForceDismount();
        }
    }
    if (isRiding && (player->transformation != PLAYER_FORM_HUMAN)) {
        MasterCycle_ForceDismount();
    }
    sMc.pitch = (s16)(-sMc.terrainPitch - sMc.wheeliePitch);
    // Nose-up is negative rot.x. In a drift world.rot.y is the travel direction and shape.rot.y the facing.
    thisx->shape.rot.y = (s16)(thisx->world.rot.y + sMc.driftAngle);
    thisx->shape.rot.x = sMc.pitch;
    thisx->shape.rot.z = sMc.lean;
    thisx->world.rot.x = 0;
    thisx->world.rot.z = 0;
    MasterCycle_SeatPos(thisx, &horse->riderPos);
    MasterCycle_UpdateRiderPose(horse);

    // The ride action copies only the yaw, so the lean and pitch are written here and zeroed on dismount.
    if (isRiding) {
        player->actor.shape.rot.x = thisx->shape.rot.x;
        player->actor.shape.rot.z = thisx->shape.rot.z;
        sMc.riderTilted = 1;
    } else if (sMc.riderTilted) {
        player->actor.shape.rot.x = 0;
        player->actor.shape.rot.z = 0;
        sMc.riderTilted = 0;
    }
    MasterCycle_UpdateColliders(thisx, play, isRiding);
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 45.0f;
    thisx->speed = fabsf(sMc.speed);
}

static void MasterCycle_DrawChargeFlames(PlayState* play, Actor* actor) {
    static const Color_RGBA8 sPrim[3] = {
        { 255, 200, 60, 255 },
        { 0, 170, 255, 255 },
        { 255, 170, 255, 255 },
    };
    static const Color_RGB8 sEnv[3] = {
        { 255, 80, 0 },
        { 0, 0, 255 },
        { 100, 0, 255 },
    };
    f32 charge = (f32)sMc.mtCharge / (f32)MKW_MT_MAX;

    if ((sMc.drift != MC_DRIFT_ON) || (charge <= 0.02f)) {
        return;
    }
    s32 stage = (charge < MC_MT_STAGE1) ? 0 : ((charge < MC_MT_STAGE2) ? 1 : 2);
    f32 scale = MC_MT_FLAME_SCALE * (0.55f + (0.45f * charge) + ((stage == 2) ? 0.35f : 0.0f));

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 32, 64, 1, 0,
                                             (play->gameplayFrames * -20) & 511, 32, 128, 0, 0, 0, -20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, sPrim[stage].r, sPrim[stage].g, sPrim[stage].b, sPrim[stage].a);
    gDPSetEnvColor(POLY_XLU_DISP++, sEnv[stage].r, sEnv[stage].g, sEnv[stage].b, 0);
    for (s32 i = 0; i < MC_MT_FLAME_COUNT; i++) {
        Matrix_Push();
        Matrix_Translate((i == 0) ? MC_MT_FLAME_SPREAD : -MC_MT_FLAME_SPREAD, MC_MT_PIPE_Y, MC_MT_PIPE_Z,
                         MTXMODE_APPLY);
        // The flame grows along its +Y, so a quarter turn throws it backwards; the yaw then rolls it to face the camera.
        Matrix_RotateXF(-M_PI / 2.0f, MTXMODE_APPLY);
        Matrix_RotateYS((s16)((Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) - actor->shape.rot.y) + 0x8000),
                        MTXMODE_APPLY);
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void MasterCycle_DrawWheel(PlayState* play, const char* wheelDL, f32 hubZ, f32 steer) {
    if (!ResourceMgr_FileExists(wheelDL)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(0.0f, MC_HUB_Y, hubZ, MTXMODE_APPLY);
    Matrix_RotateYF(steer, MTXMODE_APPLY);
    Matrix_RotateXF(sMc.wheelSpin, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)wheelDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void MasterCycle_Draw(Actor* thisx, PlayState* play) {
    f32 scale = thisx->scale.x * MC_MODEL_SCALE;

    if (!MasterCycle_HasModel()) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y + MC_MODEL_Y_OFFSET, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_RotateYS(thisx->shape.rot.y, MTXMODE_APPLY);
    Matrix_RotateZS(thisx->shape.rot.z, MTXMODE_APPLY);
    Matrix_Translate(0.0f, MC_HUB_Y, MC_HUB_B_Z, MTXMODE_APPLY);
    Matrix_RotateXS(thisx->shape.rot.x, MTXMODE_APPLY);
    Matrix_Translate(0.0f, -MC_HUB_Y, -MC_HUB_B_Z, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)MC_DL_BODY);
    if (ResourceMgr_FileExists(MC_DL_LIGHTS)) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)MC_DL_LIGHTS);
    }
    CLOSE_DISPS(play->state.gfxCtx);

    MasterCycle_DrawWheel(play, MC_DL_WHEEL_B, MC_HUB_B_Z, 0.0f);
    MasterCycle_DrawChargeFlames(play, thisx);
    MasterCycle_DrawWheel(play, MC_DL_WHEEL_F, MC_HUB_F_Z, BINANG_TO_RAD(sMc.steer));
}
