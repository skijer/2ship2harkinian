/*
 * File: z_en_sbeetle.c
 * Description: Scissors Beetle comparable to the Scissors Beetles from The Minish Cap.
 * Authors: @syeo501 (Model) @trueffel (Code)
 */

#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#include "object_sbeetle_assets.h"
#include "object_sbeetle_assets.inc.c"

#define SBEETLE_KEY "trutefel.enemy.sbeetle"

static const S2HModApi* sApi;

typedef enum {
    ENSBEETLE_PINCER_ATTACHED,
    ENSBEETLE_PINCER_WINDUP,
    ENSBEETLE_PINCER_OUTBOUND,
    ENSBEETLE_PINCER_RETURN,
    ENSBEETLE_PINCER_FAST_RETURN
} EnSbeetlePincerState;

struct EnSbeetle;

typedef void (*EnSbeetleActionFunc)(struct EnSbeetle*, PlayState*);

typedef struct EnSbeetle {
    Actor actor;
    Vec3s jointTable[GSCISSORSBEETLESKEL_NUM_LIMBS];
    Vec3s morphTable[GSCISSORSBEETLESKEL_NUM_LIMBS];
    SkelAnime skelAnime;
    ColliderCylinder collider;
    ColliderCylinder pincerLCollider;
    ColliderCylinder pincerRCollider;
    EnSbeetleActionFunc actionFunc;
    EnSbeetleActionFunc idleAction;
    f32 playerDistAtSetup;
    s16 nextIdleTimer;
    s16 afterAnimTimer;
    s16 attackTimer;
    s16 hurtboxCooldown;
    s16 damageTimer;
    s16 deathFreeze;
    s16 randomWalkTimer;
    s16 playerLostTimer;
    s16 spawnIceTimer;
    s16 fireTimer;
    u8 frozen;
    u8 audioPlayed;

    Vec3f pincerLWorldPos;
    Vec3f pincerRWorldPos;
    Vec3f pincerLHomePos;
    Vec3f pincerRHomePos;
    Vec3f pincerLReturnStart;
    Vec3f pincerRReturnStart;
    Vec3f pincerTargetPos;
    s16 pincerState;
    s16 pincerFlightTimer;
    s16 pincerLSpin;
    s16 pincerRSpin;
} EnSbeetle;

#define SBEETLE_FLAGS                                                                         \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR)

void EnSbeetle_Init(Actor* thisx, PlayState* play);
void EnSbeetle_Destroy(Actor* thisx, PlayState* play);
void EnSbeetle_Update(Actor* thisx, PlayState* play);
void EnSbeetle_Draw(Actor* thisx, PlayState* play);

void EnSbeetle_WorldToCurrentMatrixLocal(Vec3f* worldPos, Vec3f* localPos);
void EnSbeetle_GetPincerPath(Vec3f* start, Vec3f* end, f32 progress, f32 side, Vec3f* result);
void EnSbeetle_StartPincerReturn(EnSbeetle* self);
void EnSbeetle_StartPincerFastReturn(EnSbeetle* self);
void EnSbeetle_UpdatePincers(EnSbeetle* self, PlayState* play);

s32 EnSbeetle_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx);
void EnSbeetle_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx);

