/**
 * Cane of Pacci: a tap flips what Link aims at onto its back, helpless, the way a hammered Tektite goes over.
 * Holding the button lifts it instead and letting go throws it, and how much the cane can lift is the strength
 * of Link's form: nothing as a Deku, pots and light foes as a Human, heavy foes and big rocks as a Zora, blocks
 * as a Goron.
 */

#include <math.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Ishi/z_en_ishi.h"

#define PACCI_KEY "nei.cane_pacci"
#define PACCI_POOL_SIZE 8
#define PACCI_HOLD_FRAMES 10
#define PACCI_CAST_SPEED 2.0f
#define PACCI_CAST_FIRE_FRAME 8
#define PACCI_CAST_TIMEOUT 90

#define PACCI_SCAN_RANGE 220.0f
#define PACCI_SCAN_MIN_DIST 30.0f
#define PACCI_SCAN_CONE 0x1800
#define PACCI_TINT_FRAMES 4
#define PACCI_HURT_TINT_FRAMES 12

// En_Tite's flip onto its back, constant for constant, and a thrown pot's tumble and mass.
#define PACCI_FLIP_LAUNCH_VEL_Y 11.0f
#define PACCI_FLIP_GRAVITY -1.0f
#define PACCI_FLIP_MIN_VEL_Y -22.0f
#define PACCI_FLIP_ROT_STEP 4000
#define PACCI_FLIP_YOFFSET_STEP 400.0f
#define PACCI_FLIP_YOFFSET_MAX 2800.0f
#define PACCI_FLIP_RIGHT_VEL_Y 13.0f
#define PACCI_FLIP_RIGHT_ROT_STEP 0xFA0
#define PACCI_FLIP_ON_BACK_FRAMES 100
#define PACCI_FLIP_THROWN_MASS 240
#define PACCI_FLIP_TUMBLE_STEP 0x64

#define PACCI_LIFT_DIST 70.0f
#define PACCI_LIFT_HEIGHT 45.0f
#define PACCI_LIFT_FOLLOW 0.55f
#define PACCI_LIFT_SPIN 0x400
#define PACCI_LIFT_THROW_BASE_SPEED 8.0f
#define PACCI_LIFT_THROW_SPEED_PER_STRENGTH 4.0f
// Nearly flat: the object already hangs above Link, so the arc starts up there.
#define PACCI_LIFT_THROW_VEL_Y 0.5f
#define PACCI_LIFT_GRAVITY -1.4f
#define PACCI_LIFT_FLIGHT_FRAMES 90

#define PACCI_BURST_FRAMES 3
#define PACCI_BURST_DAMAGE_PER_STRENGTH 4
#define PACCI_BURST_MIN_DAMAGE 8

// Enemy weight classes: past these masses a foe needs the next strength.
#define PACCI_LIGHT_MASS 50
#define PACCI_MEDIUM_MASS 150

#define PACCI_BG_CHECK_FLAGS 0x85

#define PACCI_VFX_MAX_FRAMES 120
#define PACCI_VFX_REACH_FRAMES 7
#define PACCI_VFX_RELEASE_FRAMES 10
#define PACCI_VFX_RIBBON_POINTS 13
#define PACCI_VFX_GRIP_POINTS 8

typedef enum {
    PACCI_FLIP_AIRBORNE,
    PACCI_FLIP_DOWNED,
    PACCI_FLIP_RIGHTING,
} PacciFlipPhase;

typedef enum {
    PACCI_STOWED,
    PACCI_READY,
    PACCI_CASTING,
    PACCI_SWINGING,
} PacciCanePhase;

// Everything a flip overwrites, so a foe that survives it gets its own behaviour back exactly as it was.
typedef struct {
    Actor* actor;
    u8 phase;
    s16 timer;
    ActorFunc origUpdate;
    u32 origFlags;
    f32 origGravity;
    f32 origTerminalVelocity;
    Vec3s origShapeRot;
    Vec3s origWorldRot;
    s8 origRoom;
    u8 origMass;
    f32 origYOffset;
    s16 tumbleX;
    s16 tumbleY;
    s16 tumbleTargetX;
    s16 tumbleTargetY;
    ColliderCylinder collider;
    bool isColliderReady;
} PacciFlip;

typedef struct {
    Actor* held;
    bool isThrown;
    bool frozeEnemy;
    s16 flightTimer;
    f32 origGravity;
    f32 origTerminalVelocity;
    s8 origRoom;
    ActorFunc origUpdate;
    u32 origFlags;
    ColliderCylinder collider;
    bool isColliderReady;
} PacciLift;

typedef struct {
    Actor* target;
    s16 age;
    s16 releaseTimer;
    s16 startRotZ;
    f32 grabHeight;
    Vec3f lastTargetPos;
    bool isReleasing;
    bool isPersistent;
    bool hasLastTargetPos;
} PacciVfx;

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconCaneOfPacciTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gCaneOfPacciNameTex";
static const ALIGN_ASSET(2) char sCaneDL[] = "__OTR__objects/object_pacci_cane/g_somaria_cane_dl";
static const ALIGN_ASSET(2) char sCaneGiveDL[] = "__OTR__objects/object_pacci_cane/g_somaria_cane_give_dl";
static const ALIGN_ASSET(2) char sCastAnim[] = "__OTR__misc/link_animetion/gPlayerAnim_nei_somaria";
static const ALIGN_ASSET(2) char sRaisedAnim[] = "__OTR__objects/gameplay_keep/gPlayerAnim_link_fighter_power_kiru_wait";
// Majora's Mask has no hammer swing clip; the end of the sword's charged wait is the closest overhead strike.
static const ALIGN_ASSET(2) char sThrowAnim[] =
    "__OTR__objects/gameplay_keep/gPlayerAnim_link_fighter_power_kiru_wait_end";
static const ALIGN_ASSET(2) char sBeamDL[] = "__OTR__objects/object_dy_obj/gGreatFairySpiralBeamDL";
static const ALIGN_ASSET(2) char sFlash1DL[] = "__OTR__objects/gameplay_keep/gEffFlash1DL";
static const ALIGN_ASSET(2) char sFlash2DL[] = "__OTR__objects/gameplay_keep/gEffFlash2DL";
static const ALIGN_ASSET(2) char sSparklesDL[] = "__OTR__objects/gameplay_keep/gEffSparklesDL";
static const ALIGN_ASSET(2) char sFlareRingDL[] = "__OTR__objects/gameplay_keep/gLensFlareRingDL";

