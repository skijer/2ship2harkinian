/*
 * Ocarina of Time's Anubis (En_Anubice) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * Its spawn point plays En_Anubice_Tag: it mirrors the player around it. Only fire kills it.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"

#define ANUBIS_KEY "oot.anubis"
#define ANUBIS_FIRE_KEY "oot.anubis.fire"
#define ANUBICE_OBJECT "objects/oot_object_anubice/"
#define ANUBICE_ASSET(name) "__OTR__" ANUBICE_OBJECT name

static const ALIGN_ASSET(2) char sSkel[] = ANUBICE_ASSET("gAnubiceSkel");
static const ALIGN_ASSET(2) char sIdleAnim[] = ANUBICE_ASSET("gAnubiceIdleAnim");
static const ALIGN_ASSET(2) char sAttackingAnim[] = ANUBICE_ASSET("gAnubiceAttackingAnim");
static const ALIGN_ASSET(2) char sFallDownAnim[] = ANUBICE_ASSET("gAnubiceFallDownAnim");
static const ALIGN_ASSET(2) char sEyesDL[] = ANUBICE_ASSET("gAnubiceEyesDL");
static const ALIGN_ASSET(2) char sFireAttackDL[] = ANUBICE_ASSET("gAnubiceFireAttackDL");
static const ALIGN_ASSET(2) char sFireDustTex[] = "__OTR__objects/oot_gameplay_keep/gDust4Tex";

// MM has no Anubis and reuses its sfx ids for other actors, so these are MM stand-ins.
#define ANUBIS_SFX_DEAD NA_SE_EN_WIZ_DEAD
#define ANUBIS_SFX_FIRE NA_SE_EN_BUBLEFALL_FIRE
#define ANUBIS_SFX_FIREBOMB NA_SE_EN_WIZ_EXP

#define ANIM(asset) ((AnimationHeader*)(asset))
#define LAST_FRAME(asset) Animation_GetLastFrame((void*)(asset))

#define ANUBICE_LIMB_HEAD 0x0D
#define ANUBICE_LIMB_MAX 0x10

typedef enum {
    ANUBIS_DMGEFF_NONE = 0x0,
    ANUBIS_DMGEFF_FIRE = 0x2,
    ANUBIS_DMGEFF_KNOCKBACK = 0xF,
} AnubisDamageEffect;

#define ANUBIS_BURIED_OFFSET -4230.0f
#define ANUBIS_TRIGGER_RANGE 200.0f
#define ANUBIS_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED)
#define ANUBIS_FIRE_FLAGS ACTOR_FLAG_UPDATE_CULLING_DISABLED
#define ANUBIS_BGCHECK_FLAGS \
    (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_8 | UPDBGCHECKINFO_FLAG_10)
#define ANUBIS_FIRE_TRAIL_COUNT 6

static const S2HModApi* sApi;
static s16 sFireId = -1;

struct EnAnubice;
typedef void (*EnAnubiceActionFunc)(struct EnAnubice*, PlayState*);

typedef struct EnAnubice {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[ANUBICE_LIMB_MAX];
    Vec3s morphTable[ANUBICE_LIMB_MAX];
    EnAnubiceActionFunc actionFunc;
    ColliderCylinder collider;
    Vec3f fireballPos;
    Vec3f knockbackRecoveryVelocity;
    f32 animLastFrame;
    f32 playerHeightOffset;
    f32 focusHeightOffset;
    s16 hoverPhase;
    s16 fireballPitch;
    s16 fireballYaw;
    s16 fallTargetYaw;
    s16 knockbackTimer;
    u8 isFallingOver;
    u8 isMirroringPlayer;
    u8 isPlayerOutOfRange;
    u8 isKnockedBack;
    u8 isDying;
} EnAnubice;

struct EnAnubiceFire;
typedef void (*EnAnubiceFireActionFunc)(struct EnAnubiceFire*, PlayState*);

typedef struct EnAnubiceFire {
    Actor actor;
    EnAnubiceFireActionFunc actionFunc;
    ColliderCylinder collider;
    Vec3f trail[ANUBIS_FIRE_TRAIL_COUNT];
    f32 targetScale;
    f32 scale;
    s16 lifeTimer;
    s16 burstTimer;
    s16 burstCount;
    u8 isReflected;
} EnAnubiceFire;

static ColliderCylinderInit sBodyColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x00, 0x00 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_NONE,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 29, 103, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sFireColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x01, 0x04 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_NONE,
    },
    { 0, 0, 0, { 0, 0, 0 } },
};

// Every blow only knocks it aside; vanilla counts zero-damage hits, MM needs an effect for that.
static DamageTable sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Deku Stick     */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Horse trample  */ DMG_ENTRY(0, ANUBIS_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Zora boomerang */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Normal arrow   */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, ANUBIS_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Goron punch    */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Sword          */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Goron pound    */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Fire arrow     */ DMG_ENTRY(2, ANUBIS_DMGEFF_FIRE),
    /* Ice arrow      */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Light arrow    */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Goron spikes   */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Deku spin      */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Deku bubble    */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Deku launch    */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, ANUBIS_DMGEFF_NONE),
    /* Zora barrier   */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Normal shield  */ DMG_ENTRY(0, ANUBIS_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, ANUBIS_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Zora punch     */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Spin attack    */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Sword beam     */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
    /* Normal Roll    */ DMG_ENTRY(0, ANUBIS_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, ANUBIS_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, ANUBIS_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, ANUBIS_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, ANUBIS_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(0, ANUBIS_DMGEFF_KNOCKBACK),
};

static void EnAnubice_SetupIdle(EnAnubice* this);

static void EnAnubice_FacePlayer(EnAnubice* this) {
    if (!this->isKnockedBack) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 5, 3000, 0);
    }
}

