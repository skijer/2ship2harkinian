/*
 * Ocarina of Time's Stalfos (En_Test) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * oot.stalfos rises from the ground; oot.stalfos_pair brings a partner, and a fallen one revives while the other
 * still stands. MM has no BodyBreak, so a defeated Stalfos scatters its bones as debris instead.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define STALFOS_KEY "oot.stalfos"
#define STALFOS_PAIR_KEY "oot.stalfos_pair"
#define SK_OBJECT "objects/oot_object_sk2/"
#define SK_ASSET(name) "__OTR__" SK_OBJECT name

static const ALIGN_ASSET(2) char sSkel[] = SK_ASSET("gStalfosSkel");
static const ALIGN_ASSET(2) char sFlinchBehindAnim[] = SK_ASSET("gStalfosFlinchFromHitBehindAnim");
static const ALIGN_ASSET(2) char sFallBackwardsAnim[] = SK_ASSET("gStalfosFallOverBackwardsAnim");
static const ALIGN_ASSET(2) char sJumpBackAnim[] = SK_ASSET("gStalfosJumpBackwardsAnim");
static const ALIGN_ASSET(2) char sBlockAnim[] = SK_ASSET("gStalfosBlockWithShieldAnim");
static const ALIGN_ASSET(2) char sFastAdvanceAnim[] = SK_ASSET("gStalfosFastAdvanceAnim");
static const ALIGN_ASSET(2) char sGuardAnim[] = SK_ASSET("gStalfosMiddleGuardAnim");
static const ALIGN_ASSET(2) char sSlowAdvanceAnim[] = SK_ASSET("gStalfosSlowAdvanceAnim");
static const ALIGN_ASSET(2) char sFlinchFrontAnim[] = SK_ASSET("gStalfosFlinchFromHitFrontAnim");
static const ALIGN_ASSET(2) char sFallForwardsAnim[] = SK_ASSET("gStalfosFallOverForwardsAnim");
static const ALIGN_ASSET(2) char sJumpAnim[] = SK_ASSET("gStalfosJumpAnim");
static const ALIGN_ASSET(2) char sJumpslashAnim[] = SK_ASSET("gStalfosJumpslashAnim");
static const ALIGN_ASSET(2) char sDownSlashAnim[] = SK_ASSET("gStalfosDownSlashAnim");
static const ALIGN_ASSET(2) char sDownSlashEndAnim[] = SK_ASSET("gStalfosRecoverFromDownSlashAnim");
static const ALIGN_ASSET(2) char sUpSlashAnim[] = SK_ASSET("gStalfosUpSlashAnim");
static const ALIGN_ASSET(2) char sLandAnim[] = SK_ASSET("gStalfosLandFromLeapAnim");
static const ALIGN_ASSET(2) char sSidestepAnim[] = SK_ASSET("gStalfosSidestepAnim");

#define ANIM(asset) ((AnimationHeader*)(asset))
#define LAST_FRAME(asset) Animation_GetLastFrame((void*)(asset))

typedef enum {
    STALFOS_LIMB_NONE,
    STALFOS_LIMB_ROOT,
    STALFOS_LIMB_UPPERBODY_ROOT,
    STALFOS_LIMB_CORE_LOWER_ROOT,
    STALFOS_LIMB_CORE_UPPER_ROOT,
    STALFOS_LIMB_NECK_ROOT,
    STALFOS_LIMB_HEAD_ROOT,
    STALFOS_LIMB_7,
    STALFOS_LIMB_8,
    STALFOS_LIMB_JAW_ROOT,
    STALFOS_LIMB_JAW,
    STALFOS_LIMB_HEAD,
    STALFOS_LIMB_NECK_UPPER,
    STALFOS_LIMB_NECK_LOWER,
    STALFOS_LIMB_CORE_UPPER,
    STALFOS_LIMB_CHEST,
    STALFOS_LIMB_SHOULDER_R_ROOT,
    STALFOS_LIMB_SHOULDER_ARMOR_R_ROOT,
    STALFOS_LIMB_SHOULDER_ARMOR_R,
    STALFOS_LIMB_SHOULDER_L_ROOT,
    STALFOS_LIMB_SHOULDER_ARMOR_L_ROOT,
    STALFOS_LIMB_SHOULDER_ARMOR_L,
    STALFOS_LIMB_ARM_L_ROOT,
    STALFOS_LIMB_UPPERARM_L_ROOT,
    STALFOS_LIMB_FOREARM_L_ROOT,
    STALFOS_LIMB_HAND_L_ROOT,
    STALFOS_LIMB_HAND_L,
    STALFOS_LIMB_SHIELD,
    STALFOS_LIMB_FOREARM_L,
    STALFOS_LIMB_UPPERARM_L,
    STALFOS_LIMB_ARM_R_ROOT,
    STALFOS_LIMB_UPPERARM_R_ROOT,
    STALFOS_LIMB_FOREARM_R_ROOT,
    STALFOS_LIMB_HAND_R_ROOT,
    STALFOS_LIMB_SWORD,
    STALFOS_LIMB_HAND_R,
    STALFOS_LIMB_FOREARM_R,
    STALFOS_LIMB_UPPERARM_R,
    STALFOS_LIMB_CORE_LOWER,
    STALFOS_LIMB_LOWERBODY_ROOT,
    STALFOS_LIMB_WAIST_ROOT,
    STALFOS_LIMB_LEGS_ROOT,
    STALFOS_LIMB_LEG_L_ROOT,
    STALFOS_LIMB_THIGH_L_ROOT,
    STALFOS_LIMB_LOWERLEG_L_ROOT,
    STALFOS_LIMB_ANKLE_L_ROOT,
    STALFOS_LIMB_ANKLE_L,
    STALFOS_LIMB_FOOT_L_ROOT,
    STALFOS_LIMB_FOOT_L,
    STALFOS_LIMB_LOWERLEG_L,
    STALFOS_LIMB_THIGH_L,
    STALFOS_LIMB_LEG_R_ROOT,
    STALFOS_LIMB_THIGH_R_ROOT,
    STALFOS_LIMB_LOWERLEG_R_ROOT,
    STALFOS_LIMB_ANKLE_R_ROOT,
    STALFOS_LIMB_ANKLE_R,
    STALFOS_LIMB_FOOT_R_ROOT,
    STALFOS_LIMB_FOOT_R,
    STALFOS_LIMB_LOWERLEG_R,
    STALFOS_LIMB_THIGH_R,
    STALFOS_LIMB_WAIST,
    STALFOS_LIMB_MAX
} StalfosLimb;

// Vanilla's numbered states; only the ones other code compares against carry a name.
typedef enum {
    STALFOS_STATE_WAIT = 0x0,
    STALFOS_STATE_RISE = 0x3,
    STALFOS_STATE_JUMP_UP = 0x4,
    STALFOS_STATE_FALLING = 0x5,
    STALFOS_STATE_FLINCH = 0x8,
    STALFOS_STATE_VULNERABLE = 0xA,
    STALFOS_STATE_STUNNED = 0xB,
    STALFOS_STATE_LAND = 0xC,
    STALFOS_STATE_WALK = 0xD,
    STALFOS_STATE_CIRCLE = 0xE,
    STALFOS_STATE_STRAFE = 0xF,
    STALFOS_STATE_SLASH_DOWN = 0x10,
    STALFOS_STATE_SLASH_UP = 0x11,
    STALFOS_STATE_SLASH_DOWN_END = 0x12,
    STALFOS_STATE_RECOIL = 0x13,
    STALFOS_STATE_JUMP_BACK = 0x14,
    STALFOS_STATE_BLOCK = 0x15,
    STALFOS_STATE_UNBLOCK = 0x16,
    STALFOS_STATE_JUMPSLASH = 0x17,
    STALFOS_STATE_SIDESTEP = 0x18,
} StalfosState;

typedef enum {
    SHIELD_BLEND_NONE,
    SHIELD_BLEND_START,
    SHIELD_BLEND_HOLD,
    SHIELD_BLEND_RELEASE,
    SHIELD_BLEND_FADING,
    SHIELD_BLEND_FULL_BLOCK,
} ShieldBlend;

typedef enum {
    STALFOS_DMGEFF_NORMAL = 0x0,
    STALFOS_DMGEFF_STUN = 0x1,
    STALFOS_DMGEFF_IGNORE = 0xD,
    STALFOS_DMGEFF_LIGHT = 0xE,
    STALFOS_DMGEFF_FREEZE = 0xF,
} StalfosDamageEffect;

#define STALFOS_HEALTH 10
#define STALFOS_RISE_DISTANCE 600.0f
#define STALFOS_DOWN_FRAMES 80
#define STALFOS_DEBRIS_COUNT 9
#define STALFOS_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED)
#define STALFOS_PROJECTILE_FLAGS 0xFFFFFFFF

static const S2HModApi* sApi;
static s16 sStalfosId = -1;
static s16 sPairId = -1;
static s32 sBgmUsers = 0;

struct EnTest;
typedef void (*EnTestActionFunc)(struct EnTest*, PlayState*);

typedef struct EnTest {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[STALFOS_LIMB_MAX];
    Vec3s morphTable[STALFOS_LIMB_MAX];
    SkelAnime upperSkelAnime;
    Vec3s upperJointTable[STALFOS_LIMB_MAX];
    Vec3s upperMorphTable[STALFOS_LIMB_MAX];
    EnTestActionFunc actionFunc;
    ColliderCylinder bodyCollider;
    ColliderQuad swordCollider;
    ColliderCylinder shieldCollider;
    Gfx* debrisDLs[STALFOS_DEBRIS_COUNT];
    Vec3f debrisPos[STALFOS_DEBRIS_COUNT];
    Vec3s headRot;
    s32 timer;
    s32 effectIndex;
    s32 fallImpact;
    f32 approachSpeed;
    s16 downTimer;
    u8 state;
    u8 shieldBlend;
    u8 lastDamageEffect;
    s8 meleeWeaponState;
    u8 isFallingForwards;
    u8 isPair;
    u8 hasPartner;
    u8 isBgmUser;
} EnTest;

static u8 sUpperBodyCopyFlags[STALFOS_LIMB_MAX] = {
    false, false, false, false, true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,  true,  true,  false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false, false,
};

static const u8 sDebrisLimbs[STALFOS_DEBRIS_COUNT] = {
    STALFOS_LIMB_HEAD,       STALFOS_LIMB_CHEST, STALFOS_LIMB_SWORD,   STALFOS_LIMB_SHIELD,  STALFOS_LIMB_UPPERARM_R,
    STALFOS_LIMB_UPPERARM_L, STALFOS_LIMB_WAIST, STALFOS_LIMB_THIGH_L, STALFOS_LIMB_THIGH_R,
};

static ColliderCylinderInit sBodyColliderInit = {
    {
        COL_MATERIAL_HIT5,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_NONE,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 25, 65, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sShieldColliderInit = {
    {
        COL_MATERIAL_METAL,
        AT_NONE,
        AC_ON | AC_HARD | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xF7C1FFFF, 0x00, 0x00 },
        ATELEM_NONE,
        ACELEM_ON,
        OCELEM_NONE,
    },
    { 20, 70, -50, { 0, 0, 0 } },
};

static ColliderQuadInit sSwordColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_QUAD,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x00, 0x10 },
        { 0x00000000, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL | ATELEM_UNK7,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static DamageTable sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, STALFOS_DMGEFF_STUN),
    /* Deku Stick     */ DMG_ENTRY(1, STALFOS_DMGEFF_NORMAL),
    /* Horse trample  */ DMG_ENTRY(0, STALFOS_DMGEFF_NORMAL),
    /* Explosives     */ DMG_ENTRY(1, STALFOS_DMGEFF_NORMAL),
    /* Zora boomerang */ DMG_ENTRY(0, STALFOS_DMGEFF_STUN),
    /* Normal arrow   */ DMG_ENTRY(1, STALFOS_DMGEFF_NORMAL),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, STALFOS_DMGEFF_NORMAL),
    /* Hookshot       */ DMG_ENTRY(0, STALFOS_DMGEFF_STUN),
    /* Goron punch    */ DMG_ENTRY(1, STALFOS_DMGEFF_NORMAL),
    /* Sword          */ DMG_ENTRY(1, STALFOS_DMGEFF_NORMAL),
    /* Goron pound    */ DMG_ENTRY(1, STALFOS_DMGEFF_NORMAL),
    /* Fire arrow     */ DMG_ENTRY(1, STALFOS_DMGEFF_NORMAL),
    /* Ice arrow      */ DMG_ENTRY(2, STALFOS_DMGEFF_FREEZE),
    /* Light arrow    */ DMG_ENTRY(2, STALFOS_DMGEFF_LIGHT),
    /* Goron spikes   */ DMG_ENTRY(1, STALFOS_DMGEFF_NORMAL),
    /* Deku spin      */ DMG_ENTRY(0, STALFOS_DMGEFF_STUN),
    /* Deku bubble    */ DMG_ENTRY(1, STALFOS_DMGEFF_IGNORE),
    /* Deku launch    */ DMG_ENTRY(1, STALFOS_DMGEFF_NORMAL),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, STALFOS_DMGEFF_NORMAL),
    /* Zora barrier   */ DMG_ENTRY(0, STALFOS_DMGEFF_STUN),
    /* Normal shield  */ DMG_ENTRY(0, STALFOS_DMGEFF_NORMAL),
    /* Light ray      */ DMG_ENTRY(0, STALFOS_DMGEFF_NORMAL),
    /* Thrown object  */ DMG_ENTRY(1, STALFOS_DMGEFF_NORMAL),
    /* Zora punch     */ DMG_ENTRY(1, STALFOS_DMGEFF_NORMAL),
    /* Spin attack    */ DMG_ENTRY(1, STALFOS_DMGEFF_NORMAL),
    /* Sword beam     */ DMG_ENTRY(1, STALFOS_DMGEFF_NORMAL),
    /* Normal Roll    */ DMG_ENTRY(0, STALFOS_DMGEFF_NORMAL),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, STALFOS_DMGEFF_NORMAL),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, STALFOS_DMGEFF_NORMAL),
    /* Unblockable    */ DMG_ENTRY(0, STALFOS_DMGEFF_NORMAL),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, STALFOS_DMGEFF_NORMAL),
    /* Powder Keg     */ DMG_ENTRY(2, STALFOS_DMGEFF_NORMAL),
};

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(lockOnArrowOffset, 500, ICHAIN_CONTINUE),
    ICHAIN_VEC3F_DIV1000(scale, 15, ICHAIN_CONTINUE),
    ICHAIN_F32(scale.y, 0, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(gravity, -1500, ICHAIN_STOP),
};