static const u32 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,      BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT, BTN_DDOWN, BTN_DUP };

// Foes that would break if knocked over, and song spots that have no body to move.
static const s16 sUntouchable[] = { ACTOR_EN_IK,      ACTOR_EN_TORCH2, ACTOR_EN_DINOFOS,        ACTOR_EN_WALLMAS,
                                    ACTOR_EN_FLOORMAS, ACTOR_EN_RD,     ACTOR_EN_FZ,            ACTOR_EN_VM,
                                    ACTOR_EN_RR,       ACTOR_EN_OKARINA_TAG, ACTOR_EN_OKARINA_EFFECT };
// The only scenery the Goron may carry: loose blocks and crates, never lifts, doors or floors.
static const s16 sLiftableScenery[] = { ACTOR_OBJ_OSHIHIKI, ACTOR_OBJ_KIBAKO2 };

static const Color_RGBA8 sSparklePrim = { 255, 255, 190, 255 };
static const Color_RGBA8 sSparkleEnv = { 255, 150, 0, 255 };

// While flipped the target carries this collider instead of its own: AT smashes what it falls on, like a
// thrown pot, and AC is what makes a downed foe hittable at all.
static ColliderCylinderInit sFlipColliderInit = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK0,
      { 0xFFCFFFFF, 0x00, 0x08 },
      { 0xFFCFFFFF, 0x00, 0x00 },
      ATELEM_ON | ATELEM_SFX_NORMAL,
      ACELEM_ON,
      OCELEM_NONE },
    { 30, 46, -12, { 0, 0, 0 } },
};

// Owned by Link, not by the thrown object: an actor's AT never hits its own AC, and this must hurt both.
static ColliderCylinderInit sBurstColliderInit = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK0,
      { 0xFFCFFFFF, 0x00, PACCI_BURST_MIN_DAMAGE },
      { 0x00000000, 0x00, 0x00 },
      ATELEM_ON | ATELEM_SFX_NORMAL,
      ACELEM_NONE,
      OCELEM_NONE },
    { 34, 40, -10, { 0, 0, 0 } },
};

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayDrawEnd", "OnSceneInit" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static PacciFlip sFlips[PACCI_POOL_SIZE];
static PacciLift sLift;
static PacciVfx sVfx;
static ColliderCylinder sBurst;
static bool sIsBurstReady;
static s16 sBurstTimer;
static u8 sPhase;
static s16 sCastTimer;
static s16 sHoldTimer;
static bool sIsTrackingPress;
static bool sIsWaitingForRelease;
static bool sIsPoseHeld;
static s16 sLastInvincibility;

extern PlayerAnimationHeader* ResourceMgr_LoadPlayerAnimAsHeader(const char* animPath);
void Player_StopHorizontalMovement(Player* player);

static void PlayErrorSfx(void) {
    Audio_PlaySfx(NA_SE_SY_ERROR);
}

static u32 FindEquippedButtonMask(void) {
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, PACCI_KEY) == 0) {
            return sItemButtons[button];
        }
    }
    return 0;
}

static bool IsIdOneOf(s16 id, const s16* ids, u32 count) {
    for (u32 i = 0; i < count; i++) {
        if (ids[i] == id) {
            return true;
        }
    }
    return false;
}

static PacciFlip* FindFlip(Actor* actor) {
    for (u8 i = 0; i < PACCI_POOL_SIZE; i++) {
        if (sFlips[i].actor == actor) {
            return &sFlips[i];
        }
    }
    return NULL;
}

// A foe killed while flipped leaves its entry behind: the effect outlives the cane, so reap before taking one.
static PacciFlip* TakeFlip(void) {
    for (u8 i = 0; i < PACCI_POOL_SIZE; i++) {
        if (sFlips[i].actor != NULL && sFlips[i].actor->update == NULL) {
            sFlips[i].actor = NULL;
        }
    }
    return FindFlip(NULL);
}

static void RestoreFlipped(PacciFlip* flip) {
    Actor* actor = flip->actor;

    if (actor != NULL && actor->update != NULL) {
        actor->update = flip->origUpdate;
        actor->flags = flip->origFlags;
        actor->gravity = flip->origGravity;
        actor->terminalVelocity = flip->origTerminalVelocity;
        actor->shape.rot = flip->origShapeRot;
        actor->world.rot = flip->origWorldRot;
        actor->room = flip->origRoom;
        actor->colChkInfo.mass = flip->origMass;
        actor->shape.yOffset = flip->origYOffset;
        actor->speed = 0.0f;
        actor->velocity.x = 0.0f;
        actor->velocity.y = 0.0f;
        actor->velocity.z = 0.0f;
    }
    flip->actor = NULL;
}

// Anything physical that is not Link, a boss or an NPC, and that the game actually draws.
static bool IsTargetable(Actor* actor) {
    if (actor == NULL || actor->update == NULL || actor->draw == NULL || actor->id == ACTOR_PLAYER ||
        actor->category == ACTORCAT_BOSS || actor->category == ACTORCAT_NPC) {
        return false;
    }
    if (actor->category == ACTORCAT_ENEMY && actor->colChkInfo.mass == MASS_IMMOVABLE) {
        return false;
    }
    return !IsIdOneOf(actor->id, sUntouchable, ARRAY_COUNT(sUntouchable)) && FindFlip(actor) == NULL &&
           actor != sLift.held;
}

static bool IsFlippable(Actor* actor) {
    return (actor->category == ACTORCAT_ENEMY || actor->category == ACTORCAT_PROP) && IsTargetable(actor);
}

static bool IsBigRock(Actor* actor) {
    return actor->id == ACTOR_EN_ISHI && ENISHI_GET_SIZE_FLAG(actor) != 0;
}

