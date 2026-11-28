/*
 * Ocarina of Time's Stingers on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r: the lone one
 * (En_Eiyer, params 10) wanders hidden under the floor, leaps out when touched and dives from the air;
 * the water one (En_Weiyer) lives in stinger_water.c. Both share one soul.
 */
#include "oot_enemy_stinger.h"


static const ALIGN_ASSET(2) char sBackflipAnim[] = EI_ASSET("gStingerBackflipAnim");
static const ALIGN_ASSET(2) char sDiveAnim[] = EI_ASSET("gStingerDiveAnim");

#define EI_LIMB_LEFT_EYE 9
#define EI_LIMB_RIGHT_EYE 10

#define STINGER_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE)
#define STINGER_BGCHECK_FLAGS (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_4)
// Buried, only Deku Nuts, explosives and the boomerang reach it; MM keeps OoT's bits for all three.
#define STINGER_BURIED_DMG_FLAGS (DMG_DEKU_NUT | DMG_EXPLOSIVES | DMG_ZORA_BOOMERANG)
#define STINGER_GLIDE_HEIGHT 85.0f
#define STINGER_WANDER_RADIUS 100.0f

static const S2HModApi* sApi;
static s16 sLandStingerId;

struct EnEiyer;
typedef void (*EnEiyerActionFunc)(struct EnEiyer*, PlayState*);

typedef struct EnEiyer {
    Actor actor;
    SkelAnime skelAnime;
    EnEiyerActionFunc actionFunc;
    s16 timer;
    s16 targetYaw;
    Vec3s jointTable[EI_LIMB_MAX];
    Vec3s morphTable[EI_LIMB_MAX];
    Vec3f basePos;
    ColliderCylinder collider;
} EnEiyer;

static ColliderCylinderInit sColCylInit = {
    {
        COL_MATERIAL_HIT0,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x04, 0x08 },
        { STINGER_BURIED_DMG_FLAGS, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_HARD,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 27, 17, -10, { 0, 0, 0 } },
};

static void EnEiyer_AppearFromGround(EnEiyer* this, PlayState* play);
static void EnEiyer_WanderUnderground(EnEiyer* this, PlayState* play);
static void EnEiyer_Ambush(EnEiyer* this, PlayState* play);
static void EnEiyer_Glide(EnEiyer* this, PlayState* play);
static void EnEiyer_StartAttack(EnEiyer* this, PlayState* play);
static void EnEiyer_DiveAttack(EnEiyer* this, PlayState* play);
static void EnEiyer_Land(EnEiyer* this, PlayState* play);
static void EnEiyer_Hurt(EnEiyer* this, PlayState* play);
static void EnEiyer_Die(EnEiyer* this, PlayState* play);
static void EnEiyer_Dead(EnEiyer* this, PlayState* play);
static void EnEiyer_Stunned(EnEiyer* this, PlayState* play);

static bool EnEiyer_IsBuried(EnEiyer* this) {
    return this->collider.elem.acDmgInfo.dmgFlags == STINGER_BURIED_DMG_FLAGS;
}

static void EnEiyer_SetupAppearFromGround(EnEiyer* this) {
    this->collider.elem.acDmgInfo.dmgFlags = STINGER_BURIED_DMG_FLAGS;
    Animation_PlayLoop(&this->skelAnime, ANIM(sIdleAnim));
    this->actor.world.pos.x = this->actor.home.pos.x;
    this->actor.world.pos.y = this->actor.home.pos.y - 40.0f;
    this->actor.world.pos.z = this->actor.home.pos.z;
    this->actor.velocity.y = 0.0f;
    this->actor.speed = 0.0f;
    this->actor.world.rot.y = Rand_ZeroOne() * 0x10000;
    this->actor.shape.rot.y = this->actor.world.rot.y;
    this->collider.base.atFlags &= ~AT_ON;
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_IGNORE_QUAKE);
    this->actor.shape.shadowScale = 0.0f;
    this->actor.shape.yOffset = 0.0f;
    this->actionFunc = EnEiyer_AppearFromGround;
}

static void EnEiyer_SetupWanderUnderground(EnEiyer* this) {
    this->actor.speed = -0.5f;
    this->actionFunc = EnEiyer_WanderUnderground;
    this->collider.base.acFlags |= AC_ON;
    this->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
}