void EnSbeetle_CheckHurt(EnSbeetle* self, PlayState* play);
void EnSbeetle_UpdateBgCheck(EnSbeetle* self, PlayState* play);
s32 EnSbeetle_HasLostPlayer(EnSbeetle* self, PlayState* play);
s32 EnSbeetle_CheckPlayerNear(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupDoNothing(EnSbeetle* self, PlayState* play);
void EnSbeetle_DoNothing(EnSbeetle* self, PlayState* play);
void EnSbeetle_IdleActionWalk(EnSbeetle* self, PlayState* play);
void EnSbeetle_IdleActionIdle2(EnSbeetle* self, PlayState* play);
void EnSbeetle_IdleActionIdle3(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupHopWithPlayerRot(EnSbeetle* self, PlayState* play);
void EnSbeetle_HopWithPlayerRot(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupThreatPlayer(EnSbeetle* self, PlayState* play);
void EnSbeetle_ThreatPlayer(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupAttack(EnSbeetle* self, PlayState* play);
void EnSbeetle_Attack(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupSwingAttack(EnSbeetle* self, PlayState* play);
void EnSbeetle_SwingAttack(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupHopAwayFromOrTowardsPlayer(EnSbeetle* self, PlayState* play);
void EnSbeetle_HopAwayFromOrTowardsPlayer(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupStunned(EnSbeetle* self, PlayState* play);
void EnSbeetle_Stunned(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupHurt(EnSbeetle* self, PlayState* play);
void EnSbeetle_Hurt(EnSbeetle* self, PlayState* play);
void EnSbeetle_SetupDie(EnSbeetle* self, PlayState* play);
void EnSbeetle_Die(EnSbeetle* self, PlayState* play);

#define ENSBEETLE_PINCER_THROW_FRAME 12.0f
#define ENSBEETLE_PINCER_OUT_TIME 18
#define ENSBEETLE_PINCER_RETURN_TIME 20
#define ENSBEETLE_PINCER_CURVE 55.0f
#define ENSBEETLE_PINCER_ARC_HEIGHT 25.0f
#define ENSBEETLE_PINCER_SPIN_SPEED 0x2800
#define ENSBEETLE_PINCER_FAST_RETURN_TIME 6

static ColliderCylinderInit sSbeetleCylinderInit = {
    {
        COL_MATERIAL_HARD,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK1,
        { 0x00000000, 0x00, 0x00 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_NONE,
        ACELEM_ON | ACELEM_HOOKABLE,
        OCELEM_ON,
    },
    { 40, 45, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sPincerLCylinderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x00, 0x08 },
        { 0x00000000, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL | ATELEM_UNK7,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { 35, 30, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sPincerRCylinderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x00, 0x08 },
        { 0x00000000, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL | ATELEM_UNK7,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { 35, 30, 0, { 0, 0, 0 } },
};

typedef enum {
    /*  0 */ ENSBEETLE_DMGEFF_NONE,
    /*  1 */ ENSBEETLE_DMGEFF_STUN,
    /*  6 */ ENSBEETLE_DMGEFF_ICE_MAGIC = 6,
    /* 13 */ ENSBEETLE_DMGEFF_LIGHT_MAGIC = 13,
    /* 14 */ ENSBEETLE_DMGEFF_FIRE
} EnSbeetleDamageEffect;

static DamageTable sSbeetleDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_STUN),
    /* Deku Stick     */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Horse trample  */ DMG_ENTRY(1, ENSBEETLE_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Zora boomerang */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_STUN),
    /* Normal arrow   */ DMG_ENTRY(1, ENSBEETLE_DMGEFF_NONE),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_STUN),
    /* Goron punch    */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Sword          */ DMG_ENTRY(1, ENSBEETLE_DMGEFF_NONE),
    /* Goron pound    */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Fire arrow     */ DMG_ENTRY(3, ENSBEETLE_DMGEFF_FIRE),
    /* Ice arrow      */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_ICE_MAGIC),
    /* Light arrow    */ DMG_ENTRY(4, ENSBEETLE_DMGEFF_LIGHT_MAGIC),
    /* Goron spikes   */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Deku spin      */ DMG_ENTRY(1, ENSBEETLE_DMGEFF_NONE),
    /* Deku bubble    */ DMG_ENTRY(1, ENSBEETLE_DMGEFF_NONE),
    /* Deku launch    */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_STUN),
    /* Zora barrier   */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_STUN),
    /* Normal shield  */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Zora punch     */ DMG_ENTRY(1, ENSBEETLE_DMGEFF_NONE),
    /* Spin attack    */ DMG_ENTRY(3, ENSBEETLE_DMGEFF_NONE),
    /* Sword beam     */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
    /* Normal Roll    */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, ENSBEETLE_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(2, ENSBEETLE_DMGEFF_NONE),
};

static CollisionCheckInfoInit2 sSbeetleColChkInit = {
    /* health    */ 5,
    /* cylRadius */ 25,
    /* cylHeight */ 35,
    /* cylYShift */ 0,
    /* mass      */ MASS_HEAVY,
};

typedef enum {
    /* 0 */ SCISSORSBEETLE_ANIMATION_IDLE1,
    /* 1 */ SCISSORSBEETLE_ANIMATION_IDLE2,
    /* 2 */ SCISSORSBEETLE_ANIMATION_IDLE3,
    /* 3 */ SCISSORSBEETLE_ANIMATION_WALK,
    /* 4 */ SCISSORSBEETLE_ANIMATION_HOP,
    /* 5 */ SCISSORSBEETLE_ANIMATION_ATTACK,
    /* 6 */ SCISSORSBEETLE_ANIMATION_SWING,
    /* 7 */ SCISSORSBEETLE_ANIMATION_HURT,
    /* 8 */ SCISSORSBEETLE_ANIMATION_DIE
} EnSbeetleAnimation;

static AnimationInfo sSbeetleAnimationInfo[] = {
    { &gScissorsBeetleSkelIdle1Anim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP_INTERP, 3.0f },
    { &gScissorsBeetleSkelIdle2Anim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelIdle3Anim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelWalkAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP_INTERP, 3.0f },
    { &gScissorsBeetleSkelHopAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelAttackAnim, 1.5f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelSwingAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelHurtAnim, 1.5f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
    { &gScissorsBeetleSkelDieAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE, 3.0f },
};

void EnSbeetle_ChangeAnimation(EnSbeetle* self, s32 index) {
    Actor_ChangeAnimationByInfo(&self->skelAnime, sSbeetleAnimationInfo, index);
}

static void EnSbeetle_SpawnIceChunk(PlayState* play, Vec3f* pos) {
    static Color_RGBA8 sIcePrimColor = { 150, 150, 150, 250 };
    static Color_RGBA8 sIceEnvColor = { 235, 245, 255, 255 };
    static Vec3f sIceAccel = { 0.0f, -1.0f, 0.0f };
    Vec3f velocity;

    velocity.x = Rand_CenteredFloat(6.0f);
    velocity.y = (Rand_ZeroOne() * 4.0f) + 5.0f;
    velocity.z = Rand_CenteredFloat(6.0f);

    EffectSsEnIce_Spawn(play, pos, (Rand_ZeroOne() * 0.2f) + 0.15f, &velocity, &sIceAccel, &sIcePrimColor,
                        &sIceEnvColor, 30);
}

void EnSbeetle_InitAndSetCollision(EnSbeetle* self, PlayState* play) {
    Collider_InitCylinder(play, &self->collider);
    Collider_SetCylinder(play, &self->collider, &self->actor, &sSbeetleCylinderInit);
    CollisionCheck_SetInfo2(&self->actor.colChkInfo, &sSbeetleDamageTable, &sSbeetleColChkInit);

    Collider_InitCylinder(play, &self->pincerLCollider);
    Collider_SetCylinder(play, &self->pincerLCollider, &self->actor, &sPincerLCylinderInit);

    Collider_InitCylinder(play, &self->pincerRCollider);
    Collider_SetCylinder(play, &self->pincerRCollider, &self->actor, &sPincerRCylinderInit);
}

void EnSbeetle_Init(Actor* thisx, PlayState* play) {
    EnSbeetle* self = (EnSbeetle*)thisx;

    ActorShape_Init(&self->actor.shape, 0.0f, ActorShadow_DrawCircle, 10.0f);
    Actor_SetScale(&self->actor, 0.1f);
    thisx->gravity = -1.0f;
    self->nextIdleTimer = 0;
    self->attackTimer = 0;
    EnSbeetle_InitAndSetCollision(self, play);
    SkelAnime_InitFlex(play, &self->skelAnime, &gScissorsBeetleSkel, &gScissorsBeetleSkelIdle1Anim, self->jointTable,
                       self->morphTable, GSCISSORSBEETLESKEL_NUM_LIMBS);
    EnSbeetle_SetupDoNothing(self, play);
}

void EnSbeetle_Destroy(Actor* thisx, PlayState* play) {
    EnSbeetle* self = (EnSbeetle*)thisx;

    SkelAnime_Free(&self->skelAnime, play);
    Collider_DestroyCylinder(play, &self->collider);
    Collider_DestroyCylinder(play, &self->pincerLCollider);
    Collider_DestroyCylinder(play, &self->pincerRCollider);
}

void EnSbeetle_Update(Actor* thisx, PlayState* play) {
    EnSbeetle* self = (EnSbeetle*)thisx;

    EnSbeetle_CheckHurt(self, play);
    self->actionFunc(self, play);

    EnSbeetle_UpdatePincers(self, play);

    Actor_MoveWithGravity(thisx);
    EnSbeetle_UpdateBgCheck(self, play);

    Collider_UpdateCylinder(thisx, &self->collider);

    if ((self->pincerState == ENSBEETLE_PINCER_OUTBOUND) || (self->pincerState == ENSBEETLE_PINCER_RETURN) ||
        (self->pincerState == ENSBEETLE_PINCER_FAST_RETURN)) {
        Collider_UpdateCylinder(&self->actor, &self->pincerLCollider);

        Collider_UpdateCylinder(&self->actor, &self->pincerRCollider);

        self->pincerLCollider.dim.pos.x = self->pincerLWorldPos.x;
        self->pincerLCollider.dim.pos.y = self->pincerLWorldPos.y;
        self->pincerLCollider.dim.pos.z = self->pincerLWorldPos.z;

        self->pincerRCollider.dim.pos.x = self->pincerRWorldPos.x;
        self->pincerRCollider.dim.pos.y = self->pincerRWorldPos.y;
        self->pincerRCollider.dim.pos.z = self->pincerRWorldPos.z;

        if (self->pincerState != ENSBEETLE_PINCER_FAST_RETURN) {
            CollisionCheck_SetAT(play, &play->colChkCtx, &self->pincerLCollider.base);
            CollisionCheck_SetAT(play, &play->colChkCtx, &self->pincerRCollider.base);
        }
    }

    if (self->actionFunc != EnSbeetle_Die) {
        if (DECR(self->hurtboxCooldown) == 0) {
            CollisionCheck_SetAC(play, &play->colChkCtx, &self->collider.base);
        }

        CollisionCheck_SetOC(play, &play->colChkCtx, &self->collider.base);
    }
}

static Vec3f sSbeetleIceChunks[12] = {
    { 20.0f, 20.0f, 0.0f },   { 10.0f, 40.0f, 10.0f },   { -10.0f, 40.0f, 10.0f }, { -20.0f, 20.0f, 0.0f },
    { 10.0f, 40.0f, -10.0f }, { -10.0f, 40.0f, -10.0f }, { 0.0f, 20.0f, -20.0f },  { 10.0f, 0.0f, 10.0f },
    { 10.0f, 0.0f, -10.0f },  { 0.0f, 20.0f, 20.0f },    { -10.0f, 0.0f, 10.0f },  { -10.0f, 0.0f, -10.0f },
};

static Vec3f sSbeetleFlames[12] = {
    { 20.0f, 20.0f, 0.0f },   { 10.0f, 40.0f, 10.0f },   { -10.0f, 40.0f, 10.0f }, { -20.0f, 20.0f, 0.0f },
    { 10.0f, 40.0f, -10.0f }, { -10.0f, 40.0f, -10.0f }, { 0.0f, 20.0f, -20.0f },  { 10.0f, 0.0f, 10.0f },
    { 10.0f, 0.0f, -10.0f },  { 0.0f, 20.0f, 20.0f },    { -10.0f, 0.0f, 10.0f },  { -10.0f, 0.0f, -10.0f },
};

void EnSbeetle_Draw(Actor* thisx, PlayState* play) {
    EnSbeetle* self = (EnSbeetle*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    if (self->spawnIceTimer != 0) {
        thisx->colorFilterTimer++;
        self->spawnIceTimer--;
        if ((self->spawnIceTimer & 3) == 0) {
            Vec3f iceChunk;
            s32 idx = self->spawnIceTimer >> 2;

            iceChunk.x = thisx->world.pos.x + sSbeetleIceChunks[idx].x;
            iceChunk.y = thisx->world.pos.y + sSbeetleIceChunks[idx].y;
            iceChunk.z = thisx->world.pos.z + sSbeetleIceChunks[idx].z;
            EnSbeetle_SpawnIceChunk(play, &iceChunk);
        }
    }

    if (self->fireTimer != 0) {
        static Color_RGBA8 sFirePrimColor = { 200, 135, 50, 255 };
        static Color_RGBA8 sFireEnvColor = { 200, 80, 50, 255 };

        thisx->colorFilterTimer++;
        self->fireTimer--;
        if ((self->fireTimer & 3) == 0) {
            Vec3f firePos;
            Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
            Vec3f effectVel = { 0.0f, 4.0f, 0.0f };
            s32 idx = self->fireTimer >> 2;

            firePos.x = thisx->world.pos.x + sSbeetleFlames[idx].x;
            firePos.y = thisx->world.pos.y + sSbeetleFlames[idx].y;
            firePos.z = thisx->world.pos.z + sSbeetleFlames[idx].z;

            EffectSsDeadDb_Spawn(play, &firePos, &effectVel, &zeroVec, &sFirePrimColor, &sFireEnvColor, 90, 0, 9);
        }
    }

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, self->skelAnime.skeleton, self->jointTable, self->skelAnime.dListCount,
                          EnSbeetle_OverrideLimbDraw, EnSbeetle_PostLimbDraw, &self->actor);

    CLOSE_DISPS(play->state.gfxCtx);
}

void EnSbeetle_WorldToCurrentMatrixLocal(Vec3f* worldPos, Vec3f* localPos) {
    MtxF mtx;
    Vec3f delta;
    f32 scaleSqX;
    f32 scaleSqY;
    f32 scaleSqZ;

    Matrix_Get(&mtx);

    delta.x = worldPos->x - mtx.xw;
    delta.y = worldPos->y - mtx.yw;
    delta.z = worldPos->z - mtx.zw;

    scaleSqX = SQ(mtx.xx) + SQ(mtx.yx) + SQ(mtx.zx);
    scaleSqY = SQ(mtx.xy) + SQ(mtx.yy) + SQ(mtx.zy);
    scaleSqZ = SQ(mtx.xz) + SQ(mtx.yz) + SQ(mtx.zz);

    if (scaleSqX > 0.000001f) {
        localPos->x = ((delta.x * mtx.xx) + (delta.y * mtx.yx) + (delta.z * mtx.zx)) / scaleSqX;
    } else {
        localPos->x = 0.0f;
    }

    if (scaleSqY > 0.000001f) {
        localPos->y = ((delta.x * mtx.xy) + (delta.y * mtx.yy) + (delta.z * mtx.zy)) / scaleSqY;
    } else {
        localPos->y = 0.0f;
    }

    if (scaleSqZ > 0.000001f) {
        localPos->z = ((delta.x * mtx.xz) + (delta.y * mtx.yz) + (delta.z * mtx.zz)) / scaleSqZ;
    } else {
        localPos->z = 0.0f;
    }
}

void EnSbeetle_GetPincerPath(Vec3f* start, Vec3f* end, f32 progress, f32 side, Vec3f* result) {
    f32 dx;
    f32 dz;
    f32 length;
    f32 perpendicularX;
    f32 perpendicularZ;
    f32 curve;
    s16 curveAngle;

    result->x = start->x + ((end->x - start->x) * progress);
    result->y = start->y + ((end->y - start->y) * progress);
    result->z = start->z + ((end->z - start->z) * progress);

    dx = end->x - start->x;
    dz = end->z - start->z;
    length = sqrtf(SQ(dx) + SQ(dz));

    if (length > 0.001f) {
        perpendicularX = -dz / length;
        perpendicularZ = dx / length;
    } else {
        perpendicularX = 0.0f;
        perpendicularZ = 0.0f;
    }

    curveAngle = (s16)(progress * 0x7FFF);
    curve = Math_SinS(curveAngle);

    result->x += perpendicularX * curve * ENSBEETLE_PINCER_CURVE * side;
    result->z += perpendicularZ * curve * ENSBEETLE_PINCER_CURVE * side;
    result->y += curve * ENSBEETLE_PINCER_ARC_HEIGHT;
}

void EnSbeetle_StartPincerReturn(EnSbeetle* self) {
    self->pincerLReturnStart = self->pincerLWorldPos;
    self->pincerRReturnStart = self->pincerRWorldPos;

    self->pincerState = ENSBEETLE_PINCER_RETURN;
    self->pincerFlightTimer = 0;
}

void EnSbeetle_StartPincerFastReturn(EnSbeetle* self) {
    if ((self->pincerState != ENSBEETLE_PINCER_OUTBOUND) && (self->pincerState != ENSBEETLE_PINCER_RETURN)) {
        return;
    }

    self->pincerLReturnStart = self->pincerLWorldPos;
    self->pincerRReturnStart = self->pincerRWorldPos;

    self->pincerFlightTimer = 0;
    self->pincerState = ENSBEETLE_PINCER_FAST_RETURN;

    self->pincerLCollider.base.atFlags &= ~AT_HIT;
    self->pincerRCollider.base.atFlags &= ~AT_HIT;
}

void EnSbeetle_UpdatePincers(EnSbeetle* self, PlayState* play) {
    f32 progress;
    s32 pincerHit;

    switch (self->pincerState) {
        case ENSBEETLE_PINCER_OUTBOUND:
            self->pincerLSpin += ENSBEETLE_PINCER_SPIN_SPEED;
            self->pincerRSpin -= ENSBEETLE_PINCER_SPIN_SPEED;

            pincerHit = (self->pincerLCollider.base.atFlags & AT_HIT) || (self->pincerRCollider.base.atFlags & AT_HIT);

            if (pincerHit) {
                self->pincerLCollider.base.atFlags &= ~AT_HIT;
                self->pincerRCollider.base.atFlags &= ~AT_HIT;

                EnSbeetle_StartPincerReturn(self);
                break;
            }

            progress = (f32)self->pincerFlightTimer / (f32)ENSBEETLE_PINCER_OUT_TIME;

            if (progress > 1.0f) {
                progress = 1.0f;
            }

            EnSbeetle_GetPincerPath(&self->pincerLHomePos, &self->pincerTargetPos, progress, -1.0f,
                                    &self->pincerLWorldPos);

            EnSbeetle_GetPincerPath(&self->pincerRHomePos, &self->pincerTargetPos, progress, 1.0f,
                                    &self->pincerRWorldPos);

            self->pincerFlightTimer++;

            if (self->pincerFlightTimer >= ENSBEETLE_PINCER_OUT_TIME) {
                EnSbeetle_StartPincerReturn(self);
            }
            break;

        case ENSBEETLE_PINCER_RETURN:
            self->pincerLSpin += ENSBEETLE_PINCER_SPIN_SPEED;
            self->pincerRSpin -= ENSBEETLE_PINCER_SPIN_SPEED;

            progress = (f32)self->pincerFlightTimer / (f32)ENSBEETLE_PINCER_RETURN_TIME;

            if (progress > 1.0f) {
                progress = 1.0f;
            }

            EnSbeetle_GetPincerPath(&self->pincerLReturnStart, &self->pincerLHomePos, progress, 1.0f,
                                    &self->pincerLWorldPos);

            EnSbeetle_GetPincerPath(&self->pincerRReturnStart, &self->pincerRHomePos, progress, -1.0f,
                                    &self->pincerRWorldPos);

            self->pincerFlightTimer++;

            if (self->pincerFlightTimer >= ENSBEETLE_PINCER_RETURN_TIME) {
                self->pincerState = ENSBEETLE_PINCER_ATTACHED;
                self->pincerFlightTimer = 0;
                self->pincerLSpin = 0;
                self->pincerRSpin = 0;
            }
            break;

        case ENSBEETLE_PINCER_FAST_RETURN:
            self->pincerLSpin += ENSBEETLE_PINCER_SPIN_SPEED * 2;
            self->pincerRSpin -= ENSBEETLE_PINCER_SPIN_SPEED * 2;

            progress = (f32)self->pincerFlightTimer / (f32)ENSBEETLE_PINCER_FAST_RETURN_TIME;

            if (progress > 1.0f) {
                progress = 1.0f;
            }

            EnSbeetle_GetPincerPath(&self->pincerLReturnStart, &self->pincerLHomePos, progress, 0.2f,
                                    &self->pincerLWorldPos);

            EnSbeetle_GetPincerPath(&self->pincerRReturnStart, &self->pincerRHomePos, progress, -0.2f,
                                    &self->pincerRWorldPos);

            self->pincerFlightTimer++;

            if (self->pincerFlightTimer >= ENSBEETLE_PINCER_FAST_RETURN_TIME) {
                self->pincerState = ENSBEETLE_PINCER_ATTACHED;
                self->pincerFlightTimer = 0;
                self->pincerLSpin = 0;
                self->pincerRSpin = 0;

                self->pincerLCollider.base.atFlags &= ~AT_HIT;
                self->pincerRCollider.base.atFlags &= ~AT_HIT;
            }
            break;

        default:
            break;
    }
}

s32 EnSbeetle_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    EnSbeetle* self = (EnSbeetle*)thisx;
    Vec3f originalPos;

    switch (limbIndex) {
        case GSCISSORSBEETLESKEL_PINCER_L_LIMB:
            originalPos = *pos;
            Matrix_MultVec3f(&originalPos, &self->pincerLHomePos);

            if ((self->pincerState == ENSBEETLE_PINCER_OUTBOUND) || (self->pincerState == ENSBEETLE_PINCER_RETURN) ||
                (self->pincerState == ENSBEETLE_PINCER_FAST_RETURN)) {

                if ((self->pincerState == ENSBEETLE_PINCER_OUTBOUND) && (self->pincerFlightTimer <= 1)) {
                    self->pincerLWorldPos = self->pincerLHomePos;
                }

                EnSbeetle_WorldToCurrentMatrixLocal(&self->pincerLWorldPos, pos);

                rot->y += self->pincerLSpin;
            }
            break;

        case GSCISSORSBEETLESKEL_PINCER_R_LIMB:
            originalPos = *pos;
            Matrix_MultVec3f(&originalPos, &self->pincerRHomePos);

            if ((self->pincerState == ENSBEETLE_PINCER_OUTBOUND) || (self->pincerState == ENSBEETLE_PINCER_RETURN) ||
                (self->pincerState == ENSBEETLE_PINCER_FAST_RETURN)) {

                if ((self->pincerState == ENSBEETLE_PINCER_OUTBOUND) && (self->pincerFlightTimer <= 1)) {
                    self->pincerRWorldPos = self->pincerRHomePos;
                }

                EnSbeetle_WorldToCurrentMatrixLocal(&self->pincerRWorldPos, pos);

                rot->y += self->pincerRSpin;
            }
            break;
    }

    return false;
}

void EnSbeetle_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
    EnSbeetle* self = (EnSbeetle*)thisx;
    MtxF mtx;

    Matrix_Get(&mtx);

    switch (limbIndex) {
        case GSCISSORSBEETLESKEL_BODYFRONT_LIMB:
            Matrix_MultVec3f(&sZeroVec, &self->actor.focus.pos);
            break;

        case GSCISSORSBEETLESKEL_PINCER_L_LIMB:
            if ((self->pincerState != ENSBEETLE_PINCER_OUTBOUND) && (self->pincerState != ENSBEETLE_PINCER_RETURN)) {
                self->pincerLCollider.dim.pos.x = mtx.xw;
                self->pincerLCollider.dim.pos.y = mtx.yw;
                self->pincerLCollider.dim.pos.z = mtx.zw;
            }
            break;

        case GSCISSORSBEETLESKEL_PINCER_R_LIMB:
            if ((self->pincerState != ENSBEETLE_PINCER_OUTBOUND) && (self->pincerState != ENSBEETLE_PINCER_RETURN)) {
                self->pincerRCollider.dim.pos.x = mtx.xw;
                self->pincerRCollider.dim.pos.y = mtx.yw;
                self->pincerRCollider.dim.pos.z = mtx.zw;
            }
            break;
    }
}

void EnSbeetle_CheckHurt(EnSbeetle* self, PlayState* play) {
    if (self->collider.base.acFlags & AC_HIT) {
        self->collider.base.acFlags &= ~AC_HIT;
        Actor_SetDropFlag(&self->actor, &self->collider.elem);
        self->actor.speed = 0.0f;

        switch (self->actor.colChkInfo.damageEffect) {
            case ENSBEETLE_DMGEFF_STUN:
                Actor_SetColorFilter(&self->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 60);
                Actor_ApplyDamage(&self->actor);
                EnSbeetle_SetupStunned(self, play);
                break;
            case ENSBEETLE_DMGEFF_ICE_MAGIC:
                Actor_SetColorFilter(&self->actor, COLORFILTER_COLORFLAG_BLUE, 255, COLORFILTER_BUFFLAG_OPA, 80);
                self->spawnIceTimer = 48;
                self->frozen = true;
                EnSbeetle_SetupStunned(self, play);
                break;
            case ENSBEETLE_DMGEFF_FIRE:
                Actor_PlaySfx(&self->actor, NA_SE_EV_FLAME_OF_FIRE);
                Actor_SetColorFilter(&self->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 80);
                self->fireTimer = 80;
                EnSbeetle_SetupDie(self, play);
                break;
            case ENSBEETLE_DMGEFF_NONE:
            default:
                if (self->actionFunc != EnSbeetle_ThreatPlayer) {
                    EnSbeetle_SetupHurt(self, play);
                }
                break;
        }
        if (self->actor.colChkInfo.health == 0) {
            EnSbeetle_SetupDie(self, play);
        }
    }
    if ((self->actor.bgCheckFlags & BGCHECKFLAG_WATER) && self->actionFunc != EnSbeetle_Die) {
        EnSbeetle_SetupDie(self, play);
    }
}

void EnSbeetle_UpdateBgCheck(EnSbeetle* self, PlayState* play) {
    Actor_UpdateBgCheckInfo(
        play, &self->actor, self->actor.colChkInfo.cylHeight, self->actor.colChkInfo.cylRadius,
        self->actor.colChkInfo.cylHeight,
        (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_8 | UPDBGCHECKINFO_FLAG_10));
}

#define ENSBEETLE_FORGET_DISTANCE 460.0f
#define ENSBEETLE_FORGET_HEIGHT 140.0f
#define ENSBEETLE_FORGET_TIME 60

s32 EnSbeetle_HasLostPlayer(EnSbeetle* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 yDist;

    yDist = player->actor.world.pos.y - self->actor.world.pos.y;

    if (self->actor.xzDistToPlayer > ENSBEETLE_FORGET_DISTANCE) {
        return true;
    }

    if (fabsf(yDist) > ENSBEETLE_FORGET_HEIGHT) {
        return true;
    }

    return false;
}

#define ENSBEETLE_HEARING_DISTANCE 200.0f
#define ENSBEETLE_FRONT_DISTANCE 460.0f
#define ENSBEETLE_FRONT_ANGLE 0x2000
#define ENSBEETLE_SIDE_DISTANCE 300.0f
#define ENSBEETLE_SIDE_ANGLE 0x5000
#define ENSBEETLE_DETECT_HEIGHT 80.0f

s32 EnSbeetle_CheckPlayerNear(EnSbeetle* self, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 yDist;
    f32 xzDist;
    s16 yawToPlayer;
    s16 yawDiff;
    s16 absYawDiff;

    xzDist = self->actor.xzDistToPlayer;

    if (xzDist > ENSBEETLE_FRONT_DISTANCE) {
        return false;
    }

    yDist = player->actor.world.pos.y - self->actor.world.pos.y;

    if (fabsf(yDist) > ENSBEETLE_DETECT_HEIGHT) {
        return false;
    }

    if (xzDist <= ENSBEETLE_HEARING_DISTANCE) {
        return true;
    }

    yawToPlayer = Math_Vec3f_Yaw(&self->actor.world.pos, &player->actor.world.pos);

    yawDiff = yawToPlayer - self->actor.shape.rot.y;
    absYawDiff = ABS(yawDiff);

    if ((absYawDiff <= ENSBEETLE_FRONT_ANGLE) && (xzDist <= ENSBEETLE_FRONT_DISTANCE)) {
        return true;
    }

    if ((absYawDiff <= ENSBEETLE_SIDE_ANGLE) && (xzDist <= ENSBEETLE_SIDE_DISTANCE)) {
        return true;
    }

    return false;
}

void EnSbeetle_SetupDoNothing(EnSbeetle* self, PlayState* play) {
    self->actor.speed = 0.0f;
    self->nextIdleTimer = 100;
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE1);
    self->actionFunc = EnSbeetle_DoNothing;
}

void EnSbeetle_DoNothing(EnSbeetle* self, PlayState* play) {
    if (EnSbeetle_CheckPlayerNear(self, play) == true) {
        self->idleAction = NULL;
        EnSbeetle_SetupThreatPlayer(self, play);
    }

    if (self->idleAction == NULL) {
        SkelAnime_Update(&self->skelAnime);
        if (DECR(self->nextIdleTimer) == 0) {
            self->nextIdleTimer = Rand_S16Offset(40, 40);
            if (Rand_ZeroOne() > 0.4f) {
                f32 randomIdle = Rand_ZeroOne();
                if (randomIdle <= 0.3f) {
                    self->afterAnimTimer = 10;
                    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE2);
                    self->idleAction = EnSbeetle_IdleActionIdle2;
                } else if (randomIdle >= 0.4f && randomIdle <= 0.7f) {
                    self->afterAnimTimer = 10;
                    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE3);
                    self->idleAction = EnSbeetle_IdleActionIdle3;
                } else {
                    self->actor.speed = 1.0f;
                    self->randomWalkTimer = Rand_S16Offset(60, 40);
                    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_WALK);
                    self->idleAction = EnSbeetle_IdleActionWalk;
                }
            }
        }
    } else {
        self->idleAction(self, play);
    }
}