static void EnAnubice_Hover(EnAnubice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->hoverPhase += 1500;
    Math_ApproachF(&this->actor.world.pos.y, player->actor.world.pos.y + this->playerHeightOffset, 0.1f, 10.0f);
    Math_ApproachF(&this->playerHeightOffset, 10.0f, 0.1f, 0.5f);
    this->actor.velocity.y = Math_SinS(this->hoverPhase);
}

static void EnAnubice_AimFireball(EnAnubice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 x = player->actor.world.pos.x - this->fireballPos.x;
    f32 y = player->actor.world.pos.y + 10.0f - this->fireballPos.y;
    f32 z = player->actor.world.pos.z - this->fireballPos.z;

    this->fireballPitch = -RAD_TO_BINANG(Math_FAtan2F(y, sqrtf(SQ(x) + SQ(z))));
    this->fireballYaw = RAD_TO_BINANG(Math_FAtan2F(x, z));
}

// The spawn point stands in for En_Anubice_Tag, which keeps Anubis opposite the player.
static void EnAnubice_MirrorPlayer(EnAnubice* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 dist = Math_Vec3f_DistXZ(&this->actor.home.pos, &player->actor.world.pos);

    if (dist >= ANUBIS_TRIGGER_RANGE) {
        if (this->isMirroringPlayer) {
            this->isPlayerOutOfRange = true;
        }
        return;
    }
    if (this->isPlayerOutOfRange || this->isKnockedBack) {
        return;
    }
    this->isMirroringPlayer = true;
    s16 yawToPlayer = Math_Vec3f_Yaw(&this->actor.home.pos, &player->actor.world.pos);
    Math_ApproachF(&this->actor.world.pos.x, this->actor.home.pos.x - Math_SinS(yawToPlayer) * dist, 0.3f, 10.0f);
    Math_ApproachF(&this->actor.world.pos.z, this->actor.home.pos.z - Math_CosS(yawToPlayer) * dist, 0.3f, 10.0f);
}

static void EnAnubice_WaitBuried(EnAnubice* this, PlayState* play) {
    if (!this->isMirroringPlayer) {
        return;
    }
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    EnAnubice_SetupIdle(this);
}

static void EnAnubice_SetupWaitBuried(EnAnubice* this) {
    this->actor.shape.yOffset = ANUBIS_BURIED_OFFSET;
    this->isMirroringPlayer = false;
    this->isPlayerOutOfRange = false;
    this->actor.gravity = 0.0f;
    this->actionFunc = EnAnubice_WaitBuried;
}

