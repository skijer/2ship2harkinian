/*
 * Ocarina of Time's Baby Dodongo (En_Dodojr) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * It burrows up near the player, swallows loose bombs and blows up when it dies.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"

#define BABY_DODONGO_KEY "oot.baby_dodongo"
#define DODONGO_FAMILY_KEY "oot.dodongo"
#define DODONGO_SOUL_ITEM_KEY "soul.oot.dodongo"
#define DODOJR_OBJECT "objects/oot_object_dodojr/"
#define DODOJR_ASSET(name) "__OTR__" DODOJR_OBJECT name

static const ALIGN_ASSET(2) char sSkel[] = DODOJR_ASSET("object_dodojr_Skel_0020E0");
static const ALIGN_ASSET(2) char sFlipAnim[] = DODOJR_ASSET("object_dodojr_Anim_0004A0");
static const ALIGN_ASSET(2) char sDeathAnim[] = DODOJR_ASSET("object_dodojr_Anim_0005F0");
static const ALIGN_ASSET(2) char sJumpAnim[] = DODOJR_ASSET("object_dodojr_Anim_000724");
static const ALIGN_ASSET(2) char sCrawlAnim[] = DODOJR_ASSET("object_dodojr_Anim_000860");
static const ALIGN_ASSET(2) char sIdleAnim[] = DODOJR_ASSET("object_dodojr_Anim_0009D4");

// MM reuses OoT's ids of these for other actors.
#define DODOJR_SFX_BOUNCE NA_SE_EN_FALL_LAND
#define DODOJR_SFX_SWALLOW NA_SE_EN_DODO_J_EAT
#define DODOJR_SFX_FREEZE NA_SE_EN_COMMON_FREEZE

#define ANIM(asset) ((AnimationHeader*)(asset))
#define LAST_FRAME(asset) Animation_GetLastFrame((void*)(asset))

#define DODOJR_LIMB_ROOT 1
#define DODOJR_LIMB_HEAD 4
#define DODOJR_LIMB_MAX 15

#define DODOJR_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE)
#define DODOJR_BGCHECK_FLAGS (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4)
#define DODOJR_GRAVITY -0.8f
#define DODOJR_CRAWL_SPEED 2.6f

typedef enum {
    DODOJR_DMGEFF_NONE = 0x0,
    DODOJR_DMGEFF_STUN = 0x1,
} DodojrDamageEffect;

static const S2HModApi* sApi;

struct EnDodojr;
typedef void (*EnDodojrActionFunc)(struct EnDodojr*, PlayState*);

typedef struct EnDodojr {
    Actor actor;
    SkelAnime skelAnime;
    EnDodojrActionFunc actionFunc;
    ColliderCylinder collider;
    Actor* bomb;
    Vec3f headPos;
    Vec3f dustPos;
    s16 bounceCount;
    s16 stunTimer;
    s16 freezeFrames;
    s16 actionTimer;
    s16 crawlSfxTimer;
    f32 rootScale;
    Vec3s jointTable[DODOJR_LIMB_MAX];
    Vec3s morphTable[DODOJR_LIMB_MAX];
} EnDodojr;

static ColliderCylinderInit sCylinderInit = {
    {
        COL_MATERIAL_HIT6,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
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
    { 18, 20, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit2 sColChkInit = { 1, 2, 25, 25, MASS_IMMOVABLE };

// Vanilla's preset 4: one damage kills it, Deku Nuts stun it.
static DamageTable sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, DODOJR_DMGEFF_STUN),
    /* Deku Stick     */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Horse trample  */ DMG_ENTRY(0, DODOJR_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Zora boomerang */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Normal arrow   */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, DODOJR_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Goron punch    */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Sword          */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Goron pound    */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Fire arrow     */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Ice arrow      */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Light arrow    */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Goron spikes   */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Deku spin      */ DMG_ENTRY(0, DODOJR_DMGEFF_STUN),
    /* Deku bubble    */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Deku launch    */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, DODOJR_DMGEFF_NONE),
    /* Zora barrier   */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Normal shield  */ DMG_ENTRY(0, DODOJR_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, DODOJR_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Zora punch     */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Spin attack    */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Sword beam     */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
    /* Normal Roll    */ DMG_ENTRY(0, DODOJR_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, DODOJR_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, DODOJR_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, DODOJR_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, DODOJR_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(1, DODOJR_DMGEFF_NONE),
};

static void EnDodojr_WaitUnderground(EnDodojr* this, PlayState* play);
static void EnDodojr_EmergeFromGround(EnDodojr* this, PlayState* play);
static void EnDodojr_CrawlTowardsTarget(EnDodojr* this, PlayState* play);
static void EnDodojr_EatBomb(EnDodojr* this, PlayState* play);
static void EnDodojr_SwallowBomb(EnDodojr* this, PlayState* play);
static void EnDodojr_SwallowedBombDeathBounce(EnDodojr* this, PlayState* play);
static void EnDodojr_StunnedBounce(EnDodojr* this, PlayState* play);
static void EnDodojr_Stunned(EnDodojr* this, PlayState* play);
static void EnDodojr_JumpAttackBounce(EnDodojr* this, PlayState* play);
static void EnDodojr_Despawn(EnDodojr* this, PlayState* play);
static void EnDodojr_StandardDeathBounce(EnDodojr* this, PlayState* play);
static void EnDodojr_DeathSequence(EnDodojr* this, PlayState* play);
static void EnDodojr_DropItem(EnDodojr* this, PlayState* play);
static void EnDodojr_WaitFreezeFrames(EnDodojr* this, PlayState* play);

static void EnDodojr_SpawnLargeDust(EnDodojr* this, PlayState* play, s32 count) {
    Color_RGBA8 prim = { 170, 130, 90, 255 };
    Color_RGBA8 env = { 100, 60, 20, 0 };
    Vec3f velocity = { 0.0f, 0.0f, 0.0f };
    Vec3f accel = { 0.0f, 0.3f, 0.0f };
    Vec3f pos;
    s16 angle = (Rand_ZeroOne() - 0.5f) * 65536.0f;

    pos.y = this->dustPos.y;
    for (s32 i = count; i >= 0; i--) {
        accel.x = (Rand_ZeroOne() - 0.5f) * 4.0f;
        accel.z = (Rand_ZeroOne() - 0.5f) * 4.0f;
        pos.x = (Math_SinS(angle) * 22.0f) + this->dustPos.x;
        pos.z = (Math_CosS(angle) * 22.0f) + this->dustPos.z;
        func_800B0EB0(play, &pos, &velocity, &accel, &prim, &env, 120, 40, 10);
        angle += (s16)(0x10000 / count);
    }
}

static void EnDodojr_SpawnSmallDust(EnDodojr* this, PlayState* play) {
    Color_RGBA8 prim = { 170, 130, 90, 255 };
    Color_RGBA8 env = { 100, 60, 20, 0 };
    Vec3f velocity = { 0.0f, 0.0f, 0.0f };
    Vec3f accel = { (Rand_ZeroOne() - 0.5f) * 2, 0.3f, (Rand_ZeroOne() - 0.5f) * 2 };
    s16 angle = (Rand_ZeroOne() - 0.5f) * 65536.0f;
    Vec3f pos = {
        (Math_SinS(angle) * 11.0f) + this->actor.world.pos.x,
        this->actor.floorHeight,
        (Math_CosS(angle) * 11.0f) + this->actor.world.pos.z,
    };

    func_800B0EB0(play, &pos, &velocity, &accel, &prim, &env, 100, 60, 8);
}

// Returns true on the bounce that brings it to rest.
static bool EnDodojr_UpdateBounces(EnDodojr* this, PlayState* play) {
    if ((this->actor.velocity.y >= 0.0f) || (this->bounceCount == 0)) {
        return false;
    }
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        return false;
    }
    Actor_PlaySfx(&this->actor, DODOJR_SFX_BOUNCE);
    this->dustPos = this->actor.world.pos;
    EnDodojr_SpawnLargeDust(this, play, 10);
    this->actor.velocity.y = 10.0f / (4 - this->bounceCount);
    this->bounceCount--;
    if (this->bounceCount != 0) {
        return false;
    }
    this->actor.velocity.y = 0.0f;
    return true;
}