// The strength vanilla asks for the same weight: a big rock wants a Zora, a block a Goron.
static s32 RequiredStrength(Actor* actor) {
    if (actor->category == ACTORCAT_BG) {
        return PLAYER_STRENGTH_GORON;
    }
    if (actor->id == ACTOR_EN_ISHI) {
        return IsBigRock(actor) ? PLAYER_STRENGTH_ZORA : PLAYER_STRENGTH_HUMAN;
    }
    if (actor->category != ACTORCAT_ENEMY) {
        return PLAYER_STRENGTH_HUMAN;
    }
    if (actor->colChkInfo.mass <= PACCI_LIGHT_MASS) {
        return PLAYER_STRENGTH_HUMAN;
    }
    return actor->colChkInfo.mass <= PACCI_MEDIUM_MASS ? PLAYER_STRENGTH_ZORA : PLAYER_STRENGTH_GORON;
}

static bool IsLiftable(Actor* actor) {
    if (actor->category == ACTORCAT_BG && !IsIdOneOf(actor->id, sLiftableScenery, ARRAY_COUNT(sLiftableScenery))) {
        return false;
    }
    return (actor->category == ACTORCAT_ENEMY || actor->category == ACTORCAT_PROP ||
            actor->category == ACTORCAT_BG) &&
           IsTargetable(actor) && Player_GetStrength() >= RequiredStrength(actor);
}

// What Link is looking at wins over what is merely closer: the cone is measured from his facing.
static Actor* ScanAhead(PlayState* play, bool (*filter)(Actor*)) {
    static const u8 sCategories[] = { ACTORCAT_ENEMY, ACTORCAT_PROP, ACTORCAT_BG };
    Player* player = GET_PLAYER(play);
    Actor* best = NULL;
    s32 bestYawError = PACCI_SCAN_CONE;

    for (u32 i = 0; i < ARRAY_COUNT(sCategories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[sCategories[i]].first; actor != NULL; actor = actor->next) {
            if (actor->update == NULL || !filter(actor)) {
                continue;
            }
            f32 distance = Math_Vec3f_DistXZ(&player->actor.world.pos, &actor->world.pos);
            s32 yawError = ABS((s16)(Math_Vec3f_Yaw(&player->actor.world.pos, &actor->world.pos) -
                                     player->actor.shape.rot.y));

            if (distance > PACCI_SCAN_MIN_DIST && distance <= PACCI_SCAN_RANGE && yawError < bestYawError) {
                bestYawError = yawError;
                best = actor;
            }
        }
    }
    return best;
}

static void GetHandPos(Player* player, Vec3f* pos) {
    Vec3f forearm = player->bodyPartsPos[PLAYER_BODYPART_RIGHT_FOREARM];
    Vec3f hand = player->bodyPartsPos[PLAYER_BODYPART_RIGHT_HAND];
    f32 length = Math_Vec3f_DistXYZ(&forearm, &hand);

    *pos = hand;
    if (length > 0.001f) {
        pos->x += (hand.x - forearm.x) / length * 16.0f;
        pos->y += (hand.y - forearm.y) / length * 16.0f;
        pos->z += (hand.z - forearm.z) / length * 16.0f;
    }
}

static void GetVfxTargetPos(Vec3f* pos) {
    *pos = sVfx.target->world.pos;
    pos->y += sVfx.grabHeight;
}

// A quadratic arch from hand to target, bowed sideways and rippling with the frame counter.
static void GetCurvePoint(Vec3f* pos, Vec3f* start, Vec3f* end, f32 t, f32 sideOffset, u32 frame) {
    f32 inv = 1.0f - t;
    f32 dx = end->x - start->x;
    f32 dz = end->z - start->z;
    f32 xzLength = sqrtf(SQ(dx) + SQ(dz));
    f32 sideX = xzLength > 0.001f ? dz / xzLength : 1.0f;
    f32 sideZ = xzLength > 0.001f ? -dx / xzLength : 0.0f;
    s16 archAngle = (s16)(t * 0x7FFF);
    s16 waveAngle = (s16)((frame * 0x1200) + (s32)(t * 0x6000));
    Vec3f control = { (start->x + end->x) * 0.5f, (start->y + end->y) * 0.5f + 45.0f, (start->z + end->z) * 0.5f };

    pos->x = inv * inv * start->x + 2.0f * inv * t * control.x + t * t * end->x;
    pos->y = inv * inv * start->y + 2.0f * inv * t * control.y + t * t * end->y;
    pos->z = inv * inv * start->z + 2.0f * inv * t * control.z + t * t * end->z;
    sideOffset *= Math_SinS(archAngle);
    sideOffset += Math_SinS(waveAngle) * 2.5f * Math_SinS(archAngle);
    pos->x += sideX * sideOffset;
    pos->z += sideZ * sideOffset;
}

static void SpawnSparkles(PlayState* play, Vec3f* pos, f32 spread, u8 count, s16 life) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    for (u8 i = 0; i < count; i++) {
        Vec3f spark = *pos;

        spark.x += Rand_CenteredFloat(spread);
        spark.y += Rand_CenteredFloat(spread);
        spark.z += Rand_CenteredFloat(spread);
        EffectSsKirakira_SpawnDispersed(play, &spark, &zero, &zero, (Color_RGBA8*)&sSparklePrim,
                                        (Color_RGBA8*)&sSparkleEnv, 700, life);
    }
}

static void StopVfx(void) {
    memset(&sVfx, 0, sizeof(sVfx));
}

static void StartVfx(PlayState* play, Player* player, Actor* target, bool isPersistent) {
    Vec3f hand;
    Vec3f targetPos;
    f32 focusHeight = target->focus.pos.y - target->world.pos.y;

    StopVfx();
    sVfx.target = target;
    sVfx.isPersistent = isPersistent;
    sVfx.startRotZ = target->shape.rot.z;
    sVfx.grabHeight = focusHeight >= 10.0f && focusHeight <= 120.0f ? focusHeight : 30.0f;
    GetHandPos(player, &hand);
    GetVfxTargetPos(&targetPos);
    sVfx.lastTargetPos = targetPos;
    sVfx.hasLastTargetPos = true;
    SpawnSparkles(play, &hand, 12.0f, 8, 16);
    SpawnSparkles(play, &targetPos, 42.0f, 20, 18);
}

