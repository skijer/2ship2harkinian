/*
 * Ocarina of Time's lone Lizalfos (En_Zf) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * It waits unseen above the floor and drops in when the player comes near; ice arrows freeze it with MM's ice.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define LIZALFOS_KEY "oot.lizalfos"
#define ZF_OBJECT "objects/oot_object_zf/"
#define ZF_ASSET(name) "__OTR__" ZF_OBJECT name

static const ALIGN_ASSET(2) char sSkel[] = ZF_ASSET("gZfLizalfosSkel");
static const ALIGN_ASSET(2) char sCryingAnim[] = ZF_ASSET("gZfCryingAnim");
static const ALIGN_ASSET(2) char sWalkingAnim[] = ZF_ASSET("gZfWalkingAnim");
static const ALIGN_ASSET(2) char sJumpingAnim[] = ZF_ASSET("gZfJumpingAnim");
static const ALIGN_ASSET(2) char sLandingAnim[] = ZF_ASSET("gZfLandingAnim");
static const ALIGN_ASSET(2) char sSidesteppingAnim[] = ZF_ASSET("gZfSidesteppingAnim");
static const ALIGN_ASSET(2) char sSlashAnim[] = ZF_ASSET("gZfSlashAnim");
static const ALIGN_ASSET(2) char sKnockedBackAnim[] = ZF_ASSET("gZfKnockedBackAnim");
static const ALIGN_ASSET(2) char sDyingAnim[] = ZF_ASSET("gZfDyingAnim");

// MM keeps OoT's sfx ids for these under other names, as its Dinolfos shows.
#define NA_SE_EN_LIZALFOS_LAND NA_SE_EN_BOMCHU_WALK
#define NA_SE_EN_LIZALFOS_DOWN NA_SE_EN_GERUDOFT_DOWN

#define ANIM(asset) ((AnimationHeader*)(asset))
#define LAST_FRAME(asset) Animation_GetLastFrame((void*)(asset))

#define ZF_LIMB_HEAD_ROOT 0x05
#define ZF_LIMB_HEAD 0x09
#define ZF_LIMB_NECK 0x0A
#define ZF_LIMB_SWORD_ROOT 0x0E
#define ZF_LIMB_SWORD 0x0F
#define ZF_LIMB_RIGHT_FOREARM 0x10
#define ZF_LIMB_LEFT_FOREARM 0x17
#define ZF_LIMB_CHEST_ARMOR 0x19
#define ZF_LIMB_RIGHT_FOOT 0x26
#define ZF_LIMB_RIGHT_SHIN 0x27
#define ZF_LIMB_LEFT_SHIN_ROOT 0x2B
#define ZF_LIMB_LEFT_FOOT 0x2D
#define ZF_LIMB_TRUNK 0x30
#define ZF_LIMB_MAX 0x31

// Vanilla's numbered actions; the update compares ranges of them.
typedef enum {
    ZF_ACTION_DROP_IN = 0,
    ZF_ACTION_IDLE = 3,
    ZF_ACTION_APPROACH = 5,
    ZF_ACTION_TURN = 6,
    ZF_ACTION_SIDESTEP = 7,
    ZF_ACTION_RECOIL = 8,
    ZF_ACTION_SLASH = 9,
    ZF_ACTION_CIRCLE = 12,
    ZF_ACTION_JUMP_FORWARD = 13,
    ZF_ACTION_STUNNED = 14,
    ZF_ACTION_DIE = 15,
    ZF_ACTION_DAMAGED = 16,
    ZF_ACTION_JUMP_UP = 22,
} LizalfosAction;

typedef enum {
    ZF_DMGEFF_NONE = 0x0,
    ZF_DMGEFF_STUN = 0x1,
    ZF_DMGEFF_ICE = 0xF,
} LizalfosDamageEffect;

#define LIZALFOS_HEALTH 6
#define LIZALFOS_DROP_HEIGHT 300.0f
#define LIZALFOS_STUN_FRAMES 80
#define LIZALFOS_THAW_FRAMES 36
#define LIZALFOS_BODYPART_COUNT 9
#define LIZALFOS_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED)
#define LIZALFOS_PROJECTILE_FLAGS \
    (DMG_HOOKSHOT | DMG_NORMAL_ARROW | DMG_FIRE_ARROW | DMG_ICE_ARROW | DMG_LIGHT_ARROW | DMG_DEKU_BUBBLE)
#define LIZALFOS_BGCHECK_FLAGS \
    (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_8 | UPDBGCHECKINFO_FLAG_10)
#define LIZALFOS_PROBE_FLAGS (UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_8 | UPDBGCHECKINFO_FLAG_10)

static const S2HModApi* sApi;

struct EnZf;
typedef void (*EnZfActionFunc)(struct EnZf*, PlayState*);

typedef struct EnZf {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[ZF_LIMB_MAX];
    Vec3s morphTable[ZF_LIMB_MAX];
    EnZfActionFunc actionFunc;
    ColliderCylinder bodyCollider;
    ColliderQuad swordCollider;
    Vec3f rightFootPos;
    Vec3f leftFootPos;
    Vec3f bodyPartsPos[LIZALFOS_BODYPART_COUNT];
    f32 approachSpeed;
    f32 drawDmgEffAlpha;
    s32 action;
    s32 timer;
    s32 blureIndex;
    s16 headRot;
    u8 alpha;
    u8 isAirborne;
    u8 isNearLedge;
    u8 damageEffect;
} EnZf;

// IREG(0) and IREG(1) are never set, so vanilla loads a zero texture scale here every frame.
static Gfx sTextureScaleDL[] = {
    gsSPTexture(0, 0, 0, G_TX_RENDERTILE, G_ON),
    gsSPEndDisplayList(),
};
static Gfx sXluRenderModeDL[] = {
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU |
                                            FORCE_BL | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)),
    gsDPSetAlphaCompare(G_AC_THRESHOLD),
    gsSPEndDisplayList(),
};
static Gfx sEmptyDL[] = {
    gsSPEndDisplayList(),
};

static ColliderCylinderInit sBodyColliderInit = {
    {
        COL_MATERIAL_HIT0,
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
        ACELEM_ON,
        OCELEM_ON,
    },
    { 20, 70, 0, { 0, 0, 0 } },
};

static ColliderQuadInit sSwordColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_HARD | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_QUAD,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x00, 0x08 },
        { 0x00000000, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL | ATELEM_UNK7,
        ACELEM_ON,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static DamageTable sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, ZF_DMGEFF_STUN),
    /* Deku Stick     */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* Horse trample  */ DMG_ENTRY(0, ZF_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* Zora boomerang */ DMG_ENTRY(0, ZF_DMGEFF_STUN),
    /* Normal arrow   */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, ZF_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(0, ZF_DMGEFF_STUN),
    /* Goron punch    */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* Sword          */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* Goron pound    */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* Fire arrow     */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* Ice arrow      */ DMG_ENTRY(2, ZF_DMGEFF_ICE),
    /* Light arrow    */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* Goron spikes   */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* Deku spin      */ DMG_ENTRY(0, ZF_DMGEFF_STUN),
    /* Deku bubble    */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* Deku launch    */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, ZF_DMGEFF_NONE),
    /* Zora barrier   */ DMG_ENTRY(0, ZF_DMGEFF_STUN),
    /* Normal shield  */ DMG_ENTRY(0, ZF_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, ZF_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* Zora punch     */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* Spin attack    */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* Sword beam     */ DMG_ENTRY(1, ZF_DMGEFF_NONE),
    /* Normal Roll    */ DMG_ENTRY(0, ZF_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, ZF_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, ZF_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, ZF_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, ZF_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(2, ZF_DMGEFF_NONE),
};

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(lockOnArrowOffset, 2000, ICHAIN_CONTINUE),
    ICHAIN_VEC3F_DIV1000(scale, 15, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(gravity, -3500, ICHAIN_STOP),
};

static void EnZf_SetupIdle(EnZf* this);
static void EnZf_SetupApproach(EnZf* this);
static void EnZf_SetupTurn(EnZf* this);
static void EnZf_SetupSidestep(EnZf* this);
static void EnZf_SetupCircle(EnZf* this, PlayState* play);
static void EnZf_SetupSlash(EnZf* this);
static void EnZf_SetupJumpForward(EnZf* this);
static void EnZf_SetupJumpUp(EnZf* this);
static void EnZf_SetupDie(EnZf* this);
static bool EnZf_DodgeProjectile(EnZf* this, PlayState* play);

static s16 AbsYawDiff(s16 a, s16 b) {
    s16 diff = a - b;

    return ABS(diff);
}

static bool IsOnGround(EnZf* this) {
    return (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static bool HasLandedOrTouched(EnZf* this) {
    return (this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH)) != 0;
}

static bool IsRestingOnGround(EnZf* this) {
    return IsOnGround(this) && ((this->actor.velocity.y == 0.0f) || (this->actor.velocity.y == -4.0f));
}

static bool EnZf_IsLedgeInDirection(EnZf* this, PlayState* play, s16 yaw, f32 dist) {
    Vec3f pos = this->actor.world.pos;
    u16 bgCheckFlags = this->actor.bgCheckFlags;

    this->actor.world.pos.x += Math_SinS(yaw) * dist;
    this->actor.world.pos.z += Math_CosS(yaw) * dist;
    Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 0.0f, 0.0f, LIZALFOS_PROBE_FLAGS);
    this->actor.world.pos = pos;
    bool isLedge = !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND);
    this->actor.bgCheckFlags = bgCheckFlags;
    return isLedge;
}