void EnSbeetle_IdleActionWalk(EnSbeetle* self, PlayState* play) {
    f32 distToHome = Math_Vec3f_DistXZ(&self->actor.world.pos, &self->actor.home.pos);

    SkelAnime_Update(&self->skelAnime);

    if (distToHome > 300.0f) {
        Math_ApproachS(&self->actor.world.rot.y, Math_Vec3f_Yaw(&self->actor.world.pos, &self->actor.home.pos), 3,
                       4000);
    } else {
        Math_ApproachS(&self->actor.world.rot.y, Rand_S16Offset(self->actor.world.rot.y, 0x400), 3, 4000);
    }
    Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 6000);

    if (Animation_OnFrame(&self->skelAnime, 10.0f) || Animation_OnFrame(&self->skelAnime, 17.0f)) {
        Actor_PlaySfx(&self->actor, NA_SE_EN_TEKU_WALK);
    }

    if (DECR(self->randomWalkTimer) == 0) {
        self->actor.speed = 0.0f;
        EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE1);
        self->idleAction = NULL;
    }
}

void EnSbeetle_IdleActionIdle2(EnSbeetle* self, PlayState* play) {
    if (Animation_OnFrame(&self->skelAnime, 7.0f)) {
        Actor_PlaySfx(&self->actor, NA_SE_EN_TUBOOCK_FLY);
    }
    if (Animation_OnFrame(&self->skelAnime, 30.0f)) {
        AudioSfx_StopById(NA_SE_EN_TUBOOCK_FLY);
    }

    if (SkelAnime_Update(&self->skelAnime)) {
        if (DECR(self->afterAnimTimer) == 0) {
            EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE1);
            self->idleAction = NULL;
        }
    }
}