static void EnTest_SetupJumpBack(EnTest* this);
static void EnTest_SetupJumpslash(EnTest* this);
static void EnTest_SetupWalkAndBlock(EnTest* this);
static void EnTest_SetupStrafe(EnTest* this);
static void EnTest_SetupSlashDown(EnTest* this);
static void EnTest_SetupCircle(EnTest* this);
static void EnTest_SetupIdle(EnTest* this);
static void EnTest_SetupStopAndBlock(EnTest* this);
static void EnTest_SetupSidestep(EnTest* this, PlayState* play);
static void EnTest_SetupJumpUp(EnTest* this);
static void EnTest_SetupFall(EnTest* this, PlayState* play, bool isForwards);
static bool EnTest_ReactToProjectile(EnTest* this, PlayState* play);

static bool IsStalfos(Actor* actor) {
    return (actor->id == sStalfosId) || (actor->id == sPairId);
}

static bool IsAnotherStalfosStanding(EnTest* this, PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].first; actor != NULL; actor = actor->next) {
        if ((actor != &this->actor) && IsStalfos(actor)) {
            return true;
        }
    }
    return false;
}

static bool IsPartnerStanding(EnTest* this, PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].first; actor != NULL; actor = actor->next) {
        if ((actor != &this->actor) && (actor->id == sPairId)) {
            return true;
        }
    }
    return false;
}

static void EnTest_StartBgm(EnTest* this) {
    if (this->isBgmUser) {
        return;
    }
    if (sBgmUsers == 0) {
        Audio_PlayBgm_StorePrevBgm(NA_BGM_MINI_BOSS);
    }
    sBgmUsers++;
    this->isBgmUser = true;
}

static void EnTest_StopBgm(EnTest* this) {
    if (!this->isBgmUser) {
        return;
    }
    this->isBgmUser = false;
    sBgmUsers--;
    if (sBgmUsers == 0) {
        Audio_RestorePrevBgm();
    }
}

static s16 AbsYawDiff(s16 a, s16 b) {
    s16 diff = a - b;

    return ABS(diff);
}

static bool IsPlayerHostileLockedOn(Player* player) {
    return (player->stateFlags3 & PLAYER_STATE3_HOSTILE_LOCK_ON) != 0;
}

static bool IsWallAhead(EnTest* this) {
    return (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) &&
           (AbsYawDiff(this->actor.wallYaw, this->actor.shape.rot.y) < 0x38A4) && (this->actor.xzDistToPlayer < 80.0f);
}

static void EnTest_SetupSlashOrDodge(EnTest* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!IsPlayerHostileLockedOn(player) || this->actor.isLockedOn) {
        EnTest_SetupSlashDown(this);
        return;
    }
    EnTest_SetupSidestep(this, play);
}

static void EnTest_ChooseRandomAction(EnTest* this, PlayState* play) {
    switch ((u32)(Rand_ZeroOne() * 10.0f)) {
        case 0:
        case 1:
        case 5:
        case 6:
            if ((this->actor.xzDistToPlayer < 220.0f) && (this->actor.xzDistToPlayer > 170.0f) &&
                Actor_IsFacingPlayer(&this->actor, 0x71C) && Actor_IsTargeted(play, &this->actor)) {
                EnTest_SetupJumpslash(this);
                break;
            }
            EnTest_SetupWalkAndBlock(this);
            break;
        case 8:
            EnTest_SetupWalkAndBlock(this);
            break;
        case 3:
        case 4:
        case 7:
            EnTest_SetupSidestep(this, play);
            break;
        default:
            EnTest_SetupStopAndBlock(this);
            break;
    }
}