static bool EnZf_IsLedgeAhead(EnZf* this, PlayState* play, f32 dist) {
    if (dist == 0.0f) {
        dist = (this->actor.speed >= 0.0f) ? 30.0f : -30.0f;
    }
    return EnZf_IsLedgeInDirection(this, play, this->actor.world.rot.y, dist);
}

static bool EnZf_IsLedgeAlongMoveOrFacing(EnZf* this, PlayState* play, f32 dist) {
    if ((this->actor.speed != 0.0f) && EnZf_IsLedgeAhead(this, play, this->actor.speed)) {
        return true;
    }
    return EnZf_IsLedgeInDirection(this, play, this->actor.shape.rot.y, dist);
}

static bool EnZf_CanAttack(EnZf* this, PlayState* play) {
    return !Actor_OtherIsTargeted(play, &this->actor);
}

static void EnZf_PlayCry(EnZf* this, PlayState* play) {
    if ((play->gameplayFrames & 0x5F) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_CRY);
    }
}

static void EnZf_PlayStepSfx(EnZf* this, s32 prevFrame, s32 firstStep, s32 secondStep) {
    s32 afterStep = this->skelAnime.curFrame - ABS(this->skelAnime.playSpeed);
    s32 reach = (s32)ABS(this->skelAnime.playSpeed) + prevFrame;

    if (prevFrame == (s32)this->skelAnime.curFrame) {
        return;
    }
    if (((afterStep < firstStep) && (reach >= firstStep + 2)) ||
        ((afterStep < secondStep) && (reach >= secondStep + 2))) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_WALK);
    }
}

static void EnZf_FacePlayer(EnZf* this) {
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
}

static void EnZf_Freeze(EnZf* this) {
    this->drawDmgEffAlpha = 1.0f;
}

static void EnZf_Thaw(EnZf* this, PlayState* play) {
    if (this->drawDmgEffAlpha == 0.0f) {
        return;
    }
    this->drawDmgEffAlpha = 0.0f;
    Actor_SpawnIceEffects(play, &this->actor, this->bodyPartsPos, LIZALFOS_BODYPART_COUNT, 2, 0.3f, 0.2f);
}

static void EnZf_ChooseAfterHit(EnZf* this, PlayState* play) {
    if (AbsYawDiff(this->actor.yawTowardsPlayer, this->actor.shape.rot.y) >= 0x1B58) {
        EnZf_SetupCircle(this, play);
    } else if ((this->actor.xzDistToPlayer <= 100.0f) && ((play->gameplayFrames % 8) != 0) &&
               EnZf_CanAttack(this, play)) {
        EnZf_SetupSlash(this);
    } else {
        EnZf_SetupIdle(this);
    }
}

static void EnZf_DropIn(EnZf* this, PlayState* play) {
    if (this->timer != 0) {
        if (this->actor.xzDistToPlayer <= 160.0f) {
            this->timer = 0;
            this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
            Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_CRY);
        }
        this->actor.world.pos.y = this->actor.floorHeight + LIZALFOS_DROP_HEIGHT;
    } else if (this->alpha < 255) {
        this->alpha += 255 / 5;
    }
    if (HasLandedOrTouched(this) && this->isAirborne) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_LIZALFOS_LAND);
        Animation_Change(&this->skelAnime, ANIM(sLandingAnim), 1.0f, 0.0f, 17.0f, ANIMMODE_ONCE, 0.0f);
        this->isAirborne = false;
        this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND_TOUCH;
        this->actor.world.pos.y = this->actor.floorHeight;
        this->actor.velocity.y = 0.0f;
        Actor_SpawnFloorDustRing(play, &this->actor, &this->leftFootPos, 3.0f, 2, 2.0f, 0, 0, false);
        Actor_SpawnFloorDustRing(play, &this->actor, &this->rightFootPos, 3.0f, 2, 2.0f, 0, 0, false);
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        this->alpha = 255;
        EnZf_SetupIdle(this);
    }
    this->actor.shape.shadowAlpha = this->alpha;
}

