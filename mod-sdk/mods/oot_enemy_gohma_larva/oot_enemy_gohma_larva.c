/*
 * Ocarina of Time's Gohma Larva (En_Goma) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * It waits as an egg on the floor, hatches when the player comes near and leaps at them.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/effects/ovl_Effect_Ss_Hahen/z_eff_ss_hahen.h"

#define GOHMA_LARVA_KEY "oot.gohma_larva"
#define GOL_OBJECT "objects/oot_object_gol/"
#define GOL_ASSET(name) "__OTR__" GOL_OBJECT name

static const ALIGN_ASSET(2) char sSkel[] = GOL_ASSET("gObjectGolSkel");
static const ALIGN_ASSET(2) char sStandAnim[] = GOL_ASSET("gObjectGolStandAnim");
static const ALIGN_ASSET(2) char sRunningAnim[] = GOL_ASSET("gObjectGolRunningAnim");
static const ALIGN_ASSET(2) char sJumpHeadbuttAnim[] = GOL_ASSET("gObjectGolJumpHeadbuttAnim");
static const ALIGN_ASSET(2) char sDamagedAnim[] = GOL_ASSET("gObjectGolDamagedAnim");
static const ALIGN_ASSET(2) char sDeathAnim[] = GOL_ASSET("gObjectGolDeathAnim");
static const ALIGN_ASSET(2) char sDeadTwitchingAnim[] = GOL_ASSET("gObjectGolDeadTwitchingAnim");
static const ALIGN_ASSET(2) char sPrepareJumpAnim[] = GOL_ASSET("gObjectGolPrepareJumpAnim");
static const ALIGN_ASSET(2) char sLandFromJumpAnim[] = GOL_ASSET("gObjectGolLandFromJumpAnim");
static const ALIGN_ASSET(2) char sEggDL[] = GOL_ASSET("gObjectGolEggDL");

// MM reuses every Gohma Larva sfx id for other actors; these are MM stand-ins.
#define GOMA_SFX_CRY NA_SE_EN_TEKU_JUMP
#define GOMA_SFX_DAMAGE NA_SE_EN_TEKU_DAMAGE
#define GOMA_SFX_DEAD NA_SE_EN_TEKU_DEAD
#define GOMA_SFX_LAND NA_SE_EN_FALL_LAND
#define GOMA_SFX_WALK NA_SE_EN_MUSI_WALK
#define GOMA_SFX_FREEZE NA_SE_EN_COMMON_FREEZE
#define GOMA_SFX_HATCH NA_SE_EV_BOX_BREAK

#define ANIM(asset) ((AnimationHeader*)(asset))
#define LAST_FRAME(asset) Animation_GetLastFrame((void*)(asset))

#define GOMA_LIMB_BODY 3
#define GOMA_LIMB_EYE_IRIS_ROOT1 7
#define GOMA_LIMB_MAX 24

#define GOMA_FLAGS                                                                            \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_DRAW_CULLING_DISABLED)
#define GOMA_BGCHECK_FLAGS (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4)
#define GOMA_HEALTH 2
#define GOMA_EGG_Y_OFFSET 1500.0f
#define GOMA_HATCH_FRAGMENTS 15

typedef enum {
    GOMA_VISUAL_ANGRY,
    GOMA_VISUAL_NORMAL,
    GOMA_VISUAL_DAZED,
} GomaVisualState;

static const S2HModApi* sApi;

struct EnGoma;
typedef void (*EnGomaActionFunc)(struct EnGoma*, PlayState*);

typedef struct EnGoma {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[GOMA_LIMB_MAX];
    Vec3s morphTable[GOMA_LIMB_MAX];
    EnGomaActionFunc actionFunc;
    ColliderCylinder bodyCollider;
    ColliderCylinder hurtCollider;
    Vec3f shieldKnockbackVel;
    f32 eyeEnvColor[3];
    f32 eggSquishAngle;
    f32 eggSquishAmount;
    s16 slopePitch;
    s16 slopeRoll;
    s16 eyePitch;
    s16 eyeYaw;
    s16 eggTimer;
    s16 hurtTimer;
    s16 visualState;
    s16 playerDetectionTimer;
    s16 invincibilityTimer;
    s16 actionTimer;
    s16 stunTimer;
    u8 isEgg;
} EnGoma;

static ColliderCylinderInit sBodyColliderInit = {
    {
        COL_MATERIAL_HIT3,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x00, 0x08 },
        { 0xF7DFFFFF, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_NONE,
        OCELEM_ON,
    },
    { 15, 30, 10, { 0, 0, 0 } },
};

// Takes the shield too: a shield push knocks the larva back.
static ColliderCylinderInit sHurtColliderInit = {
    {
        COL_MATERIAL_HIT3,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x00, 0x08 },
        { 0xF7DFFFFF, 0x00, 0x00 },
        ATELEM_NONE,
        ACELEM_ON,
        OCELEM_NONE,
    },
    { 15, 30, 10, { 0, 0, 0 } },
};

static void EnGoma_SetupStand(EnGoma* this);
static void EnGoma_SetupJump(EnGoma* this);

static void EnGoma_Flee(EnGoma* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_ApproachF(&this->actor.speed, 20.0f / 3.0f, 0.5f, 2.0f);
    Math_ApproachS(&this->actor.world.rot.y, Actor_WorldYawTowardActor(&this->actor, &GET_PLAYER(play)->actor) + 0x8000,
                   3, 2000);
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 3000);
    if (this->actionTimer == 0) {
        EnGoma_SetupStand(this);
    }
}

static void EnGoma_SetupFlee(EnGoma* this) {
    Animation_Change(&this->skelAnime, ANIM(sRunningAnim), 2.0f, 0.0f, LAST_FRAME(sRunningAnim), ANIMMODE_LOOP, -2.0f);
    this->actionFunc = EnGoma_Flee;
    this->actionTimer = 20;
    Actor_PlaySfx(&this->actor, GOMA_SFX_DAMAGE);
}

static void EnGoma_SpawnHatchDebris(EnGoma* this, PlayState* play) {
    Vec3f pos = this->actor.world.pos;

    pos.y += 15.0f;
    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, GOMA_SFX_HATCH);
    EffectSsHahen_SpawnBurst(play, &pos, 5.0f, 0, 10, 5, GOMA_HATCH_FRAGMENTS, HAHEN_OBJECT_DEFAULT, 10, NULL);
}

static void EnGoma_Hatch(EnGoma* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->actionTimer == 0) {
        EnGoma_SetupStand(this);
    }
}

static void EnGoma_SetupHatch(EnGoma* this, PlayState* play) {
    Animation_Change(&this->skelAnime, ANIM(sJumpHeadbuttAnim), 1.0f, 0.0f, LAST_FRAME(sJumpHeadbuttAnim),
                     ANIMMODE_ONCE, 0.0f);
    this->actionFunc = EnGoma_Hatch;
    Actor_SetScale(&this->actor, 0.005f);
    this->isEgg = false;
    this->actor.gravity = -1.3f;
    this->actionTimer = 5;
    this->actor.shape.rot.y = Actor_WorldYawTowardActor(&this->actor, &GET_PLAYER(play)->actor);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    EnGoma_SpawnHatchDebris(this, play);
    this->actor.speed = 0.0f;
}

// The egg lies on the floor until the player stays close for a few frames.
static void EnGoma_Egg(EnGoma* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->eggSquishAngle += 1.0f;
    Math_ApproachF(&this->eggSquishAmount, 0.1f, 1.0f, 0.005f);
    bool isPlayerNear = (fabsf(this->actor.world.pos.x - player->actor.world.pos.x) < 100.0f) &&
                        (fabsf(this->actor.world.pos.z - player->actor.world.pos.z) < 100.0f);
    if (!isPlayerNear) {
        this->playerDetectionTimer = 0;
    } else if (++this->playerDetectionTimer > 9) {
        EnGoma_SetupHatch(this, play);
        return;
    }
    if (((this->eggTimer & 0xF) != 0) || (Rand_ZeroOne() >= 0.5f)) {
        return;
    }
    for (s32 i = 0; i < 2; i++) {
        Vec3f velocity = { 0.0f, 0.0f, 0.0f };
        Vec3f accel = { 0.0f, -0.5f, 0.0f };
        Vec3f pos = {
            Rand_CenteredFloat(30.0f) + this->actor.world.pos.x,
            Rand_ZeroFloat(30.0f) + this->actor.world.pos.y,
            Rand_CenteredFloat(30.0f) + this->actor.world.pos.z,
        };
        EffectSsHahen_Spawn(play, &pos, &velocity, &accel, 0, (s16)(Rand_ZeroOne() * 5.0f) + 10, HAHEN_OBJECT_DEFAULT,
                            10, NULL);
    }
}

static void EnGoma_SetupDead(EnGoma* this);

static void EnGoma_Die(EnGoma* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        Math_ApproachZeroF(&this->actor.speed, 1.0f, 2.0f);
    }
    if (this->actionTimer == 17) {
        Actor_PlaySfx(&this->actor, GOMA_SFX_LAND);
    }
    if (this->actionTimer == 0) {
        EnGoma_SetupDead(this);
    }
}

static void EnGoma_SetupDie(EnGoma* this) {
    Animation_Change(&this->skelAnime, ANIM(sDeathAnim), 1.0f, 0.0f, LAST_FRAME(sDeathAnim), ANIMMODE_ONCE, -2.0f);
    this->actionFunc = EnGoma_Die;
    this->actionTimer = 30;
    Actor_PlaySfx(&this->actor, GOMA_SFX_DEAD);
    this->invincibilityTimer = 100;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
}

static void EnGoma_Hurt(EnGoma* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        Math_ApproachZeroF(&this->actor.speed, 1.0f, 2.0f);
    }
    if (this->actionTimer != 0) {
        return;
    }
    if ((s8)this->actor.colChkInfo.health <= 0) {
        EnGoma_SetupDie(this);
    } else {
        EnGoma_SetupFlee(this);
    }
}

static void EnGoma_SetupHurt(EnGoma* this, PlayState* play) {
    Animation_Change(&this->skelAnime, ANIM(sDamagedAnim), 1.0f, 0.0f, LAST_FRAME(sDamagedAnim), ANIMMODE_ONCE, -2.0f);
    this->actionFunc = EnGoma_Hurt;
    if ((s8)this->actor.colChkInfo.health <= 0) {
        this->actionTimer = 5;
        Enemy_StartFinishingBlow(play, &this->actor);
    } else {
        this->actionTimer = 10;
    }
    this->actor.speed = 20.0f;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer + 0x8000;
    Actor_PlaySfx(&this->actor, GOMA_SFX_DAMAGE);
}

static void EnGoma_Dead(EnGoma* this, PlayState* play) {
    static Vec3f sDeadEffectVel = { 0.0f, 0.0f, 0.0f };

    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->actor.speed, 1.0f, 2.0f);
    this->visualState = GOMA_VISUAL_DAZED;
    if (this->actionTimer == 2) {
        Vec3f pos = { this->actor.world.pos.x, this->actor.world.pos.y - 5.0f, this->actor.world.pos.z };
        Vec3f accel = { 0.0f, 0.03f, 0.0f };
        EffectSsKFire_Spawn(play, &pos, &sDeadEffectVel, &accel, 40, 0);
    }
    if ((this->actionTimer != 0) ||
        (Math_SmoothStepToF(&this->actor.scale.y, 0.0f, 0.5f, 0.00225f, 0.00001f) > 0.001f)) {
        return;
    }
    Audio_PlaySfx_AtPos(&this->actor.projectedPos, NA_SE_EN_EXTINCT);
    Item_DropCollectibleRandom(play, NULL, &this->actor.world.pos, 0x30);
    Actor_Kill(&this->actor);
}

static void EnGoma_SetupDead(EnGoma* this) {
    Animation_Change(&this->skelAnime, ANIM(sDeadTwitchingAnim), 1.0f, 0.0f, LAST_FRAME(sDeadTwitchingAnim),
                     ANIMMODE_LOOP, -2.0f);
    this->actionFunc = EnGoma_Dead;
    this->actionTimer = 3;
}

static void EnGoma_Stand(EnGoma* this, PlayState* play);
static void EnGoma_ChasePlayer(EnGoma* this, PlayState* play);

static void EnGoma_SetupStand(EnGoma* this) {
    this->actionTimer = Rand_S16Offset(10, 30);
    Animation_Change(&this->skelAnime, ANIM(sStandAnim), 1.0f, 0.0f, LAST_FRAME(sStandAnim), ANIMMODE_LOOP, -5.0f);
    this->actionFunc = EnGoma_Stand;
    this->isEgg = false;
}

static void EnGoma_SetupChasePlayer(EnGoma* this) {
    Animation_Change(&this->skelAnime, ANIM(sRunningAnim), 1.0f, 0.0f, LAST_FRAME(sRunningAnim), ANIMMODE_LOOP, -5.0f);
    this->actionFunc = EnGoma_ChasePlayer;
    this->actionTimer = Rand_S16Offset(70, 110);
}

static void EnGoma_PrepareJump(EnGoma* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->actor.speed, 0.5f, 2.0f);
    s16 targetAngle = Actor_WorldYawTowardActor(&this->actor, &GET_PLAYER(play)->actor);
    Math_ApproachS(&this->actor.world.rot.y, targetAngle, 2, 4000);
    Math_ApproachS(&this->actor.shape.rot.y, targetAngle, 2, 3000);
    if (this->actionTimer == 0) {
        EnGoma_SetupJump(this);
    }
    this->visualState = GOMA_VISUAL_ANGRY;
}

static void EnGoma_SetupPrepareJump(EnGoma* this) {
    Animation_Change(&this->skelAnime, ANIM(sPrepareJumpAnim), 1.0f, 0.0f, LAST_FRAME(sPrepareJumpAnim), ANIMMODE_ONCE,
                     -5.0f);
    this->actionFunc = EnGoma_PrepareJump;
    this->actionTimer = 30;
}

static void EnGoma_Land(EnGoma* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        Math_ApproachZeroF(&this->actor.speed, 1.0f, 2.0f);
    }
    if (this->actionTimer == 0) {
        EnGoma_SetupStand(this);
    }
}

static void EnGoma_SetupLand(EnGoma* this) {
    Animation_Change(&this->skelAnime, ANIM(sLandFromJumpAnim), 1.0f, 0.0f, LAST_FRAME(sLandFromJumpAnim),
                     ANIMMODE_ONCE, 0.0f);
    this->actionFunc = EnGoma_Land;
    this->actionTimer = 10;
}

static void EnGoma_Jump(EnGoma* this, PlayState* play) {
    this->actor.flags |= ACTOR_FLAG_SFX_FOR_PLAYER_BODY_HIT;
    SkelAnime_Update(&this->skelAnime);
    Math_ApproachF(&this->actor.speed, 10.0f, 0.5f, 5.0f);
    if ((this->actor.velocity.y <= 0.0f) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        EnGoma_SetupLand(this);
        Actor_PlaySfx(&this->actor, GOMA_SFX_LAND);
    }
    this->visualState = GOMA_VISUAL_ANGRY;
}

static void EnGoma_SetupJump(EnGoma* this) {
    Animation_Change(&this->skelAnime, ANIM(sJumpHeadbuttAnim), 1.0f, 0.0f, LAST_FRAME(sJumpHeadbuttAnim),
                     ANIMMODE_ONCE, 0.0f);
    this->actionFunc = EnGoma_Jump;
    this->actor.velocity.y = 8.0f;
    Actor_PlaySfx(&this->actor, GOMA_SFX_CRY);
}

static void EnGoma_Stand(EnGoma* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->actor.speed, 0.5f, 2.0f);
    Math_ApproachS(&this->actor.shape.rot.y, Actor_WorldYawTowardActor(&this->actor, &GET_PLAYER(play)->actor), 2,
                   3000);
    if (this->actionTimer == 0) {
        EnGoma_SetupChasePlayer(this);
    }
}

static void EnGoma_ChasePlayer(EnGoma* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, 1.0f) || Animation_OnFrame(&this->skelAnime, 5.0f)) {
        Actor_PlaySfx(&this->actor, GOMA_SFX_WALK);
    }
    Math_ApproachF(&this->actor.speed, 10.0f / 3.0f, 0.5f, 2.0f);
    Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 2000);
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 3000);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        this->actor.velocity.y = 0.0f;
    }
    if (this->actor.xzDistToPlayer <= 150.0f) {
        EnGoma_SetupPrepareJump(this);
    }
}

static void EnGoma_Stunned(EnGoma* this, PlayState* play) {
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 180, COLORFILTER_BUFFLAG_OPA, 2);
    this->visualState = GOMA_VISUAL_DAZED;
    if (this->actionTimer != 0) {
        SkelAnime_Update(&this->skelAnime);
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        this->actor.velocity.y = 0.0f;
        Math_ApproachZeroF(&this->actor.speed, 0.5f, 2.0f);
    }
    if (this->stunTimer == 0) {
        EnGoma_SetupStand(this);
        return;
    }
    if (--this->stunTimer >= 30) {
        return;
    }
    f32 shake = (this->stunTimer & 1) ? 1.5f : -1.5f;
    this->actor.world.pos.x += shake;
    this->actor.world.pos.z += shake;
}

static void EnGoma_SetupStunned(EnGoma* this) {
    this->actionFunc = EnGoma_Stunned;
    this->stunTimer = 100;
    Animation_MorphToLoop(&this->skelAnime, ANIM(sStandAnim), -5.0f);
    this->actionTimer = (s16)Rand_ZeroFloat(15.0f) + 3;
    Actor_PlaySfx(&this->actor, GOMA_SFX_FREEZE);
}

static void EnGoma_LookAtPlayer(EnGoma* this, PlayState* play) {
    Actor* player = &GET_PLAYER(play)->actor;
    s16 eyeYaw = CLAMP(Actor_WorldYawTowardActor(&this->actor, player) - this->actor.shape.rot.y, -6000, 6000);
    s16 eyePitch = Actor_WorldPitchTowardActor(&this->actor, player) - this->actor.shape.rot.x;

    Math_ApproachS(&this->eyeYaw, eyeYaw, 3, 2000);
    Math_ApproachS(&this->eyePitch, eyePitch, 3, 2000);
}

static void EnGoma_ReactToShield(EnGoma* this, PlayState* play) {
    static Vec3f sShieldKnockbackVel = { 0.0f, 0.0f, 20.0f };

    if (this->actionFunc == EnGoma_Jump) {
        EnGoma_SetupLand(this);
        this->actor.velocity.y = 0.0f;
        this->actor.speed = -5.0f;
        return;
    }
    Matrix_RotateYS(GET_PLAYER(play)->actor.shape.rot.y, MTXMODE_NEW);
    Matrix_MultVec3f(&sShieldKnockbackVel, &this->shieldKnockbackVel);
    this->invincibilityTimer = 5;
}

static void EnGoma_TakeDamage(EnGoma* this, PlayState* play, ColliderElement* acHitElem) {
    u8 damage = acHitElem->atDmgInfo.damage;
    bool isSword = acHitElem->atDmgInfo.dmgFlags & (DMG_SWORD | DMG_SPIN_ATTACK | DMG_DEKU_STICK);

    if (isSword && (damage != 0)) {
        EffectSsSibuki_SpawnBurst(play, &this->actor.focus.pos);
    }
    this->actor.colChkInfo.health -= MAX(damage, 1);
}

static void EnGoma_UpdateHit(EnGoma* this, PlayState* play) {
    if (this->hurtTimer != 0) {
        this->hurtTimer--;
        return;
    }
    if ((this->bodyCollider.base.atFlags & AT_HIT) && (this->actionFunc == EnGoma_Jump)) {
        EnGoma_SetupLand(this);
        this->actor.speed = 0.0f;
        this->actor.velocity.y = 0.0f;
    }
    if (!(this->hurtCollider.base.acFlags & AC_HIT) || ((s8)this->actor.colChkInfo.health <= 0)) {
        return;
    }
    this->hurtCollider.base.acFlags &= ~AC_HIT;
    // An unhatched egg breaks at any blow, without a death.
    if (this->isEgg) {
        EnGoma_SpawnHatchDebris(this, play);
        Actor_Kill(&this->actor);
        return;
    }
    ColliderElement* acHitElem = this->hurtCollider.elem.acHitElem;
    u32 dmgFlags = acHitElem->atDmgInfo.dmgFlags;
    if (dmgFlags & DMG_NORMAL_SHIELD) {
        EnGoma_ReactToShield(this, play);
        return;
    }
    if (dmgFlags & DMG_DEKU_NUT) {
        if (this->actionFunc != EnGoma_Stunned) {
            EnGoma_SetupStunned(this);
            this->hurtTimer = 8;
        }
        return;
    }
    EnGoma_TakeDamage(this, play, acHitElem);
    EnGoma_SetupHurt(this, play);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 5);
    this->hurtTimer = 13;
}

static void EnGoma_UpdateEyeEnvColor(EnGoma* this) {
    static const f32 sTargetEyeEnvColors[3][3] = {
        { 255.0f, 0.0f, 50.0f },
        { 17.0f, 255.0f, 50.0f },
        { 0.0f, 170.0f, 50.0f },
    };

    for (s32 i = 0; i < 3; i++) {
        Math_ApproachF(&this->eyeEnvColor[i], sTargetEyeEnvColors[i][this->visualState], 0.5f, 20.0f);
    }
}

static void EnGoma_SetFloorRot(EnGoma* this) {
    if (this->actor.floorPoly == NULL) {
        return;
    }
    f32 nx = COLPOLY_GET_NORMAL(this->actor.floorPoly->normal.x);
    f32 ny = COLPOLY_GET_NORMAL(this->actor.floorPoly->normal.y);
    f32 nz = COLPOLY_GET_NORMAL(this->actor.floorPoly->normal.z);
    Math_ApproachS(&this->slopePitch, -Math_FAtan2F(-nz * ny, 1.0f) * (0x8000 / M_PIf), 1, 1000);
    Math_ApproachS(&this->slopeRoll, Math_FAtan2F(-nx * ny, 1.0f) * (0x8000 / M_PIf), 1, 1000);
}

static void EnGoma_UpdateColliders(EnGoma* this, PlayState* play) {
    // The hurtbox grows while the player swings so a near miss still lands.
    bool isPlayerSwinging = GET_PLAYER(play)->meleeWeaponState != 0;

    this->hurtCollider.dim.radius = isPlayerSwinging ? 35 : 15;
    this->hurtCollider.dim.height = isPlayerSwinging ? 35 : 30;
    this->hurtCollider.dim.yShift = isPlayerSwinging ? 0 : 10;
    if (this->invincibilityTimer != 0) {
        return;
    }
    Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
    Collider_UpdateCylinder(&this->actor, &this->hurtCollider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->hurtCollider.base);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->bodyCollider.base);
}

static void EnGoma_Update(Actor* thisx, PlayState* play) {
    EnGoma* this = (EnGoma*)thisx;

    if (this->actionTimer != 0) {
        this->actionTimer--;
    }
    if (this->invincibilityTimer != 0) {
        this->invincibilityTimer--;
    }
    this->actionFunc(this, play);
    Actor_MoveWithGravity(&this->actor);
    this->actor.world.pos.x += this->shieldKnockbackVel.x;
    this->actor.world.pos.z += this->shieldKnockbackVel.z;
    Math_ApproachZeroF(&this->shieldKnockbackVel.x, 1.0f, 3.0f);
    Math_ApproachZeroF(&this->shieldKnockbackVel.z, 1.0f, 3.0f);
    this->eggTimer++;
    Math_SmoothStepToF(&this->actor.scale.x, 0.01f, 0.5f, 0.00075f, 0.000001f);
    Math_SmoothStepToF(&this->actor.scale.y, 0.01f, 0.5f, 0.00075f, 0.000001f);
    Math_SmoothStepToF(&this->actor.scale.z, 0.01f, 0.5f, 0.00075f, 0.000001f);
    EnGoma_UpdateHit(this, play);
    Actor_UpdateBgCheckInfo(play, &this->actor, 50.0f, 50.0f, 100.0f, GOMA_BGCHECK_FLAGS);
    EnGoma_SetFloorRot(this);
    Actor_SetFocus(&this->actor, 20.0f);
    EnGoma_LookAtPlayer(this, play);
    EnGoma_UpdateEyeEnvColor(this);
    this->visualState = GOMA_VISUAL_NORMAL;
    EnGoma_UpdateColliders(this, play);
}

static void EnGoma_Init(Actor* thisx, PlayState* play) {
    EnGoma* this = (EnGoma*)thisx;

    this->eggTimer = Rand_ZeroOne() * 200.0f;
    this->actor.attentionRangeType = ATTENTION_RANGE_3;
    this->actor.gravity = 0.0f;
    this->actor.lockOnArrowOffset = 20.0f;
    Actor_SetScale(&this->actor, 0.01f);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 40.0f);
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sSkel, ANIM(sStandAnim), this->jointTable, this->morphTable,
                   GOMA_LIMB_MAX);
    Animation_PlayLoop(&this->skelAnime, ANIM(sStandAnim));
    this->actor.colChkInfo.health = GOMA_HEALTH;
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->isEgg = true;
    this->eggSquishAngle = Rand_ZeroOne() * 1000.0f;
    this->actionTimer = 50;
    this->actionFunc = EnGoma_Egg;
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyColliderInit);
    Collider_InitCylinder(play, &this->hurtCollider);
    Collider_SetCylinder(play, &this->hurtCollider, &this->actor, &sHurtColliderInit);
}

static void EnGoma_Destroy(Actor* thisx, PlayState* play) {
    EnGoma* this = (EnGoma*)thisx;

    Collider_DestroyCylinder(play, &this->bodyCollider);
    Collider_DestroyCylinder(play, &this->hurtCollider);
}

static s32 EnGoma_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    EnGoma* this = (EnGoma*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetEnvColor(POLY_OPA_DISP++, (s16)this->eyeEnvColor[0], (s16)this->eyeEnvColor[1], (s16)this->eyeEnvColor[2],
                   255);
    if (limbIndex == GOMA_LIMB_EYE_IRIS_ROOT1) {
        rot->x += this->eyePitch;
        rot->y += this->eyeYaw;
    } else if ((limbIndex == GOMA_LIMB_BODY) && (this->hurtTimer != 0)) {
        gDPSetEnvColor(POLY_OPA_DISP++, (s16)(Rand_ZeroOne() * 255.0f), (s16)(Rand_ZeroOne() * 255.0f),
                       (s16)(Rand_ZeroOne() * 255.0f), 255);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    return false;
}

static void EnGoma_DrawLarva(EnGoma* this, PlayState* play) {
    Matrix_Translate(this->actor.world.pos.x,
                     this->actor.world.pos.y + (this->actor.shape.yOffset * this->actor.scale.y),
                     this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateXS(this->slopePitch, MTXMODE_APPLY);
    Matrix_RotateZS(this->slopeRoll, MTXMODE_APPLY);
    Matrix_RotateYS(this->actor.shape.rot.y, MTXMODE_APPLY);
    Matrix_RotateXS(this->actor.shape.rot.x, MTXMODE_APPLY);
    Matrix_RotateZS(this->actor.shape.rot.z, MTXMODE_APPLY);
    Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, EnGoma_OverrideLimbDraw, NULL,
                      &this->actor);
}

static void EnGoma_DrawEgg(EnGoma* this, PlayState* play) {
    s32 scrollY = (s16)(sinf((this->eggTimer * 5.0f * 3.1415f) / 180.0f) * 31.9f) + 31;

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)Gfx_TexScroll(play->state.gfxCtx, 0, scrollY, 0, 0));
    Matrix_Push();
    Matrix_RotateYF(this->eggSquishAngle * 0.15f, MTXMODE_APPLY);
    Matrix_RotateZF(this->eggSquishAngle * 0.1f, MTXMODE_APPLY);
    Matrix_Scale(0.95f - this->eggSquishAmount, this->eggSquishAmount + 1.05f, 0.95f - this->eggSquishAmount,
                 MTXMODE_APPLY);
    Matrix_RotateZF(-(this->eggSquishAngle * 0.1f), MTXMODE_APPLY);
    Matrix_RotateYF(-(this->eggSquishAngle * 0.15f), MTXMODE_APPLY);
    Matrix_Translate(0.0f, GOMA_EGG_Y_OFFSET, 0.0f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sEggDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnGoma_Draw(Actor* thisx, PlayState* play) {
    EnGoma* this = (EnGoma*)thisx;

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    if (this->isEgg) {
        EnGoma_DrawEgg(this, play);
    } else {
        EnGoma_DrawLarva(this, play);
    }
}

// Ship (RE_GOHMA_LARVA): any attack.
static bool CanKillGohmaLarva(void* userData) {
    return sApi->LogicHasCapability("CanAttack");
}

static s16 RegisterGohmaLarva(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = GOHMA_LARVA_KEY;
    definition.description = "Gohma Larva";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = GOMA_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnGoma);
    definition.init = EnGoma_Init;
    definition.destroy = EnGoma_Destroy;
    definition.update = EnGoma_Update;
    definition.draw = EnGoma_Draw;
    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    definition.naviHint = "Gohma Larva&It hatches when you come close and leaps&at you. Stun it with a Deku Nut!";
    definition.soulKey = GOHMA_LARVA_KEY;
    definition.logicKill = CanKillGohmaLarva;
    definition.enemyCategory = S2H_ENEMY_CATEGORY_REGULAR;
    definition.soulModel.skeleton = sSkel;
    definition.soulModel.idleAnimation = sStandAnim;
    definition.soulModel.limbCount = GOMA_LIMB_MAX;
    definition.soulModel.scale = 0.01f;
    definition.soulModel.flameRed = 120;
    definition.soulModel.flameGreen = 230;
    definition.soulModel.flameBlue = 90;
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
    if (!RequireOotAsset(GOL_OBJECT "gObjectGolSkel")) {
        sApi->Log(GOHMA_LARVA_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    if (RegisterGohmaLarva() < 0) {
        sApi->Log("Could not register " GOHMA_LARVA_KEY);
    }
}