void EnSbeetle_IdleActionIdle3(EnSbeetle* self, PlayState* play) {
    if (Animation_OnFrame(&self->skelAnime, 7.0f) || Animation_OnFrame(&self->skelAnime, 39.0f)) {
        Actor_PlaySfx(&self->actor, NA_SE_EN_TEKU_WALK);
    }

    if (SkelAnime_Update(&self->skelAnime)) {
        if (DECR(self->afterAnimTimer) == 0) {
            EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE1);
            self->idleAction = NULL;
        }
    }
}

void EnSbeetle_SetupHopWithPlayerRot(EnSbeetle* self, PlayState* play) {
    self->actor.speed = 0.0f;
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_HOP);
    self->actionFunc = EnSbeetle_HopWithPlayerRot;
}

void EnSbeetle_HopWithPlayerRot(EnSbeetle* self, PlayState* play) {
    if (self->skelAnime.curFrame >= 7.0f) {
        Math_ApproachS(&self->actor.world.rot.y, self->actor.yawTowardsPlayer, 3, 4000);
        Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 6000);
    }
    if (SkelAnime_Update(&self->skelAnime)) {
        EnSbeetle_SetupThreatPlayer(self, play);
    }
}

void EnSbeetle_SetupThreatPlayer(EnSbeetle* self, PlayState* play) {
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_IDLE1);
    self->attackTimer = 40;
    self->playerLostTimer = ENSBEETLE_FORGET_TIME;
    self->actionFunc = EnSbeetle_ThreatPlayer;
}