static void EnZf_SetupDropIn(EnZf* this) {
    Animation_Change(&this->skelAnime, ANIM(sJumpingAnim), 0.0f, 9.0f, LAST_FRAME(sJumpingAnim), ANIMMODE_LOOP, 0.0f);
    this->actor.world.pos.y = this->actor.floorHeight + LIZALFOS_DROP_HEIGHT;
    this->alpha = 0;
    this->actor.shape.shadowAlpha = 0;
    this->timer = 10;
    this->isAirborne = true;
    this->action = ZF_ACTION_DROP_IN;
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND_TOUCH;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    EnZf_FacePlayer(this);
    this->actionFunc = EnZf_DropIn;
}

static bool EnZf_IsPlayerSwingingAtFront(EnZf* this, PlayState* play, f32 range) {
    Player* player = GET_PLAYER(play);

    return (this->actor.xzDistToPlayer < range) && (player->meleeWeaponState != 0) &&
           (AbsYawDiff(player->actor.shape.rot.y, this->actor.shape.rot.y) >= 0x1F40);
}

static void EnZf_Idle(EnZf* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (EnZf_DodgeProjectile(this, play)) {
        return;
    }
    if (EnZf_IsPlayerSwingingAtFront(this, play, 100.0f)) {
        EnZf_FacePlayer(this);
        EnZf_SetupCircle(this, play);
        return;
    }
    if (this->timer != 0) {
        this->timer--;
        return;
    }
    if (!Actor_IsFacingPlayer(&this->actor, 0x1555)) {
        EnZf_SetupTurn(this);
    } else if ((this->actor.xzDistToPlayer < 200.0f) && (this->actor.xzDistToPlayer > 100.0f) &&
               (Rand_ZeroOne() < 0.3f)) {
        EnZf_SetupCircle(this, play);
    } else if (Rand_ZeroOne() > 0.3f) {
        EnZf_SetupApproach(this);
    } else {
        EnZf_SetupCircle(this, play);
    }
    EnZf_PlayCry(this, play);
}

static void EnZf_SetupIdle(EnZf* this) {
    Animation_Change(&this->skelAnime, ANIM(sCryingAnim), 1.0f, 0.0f, LAST_FRAME(sCryingAnim), ANIMMODE_LOOP_INTERP,
                     -4.0f);
    this->action = ZF_ACTION_IDLE;
    this->timer = (Rand_ZeroOne() * 10.0f) + 5.0f;
    this->actor.speed = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = EnZf_Idle;
}

static void EnZf_Approach(EnZf* this, PlayState* play) {
    if (EnZf_DodgeProjectile(this, play)) {
        return;
    }
    f32 keepAway = Actor_OtherIsTargeted(play, &this->actor) ? 100.0f : 0.0f;
    if (this->actor.xzDistToPlayer <= (70.0f + keepAway)) {
        Math_SmoothStepToF(&this->actor.speed, -8.0f, 1.0f, 0.5f, 0.0f);
    } else {
        Math_SmoothStepToF(&this->actor.speed, 8.0f, 1.0f, 0.5f, 0.0f);
    }
    this->skelAnime.playSpeed = this->actor.speed * 1.2f;
    if (EnZf_IsPlayerSwingingAtFront(this, play, 150.0f)) {
        EnZf_FacePlayer(this);
        if (Rand_ZeroOne() > 0.7f) {
            EnZf_SetupCircle(this, play);
            return;
        }
    }
    s32 prevFrame = this->skelAnime.curFrame;
    SkelAnime_Update(&this->skelAnime);
    EnZf_PlayStepSfx(this, prevFrame, 2, 32);
    EnZf_PlayCry(this, play);
    if (!Actor_IsFacingPlayer(&this->actor, 0x11C7)) {
        if (Rand_ZeroOne() > 0.5f) {
            EnZf_SetupSidestep(this);
        } else {
            EnZf_SetupIdle(this);
        }
    } else if (this->actor.xzDistToPlayer < 100.0f) {
        if ((Rand_ZeroOne() > 0.05f) && EnZf_CanAttack(this, play)) {
            EnZf_SetupSlash(this);
        } else if (Rand_ZeroOne() > 0.5f) {
            EnZf_SetupCircle(this, play);
        } else {
            EnZf_SetupIdle(this);
        }
    } else if (this->isNearLedge) {
        EnZf_SetupSidestep(this);
    } else if (Rand_ZeroOne() < 0.1f) {
        EnZf_SetupIdle(this);
    }
}

static void EnZf_SetupApproach(EnZf* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sWalkingAnim), -4.0f);
    this->action = ZF_ACTION_APPROACH;
    this->actor.speed = 0.0f;
    this->actionFunc = EnZf_Approach;
}

static void EnZf_LandFromJump(EnZf* this) {
    if ((this->timer == 0) || (this->actor.world.pos.y > this->actor.floorHeight)) {
        return;
    }
    this->actor.world.pos.y = this->actor.floorHeight;
    this->isAirborne = false;
    this->actor.velocity.y = 0.0f;
    this->actor.speed = 0.0f;
}

static void EnZf_JumpForward(EnZf* this, PlayState* play) {
    EnZf_LandFromJump(this);
    if (SkelAnime_Update(&this->skelAnime)) {
        if (this->timer == 0) {
            Animation_Change(&this->skelAnime, ANIM(sLandingAnim), 3.0f, 0.0f, 17.0f, ANIMMODE_ONCE, -3.0f);
            this->timer = 10;
            Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_JUMP);
        } else {
            this->actor.speed = 0.0f;
            this->isAirborne = false;
            EnZf_SetupApproach(this);
        }
    }
    EnZf_PlayCry(this, play);
}