static void EnTest_ChooseAction(EnTest* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 facingDiff = AbsYawDiff(player->actor.shape.rot.y, this->actor.shape.rot.y);

    if (facingDiff >= 0x61A8) {
        switch ((u32)(Rand_ZeroOne() * 10.0f)) {
            case 0:
            case 3:
            case 7:
                EnTest_SetupStopAndBlock(this);
                break;
            case 1:
            case 5:
            case 6:
            case 8:
                EnTest_SetupSidestep(this, play);
                break;
            default:
                this->actor.world.rot.y = this->actor.yawTowardsPlayer;
                EnTest_SetupJumpBack(this);
                break;
        }
        return;
    }
    if (facingDiff > 0x3E80) {
        EnTest_ChooseRandomAction(this, play);
        return;
    }
    if (AbsYawDiff(this->actor.yawTowardsPlayer, this->actor.shape.rot.y) > 0x3E80) {
        if ((play->gameplayFrames % 2) != 0) {
            this->actor.world.rot.y = this->actor.yawTowardsPlayer;
            EnTest_SetupJumpBack(this);
        } else if ((this->actor.xzDistToPlayer < 220.0f) && (this->actor.xzDistToPlayer > 170.0f)) {
            if (Actor_IsFacingPlayer(&this->actor, 0x71C) && !Actor_IsTargeted(play, &this->actor)) {
                EnTest_SetupJumpslash(this);
            }
        } else {
            EnTest_SetupWalkAndBlock(this);
        }
        return;
    }
    if (this->actor.xzDistToPlayer >= 110.0f) {
        EnTest_ChooseRandomAction(this, play);
        return;
    }
    if (Rand_ZeroOne() > 0.2f) {
        EnTest_SetupSlashOrDodge(this, play);
    }
}

static void EnTest_PlayStepSfx(EnTest* this, s32 prevFrame, s32 firstStep, s32 secondStep) {
    s32 afterStep = this->skelAnime.curFrame - ABS(this->skelAnime.playSpeed);
    s32 reach = (s32)ABS(this->skelAnime.playSpeed) + prevFrame;

    if ((s32)this->skelAnime.curFrame == prevFrame) {
        return;
    }
    if (((reach > firstStep) && (afterStep <= 0)) || ((afterStep < 7) && (reach >= secondStep))) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_STAL_WALK);
    }
}

static void EnTest_Rise(EnTest* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.scale.y < 0.015f) {
        this->actor.scale.y += 0.002f;
        this->actor.world.pos.y = this->actor.home.pos.y - 3.5f;
        return;
    }
    this->actor.world.pos.y = this->actor.home.pos.y;
    EnTest_SetupJumpBack(this);
}

static void EnTest_WaitGround(EnTest* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    bool isPlayerNear =
        (fabsf(this->actor.playerHeightRel) < 150.0f) && (this->actor.xzDistToPlayer < STALFOS_RISE_DISTANCE);
    if ((this->timer == 0) && isPlayerNear) {
        this->state = STALFOS_STATE_RISE;
        this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
        EnTest_StartBgm(this);
        this->actionFunc = EnTest_Rise;
        return;
    }
    if (this->timer != 0) {
        this->timer--;
    }
    this->actor.world.pos.y = this->actor.home.pos.y - 3.5f;
}

static void EnTest_SetupWaitGround(EnTest* this) {
    Animation_PlayLoop(&this->skelAnime, ANIM(sGuardAnim));
    this->state = STALFOS_STATE_WAIT;
    this->timer = 15;
    this->actor.scale.y = 0.0f;
    this->actor.world.pos.y = this->actor.home.pos.y - 3.5f;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actionFunc = EnTest_WaitGround;
}

static void EnTest_ReactToPlayerSwing(EnTest* this, PlayState* play, bool canJumpUp) {
    Player* player = GET_PLAYER(play);

    if (canJumpUp && IsWallAhead(this)) {
        EnTest_SetupJumpUp(this);
    } else if ((Rand_ZeroOne() > 0.7f) && (player->meleeWeaponAnimation != PLAYER_MWA_JUMPSLASH_START)) {
        EnTest_SetupJumpBack(this);
    } else {
        EnTest_SetupStopAndBlock(this);
    }
}

static void EnTest_Idle(EnTest* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&this->skelAnime);
    if (EnTest_ReactToProjectile(this, play)) {
        return;
    }
    if ((this->actor.xzDistToPlayer < 100.0f) && (player->meleeWeaponState != 0) &&
        (AbsYawDiff(player->actor.shape.rot.y, this->actor.shape.rot.y) >= 0x1F40)) {
        this->actor.shape.rot.y = this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        if ((Rand_ZeroOne() > 0.7f) && (player->meleeWeaponAnimation != PLAYER_MWA_JUMPSLASH_START)) {
            EnTest_SetupJumpBack(this);
        } else {
            EnTest_SetupSidestep(this, play);
        }
        return;
    }
    if (this->timer != 0) {
        this->timer--;
        return;
    }
    if (!Actor_IsFacingPlayer(&this->actor, 0x1555)) {
        if (Rand_ZeroOne() > 0.7f) {
            EnTest_SetupCircle(this);
        } else {
            EnTest_ChooseAction(this, play);
        }
        return;
    }
    bool isJumpslashRange = (this->actor.xzDistToPlayer < 220.0f) && (this->actor.xzDistToPlayer > 160.0f);
    if (isJumpslashRange && (Rand_ZeroOne() < 0.3f)) {
        if (Actor_IsTargeted(play, &this->actor)) {
            EnTest_SetupJumpslash(this);
        } else {
            EnTest_SetupSidestep(this, play);
        }
    } else if (Rand_ZeroOne() > 0.3f) {
        EnTest_SetupWalkAndBlock(this);
    } else {
        EnTest_SetupSidestep(this, play);
    }
}

static void EnTest_SetupIdle(EnTest* this) {
    Animation_PlayLoop(&this->skelAnime, ANIM(sGuardAnim));
    this->state = STALFOS_STATE_VULNERABLE;
    this->timer = (Rand_ZeroOne() * 10.0f) + 5.0f;
    this->actor.speed = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = EnTest_Idle;
}

static void EnTest_Land(EnTest* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        EnTest_SetupIdle(this);
        this->timer = (Rand_ZeroOne() * 10.0f) + 5.0f;
    }
}

static void EnTest_WalkAndBlock(EnTest* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 keepAway = Actor_OtherIsTargeted(play, &this->actor) ? 150.0f : 0.0f;

    if (EnTest_ReactToProjectile(this, play)) {
        return;
    }
    this->timer++;
    if (this->actor.xzDistToPlayer <= (80.0f + keepAway)) {
        Math_SmoothStepToF(&this->actor.speed, -5.0f, 1.0f, 0.8f, 0.0f);
    } else if (this->actor.xzDistToPlayer > (110.0f + keepAway)) {
        Math_SmoothStepToF(&this->actor.speed, 5.0f, 1.0f, 0.8f, 0.0f);
    }
    this->actor.speed = CLAMP(this->actor.speed, -5.0f, 5.0f);
    f32 playSpeed;
    if (fabsf(this->actor.speed) < 3.0f) {
        Animation_Change(&this->skelAnime, ANIM(sSlowAdvanceAnim), 0.0f, this->skelAnime.curFrame,
                         LAST_FRAME(sSlowAdvanceAnim), ANIMMODE_LOOP, -6.0f);
        playSpeed = this->actor.speed * 10.0f;
    } else {
        Animation_Change(&this->skelAnime, ANIM(sFastAdvanceAnim), 0.0f, this->skelAnime.curFrame,
                         LAST_FRAME(sFastAdvanceAnim), ANIMMODE_LOOP, -4.0f);
        playSpeed = this->actor.speed * 10.0f * 0.02f;
    }
    if (this->actor.speed >= 0.0f) {
        if (this->shieldBlend == SHIELD_BLEND_NONE) {
            this->shieldBlend = SHIELD_BLEND_START;
        }
        this->skelAnime.playSpeed = CLAMP_MAX(playSpeed, 2.5f);
    } else {
        this->skelAnime.playSpeed = CLAMP_MIN(playSpeed, -2.5f);
    }
    if ((this->actor.xzDistToPlayer < 100.0f) && (player->meleeWeaponState != 0) &&
        (AbsYawDiff(player->actor.shape.rot.y, this->actor.shape.rot.y) >= 0x1F40)) {
        this->actor.shape.rot.y = this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        if ((Rand_ZeroOne() > 0.7f) && (player->meleeWeaponAnimation != PLAYER_MWA_JUMPSLASH_START)) {
            EnTest_SetupJumpBack(this);
        } else {
            EnTest_SetupStopAndBlock(this);
        }
        return;
    }
    s32 prevFrame = this->skelAnime.curFrame;
    SkelAnime_Update(&this->skelAnime);
    EnTest_PlayStepSfx(this, prevFrame, 1, 8);
    if ((this->timer % 32) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_STAL_WARAU);
        this->timer += (s16)(Rand_ZeroOne() * 5.0f);
    }
    bool isJumpslashRange = (this->actor.xzDistToPlayer < 220.0f) && (this->actor.xzDistToPlayer > 160.0f);
    if (isJumpslashRange && Actor_IsFacingPlayer(&this->actor, 0x71C)) {
        if (Actor_IsTargeted(play, &this->actor)) {
            if (Rand_ZeroOne() < 0.1f) {
                EnTest_SetupJumpslash(this);
                return;
            }
        } else if (player->heldItemAction != PLAYER_IA_NONE) {
            if (!this->actor.isLockedOn) {
                EnTest_SetupStrafe(this);
            } else if ((play->gameplayFrames % 2) != 0) {
                EnTest_SetupSidestep(this, play);
                return;
            } else {
                EnTest_ChooseAction(this, play);
            }
        }
    }
    if (Rand_ZeroOne() < 0.4f) {
        this->actor.shape.rot.y = this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    }
    if (!Actor_IsFacingPlayer(&this->actor, 0x11C7)) {
        EnTest_SetupIdle(this);
        this->timer = (Rand_ZeroOne() * 10.0f) + 10.0f;
        return;
    }
    if (this->actor.xzDistToPlayer >= 110.0f) {
        if (Rand_ZeroOne() < 0.1f) {
            this->actor.speed = 5.0f;
        }
        return;
    }
    if (Rand_ZeroOne() > 0.2f) {
        EnTest_SetupSlashOrDodge(this, play);
    } else {
        EnTest_SetupStopAndBlock(this);
    }
}