static void ReleaseVfx(void) {
    if (sVfx.target != NULL && !sVfx.isReleasing) {
        sVfx.isPersistent = false;
        sVfx.isReleasing = true;
        sVfx.releaseTimer = PACCI_VFX_RELEASE_FRAMES;
    }
}

static void UpdateVfx(PlayState* play, Player* player) {
    Vec3f hand;
    Vec3f target;

    if (sVfx.target == NULL) {
        return;
    }
    if (sVfx.target->update == NULL) {
        StopVfx();
        return;
    }
    sVfx.age++;
    if (!sVfx.isPersistent && !sVfx.isReleasing &&
        (sVfx.age >= PACCI_VFX_MAX_FRAMES ||
         (sVfx.age > PACCI_VFX_REACH_FRAMES && (sVfx.target->bgCheckFlags & BGCHECKFLAG_GROUND)))) {
        sVfx.isReleasing = true;
        sVfx.releaseTimer = PACCI_VFX_RELEASE_FRAMES;
        GetVfxTargetPos(&target);
        SpawnSparkles(play, &target, 28.0f, 10, 14);
    }
    if (sVfx.isReleasing) {
        if (sVfx.releaseTimer <= 0) {
            StopVfx();
            return;
        }
        sVfx.releaseTimer--;
    }
    GetHandPos(player, &hand);
    GetVfxTargetPos(&target);
    if (sVfx.hasLastTargetPos) {
        Vec3f trail = { (sVfx.lastTargetPos.x + target.x) * 0.5f, (sVfx.lastTargetPos.y + target.y) * 0.5f,
                        (sVfx.lastTargetPos.z + target.z) * 0.5f };

        SpawnSparkles(play, &trail, 8.0f, 1, 10);
    }
    sVfx.lastTargetPos = target;
    sVfx.hasLastTargetPos = true;
    if ((sVfx.age & 1) == 0) {
        Vec3f trail;

        GetCurvePoint(&trail, &hand, &target, Rand_ZeroOne(), Rand_CenteredFloat(8.0f), play->gameplayFrames);
        SpawnSparkles(play, &trail, 4.0f, 1, 8);
        SpawnSparkles(play, &hand, 5.0f, 1, 8);
        SpawnSparkles(play, &target, 12.0f, 1, 10);
    }
}

static void DrawSprite(PlayState* play, Vec3f* pos, f32 scale, const char* dList) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)dList);
    CLOSE_DISPS(play->state.gfxCtx);
}

// The Great Fairy's spiral beam is 8000 units long and 1200 wide in its own space.
static void DrawBeamSegment(PlayState* play, Vec3f* start, Vec3f* end, f32 radius, Color_RGB8 color, u8 alpha) {
    f32 dy = end->y - start->y;
    f32 xzLength = Math_Vec3f_DistXZ(start, end);
    f32 length = Math_Vec3f_DistXYZ(start, end);

    if (length < 0.5f) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(start->x, start->y, start->z, MTXMODE_NEW);
    Matrix_RotateYS(Math_Vec3f_Yaw(start, end), MTXMODE_APPLY);
    Matrix_RotateXF(atan2f(xzLength, dy), MTXMODE_APPLY);
    Matrix_Scale(radius / 1200.0f, length / 8000.0f, radius / 1200.0f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, color.r, color.g, color.b, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, color.r / 3, color.g / 3, color.b / 3, alpha);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBeamDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawBeamPass(PlayState* play, Vec3f* hand, Vec3f* target, f32 reach, f32 width, f32 sideBias,
                         Color_RGB8 color, u8 alpha) {
    Vec3f centers[PACCI_VFX_RIBBON_POINTS];

    if (sVfx.isPersistent) {
        Vec3f tip = { hand->x + (target->x - hand->x) * reach, hand->y + (target->y - hand->y) * reach,
                      hand->z + (target->z - hand->z) * reach };

        DrawBeamSegment(play, hand, &tip, width, color, alpha);
        return;
    }
    for (u8 i = 0; i < PACCI_VFX_RIBBON_POINTS; i++) {
        f32 t = (f32)i / (PACCI_VFX_RIBBON_POINTS - 1) * reach;

        GetCurvePoint(&centers[i], hand, target, t, sideBias, play->gameplayFrames);
    }
    for (u8 i = 0; i < PACCI_VFX_RIBBON_POINTS - 1; i++) {
        f32 edge = Math_SinS((s16)((i * 0x7FFF) / (PACCI_VFX_RIBBON_POINTS - 2)));

        DrawBeamSegment(play, &centers[i], &centers[i + 1], width * (0.65f + edge * 0.35f), color, alpha);
    }
}

static void DrawGrip(PlayState* play, Vec3f* hand, Vec3f* target, f32 reach, f32 pulse) {
    f32 xzLength = Math_Vec3f_DistXZ(hand, target);
    f32 sideX = xzLength > 0.001f ? (target->z - hand->z) / xzLength : 1.0f;
    f32 sideZ = xzLength > 0.001f ? -(target->x - hand->x) / xzLength : 0.0f;
    f32 gripRadius = 42.0f - 18.0f * reach;

    for (u8 i = 0; i < PACCI_VFX_GRIP_POINTS; i++) {
        s16 angle = (s16)(i * (0x10000 / PACCI_VFX_GRIP_POINTS) + (sVfx.target->shape.rot.z - sVfx.startRotZ));
        f32 side = Math_CosS(angle) * gripRadius;
        Vec3f point = { target->x + sideX * side, target->y + Math_SinS(angle) * gripRadius,
                        target->z + sideZ * side };

        DrawSprite(play, &point, 0.014f * pulse, sSparklesDL);
    }
}

// The cast visual: a beam from the cane hand to the target, sparkles along it and a ring gripping the target.
static void DrawVfx(PlayState* play, Player* player) {
    Vec3f hand;
    Vec3f target;
    f32 reach = sVfx.age < PACCI_VFX_REACH_FRAMES ? (f32)sVfx.age / PACCI_VFX_REACH_FRAMES : 1.0f;
    u8 alpha = sVfx.isReleasing ? (u8)(255 * sVfx.releaseTimer / PACCI_VFX_RELEASE_FRAMES) : 255;
    f32 pulse = 1.0f + Math_SinS((s16)(play->gameplayFrames * 0x1000)) * 0.18f;

    GetHandPos(player, &hand);
    GetVfxTargetPos(&target);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, play->gameplayFrames * 2, 0, 0x20, 0x40, 1,
                                             play->gameplayFrames, play->gameplayFrames * -8, 0x10, 0x10, 2, 0, 1,
                                             -8));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 190, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 150, 0, alpha);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawSprite(play, &hand, 0.030f * pulse, sFlash1DL);
    DrawSprite(play, &hand, 0.016f, sSparklesDL);
    DrawBeamPass(play, &hand, &target, reach, 8.0f * pulse, 0.0f, (Color_RGB8){ 25, 210, 255 }, (u8)(alpha * 0.78f));
    DrawBeamPass(play, &hand, &target, reach, 3.8f * pulse, 2.5f, (Color_RGB8){ 255, 190, 25 }, alpha);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 190, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 150, 0, alpha);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawGrip(play, &hand, &target, reach, pulse);

    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetCombineLERP(POLY_XLU_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 30, 225, 255, alpha);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawSprite(play, &target, 0.065f * pulse, sFlareRingDL);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 220, 80, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 120, 0, alpha);
    CLOSE_DISPS(play->state.gfxCtx);
    DrawSprite(play, &target, 0.034f * pulse, sFlash2DL);
}

