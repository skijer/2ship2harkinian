/*
 * Ocarina of Time's hostile Skull Kid (En_Skj) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * He plays his flute until he notices the player, then circles, shoots needles and backflips; only a sword
 * hurts him and any other hit makes him vanish. No Lost Woods minigame, songs or mask trade.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/effects/ovl_Effect_Ss_Hitmark/z_eff_ss_hitmark.h"

#define SKULL_KID_KEY "oot.skull_kid"
#define SKULL_KID_NEEDLE_KEY "oot.skull_kid.needle"
#define SKJ_OBJECT "objects/oot_object_skj/"
#define SKJ_ASSET(name) "__OTR__" SKJ_OBJECT name

static const ALIGN_ASSET(2) char sSkel[] = SKJ_ASSET("gSkullKidSkel");
static const ALIGN_ASSET(2) char sBackflipAnim[] = SKJ_ASSET("gSkullKidBackflipAnim");
static const ALIGN_ASSET(2) char sShootNeedleAnim[] = SKJ_ASSET("gSkullKidShootNeedleAnim");
static const ALIGN_ASSET(2) char sPlayFluteAnim[] = SKJ_ASSET("gSkullKidPlayFluteAnim");
static const ALIGN_ASSET(2) char sDieAnim[] = SKJ_ASSET("gSkullKidDieAnim");
static const ALIGN_ASSET(2) char sHitAnim[] = SKJ_ASSET("gSkullKidHitAnim");
static const ALIGN_ASSET(2) char sLandAnim[] = SKJ_ASSET("gSkullKidLandAnim");
static const ALIGN_ASSET(2) char sLookAroundAnim[] = SKJ_ASSET("gSkullKidLookLeftAndRightAnim");
static const ALIGN_ASSET(2) char sFightingStanceAnim[] = SKJ_ASSET("gSkullKidFightingStanceAnim");
static const ALIGN_ASSET(2) char sNeedleDL[] = SKJ_ASSET("gSkullKidNeedleDL");

#define ANIM(asset) ((AnimationHeader*)(asset))

#define SKJ_LIMB_MAX 19
#define SKULL_KID_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED)
#define SKULL_KID_NEEDLE_FLAGS (ACTOR_FLAG_HOSTILE | ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR)
#define SKJ_BGCHECK_FLAGS (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_4)
#define SKJ_ORBIT_RADIUS 120.0f
#define SKJ_HITS_UNTIL_DODGE 3
#define SKJ_NEEDLES_PER_VOLLEY 3

typedef enum {
    SKJ_DMGEFF_NONE = 0x0,
    SKJ_DMGEFF_SCARE = 0x1,
    SKJ_DMGEFF_SLASH = 0xF,
} SkullKidDamageEffect;

static const S2HModApi* sApi;
static s16 sNeedleId = -1;

struct EnSkj;
typedef void (*EnSkjActionFunc)(struct EnSkj*, PlayState*);

typedef struct EnSkj {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[SKJ_LIMB_MAX];
    Vec3s morphTable[SKJ_LIMB_MAX];
    EnSkjActionFunc actionFunc;
    ColliderCylinder collider;
    s16 battleExitTimer;
    s16 needleShootTimer;
    s16 dodgeResetTimer;
    s16 noticeYaw;
    s16 alpha;
    u8 needlesToShoot;
    u8 hitsUntilDodge;
    bool isVanishing;
    f32 noticeRange;
    f32 turnStep;
    Vec3f center;
} EnSkj;

typedef struct EnSkjneedle {
    Actor actor;
    ColliderCylinder collider;
    s16 killTimer;
} EnSkjneedle;

static ColliderCylinderInitType1 sSkullKidCylinderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x00, 0x08 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 8, 48, 0, { 0, 0, 0 } },
};

static ColliderCylinderInitType1 sNeedleCylinderInit = {
    {
        COL_MATERIAL_HIT1,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x00, 0x08 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 10, 4, -2, { 0, 0, 0 } },
};

// Vanilla: only a sword slash hurts; every other hit sends him away. MM drops 0-damage hits without an effect.
static DamageTable sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Deku Stick     */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Horse trample  */ DMG_ENTRY(0, SKJ_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Zora boomerang */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Normal arrow   */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, SKJ_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Goron punch    */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Sword          */ DMG_ENTRY(1, SKJ_DMGEFF_SLASH),
    /* Goron pound    */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Fire arrow     */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Ice arrow      */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Light arrow    */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Goron spikes   */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Deku spin      */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Deku bubble    */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Deku launch    */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, SKJ_DMGEFF_NONE),
    /* Zora barrier   */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Normal shield  */ DMG_ENTRY(0, SKJ_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, SKJ_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Zora punch     */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Spin attack    */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Sword beam     */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
    /* Normal Roll    */ DMG_ENTRY(0, SKJ_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, SKJ_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, SKJ_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, SKJ_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, SKJ_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(0, SKJ_DMGEFF_SCARE),
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Color_RGBA8 sDeathPrimColor = { 255, 255, 255, 255 };
static Color_RGBA8 sDeathEnvColor = { 0, 0, 255, 255 };

static void EnSkj_PlayFlute(EnSkj* this, PlayState* play);
static void EnSkj_LookAround(EnSkj* this, PlayState* play);
static void EnSkj_Fight(EnSkj* this, PlayState* play);
static void EnSkj_ShootNeedles(EnSkj* this, PlayState* play);
static void EnSkj_NeedleRecover(EnSkj* this, PlayState* play);
static void EnSkj_Backflip(EnSkj* this, PlayState* play);
static void EnSkj_Land(EnSkj* this, PlayState* play);
static void EnSkj_Flinch(EnSkj* this, PlayState* play);
static void EnSkj_Die(EnSkj* this, PlayState* play);
static void EnSkj_FadeAway(EnSkj* this, PlayState* play);

static void EnSkj_ChangeAnim(EnSkj* this, const char* animation, u8 mode) {
    Animation_Change(&this->skelAnime, ANIM(animation), 1.0f, 0.0f, Animation_GetLastFrame((void*)animation), mode,
                     0.0f);
}

static bool EnSkj_IsAnimDone(EnSkj* this) {
    return this->skelAnime.curFrame == this->skelAnime.endFrame;
}

static bool EnSkj_IsVulnerable(EnSkj* this) {
    return (this->actionFunc != EnSkj_Backflip) && (this->actionFunc != EnSkj_Flinch) &&
           (this->actionFunc != EnSkj_Die) && (this->actionFunc != EnSkj_FadeAway);
}

static void EnSkj_GetPointAhead(EnSkj* this, f32 side, f32 forward, Vec3f* point) {
    f32 sin = Math_SinS(this->actor.shape.rot.y);
    f32 cos = Math_CosS(this->actor.shape.rot.y);

    point->x = this->actor.world.pos.x + (side * cos) + (forward * sin);
    point->y = this->actor.world.pos.y;
    point->z = this->actor.world.pos.z - (side * sin) + (forward * cos);
}

// He orbits this point, keeping it SKJ_ORBIT_RADIUS ahead while turning to face the player.
static void EnSkj_CalculateCenter(EnSkj* this) {
    EnSkj_GetPointAhead(this, 0.0f, SKJ_ORBIT_RADIUS, &this->center);
}

static bool EnSkj_HasNoticedPlayer(EnSkj* this, PlayState* play) {
    if ((this->actor.xzDistToPlayer < this->noticeRange) && func_800B715C(play)) {
        return true;
    }
    s16 yawToPlayer = this->actor.yawTowardsPlayer - this->actor.world.rot.y;
    return (yawToPlayer < this->noticeYaw) && (-this->noticeYaw < yawToPlayer);
}

static void EnSkj_SetupPlayFlute(EnSkj* this) {
    this->noticeYaw = 0xAAA;
    this->noticeRange = 200.0f;
    EnSkj_ChangeAnim(this, sPlayFluteAnim, ANIMMODE_LOOP);
    this->actionFunc = EnSkj_PlayFlute;
}

static void EnSkj_SetupLookAround(EnSkj* this) {
    this->noticeYaw = 0x2000;
    this->battleExitTimer = 400;
    this->noticeRange = 600.0f;
    EnSkj_ChangeAnim(this, sLookAroundAnim, ANIMMODE_LOOP);
    this->actionFunc = EnSkj_LookAround;
}

static void EnSkj_SetupStand(EnSkj* this) {
    this->needleShootTimer = 60;
    this->noticeYaw = 0x2000;
    this->turnStep = 0.0f;
    this->noticeRange = 600.0f;
    EnSkj_ChangeAnim(this, sFightingStanceAnim, ANIMMODE_LOOP);
    this->actionFunc = EnSkj_Fight;
}

static void EnSkj_SetupShootNeedles(EnSkj* this) {
    this->needlesToShoot = SKJ_NEEDLES_PER_VOLLEY;
    this->needleShootTimer = 0;
    EnSkj_ChangeAnim(this, sShootNeedleAnim, ANIMMODE_ONCE);
    this->actionFunc = EnSkj_ShootNeedles;
}

static void EnSkj_SetupNeedleRecover(EnSkj* this) {
    Animation_Reverse(&this->skelAnime);
    this->actionFunc = EnSkj_NeedleRecover;
}

static void EnSkj_SetupBackflip(EnSkj* this) {
    this->actor.velocity.y = 8.0f;
    this->actor.speed = -8.0f;
    EnSkj_ChangeAnim(this, sBackflipAnim, ANIMMODE_ONCE);
    this->actionFunc = EnSkj_Backflip;
}

static void EnSkj_SetupLand(EnSkj* this) {
    EnSkj_CalculateCenter(this);
    this->actor.speed = 0.0f;
    EnSkj_ChangeAnim(this, sLandAnim, ANIMMODE_ONCE);
    this->actionFunc = EnSkj_Land;
}

static void EnSkj_SetupFlinch(EnSkj* this) {
    EnSkj_ChangeAnim(this, sHitAnim, ANIMMODE_ONCE);
    this->actionFunc = EnSkj_Flinch;
}

static void EnSkj_SetupDie(EnSkj* this) {
    EnSkj_ChangeAnim(this, sDieAnim, ANIMMODE_ONCE);
    this->actionFunc = EnSkj_Die;
}

static void EnSkj_SetupFadeAway(EnSkj* this) {
    this->isVanishing = true;
    this->actionFunc = EnSkj_FadeAway;
}

static void EnSkj_PlayFlute(EnSkj* this, PlayState* play) {
    if (EnSkj_HasNoticedPlayer(this, play)) {
        EnSkj_SetupLookAround(this);
    }
}

static void EnSkj_LookAround(EnSkj* this, PlayState* play) {
    if (this->battleExitTimer == 0) {
        EnSkj_SetupPlayFlute(this);
    } else if (EnSkj_HasNoticedPlayer(this, play)) {
        this->battleExitTimer = 600;
        EnSkj_SetupStand(this);
    }
}

static void EnSkj_Fight(EnSkj* this, PlayState* play) {
    if (this->needleShootTimer == 0) {
        EnSkj_SetupShootNeedles(this);
        return;
    }
    if (this->battleExitTimer == 0) {
        if (EnSkj_HasNoticedPlayer(this, play)) {
            this->isVanishing = true;
            EnSkj_SetupBackflip(this);
        } else {
            EnSkj_SetupPlayFlute(this);
        }
        return;
    }
    s16 yawLeft = Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 5, this->turnStep, 0);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    Math_ApproachF(&this->turnStep, 2000.0f, 1.0f, 200.0f);
    Vec3f prevPos = this->actor.world.pos;
    this->actor.world.pos.x = this->center.x - Math_SinS(this->actor.shape.rot.y) * SKJ_ORBIT_RADIUS;
    this->actor.world.pos.z = this->center.z - Math_CosS(this->actor.shape.rot.y) * SKJ_ORBIT_RADIUS;
    f32 stride = CLAMP_MAX(Math_Vec3f_DistXZ(&this->actor.world.pos, &prevPos), 10.0f) / 10.0f;
    this->skelAnime.playSpeed = (yawLeft < 0) ? -(1.0f + stride) : (1.0f + stride);
}

static void EnSkj_ShootNeedle(EnSkj* this, PlayState* play) {
    Vec3f muzzle;

    EnSkj_GetPointAhead(this, 1.5f, 40.0f, &muzzle);
    Actor_Spawn(&play->actorCtx, play, sNeedleId, muzzle.x, muzzle.y + 27.0f, muzzle.z, this->actor.shape.rot.x,
                this->actor.shape.rot.y, this->actor.shape.rot.z, 0);
}

static void EnSkj_ShootNeedles(EnSkj* this, PlayState* play) {
    if (!EnSkj_IsAnimDone(this) || (this->needleShootTimer != 0)) {
        return;
    }
    if (this->needlesToShoot == 0) {
        EnSkj_SetupNeedleRecover(this);
        return;
    }
    EnSkj_ShootNeedle(this, play);
    this->needleShootTimer = 4;
    this->needlesToShoot--;
}

static void EnSkj_NeedleRecover(EnSkj* this, PlayState* play) {
    if (EnSkj_IsAnimDone(this)) {
        EnSkj_SetupStand(this);
    }
}

// A vanishing backflip fades him out; the update removes him once invisible.
static void EnSkj_Backflip(EnSkj* this, PlayState* play) {
    if (this->isVanishing) {
        this->alpha = CLAMP_MIN(this->alpha - 20, 0);
        this->actor.shape.shadowAlpha = this->alpha;
    }
    if ((this->actor.velocity.y <= 0.0f) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH)) {
        this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND_TOUCH;
        EnSkj_SetupLand(this);
    }
}