static void EnEiyer_SetupAmbush(EnEiyer* this, PlayState* play) {
    this->actor.speed = 0.0f;
    Animation_PlayOnce(&this->skelAnime, ANIM(sBackflipAnim));
    this->collider.elem.acDmgInfo.dmgFlags = STINGER_EXPOSED_DMG_FLAGS;
    this->basePos = this->actor.world.pos;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.flags |= ACTOR_FLAG_IGNORE_QUAKE;
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.shape.shadowScale = 65.0f;
    this->actor.shape.yOffset = 600.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_OCTAROCK_JUMP);
    EffectSsGSplash_Spawn(play, &this->actor.world.pos, NULL, NULL, 1, 700);
    this->actionFunc = EnEiyer_Ambush;
}

static void EnEiyer_SetupGlide(EnEiyer* this) {
    this->targetYaw = this->actor.shape.rot.y;
    this->basePos.y = (cosf(-M_PIf / 8) * 5.0f) + this->actor.world.pos.y;
    Animation_MorphToLoop(&this->skelAnime, ANIM(sHitAnim), -5.0f);
    this->timer = 60;
    this->actionFunc = EnEiyer_Glide;
}

static void EnEiyer_SetupDiveAttack(EnEiyer* this, PlayState* play) {
    this->actor.velocity.y = 0.0f;
    this->basePos.y = GET_PLAYER(play)->actor.world.pos.y + 15.0f;
    this->collider.base.atFlags |= AT_ON;
    this->collider.base.atFlags &= ~AT_HIT;
    this->actionFunc = EnEiyer_DiveAttack;
}

static void EnEiyer_SetupLand(EnEiyer* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sDiveAnim), -3.0f);
    this->collider.base.atFlags &= ~AT_ON;
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    // -1 makes the first land frame refresh BgCheck and splash.
    this->timer = -1;
    this->actor.gravity = 0.0f;
    this->collider.dim.height = sColCylInit.dim.height;
    this->actionFunc = EnEiyer_Land;
}

static void EnEiyer_SetupHurt(EnEiyer* this) {
    this->basePos.y = this->actor.world.pos.y;
    Animation_Change(&this->skelAnime, ANIM(sHitAnim), 2.0f, 0.0f, 0.0f, ANIMMODE_LOOP, -3.0f);
    this->timer = 40;
    this->actor.gravity = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.speed = 5.0f;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 200, COLORFILTER_BUFFLAG_OPA, 40);
    this->collider.base.acFlags &= ~AC_ON;
    this->actionFunc = EnEiyer_Hurt;
}

static void EnEiyer_SetupDie(EnEiyer* this) {
    this->timer = 20;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 200, COLORFILTER_BUFFLAG_OPA, 40);
    if (!EnEiyer_IsBuried(this)) {
        this->actor.speed = 6.0f;
        Animation_MorphToLoop(&this->skelAnime, ANIM(sHitAnim), -3.0f);
    } else {
        this->actor.speed -= 6.0f;
    }
    this->collider.elem.acDmgInfo.dmgFlags = STINGER_EXPOSED_DMG_FLAGS;
    this->collider.base.atFlags &= ~AT_ON;
    this->collider.base.acFlags &= ~AC_ON;
    this->actionFunc = EnEiyer_Die;
}

static void EnEiyer_SetupDead(EnEiyer* this) {
    this->actor.colorFilterParams |= COLORFILTER_BUFFLAG_XLU;
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actionFunc = EnEiyer_Dead;
}

static void EnEiyer_SetupStunned(EnEiyer* this) {
    Animation_Change(&this->skelAnime, ANIM(sPopOutAnim), 2.0f, 0.0f, 0.0f, ANIMMODE_LOOP, -8.0f);
    this->timer = 80;
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -1.0f;
    this->collider.dim.height = sColCylInit.dim.height + 8;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 200, COLORFILTER_BUFFLAG_OPA, 80);
    this->collider.base.atFlags &= ~AT_ON;
    Actor_PlaySfx(&this->actor, STINGER_SFX_FREEZE);
    this->actionFunc = EnEiyer_Stunned;
}

static void EnEiyer_AppearFromGround(EnEiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Math_StepToF(&this->actor.world.pos.y, this->actor.home.pos.y, 0.75f)) {
        EnEiyer_SetupWanderUnderground(this);
    }
}