// Impact burst: the thrown object and whatever it hit take a real hit, with their own flinch, death and drop.
static void SpawnBurst(PlayState* play, Player* player, Vec3f* pos, bool isHeavy) {
    s32 strength = Player_GetStrength();

    if (!sIsBurstReady) {
        Collider_InitCylinder(play, &sBurst);
        sIsBurstReady = true;
    }
    Collider_SetCylinder(play, &sBurst, &player->actor, &sBurstColliderInit);
    // Rocks only break to a pound, and the Goron throws hard enough to deliver one.
    sBurst.elem.atDmgInfo.dmgFlags =
        isHeavy || strength >= PLAYER_STRENGTH_GORON ? (DMG_GORON_POUND | DMG_THROWN_OBJECT) : DMG_THROWN_OBJECT;
    sBurst.elem.atDmgInfo.damage = (u8)MAX(PACCI_BURST_MIN_DAMAGE, strength * PACCI_BURST_DAMAGE_PER_STRENGTH);
    sBurst.dim.pos.x = (s16)pos->x;
    sBurst.dim.pos.y = (s16)pos->y;
    sBurst.dim.pos.z = (s16)pos->z;
    sBurstTimer = PACCI_BURST_FRAMES;
}

static void UpdateBurst(PlayState* play) {
    if (sBurstTimer <= 0 || !sIsBurstReady) {
        return;
    }
    sBurstTimer--;
    CollisionCheck_SetAT(play, &play->colChkCtx, &sBurst.base);
}

static void RollTumble(PacciFlip* flip) {
    flip->tumbleTargetX = (s16)((Rand_ZeroOne() - 0.7f) * 2800.0f);
    flip->tumbleTargetY = (s16)((Rand_ZeroOne() - 0.5f) * 2000.0f);
    flip->tumbleX = 0;
    flip->tumbleY = 0;
}

static void FallStep(PacciFlip* flip, Actor* actor, PlayState* play) {
    actor->velocity.y = MAX(actor->velocity.y + actor->gravity, actor->terminalVelocity);
    Actor_UpdatePos(actor);
    Math_StepToS(&flip->tumbleX, flip->tumbleTargetX, PACCI_FLIP_TUMBLE_STEP);
    Math_StepToS(&flip->tumbleY, flip->tumbleTargetY, PACCI_FLIP_TUMBLE_STEP);
    actor->shape.rot.x += flip->tumbleX;
    actor->shape.rot.y += flip->tumbleY;
    Actor_UpdateBgCheckInfo(play, actor, 5.0f, 15.0f, 0.0f, PACCI_BG_CHECK_FLAGS);
}

// AC and OC stay live the whole time so a downed foe can be hit; AT only while it falls.
static void SubmitFlipColliders(PacciFlip* flip, Actor* actor, PlayState* play, bool isAirborne) {
    Collider_UpdateCylinder(actor, &flip->collider);
    if (isAirborne) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &flip->collider.base);
    }
    CollisionCheck_SetAC(play, &play->colChkCtx, &flip->collider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &flip->collider.base);
}

// A hit on a downed foe comes off its health without waking it; only the killing blow gives it back, with
// colChkInfo untouched, so its own update runs its own death.
static bool TakeDownedHit(PacciFlip* flip, Actor* actor) {
    if (!(flip->collider.base.acFlags & AC_HIT)) {
        return false;
    }
    flip->collider.base.acFlags &= ~AC_HIT;
    if (actor->colChkInfo.damage == 0) {
        return false;
    }
    if (actor->colChkInfo.health <= actor->colChkInfo.damage) {
        actor->colChkInfo.health = 0;
        RestoreFlipped(flip);
        return true;
    }
    actor->colChkInfo.health -= actor->colChkInfo.damage;
    actor->colChkInfo.damage = 0;
    Actor_SetColorFilter(actor, COLORFILTER_COLORFLAG_GRAY, 255, 0, PACCI_HURT_TINT_FRAMES);
    Actor_PlaySfx(actor, NA_SE_EN_STAL_DAMAGE);
    return false;
}

static void UpdateAirborne(PacciFlip* flip, Actor* actor, PlayState* play) {
    Math_SmoothStepToS(&actor->shape.rot.z, 0x7FFF, 1, PACCI_FLIP_ROT_STEP, 0);
    FallStep(flip, actor, play);
    SubmitFlipColliders(flip, actor, play, true);
    if (!(actor->bgCheckFlags & BGCHECKFLAG_GROUND)) {
        actor->shape.yOffset = MIN(actor->shape.yOffset + PACCI_FLIP_YOFFSET_STEP, PACCI_FLIP_YOFFSET_MAX);
        return;
    }
    if (actor->bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
        Actor_SpawnFloorDustRing(play, actor, &actor->world.pos, 20.0f, 11, 4.0f, 0, 0, false);
        Actor_PlaySfx(actor, NA_SE_EN_STAL_DAMAGE);
    }
    // A flipped prop lands like a dropped pot and takes the impact; only foes stay down, helpless.
    if (actor->category != ACTORCAT_ENEMY) {
        Vec3f impact = actor->world.pos;
        bool isHeavy = IsBigRock(actor);

        RestoreFlipped(flip);
        SpawnBurst(play, GET_PLAYER(play), &impact, isHeavy);
        return;
    }
    flip->phase = PACCI_FLIP_DOWNED;
    flip->timer = PACCI_FLIP_ON_BACK_FRAMES;
    actor->speed = 0.0f;
}

