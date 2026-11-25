/*
 * Ocarina of Time's Spike (En_Ny) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * It rolls at the player with its spikes out, then turns to stone; only the stone shell can be hurt.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"

#define SPIKE_KEY "oot.spike"
#define SPIKE_SOUL_KEY "soul.oot.spike"
#define NY_OBJECT "objects/oot_object_ny/"
#define NY_ASSET(name) "__OTR__" NY_OBJECT name

static const ALIGN_ASSET(2) char sMetalBodyDL[] = NY_ASSET("gEnNyMetalBodyDL");
static const ALIGN_ASSET(2) char sRockBodyDL[] = NY_ASSET("gEnNyRockBodyDL");
static const ALIGN_ASSET(2) char sSpikeDL[] = NY_ASSET("gEnNySpikeDL");

// MM reuses every Spike sfx id for other actors; these are MM stand-ins.
#define SPIKE_SFX_ROLL NA_SE_EN_PAMET_ROLL
#define SPIKE_SFX_HARDEN NA_SE_EN_PAMET_CUTTER_OFF
#define SPIKE_SFX_LAND NA_SE_EN_FALL_LAND
#define SPIKE_SFX_DEAD NA_SE_EN_B_PAMET_BREAK

#define SPIKE_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE)
#define SPIKE_BGCHECK_FLAGS (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_4)
#define SPIKE_HEALTH 2
#define SPIKE_GRAVITY -0.4f
#define SPIKE_RETRACTED 0.25f
#define SPIKE_WAKE_RANGE_SQ 25600.0f
#define SPIKE_DEBRIS_COUNT 8
#define SPIKE_HURT_FILTER_DURATION 80

typedef enum {
    SPIKE_DMGEFF_NONE = 0x0,
    SPIKE_DMGEFF_STUN = 0x1,
    SPIKE_DMGEFF_FIRE = 0x2,
    SPIKE_DMGEFF_HIT = 0xF,
} SpikeDamageEffect;

static const S2HModApi* sApi;

struct EnNy;
typedef void (*EnNyActionFunc)(struct EnNy*, PlayState*);

typedef struct EnNy {
    Actor actor;
    EnNyActionFunc actionFunc;
    ColliderJntSph collider;
    ColliderJntSphElement elements[1];
    Vec3f debrisPos[SPIKE_DEBRIS_COUNT];
    Vec3f debrisVel[SPIKE_DEBRIS_COUNT];
    f32 spikeScale;
    f32 rollSpeed;
    f32 targetRollSpeed;
    f32 swimVelocityY;
    f32 floatHeight;
    f32 turnStep;
    s32 rockAlpha;
    s32 metalAlpha;
    s32 stoneTimer;
    s16 timer;
    s16 burnTimer;
    s16 hitPlayer;
    u32 epoch;
} EnNy;

static ColliderJntSphElementInit sJntSphElementsInit[1] = {
    {
        {
            ELEM_MATERIAL_UNK0,
            { 0xF7CFFFFF, 0x04, 0x08 },
            { 0xF7CFFFFF, 0x00, 0x00 },
            ATELEM_ON | ATELEM_SFX_NORMAL,
            ACELEM_ON,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 15 }, 100 },
    },
};

static ColliderJntSphInit sColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_JNTSPH,
    },
    1,
    sJntSphElementsInit,
};

// Vanilla's Kokiri Sword and sticks bounce off; MM's one sword type takes the sword row.
static DamageTable sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* Deku Stick     */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* Horse trample  */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(2, SPIKE_DMGEFF_HIT),
    /* Zora boomerang */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* Normal arrow   */ DMG_ENTRY(2, SPIKE_DMGEFF_HIT),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(2, SPIKE_DMGEFF_STUN),
    /* Goron punch    */ DMG_ENTRY(2, SPIKE_DMGEFF_HIT),
    /* Sword          */ DMG_ENTRY(1, SPIKE_DMGEFF_HIT),
    /* Goron pound    */ DMG_ENTRY(2, SPIKE_DMGEFF_HIT),
    /* Fire arrow     */ DMG_ENTRY(2, SPIKE_DMGEFF_FIRE),
    /* Ice arrow      */ DMG_ENTRY(2, SPIKE_DMGEFF_HIT),
    /* Light arrow    */ DMG_ENTRY(2, SPIKE_DMGEFF_HIT),
    /* Goron spikes   */ DMG_ENTRY(2, SPIKE_DMGEFF_HIT),
    /* Deku spin      */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* Deku bubble    */ DMG_ENTRY(1, SPIKE_DMGEFF_HIT),
    /* Deku launch    */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* Zora barrier   */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* Normal shield  */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(1, SPIKE_DMGEFF_HIT),
    /* Zora punch     */ DMG_ENTRY(1, SPIKE_DMGEFF_HIT),
    /* Spin attack    */ DMG_ENTRY(1, SPIKE_DMGEFF_HIT),
    /* Sword beam     */ DMG_ENTRY(1, SPIKE_DMGEFF_HIT),
    /* Normal Roll    */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, SPIKE_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(2, SPIKE_DMGEFF_HIT),
};