static void EnTest_SetupWalkAndBlock(EnTest* this) {
    Animation_Change(&this->upperSkelAnime, ANIM(sBlockAnim), 2.0f, 0.0f, LAST_FRAME(sBlockAnim), ANIMMODE_ONCE, 2.0f);
    Animation_PlayLoop(&this->skelAnime, ANIM(sSlowAdvanceAnim));
    this->timer = (s16)(Rand_ZeroOne() * 5.0f);
    this->state = STALFOS_STATE_WALK;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = EnTest_WalkAndBlock;
}

static void EnTest_Circle(EnTest* this, PlayState* play) {
    if (EnTest_ReactToProjectile(this, play)) {
        return;
    }
    s16 yawDiff = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
    s16 yawChange = (yawDiff > 0) ? ((yawDiff / 42.0f) + 300.0f) : ((yawDiff / 42.0f) - 300.0f);
    this->actor.shape.rot.y += yawChange * 2;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    f32 playSpeed = yawChange * 0.02f;
    this->skelAnime.playSpeed = (yawDiff > 0) ? CLAMP_MAX(playSpeed, 1.0f) : CLAMP_MIN(playSpeed, -1.0f);
    s32 prevFrame = this->skelAnime.curFrame;
    SkelAnime_Update(&this->skelAnime);
    EnTest_PlayStepSfx(this, prevFrame, 2, 9);
    if (!Actor_IsFacingPlayer(&this->actor, 0x71C)) {
        return;
    }
    if (Rand_ZeroOne() <= 0.8f) {
        EnTest_SetupWalkAndBlock(this);
    } else if (Rand_ZeroOne() > 0.7f) {
        EnTest_SetupStrafe(this);
    } else {
        EnTest_ChooseAction(this, play);
    }
}

static void EnTest_SetupCircle(EnTest* this) {
    Animation_PlayLoop(&this->skelAnime, ANIM(sSidestepAnim));
    this->state = STALFOS_STATE_CIRCLE;
    this->actionFunc = EnTest_Circle;
}

static void EnTest_BounceOffWalls(EnTest* this, s16 moveYaw) {
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
        return;
    }
    s16 sideYaw = (this->actor.speed >= 0.0f) ? (s16)(moveYaw + 0x3FFF) : (s16)(moveYaw - 0x3FFF);
    if (AbsYawDiff(this->actor.wallYaw, sideYaw) <= 0x4000) {
        return;
    }
    this->actor.speed *= -0.8f;
    this->actor.speed += (this->actor.speed < 0.0f) ? -0.5f : 0.5f;
}

static void EnTest_KeepDistance(EnTest* this, PlayState* play) {
    f32 keepAway = Actor_OtherIsTargeted(play, &this->actor) ? 200.0f : 0.0f;

    if (this->actor.xzDistToPlayer <= (80.0f + keepAway)) {
        Math_SmoothStepToF(&this->approachSpeed, -2.5f, 1.0f, 0.8f, 0.0f);
    } else if (this->actor.xzDistToPlayer > (110.0f + keepAway)) {
        Math_SmoothStepToF(&this->approachSpeed, 2.5f, 1.0f, 0.8f, 0.0f);
    } else {
        Math_SmoothStepToF(&this->approachSpeed, 0.0f, 1.0f, 6.65f, 0.0f);
    }
    if (this->approachSpeed != 0.0f) {
        this->actor.world.pos.x += Math_SinS(this->actor.shape.rot.y) * this->approachSpeed;
        this->actor.world.pos.z += Math_CosS(this->actor.shape.rot.y) * this->approachSpeed;
    }
}

static void EnTest_Strafe(EnTest* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (EnTest_ReactToProjectile(this, play)) {
        return;
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xFA0, 1);
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x3E80;
    if (this->actor.speed >= 0.0f) {
        this->actor.speed = MIN(this->actor.speed + 0.5f, 6.0f);
    } else {
        this->actor.speed = MAX(this->actor.speed - 0.5f, -6.0f);
    }
    EnTest_BounceOffWalls(this, this->actor.shape.rot.y);
    EnTest_KeepDistance(this, play);
    this->skelAnime.playSpeed = this->actor.speed * 0.5f;
    s32 prevFrame = this->skelAnime.curFrame;
    SkelAnime_Update(&this->skelAnime);
    EnTest_PlayStepSfx(this, prevFrame, 1, 8);
    if ((play->gameplayFrames & 95) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_STAL_WARAU);
    }
    if ((AbsYawDiff(player->actor.shape.rot.y + 0x8000, this->actor.shape.rot.y) > 0x6800) || (this->timer == 0)) {
        EnTest_ChooseAction(this, play);
    } else {
        this->timer--;
    }
}

static void EnTest_SetupStrafe(EnTest* this) {
    Animation_PlayLoop(&this->skelAnime, ANIM(sSidestepAnim));
    this->state = STALFOS_STATE_STRAFE;
    this->actor.speed = (Rand_ZeroOne() > 0.5f) ? -0.5f : 0.5f;
    this->timer = (s16)((Rand_ZeroOne() * 15.0f) + 25.0f);
    this->approachSpeed = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = EnTest_Strafe;
}

static void EnTest_SetupSlashDownEnd(EnTest* this);
static void EnTest_SetupSlashUp(EnTest* this);

static void EnTest_SlashDown(EnTest* this, PlayState* play) {
    s32 frame = (s32)this->skelAnime.curFrame;

    this->actor.speed = 0.0f;
    if (frame < 4) {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xBB8, 0);
    }
    if (frame == 7) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_STAL_SAKEBI);
    }
    this->meleeWeaponState = ((this->skelAnime.curFrame > 7.0f) && (this->skelAnime.curFrame < 11.0f)) ? 1 : 0;
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if ((play->gameplayFrames % 2) != 0) {
        EnTest_SetupSlashDownEnd(this);
    } else {
        EnTest_SetupSlashUp(this);
    }
}

static void EnTest_StartSwing(EnTest* this, u8 damage) {
    this->swordCollider.base.atFlags &= ~AT_BOUNCED;
    this->swordCollider.elem.atDmgInfo.damage = damage;
    if (this->shieldBlend != SHIELD_BLEND_NONE) {
        this->shieldBlend = SHIELD_BLEND_RELEASE;
    }
}

static void EnTest_SetupSlashDown(EnTest* this) {
    Animation_PlayOnce(&this->skelAnime, ANIM(sDownSlashAnim));
    AudioSfx_StopByPosAndId(&this->actor.projectedPos, NA_SE_EN_STAL_WARAU);
    this->state = STALFOS_STATE_SLASH_DOWN;
    this->actor.speed = 0.0f;
    EnTest_StartSwing(this, 16);
    this->actionFunc = EnTest_SlashDown;
}

static void EnTest_SlashDownEnd(EnTest* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (this->swordCollider.base.atFlags & AT_HIT) {
        this->swordCollider.base.atFlags &= ~AT_HIT;
        EnTest_SetupJumpBack(this);
        return;
    }
    if (Rand_ZeroOne() > 0.7f) {
        EnTest_SetupIdle(this);
        this->timer = (Rand_ZeroOne() * 5.0f) + 5.0f;
        return;
    }
    this->actor.world.rot.y = Actor_WorldYawTowardActor(&this->actor, &player->actor);
    if (Rand_ZeroOne() > 0.7f) {
        EnTest_SetupJumpBack(this);
        return;
    }
    if (AbsYawDiff(player->actor.shape.rot.y, this->actor.shape.rot.y) > 0x2710) {
        EnTest_SetupSidestep(this, play);
        return;
    }
    if (AbsYawDiff(this->actor.yawTowardsPlayer, this->actor.shape.rot.y) > 0x3E80) {
        this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        EnTest_SetupJumpBack(this);
    } else if (!IsPlayerHostileLockedOn(player) || this->actor.isLockedOn) {
        EnTest_SetupSlashDown(this);
    } else if ((play->gameplayFrames % 2) != 0) {
        EnTest_SetupSidestep(this, play);
    } else {
        EnTest_SetupJumpBack(this);
    }
}