static void UpdateDowned(PacciFlip* flip, Actor* actor, PlayState* play) {
    Math_SmoothStepToS(&actor->shape.rot.z, 0x7FFF, 1, PACCI_FLIP_ROT_STEP, 0);
    Math_StepToF(&actor->speed, 0.0f, 1.0f);
    Actor_MoveWithGravity(actor);
    Actor_UpdateBgCheckInfo(play, actor, 5.0f, 15.0f, 0.0f, PACCI_BG_CHECK_FLAGS);
    SubmitFlipColliders(flip, actor, play, false);
    if (flip->timer > 0) {
        flip->timer--;
        return;
    }
    flip->phase = PACCI_FLIP_RIGHTING;
    actor->velocity.y = PACCI_FLIP_RIGHT_VEL_Y;
    Actor_PlaySfx(actor, NA_SE_EN_STAL_DAMAGE);
}

static void UpdateRighting(PacciFlip* flip, Actor* actor, PlayState* play) {
    Math_SmoothStepToS(&actor->shape.rot.z, flip->origShapeRot.z, 1, PACCI_FLIP_RIGHT_ROT_STEP, 0);
    Actor_MoveWithGravity(actor);
    Actor_UpdateBgCheckInfo(play, actor, 5.0f, 15.0f, 0.0f, PACCI_BG_CHECK_FLAGS);
    SubmitFlipColliders(flip, actor, play, false);
    if (actor->bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
        Actor_PlaySfx(actor, NA_SE_EN_STAL_DAMAGE);
        actor->world.pos.y = actor->floorHeight;
        RestoreFlipped(flip);
    }
}

// The flipped actor's update: its own AI does not run at all while this one stands in for it.
static void FlippedUpdate(Actor* actor, PlayState* play) {
    PacciFlip* flip = FindFlip(actor);

    if (flip == NULL || TakeDownedHit(flip, actor)) {
        return;
    }
    switch (flip->phase) {
        case PACCI_FLIP_AIRBORNE:
            UpdateAirborne(flip, actor, play);
            break;
        case PACCI_FLIP_DOWNED:
            UpdateDowned(flip, actor, play);
            break;
        case PACCI_FLIP_RIGHTING:
            UpdateRighting(flip, actor, play);
            break;
        default:
            RestoreFlipped(flip);
            return;
    }
    actor->focus.pos = actor->world.pos;
}

static bool CastFlip(PlayState* play, Player* player) {
    Actor* target = ScanAhead(play, IsFlippable);
    PacciFlip* flip = target == NULL ? NULL : TakeFlip();

    if (flip == NULL) {
        return false;
    }
    flip->actor = target;
    flip->phase = PACCI_FLIP_AIRBORNE;
    flip->timer = 0;
    flip->origUpdate = target->update;
    flip->origFlags = target->flags;
    flip->origGravity = target->gravity;
    flip->origTerminalVelocity = target->terminalVelocity;
    flip->origShapeRot = target->shape.rot;
    flip->origWorldRot = target->world.rot;
    flip->origRoom = target->room;
    flip->origMass = target->colChkInfo.mass;
    flip->origYOffset = target->shape.yOffset;
    RollTumble(flip);
    if (!flip->isColliderReady) {
        Collider_InitCylinder(play, &flip->collider);
        flip->isColliderReady = true;
    }
    Collider_SetCylinder(play, &flip->collider, target, &sFlipColliderInit);
    target->update = FlippedUpdate;
    target->gravity = PACCI_FLIP_GRAVITY;
    target->terminalVelocity = PACCI_FLIP_MIN_VEL_Y;
    target->velocity.y = PACCI_FLIP_LAUNCH_VEL_Y;
    target->speed = 0.0f;
    target->colChkInfo.mass = PACCI_FLIP_THROWN_MASS;
    target->bgCheckFlags &= ~(BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH);
    // Culling would stop this update too and freeze the foe mid-air; the flags go back with origFlags.
    target->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    Actor_PlaySfx(target, NA_SE_EN_STAL_DAMAGE);
    StartVfx(play, player, target, false);
    return true;
}

// A held foe's update: the cane owns its movement and collision, and a live no-op keeps it off the kill list.
static void HeldEnemyUpdate(Actor* actor, PlayState* play) {
}

static void LetGo(void) {
    Actor* actor = sLift.held;

    if (actor != NULL && actor->update != NULL) {
        if (sLift.frozeEnemy) {
            actor->update = sLift.origUpdate;
            actor->flags = sLift.origFlags;
        }
        actor->gravity = sLift.origGravity;
        actor->terminalVelocity = sLift.origTerminalVelocity;
        actor->room = sLift.origRoom;
        actor->colorFilterTimer = 0;
        // Heavy enough to throw is heavy enough to hold down a floor switch.
        actor->flags |= ACTOR_FLAG_CAN_PRESS_SWITCHES;
    }
    sLift.held = NULL;
    sLift.isThrown = false;
    sLift.frozeEnemy = false;
    sLift.flightTimer = 0;
}

static void CancelLift(void) {
    ReleaseVfx();
    LetGo();
}

static bool TryGrab(PlayState* play, Player* player) {
    Actor* target = ScanAhead(play, IsLiftable);

    if (target == NULL) {
        return false;
    }
    if (!sLift.isColliderReady) {
        Collider_InitCylinder(play, &sLift.collider);
        sLift.isColliderReady = true;
    }
    Collider_SetCylinder(play, &sLift.collider, target, &sFlipColliderInit);
    sLift.held = target;
    sLift.isThrown = false;
    sLift.flightTimer = 0;
    sLift.origGravity = target->gravity;
    sLift.origTerminalVelocity = target->terminalVelocity;
    sLift.origRoom = target->room;
    sLift.frozeEnemy = target->category == ACTORCAT_ENEMY;
    if (sLift.frozeEnemy) {
        sLift.origUpdate = target->update;
        sLift.origFlags = target->flags;
        target->update = HeldEnemyUpdate;
        target->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    }
    // A held object must survive the room it was picked up in unloading.
    target->room = -1;
    Actor_PlaySfx(target, NA_SE_EN_STAL_DAMAGE);
    StartVfx(play, player, target, true);
    return true;
}