static void EnSkj_Land(EnSkj* this, PlayState* play) {
    if (EnSkj_IsAnimDone(this)) {
        EnSkj_SetupStand(this);
    }
}

static void EnSkj_Flinch(EnSkj* this, PlayState* play) {
    if (!EnSkj_IsAnimDone(this)) {
        return;
    }
    if (this->hitsUntilDodge == 0) {
        this->hitsUntilDodge = SKJ_HITS_UNTIL_DODGE;
        EnSkj_SetupBackflip(this);
    } else {
        EnSkj_SetupStand(this);
    }
}

static void EnSkj_Die(EnSkj* this, PlayState* play) {
    if (EnSkj_IsAnimDone(this)) {
        EnSkj_SetupFadeAway(this);
    }
}

static void EnSkj_FadeAway(EnSkj* this, PlayState* play) {
    this->alpha = CLAMP_MIN(this->alpha - 4, 0);
    this->actor.shape.shadowAlpha = this->alpha;
    Vec3f effectPos = {
        Rand_CenteredFloat(30.0f) + this->actor.world.pos.x,
        Rand_CenteredFloat(30.0f) + this->actor.world.pos.y,
        Rand_CenteredFloat(30.0f) + this->actor.world.pos.z,
    };
    EffectSsDeadDb_Spawn(play, &effectPos, &sZeroVec, &sZeroVec, &sDeathPrimColor, &sDeathEnvColor, 100, 10, 9);
}