static void EnAnubice_GoToHome(EnAnubice* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_ApproachF(&this->actor.shape.yOffset, ANUBIS_BURIED_OFFSET, 0.5f, 300.0f);
    Math_ApproachZeroF(&this->focusHeightOffset, 0.5f, 5.0f);
    EnAnubice_FacePlayer(this);
    f32 x = this->actor.home.pos.x - this->actor.world.pos.x;
    f32 z = this->actor.home.pos.z - this->actor.world.pos.z;
    if ((fabsf(x) > 3.0f) && (fabsf(z) > 3.0f)) {
        f32 xzDist = sqrtf(SQ(x) + SQ(z));
        this->actor.world.pos.x += (x / xzDist) * 8.0f;
        this->actor.world.pos.z += (z / xzDist) * 8.0f;
    } else if (this->actor.shape.yOffset < -4220.0f) {
        EnAnubice_SetupWaitBuried(this);
    }
}

static void EnAnubice_ShootFireball(EnAnubice* this, PlayState* play) {
    f32 curFrame = this->skelAnime.curFrame;

    SkelAnime_Update(&this->skelAnime);
    EnAnubice_FacePlayer(this);
    EnAnubice_AimFireball(this, play);
    if (curFrame == 12.0f) {
        Actor_Spawn(&play->actorCtx, play, sFireId, this->fireballPos.x, this->fireballPos.y + 15.0f,
                    this->fireballPos.z, this->fireballPitch, this->fireballYaw, 0, 0);
    }
    if (this->animLastFrame <= curFrame) {
        EnAnubice_SetupIdle(this);
    }
}

static void EnAnubice_SetupShootFireball(EnAnubice* this) {
    this->animLastFrame = LAST_FRAME(sAttackingAnim);
    Animation_Change(&this->skelAnime, ANIM(sAttackingAnim), 1.0f, 0.0f, this->animLastFrame, ANIMMODE_ONCE, -10.0f);
    this->actor.velocity.x = 0.0f;
    this->actor.velocity.z = 0.0f;
    this->actionFunc = EnAnubice_ShootFireball;
}

static void EnAnubice_Idle(EnAnubice* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->actor.shape.yOffset, 0.5f, 300.0f);
    Math_ApproachF(&this->focusHeightOffset, 70.0f, 0.5f, 5.0f);
    EnAnubice_FacePlayer(this);
    if (this->actor.shape.yOffset <= -2.0f) {
        return;
    }
    this->actor.shape.yOffset = 0.0f;
    if (GET_PLAYER(play)->meleeWeaponState != 0) {
        EnAnubice_SetupShootFireball(this);
    } else if (this->isPlayerOutOfRange) {
        this->actor.velocity.y = 0.0f;
        this->actor.gravity = -1.0f;
        this->actionFunc = EnAnubice_GoToHome;
    }
}

static void EnAnubice_SetupIdle(EnAnubice* this) {
    Animation_Change(&this->skelAnime, ANIM(sIdleAnim), 1.0f, 0.0f, (s16)LAST_FRAME(sIdleAnim), ANIMMODE_LOOP, -10.0f);
    this->actor.velocity.x = 0.0f;
    this->actor.velocity.z = 0.0f;
    this->actor.gravity = 0.0f;
    this->actionFunc = EnAnubice_Idle;
}

static void EnAnubice_SpawnDeathFlame(EnAnubice* this, PlayState* play) {
    Vec3f offset = { 0.0f, 0.0f, 0.0f };
    Vec3f firePos;
    f32 tilt = CLAMP_MIN(this->skelAnime.curFrame * -3000.0f, -11000.0f);

    Matrix_RotateYS(this->actor.shape.rot.y, MTXMODE_NEW);
    Matrix_RotateXS((s16)tilt, MTXMODE_APPLY);
    offset.y = Rand_CenteredFloat(10.0f) + 30.0f;
    Matrix_MultVec3f(&offset, &firePos);
    firePos.x += this->actor.world.pos.x + Rand_CenteredFloat(40.0f);
    firePos.y += this->actor.world.pos.y + Rand_CenteredFloat(40.0f);
    firePos.z += this->actor.world.pos.z + Rand_CenteredFloat(30.0f);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 128, COLORFILTER_BUFFLAG_OPA, 8);
    EffectSsEnFire_SpawnVec3f(play, &this->actor, &firePos, 100, 0, 0, -1);
}