void EnSbeetle_ThreatPlayer(EnSbeetle* self, PlayState* play) {
    SkelAnime_Update(&self->skelAnime);

    if (!EnSbeetle_HasLostPlayer(self, play)) {
        self->playerLostTimer = ENSBEETLE_FORGET_TIME;
    } else if (DECR(self->playerLostTimer) == 0) {
        self->actor.speed = 0.0f;
        EnSbeetle_SetupDoNothing(self, play);
        return;
    }

    Math_ApproachS(&self->actor.world.rot.y, self->actor.yawTowardsPlayer, 3, 3000);
    Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 5000);

    if (DECR(self->attackTimer) == 0) {
        if (Rand_ZeroOne() < 0.6f) {
            if (Rand_ZeroOne() < 0.6f) {
                EnSbeetle_SetupAttack(self, play);
            } else {
                EnSbeetle_SetupSwingAttack(self, play);
            }
            return;
        }
        self->attackTimer = 40;
    }
}

void EnSbeetle_SetupAttack(EnSbeetle* self, PlayState* play) {
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_ATTACK);
    self->actor.speed = 0.0f;
    self->pincerLCollider.elem.atDmgInfo.damage = 0x10;
    self->audioPlayed = false;
    self->actionFunc = EnSbeetle_Attack;
}