static void EnZf_SetupJumpForward(EnZf* this) {
    Animation_Change(&this->skelAnime, ANIM(sJumpingAnim), 1.0f, 0.0f, 3.0f, ANIMMODE_ONCE, -3.0f);
    this->timer = 0;
    this->isAirborne = true;
    this->actor.velocity.y = 15.0f;
    this->actor.speed = 10.0f;
    this->action = ZF_ACTION_JUMP_FORWARD;
    Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_JUMP);
    this->actionFunc = EnZf_JumpForward;
}

static void EnZf_Turn(EnZf* this, PlayState* play) {
    if (EnZf_DodgeProjectile(this, play)) {
        return;
    }
    s16 yawDiff = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
    s16 turn = (yawDiff > 0) ? ((yawDiff * 0.25f) + 2000.0f) : ((yawDiff * 0.25f) - 2000.0f);
    this->actor.shape.rot.y += turn;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    f32 playSpeed = turn * 1.5f;
    this->skelAnime.playSpeed = (yawDiff > 0) ? -CLAMP_MAX(playSpeed, 2.0f) : -CLAMP_MIN(playSpeed, -2.0f);
    SkelAnime_Update(&this->skelAnime);
    if (Actor_IsFacingPlayer(&this->actor, 0x1555)) {
        if (Rand_ZeroOne() > 0.8f) {
            EnZf_SetupSidestep(this);
        } else {
            EnZf_SetupApproach(this);
        }
    }
    EnZf_PlayCry(this, play);
}

static void EnZf_SetupTurn(EnZf* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sWalkingAnim), -4.0f);
    this->action = ZF_ACTION_TURN;
    this->actionFunc = EnZf_Turn;
}

static void EnZf_BounceOffEdges(EnZf* this, PlayState* play) {
    bool isTouchingWall = (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) != 0;

    if (!isTouchingWall &&
        Actor_TestFloorInDirection(&this->actor, play, this->actor.speed, this->actor.shape.rot.y + 0x3FFF)) {
        return;
    }
    s16 wallDiff = 0;
    if (isTouchingWall) {
        s16 moveYaw = (this->actor.speed >= 0.0f) ? (s16)(this->actor.shape.rot.y + 0x3FFF)
                                                  : (s16)(this->actor.shape.rot.y - 0x3FFF);
        wallDiff = this->actor.wallYaw - moveYaw;
    } else {
        this->actor.speed *= -0.8f;
    }
    if (ABS(wallDiff) <= 0x4000) {
        return;
    }
    this->actor.speed *= -0.8f;
    this->actor.speed += (this->actor.speed < 0.0f) ? -0.5f : 0.5f;
}

static void EnZf_KeepDistance(EnZf* this, PlayState* play) {
    f32 keepAway = Actor_OtherIsTargeted(play, &this->actor) ? 100.0f : 0.0f;

    if (this->actor.xzDistToPlayer <= (70.0f + keepAway)) {
        Math_SmoothStepToF(&this->approachSpeed, -4.0f, 1.0f, 1.5f, 0.0f);
    } else if (this->actor.xzDistToPlayer > (90.0f + keepAway)) {
        Math_SmoothStepToF(&this->approachSpeed, 4.0f, 1.0f, 1.5f, 0.0f);
    } else {
        Math_SmoothStepToF(&this->approachSpeed, 0.0f, 1.0f, 5.65f, 0.0f);
    }
    if ((this->approachSpeed != 0.0f) && !EnZf_IsLedgeAlongMoveOrFacing(this, play, this->approachSpeed)) {
        this->actor.world.pos.x += Math_SinS(this->actor.shape.rot.y) * this->approachSpeed;
        this->actor.world.pos.z += Math_CosS(this->actor.shape.rot.y) * this->approachSpeed;
    }
}

static f32 EnZf_GetStrafePlaySpeed(EnZf* this, f32 sideSpeed) {
    if (ABS(this->actor.speed) >= ABS(this->approachSpeed)) {
        return sideSpeed * 0.75f;
    }
    return (this->skelAnime.playSpeed < 0.0f) ? (this->approachSpeed * -0.75f) : (this->approachSpeed * 0.75f);
}

static void EnZf_Sidestep(EnZf* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 4000, 1);
    if (EnZf_DodgeProjectile(this, play)) {
        return;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x3A98;
    s16 behindPlayer = player->actor.shape.rot.y + 0x8000;
    if (Math_SinS(behindPlayer - this->actor.shape.rot.y) >= 0.0f) {
        this->actor.speed = MAX(this->actor.speed - 0.25f, -8.0f);
    } else {
        this->actor.speed = MIN(this->actor.speed + 0.25f, 8.0f);
    }
    EnZf_BounceOffEdges(this, play);
    EnZf_KeepDistance(this, play);
    this->skelAnime.playSpeed = EnZf_GetStrafePlaySpeed(this, this->actor.speed);
    s32 prevFrame = this->skelAnime.curFrame;
    SkelAnime_Update(&this->skelAnime);
    EnZf_PlayStepSfx(this, prevFrame, 14, 27);
    EnZf_PlayCry(this, play);
    if ((Math_CosS(behindPlayer - this->actor.shape.rot.y) >= -0.85f) && (this->timer != 0)) {
        this->timer--;
        return;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if ((this->actor.xzDistToPlayer <= 100.0f) && ((play->gameplayFrames % 4) == 0) && EnZf_CanAttack(this, play)) {
        EnZf_SetupSlash(this);
    } else {
        EnZf_SetupIdle(this);
    }
}

static void EnZf_SetupSidestep(EnZf* this) {
    Animation_PlayLoop(&this->skelAnime, ANIM(sSidesteppingAnim));
    this->actor.speed = Rand_CenteredFloat(12.0f);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->timer = (Rand_ZeroOne() * 10.0f) + 20.0f;
    this->isAirborne = false;
    this->action = ZF_ACTION_SIDESTEP;
    this->approachSpeed = 0.0f;
    this->actionFunc = EnZf_Sidestep;
}

static bool IsPlayerGuardedOrHanging(Player* player) {
    return (player->stateFlags3 & PLAYER_STATE3_HOSTILE_LOCK_ON) ||
           (player->stateFlags1 & (PLAYER_STATE1_2000 | PLAYER_STATE1_4000));
}

static void EnZf_Slash(EnZf* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->actor.speed = 0.0f;
    if ((s32)this->skelAnime.curFrame == 10) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_ATTACK);
    }
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    EffectBlure_AddSpace(Effect_GetByIndex(this->blureIndex));
    if ((Rand_ZeroOne() > 0.7f) || (this->actor.xzDistToPlayer >= 120.0f)) {
        EnZf_SetupIdle(this);
        this->timer = (Rand_ZeroOne() * 5.0f) + 5.0f;
        return;
    }
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    if ((Rand_ZeroOne() > 0.7f) || (AbsYawDiff(player->actor.shape.rot.y, this->actor.shape.rot.y) > 10000)) {
        EnZf_SetupCircle(this, play);
    } else if (AbsYawDiff(this->actor.yawTowardsPlayer, this->actor.shape.rot.y) > 16000) {
        this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        EnZf_SetupCircle(this, play);
    } else if (IsPlayerGuardedOrHanging(player) && !this->actor.isLockedOn) {
        EnZf_SetupCircle(this, play);
    } else {
        EnZf_SetupSlash(this);
    }
}