// Aimed at Link's lock-on target when he has one, straight ahead otherwise; the stronger he is, the faster.
static void Throw(Player* player) {
    Actor* actor = sLift.held;
    Actor* focus = player->focusActor;
    s16 yaw;

    if (actor == NULL || sLift.isThrown) {
        return;
    }
    if (actor->update == NULL) {
        LetGo();
        return;
    }
    yaw = focus != NULL && focus != actor && focus->update != NULL ? Math_Vec3f_Yaw(&actor->world.pos, &focus->world.pos)
                                                                   : player->actor.shape.rot.y;
    actor->world.rot.y = yaw;
    actor->shape.rot.y = yaw;
    actor->speed = PACCI_LIFT_THROW_BASE_SPEED + PACCI_LIFT_THROW_SPEED_PER_STRENGTH * Player_GetStrength();
    actor->velocity.y = PACCI_LIFT_THROW_VEL_Y;
    actor->gravity = PACCI_LIFT_GRAVITY;
    actor->colorFilterTimer = 0;
    sLift.isThrown = true;
    sLift.flightTimer = PACCI_LIFT_FLIGHT_FRAMES;
    ReleaseVfx();
    Actor_PlaySfx(actor, NA_SE_EN_STAL_DAMAGE);
}

static void HoldAloft(Player* player, Actor* actor) {
    f32 targetX = player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * PACCI_LIFT_DIST;
    f32 targetY = player->actor.world.pos.y + PACCI_LIFT_HEIGHT;
    f32 targetZ = player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * PACCI_LIFT_DIST;

    actor->world.pos.x = actor->world.pos.x * PACCI_LIFT_FOLLOW + targetX * (1.0f - PACCI_LIFT_FOLLOW);
    actor->world.pos.y = actor->world.pos.y * PACCI_LIFT_FOLLOW + targetY * (1.0f - PACCI_LIFT_FOLLOW);
    actor->world.pos.z = actor->world.pos.z * PACCI_LIFT_FOLLOW + targetZ * (1.0f - PACCI_LIFT_FOLLOW);
    actor->velocity.x = 0.0f;
    actor->velocity.y = 0.0f;
    actor->velocity.z = 0.0f;
    actor->speed = 0.0f;
    actor->gravity = 0.0f;
    actor->shape.rot.y += PACCI_LIFT_SPIN;
    Actor_SetColorFilter(actor, COLORFILTER_COLORFLAG_RED, 255, 0, 8);
}

// Landed, hit a wall, hit something, or flew too long: both ends of the impact take a real hit.
static void FlyThrown(PlayState* play, Player* player, Actor* actor) {
    Actor_MoveWithGravity(actor);
    Actor_UpdateBgCheckInfo(play, actor, 5.0f, 15.0f, 0.0f, PACCI_BG_CHECK_FLAGS);
    Collider_UpdateCylinder(actor, &sLift.collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &sLift.collider.base);
    if (sLift.flightTimer > 0) {
        sLift.flightTimer--;
    }
    if (!(actor->bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_WALL)) && sLift.flightTimer > 0 &&
        !(sLift.collider.base.atFlags & AT_HIT)) {
        return;
    }
    Vec3f impact = actor->world.pos;
    bool isHeavy = IsBigRock(actor);

    Actor_PlaySfx(actor, NA_SE_EN_STAL_DAMAGE);
    LetGo();
    SpawnBurst(play, player, &impact, isHeavy);
}

static void UpdateLift(PlayState* play, Player* player) {
    Actor* actor = sLift.held;

    if (actor == NULL) {
        return;
    }
    if (actor->update == NULL) {
        LetGo();
        return;
    }
    if (sLift.isThrown) {
        FlyThrown(play, player, actor);
    } else {
        HoldAloft(player, actor);
    }
}

static void StartUpperAnim(PlayState* play, Player* player, PlayerAnimationHeader* anim, f32 speed) {
    if (anim != NULL) {
        PlayerAnimation_PlayOnceSetSpeed(play, &player->skelAnimeUpper, anim, speed);
    }
}

static void StartCast(PlayState* play, Player* player) {
    sPhase = PACCI_CASTING;
    sCastTimer = 0;
    sIsPoseHeld = false;
    StartUpperAnim(play, player, ResourceMgr_LoadPlayerAnimAsHeader(sCastAnim), PACCI_CAST_SPEED);
}

static void StartThrowSwing(PlayState* play, Player* player) {
    sPhase = PACCI_SWINGING;
    sCastTimer = 0;
    sIsPoseHeld = false;
    StartUpperAnim(play, player, (PlayerAnimationHeader*)sThrowAnim, 1.0f);
}

static bool CanCast(Player* player) {
    return !(player->stateFlags1 & PLAYER_STATE1_8000000) && player->meleeWeaponState == PLAYER_MELEE_WEAPON_STATE_0;
}

static void OnRelease(PlayState* play, Player* player) {
    s16 held = sHoldTimer;

    sHoldTimer = 0;
    sIsTrackingPress = false;
    if (held >= PACCI_HOLD_FRAMES) {
        Throw(player);
        StartThrowSwing(play, player);
    } else if (CanCast(player)) {
        StartCast(play, player);
    } else {
        PlayErrorSfx();
    }
}

// Only strength can lift: as a Deku the hold does nothing, and anything too heavy is refused.
static void OnHoldReached(PlayState* play, Player* player) {
    if (Player_GetStrength() == PLAYER_STRENGTH_DEKU || !TryGrab(play, player)) {
        PlayErrorSfx();
    }
}