void EnSbeetle_Attack(EnSbeetle* self, PlayState* play) {
    CollisionCheck_SetAT(play, &play->colChkCtx, &self->pincerLCollider.base);

    if (self->skelAnime.curFrame < 20.0f) {
        Math_ApproachS(&self->actor.world.rot.y, self->actor.yawTowardsPlayer, 3, 3000);
        Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 5000);
    }

    if (Animation_OnFrame(&self->skelAnime, 24.0f) || Animation_OnFrame(&self->skelAnime, 25.0f)) {
        self->actor.speed = self->actor.xzDistToPlayer / 2;
        if (!self->audioPlayed) {
            Actor_PlaySfx(&self->actor, NA_SE_IT_SWORD_SWING_HARD);
            self->audioPlayed = true;
        }
    } else {
        self->actor.speed = 0.0f;
    }

    if (SkelAnime_Update(&self->skelAnime)) {
        EnSbeetle_SetupHopAwayFromOrTowardsPlayer(self, play);
    }
}

void EnSbeetle_SetupSwingAttack(EnSbeetle* self, PlayState* play) {
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_SWING);

    self->actor.speed = 0.0f;
    self->pincerLCollider.elem.atDmgInfo.damage = 0x08;

    self->pincerState = ENSBEETLE_PINCER_WINDUP;
    self->pincerFlightTimer = 0;

    self->pincerLSpin = 0;
    self->pincerRSpin = 0;

    self->pincerLCollider.base.atFlags &= ~AT_HIT;
    self->pincerRCollider.base.atFlags &= ~AT_HIT;

    self->actionFunc = EnSbeetle_SwingAttack;
}