static void EnZf_SetupSlash(EnZf* this) {
    Animation_Change(&this->skelAnime, ANIM(sSlashAnim), 1.25f, 0.0f, LAST_FRAME(sSlashAnim), ANIMMODE_ONCE, -4.0f);
    this->swordCollider.base.atFlags &= ~AT_BOUNCED;
    this->action = ZF_ACTION_SLASH;
    Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_CRY);
    this->actor.speed = 0.0f;
    this->actionFunc = EnZf_Slash;
}

static void EnZf_Recoil(EnZf* this, PlayState* play) {
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (Rand_ZeroOne() > 0.7f) {
        EnZf_SetupIdle(this);
    } else if ((Rand_ZeroOne() > 0.2f) && EnZf_CanAttack(this, play)) {
        EnZf_SetupSlash(this);
    } else {
        EnZf_SetupCircle(this, play);
    }
}

static void EnZf_SetupRecoil(EnZf* this) {
    Animation_Change(&this->skelAnime, ANIM(sSlashAnim), -1.0f, this->skelAnime.curFrame - 3.0f, 0.0f, ANIMMODE_ONCE,
                     0.0f);
    this->action = ZF_ACTION_RECOIL;
    this->actionFunc = EnZf_Recoil;
}

static void EnZf_SlowOnGround(EnZf* this) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
        this->actor.speed = 0.0f;
    }
    if (!IsOnGround(this)) {
        return;
    }
    if (this->actor.speed < 0.0f) {
        this->actor.speed += 0.05f;
    }
    this->isAirborne = false;
}

static void EnZf_Stunned(EnZf* this, PlayState* play) {
    EnZf_SlowOnGround(this);
    if ((this->actor.colorFilterTimer != 0) || !IsOnGround(this)) {
        return;
    }
    EnZf_Thaw(this, play);
    if (this->actor.colChkInfo.health == 0) {
        EnZf_SetupDie(this);
    } else if (!EnZf_DodgeProjectile(this, play)) {
        EnZf_ChooseAfterHit(this, play);
    }
}

static void EnZf_SetupStunned(EnZf* this) {
    if (IsRestingOnGround(this)) {
        this->actor.speed = 0.0f;
        this->isAirborne = false;
    } else {
        this->isAirborne = true;
    }
    if (this->damageEffect == ZF_DMGEFF_ICE) {
        EnZf_Freeze(this);
    } else {
        Animation_PlayOnceSetSpeed(&this->skelAnime, ANIM(sKnockedBackAnim), 0.0f);
    }
    Actor_PlaySfx(&this->actor, NA_SE_EN_COMMON_FREEZE);
    this->action = ZF_ACTION_STUNNED;
    this->actionFunc = EnZf_Stunned;
}

static void EnZf_Damaged(EnZf* this, PlayState* play) {
    EnZf_SlowOnGround(this);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 4500, 0);
    if (!SkelAnime_Update(&this->skelAnime) || !IsOnGround(this) || EnZf_DodgeProjectile(this, play)) {
        return;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (!EnZf_IsLedgeAhead(this, play, 135.0f) && (this->actor.xzDistToPlayer < 90.0f)) {
        EnZf_SetupJumpUp(this);
    } else if ((this->actor.xzDistToPlayer <= 100.0f) && ((play->gameplayFrames % 4) == 0)) {
        EnZf_SetupSlash(this);
    } else {
        EnZf_ChooseAfterHit(this, play);
    }
}

static void EnZf_SetupDamaged(EnZf* this) {
    Animation_Change(&this->skelAnime, ANIM(sKnockedBackAnim), 1.5f, 0.0f, LAST_FRAME(sKnockedBackAnim), ANIMMODE_ONCE,
                     -4.0f);
    if (IsRestingOnGround(this)) {
        this->actor.speed = -4.0f;
        this->isAirborne = false;
    } else {
        this->isAirborne = true;
    }
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_DAMAGE);
    this->action = ZF_ACTION_DAMAGED;
    this->actionFunc = EnZf_Damaged;
}

static void EnZf_SpawnTakeoffSparkles(PlayState* play, Vec3f* pos) {
    static Vec3f sVelocity = { 0.0f, -1.5f, 0.0f };
    static Vec3f sAccel = { 0.0f, -0.2f, 0.0f };
    Color_RGBA8 primColor = { 200, 160, 120, 255 };
    Color_RGBA8 envColor = { 130, 90, 50, 255 };

    EffectSsKirakira_SpawnSmall(play, pos, &sVelocity, &sAccel, &primColor, &envColor);
}

static void EnZf_JumpUp(EnZf* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 4000, 1);
    if (this->actor.velocity.y >= 5.0f) {
        EnZf_SpawnTakeoffSparkles(play, &this->leftFootPos);
        EnZf_SpawnTakeoffSparkles(play, &this->rightFootPos);
    }
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (this->timer == 0) {
        Animation_Change(&this->skelAnime, ANIM(sSlashAnim), 3.0f, 0.0f, 13.0f, ANIMMODE_ONCE, -4.0f);
        this->timer = 10;
        return;
    }
    if (!HasLandedOrTouched(this)) {
        return;
    }
    this->actor.velocity.y = 0.0f;
    EnZf_FacePlayer(this);
    this->actor.speed = 0.0f;
    this->actor.world.pos.y = this->actor.floorHeight;
    EnZf_SetupSlash(this);
    Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_ATTACK);
    this->skelAnime.curFrame = 13.0f;
}

static void EnZf_SetupJumpUp(EnZf* this) {
    Animation_Change(&this->skelAnime, ANIM(sJumpingAnim), 1.0f, 0.0f, 3.0f, ANIMMODE_ONCE, 0.0f);
    this->timer = 0;
    this->isAirborne = true;
    this->action = ZF_ACTION_JUMP_UP;
    this->actor.velocity.y = 22.0f;
    this->actor.speed = 7.5f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_JUMP);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = EnZf_JumpUp;
}