static void EnNy_Move(EnNy* this, PlayState* play);
static void EnNy_UpdateDeath(Actor* thisx, PlayState* play);
static void EnNy_DrawDeath(Actor* thisx, PlayState* play);

static void EnNy_UpdateRollSpeed(EnNy* this) {
    this->targetRollSpeed = 2.8f * ((this->actor.depthInWater > 0.0f) ? 0.7f : 1.0f);
}

static void EnNy_SetupMove(EnNy* this) {
    this->turnStep = 0.0f;
    EnNy_UpdateRollSpeed(this);
    this->stoneTimer = 180;
    this->actionFunc = EnNy_Move;
}

static void EnNy_ExtendSpikes(EnNy* this, PlayState* play) {
    this->spikeScale = MIN(this->spikeScale + 2.0f, 1.0f);
    if (this->spikeScale >= 1.0f) {
        EnNy_SetupMove(this);
    }
}

static void EnNy_TurnToMetal(EnNy* this, PlayState* play) {
    this->rockAlpha -= 0x40;
    this->metalAlpha += 0x40;
    if (this->metalAlpha >= 0xFF) {
        this->rockAlpha = 0;
        this->metalAlpha = 0xFF;
        this->actionFunc = EnNy_ExtendSpikes;
    }
}

static void EnNy_WaitForPlayer(EnNy* this, PlayState* play) {
    if (this->actor.xyzDistToPlayerSq <= SPIKE_WAKE_RANGE_SQ) {
        this->stoneTimer = 20;
        this->actionFunc = EnNy_TurnToMetal;
    }
}

static void EnNy_TurnToStone(EnNy* this, PlayState* play) {
    this->rockAlpha += 0x40;
    this->metalAlpha -= 0x40;
    if (this->rockAlpha < 0xFF) {
        return;
    }
    this->rockAlpha = 0xFF;
    this->metalAlpha = 0;
    if (this->stoneTimer != 0) {
        this->stoneTimer--;
    } else {
        this->actionFunc = EnNy_WaitForPlayer;
    }
}

static void EnNy_RetractSpikes(EnNy* this, PlayState* play) {
    this->spikeScale = MAX(this->spikeScale - 2.0f, SPIKE_RETRACTED);
    if ((this->spikeScale > SPIKE_RETRACTED) || !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH)) {
        return;
    }
    if (this->floatHeight >= this->actor.depthInWater) {
        Actor_PlaySfx(&this->actor, SPIKE_SFX_LAND);
    }
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND_TOUCH;
    this->actor.speed = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->stoneTimer = 60;
    this->actionFunc = EnNy_TurnToStone;
}

static void EnNy_SetupRetractSpikes(EnNy* this) {
    Actor_PlaySfx(&this->actor, SPIKE_SFX_HARDEN);
    this->actionFunc = EnNy_RetractSpikes;
    this->targetRollSpeed = 0.0f;
}