static void EnEiyer_WanderUnderground(EnEiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Actor_WorldDistXZToPoint(&this->actor, &this->actor.home.pos) > STINGER_WANDER_RADIUS) {
        this->targetYaw = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos) + 0x8000;
    } else if ((this->targetYaw == this->actor.world.rot.y) && (Rand_ZeroOne() > 0.99f)) {
        s16 turn = Rand_ZeroOne() * 0x2000 + 0x2000;
        this->targetYaw = this->actor.world.rot.y + ((Rand_ZeroOne() < 0.5f) ? -turn : turn);
    }
    Math_ScaledStepToS(&this->actor.world.rot.y, this->targetYaw, 0xB6);
    if (this->collider.base.ocFlags2 & OC2_HIT_PLAYER) {
        this->collider.base.ocFlags2 &= ~OC2_HIT_PLAYER;
        EnEiyer_SetupAmbush(this, play);
    }
}

static void EnEiyer_Ambush(EnEiyer* this, PlayState* play) {
    bool isAnimDone = SkelAnime_Update(&this->skelAnime);
    f32 curFrame = this->skelAnime.curFrame;

    if (curFrame < 12.0f) {
        f32 arc = (0.996f * M_PIf / 12.0f) * curFrame;
        this->actor.world.pos.y = ((1.0f - cosf(arc)) * 40.0f) + this->actor.home.pos.y;
        f32 xzOffset = sinf(arc) * -40.0f;
        this->actor.world.pos.x = (Math_SinS(this->actor.shape.rot.y) * xzOffset) + this->basePos.x;
        this->actor.world.pos.z = (Math_CosS(this->actor.shape.rot.y) * xzOffset) + this->basePos.z;
    } else {
        Math_StepToF(&this->actor.world.pos.y, this->actor.home.pos.y + 80.0f, 0.5f);
        this->actor.speed = 0.8f;
    }
    if (isAnimDone) {
        this->collider.base.acFlags |= AC_ON;
        EnEiyer_SetupGlide(this);
        return;
    }
    s32 bgId;
    this->actor.floorHeight =
        BgCheck_EntityRaycastFloor5(&play->colCtx, &this->actor.floorPoly, &bgId, &this->actor, &this->actor.world.pos);
}

static void EnEiyer_Glide(EnEiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer != 0) {
        this->timer--;
    }
    f32 curFrame = this->skelAnime.curFrame;
    Math_ApproachF(&this->basePos.y, this->actor.floorHeight + STINGER_GLIDE_HEIGHT, 0.3f, this->actor.speed);
    this->actor.world.pos.y = this->basePos.y - cosf((curFrame - 5.0f) * (M_PIf / 40)) * 5.0f;
    Math_StepToF(&this->actor.speed, (curFrame <= 45.0f) ? 1.0f : 1.5f, 0.03f);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->targetYaw = this->actor.wallYaw;
    }
    if (Math_ScaledStepToS(&this->actor.world.rot.y, this->targetYaw, 0xB6)) {
        if ((this->timer != 0) || (Rand_ZeroOne() > 0.05f)) {
            this->actor.world.rot.y += 0x100;
        } else {
            s16 yawChange = Rand_S16Offset(0x2000, 0x2000);
            this->targetYaw = ((Rand_ZeroOne() < 0.5f) ? -yawChange : yawChange) + this->actor.world.rot.y;
        }
    }
    if ((this->timer == 0) && (this->actor.playerHeightRel < 0.0f) && (this->actor.xzDistToPlayer < 120.0f)) {
        this->actionFunc = EnEiyer_StartAttack;
    }
    Actor_PlaySfx_Flagged(&this->actor, STINGER_SFX_FLY - SFX_FLAG);
}

static void EnEiyer_StartAttack(EnEiyer* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&this->skelAnime);
    if (this->actor.shape.rot.x > 0) {
        Vec3f focus = { player->actor.world.pos.x, player->actor.world.pos.y + 20.0f, player->actor.world.pos.z };
        if (Math_ScaledStepToS(&this->actor.shape.rot.x, Actor_WorldPitchTowardPoint(&this->actor, &focus), 0x1000)) {
            EnEiyer_SetupDiveAttack(this, play);
        }
    } else {
        this->actor.shape.rot.x -= 0x1000;
    }
    this->actor.world.rot.x = -this->actor.shape.rot.x;
    Math_StepToF(&this->actor.speed, 5.0f, 0.3f);
    Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 2, 0x71C);
    Actor_PlaySfx_Flagged(&this->actor, STINGER_SFX_FLY - SFX_FLAG);
}