static void EnZf_ChooseAfterCircle(EnZf* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (AbsYawDiff(player->actor.shape.rot.y, this->actor.shape.rot.y) >= 0x3A98) {
        EnZf_SetupIdle(this);
        this->timer = (Rand_ZeroOne() * 5.0f) + 1.0f;
        return;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    bool isLeapRange = (this->actor.xzDistToPlayer < 280.0f) && (this->actor.xzDistToPlayer > 240.0f);
    if ((this->actor.xzDistToPlayer <= 100.0f) && ((play->gameplayFrames % 4) == 0) && EnZf_CanAttack(this, play)) {
        EnZf_SetupSlash(this);
    } else if (isLeapRange && !EnZf_IsLedgeAhead(this, play, 191.9956f) && ((play->gameplayFrames % 2) == 0)) {
        EnZf_SetupJumpForward(this);
    } else {
        EnZf_SetupApproach(this);
    }
}

static void EnZf_Circle(EnZf* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xBB8, 1);
    EnZf_BounceOffEdges(this, play);
    if (Math_SinS(player->actor.shape.rot.y - this->actor.shape.rot.y) >= 0.0f) {
        this->actor.speed += 0.125f;
    } else {
        this->actor.speed -= 0.125f;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x4000;
    EnZf_KeepDistance(this, play);
    this->skelAnime.playSpeed = EnZf_GetStrafePlaySpeed(this, -this->actor.speed);
    s32 prevFrame = this->skelAnime.curFrame;
    SkelAnime_Update(&this->skelAnime);
    EnZf_PlayStepSfx(this, prevFrame, 14, 27);
    EnZf_PlayCry(this, play);
    if (this->timer != 0) {
        this->timer--;
        return;
    }
    EnZf_ChooseAfterCircle(this, play);
}

static void EnZf_StartCircle(EnZf* this, f32 speed) {
    this->actor.speed = speed;
    this->approachSpeed = 0.0f;
    this->isAirborne = false;
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x3FFF;
    this->action = ZF_ACTION_CIRCLE;
    this->actionFunc = EnZf_Circle;
}

static void EnZf_SetupCircle(EnZf* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Animation_PlayLoop(&this->skelAnime, ANIM(sSidesteppingAnim));
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 4000, 1);
    EnZf_StartCircle(this, (Math_SinS(player->actor.shape.rot.y - this->actor.shape.rot.y) >= 0.0f) ? -6.0f : 6.0f);
    this->timer = (Rand_ZeroOne() * 10.0f) + 5.0f;
}

static void EnZf_SetupDodgeCircle(EnZf* this, f32 speed) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sSidesteppingAnim), -1.0f);
    this->timer = (Rand_ZeroOne() * 10.0f) + 8.0f;
    EnZf_StartCircle(this, speed);
}

static void EnZf_Die(EnZf* this, PlayState* play) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
        this->actor.speed = 0.0f;
    }
    if (IsOnGround(this)) {
        Math_SmoothStepToF(&this->actor.speed, 0.0f, 1.0f, 0.15f, 0.0f);
        this->isAirborne = false;
    }
    if (!SkelAnime_Update(&this->skelAnime)) {
        s32 frame = this->skelAnime.curFrame;

        if ((frame == 10) || (frame == 18)) {
            Actor_PlaySfx(&this->actor, NA_SE_EN_LIZALFOS_DOWN);
        }
        return;
    }
    if (this->actor.category != ACTORCAT_PROP) {
        Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_PROP);
    }
    if (this->alpha == 0) {
        Actor_Kill(&this->actor);
        return;
    }
    this->alpha -= 5;
    this->actor.shape.shadowAlpha = this->alpha;
}

static void EnZf_SetupDie(EnZf* this) {
    Animation_Change(&this->skelAnime, ANIM(sDyingAnim), 1.5f, 0.0f, LAST_FRAME(sDyingAnim), ANIMMODE_ONCE, -4.0f);
    if (IsRestingOnGround(this)) {
        this->actor.speed = 0.0f;
        this->isAirborne = false;
    } else {
        this->isAirborne = true;
    }
    this->action = ZF_ACTION_DIE;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_DEAD);
    this->actionFunc = EnZf_Die;
}

static bool EnZf_DodgeProjectile(EnZf* this, PlayState* play) {
    Actor* projectile = func_800BC270(play, &this->actor, 600.0f, LIZALFOS_PROJECTILE_FLAGS);

    if (projectile == NULL) {
        return false;
    }
    s16 yawToProjectile = Actor_WorldYawTowardActor(&this->actor, projectile) - this->actor.shape.rot.y;
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x3FFF;
    s16 blockedSides = EnZf_IsLedgeAhead(this, play, -8.0f) ? 1 : 0;
    if (EnZf_IsLedgeAhead(this, play, 8.0f)) {
        blockedSides |= 2;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    bool isCornered = (this->actor.xzDistToPlayer < 90.0f) || (blockedSides == 3);
    if ((isCornered && !EnZf_IsLedgeAhead(this, play, 135.0f)) || (projectile->id == ACTOR_ARMS_HOOK)) {
        EnZf_SetupJumpUp(this);
        return true;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x3FFF;
    bool goesRight = ((blockedSides == 0) ? (play->gameplayFrames % 2) : blockedSides) & 1;
    if ((ABS(yawToProjectile) < 0x2000) || (ABS(yawToProjectile) >= 0x6000)) {
        EnZf_SetupDodgeCircle(this, goesRight ? 8.0f : -8.0f);
    } else if (ABS(yawToProjectile) < 0x5FFF) {
        EnZf_SetupDodgeCircle(this, goesRight ? 4.0f : -4.0f);
    }
    return true;
}

static void EnZf_UpdateHeadRot(EnZf* this) {
    s16 lookAngle = this->actor.yawTowardsPlayer - (s16)(this->headRot + this->actor.shape.rot.y);

    this->headRot += CLAMP(lookAngle, -0x7D0, 0x7D0);
    this->headRot = CLAMP(this->headRot, -0x1CD7, 0x1CD7);
}

// Vanilla leaves a hit pending while the action is past STUNNED and handles it once the action drops back.
static void EnZf_UpdateDamage(EnZf* this, PlayState* play) {
    if (!(this->bodyCollider.base.acFlags & AC_HIT) || (this->action > ZF_ACTION_STUNNED)) {
        return;
    }
    this->bodyCollider.base.acFlags &= ~AC_HIT;
    u8 effect = this->actor.colChkInfo.damageEffect;
    this->damageEffect = effect;
    Actor_SetDropFlag(&this->actor, &this->bodyCollider.elem);
    if ((effect == ZF_DMGEFF_STUN) || (effect == ZF_DMGEFF_ICE)) {
        if (this->action != ZF_ACTION_STUNNED) {
            u16 frames =
                (effect == ZF_DMGEFF_ICE) ? (LIZALFOS_STUN_FRAMES + LIZALFOS_THAW_FRAMES) : LIZALFOS_STUN_FRAMES;
            Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, frames);
            Actor_ApplyDamage(&this->actor);
            EnZf_SetupStunned(this);
        }
        return;
    }
    Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_CRY);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
    if (Actor_ApplyDamage(&this->actor) != 0) {
        EnZf_SetupDamaged(this);
        return;
    }
    EnZf_SetupDie(this);
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x40);
    Enemy_StartFinishingBlow(play, &this->actor);
}