static void EnTest_SetupSlashDownEnd(EnTest* this) {
    Animation_PlayOnce(&this->skelAnime, ANIM(sDownSlashEndAnim));
    this->state = STALFOS_STATE_SLASH_DOWN_END;
    this->actor.speed = 0.0f;
    this->actionFunc = EnTest_SlashDownEnd;
}

static void EnTest_SlashUp(EnTest* this, PlayState* play) {
    this->actor.speed = 0.0f;
    if ((s32)this->skelAnime.curFrame == 2) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_STAL_SAKEBI);
    }
    this->meleeWeaponState = ((this->skelAnime.curFrame > 1.0f) && (this->skelAnime.curFrame < 8.0f)) ? 1 : 0;
    if (SkelAnime_Update(&this->skelAnime)) {
        EnTest_SetupSlashDown(this);
    }
}

static void EnTest_SetupSlashUp(EnTest* this) {
    Animation_PlayOnce(&this->skelAnime, ANIM(sUpSlashAnim));
    this->state = STALFOS_STATE_SLASH_UP;
    this->actor.speed = 0.0f;
    EnTest_StartSwing(this, 16);
    this->actionFunc = EnTest_SlashUp;
}

static void EnTest_JumpBack(EnTest* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xBB8, 1);
    if (this->timer == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_STAL_WARAU);
    } else {
        this->timer--;
    }
    if (!SkelAnime_Update(&this->skelAnime)) {
        if (this->skelAnime.curFrame == (this->skelAnime.endFrame - 4.0f)) {
            Actor_PlaySfx(&this->actor, NA_SE_EN_DODO_M_DOWN);
        }
        return;
    }
    if (EnTest_ReactToProjectile(this, play)) {
        return;
    }
    if ((this->actor.xzDistToPlayer <= 100.0f) && Actor_IsFacingPlayer(&this->actor, 0x1555)) {
        EnTest_SetupSlashDown(this);
    } else if ((this->actor.xzDistToPlayer > 100.0f) && (this->actor.xzDistToPlayer <= 220.0f) &&
               Actor_IsFacingPlayer(&this->actor, 0xE38)) {
        EnTest_SetupJumpslash(this);
    } else {
        EnTest_SetupIdle(this);
        this->timer = (Rand_ZeroOne() * 5.0f) + 5.0f;
    }
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
}

static void EnTest_SetupJumpBack(EnTest* this) {
    Animation_PlayOnce(&this->skelAnime, ANIM(sJumpBackAnim));
    Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_JUMP);
    this->state = STALFOS_STATE_JUMP_BACK;
    this->timer = 5;
    this->actor.speed = -11.0f;
    if (this->shieldBlend != SHIELD_BLEND_NONE) {
        this->shieldBlend = SHIELD_BLEND_RELEASE;
    }
    this->actionFunc = EnTest_JumpBack;
}

static void EnTest_Jumpslash(EnTest* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        if (this->timer == 0) {
            Animation_PlayOnce(&this->skelAnime, ANIM(sJumpslashAnim));
            this->timer = 1;
            this->meleeWeaponState = 1;
            Actor_PlaySfx(&this->actor, NA_SE_EN_STAL_SAKEBI);
            Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_JUMP);
        } else {
            this->actor.speed = 0.0f;
            EnTest_SetupIdle(this);
        }
    }
    if ((this->timer != 0) && (this->skelAnime.curFrame >= 5.0f)) {
        this->meleeWeaponState = 0;
    }
    if (this->actor.world.pos.y > this->actor.floorHeight) {
        return;
    }
    if (this->actor.speed != 0.0f) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_DODO_M_DOWN);
    }
    this->actor.world.pos.y = this->actor.floorHeight;
    this->actor.velocity.y = 0.0f;
    this->actor.speed = 0.0f;
}

static void EnTest_SetupJumpslash(EnTest* this) {
    Animation_PlayOnce(&this->skelAnime, ANIM(sJumpAnim));
    AudioSfx_StopByPosAndId(&this->actor.projectedPos, NA_SE_EN_STAL_WARAU);
    this->timer = 0;
    this->state = STALFOS_STATE_JUMPSLASH;
    this->actor.velocity.y = 10.0f;
    this->actor.speed = 8.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_JUMP);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    EnTest_StartSwing(this, 32);
    this->actionFunc = EnTest_Jumpslash;
}

static void EnTest_JumpUp(EnTest* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xFA0, 1);
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.world.pos.y > this->actor.floorHeight) {
        return;
    }
    Actor_PlaySfx(&this->actor, NA_SE_EN_DODO_M_DOWN);
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    this->actor.world.pos.y = this->actor.floorHeight;
    this->fallImpact = MAX(-(s32)this->actor.velocity.y, 1);
    this->actor.velocity.y = 0.0f;
    this->actor.speed = 0.0f;
    this->state = STALFOS_STATE_LAND;
    this->timer = 4;
    Animation_Change(&this->skelAnime, ANIM(sLandAnim), 0.0f, 0.0f, 0.0f, ANIMMODE_ONCE, 0.0f);
    this->actionFunc = EnTest_Land;
}

static void EnTest_SetupJumpUp(EnTest* this) {
    Animation_PlayOnce(&this->skelAnime, ANIM(sJumpAnim));
    this->timer = 0;
    this->state = STALFOS_STATE_JUMP_UP;
    this->actor.velocity.y = 14.0f;
    this->actor.speed = 6.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_RIZA_JUMP);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = EnTest_JumpUp;
}

static void EnTest_IdleFromBlock(EnTest* this, PlayState* play) {
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 1.0f, 1.5f, 0.0f);
    SkelAnime_Update(&this->skelAnime);
    if (this->skelAnime.morphWeight != 0.0f) {
        return;
    }
    this->actor.speed = 0.0f;
    this->shieldBlend = SHIELD_BLEND_NONE;
    if (EnTest_ReactToProjectile(this, play)) {
        return;
    }
    if (this->actor.xzDistToPlayer < 500.0f) {
        EnTest_ChooseAction(this, play);
    } else {
        EnTest_SetupSidestep(this, play);
    }
}

static void EnTest_StopAndBlock(EnTest* this, PlayState* play) {
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 1.0f, 0.5f, 0.0f);
    SkelAnime_Update(&this->skelAnime);
    if ((AbsYawDiff(this->actor.yawTowardsPlayer, this->actor.shape.rot.y) > 0x3E80) &&
        ((play->gameplayFrames % 2) != 0)) {
        this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        EnTest_SetupJumpBack(this);
        return;
    }
    if (this->timer != 0) {
        this->timer--;
        return;
    }
    Animation_MorphToLoop(&this->skelAnime, ANIM(sGuardAnim), -4.0f);
    this->state = STALFOS_STATE_UNBLOCK;
    this->actionFunc = EnTest_IdleFromBlock;
}

static void EnTest_SetupStopAndBlock(EnTest* this) {
    Animation_Change(&this->skelAnime, ANIM(sBlockAnim), 2.0f, 0.0f, LAST_FRAME(sBlockAnim), ANIMMODE_ONCE, 2.0f);
    this->state = STALFOS_STATE_BLOCK;
    this->actor.speed = 0.0f;
    this->timer = (Rand_ZeroOne() * 10.0f) + 11.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->shieldBlend = SHIELD_BLEND_FULL_BLOCK;
    this->actionFunc = EnTest_StopAndBlock;
}

static void EnTest_Flinch(EnTest* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    bool isFromFront = this->state == STALFOS_STATE_FLINCH;

    Math_SmoothStepToF(&this->actor.speed, 0.0f, 1.0f, 0.1f, 0.0f);
    if (SkelAnime_Update(&this->skelAnime)) {
        this->actor.speed = 0.0f;
        if (isFromFront && IsWallAhead(this)) {
            EnTest_SetupJumpUp(this);
        } else if (EnTest_ReactToProjectile(this, play)) {
            return;
        } else {
            EnTest_ChooseAction(this, play);
        }
    }
    if (player->meleeWeaponState != 0) {
        EnTest_ReactToPlayerSwing(this, play, true);
        this->state = STALFOS_STATE_FLINCH;
    }
}

static void EnTest_SetupFlinch(EnTest* this, bool isFromFront) {
    Animation_PlayOnce(&this->skelAnime, ANIM(isFromFront ? sFlinchFrontAnim : sFlinchBehindAnim));
    Actor_PlaySfx(&this->actor, NA_SE_EN_STAL_DAMAGE);
    this->state = isFromFront ? STALFOS_STATE_FLINCH : STALFOS_STATE_FLINCH + 1;
    this->actor.speed = -2.0f;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
    this->actionFunc = EnTest_Flinch;
}