static void EnDodojr_SetupCrawlTowardsTarget(EnDodojr* this) {
    Animation_Change(&this->skelAnime, ANIM(sCrawlAnim), 1.8f, 0.0f, LAST_FRAME(sCrawlAnim), ANIMMODE_LOOP_INTERP,
                     -10.0f);
    this->actor.velocity.y = 0.0f;
    this->actor.speed = DODOJR_CRAWL_SPEED;
    this->actor.gravity = DODOJR_GRAVITY;
    this->actionFunc = EnDodojr_CrawlTowardsTarget;
}

static void EnDodojr_SetupFlipBounce(EnDodojr* this) {
    Animation_Change(&this->skelAnime, ANIM(sFlipAnim), 1.0f, 0.0f, LAST_FRAME(sFlipAnim), ANIMMODE_ONCE, -10.0f);
    this->actor.speed = 0.0f;
    this->actor.velocity.x = 0.0f;
    this->actor.velocity.z = 0.0f;
    this->actor.gravity = DODOJR_GRAVITY;
    if (this->bounceCount == 0) {
        this->bounceCount = 3;
        this->actor.velocity.y = 10.0f;
    }
}

static void EnDodojr_SetupDeathAnim(EnDodojr* this) {
    Animation_Change(&this->skelAnime, ANIM(sDeathAnim), 1.0f, 0.0f, LAST_FRAME(sDeathAnim), ANIMMODE_LOOP, 0.0f);
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = DODOJR_GRAVITY;
}