static void EnNy_Move(EnNy* this, PlayState* play) {
    bool isUnderwater = this->floatHeight < this->actor.depthInWater;

    if (!isUnderwater) {
        Actor_PlaySfx_Flagged(&this->actor, SPIKE_SFX_ROLL - SFX_FLAG);
    }
    EnNy_UpdateRollSpeed(this);
    if ((this->stoneTimer-- <= 0) || this->hitPlayer) {
        EnNy_SetupRetractSpikes(this);
        return;
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0xA, this->turnStep, 0);
    Math_ApproachF(&this->turnStep, 2000.0f, 1.0f, 100.0f);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    f32 pitchToPlayer = Math_FAtan2F(this->actor.playerHeightRel, this->actor.xzDistToPlayer);
    this->actor.speed = fabsf(cosf(pitchToPlayer) * this->targetRollSpeed);
    if (isUnderwater) {
        this->swimVelocityY = sinf(pitchToPlayer) * this->targetRollSpeed;
    }
}

static void EnNy_ApplyStoneHit(EnNy* this) {
    switch (this->actor.colChkInfo.damageEffect) {
        case SPIKE_DMGEFF_FIRE:
            this->burnTimer = 4;
            break;
        case SPIKE_DMGEFF_STUN:
        case SPIKE_DMGEFF_HIT:
            break;
        default:
            return;
    }
    Actor_ApplyDamage(&this->actor);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_XLU,
                         SPIKE_HURT_FILTER_DURATION);
}

// Returns true once a blow kills it.
static bool EnNy_CheckCollision(EnNy* this, PlayState* play) {
    this->hitPlayer = false;
    if (this->collider.base.atFlags & AT_BOUNCED) {
        this->collider.base.atFlags &= ~AT_BOUNCED;
        this->hitPlayer = true;
        this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        this->actor.speed = -4.0f;
        return false;
    }
    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~AT_HIT;
        this->hitPlayer = true;
        return false;
    }
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return false;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    Vec3f hitPos = { this->collider.elements[0].base.acDmgInfo.hitPos.x,
                     this->collider.elements[0].base.acDmgInfo.hitPos.y,
                     this->collider.elements[0].base.acDmgInfo.hitPos.z };
    bool isStone = (this->spikeScale == SPIKE_RETRACTED) && (this->rockAlpha == 0xFF);
    if (isStone) {
        EnNy_ApplyStoneHit(this);
    }
    this->stoneTimer = 0;
    if (this->actor.colChkInfo.health == 0) {
        this->actor.shape.shadowAlpha = 0;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        Enemy_StartFinishingBlow(play, &this->actor);
        return true;
    }
    EffectSsHitmark_SpawnFixedScale(play, 0, &hitPos);
    return false;
}

// Under water it swims towards the player's height instead of falling.
static void EnNy_UpdateSwim(EnNy* this, f32 surfaceDepth, f32 swimDepth) {
    if ((this->targetRollSpeed == 0.0f) || (surfaceDepth >= this->actor.depthInWater)) {
        this->actor.gravity = SPIKE_GRAVITY;
        return;
    }
    if (swimDepth >= this->actor.depthInWater) {
        return;
    }
    this->actor.gravity = 0.0f;
    if (this->swimVelocityY < this->actor.velocity.y) {
        this->actor.velocity.y = MAX(this->actor.velocity.y - 0.4f, this->swimVelocityY);
    } else if (this->actor.velocity.y < this->swimVelocityY) {
        this->actor.velocity.y = MIN(this->actor.velocity.y + 0.4f, this->swimVelocityY);
    }
}