static void EnAnubice_Die(EnAnubice* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_ApproachZeroF(&this->actor.shape.shadowScale, 0.4f, 0.25f);
    if (this->isFallingOver) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->fallTargetYaw, 1, 10000, 0);
        if (ABS(this->actor.shape.rot.y - this->fallTargetYaw) < 100) {
            this->isFallingOver = false;
        }
    }
    EnAnubice_SpawnDeathFlame(this, play);
    bool hasFallen =
        (this->animLastFrame <= this->skelAnime.curFrame) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND);
    if (!hasFallen) {
        return;
    }
    Math_ApproachF(&this->actor.shape.yOffset, ANUBIS_BURIED_OFFSET, 0.5f, 300.0f);
    if (this->actor.shape.yOffset < -2000.0f) {
        Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xC0);
        Actor_Kill(&this->actor);
    }
}

static void EnAnubice_SetupDie(EnAnubice* this, PlayState* play) {
    Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_PROP);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Enemy_StartFinishingBlow(play, &this->actor);
    Actor_PlaySfx(&this->actor, ANUBIS_SFX_DEAD);
    this->isDying = true;
    this->animLastFrame = LAST_FRAME(sFallDownAnim);
    Animation_Change(&this->skelAnime, ANIM(sFallDownAnim), 1.0f, 0.0f, this->animLastFrame, ANIMMODE_ONCE, -20.0f);
    this->isFallingOver = false;
    this->fallTargetYaw = 0;
    this->actor.velocity.x = 0.0f;
    this->actor.velocity.z = 0.0f;
    this->actor.gravity = -1.0f;
    if (BgCheck_SphVsFirstPoly(&play->colCtx, &this->fireballPos, 70.0f)) {
        this->isFallingOver = true;
        this->fallTargetYaw = this->actor.shape.rot.x - 0x7F00;
    }
    this->actionFunc = EnAnubice_Die;
}

static void EnAnubice_StartKnockback(EnAnubice* this) {
    Vec3f push = { 0.0f, 0.0f, -10.0f };
    Vec3f velocity;

    this->knockbackTimer = 10;
    this->isKnockedBack = true;
    Matrix_RotateYS(this->actor.shape.rot.y, MTXMODE_NEW);
    Matrix_MultVec3f(&push, &velocity);
    this->actor.velocity.x = velocity.x;
    this->actor.velocity.z = velocity.z;
    this->knockbackRecoveryVelocity.x = -velocity.x;
    this->knockbackRecoveryVelocity.z = -velocity.z;
    Actor_PlaySfx(&this->actor, NA_SE_EN_CUTBODY);
}

static void EnAnubice_UpdateKnockback(EnAnubice* this) {
    this->actor.shape.rot.y += 6500;
    Math_ApproachF(&this->actor.velocity.x, this->knockbackRecoveryVelocity.x, 0.3f, 1.0f);
    Math_ApproachF(&this->actor.velocity.z, this->knockbackRecoveryVelocity.z, 0.3f, 1.0f);
    if (this->knockbackTimer != 0) {
        return;
    }
    this->actor.velocity.x = 0.0f;
    this->actor.velocity.z = 0.0f;
    this->knockbackRecoveryVelocity.x = 0.0f;
    this->knockbackRecoveryVelocity.z = 0.0f;
    this->isKnockedBack = false;
}

// Returns true once a fire hit has started the death.
static bool EnAnubice_UpdateHovering(EnAnubice* this, PlayState* play) {
    EnAnubice_Hover(this, play);
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        if (this->actor.colChkInfo.damageEffect == ANUBIS_DMGEFF_FIRE) {
            EnAnubice_SetupDie(this, play);
            return true;
        }
        if (!this->isKnockedBack) {
            EnAnubice_StartKnockback(this);
        }
    }
    if (this->isKnockedBack) {
        EnAnubice_UpdateKnockback(this);
    }
    return false;
}