static void EnDodojr_SetupJumpAttackBounce(EnDodojr* this) {
    Animation_Change(&this->skelAnime, ANIM(sJumpAnim), 1.0f, 0.0f, LAST_FRAME(sJumpAnim), ANIMMODE_LOOP, -10.0f);
    this->actor.gravity = DODOJR_GRAVITY;
    this->bounceCount = 3;
    this->actor.velocity.y = 10.0f;
    this->actionFunc = EnDodojr_JumpAttackBounce;
}

static void EnDodojr_SetupDespawn(EnDodojr* this) {
    this->actor.shape.shadowDraw = NULL;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.home.pos = this->actor.world.pos;
    this->actor.speed = 0.0f;
    this->actor.gravity = DODOJR_GRAVITY;
    this->actionTimer = 30;
    this->dustPos = this->actor.world.pos;
    this->actionFunc = EnDodojr_Despawn;
}

static void EnDodojr_SetupEatBomb(EnDodojr* this) {
    Animation_Change(&this->skelAnime, ANIM(sJumpAnim), 1.0f, 8.0f, 12.0f, ANIMMODE_ONCE, 0.0f);
    Actor_PlaySfx(&this->actor, NA_SE_EN_DODO_M_EAT);
    this->actor.speed = 0.0f;
    this->actor.velocity.x = 0.0f;
    this->actor.velocity.z = 0.0f;
    this->actor.gravity = DODOJR_GRAVITY;
    this->actionFunc = EnDodojr_EatBomb;
}

static bool EnDodojr_IsEdibleBomb(Actor* bomb) {
    return (bomb->id == ACTOR_EN_BOM) && (bomb->params == BOMB_TYPE_BODY) && !((EnBom*)bomb)->isPowderKeg &&
           (bomb->parent == NULL) && (bomb->update != NULL);
}

// The bomb may have exploded and been freed since last frame: only a pointer still in the list is safe to touch.
static bool EnDodojr_IsBombListed(PlayState* play, Actor* bomb) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_EXPLOSIVES].first; actor != NULL; actor = actor->next) {
        if (actor == bomb) {
            return actor->update != NULL;
        }
    }
    return false;
}

static void EnDodojr_ForgetFreedBomb(EnDodojr* this, PlayState* play) {
    if ((this->bomb != NULL) && !EnDodojr_IsBombListed(play, this->bomb)) {
        this->bomb = NULL;
    }
}