static bool EnEiyer_IsTouchingFloor(EnEiyer* this) {
    return this->actor.bgCheckFlags & (BGCHECKFLAG_WALL | BGCHECKFLAG_GROUND);
}

static void EnEiyer_DiveAttack(EnEiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->actor.speed *= 1.1f;
    if (EnEiyer_IsTouchingFloor(this)) {
        EnEiyer_SetupLand(this);
    }
    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~(AT_ON | AT_HIT);
    }
    Actor_PlaySfx_Flagged(&this->actor, STINGER_SFX_FLY - SFX_FLAG);
}

static void EnEiyer_Land(EnEiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_ScaledStepToS(&this->actor.world.rot.x, -0x4000, 0x450);
    Math_StepToF(&this->actor.speed, 7.0f, 1.0f);
    if (this->timer == -1) {
        if (EnEiyer_IsTouchingFloor(this)) {
            this->timer = 10;
            SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 30, STINGER_SFX_SINK);
            if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
                EffectSsGSplash_Spawn(play, &this->actor.world.pos, NULL, NULL, 1, 700);
            }
        }
        return;
    }
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer == 0) {
        this->actor.shape.rot.x = 0;
        this->actor.world.rot.x = 0;
        EnEiyer_SetupAppearFromGround(this);
    }
}

static void EnEiyer_Hurt(EnEiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer != 0) {
        this->timer--;
    }
    Math_ApproachF(&this->basePos.y, this->actor.floorHeight + STINGER_GLIDE_HEIGHT, 0.5f, this->actor.speed);
    this->actor.world.pos.y = this->basePos.y - 5.0f;
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->targetYaw = this->actor.wallYaw;
    } else {
        this->targetYaw = this->actor.yawTowardsPlayer + 0x8000;
    }
    Math_ScaledStepToS(&this->actor.world.rot.y, this->targetYaw, 0x38E);
    Math_ScaledStepToS(&this->actor.shape.rot.x, 0, 0x200);
    this->actor.shape.rot.z = sinf(this->timer * (M_PIf / 5)) * 5120.0f;
    if (this->timer == 0) {
        this->actor.shape.rot.x = 0;
        this->actor.shape.rot.z = 0;
        this->collider.base.acFlags |= AC_ON;
        EnEiyer_SetupGlide(this);
    }
    this->actor.world.rot.x = -this->actor.shape.rot.x;
}

static void EnEiyer_Die(EnEiyer* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_ScaledStepToS(&this->actor.shape.rot.x, (this->actor.speed > 0.0f) ? -0x4000 : 0x4000, 0x400);
    this->actor.shape.rot.z += 0x1000;
    if (this->timer != 0) {
        this->timer--;
    }
    this->actor.world.rot.x = -this->actor.shape.rot.x;
    if ((this->timer == 0) || (this->actor.bgCheckFlags & BGCHECKFLAG_CEILING)) {
        EnEiyer_SetupDead(this);
    }
}

static void EnEiyer_Dead(EnEiyer* this, PlayState* play) {
    this->actor.shape.shadowAlpha = CLAMP_MIN((s16)(this->actor.shape.shadowAlpha - 5), 0);
    this->actor.world.pos.y -= 2.0f;
    if (this->actor.shape.shadowAlpha == 0) {
        Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 80);
        Actor_Kill(&this->actor);
    }
}

static void EnEiyer_Stunned(EnEiyer* this, PlayState* play) {
    if (this->timer != 0) {
        this->timer--;
    }
    Math_ScaledStepToS(&this->actor.shape.rot.x, 0, 0x200);
    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
        Actor_PlaySfx(&this->actor, STINGER_SFX_FLUTTER);
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
        Actor_PlaySfx(&this->actor, STINGER_SFX_LAND);
    }
    if (this->timer == 0) {
        this->actor.gravity = 0.0f;
        this->actor.velocity.y = 0.0f;
        this->collider.dim.height = sColCylInit.dim.height;
        EnEiyer_SetupGlide(this);
    }
}