static void EnNy_Die(EnNy* this, PlayState* play) {
    bool isUnderwater = this->actor.depthInWater > 0.0f;
    f32 xzDrag = isUnderwater ? 0.1f : 0.15f;
    f32 fallAccel = isUnderwater ? 0.4f : 0.6f;

    for (s32 i = 0; i < SPIKE_DEBRIS_COUNT; i++) {
        this->debrisPos[i].x += this->debrisVel[i].x;
        this->debrisPos[i].y += this->debrisVel[i].y;
        this->debrisPos[i].z += this->debrisVel[i].z;
        Math_StepToF(&this->debrisVel[i].x, 0.0f, xzDrag);
        Math_StepToF(&this->debrisVel[i].y, -1.0f, fallAccel);
        Math_StepToF(&this->debrisVel[i].z, 0.0f, xzDrag);
    }
    if (this->timer >= (isUnderwater ? 0x1F : 0x10)) {
        Actor_Kill(&this->actor);
    }
}

static void EnNy_SpawnUnderwaterBubbles(EnNy* this, PlayState* play) {
    Vec3f velocity = { 0.0f, 0.0f, 0.0f };
    Vec3f accel = { 0.0f, 0.1f, 0.0f };

    for (s32 i = 0; i < 10; i++) {
        Vec3f pos = { Rand_CenteredFloat(30.0f) + this->actor.world.pos.x,
                      Rand_CenteredFloat(30.0f) + this->actor.world.pos.y,
                      Rand_CenteredFloat(30.0f) + this->actor.world.pos.z };
        EffectSsDtBubble_SpawnColorProfile(play, &pos, &velocity, &accel, Rand_S16Offset(0x50, 0x64), 25, 0, 1);
    }
    for (s32 i = 0; i < 20; i++) {
        Vec3f pos = { Rand_CenteredFloat(30.0f) + this->actor.world.pos.x,
                      Rand_CenteredFloat(30.0f) + this->actor.world.pos.y,
                      Rand_CenteredFloat(30.0f) + this->actor.world.pos.z };
        EffectSsBubble_Spawn(play, &pos, 10.0f, 10.0f, 30.0f, 0.25f);
    }
}

static void EnNy_Shatter(EnNy* this, PlayState* play) {
    if (this->timer < 2) {
        return;
    }
    if (this->actor.depthInWater > 0.0f) {
        EnNy_SpawnUnderwaterBubbles(this, play);
    }
    for (s32 i = 0; i < SPIKE_DEBRIS_COUNT; i++) {
        this->debrisVel[i].x = Rand_CenteredFloat(10.0f);
        this->debrisVel[i].z = Rand_CenteredFloat(10.0f);
        this->debrisVel[i].y = Rand_ZeroFloat(4.0f) + 4.0f;
    }
    this->timer = 0;
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xA0);
    Actor_PlaySfx(&this->actor, SPIKE_SFX_DEAD);
    this->actionFunc = EnNy_Die;
}

static void EnNy_StartDeath(EnNy* this) {
    for (s32 i = 0; i < SPIKE_DEBRIS_COUNT; i++) {
        this->debrisPos[i].x = Rand_CenteredFloat(20.0f) + this->actor.world.pos.x;
        this->debrisPos[i].y = Rand_CenteredFloat(20.0f) + this->actor.world.pos.y;
        this->debrisPos[i].z = Rand_CenteredFloat(20.0f) + this->actor.world.pos.z;
    }
    this->timer = 0;
    this->actor.update = EnNy_UpdateDeath;
    this->actor.draw = EnNy_DrawDeath;
    this->actionFunc = EnNy_Shatter;
}