static void EnDodojr_FindNearestBomb(EnDodojr* this, PlayState* play) {
    Vec3f nearest = { 99999.0f, 99999.0f, 99999.0f };

    this->bomb = NULL;
    for (Actor* bomb = play->actorCtx.actorLists[ACTORCAT_EXPLOSIVES].first; bomb != NULL; bomb = bomb->next) {
        if (!EnDodojr_IsEdibleBomb(bomb)) {
            continue;
        }
        f32 xDist = bomb->world.pos.x - this->actor.world.pos.x;
        f32 yDist = bomb->world.pos.y - this->actor.world.pos.y;
        f32 zDist = bomb->world.pos.z - this->actor.world.pos.z;
        if ((fabsf(xDist) >= fabsf(nearest.x)) || (fabsf(yDist) >= fabsf(nearest.y)) ||
            (fabsf(zDist) >= fabsf(nearest.z))) {
            continue;
        }
        this->bomb = bomb;
        nearest = bomb->world.pos;
    }
}

static bool EnDodojr_TryGrabBomb(EnDodojr* this) {
    if ((this->bomb == NULL) || (this->bomb->parent != NULL)) {
        return false;
    }
    if (Math_Vec3f_DistXYZ(&this->actor.world.pos, &this->bomb->world.pos) > 30.0f) {
        return false;
    }
    this->bomb->parent = &this->actor;
    return true;
}

static void EnDodojr_SteerTowardsTarget(EnDodojr* this, PlayState* play) {
    // Vanilla feeds these degrees to sinf/cosf as radians; kept for the same wander.
    static const f32 sWanderAngles[] = { 0.0f,   210.0f, 60.0f,  270.0f, 120.0f, 330.0f,
                                         180.0f, 30.0f,  240.0f, 90.0f,  300.0f, 150.0f };
    Vec3f target;

    if ((this->bomb == NULL) || (this->bomb->update == NULL) || (this->bomb->parent != NULL)) {
        EnDodojr_FindNearestBomb(this, play);
    }
    target = (this->bomb != NULL) ? this->bomb->world.pos : GET_PLAYER(play)->actor.world.pos;
    if (Math_Vec3f_DistXYZ(&this->actor.world.pos, &target) > 80.0f) {
        s16 angleIndex =
            (s16)(this->actor.home.pos.x + this->actor.home.pos.y + this->actor.home.pos.z + play->state.frames / 30) %
            12;
        angleIndex = ABS(angleIndex);
        target.x += 80.0f * sinf(sWanderAngles[angleIndex]);
        target.z += 80.0f * cosf(sWanderAngles[angleIndex]);
    }
    Math_SmoothStepToS(&this->actor.world.rot.y, Math_Vec3f_Yaw(&this->actor.world.pos, &target), 10, 1000, 1);
    this->actor.shape.rot.y = this->actor.world.rot.y;
}

static void EnDodojr_SetupStandardDeathBounce(EnDodojr* this) {
    Actor_PlaySfx(&this->actor, NA_SE_EN_DODO_M_DEAD);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    EnDodojr_SetupFlipBounce(this);
    this->actionFunc = EnDodojr_StandardDeathBounce;
}

static bool EnDodojr_IsDying(EnDodojr* this) {
    return (this->actionFunc == EnDodojr_SwallowBomb) || (this->actionFunc == EnDodojr_SwallowedBombDeathBounce) ||
           (this->actionFunc == EnDodojr_Despawn) || (this->actionFunc == EnDodojr_StandardDeathBounce) ||
           (this->actionFunc == EnDodojr_DeathSequence) || (this->actionFunc == EnDodojr_DropItem);
}