static void EnEiyer_ReactToHit(EnEiyer* this, PlayState* play) {
    // Buried, any damaging blow kills it at once.
    if (EnEiyer_IsBuried(this)) {
        if (this->actor.colChkInfo.damage == 0) {
            EnEiyer_SetupAmbush(this, play);
        } else {
            EnEiyer_SetupDie(this);
        }
        return;
    }
    if (this->actor.colChkInfo.damageEffect == STINGER_DMGEFF_STUN) {
        if (this->actionFunc != EnEiyer_Stunned) {
            EnEiyer_SetupStunned(this);
        }
        return;
    }
    if (this->actor.colChkInfo.health != 0) {
        Actor_PlaySfx(&this->actor, STINGER_SFX_DAMAGE);
        EnEiyer_SetupHurt(this);
        return;
    }
    this->collider.dim.height = sColCylInit.dim.height;
    EnEiyer_SetupDie(this);
}

static void EnEiyer_UpdateDamage(EnEiyer* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    Actor_SetDropFlag(&this->actor, &this->collider.elem);
    if ((this->actor.colChkInfo.damageEffect == STINGER_DMGEFF_NONE) && (this->actor.colChkInfo.damage == 0)) {
        return;
    }
    if (Actor_ApplyDamage(&this->actor) == 0) {
        Enemy_StartFinishingBlow(play, &this->actor);
        Actor_PlaySfx(&this->actor, STINGER_SFX_DEAD);
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    }
    EnEiyer_ReactToHit(this, play);
}

static bool EnEiyer_ShouldUpdateBgCheck(EnEiyer* this) {
    return (this->actionFunc == EnEiyer_Glide) || (this->actionFunc == EnEiyer_DiveAttack) ||
           (this->actionFunc == EnEiyer_Stunned) || (this->actionFunc == EnEiyer_Die) ||
           (this->actionFunc == EnEiyer_Hurt) || ((this->actionFunc == EnEiyer_Land) && (this->timer == -1));
}

static void EnEiyer_Update(Actor* thisx, PlayState* play) {
    EnEiyer* this = (EnEiyer*)thisx;

    EnEiyer_UpdateDamage(this, play);
    this->actionFunc(this, play);
    if ((this->actor.world.rot.x == 0) || (this->actionFunc == EnEiyer_Stunned)) {
        Actor_MoveWithGravity(&this->actor);
    } else {
        Actor_MoveWithoutGravity(&this->actor);
    }
    if (EnEiyer_ShouldUpdateBgCheck(this)) {
        Actor_UpdateBgCheckInfo(play, &this->actor, 5.0f, 27.0f, 30.0f, STINGER_BGCHECK_FLAGS);
    }
    this->actor.shape.rot.y = this->actor.world.rot.y;
    Collider_UpdateCylinder(&this->actor, &this->collider);
    if (this->collider.base.atFlags & AT_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->actionFunc != EnEiyer_Ambush) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->actor.flags & ACTOR_FLAG_ATTENTION_ENABLED) {
        this->actor.focus.pos.x = this->actor.world.pos.x + Math_SinS(this->actor.shape.rot.y) * 12.5f;
        this->actor.focus.pos.z = this->actor.world.pos.z + Math_CosS(this->actor.shape.rot.y) * 12.5f;
        this->actor.focus.pos.y = this->actor.world.pos.y;
    }
}

static void EnEiyer_Init(Actor* thisx, PlayState* play) {
    EnEiyer* this = (EnEiyer*)thisx;

    Actor_SetScale(&this->actor, 0.005f);
    this->actor.lockOnArrowOffset = 2500.0f;
    ActorShape_Init(&this->actor.shape, 600.0f, ActorShadow_DrawCircle, 65.0f);
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sSkel, ANIM(sIdleAnim), this->jointTable, this->morphTable,
                   EI_LIMB_MAX);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sColCylInit);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    EnEiyer_SetupAppearFromGround(this);
}