static void EnNy_Update(Actor* thisx, PlayState* play) {
    EnNy* this = (EnNy*)thisx;
    f32 spikeGrowth = this->spikeScale - SPIKE_RETRACTED;

    this->timer++;
    if (this->burnTimer != 0) {
        this->burnTimer--;
    }
    Actor_SetFocus(&this->actor, 0.0f);
    Actor_SetScale(&this->actor, 0.01f);
    this->collider.elements[0].dim.scale = 1.33f * spikeGrowth + 1.0f;
    // Its origin is its centre; the floor checks run from the bottom of the shell.
    f32 radius = (24.0f * spikeGrowth) + 12.0f;
    this->actor.shape.rot.x += (s16)(this->targetRollSpeed * 1000.0f);
    EnNy_UpdateSwim(this, radius + 10.0f, radius - 10.0f);
    Actor_MoveWithGravity(&this->actor);
    Math_StepToF(&this->rollSpeed, this->targetRollSpeed, 0.1f);
    this->actionFunc(this, play);
    this->actor.prevPos.y -= radius;
    this->actor.world.pos.y -= radius;
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 20.0f, 60.0f, SPIKE_BGCHECK_FLAGS);
    this->floatHeight = radius;
    this->actor.world.pos.y += radius;
    if (EnNy_CheckCollision(this, play)) {
        EnNy_StartDeath(this);
        return;
    }
    if (this->spikeScale > SPIKE_RETRACTED) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void EnNy_UpdateDeath(Actor* thisx, PlayState* play) {
    EnNy* this = (EnNy*)thisx;

    this->timer++;
    if (this->burnTimer != 0) {
        this->burnTimer--;
    }
    this->actionFunc(this, play);
}

static void EnNy_Init(Actor* thisx, PlayState* play) {
    EnNy* this = (EnNy*)thisx;

    this->epoch++;
    this->actor.attentionRangeType = ATTENTION_RANGE_2;
    this->actor.lockOnArrowOffset = 30.0f;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    this->actor.colChkInfo.health = SPIKE_HEALTH;
    this->actor.colChkInfo.mass = 0;
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sColliderInit, this->elements);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 20.0f);
    Actor_SetScale(&this->actor, 0.01f);
    this->actor.speed = 0.0f;
    this->actor.shape.rot.y = 0;
    this->actor.gravity = SPIKE_GRAVITY;
    this->actor.velocity.y = 0.0f;
    this->rockAlpha = 0;
    this->metalAlpha = 0xFF;
    this->spikeScale = 1.0f;
    EnNy_SetupMove(this);
}

static void EnNy_Destroy(Actor* thisx, PlayState* play) {
    EnNy* this = (EnNy*)thisx;

    Collider_DestroyJntSph(play, &this->collider);
}

static void EnNy_SpawnBurnFlame(EnNy* this, PlayState* play, s16 offsetIndex) {
    static Vec3f sFireOffsets[] = {
        { 5.0f, 0.0f, 0.0f },
        { -5.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 5.0f },
        { 0.0f, 0.0f, -5.0f },
    };
    Vec3f* offset = &sFireOffsets[offsetIndex & 3];
    Vec3f pos = { Rand_CenteredFloat(5.0f) + this->actor.world.pos.x + offset->x,
                  Rand_CenteredFloat(5.0f) + this->actor.world.pos.y + offset->y,
                  Rand_CenteredFloat(5.0f) + this->actor.world.pos.z + offset->z };

    EffectSsEnFire_SpawnVec3f(play, &this->actor, &pos, 100, 0, 0, -1);
}

static void EnNy_Draw(Actor* thisx, PlayState* play) {
    EnNy* this = (EnNy*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Collider_UpdateSpheres(0, &this->collider);
    func_800B8118(&this->actor, play, 1);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, this->metalAlpha);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sMetalBodyDL);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, this->rockAlpha);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sRockBodyDL);
    if (this->spikeScale > SPIKE_RETRACTED) {
        Matrix_Scale(this->spikeScale, this->spikeScale, this->spikeScale, MTXMODE_APPLY);
        func_800B8050(&this->actor, play, 1);
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sSpikeDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    if (this->burnTimer == 0) {
        return;
    }
    this->actor.colorFilterTimer++;
    if ((this->burnTimer - 1) == 0) {
        EnNy_SpawnBurnFlame(this, play, 0);
    }
}