static void EnZf_UpdateMovement(EnZf* this, PlayState* play) {
    this->isNearLedge = false;
    if (!this->isAirborne) {
        if (this->actor.speed != 0.0f) {
            this->isNearLedge = EnZf_IsLedgeAhead(this, play, this->actor.speed * 1.5f);
        }
        if (!this->isNearLedge) {
            this->isNearLedge = EnZf_IsLedgeAhead(this, play, 0.0f);
        }
    }
    if (!this->isNearLedge) {
        Actor_MoveWithGravity(&this->actor);
    }
    Actor_UpdateBgCheckInfo(play, &this->actor, 25.0f, 30.0f, 60.0f, LIZALFOS_BGCHECK_FLAGS);
    if (!IsOnGround(this)) {
        this->isAirborne = true;
    }
}

static void EnZf_UpdateCollision(EnZf* this, PlayState* play) {
    CollisionCheckContext* colChkCtx = &play->colChkCtx;

    if ((this->actor.colChkInfo.health > 0) && (this->alpha == 255)) {
        bool isRedFlashing = (this->actor.colorFilterTimer != 0) && (this->actor.colorFilterParams & 0x4000);

        Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
        if ((this->actor.world.pos.y == this->actor.floorHeight) && (this->action <= ZF_ACTION_DAMAGED)) {
            CollisionCheck_SetOC(play, colChkCtx, &this->bodyCollider.base);
        }
        if (!isRedFlashing) {
            CollisionCheck_SetAC(play, colChkCtx, &this->bodyCollider.base);
        }
    }
    bool isSwinging =
        (this->action == ZF_ACTION_SLASH) && (this->skelAnime.curFrame >= 14.0f) && (this->skelAnime.curFrame <= 20.0f);
    if (!isSwinging) {
        return;
    }
    if ((this->swordCollider.base.atFlags & AT_BOUNCED) || (this->swordCollider.base.acFlags & AC_HIT)) {
        this->swordCollider.base.atFlags &= ~AT_BOUNCED;
        this->swordCollider.base.acFlags &= ~AC_HIT;
        EnZf_SetupRecoil(this);
        return;
    }
    CollisionCheck_SetAT(play, colChkCtx, &this->swordCollider.base);
}

static void EnZf_Update(Actor* thisx, PlayState* play) {
    EnZf* this = (EnZf*)thisx;

    EnZf_UpdateDamage(this, play);
    EnZf_UpdateMovement(this, play);
    this->actionFunc(this, play);
    if ((this->actor.colChkInfo.health > 0) && (this->action != ZF_ACTION_SLASH) &&
        (this->action != ZF_ACTION_STUNNED)) {
        EnZf_UpdateHeadRot(this);
    }
    if (this->action >= ZF_ACTION_DIE) {
        Math_SmoothStepToS(&this->headRot, 0, 1, 2000, 0);
    }
    this->actor.focus.pos = this->actor.world.pos;
    this->actor.focus.pos.y += 40.0f;
    EnZf_UpdateCollision(this, play);
}

static void EnZf_Init(Actor* thisx, PlayState* play) {
    EnZf* this = (EnZf*)thisx;
    EffectBlureInit1 slashBlure;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    this->actor.attentionRangeType = ATTENTION_RANGE_3;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawFeet, 90.0f);
    this->actor.colChkInfo.mass = MASS_HEAVY;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    memset(&slashBlure, 0, sizeof(slashBlure));
    for (s32 i = 0; i < 4; i++) {
        slashBlure.p1StartColor[i] = 255;
        slashBlure.p2StartColor[i] = 255;
        slashBlure.p1EndColor[i] = 255;
        slashBlure.p2EndColor[i] = 255;
    }
    slashBlure.p2StartColor[3] = 64;
    slashBlure.p1EndColor[3] = 0;
    slashBlure.p2EndColor[3] = 0;
    slashBlure.elemDuration = 8;
    slashBlure.calcMode = 2;
    Effect_Add(play, &this->blureIndex, EFFECT_BLURE1, 0, 0, &slashBlure);
    Actor_UpdateBgCheckInfo(play, &this->actor, 75.0f, 45.0f, 45.0f, LIZALFOS_BGCHECK_FLAGS);
    this->alpha = 255;
    this->actor.colChkInfo.cylRadius = 40;
    this->actor.colChkInfo.cylHeight = 100;
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyColliderInit);
    Collider_InitQuad(play, &this->swordCollider);
    Collider_SetQuad(play, &this->swordCollider, &this->actor, &sSwordColliderInit);
    this->actor.colChkInfo.health = LIZALFOS_HEALTH;
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sSkel, ANIM(sCryingAnim), this->jointTable,
                   this->morphTable, ZF_LIMB_MAX);
    EnZf_SetupDropIn(this);
}

static void EnZf_Destroy(Actor* thisx, PlayState* play) {
    EnZf* this = (EnZf*)thisx;

    Effect_Destroy(play, this->blureIndex);
    Collider_DestroyCylinder(play, &this->bodyCollider);
    Collider_DestroyQuad(play, &this->swordCollider);
}

static s32 EnZf_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx,
                                 Gfx** gfx) {
    EnZf* this = (EnZf*)thisx;

    if (limbIndex == ZF_LIMB_HEAD_ROOT) {
        rot->y -= this->headRot;
    }
    return false;
}