static void EnEiyer_Destroy(Actor* thisx, PlayState* play) {
    EnEiyer* this = (EnEiyer*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static s32 EnEiyer_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx,
                                    Gfx** gfx) {
    EnEiyer* this = (EnEiyer*)thisx;

    if (limbIndex == EI_LIMB_ROOT) {
        pos->z += 2500.0f;
    }
    // Buried, only the eyes show above the floor.
    if (EnEiyer_IsBuried(this) && (limbIndex != EI_LIMB_LEFT_EYE) && (limbIndex != EI_LIMB_RIGHT_EYE)) {
        *dList = NULL;
    }
    return false;
}

static void EnEiyer_Draw(Actor* thisx, PlayState* play) {
    EnEiyer* this = (EnEiyer*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    if (this->actionFunc != EnEiyer_Dead) {
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEmptyDL);
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 255, 255);
        POLY_OPA_DISP = SkelAnime_Draw(play, this->skelAnime.skeleton, this->skelAnime.jointTable,
                                       EnEiyer_OverrideLimbDraw, NULL, &this->actor, POLY_OPA_DISP);
    } else {
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)sXluRenderModeDL);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, this->actor.shape.shadowAlpha);
        POLY_XLU_DISP = SkelAnime_Draw(play, this->skelAnime.skeleton, this->skelAnime.jointTable,
                                       EnEiyer_OverrideLimbDraw, NULL, &this->actor, POLY_XLU_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// Ship (RE_STINGER, close range): a sword or the hammer (Goron here), sticks, explosives, hookshot or bow.
static bool CanKillStinger(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicHasCapability("CanBeGoron") ||
           sApi->LogicCanUseItem(RI_DEKU_STICK) || sApi->LogicHasCapability("HasExplosives") ||
           sApi->LogicCanUseItem(RI_HOOKSHOT) || sApi->LogicCanUseItem(RI_BOW);
}

// As MM's own underwater enemies: Zora's barrier, which this table lets hurt it.
static bool CanKillWaterStinger(void* userData) {
    return sApi->LogicHasCapability("CanOpenUnderwaterChest") && sApi->LogicHasCapability("HasMagic");
}

static S2HActorDefinition DefineStingerKind(const char* key, const char* description, u32 placementFlags) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = key;
    definition.description = description;
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = STINGER_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.enemyFlags = S2H_ACTOR_ENEMY | placementFlags;
    definition.soulKey = STINGER_KEY;
    definition.logicKill = CanKillStinger;
    return definition;
}

static s16 RegisterStinger(void) {
    S2HActorDefinition definition = DefineStingerKind(STINGER_KEY, "Stinger", S2H_ACTOR_ENEMY_NOT_IN_WATER);

    definition.instanceSize = sizeof(EnEiyer);
    definition.init = EnEiyer_Init;
    definition.destroy = EnEiyer_Destroy;
    definition.update = EnEiyer_Update;
    definition.draw = EnEiyer_Draw;
    definition.enemyCategory = S2H_ENEMY_CATEGORY_REGULAR;
    definition.soulModel.skeleton = sSkel;
    definition.soulModel.idleAnimation = sIdleAnim;
    definition.soulModel.limbCount = EI_LIMB_MAX;
    definition.soulModel.scale = 0.01f;
    definition.soulModel.flameRed = 230;
    definition.soulModel.flameGreen = 200;
    definition.soulModel.flameBlue = 120;
    definition.naviHint = "Stinger&It hides under the floor and dives at you&from above. Strike as it swoops down!";
    return sApi->RegisterActor(&definition);
}

void ReplaceWithLandStinger(Actor* waterStinger, PlayState* play) {
    sApi->Log(STINGER_WATER_KEY ": no swimmable water in this room; a land Stinger takes its place.");
    Actor_Spawn(&play->actorCtx, play, sLandStingerId, waterStinger->world.pos.x, waterStinger->world.pos.y,
                waterStinger->world.pos.z, 0, waterStinger->shape.rot.y, 0, 0);
    Actor_Kill(waterStinger);
}

static s16 RegisterWaterStinger(void) {
    const u32 placementFlags = S2H_ACTOR_ENEMY_NEEDS_WATER | S2H_ACTOR_ENEMY_UNDERWATER;
    S2HActorDefinition definition = DefineStingerKind(STINGER_WATER_KEY, "Stinger (water)", placementFlags);

    definition.logicKill = CanKillWaterStinger;
    SetWaterStingerFunctions(&definition);
    // It may swim off to distant water, out of the camera's culling volume.
    definition.actorFlags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.naviHint = "Stinger&It swims in deep water and rams you&when you come close. Strike it back!";
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
    if (!RequireOotAsset(EI_OBJECT "gStingerSkel")) {
        sApi->Log(STINGER_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    sLandStingerId = RegisterStinger();
    if (sLandStingerId < 0) {
        sApi->Log("Could not register " STINGER_KEY);
        return;
    }
    if (RegisterWaterStinger() < 0) {
        sApi->Log("Could not register " STINGER_WATER_KEY);
    }
}