static void EnSkj_SpawnBlood(PlayState* play, Vec3f* pos) {
    EffectSparkInit spark;
    s32 index;

    spark.position.x = pos->x;
    spark.position.y = pos->y;
    spark.position.z = pos->z;
    spark.uDiv = 5;
    spark.vDiv = 5;
    for (s32 i = 0; i < ARRAY_COUNT(spark.colorStart); i++) {
        spark.colorStart[i] = (Color_RGBA8){ 0, 0, 128, 255 };
        spark.colorEnd[i] = (Color_RGBA8){ 0, 0, (i < 2) ? 32 : 64, 0 };
    }
    spark.speed = 8.0f;
    spark.gravity = -1.0f;
    spark.timer = 0;
    spark.duration = 8;
    Effect_Add(play, &index, EFFECT_SPARK, 0, 1, &spark);
}

// Struck from behind before he joins the fight, he dies at once.
static bool EnSkj_IsCaughtUnaware(EnSkj* this) {
    if ((this->actionFunc != EnSkj_PlayFlute) && (this->actionFunc != EnSkj_LookAround)) {
        return false;
    }
    s16 yawToPlayer = this->actor.yawTowardsPlayer - this->actor.world.rot.y;
    return (yawToPlayer > 0x6000) || (yawToPlayer < -0x6000);
}