static void EnTest_Stunned(EnTest* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Math_SmoothStepToF(&this->actor.speed, 0.0f, 1.0f, 1.0f, 0.0f);
    if (this->actor.colorFilterTimer != 0) {
        return;
    }
    if (this->actor.colChkInfo.health == 0) {
        EnTest_SetupFall(this, play, false);
    } else if (player->meleeWeaponState != 0) {
        EnTest_ReactToPlayerSwing(this, play, true);
        this->state = STALFOS_STATE_FLINCH;
    } else {
        this->actor.speed = 0.0f;
        if (!EnTest_ReactToProjectile(this, play)) {
            EnTest_ChooseAction(this, play);
        }
    }
}

static void EnTest_SetupStunned(EnTest* this) {
    this->state = STALFOS_STATE_STUNNED;
    this->shieldBlend = SHIELD_BLEND_NONE;
    this->meleeWeaponState = 0;
    this->skelAnime.playSpeed = 0.0f;
    this->actor.speed = -4.0f;
    if (this->lastDamageEffect == STALFOS_DMGEFF_LIGHT) {
        Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_GRAY, 120 | COLORFILTER_INTENSITY_FLAG,
                             COLORFILTER_BUFFLAG_OPA, 80);
    } else {
        Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 80);
        Animation_PlayOnceSetSpeed(&this->skelAnime, ANIM(sFlinchFrontAnim), 0.0f);
    }
    Actor_PlaySfx(&this->actor, NA_SE_EN_COMMON_FREEZE);
    this->actionFunc = EnTest_Stunned;
}

static void EnTest_Sidestep(EnTest* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xFA0, 1);
    if (this->shieldBlend == SHIELD_BLEND_NONE) {
        this->shieldBlend = SHIELD_BLEND_START;
    }
    if (this->actor.speed >= 0.0f) {
        this->actor.speed = MIN(this->actor.speed + 0.125f, 6.0f);
    } else {
        this->actor.speed = MAX(this->actor.speed - 0.125f, -6.0f);
    }
    EnTest_BounceOffWalls(this, this->actor.shape.rot.y);
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x3FFF;
    EnTest_KeepDistance(this, play);
    this->skelAnime.playSpeed = this->actor.speed * 0.5f;
    s32 prevFrame = this->skelAnime.curFrame;
    SkelAnime_Update(&this->skelAnime);
    if ((this->timer % 32) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_STAL_WARAU);
    }
    EnTest_PlayStepSfx(this, prevFrame, 1, 8);
    if (this->timer != 0) {
        this->timer--;
        return;
    }
    if (Actor_OtherIsTargeted(play, &this->actor)) {
        EnTest_SetupIdle(this);
    } else if (Actor_IsTargeted(play, &this->actor)) {
        if (!EnTest_ReactToProjectile(this, play)) {
            EnTest_ChooseAction(this, play);
        }
    } else if ((player->heldItemAction != PLAYER_IA_NONE) && ((play->gameplayFrames % 2) != 0)) {
        EnTest_SetupIdle(this);
    } else {
        EnTest_SetupWalkAndBlock(this);
    }
}

static void EnTest_StartSidestep(EnTest* this, f32 speed, f32 minFrames) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sSidestepAnim), -2.0f);
    this->actor.speed = speed;
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x3FFF;
    this->timer = (Rand_ZeroOne() * 20.0f) + minFrames;
    this->state = STALFOS_STATE_SIDESTEP;
    this->approachSpeed = 0.0f;
    this->actionFunc = EnTest_Sidestep;
}

static void EnTest_SetupSidestep(EnTest* this, PlayState* play) {
    if (Actor_OtherIsTargeted(play, &this->actor)) {
        EnTest_SetupStrafe(this);
        return;
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xFA0, 1);
    EnTest_StartSidestep(this, ((play->gameplayFrames % 2) != 0) ? -4.0f : 4.0f, 20.0f);
}

static void EnTest_Recoil(EnTest* this, PlayState* play) {
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (Rand_ZeroOne() > 0.7f) {
        EnTest_SetupIdle(this);
        this->timer = (Rand_ZeroOne() * 5.0f) + 5.0f;
    } else if ((play->gameplayFrames % 2) != 0) {
        EnTest_SetupJumpBack(this);
    } else {
        EnTest_SetupSidestep(this, play);
    }
}

static void EnTest_SetupRecoil(EnTest* this) {
    this->meleeWeaponState = 0;
    this->state = STALFOS_STATE_RECOIL;
    this->skelAnime.playSpeed = -1.0f;
    this->skelAnime.startFrame = this->skelAnime.curFrame;
    this->skelAnime.endFrame = 0.0f;
    this->actionFunc = EnTest_Recoil;
}

static void EnTest_ScatterBones(EnTest* this, PlayState* play) {
    Vec3f accel = { 0.0f, -1.0f, 0.0f };

    for (s32 i = 0; i < STALFOS_DEBRIS_COUNT; i++) {
        if (this->debrisDLs[i] == NULL) {
            continue;
        }
        Vec3f velocity = { Rand_CenteredFloat(8.0f), Rand_ZeroOne() * 6.0f + 4.0f, Rand_CenteredFloat(8.0f) };
        EffectSsHahen_Spawn(play, &this->debrisPos[i], &velocity, &accel, 0, 15, -1, 30, this->debrisDLs[i]);
    }
    Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, 30.0f, 7, 4.0f, 120, 15, true);
}

static void EnTest_Die(EnTest* this, PlayState* play) {
    EnTest_ScatterBones(this, play);
    EnTest_StopBgm(this);
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xD0);
    Actor_Kill(&this->actor);
}

static void EnTest_Down(EnTest* this, PlayState* play) {
    if (DECR(this->downTimer) != 0) {
        return;
    }
    if (!IsPartnerStanding(this, play)) {
        EnTest_Die(this, play);
        return;
    }
    this->actor.colChkInfo.health = STALFOS_HEALTH;
    Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_ENEMY);
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, 30.0f, 7, 4.0f, 120, 15, true);
    EnTest_SetupJumpBack(this);
}

// A pair member falls apart and waits; it only stays down if its partner falls too.
static void EnTest_Collapse(EnTest* this, PlayState* play) {
    if (!this->isPair || !IsPartnerStanding(this, play)) {
        EnTest_Die(this, play);
        return;
    }
    EnTest_ScatterBones(this, play);
    Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_PROP);
    this->downTimer = STALFOS_DOWN_FRAMES;
    this->actionFunc = EnTest_Down;
}

static void EnTest_FallOver(EnTest* this, PlayState* play) {
    s32 frame = (s32)this->skelAnime.curFrame;
    bool isForwards = this->isFallingForwards;

    if (SkelAnime_Update(&this->skelAnime)) {
        EnTest_Collapse(this, play);
        return;
    }
    if ((!isForwards && (frame == 15)) || (isForwards && ((frame == 10) || (frame == 25)))) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_GERUDOFT_DOWN);
    }
}

static void EnTest_SetupFall(EnTest* this, PlayState* play, bool isForwards) {
    Animation_PlayOnce(&this->skelAnime, ANIM(isForwards ? sFallForwardsAnim : sFallBackwardsAnim));
    this->isFallingForwards = isForwards;
    Actor_PlaySfx(&this->actor, NA_SE_EN_STAL_DEAD);
    this->state = STALFOS_STATE_FALLING;
    this->shieldBlend = SHIELD_BLEND_NONE;
    this->meleeWeaponState = 0;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.colorFilterTimer = 0;
    this->actor.speed = 0.0f;
    this->actionFunc = EnTest_FallOver;
}

static void EnTest_SidestepAway(EnTest* this, f32 speed) {
    EnTest_StartSidestep(this, speed, 15.0f);
}

static bool EnTest_ReactToProjectile(EnTest* this, PlayState* play) {
    Actor* projectile = func_800BC270(play, &this->actor, 300.0f, STALFOS_PROJECTILE_FLAGS);

    if (projectile == NULL) {
        return false;
    }
    s16 yawToProjectile = Actor_WorldYawTowardActor(&this->actor, projectile) - this->actor.shape.rot.y;
    bool isTouchingWall = (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) != 0;
    s16 wallYawDiff = isTouchingWall ? (s16)(this->actor.wallYaw - this->actor.shape.rot.y) : 0;
    bool isHookshotAimed = Actor_IsTargeted(play, &this->actor) && (projectile->id == ACTOR_ARMS_HOOK);

    if (Math_Vec3f_DistXYZ(&this->actor.world.pos, &projectile->world.pos) < 200.0f) {
        if (isHookshotAimed) {
            EnTest_SetupJumpUp(this);
        } else if (ABS(yawToProjectile) < 0x2000) {
            EnTest_SetupStopAndBlock(this);
        } else if (ABS(yawToProjectile) < 0x6000) {
            EnTest_SetupJumpBack(this);
        } else {
            EnTest_SetupJumpUp(this);
        }
        return true;
    }
    if (isHookshotAimed) {
        EnTest_SetupJumpUp(this);
        return true;
    }
    bool goesRight = (play->gameplayFrames % 2) != 0;
    if ((ABS(yawToProjectile) < 0x2000) || (ABS(yawToProjectile) > 0x6000)) {
        if (isTouchingWall && (wallYawDiff > 0x2000) && (wallYawDiff < 0x6000)) {
            goesRight = false;
        } else if (isTouchingWall && (wallYawDiff < -0x2000) && (wallYawDiff > -0x6000)) {
            goesRight = true;
        }
        EnTest_SidestepAway(this, goesRight ? 4.0f : -4.0f);
        return true;
    }
    if (isTouchingWall && (ABS(wallYawDiff) > 0x6000)) {
        goesRight = false;
    } else if (isTouchingWall && (ABS(wallYawDiff) < 0x2000)) {
        goesRight = true;
    }
    if (goesRight) {
        EnTest_SetupJumpBack(this);
    } else {
        EnTest_SetupJumpUp(this);
    }
    return true;
}