static void EnDodojr_CheckDamaged(EnDodojr* this, PlayState* play) {
    if (EnDodojr_IsDying(this)) {
        return;
    }
    bool isUnderground =
        (this->actionFunc == EnDodojr_WaitUnderground) || (this->actionFunc == EnDodojr_EmergeFromGround);
    // OoT's hammer quake flips it over; in MM the Goron pound raises the same flag.
    if (play->actorCtx.unk2 != 0) {
        if (this->actionFunc == EnDodojr_WaitUnderground) {
            return;
        }
        if (this->actionFunc == EnDodojr_EmergeFromGround) {
            this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
        }
        EnDodojr_SetupStandardDeathBounce(this);
        return;
    }
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    if (isUnderground) {
        this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
    }
    if ((this->actor.colChkInfo.damageEffect == DODOJR_DMGEFF_NONE) && (this->actor.colChkInfo.damage != 0)) {
        Enemy_StartFinishingBlow(play, &this->actor);
        this->freezeFrames = 2;
        this->actionFunc = EnDodojr_WaitFreezeFrames;
        return;
    }
    bool isStunned = (this->actionFunc == EnDodojr_Stunned) || (this->actionFunc == EnDodojr_StunnedBounce);
    if ((this->actor.colChkInfo.damageEffect == DODOJR_DMGEFF_STUN) && !isStunned) {
        Actor_PlaySfx(&this->actor, DODOJR_SFX_FREEZE);
        this->stunTimer = 120;
        Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 200, COLORFILTER_BUFFLAG_OPA, 120);
        EnDodojr_SetupFlipBounce(this);
        this->actionFunc = EnDodojr_StunnedBounce;
    }
}

static void EnDodojr_UpdateCollider(EnDodojr* this, PlayState* play) {
    Collider_UpdateCylinder(&this->actor, &this->collider);
    if ((this->actionFunc == EnDodojr_WaitUnderground) || (this->actionFunc == EnDodojr_DropItem)) {
        return;
    }
    bool isAttacking = (this->actionFunc == EnDodojr_EmergeFromGround) ||
                       (this->actionFunc == EnDodojr_CrawlTowardsTarget) ||
                       (this->actionFunc == EnDodojr_JumpAttackBounce);
    if (isAttacking) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (isAttacking || (this->actionFunc == EnDodojr_StunnedBounce) || (this->actionFunc == EnDodojr_Stunned)) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void EnDodojr_WaitUnderground(EnDodojr* this, PlayState* play) {
    if (this->actor.xzDistToPlayer >= 320.0f) {
        return;
    }
    if (this->actor.world.pos.y - GET_PLAYER(play)->actor.world.pos.y >= 40.0f) {
        return;
    }
    Animation_Change(&this->skelAnime, ANIM(sCrawlAnim), 1.8f, 0.0f, LAST_FRAME(sCrawlAnim), ANIMMODE_LOOP_INTERP,
                     -10.0f);
    Actor_PlaySfx(&this->actor, NA_SE_EN_DODO_M_UP);
    this->actor.world.pos.y -= 60.0f;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.world.rot.x -= 0x4000;
    this->actor.shape.rot.x = this->actor.world.rot.x;
    this->dustPos = this->actor.world.pos;
    this->dustPos.y = this->actor.floorHeight;
    this->actionFunc = EnDodojr_EmergeFromGround;
}

static void EnDodojr_EmergeFromGround(EnDodojr* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.x, 0, 4, 0x3E8, 0x64);
    f32 tilt = this->actor.shape.rot.x / 16384.0f;
    this->actor.world.pos.y = this->actor.home.pos.y + (60.0f * tilt);
    EnDodojr_SpawnLargeDust(this, play, 3);
    if (tilt != 0.0f) {
        return;
    }
    this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
    this->actor.world.rot.x = this->actor.shape.rot.x;
    this->actor.speed = DODOJR_CRAWL_SPEED;
    this->actionFunc = EnDodojr_CrawlTowardsTarget;
}

static void EnDodojr_CrawlTowardsTarget(EnDodojr* this, PlayState* play) {
    Actor_UpdateVelocityWithGravity(&this->actor);
    EnDodojr_SpawnSmallDust(this, play);
    if (DECR(this->crawlSfxTimer) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_DODO_M_MOVE);
        this->crawlSfxTimer = 5;
    }
    if (EnDodojr_TryGrabBomb(this)) {
        EnDodojr_SetupEatBomb(this);
        return;
    }
    EnDodojr_SteerTowardsTarget(this, play);
    if (this->actor.xzDistToPlayer <= 40.0f) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_DODO_M_CRY);
        EnDodojr_SetupJumpAttackBounce(this);
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_DODO_M_DOWN);
        EnDodojr_SetupDespawn(this);
    }
}