static void EnSkj_TakeSlash(EnSkj* this, PlayState* play) {
    Vec3f hitPos;

    Math_Vec3s_ToVec3f(&hitPos, &this->collider.elem.acDmgInfo.hitPos);
    EnSkj_SpawnBlood(play, &hitPos);
    EffectSsHitmark_SpawnFixedScale(play, EFFECT_HITMARK_DUST, &hitPos);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
    if (EnSkj_IsCaughtUnaware(this) || (Actor_ApplyDamage(&this->actor) == 0)) {
        EnSkj_SetupDie(this);
        return;
    }
    if (this->hitsUntilDodge != 0) {
        this->hitsUntilDodge--;
    }
    if (this->dodgeResetTimer == 0) {
        this->dodgeResetTimer = 60;
    }
    EnSkj_SetupFlinch(this);
}

// A hit landing while he is busy waits until he can be hit again, as in vanilla.
static void EnSkj_UpdateDamage(EnSkj* this, PlayState* play) {
    if (!EnSkj_IsVulnerable(this) || !(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    switch (this->actor.colChkInfo.damageEffect) {
        case SKJ_DMGEFF_SLASH:
            EnSkj_TakeSlash(this, play);
            break;
        case SKJ_DMGEFF_SCARE:
            this->isVanishing = true;
            EnSkj_SetupBackflip(this);
            break;
        default:
            break;
    }
}

static void EnSkj_UpdateTimers(EnSkj* this) {
    if (this->battleExitTimer != 0) {
        this->battleExitTimer--;
    }
    if (this->needleShootTimer != 0) {
        this->needleShootTimer--;
    }
    if (this->dodgeResetTimer != 0) {
        this->dodgeResetTimer--;
    }
    if (this->dodgeResetTimer == 0) {
        this->hitsUntilDodge = SKJ_HITS_UNTIL_DODGE;
    }
}

static void EnSkj_Update(Actor* thisx, PlayState* play) {
    EnSkj* this = (EnSkj*)thisx;

    EnSkj_UpdateTimers(this);
    if (this->isVanishing && (this->alpha == 0)) {
        if (this->actionFunc == EnSkj_FadeAway) {
            Item_DropCollectible(play, &this->actor.world.pos, ITEM00_RUPEE_HUGE);
        }
        Actor_Kill(&this->actor);
        return;
    }
    Actor_SetFocus(&this->actor, 30.0f);
    this->actionFunc(this, play);
    EnSkj_UpdateDamage(this, play);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    if (EnSkj_IsVulnerable(this)) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        if (this->actor.colorFilterTimer == 0) {
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }
    }
    SkelAnime_Update(&this->skelAnime);
    Actor_MoveWithGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 20.0f, 20.0f, SKJ_BGCHECK_FLAGS);
}

