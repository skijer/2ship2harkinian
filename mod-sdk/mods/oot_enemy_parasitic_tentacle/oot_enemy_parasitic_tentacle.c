/*
 * Ocarina of Time's Parasitic Tentacle (En_Ba, Jabu-Jabu) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * It hangs from above and lashes at the player; only a boomerang hurts it. Standalone: no switch flag.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"

#define TENTACLE_KEY "oot.parasitic_tentacle"
#define TENTACLE_BLOB_KEY "oot.parasitic_tentacle.blob"
#define TENTACLE_SOUL_KEY "soul.oot.parasitic_tentacle"
#define BXA_OBJECT "objects/oot_object_bxa/"
#define BXA_ASSET(name) "__OTR__" BXA_OBJECT name

static const ALIGN_ASSET(2) char sTentacleDL[] = BXA_ASSET("object_bxa_DL_000890");
static const ALIGN_ASSET(2) char sBlobDL[] = BXA_ASSET("object_bxa_DL_001D80");
static const ALIGN_ASSET(2) char sRedTex[] = BXA_ASSET("object_bxa_Tex_0024F0");
static const ALIGN_ASSET(2) char sGreenTex[] = BXA_ASSET("object_bxa_Tex_0027F0");
static const ALIGN_ASSET(2) char sGrayTex[] = BXA_ASSET("object_bxa_Tex_0029F0");

// MM reuses every Barinade tentacle sfx id; these are stand-ins from MM's Dexihand and Like Like.
#define TENTACLE_SFX_RAISE NA_SE_EN_HANDW_GET
#define TENTACLE_SFX_LASH NA_SE_EN_HANDW_RELEASE
#define TENTACLE_SFX_DAMAGE NA_SE_EN_LIKE_DAMAGE
#define TENTACLE_SFX_DEAD NA_SE_EN_LIKE_DEAD

#define TENTACLE_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED)
#define TENTACLE_SEGMENTS 14
#define TENTACLE_LAST_SEGMENT (TENTACLE_SEGMENTS - 1)
#define TENTACLE_HEALTH 4
#define TENTACLE_REACH 175.0f
#define TENTACLE_LOW_HEIGHT 100.0f
#define TENTACLE_HIGH_HEIGHT 330.0f
#define TENTACLE_ROOT_DEPTH 448.0f
#define TENTACLE_SEGMENT_SCALE 0.01f
// OoT's PLAYER_STATE1_DAMAGED is MM's unnamed bit 26.
#define TENTACLE_PLAYER_STATE1_DAMAGED PLAYER_STATE1_4000000

typedef enum {
    TENTACLE_COLOR_RED,
    TENTACLE_COLOR_GREEN,
    TENTACLE_COLOR_GRAY,
    TENTACLE_COLOR_MAX,
} TentacleColor;

// States from TENTACLE_STATE_IDLE up take hits.
typedef enum {
    TENTACLE_STATE_DEAD,
    TENTACLE_STATE_RECOIL,
    TENTACLE_STATE_IDLE,
    TENTACLE_STATE_SWING,
} TentacleState;

static const S2HModApi* sApi;
static s16 sBlobId = -1;

struct EnBa;
typedef void (*EnBaActionFunc)(struct EnBa*, PlayState*);

typedef struct EnBa {
    Actor actor;
    s32 state;
    EnBaActionFunc actionFunc;
    Vec3f segmentPos[TENTACLE_SEGMENTS];
    Vec3s segmentRot[TENTACLE_SEGMENTS];
    Vec3f rootTarget;
    Vec3f swayOffset;
    f32 swayPhaseX;
    f32 swayPhaseZ;
    s16 timer;
    s16 lashTimer;
    s16 rotStep;
    u32 epoch;
    ColliderJntSph collider;
    ColliderJntSphElement colliderItems[2];
} EnBa;

static Vec3f sSegmentOffset = { 0.0f, 0.0f, 32.0f };
static Vec3f sZeroOffset = { 0.0f, 0.0f, 0.0f };

static ColliderJntSphElementInit sJntSphElementInit[2] = {
    {
        {
            ELEM_MATERIAL_UNK0,
            { 0x00000000, 0x00, 0x00 },
            { DMG_ZORA_BOOMERANG, 0x00, 0x00 },
            ATELEM_NONE,
            ACELEM_ON,
            OCELEM_NONE,
        },
        { 8, { { 0, 0, 0 }, 20 }, 100 },
    },
    {
        {
            ELEM_MATERIAL_UNK0,
            { DMG_UNBLOCKABLE, 0x00, 0x04 },
            { 0x00000000, 0x00, 0x00 },
            ATELEM_ON | ATELEM_SFX_NORMAL,
            ACELEM_NONE,
            OCELEM_NONE,
        },
        { 13, { { 0, 0, 0 }, 25 }, 100 },
    },
};

static ColliderJntSphInit sJntSphInit = {
    {
        COL_MATERIAL_HIT0,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_JNTSPH,
    },
    2,
    sJntSphElementInit,
};

static void EnBa_Idle(EnBa* this, PlayState* play);
static void EnBa_SwingAtPlayer(EnBa* this, PlayState* play);
static void EnBa_RecoilFromDamage(EnBa* this, PlayState* play);
static void EnBa_Die(EnBa* this, PlayState* play);

// OoT's func_80035844: yaw and pitch from one point to another.
static Vec3s EnBa_AimBetween(Vec3f* from, Vec3f* to) {
    f32 dx = to->x - from->x;
    f32 dz = to->z - from->z;
    f32 dy = from->y - to->y;
    Vec3s rot = { Math_Atan2S(sqrtf(SQ(dx) + SQ(dz)), dy), Math_Atan2S(dz, dx), 0 };

    return rot;
}

static void EnBa_StepTowards(Vec3f* target, Vec3f* pos, f32 step) {
    Math_SmoothStepToF(&pos->x, target->x, 1.0f, step, 0.0f);
    Math_SmoothStepToF(&pos->y, target->y, 1.0f, step, 0.0f);
    Math_SmoothStepToF(&pos->z, target->z, 1.0f, step, 0.0f);
}

static void EnBa_PlaceHeadSegment(EnBa* this) {
    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateZYX(this->actor.shape.rot.x - 0x8000, this->actor.shape.rot.y, 0, MTXMODE_APPLY);
    Matrix_MultVec3f(&sSegmentOffset, &this->segmentPos[0]);
}

static void EnBa_PlaceNextSegment(EnBa* this, s32 i, Vec3f* offset) {
    Matrix_Translate(this->segmentPos[i].x, this->segmentPos[i].y, this->segmentPos[i].z, MTXMODE_NEW);
    Matrix_RotateZYX(this->segmentRot[i].x - 0x8000, this->segmentRot[i].y, 0, MTXMODE_APPLY);
    Matrix_MultVec3f(offset, &this->segmentPos[i + 1]);
}

static void EnBa_CopyTipRotation(EnBa* this) {
    this->segmentRot[TENTACLE_LAST_SEGMENT].x = this->segmentRot[TENTACLE_LAST_SEGMENT - 1].x;
    this->segmentRot[TENTACLE_LAST_SEGMENT].y = this->segmentRot[TENTACLE_LAST_SEGMENT - 1].y;
}

// The root drifts around a point below the head, and the segments are dragged back up to it.
static void EnBa_SwayRoot(EnBa* this, PlayState* play) {
    this->rootTarget = this->actor.world.pos;
    if ((play->gameplayFrames % 16) == 0) {
        this->swayPhaseX += Rand_CenteredFloat(180.0f);
        this->swayPhaseZ += Rand_CenteredFloat(180.0f);
        this->swayOffset.x = Math_SinF(this->swayPhaseX) * 80.0f;
        this->swayOffset.y = Math_CosF(this->swayPhaseZ) * 80.0f;
    }
    this->rootTarget.y -= TENTACLE_ROOT_DEPTH;
    this->rootTarget.x += this->swayOffset.x;
    this->rootTarget.z += this->swayOffset.y;
    EnBa_StepTowards(&this->rootTarget, &this->segmentPos[TENTACLE_LAST_SEGMENT], this->actor.speed);
    for (s32 i = TENTACLE_LAST_SEGMENT - 1; i >= 0; i--) {
        Vec3s rot = EnBa_AimBetween(&this->segmentPos[i + 1], &this->segmentPos[i]);
        Matrix_Translate(this->segmentPos[i + 1].x, this->segmentPos[i + 1].y, this->segmentPos[i + 1].z, MTXMODE_NEW);
        Matrix_RotateZYX(rot.x, rot.y, 0, MTXMODE_APPLY);
        Matrix_MultVec3f(&sSegmentOffset, &this->segmentPos[i]);
    }
}

static void EnBa_SetupIdle(EnBa* this) {
    this->state = TENTACLE_STATE_IDLE;
    this->rotStep = 1500;
    this->actor.speed = 10.0f;
    this->actionFunc = EnBa_Idle;
}

static void EnBa_SetupSwingAtPlayer(EnBa* this) {
    this->state = TENTACLE_STATE_SWING;
    this->timer = 20;
    this->lashTimer = 0;
    this->rotStep = 1500;
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->actor.speed = 20.0f;
    this->actionFunc = EnBa_SwingAtPlayer;
}

static bool EnBa_IsPlayerDamaged(PlayState* play) {
    return GET_PLAYER(play)->stateFlags1 & TENTACLE_PLAYER_STATE1_DAMAGED;
}

static void EnBa_Idle(EnBa* this, PlayState* play) {
    // Once it has struck, it stays retracted high while the player keeps away.
    if ((this->actor.colChkInfo.mass == MASS_IMMOVABLE) && (this->actor.xzDistToPlayer > TENTACLE_REACH)) {
        Math_SmoothStepToF(&this->actor.world.pos.y, this->actor.home.pos.y + TENTACLE_HIGH_HEIGHT, 1.0f, 7.0f, 0.0f);
    } else {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        Math_SmoothStepToF(&this->actor.world.pos.y, this->actor.home.pos.y + TENTACLE_LOW_HEIGHT, 1.0f, 10.0f, 0.0f);
    }
    EnBa_SwayRoot(this, play);
    Vec3s rootRot = EnBa_AimBetween(&this->segmentPos[0], &this->rootTarget);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->segmentRot[0].y, 3, this->rotStep, 182);
    Math_SmoothStepToS(&this->actor.shape.rot.x, this->segmentRot[0].x, 3, this->rotStep, 182);
    EnBa_PlaceHeadSegment(this);
    this->segmentRot[TENTACLE_LAST_SEGMENT].y = rootRot.y;
    this->segmentRot[TENTACLE_LAST_SEGMENT].x = rootRot.x + 0x8000;
    for (s32 i = 0; i < TENTACLE_LAST_SEGMENT; i++) {
        Math_SmoothStepToS(&this->segmentRot[i].y, this->segmentRot[i + 1].y, 3, this->rotStep, 182);
        Math_SmoothStepToS(&this->segmentRot[i].x, this->segmentRot[i + 1].x, 3, this->rotStep, 182);
        EnBa_PlaceNextSegment(this, i, &sSegmentOffset);
    }
    EnBa_CopyTipRotation(this);
    bool isReadyToStrike = (this->actor.xzDistToPlayer <= TENTACLE_REACH) &&
                           (this->actor.world.pos.y == this->actor.home.pos.y + TENTACLE_LOW_HEIGHT);
    if (!EnBa_IsPlayerDamaged(play) && isReadyToStrike) {
        EnBa_SetupSwingAtPlayer(this);
    }
}

static void EnBa_RestartOrRest(EnBa* this, PlayState* play) {
    if ((this->actor.xzDistToPlayer > TENTACLE_REACH) || EnBa_IsPlayerDamaged(play)) {
        EnBa_SetupIdle(this);
        return;
    }
    EnBa_SetupSwingAtPlayer(this);
    this->timer = 27;
    this->rotStep = 750;
}

static void EnBa_WindUp(EnBa* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 yaw;

    this->lashTimer = 10;
    this->timer--;
    if (this->timer >= 11) {
        this->rootTarget = player->actor.world.pos;
        this->rootTarget.y += 30.0f;
        yaw = this->actor.yawTowardsPlayer;
    } else {
        yaw = Math_Vec3f_Yaw(&this->actor.world.pos, &this->rootTarget);
    }
    Math_SmoothStepToS(&this->rotStep, 1500, 1, 30, 0);
    Vec3s rot = EnBa_AimBetween(&this->actor.world.pos, &this->segmentPos[0]);
    Math_SmoothStepToS(&this->actor.shape.rot.y, rot.y, 1, this->rotStep, 0);
    Math_SmoothStepToS(&this->actor.shape.rot.x, rot.x + 0x8000, 1, this->rotStep, 0);
    EnBa_PlaceHeadSegment(this);
    for (s32 i = 0; i < TENTACLE_LAST_SEGMENT; i++) {
        Math_SmoothStepToS(&this->segmentRot[i].x, (i * 1200) - 0x4000, 1, this->rotStep, 0);
        Math_SmoothStepToS(&this->segmentRot[i].y, yaw, 1, this->rotStep, 0);
        EnBa_PlaceNextSegment(this, i, &sSegmentOffset);
    }
}

static void EnBa_Lash(EnBa* this) {
    this->rotStep = 8000;
    this->actor.speed = 30.0f;
    s16 yaw = Math_Vec3f_Yaw(&this->actor.world.pos, &this->rootTarget);
    s16 pitch = Math_Vec3f_Pitch(&this->actor.world.pos, &this->segmentPos[0]) + 0x8000;
    Math_SmoothStepToS(&this->actor.shape.rot.y, yaw, 1, this->rotStep, 0);
    Math_SmoothStepToS(&this->actor.shape.rot.x, pitch, 1, this->rotStep, 0);
    EnBa_PlaceHeadSegment(this);
    for (s32 i = 0; i < TENTACLE_LAST_SEGMENT; i++) {
        s16 curl = -Math_CosS(this->lashTimer * 0xCCC) * (i * 1200);
        Math_SmoothStepToS(&this->segmentRot[i].x, curl - 0x4000, 1, this->rotStep, 0);
        Math_SmoothStepToS(&this->segmentRot[i].y, yaw, 1, this->rotStep, 0);
        EnBa_PlaceNextSegment(this, i, &sSegmentOffset);
    }
    this->lashTimer--;
}

static void EnBa_SwingAtPlayer(EnBa* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Math_SmoothStepToF(&this->actor.world.pos.y, this->actor.home.pos.y + 60.0f, 1.0f, 10.0f, 0.0f);
    if ((this->actor.xzDistToPlayer > TENTACLE_REACH) && (this->lashTimer == 0)) {
        EnBa_RestartOrRest(this, play);
        return;
    }
    if (this->timer == 20) {
        Actor_PlaySfx(&this->actor, TENTACLE_SFX_RAISE);
        this->rotStep = 1500;
    }
    if (this->timer != 0) {
        EnBa_WindUp(this, play);
    } else {
        if (this->lashTimer == 10) {
            Actor_PlaySfx(&this->actor, TENTACLE_SFX_LASH);
        }
        if (this->lashTimer != 0) {
            EnBa_Lash(this);
        } else {
            EnBa_RestartOrRest(this, play);
        }
    }
    EnBa_CopyTipRotation(this);
    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~AT_HIT;
        if (this->collider.base.at == &player->actor) {
            func_800B8E1C(play, &this->actor, 8.0f, this->actor.yawTowardsPlayer, 8.0f);
        }
    }
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
}

static void EnBa_SetupRecoil(EnBa* this) {
    this->state = TENTACLE_STATE_RECOIL;
    this->rotStep = 1500;
    this->timer = 20;
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->actor.speed = 10.0f;
    Actor_PlaySfx(&this->actor, TENTACLE_SFX_DAMAGE);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 12);
    this->actionFunc = EnBa_RecoilFromDamage;
}

static void EnBa_RecoilFromDamage(EnBa* this, PlayState* play) {
    Math_SmoothStepToF(&this->actor.world.pos.y, this->actor.home.pos.y + TENTACLE_HIGH_HEIGHT, 1.0f, 30.0f, 0.0f);
    EnBa_SwayRoot(this, play);
    Vec3s rot = EnBa_AimBetween(&this->actor.world.pos, &this->segmentPos[0]);
    Math_SmoothStepToS(&this->actor.shape.rot.y, rot.y, 3, this->rotStep, 182);
    Math_SmoothStepToS(&this->actor.shape.rot.x, rot.x + 0x8000, 3, this->rotStep, 182);
    EnBa_PlaceHeadSegment(this);
    for (s32 i = 0; i < TENTACLE_LAST_SEGMENT; i++) {
        rot = EnBa_AimBetween(&this->segmentPos[i], &this->segmentPos[i + 1]);
        Math_SmoothStepToS(&this->segmentRot[i].y, rot.y, 3, this->rotStep, 182);
        Math_SmoothStepToS(&this->segmentRot[i].x, rot.x + 0x8000, 3, this->rotStep, 182);
        EnBa_PlaceNextSegment(this, i, &sSegmentOffset);
    }
    EnBa_CopyTipRotation(this);
    this->timer--;
    if (this->timer == 0) {
        EnBa_SetupIdle(this);
    }
}

// The lower half collapses onto the middle segment while the upper half thrashes.
static void EnBa_CollapseLowerHalf(EnBa* this) {
    for (s32 i = 5; i < TENTACLE_LAST_SEGMENT; i++) {
        Math_SmoothStepToS(&this->segmentRot[i].x, this->segmentRot[5].x, 1, this->rotStep, 0);
        Math_SmoothStepToS(&this->segmentRot[i].y, this->segmentRot[5].y, 1, this->rotStep, 0);
        EnBa_PlaceNextSegment(this, i, &sZeroOffset);
    }
}

static void EnBa_AimHeadAtPlayer(EnBa* this) {
    s16 pitch = Math_Vec3f_Pitch(&this->actor.world.pos, &this->segmentPos[0]) + 0x8000;

    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, this->rotStep, 0);
    Math_SmoothStepToS(&this->actor.shape.rot.x, pitch, 1, this->rotStep, 0);
    EnBa_PlaceHeadSegment(this);
}

static void EnBa_SetupDie(EnBa* this, PlayState* play) {
    this->rotStep = 2500;
    Actor_PlaySfx(&this->actor, TENTACLE_SFX_DEAD);
    this->state = TENTACLE_STATE_DEAD;
    for (s32 i = 7; i < TENTACLE_SEGMENTS; i++) {
        Actor_Spawn(&play->actorCtx, play, sBlobId, this->segmentPos[i].x, this->segmentPos[i].y, this->segmentPos[i].z,
                    0, 0, 0, 0);
    }
    EnBa_AimHeadAtPlayer(this);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    EnBa_CollapseLowerHalf(this);
    this->lashTimer = 15;
    this->actionFunc = EnBa_Die;
}

static void EnBa_Die(EnBa* this, PlayState* play) {
    if (this->lashTimer == 0) {
        Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x50);
        Actor_Kill(&this->actor);
        return;
    }
    this->actor.speed = 30.0f;
    this->rotStep = 8000;
    this->actor.world.pos.y += 8.0f;
    EnBa_AimHeadAtPlayer(this);
    for (s32 i = 0; i < 5; i++) {
        s16 curl = -Math_CosS(this->lashTimer * 0x444) * (i * 400);
        Math_SmoothStepToS(&this->segmentRot[i].x, curl - 0x4000, 1, this->rotStep, 0);
        Math_SmoothStepToS(&this->segmentRot[i].y, this->actor.yawTowardsPlayer, 1, this->rotStep, 0);
        EnBa_PlaceNextSegment(this, i, &sSegmentOffset);
    }
    EnBa_CollapseLowerHalf(this);
    this->lashTimer--;
}

// Segment 6 is drawn thinner.
static void EnBa_SetSegmentMatrix(EnBa* this, s32 segment) {
    Matrix_Translate(this->segmentPos[segment].x, this->segmentPos[segment].y, this->segmentPos[segment].z,
                     MTXMODE_NEW);
    Matrix_RotateZYX(this->segmentRot[segment].x, this->segmentRot[segment].y, this->segmentRot[segment].z,
                     MTXMODE_APPLY);
    Matrix_Scale(TENTACLE_SEGMENT_SCALE, TENTACLE_SEGMENT_SCALE, TENTACLE_SEGMENT_SCALE, MTXMODE_APPLY);
    if (segment == 6) {
        Matrix_Scale(0.5f, 0.5f, 1.0f, MTXMODE_APPLY);
    }
}

// Vanilla moves the spheres in Draw, which a culled tentacle skips; the hurtbox rides segment 6 (limb 8).
static void EnBa_UpdateColliderSpheres(EnBa* this) {
    Matrix_Push();
    EnBa_SetSegmentMatrix(this, 6);
    Collider_UpdateSpheres(8, &this->collider);
    EnBa_SetSegmentMatrix(this, TENTACLE_LAST_SEGMENT);
    Collider_UpdateSpheres(TENTACLE_LAST_SEGMENT, &this->collider);
    Matrix_Pop();
}

static void EnBa_Update(Actor* thisx, PlayState* play) {
    EnBa* this = (EnBa*)thisx;

    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        this->actor.colChkInfo.health--;
        if (this->actor.colChkInfo.health == 0) {
            Enemy_StartFinishingBlow(play, &this->actor);
            EnBa_SetupDie(this, play);
        } else {
            EnBa_SetupRecoil(this);
        }
    }
    this->actionFunc(this, play);
    this->actor.focus.pos = this->segmentPos[6];
    EnBa_UpdateColliderSpheres(this);
    if (this->state >= TENTACLE_STATE_IDLE) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
}

static void EnBa_Init(Actor* thisx, PlayState* play) {
    EnBa* this = (EnBa*)thisx;

    this->actor.cullingVolumeScale = 1500.0f;
    this->actor.cullingVolumeDownward = 2500.0f;
    this->actor.lockOnArrowOffset = 0.0f;
    this->actor.attentionRangeType = ATTENTION_RANGE_4;
    this->actor.world.pos.y = this->actor.home.pos.y + TENTACLE_LOW_HEIGHT;
    for (s32 i = TENTACLE_LAST_SEGMENT; i >= 0; i--) {
        this->segmentRot[i].x = -0x4000;
        this->segmentPos[i] = this->actor.world.pos;
        this->segmentPos[i].y = this->actor.world.pos.y - (i + 1) * 32.0f;
    }
    this->actor.params = CLAMP(this->actor.params & 0xFF, TENTACLE_COLOR_RED, TENTACLE_COLOR_MAX - 1);
    this->epoch++;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 48.0f);
    Actor_SetScale(&this->actor, 0.01f);
    EnBa_SetupIdle(this);
    this->actor.colChkInfo.health = TENTACLE_HEALTH;
    this->actor.colChkInfo.mass = MASS_HEAVY;
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sJntSphInit, this->colliderItems);
}

static void EnBa_Destroy(Actor* thisx, PlayState* play) {
    EnBa* this = (EnBa*)thisx;

    Collider_DestroyJntSph(play, &this->collider);
}

static void EnBa_Draw(Actor* thisx, PlayState* play) {
    static const char* sColorTextures[TENTACLE_COLOR_MAX] = { sRedTex, sGreenTex, sGrayTex };
    EnBa* this = (EnBa*)thisx;
    // The tentacle DL loads one matrix per segment from segment 0x0C.
    Mtx* mtx = GRAPH_ALLOC(play->state.gfxCtx, sizeof(Mtx) * TENTACLE_SEGMENTS);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Push();
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)mtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sColorTextures[this->actor.params]);
    gSPSegment(POLY_OPA_DISP++, 0x09,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 16, 16, 1, 0,
                                             (play->gameplayFrames * -10) % 128, 32, 32, 0, 0, 0, -10));
    for (s32 i = 0; i < TENTACLE_SEGMENTS; i++, mtx++) {
        FrameInterpolation_RecordOpenChild(this, this->epoch + i * 25);
        EnBa_SetSegmentMatrix(this, i);
        Matrix_ToMtx(mtx);
        FrameInterpolation_RecordCloseChild();
    }
    Matrix_Pop();
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sTentacleDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnBaBlob_Update(Actor* thisx, PlayState* play) {
    EnBa* this = (EnBa*)thisx;

    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        Actor_MoveWithGravity(&this->actor);
        Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, 28.0f, 80.0f, UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4);
        return;
    }
    this->actor.scale.y -= 0.001f;
    this->actor.scale.x += 0.0005f;
    this->actor.scale.z += 0.0005f;
    this->timer--;
    if (this->timer == 0) {
        Actor_Kill(&this->actor);
    }
}

static void EnBaBlob_Init(Actor* thisx, PlayState* play) {
    EnBa* this = (EnBa*)thisx;

    Actor_SetScale(&this->actor, 0.021f);
    this->actor.speed = Rand_CenteredFloat(8.0f);
    this->actor.world.rot.y = Rand_CenteredFloat(65535.0f);
    this->timer = 20;
    this->actor.gravity = -2.0f;
}

static void EnBaBlob_Draw(Actor* thisx, PlayState* play) {
    u32 frames = play->gameplayFrames;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (frames * 2) % 128, (frames * 2) % 128, 32, 32, 1,
                                             (frames * -5) % 128, (frames * -5) % 128, 32, 32, 2, 2, -5, -5));
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 125, 100, 255);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sBlobDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawSoul(PlayState* play, S2HGetItemEntry* entry) {
    u32 frames = play->state.frames;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x10, 0x20, 1, (frames * 2) & 0x3F,
                                           (frames * -6) & 0x7F, 0x10, 0x20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0xC0, 0xC0, 255, 110, 140, 0);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 110, 140, 0);
    Matrix_Scale(0.13333333f, 0.13333333f, 0.13333333f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gameplay_keep_DL_01ACF0);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Ship (RE_TENTACLE): the boomerang, which in MM is Zora's.
static bool CanKillTentacle(void* userData) {
    return sApi->LogicHasCapability("CanBeZora");
}

static void RegisterSoul(void) {
    static S2HCustomItemRandomizer sSoulLogic = { 0 };
    S2HCustomItemDefinition soul = { 0 };

    sSoulLogic.structSize = sizeof(sSoulLogic);
    sSoulLogic.flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
    sSoulLogic.type = S2H_CUSTOM_ITEM_TYPE_ITEM;
    sSoulLogic.logicKey = TENTACLE_SOUL_KEY;
    sSoulLogic.hintKey = TENTACLE_SOUL_KEY;
    soul.structSize = sizeof(soul);
    soul.key = TENTACLE_SOUL_KEY;
    soul.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO;
    soul.getItemText = "You got the %rSoul of Parasitic Tentacle%w!&They can be defeated now.";
    soul.getItemEntry.drawFunc = DrawSoul;
    soul.randomizer = &sSoulLogic;
    if (!sApi->RegisterCustomItem(&soul)) {
        sApi->Log("Could not register " TENTACLE_SOUL_KEY);
    }
}

static s16 RegisterBlob(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = TENTACLE_BLOB_KEY;
    definition.description = "Parasitic Tentacle remains";
    definition.category = ACTORCAT_MISC;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnBa);
    definition.init = EnBaBlob_Init;
    definition.update = EnBaBlob_Update;
    definition.draw = EnBaBlob_Draw;
    return sApi->RegisterActor(&definition);
}

static s16 RegisterTentacle(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = TENTACLE_KEY;
    definition.description = "Parasitic Tentacle";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = TENTACLE_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnBa);
    definition.init = EnBa_Init;
    definition.destroy = EnBa_Destroy;
    definition.update = EnBa_Update;
    definition.draw = EnBa_Draw;
    // It hangs 330 units above its spawn and reaches 448 down, so it needs a high ceiling.
    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_NEEDS_CEILING | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    definition.enemyParams = TENTACLE_COLOR_RED;
    definition.naviHint = "Parasitic Tentacle&It lashes out when you come close.&Only a boomerang can hurt it!";
    definition.soulKey = TENTACLE_KEY;
    definition.logicKill = CanKillTentacle;
    return sApi->RegisterActor(&definition);
}

typedef bool (*OotAssetsRequireFunc)(const char* path);

// oot_assets offers to extract the ROM again when a resource is missing.
static bool RequireOotAsset(const char* path) {
    OotAssetsRequireFunc require = (OotAssetsRequireFunc)sApi->FindService("oot_assets.require");

    return (require != NULL) ? require(path) : (sApi->HasResource(path) != 0);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    static const S2HModRequirements requirements = { sizeof(S2HModRequirements), NULL, 0 };
    return &requirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, FindService)) {
        return;
    }
    if (!RequireOotAsset(BXA_OBJECT "object_bxa_DL_000890")) {
        sApi->Log(TENTACLE_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    sBlobId = RegisterBlob();
    if (sBlobId < 0) {
        sApi->Log("Could not register " TENTACLE_BLOB_KEY);
        return;
    }
    if (RegisterTentacle() < 0) {
        sApi->Log("Could not register " TENTACLE_KEY);
        return;
    }
    RegisterSoul();
}