static void EnDodojr_EatBomb(EnDodojr* this, PlayState* play) {
    if (this->bomb == NULL) {
        EnDodojr_SetupCrawlTowardsTarget(this);
        return;
    }
    if (((s16)this->skelAnime.curFrame - 8) < 4) {
        ((EnBom*)this->bomb)->timer++;
        this->bomb->world.pos = this->headPos;
        return;
    }
    Actor_PlaySfx(&this->actor, DODOJR_SFX_SWALLOW);
    Actor_Kill(this->bomb);
    this->bomb = NULL;
    this->actionTimer = 24;
    this->bounceCount = 0;
    this->actionFunc = EnDodojr_SwallowBomb;
}

static void EnDodojr_SwallowBomb(EnDodojr* this, PlayState* play) {
    if (DECR(this->actionTimer) != 0) {
        return;
    }
    Actor_PlaySfx(&this->actor, NA_SE_IT_BOMB_EXPLOSION);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 200, COLORFILTER_BUFFLAG_OPA, 8);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    EnDodojr_SetupFlipBounce(this);
    this->actionFunc = EnDodojr_SwallowedBombDeathBounce;
}

static void EnDodojr_StartDeathSequence(EnDodojr* this) {
    this->actionTimer = 60;
    EnDodojr_SetupDeathAnim(this);
    this->bounceCount = 7;
    this->actionFunc = EnDodojr_DeathSequence;
}

static void EnDodojr_SwallowedBombDeathBounce(EnDodojr* this, PlayState* play) {
    this->rootScale = 1.2f * ((f32)this->actor.colorFilterTimer / 8);
    Actor_UpdateVelocityWithGravity(&this->actor);
    if (EnDodojr_UpdateBounces(this, play)) {
        EnDodojr_StartDeathSequence(this);
    }
}

static void EnDodojr_StunnedBounce(EnDodojr* this, PlayState* play) {
    Actor_UpdateVelocityWithGravity(&this->actor);
    if (EnDodojr_UpdateBounces(this, play)) {
        EnDodojr_SetupDeathAnim(this);
        this->actionFunc = EnDodojr_Stunned;
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, 0, 4, 1000, 10);
    this->actor.world.rot.x = this->actor.shape.rot.x;
    this->actor.colorFilterTimer = this->stunTimer;
}

static void EnDodojr_Stunned(EnDodojr* this, PlayState* play) {
    if (DECR(this->stunTimer) == 0) {
        EnDodojr_SetupCrawlTowardsTarget(this);
        return;
    }
    if (this->stunTimer >= 30) {
        return;
    }
    f32 shake = ((this->stunTimer & 1) != 0) ? 1.5f : -1.5f;
    this->actor.world.pos.x += shake;
    this->actor.world.pos.z += shake;
}

static void EnDodojr_JumpAttackBounce(EnDodojr* this, PlayState* play) {
    this->actor.flags |= ACTOR_FLAG_SFX_FOR_PLAYER_BODY_HIT;
    Actor_UpdateVelocityWithGravity(&this->actor);
    if (EnDodojr_UpdateBounces(this, play)) {
        EnDodojr_SetupCrawlTowardsTarget(this);
    }
}

static void EnDodojr_Despawn(EnDodojr* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.x, 0x4000, 4, 1000, 100);
    if (DECR(this->actionTimer) == 0) {
        Actor_Kill(&this->actor);
        return;
    }
    this->actor.world.pos.y = this->actor.home.pos.y - (60.0f * ((30 - this->actionTimer) / 30.0f));
    EnDodojr_SpawnLargeDust(this, play, 3);
}

static void EnDodojr_StandardDeathBounce(EnDodojr* this, PlayState* play) {
    Actor_UpdateVelocityWithGravity(&this->actor);
    Math_SmoothStepToS(&this->actor.shape.rot.y, 0, 4, 1000, 10);
    this->actor.world.rot.x = this->actor.shape.rot.x;
    if (EnDodojr_UpdateBounces(this, play)) {
        EnDodojr_StartDeathSequence(this);
    }
}