static void EnTest_UpdateHeadRot(EnTest* this) {
    s16 lookAngle = this->actor.yawTowardsPlayer - (s16)(this->headRot.y + this->actor.shape.rot.y);

    this->headRot.y += CLAMP(lookAngle, -0x7D0, 0x7D0);
    this->headRot.y = CLAMP(this->headRot.y, -0x382F, 0x382F);
}

static void EnTest_UpdateDamage(EnTest* this, PlayState* play) {
    if (this->shieldCollider.base.acFlags & AC_BOUNCED) {
        this->shieldCollider.base.acFlags &= ~AC_BOUNCED;
        this->bodyCollider.base.acFlags &= ~AC_HIT;
        if (this->state >= STALFOS_STATE_VULNERABLE) {
            this->actor.speed = -4.0f;
        }
        return;
    }
    if (!(this->bodyCollider.base.acFlags & AC_HIT)) {
        return;
    }
    this->bodyCollider.base.acFlags &= ~AC_HIT;
    u8 effect = this->actor.colChkInfo.damageEffect;
    if (effect == STALFOS_DMGEFF_IGNORE) {
        return;
    }
    this->lastDamageEffect = effect;
    if (this->meleeWeaponState >= 1) {
        this->meleeWeaponState = 0;
    }
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    Actor_SetDropFlag(&this->actor, &this->bodyCollider.elem);
    AudioSfx_StopByPosAndId(&this->actor.projectedPos, NA_SE_EN_STAL_WARAU);
    if ((effect == STALFOS_DMGEFF_STUN) || (effect == STALFOS_DMGEFF_FREEZE) || (effect == STALFOS_DMGEFF_LIGHT)) {
        if (this->state != STALFOS_STATE_STUNNED) {
            Actor_ApplyDamage(&this->actor);
            EnTest_SetupStunned(this);
        }
        return;
    }
    bool isFromFront = Actor_IsFacingPlayer(&this->actor, 0x4000);
    if (Actor_ApplyDamage(&this->actor) != 0) {
        EnTest_SetupFlinch(this, isFromFront);
        return;
    }
    Enemy_StartFinishingBlow(play, &this->actor);
    EnTest_SetupFall(this, play, !isFromFront);
}

static void EnTest_InterpolateUpperBody(EnTest* this, f32 weight) {
    for (s32 i = 0; i < this->skelAnime.limbCount; i++) {
        Vec3s* upper = &this->upperJointTable[i];
        Vec3s* lower = &this->jointTable[i];

        upper->x += (s16)((s16)(lower->x - upper->x) * weight);
        upper->y += (s16)((s16)(lower->y - upper->y) * weight);
        upper->z += (s16)((s16)(lower->z - upper->z) * weight);
    }
}

static void EnTest_UpdateShieldBlend(EnTest* this, PlayState* play) {
    switch (this->shieldBlend) {
        case SHIELD_BLEND_START:
            Animation_Change(&this->upperSkelAnime, ANIM(sBlockAnim), 2.0f, 0.0f, LAST_FRAME(sBlockAnim), ANIMMODE_ONCE,
                             2.0f);
            AnimTaskQueue_AddCopyUsingMap(play, this->skelAnime.limbCount, this->skelAnime.jointTable,
                                          this->upperSkelAnime.jointTable, sUpperBodyCopyFlags);
            this->shieldBlend = SHIELD_BLEND_HOLD;
            break;
        case SHIELD_BLEND_HOLD:
            SkelAnime_Update(&this->upperSkelAnime);
            SkelAnime_CopyFrameTableTrue(&this->skelAnime, this->skelAnime.jointTable, this->upperSkelAnime.jointTable,
                                         sUpperBodyCopyFlags);
            break;
        case SHIELD_BLEND_RELEASE:
        case SHIELD_BLEND_FADING: {
            if (this->shieldBlend == SHIELD_BLEND_RELEASE) {
                this->shieldBlend = SHIELD_BLEND_FADING;
                this->upperSkelAnime.morphWeight = 4.0f;
            }
            f32 oldWeight = this->upperSkelAnime.morphWeight;
            this->upperSkelAnime.morphWeight -= 1.0f;
            if (this->upperSkelAnime.morphWeight <= 0.0f) {
                this->shieldBlend = SHIELD_BLEND_NONE;
            }
            EnTest_InterpolateUpperBody(this, 1.0f - (this->upperSkelAnime.morphWeight / oldWeight));
            SkelAnime_CopyFrameTableTrue(&this->skelAnime, this->skelAnime.jointTable, this->upperSkelAnime.jointTable,
                                         sUpperBodyCopyFlags);
            break;
        }
        default:
            break;
    }
}

static bool EnTest_IsOverVoid(EnTest* this, PlayState* play) {
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH)) {
        return false;
    }
    FloorProperty property = SurfaceType_GetFloorProperty(&play->colCtx, this->actor.floorPoly, this->actor.floorBgId);
    return (property == FLOOR_PROPERTY_5) || (property == FLOOR_PROPERTY_12);
}

static void EnTest_SpawnPartner(EnTest* this, PlayState* play) {
    Vec3f spot = this->actor.home.pos;
    s16 yaw = this->actor.home.rot.y + 0x4000;

    spot.x += Math_SinS(yaw) * 120.0f;
    spot.z += Math_CosS(yaw) * 120.0f;
    Actor_Spawn(&play->actorCtx, play, sPairId, spot.x, spot.y, spot.z, 0, this->actor.home.rot.y, 0, 1);
}

static void EnTest_Init(Actor* thisx, PlayState* play) {
    EnTest* this = (EnTest*)thisx;
    EffectBlureInit1 slashBlure;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sSkel, ANIM(sGuardAnim), this->jointTable, this->morphTable,
                   STALFOS_LIMB_MAX);
    SkelAnime_Init(play, &this->upperSkelAnime, (SkeletonHeader*)sSkel, ANIM(sGuardAnim), this->upperJointTable,
                   this->upperMorphTable, STALFOS_LIMB_MAX);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawFeet, 90.0f);
    this->actor.colChkInfo.cylRadius = 40;
    this->actor.colChkInfo.cylHeight = 100;
    this->actor.focus.pos = this->actor.world.pos;
    this->actor.focus.pos.y += 45.0f;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyColliderInit);
    Collider_InitCylinder(play, &this->shieldCollider);
    Collider_SetCylinder(play, &this->shieldCollider, &this->actor, &sShieldColliderInit);
    Collider_InitQuad(play, &this->swordCollider);
    Collider_SetQuad(play, &this->swordCollider, &this->actor, &sSwordColliderInit);
    this->actor.colChkInfo.mass = MASS_HEAVY;
    this->actor.colChkInfo.health = STALFOS_HEALTH;
    memset(&slashBlure, 0, sizeof(slashBlure));
    for (s32 i = 0; i < 4; i++) {
        slashBlure.p1StartColor[i] = 255;
        slashBlure.p2StartColor[i] = 255;
        slashBlure.p1EndColor[i] = 255;
        slashBlure.p2EndColor[i] = 255;
    }
    slashBlure.p1EndColor[3] = 0;
    slashBlure.p2EndColor[3] = 0;
    slashBlure.p2StartColor[3] = 64;
    slashBlure.elemDuration = 4;
    slashBlure.unkFlag = 0;
    slashBlure.calcMode = 2;
    Effect_Add(play, &this->effectIndex, EFFECT_BLURE1, 0, 0, &slashBlure);
    this->isPair = this->actor.id == sPairId;
    this->hasPartner = this->actor.params != 0;
    EnTest_SetupWaitGround(this);
}

static void EnTest_Destroy(Actor* thisx, PlayState* play) {
    EnTest* this = (EnTest*)thisx;

    EnTest_StopBgm(this);
    Effect_Destroy(play, this->effectIndex);
    Collider_DestroyCylinder(play, &this->shieldCollider);
    Collider_DestroyCylinder(play, &this->bodyCollider);
    Collider_DestroyQuad(play, &this->swordCollider);
}