static void EnNy_DrawDeath(Actor* thisx, PlayState* play) {
    EnNy* this = (EnNy*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gDPSetEnvColor(POLY_OPA_DISP++, 0x00, 0x00, 0x00, 0xFF);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_SURF2);
    gDPPipeSync(POLY_OPA_DISP++);
    for (s32 i = 0; i < SPIKE_DEBRIS_COUNT; i++) {
        if (this->timer >= (i + 22)) {
            continue;
        }
        FrameInterpolation_RecordOpenChild(this, this->epoch + i * 25);
        f32 scale = this->actor.scale.x * 0.4f * (1.0f + (i * 0.04f));
        Matrix_Translate(this->debrisPos[i].x, this->debrisPos[i].y, this->debrisPos[i].z, MTXMODE_NEW);
        Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRockBodyDL);
        FrameInterpolation_RecordCloseChild();
    }
    CLOSE_DISPS(play->state.gfxCtx);
    if (this->burnTimer == 0) {
        return;
    }
    s16 offsetIndex = this->burnTimer - 1;
    this->actor.colorFilterTimer++;
    if ((offsetIndex & 1) == 0) {
        EnNy_SpawnBurnFlame(this, play, offsetIndex);
    }
}

// The soul shows the spiked rock turning, as Majora's Mask souls show their enemy.
static void DrawSoulModel(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Matrix_RotateYS((s16)(play->state.frames * 0x300), MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_OPA_SURF2);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sRockBodyDL);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sSpikeDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawSoul(PlayState* play, S2HGetItemEntry* entry) {
    u32 frames = play->state.frames;

    DrawSoulModel(play);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x10, 0x20, 1, (frames * 2) & 0x3F,
                                           (frames * -6) & 0x7F, 0x10, 0x20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0xC0, 0xC0, 180, 180, 200, 0);
    gDPSetEnvColor(POLY_XLU_DISP++, 180, 180, 200, 0);
    Matrix_Scale(0.13333333f, 0.13333333f, 0.13333333f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gameplay_keep_DL_01ACF0);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Ship (RE_SPIKE): a strong sword or the hammer (Goron here), sticks, explosives, hookshot or bow.
static bool CanKillSpike(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicHasCapability("CanBeGoron") ||
           sApi->LogicHasCapability("HasExplosives") || sApi->LogicCanUseItem(RI_HOOKSHOT) ||
           sApi->LogicCanUseItem(RI_BOW);
}

static void RegisterSoul(void) {
    static S2HCustomItemRandomizer sSoulLogic = { 0 };
    S2HCustomItemDefinition soul = { 0 };

    sSoulLogic.structSize = sizeof(sSoulLogic);
    sSoulLogic.flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
    sSoulLogic.type = S2H_CUSTOM_ITEM_TYPE_ITEM;
    sSoulLogic.logicKey = SPIKE_SOUL_KEY;
    sSoulLogic.hintKey = SPIKE_SOUL_KEY;
    soul.structSize = sizeof(soul);
    soul.key = SPIKE_SOUL_KEY;
    soul.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO;
    soul.getItemText = "You got the %rSoul of Spike%w!&They can be defeated now.";
    soul.getItemEntry.drawFunc = DrawSoul;
    soul.randomizer = &sSoulLogic;
    if (!sApi->RegisterCustomItem(&soul)) {
        sApi->Log("Could not register " SPIKE_SOUL_KEY);
    }
}

static s16 RegisterSpike(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = SPIKE_KEY;
    definition.description = "Spike";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = SPIKE_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnNy);
    definition.init = EnNy_Init;
    definition.destroy = EnNy_Destroy;
    definition.update = EnNy_Update;
    definition.draw = EnNy_Draw;
    definition.enemyFlags = S2H_ACTOR_ENEMY;
    definition.naviHint = "Spike&It rolls at you with its spikes out. Strike&when it hardens into stone!";
    definition.soulKey = SPIKE_KEY;
    definition.logicKill = CanKillSpike;
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
    if (!RequireOotAsset(NY_OBJECT "gEnNySpikeDL")) {
        sApi->Log(SPIKE_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    if (RegisterSpike() < 0) {
        sApi->Log("Could not register " SPIKE_KEY);
        return;
    }
    RegisterSoul();
}