static void EnDodojr_DeathSequence(EnDodojr* this, PlayState* play) {
    if (this->bounceCount != 0) {
        if (this->actor.colorFilterTimer == 0) {
            Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 200, COLORFILTER_BUFFLAG_OPA,
                                 this->bounceCount);
            this->bounceCount--;
        }
        return;
    }
    EnBom* bomb =
        (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, this->actor.world.pos.x, this->actor.world.pos.y,
                            this->actor.world.pos.z, BOMB_EXPLOSIVE_TYPE_BOMB, 0, 0, BOMB_TYPE_BODY);
    if (bomb != NULL) {
        bomb->timer = 0;
    }
    this->actionTimer = 8;
    this->actionFunc = EnDodojr_DropItem;
}

static void EnDodojr_DropItem(EnDodojr* this, PlayState* play) {
    if (DECR(this->actionTimer) == 0) {
        Item_DropCollectibleRandom(play, NULL, &this->actor.world.pos, 0x40);
        Actor_Kill(&this->actor);
    }
}

static void EnDodojr_WaitFreezeFrames(EnDodojr* this, PlayState* play) {
    if (DECR(this->freezeFrames) == 0) {
        EnDodojr_SetupStandardDeathBounce(this);
    }
}

static void EnDodojr_Update(Actor* thisx, PlayState* play) {
    EnDodojr* this = (EnDodojr*)thisx;

    EnDodojr_ForgetFreedBomb(this, play);
    SkelAnime_Update(&this->skelAnime);
    Actor_MoveWithGravity(&this->actor);
    EnDodojr_CheckDamaged(this, play);
    if (this->actionFunc != EnDodojr_WaitUnderground) {
        Actor_UpdateBgCheckInfo(play, &this->actor, this->collider.dim.radius, this->collider.dim.height, 0.0f,
                                DODOJR_BGCHECK_FLAGS);
    }
    this->actionFunc(this, play);
    Actor_SetFocus(&this->actor, 10.0f);
    EnDodojr_UpdateCollider(this, play);
}

static void EnDodojr_Init(Actor* thisx, PlayState* play) {
    EnDodojr* this = (EnDodojr*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, NULL, 18.0f);
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sSkel, ANIM(sIdleAnim), this->jointTable, this->morphTable,
                   DODOJR_LIMB_MAX);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, &sDamageTable, &sColChkInit);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Actor_SetScale(&this->actor, 0.02f);
    this->actionFunc = EnDodojr_WaitUnderground;
}

static void EnDodojr_Destroy(Actor* thisx, PlayState* play) {
    EnDodojr* this = (EnDodojr*)thisx;

    EnDodojr_ForgetFreedBomb(this, play);
    if ((this->bomb != NULL) && (this->bomb->parent == &this->actor)) {
        this->bomb->parent = NULL;
    }
    Collider_DestroyCylinder(play, &this->collider);
}

static s32 EnDodojr_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                     Actor* thisx) {
    static Vec3f sMouthOffset = { 480.0f, 620.0f, 0.0f };
    EnDodojr* this = (EnDodojr*)thisx;

    if (limbIndex == DODOJR_LIMB_ROOT) {
        Matrix_Scale((this->rootScale * 0.5f) + 1.0f, this->rootScale + 1.0f, (this->rootScale * 0.5f) + 1.0f,
                     MTXMODE_APPLY);
    }
    if (limbIndex == DODOJR_LIMB_HEAD) {
        Matrix_MultVec3f(&sMouthOffset, &this->headPos);
    }
    return false;
}

static void EnDodojr_Draw(Actor* thisx, PlayState* play) {
    EnDodojr* this = (EnDodojr*)thisx;

    if ((this->actionFunc == EnDodojr_WaitUnderground) || (this->actionFunc == EnDodojr_DropItem)) {
        return;
    }
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, EnDodojr_OverrideLimbDraw, NULL,
                      &this->actor);
}

static Gfx sSoulEmptyDL[] = {
    gsSPEndDisplayList(),
};