void EnSbeetle_ThrowPincers(EnSbeetle* self, PlayState* play) {
    Player* player = GET_PLAYER(play);

    self->pincerLWorldPos = self->pincerLHomePos;
    self->pincerRWorldPos = self->pincerRHomePos;

    self->pincerTargetPos = player->actor.world.pos;
    self->pincerTargetPos.y += 30.0f;

    self->pincerFlightTimer = 0;
    self->pincerLSpin = 0;
    self->pincerRSpin = 0;
    self->pincerState = ENSBEETLE_PINCER_OUTBOUND;

    self->pincerLCollider.base.atFlags &= ~AT_HIT;
    self->pincerRCollider.base.atFlags &= ~AT_HIT;

    Actor_PlaySfx(&self->actor, NA_SE_IT_BOOMERANG_THROW);
}

void EnSbeetle_SwingAttack(EnSbeetle* self, PlayState* play) {
    s32 animationFinished;

    self->actor.speed = 0.0f;

    if (self->pincerState == ENSBEETLE_PINCER_WINDUP) {
        Math_ApproachS(&self->actor.world.rot.y, self->actor.yawTowardsPlayer, 3, 3000);

        Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 5000);
    }

    animationFinished = SkelAnime_Update(&self->skelAnime);

    if ((self->pincerState == ENSBEETLE_PINCER_WINDUP) &&
        Animation_OnFrame(&self->skelAnime, ENSBEETLE_PINCER_THROW_FRAME)) {
        EnSbeetle_ThrowPincers(self, play);
    }

    if (animationFinished && (self->pincerState == ENSBEETLE_PINCER_ATTACHED)) {
        EnSbeetle_SetupHopAwayFromOrTowardsPlayer(self, play);
    }
}