static void EnAnubice_Update(Actor* thisx, PlayState* play) {
    EnAnubice* this = (EnAnubice*)thisx;

    if (!this->isDying) {
        EnAnubice_MirrorPlayer(this, play);
    }
    if (!this->isDying && (this->actor.shape.yOffset == 0.0f) && EnAnubice_UpdateHovering(this, play)) {
        return;
    }
    if (this->knockbackTimer != 0) {
        this->knockbackTimer--;
    }
    this->actionFunc(this, play);
    this->actor.velocity.y += this->actor.gravity;
    Actor_UpdatePos(&this->actor);
    u32 bgCheckFlags =
        this->isPlayerOutOfRange ? (ANUBIS_BGCHECK_FLAGS & ~UPDBGCHECKINFO_FLAG_1) : ANUBIS_BGCHECK_FLAGS;
    Actor_UpdateBgCheckInfo(play, &this->actor, 5.0f, 5.0f, 10.0f, bgCheckFlags);
    if (this->isDying) {
        return;
    }
    Actor_SetFocus(&this->actor, this->focusHeightOffset);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    if (!this->isKnockedBack && (this->actor.shape.yOffset == 0.0f)) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
}

static void EnAnubice_Init(Actor* thisx, PlayState* play) {
    EnAnubice* this = (EnAnubice*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 20.0f);
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sSkel, ANIM(sIdleAnim), this->jointTable, this->morphTable,
                   ANUBICE_LIMB_MAX);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sBodyColliderInit);
    Actor_SetScale(&this->actor, 0.015f);
    this->actor.colChkInfo.damageTable = &sDamageTable;
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->focusHeightOffset = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.attentionRangeType = ATTENTION_RANGE_3;
    EnAnubice_SetupWaitBuried(this);
}

static void EnAnubice_Destroy(Actor* thisx, PlayState* play) {
    EnAnubice* this = (EnAnubice*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void EnAnubice_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    static Vec3f sOrigin = { 0.0f, 0.0f, 0.0f };
    EnAnubice* this = (EnAnubice*)thisx;

    if (limbIndex != ANUBICE_LIMB_HEAD) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sEyesDL);
    Matrix_MultVec3f(&sOrigin, &this->fireballPos);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnAnubice_Draw(Actor* thisx, PlayState* play) {
    EnAnubice* this = (EnAnubice*)thisx;

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, NULL, EnAnubice_PostLimbDraw,
                      &this->actor);
}

static void EnAnubiceFire_Burst(EnAnubiceFire* this, PlayState* play) {
    Vec3f velocity = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 primColor = { 255, 255, 0, 255 };
    Color_RGBA8 envColor = { 255, 0, 0, 255 };

    if (this->burstTimer != 0) {
        return;
    }
    for (s32 i = 0; i < 20; i++) {
        Vec3f accel = { Rand_CenteredFloat(8.0f), Rand_CenteredFloat(2.0f), Rand_CenteredFloat(8.0f) };
        EffectSsKirakira_SpawnDispersed(play, &this->actor.world.pos, &velocity, &accel, &primColor, &envColor, 2000,
                                        10);
    }
    this->burstTimer = 2;
    this->burstCount++;
    if (this->burstCount >= ANUBIS_FIRE_TRAIL_COUNT) {
        Actor_Kill(&this->actor);
    }
}

static void EnAnubiceFire_StartBurst(EnAnubiceFire* this) {
    this->actor.velocity.x = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.velocity.z = 0.0f;
    Actor_PlaySfx(&this->actor, ANUBIS_SFX_FIREBOMB);
    this->actionFunc = EnAnubiceFire_Burst;
}

static void EnAnubiceFire_Reflect(EnAnubiceFire* this) {
    Actor_PlaySfx(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW);
    this->collider.base.atFlags &= ~(AT_HIT | AT_BOUNCED | AT_TYPE_ENEMY);
    this->collider.base.atFlags |= AT_TYPE_PLAYER;
    // The restored behaviour (Ship's "AnubisFix"): the returned fireball burns Anubis like fire.
    this->collider.elem.atDmgInfo.dmgFlags = DMG_FIRE_ARROW;
    this->lifeTimer = 30;
    this->isReflected = true;
    this->actor.velocity.x *= -1.0f;
    this->actor.velocity.y *= -0.5f;
    this->actor.velocity.z *= -1.0f;
}