// The soul shows its enemy in an idle loop, as Majora's Mask souls do.
static void DrawSoulModel(PlayState* play) {
    static SkelAnime sSoulSkelAnime;
    static Vec3s sSoulJointTable[DODOJR_LIMB_MAX];
    static Vec3s sSoulMorphTable[DODOJR_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    if (!sIsSoulPosed) {
        SkelAnime_Init(play, &sSoulSkelAnime, (SkeletonHeader*)sSkel, ANIM(sCrawlAnim), sSoulJointTable,
                       sSoulMorphTable, DODOJR_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(0.03f, 0.03f, 0.03f, MTXMODE_APPLY);
    // Side on: from the front the long body hides behind its head.
    Matrix_RotateYS(0x4000, MTXMODE_APPLY);
    // Animations place the root far from the origin; the soul floats on its root instead.
    Matrix_Translate(-sSoulJointTable[0].x, -sSoulJointTable[0].y, -sSoulJointTable[0].z, MTXMODE_APPLY);
    // Eyes and effects read segments the actor fills; with no actor they point at harmless lists.
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0B, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sSoulEmptyDL);
    SkelAnime_DrawOpa(play, sSoulSkelAnime.skeleton, sSoulSkelAnime.jointTable, NULL, NULL, NULL);
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
    gDPSetPrimColor(POLY_XLU_DISP++, 0xC0, 0xC0, 230, 130, 60, 0);
    gDPSetEnvColor(POLY_XLU_DISP++, 230, 130, 60, 0);
    Matrix_Scale(0.13333333f, 0.13333333f, 0.13333333f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gameplay_keep_DL_01ACF0);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Ship (RE_DODONGO): a sword, the hammer (Goron here), sticks, explosives, the slingshot or the bow.
static bool CanKillBabyDodongo(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicHasCapability("CanBeGoron") ||
           sApi->LogicCanUseItem(RI_DEKU_STICK) || sApi->LogicHasCapability("HasExplosives") ||
           sApi->LogicCanUseItem(RI_BOW);
}

// The Dodongo family soul, as in Ship: a Dodongo mod defines it alike, so whichever loads first registers it.
static void RegisterSoul(void) {
    if (S2H_MOD_API_HAS(sApi, FindCustomItem) && sApi->FindCustomItem(DODONGO_SOUL_ITEM_KEY) != NULL) {
        return;
    }
    static S2HCustomItemRandomizer sSoulLogic = { 0 };
    S2HCustomItemDefinition soul = { 0 };

    sSoulLogic.structSize = sizeof(sSoulLogic);
    sSoulLogic.flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
    sSoulLogic.type = S2H_CUSTOM_ITEM_TYPE_ITEM;
    sSoulLogic.logicKey = DODONGO_SOUL_ITEM_KEY;
    sSoulLogic.hintKey = DODONGO_SOUL_ITEM_KEY;
    soul.structSize = sizeof(soul);
    soul.key = DODONGO_SOUL_ITEM_KEY;
    soul.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO;
    soul.getItemText = "You got the %rSoul of Dodongos%w!&Dodongos can be defeated now.";
    soul.getItemEntry.drawFunc = DrawSoul;
    soul.randomizer = &sSoulLogic;
    if (!sApi->RegisterCustomItem(&soul)) {
        sApi->Log("Could not register " DODONGO_SOUL_ITEM_KEY);
    }
}

static s16 RegisterBabyDodongo(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = BABY_DODONGO_KEY;
    definition.description = "Baby Dodongo";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = DODOJR_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnDodojr);
    definition.init = EnDodojr_Init;
    definition.destroy = EnDodojr_Destroy;
    definition.update = EnDodojr_Update;
    definition.draw = EnDodojr_Draw;
    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    definition.naviHint = "Baby Dodongo&It burrows up and lunges at you, and it&explodes when it dies. Keep away!";
    definition.soulKey = DODONGO_FAMILY_KEY;
    definition.logicKill = CanKillBabyDodongo;
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
    if (!RequireOotAsset(DODOJR_OBJECT "object_dodojr_Skel_0020E0")) {
        sApi->Log(BABY_DODONGO_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    if (RegisterBabyDodongo() < 0) {
        sApi->Log("Could not register " BABY_DODONGO_KEY);
        return;
    }
    RegisterSoul();
}