static void TrackButton(PlayState* play, Player* player) {
    u32 button = FindEquippedButtonMask();
    Input* input = &play->state.input[0];
    bool isHeld = (input->cur.button & button) != 0;

    if (sIsWaitingForRelease) {
        sIsWaitingForRelease = isHeld;
        return;
    }
    if (!sIsTrackingPress) {
        sIsTrackingPress = (input->press.button & button) != 0 && sPhase == PACCI_READY;
        return;
    }
    if (!isHeld) {
        OnRelease(play, player);
        return;
    }
    if (sHoldTimer <= PACCI_HOLD_FRAMES) {
        sHoldTimer++;
    }
    if (sHoldTimer == PACCI_HOLD_FRAMES) {
        OnHoldReached(play, player);
    }
}

static void HighlightTarget(PlayState* play) {
    Actor* target;

    if (sLift.held != NULL || sPhase != PACCI_READY) {
        return;
    }
    target = ScanAhead(play, sHoldTimer > 0 ? IsLiftable : IsFlippable);
    if (target != NULL) {
        Actor_SetColorFilter(target, COLORFILTER_COLORFLAG_RED, 255, 0, PACCI_TINT_FRAMES);
    }
}

static bool WasJustHurt(Player* player) {
    bool isHurt = player->invincibilityTimer > 0 && sLastInvincibility <= 0;

    sLastInvincibility = player->invincibilityTimer;
    return isHurt;
}

// The raised pose belongs to the hold: frame 0 of the wait animation, frozen, with the legs left alone.
static void HoldRaisedPose(PlayState* play, Player* player) {
    if (!sIsPoseHeld) {
        StartUpperAnim(play, player, (PlayerAnimationHeader*)sRaisedAnim, 0.0f);
        sIsPoseHeld = true;
    }
    player->skelAnimeUpper.curFrame = 0.0f;
    PlayerAnimation_Update(play, &player->skelAnimeUpper);
}

// The return value is Link's "upper body is busy": only while the cane is actually swinging or raised.
static int32_t UpdateCane(Player* player, PlayState* play) {
    if (sPhase == PACCI_CASTING || sPhase == PACCI_SWINGING) {
        Player_StopHorizontalMovement(player);
        sCastTimer++;
        if (sPhase == PACCI_CASTING && sCastTimer == PACCI_CAST_FIRE_FRAME && !CastFlip(play, player)) {
            PlayErrorSfx();
        }
        if (PlayerAnimation_Update(play, &player->skelAnimeUpper) || sCastTimer > PACCI_CAST_TIMEOUT) {
            sPhase = PACCI_READY;
        }
        return 1;
    }
    if (sPhase == PACCI_READY && sHoldTimer >= PACCI_HOLD_FRAMES) {
        HoldRaisedPose(play, player);
        return 1;
    }
    sIsPoseHeld = false;
    return 0;
}

static void TakeOutCane(PlayState* play, Player* player) {
    sPhase = PACCI_READY;
    sHoldTimer = 0;
    sIsTrackingPress = false;
    sIsPoseHeld = false;
    // The press that drew the cane is not a cast.
    sIsWaitingForRelease = true;
    sLastInvincibility = player->invincibilityTimer;
}

// Tap and hold are told apart from the raw button in TrackButton; the press itself only has to be consumed.
static void PressCane(Player* player, PlayState* play) {
}

// Flipped foes keep their state on purpose: knock one over, switch to the sword, hit it while it is down.
static void PutCaneAway(Player* player, PlayState* play) {
    sPhase = PACCI_STOWED;
    sHoldTimer = 0;
    sIsTrackingPress = false;
    sIsPoseHeld = false;
    if (sLift.held != NULL && !sLift.isThrown) {
        CancelLift();
    }
}

static void UpdatePacciWorld(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL) {
        return;
    }
    UpdateBurst(play);
    UpdateLift(play, player);
    UpdateVfx(play, player);
    if (sPhase == PACCI_STOWED) {
        return;
    }
    if (WasJustHurt(player)) {
        sHoldTimer = 0;
        sIsTrackingPress = false;
        if (sLift.held != NULL && !sLift.isThrown) {
            CancelLift();
        }
        return;
    }
    HighlightTarget(play);
    TrackButton(play, player);
}

static void DrawPacciEffects(void) {
    PlayState* play = gPlayState;

    if (play != NULL && sVfx.target != NULL && sVfx.target->update != NULL) {
        DrawVfx(play, GET_PLAYER(play));
    }
}

// Every actor the cane was holding or flipping belongs to the scene that is ending.
static void ForgetScene(s16 sceneId, s8 spawnNum) {
    for (u8 i = 0; i < PACCI_POOL_SIZE; i++) {
        sFlips[i].actor = NULL;
    }
    sLift.held = NULL;
    sLift.isThrown = false;
    sBurstTimer = 0;
    StopVfx();
    sPhase = PACCI_STOWED;
    sHoldTimer = 0;
    sIsTrackingPress = false;
}

// Shuffled once by the randomizer; no vanilla logic needs it.
static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, PACCI_KEY, PACCI_KEY, "",
};

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, GetEquippedCustomItem)) {
        return;
    }

    S2HCustomItemDefinition cane = Z64Items_Define(PACCI_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&cane, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    // Shares the Cane of Somaria's cell, after it.
    Z64Items_SetPlacement(&cane, 0, 21, -1);
    Z64Items_SetTextbox(&cane, "You got the %yCane of Pacci%w!&Its magic %yflips%w what it touches&onto its back.^"
                               "Hold its %yC button%w to %ylift%w it instead&and let go to throw it. How much it "
                               "can&lift depends on your %rstrength%w.");
    Z64Items_SetPauseText(&cane, "%yCane of Pacci&%wTap its %yC button%w to flip, hold to lift&and release to throw "
                                 "at your %gZ%w target.");
    Z64Items_SetAction(&cane, TakeOutCane, UpdateCane);
    Z64Items_SetHeldCallbacks(&cane, PressCane, PutCaneAway, NULL);
    Z64Items_SetHeldModel(&cane, sCaneDL, PLAYER_MODELGROUP_DEFAULT);
    Z64Items_SetGetItemModel(&cane, sCaneGiveDL);

    Z64Items_SetLogic(&cane, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &cane)) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdatePacciWorld);
    S2H_REGISTER_HOOK(sApi, OnPlayDrawEnd, DrawPacciEffects);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetScene);
}