static void EnSkj_Init(Actor* thisx, PlayState* play) {
    EnSkj* this = (EnSkj*)thisx;

    this->actor.attentionRangeType = ATTENTION_RANGE_2;
    this->actor.lockOnArrowOffset = 30.0f;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, ANIM(sPlayFluteAnim), this->jointTable,
                       this->morphTable, SKJ_LIMB_MAX);
    this->alpha = 255;
    EnSkj_SetupPlayFlute(this);
    this->actor.colChkInfo.damageTable = &sDamageTable;
    this->actor.colChkInfo.health = 10;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinderType1(play, &this->collider, &this->actor, &sSkullKidCylinderInit);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 40.0f);
    Actor_SetScale(&this->actor, 0.01f);
    this->needlesToShoot = SKJ_NEEDLES_PER_VOLLEY;
    this->hitsUntilDodge = SKJ_HITS_UNTIL_DODGE;
    this->actor.gravity = -1.0f;
    EnSkj_CalculateCenter(this);
}

static void EnSkj_Destroy(Actor* thisx, PlayState* play) {
    EnSkj* this = (EnSkj*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

// The body DLs take their render mode and alpha from segment 0x0C.
static Gfx* EnSkj_BuildAlphaDL(GraphicsContext* gfxCtx, s16 alpha) {
    Gfx* dList = GRAPH_ALLOC(gfxCtx, 3 * sizeof(Gfx));
    Gfx* head = dList;

    if (alpha < 255) {
        gDPSetRenderMode(head++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2);
    }
    gDPSetEnvColor(head++, 0, 0, 0, alpha);
    gSPEndDisplayList(head++);
    return dList;
}

static void EnSkj_Draw(Actor* thisx, PlayState* play) {
    EnSkj* this = (EnSkj*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)EnSkj_BuildAlphaDL(play->state.gfxCtx, this->alpha));
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          NULL, &this->actor);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnSkjneedle_Init(Actor* thisx, PlayState* play) {
    EnSkjneedle* this = (EnSkjneedle*)thisx;

    this->actor.attentionRangeType = ATTENTION_RANGE_2;
    this->actor.lockOnArrowOffset = 30.0f;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinderType1(play, &this->collider, &this->actor, &sNeedleCylinderInit);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 20.0f);
    Actor_SetScale(&this->actor, 0.01f);
    this->killTimer = 100;
    this->actor.speed = 24.0f;
}