static void EnTest_UpdateCollision(EnTest* this, PlayState* play) {
    CollisionCheckContext* colChkCtx = &play->colChkCtx;
    bool isRedFlashing = (this->actor.colorFilterTimer != 0) && (this->actor.colorFilterParams & 0x4000);

    if ((this->actor.colChkInfo.health > 0) || (this->actor.colorFilterTimer != 0)) {
        CollisionCheck_SetOC(play, colChkCtx, &this->bodyCollider.base);
        if ((this->state >= STALFOS_STATE_VULNERABLE) && !isRedFlashing) {
            CollisionCheck_SetAC(play, colChkCtx, &this->bodyCollider.base);
        }
        if (this->shieldBlend != SHIELD_BLEND_NONE) {
            CollisionCheck_SetAC(play, colChkCtx, &this->shieldCollider.base);
        }
    }
    if (this->meleeWeaponState < 1) {
        return;
    }
    if (this->swordCollider.base.atFlags & AT_BOUNCED) {
        this->swordCollider.base.atFlags &= ~AT_BOUNCED;
        EnTest_SetupRecoil(this);
        return;
    }
    CollisionCheck_SetAT(play, colChkCtx, &this->swordCollider.base);
}

static void EnTest_Update(Actor* thisx, PlayState* play) {
    EnTest* this = (EnTest*)thisx;

    // Spawned here, not in Init: a pair removed right after Init (missing soul) must not leave its partner behind.
    if (this->isPair && !this->hasPartner) {
        this->hasPartner = true;
        EnTest_SpawnPartner(this, play);
    }
    if (this->actionFunc == EnTest_Down) {
        EnTest_Down(this, play);
        return;
    }
    EnTest_UpdateDamage(this, play);
    Actor_MoveWithGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 75.0f, 30.0f, 30.0f,
                            UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_8 |
                                UPDBGCHECKINFO_FLAG_10);
    if (EnTest_IsOverVoid(this, play)) {
        EnTest_StopBgm(this);
        Actor_Kill(&this->actor);
        return;
    }
    this->actionFunc(this, play);
    EnTest_UpdateShieldBlend(this, play);
    if ((this->actor.colorFilterTimer == 0) && (this->actor.colChkInfo.health != 0)) {
        if ((this->state != STALFOS_STATE_SLASH_DOWN) && (this->state != STALFOS_STATE_JUMPSLASH)) {
            EnTest_UpdateHeadRot(this);
        } else {
            Math_SmoothStepToS(&this->headRot.y, 0, 1, 0x3E8, 0);
        }
    }
    Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
    this->actor.focus.pos = this->actor.world.pos;
    this->actor.focus.pos.y += 45.0f;
    EnTest_UpdateCollision(this, play);
}

static s32 EnTest_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    EnTest* this = (EnTest*)thisx;

    if (limbIndex == STALFOS_LIMB_HEAD_ROOT) {
        rot->x += this->headRot.y;
        rot->y -= this->headRot.x;
        rot->z += this->headRot.z;
    } else if (limbIndex == STALFOS_LIMB_HEAD) {
        OPEN_DISPS(play->state.gfxCtx);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, 80 + ABS((s16)(Math_SinS(play->gameplayFrames * 2000) * 175.0f)), 0, 0, 255);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    return false;
}

static void EnTest_UpdateSword(EnTest* this, PlayState* play) {
    static Vec3f sQuadTip = { 7000.0f, 1000.0f, 0.0f };
    static Vec3f sQuadBaseLeft = { 3000.0f, -2000.0f, -1000.0f };
    static Vec3f sQuadBaseRight = { 3000.0f, -2000.0f, 1000.0f };
    static Vec3f sQuadHilt = { -1300.0f, 1100.0f, 0.0f };
    static Vec3f sTrailTip = { 3400.0f, 0.0f, 0.0f };
    static Vec3f sOrigin = { 0.0f, 0.0f, 0.0f };
    ColliderQuadDim* quad = &this->swordCollider.dim;
    Vec3f tip;
    Vec3f base;

    Matrix_MultVec3f(&sQuadTip, &quad->quad[1]);
    Matrix_MultVec3f(&sQuadBaseLeft, &quad->quad[0]);
    Matrix_MultVec3f(&sQuadBaseRight, &quad->quad[3]);
    Matrix_MultVec3f(&sQuadHilt, &quad->quad[2]);
    Collider_SetQuadVertices(&this->swordCollider, &quad->quad[0], &quad->quad[1], &quad->quad[2], &quad->quad[3]);
    Matrix_MultVec3f(&sTrailTip, &tip);
    Matrix_MultVec3f(&sOrigin, &base);
    if (this->meleeWeaponState >= 1) {
        EffectBlure_AddVertex(Effect_GetByIndex(this->effectIndex), &tip, &base);
    } else if (this->meleeWeaponState >= 0) {
        EffectBlure_AddSpace(Effect_GetByIndex(this->effectIndex));
        this->meleeWeaponState = -1;
    }
}

static void EnTest_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    static Vec3f sFootOffset = { 300.0f, 0.0f, 0.0f };
    static Vec3f sOrigin = { 0.0f, 0.0f, 0.0f };
    EnTest* this = (EnTest*)thisx;

    for (s32 i = 0; i < STALFOS_DEBRIS_COUNT; i++) {
        if (sDebrisLimbs[i] == limbIndex) {
            this->debrisDLs[i] = *dList;
            Matrix_MultVec3f(&sOrigin, &this->debrisPos[i]);
        }
    }
    if (limbIndex == STALFOS_LIMB_SWORD) {
        EnTest_UpdateSword(this, play);
        return;
    }
    if (limbIndex == STALFOS_LIMB_SHIELD) {
        if (this->shieldBlend != SHIELD_BLEND_NONE) {
            Matrix_MultVec3f(&sOrigin, &this->shieldCollider.dim.pos);
        }
        return;
    }
    Actor_SetFeetPos(&this->actor, limbIndex, STALFOS_LIMB_FOOT_L, &sFootOffset, STALFOS_LIMB_ANKLE_R, &sFootOffset);
    bool isFoot = (limbIndex == STALFOS_LIMB_FOOT_L) || (limbIndex == STALFOS_LIMB_ANKLE_R);
    bool isSkidding = (this->state == STALFOS_STATE_BLOCK) || (this->state == STALFOS_STATE_UNBLOCK);
    if (isFoot && isSkidding && (this->actor.speed != 0.0f)) {
        Vec3f footPos;

        Matrix_MultVec3f(&sFootOffset, &footPos);
        Actor_SpawnFloorDustRing(play, &this->actor, &footPos, 10.0f, 1, 8.0f, 100, 15, false);
    }
}

static void EnTest_Draw(Actor* thisx, PlayState* play) {
    EnTest* this = (EnTest*)thisx;

    if (this->actionFunc == EnTest_Down) {
        return;
    }
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    func_800B8050(&this->actor, play, 1);
    SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, EnTest_OverrideLimbDraw,
                      EnTest_PostLimbDraw, &this->actor);
}

// Ship's close-range rule: a sword or hammer (Goron here), the bow, or bombs once nuts or the hookshot stun it.
static bool CanKillStalfos(void* userData) {
    bool canStun = sApi->LogicCanUseItem(RI_DEKU_NUT) || sApi->LogicHasCapability("HookshotOrBoomerang");

    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicHasCapability("CanBeGoron") ||
           sApi->LogicCanUseItem(RI_BOW) || (sApi->LogicHasCapability("HasExplosives") && canStun);
}

static s16 RegisterStalfos(const char* key, const char* description, u32 enemyFlags) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = key;
    definition.description = description;
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = STALFOS_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnTest);
    definition.init = EnTest_Init;
    definition.destroy = EnTest_Destroy;
    definition.update = EnTest_Update;
    definition.draw = EnTest_Draw;
    definition.enemyFlags = enemyFlags;
    definition.naviHint = "Stalfos&A skeletal swordsman. Watch its shield,&then strike when it lowers its guard!";
    // The host finds the soul item as "soul." + soulKey, so both rows share the family key.
    definition.soulKey = STALFOS_KEY;
    definition.logicKill = CanKillStalfos;
    definition.enemyCategory = S2H_ENEMY_CATEGORY_REGULAR;
    definition.soulModel.skeleton = sSkel;
    definition.soulModel.idleAnimation = sGuardAnim;
    definition.soulModel.limbCount = STALFOS_LIMB_MAX;
    definition.soulModel.scale = 0.01f;
    definition.soulModel.flameRed = 230;
    definition.soulModel.flameGreen = 220;
    definition.soulModel.flameBlue = 180;
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
    if (!RequireOotAsset(SK_OBJECT "gStalfosSkel")) {
        sApi->Log(STALFOS_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    sStalfosId = RegisterStalfos(STALFOS_KEY, "Stalfos", S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_NOT_IN_WATER);
    sPairId = RegisterStalfos(STALFOS_PAIR_KEY, "Stalfos pair",
                              S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_MINIBOSS | S2H_ACTOR_ENEMY_NOT_IN_WATER |
                                  S2H_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS);
    if ((sStalfosId < 0) || (sPairId < 0)) {
        sApi->Log("Could not register " STALFOS_KEY);
    }
}