static void EnAnubiceFire_SpawnSparkles(EnAnubiceFire* this, PlayState* play) {
    Vec3f velocity = { 0.0f, 0.0f, 0.0f };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 primColor = { 255, 255, 0, 255 };
    Color_RGBA8 envColor = { 255, 0, 0, 255 };

    for (s32 i = 0; i < 10; i++) {
        Vec3f pos;
        pos.x = this->actor.world.pos.x + (Rand_ZeroOne() - 0.5f) * (this->scale * 20.0f);
        pos.y = this->actor.world.pos.y + (Rand_ZeroOne() - 0.5f) * (this->scale * 20.0f);
        pos.z = this->actor.world.pos.z;
        EffectSsKirakira_SpawnDispersed(play, &pos, &velocity, &accel, &primColor, &envColor, 1000, 10);
    }
    Actor_PlaySfx_Flagged(&this->actor, ANUBIS_SFX_FIRE - SFX_FLAG);
}

static void EnAnubiceFire_Fly(EnAnubiceFire* this, PlayState* play) {
    this->actor.world.rot.z += 5000;
    if (this->lifeTimer == 0) {
        this->targetScale = 0.0f;
    }
    Math_ApproachF(&this->scale, this->targetScale, 0.2f, 0.4f);
    if ((this->lifeTimer == 0) && (this->scale < 0.1f)) {
        Actor_Kill(&this->actor);
        return;
    }
    if (!this->isReflected && (this->collider.base.atFlags & AT_BOUNCED)) {
        if (Player_HasMirrorShieldEquipped(play)) {
            EnAnubiceFire_Reflect(this);
            return;
        }
        Vec3f zero = { 0.0f, 0.0f, 0.0f };
        this->lifeTimer = 0;
        EffectSsBomb2_SpawnLayered(play, &this->actor.world.pos, &zero, &zero, 10, 5);
        EnAnubiceFire_StartBurst(this);
        return;
    }
    if (this->scale >= 0.4f) {
        EnAnubiceFire_SpawnSparkles(this, play);
    }
}

static void EnAnubiceFire_Launch(EnAnubiceFire* this, PlayState* play) {
    Vec3f forward = { 0.0f, 0.0f, 15.0f };

    Matrix_Push();
    Matrix_RotateYS(this->actor.world.rot.y, MTXMODE_NEW);
    Matrix_RotateXS(this->actor.world.rot.x, MTXMODE_APPLY);
    Matrix_MultVec3f(&forward, &this->actor.velocity);
    Matrix_Pop();
    this->actor.world.rot.x = 0;
    this->actor.world.rot.y = 0;
    this->actor.world.rot.z = 0;
    this->actionFunc = EnAnubiceFire_Fly;
}

static void EnAnubiceFire_UpdateTrail(EnAnubiceFire* this) {
    this->trail[0] = this->actor.world.pos;
    for (s32 i = ANUBIS_FIRE_TRAIL_COUNT - 2; i >= 0; i--) {
        this->trail[i + 1] = this->trail[i];
    }
}