static void EnSkjneedle_Destroy(Actor* thisx, PlayState* play) {
    EnSkjneedle* this = (EnSkjneedle*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void EnSkjneedle_Update(Actor* thisx, PlayState* play) {
    EnSkjneedle* this = (EnSkjneedle*)thisx;

    if (this->killTimer != 0) {
        this->killTimer--;
    }
    if ((this->collider.base.atFlags & AT_HIT) || (this->killTimer == 0)) {
        Actor_Kill(&this->actor);
        return;
    }
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    Actor_MoveWithGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 20.0f, 20.0f, SKJ_BGCHECK_FLAGS);
}

static void EnSkjneedle_Draw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sNeedleDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Only a sword kills him; any other hit just makes him flee.
static bool CanKillSkullKid(void* userData) {
    return sApi->LogicHasCapability("CanUseSword");
}

static s16 RegisterNeedle(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = SKULL_KID_NEEDLE_KEY;
    definition.description = "Skull Kid's needle";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = SKULL_KID_NEEDLE_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnSkjneedle);
    definition.init = EnSkjneedle_Init;
    definition.destroy = EnSkjneedle_Destroy;
    definition.update = EnSkjneedle_Update;
    definition.draw = EnSkjneedle_Draw;
    return sApi->RegisterActor(&definition);
}

static s16 RegisterSkullKid(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = SKULL_KID_KEY;
    definition.description = "Skull Kid";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = SKULL_KID_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnSkj);
    definition.init = EnSkj_Init;
    definition.destroy = EnSkj_Destroy;
    definition.update = EnSkj_Update;
    definition.draw = EnSkj_Draw;
    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    definition.naviHint = "Skull Kid&Only a sword hurts him; anything else&scares him away. Strike from behind!";
    // Ship's catalog gives him no soul.
    definition.soulKey = NULL;
    definition.logicKill = CanKillSkullKid;
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
    if (!RequireOotAsset(SKJ_OBJECT "gSkullKidSkel")) {
        sApi->Log(SKULL_KID_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    sNeedleId = RegisterNeedle();
    if (sNeedleId < 0) {
        sApi->Log("Could not register " SKULL_KID_NEEDLE_KEY);
        return;
    }
    if (RegisterSkullKid() < 0) {
        sApi->Log("Could not register " SKULL_KID_KEY);
    }
}