static void EnZf_UpdateSword(EnZf* this) {
    static Vec3f sQuadOffset0 = { -600.0f, -3000.0f, 1000.0f };
    static Vec3f sQuadOffset1 = { 0.0f, 1500.0f, 0.0f };
    static Vec3f sQuadOffset2 = { 1500.0f, -3000.0f, 0.0f };
    static Vec3f sQuadOffset3 = { -600.0f, -3000.0f, -1000.0f };
    static Vec3f sTrailTip = { 300.0f, -1700.0f, 0.0f };
    static Vec3f sTrailBase = { -600.0f, 300.0f, 0.0f };
    ColliderQuadDim* quad = &this->swordCollider.dim;
    Vec3f tip;
    Vec3f base;

    Matrix_MultVec3f(&sQuadOffset1, &quad->quad[1]);
    Matrix_MultVec3f(&sQuadOffset0, &quad->quad[0]);
    Matrix_MultVec3f(&sQuadOffset3, &quad->quad[3]);
    Matrix_MultVec3f(&sQuadOffset2, &quad->quad[2]);
    Collider_SetQuadVertices(&this->swordCollider, &quad->quad[0], &quad->quad[1], &quad->quad[2], &quad->quad[3]);
    Matrix_MultVec3f(&sTrailTip, &tip);
    Matrix_MultVec3f(&sTrailBase, &base);
    if (this->action != ZF_ACTION_SLASH) {
        return;
    }
    if (this->skelAnime.curFrame < 14.0f) {
        EffectBlure_AddSpace(Effect_GetByIndex(this->blureIndex));
    } else if (this->skelAnime.curFrame < 20.0f) {
        EffectBlure_AddVertex(Effect_GetByIndex(this->blureIndex), &tip, &base);
    }
}

static s32 EnZf_GetBodyPart(s32 limbIndex) {
    switch (limbIndex) {
        case ZF_LIMB_HEAD:
            return 0;
        case ZF_LIMB_NECK:
            return 1;
        case ZF_LIMB_CHEST_ARMOR:
            return 2;
        case ZF_LIMB_RIGHT_FOREARM:
            return 3;
        case ZF_LIMB_LEFT_FOREARM:
            return 4;
        case ZF_LIMB_TRUNK:
            return 5;
        case ZF_LIMB_SWORD_ROOT:
            return 6;
        case ZF_LIMB_RIGHT_SHIN:
            return 7;
        case ZF_LIMB_LEFT_SHIN_ROOT:
            return 8;
        default:
            return -1;
    }
}

static void EnZf_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx, Gfx** gfx) {
    static Vec3f sFootOffset = { 300.0f, 0.0f, 0.0f };
    static Vec3f sOrigin = { 0.0f, 0.0f, 0.0f };
    EnZf* this = (EnZf*)thisx;

    if (limbIndex == ZF_LIMB_SWORD) {
        EnZf_UpdateSword(this);
    } else {
        Actor_SetFeetPos(&this->actor, limbIndex, ZF_LIMB_LEFT_FOOT, &sFootOffset, ZF_LIMB_RIGHT_FOOT, &sFootOffset);
    }
    if (limbIndex == ZF_LIMB_LEFT_FOOT) {
        Matrix_MultVec3f(&sFootOffset, &this->leftFootPos);
    } else if (limbIndex == ZF_LIMB_RIGHT_FOOT) {
        Matrix_MultVec3f(&sFootOffset, &this->rightFootPos);
    }
    s32 bodyPart = EnZf_GetBodyPart(limbIndex);
    if (bodyPart >= 0) {
        Matrix_MultVec3f(&sOrigin, &this->bodyPartsPos[bodyPart]);
    }
}

static void EnZf_Draw(Actor* thisx, PlayState* play) {
    EnZf* this = (EnZf*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    func_800B8050(&this->actor, play, 1);
    if (this->alpha == 255) {
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, this->alpha);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sTextureScaleDL);
        gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sEmptyDL);
        POLY_OPA_DISP = SkelAnime_Draw(play, this->skelAnime.skeleton, this->skelAnime.jointTable,
                                       EnZf_OverrideLimbDraw, EnZf_PostLimbDraw, &this->actor, POLY_OPA_DISP);
    } else {
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, this->alpha);
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)sTextureScaleDL);
        gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)sXluRenderModeDL);
        POLY_XLU_DISP = SkelAnime_Draw(play, this->skelAnime.skeleton, this->skelAnime.jointTable,
                                       EnZf_OverrideLimbDraw, EnZf_PostLimbDraw, &this->actor, POLY_XLU_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    Actor_DrawDamageEffects(play, &this->actor, this->bodyPartsPos, LIZALFOS_BODYPART_COUNT, 0.55f, 0.825f,
                            this->drawDmgEffAlpha, ACTOR_DRAW_DMGEFF_FROZEN_NO_SFX);
}

// Ship: a jumpslash (sword, stick or hammer; Goron here), explosives, the slingshot or the bow.
static bool CanKillLizalfos(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RI_DEKU_STICK) ||
           sApi->LogicHasCapability("CanBeGoron") || sApi->LogicHasCapability("HasExplosives") ||
           sApi->LogicCanUseItem(RI_BOW);
}

static s16 RegisterLizalfos(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = LIZALFOS_KEY;
    definition.description = "Lizalfos";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = LIZALFOS_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnZf);
    definition.init = EnZf_Init;
    definition.destroy = EnZf_Destroy;
    definition.update = EnZf_Update;
    definition.draw = EnZf_Draw;
    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    definition.naviHint = "Lizalfos&A nimble lizard swordsman that dodges&arrows. Strike right after it swings!";
    // The host finds the soul item as "soul." + soulKey, so this is the family key.
    definition.soulKey = LIZALFOS_KEY;
    definition.logicKill = CanKillLizalfos;
    definition.enemyCategory = S2H_ENEMY_CATEGORY_REGULAR;
    definition.soulModel.skeleton = sSkel;
    definition.soulModel.idleAnimation = sSidesteppingAnim;
    definition.soulModel.limbCount = ZF_LIMB_MAX;
    definition.soulModel.scale = 0.012f;
    definition.soulModel.flameRed = 140;
    definition.soulModel.flameGreen = 230;
    definition.soulModel.flameBlue = 120;
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
    if (!RequireOotAsset(ZF_OBJECT "gZfLizalfosSkel")) {
        sApi->Log(LIZALFOS_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    if (RegisterLizalfos() < 0) {
        sApi->Log("Could not register " LIZALFOS_KEY);
    }
}