void EnSbeetle_SetupHopAwayFromOrTowardsPlayer(EnSbeetle* self, PlayState* play) {
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_HOP);
    self->audioPlayed = false;
    self->playerDistAtSetup = self->actor.xzDistToPlayer;
    self->actionFunc = EnSbeetle_HopAwayFromOrTowardsPlayer;
}

void EnSbeetle_HopAwayFromOrTowardsPlayer(EnSbeetle* self, PlayState* play) {
    if (self->skelAnime.curFrame <= 6.0f || self->skelAnime.curFrame >= 15.0f) {
        self->actor.speed = 0.0f;
        Math_ApproachS(&self->actor.world.rot.y, self->actor.yawTowardsPlayer, 3, 4000);
        Math_ApproachS(&self->actor.shape.rot.y, self->actor.world.rot.y, 2, 6000);
    } else {
        if (!self->audioPlayed) {
            Actor_PlaySfx(&self->actor, NA_SE_EN_RIZA_JUMP);
            self->audioPlayed = true;
        }
        if (self->playerDistAtSetup > 300.0f) {
            self->actor.speed = 12.0f;
        } else {
            self->actor.speed = -12.0f;
        }
    }

    if (SkelAnime_Update(&self->skelAnime)) {
        if (EnSbeetle_HasLostPlayer(self, play)) {
            EnSbeetle_SetupDoNothing(self, play);
        } else {
            EnSbeetle_SetupThreatPlayer(self, play);
        }
    }
}

void EnSbeetle_SetupStunned(EnSbeetle* self, PlayState* play) {
    EnSbeetle_StartPincerFastReturn(self);
    self->actor.speed = 0.0f;
    Actor_PlaySfx(&self->actor, NA_SE_EN_COMMON_FREEZE);
    Animation_PlayOnceSetSpeed(&self->skelAnime, &gScissorsBeetleSkelIdle1Anim, 0.0f);
    Actor_SetColorFilter(&self->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 60);
    self->actionFunc = EnSbeetle_Stunned;
}

void EnSbeetle_Stunned(EnSbeetle* self, PlayState* play) {
    if (self->spawnIceTimer == 0) {
        if (self->actor.colorFilterTimer == 0) {
            EnSbeetle_SetupHopAwayFromOrTowardsPlayer(self, play);
            if (self->frozen) {
                Actor_PlaySfx(&self->actor, NA_SE_EV_ICE_BROKEN);
                self->frozen = false;
            }
        }
    }
}

void EnSbeetle_SetupHurt(EnSbeetle* self, PlayState* play) {
    EnSbeetle_StartPincerFastReturn(self);
    self->damageTimer = 2;
    self->hurtboxCooldown = 40;
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_HURT);
    Actor_SetColorFilter(&self->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
    Actor_ApplyDamage(&self->actor);
    Actor_PlaySfx(&self->actor, NA_SE_EN_BUBLEWALK_AIM);
    self->actionFunc = EnSbeetle_Hurt;
}

void EnSbeetle_Hurt(EnSbeetle* self, PlayState* play) {
    if (SkelAnime_Update(&self->skelAnime)) {
        if (DECR(self->damageTimer) == 0) {
            EnSbeetle_SetupHopAwayFromOrTowardsPlayer(self, play);
        }
    }
}

void EnSbeetle_SetupDie(EnSbeetle* self, PlayState* play) {
    EnSbeetle_StartPincerFastReturn(self);
    self->deathFreeze = 12;
    self->actor.speed = 0.0f;
    self->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    self->actor.shape.shadowAlpha = 0;
    Actor_PlaySfx(&self->actor, NA_SE_EN_BUBLEWALK_DEAD);
    Enemy_StartFinishingBlow(play, &self->actor);
    EnSbeetle_ChangeAnimation(self, SCISSORSBEETLE_ANIMATION_DIE);
    self->actionFunc = EnSbeetle_Die;
}

void EnSbeetle_Die(EnSbeetle* self, PlayState* play) {
    static Color_RGBA8 sDeathPrimColor = { 255, 255, 255, 255 };
    static Color_RGBA8 sDeathEnvColor = { 0, 0, 255, 255 };
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    Vec3f effectVel = { 0.0f, 4.0f, 0.0f };
    Vec3f effectPos = self->actor.world.pos;

    if (SkelAnime_Update(&self->skelAnime)) {
        if (DECR(self->deathFreeze) == 0) {
            Math_StepToF(&self->actor.scale.x, 0.0f, 0.0084f);
            self->actor.scale.y = self->actor.scale.z = self->actor.scale.x;
            if (self->actor.scale.x <= 0.001f) {
                effectPos.y += 10.0f;
                EffectSsDeadDb_Spawn(play, &effectPos, &effectVel, &zeroVec, &sDeathPrimColor, &sDeathEnvColor, 90, 0,
                                     9);
                Item_DropCollectibleRandom(play, &self->actor, &self->actor.world.pos, 0xE0);
                Actor_Kill(&self->actor);
            }
        }
    }
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterActor)) {
        return;
    }
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = SBEETLE_KEY;
    definition.description = "Scissors Beetle";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = SBEETLE_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnSbeetle);
    definition.init = EnSbeetle_Init;
    definition.destroy = EnSbeetle_Destroy;
    definition.update = EnSbeetle_Update;
    definition.draw = EnSbeetle_Draw;

    if (sApi->RegisterActor(&definition) < 0) {
        sApi->Log("Could not register " SBEETLE_KEY);
    }
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    static const S2HModRequirements requirements = { sizeof(S2HModRequirements), NULL, 0 };
    return &requirements;
}