static void EnAnubiceFire_UpdateCollision(EnAnubiceFire* this, PlayState* play) {
    if ((this->scale < 0.6f) || (this->actionFunc == EnAnubiceFire_Burst)) {
        return;
    }
    this->collider.dim.radius = this->scale * 15.0f + 5.0f;
    this->collider.dim.height = this->scale * 15.0f + 5.0f;
    this->collider.dim.yShift = this->scale * -0.75f + -15.0f;
    if (this->lifeTimer != 0) {
        Collider_UpdateCylinder(&this->actor, &this->collider);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    if (BgCheck_SphVsFirstPoly(&play->colCtx, &this->actor.world.pos, 30.0f)) {
        EnAnubiceFire_StartBurst(this);
    }
}

static void EnAnubiceFire_Update(Actor* thisx, PlayState* play) {
    EnAnubiceFire* this = (EnAnubiceFire*)thisx;

    Actor_SetScale(&this->actor, this->scale);
    this->actionFunc(this, play);
    Actor_UpdatePos(&this->actor);
    EnAnubiceFire_UpdateTrail(this);
    if (this->lifeTimer != 0) {
        this->lifeTimer--;
    }
    if (this->burstTimer != 0) {
        this->burstTimer--;
    }
    Actor_UpdateBgCheckInfo(play, &this->actor, 5.0f, 5.0f, 10.0f, ANUBIS_BGCHECK_FLAGS);
    EnAnubiceFire_UpdateCollision(this, play);
}

static void EnAnubiceFire_Init(Actor* thisx, PlayState* play) {
    EnAnubiceFire* this = (EnAnubiceFire*)thisx;

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sFireColliderInit);
    this->lifeTimer = 30;
    this->targetScale = 2.0f;
    this->scale = 0.0f;
    for (s32 i = 0; i < ANUBIS_FIRE_TRAIL_COUNT; i++) {
        this->trail[i] = this->actor.world.pos;
    }
    this->burstCount = 0;
    this->actionFunc = EnAnubiceFire_Launch;
}

static void EnAnubiceFire_Destroy(Actor* thisx, PlayState* play) {
    EnAnubiceFire* this = (EnAnubiceFire*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void EnAnubiceFire_Draw(Actor* thisx, PlayState* play) {
    EnAnubiceFire* this = (EnAnubiceFire*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 0, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, 0);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)sFireDustTex);
    Matrix_Push();
    for (s32 i = this->burstCount; i < ANUBIS_FIRE_TRAIL_COUNT; i++) {
        FrameInterpolation_RecordOpenChild(this, i);
        f32 scale = MAX(this->actor.scale.x - (i * 0.2f), 0.0f);
        if (scale >= 0.1f) {
            Matrix_Translate(this->trail[i].x, this->trail[i].y, this->trail[i].z, MTXMODE_NEW);
            Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            // Vanilla feeds a binary angle to a radian rotation; kept for the same spin.
            Matrix_RotateZF(this->actor.world.rot.z + i * 1000.0f, MTXMODE_APPLY);
            MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sFireAttackDL);
        }
        FrameInterpolation_RecordCloseChild();
        if (this->scale < 0.1f) {
            break;
        }
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

// Ship: a fire source.
static bool CanKillAnubis(void* userData) {
    return sApi->LogicHasCapability("HasFireSource");
}

static s16 RegisterFire(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = ANUBIS_FIRE_KEY;
    definition.description = "Anubis's fireball";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = ANUBIS_FIRE_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnAnubiceFire);
    definition.init = EnAnubiceFire_Init;
    definition.destroy = EnAnubiceFire_Destroy;
    definition.update = EnAnubiceFire_Update;
    definition.draw = EnAnubiceFire_Draw;
    return sApi->RegisterActor(&definition);
}

static s16 RegisterAnubis(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = ANUBIS_KEY;
    definition.description = "Anubis";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = ANUBIS_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnAnubice);
    definition.init = EnAnubice_Init;
    definition.destroy = EnAnubice_Destroy;
    definition.update = EnAnubice_Update;
    definition.draw = EnAnubice_Draw;
    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    definition.naviHint = "Anubis&It mirrors your every move and hurls fire&when you swing. Only fire can burn it!";
    // The host finds the soul item as "soul." + soulKey, so this is the family key.
    definition.soulKey = ANUBIS_KEY;
    definition.logicKill = CanKillAnubis;
    definition.enemyCategory = S2H_ENEMY_CATEGORY_REGULAR;
    definition.dropOnKill = true;
    definition.soulModel.skeleton = sSkel;
    definition.soulModel.idleAnimation = sIdleAnim;
    definition.soulModel.limbCount = ANUBICE_LIMB_MAX;
    definition.soulModel.scale = 0.015f;
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
    if (!RequireOotAsset(ANUBICE_OBJECT "gAnubiceSkel")) {
        sApi->Log(ANUBIS_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    sFireId = RegisterFire();
    if (sFireId < 0) {
        sApi->Log("Could not register " ANUBIS_FIRE_KEY);
        return;
    }
    if (RegisterAnubis() < 0) {
        sApi->Log("Could not register " ANUBIS_KEY);
    }
}
