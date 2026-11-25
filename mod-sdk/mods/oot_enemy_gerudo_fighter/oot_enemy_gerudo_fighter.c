/*
 * Ocarina of Time's Gerudo Fighter (En_GeldB) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * She waits unseen above the floor, drops in when the player comes near and flees once beaten; no jail.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define GERUDO_FIGHTER_KEY "oot.gerudo_fighter"
#define GERUDO_FIGHTER_SOUL_KEY "soul.oot.gerudo_fighter"
#define GELDB_OBJECT "objects/oot_object_geldb/"
#define GELDB_ASSET(name) "__OTR__" GELDB_OBJECT name

static const ALIGN_ASSET(2) char sSkel[] = GELDB_ASSET("gGerudoRedSkel");
static const ALIGN_ASSET(2) char sJumpAnim[] = GELDB_ASSET("gGerudoRedJumpAnim");
static const ALIGN_ASSET(2) char sNeutralAnim[] = GELDB_ASSET("gGerudoRedNeutralAnim");
static const ALIGN_ASSET(2) char sSidestepAnim[] = GELDB_ASSET("gGerudoRedSidestepAnim");
static const ALIGN_ASSET(2) char sFlipAnim[] = GELDB_ASSET("gGerudoRedFlipAnim");
static const ALIGN_ASSET(2) char sWalkAnim[] = GELDB_ASSET("gGerudoRedWalkAnim");
static const ALIGN_ASSET(2) char sSlashAnim[] = GELDB_ASSET("gGerudoRedSlashAnim");
static const ALIGN_ASSET(2) char sSpinAttackAnim[] = GELDB_ASSET("gGerudoRedSpinAttackAnim");
static const ALIGN_ASSET(2) char sDamageAnim[] = GELDB_ASSET("gGerudoRedDamageAnim");
static const ALIGN_ASSET(2) char sDefeatAnim[] = GELDB_ASSET("gGerudoRedDefeatAnim");
static const ALIGN_ASSET(2) char sBlockAnim[] = GELDB_ASSET("gGerudoRedBlockAnim");
static const ALIGN_ASSET(2) char sEyeOpenTex[] = GELDB_ASSET("gGerudoRedEyeOpenTex");
static const ALIGN_ASSET(2) char sEyeHalfTex[] = GELDB_ASSET("gGerudoRedEyeHalfTex");
static const ALIGN_ASSET(2) char sEyeShutTex[] = GELDB_ASSET("gGerudoRedEyeShutTex");

static const char* const sEyeTextures[] = { sEyeOpenTex, sEyeHalfTex, sEyeShutTex, sEyeHalfTex };

#define ANIM(asset) ((AnimationHeader*)(asset))
#define LAST_FRAME(asset) Animation_GetLastFrame((void*)(asset))

#define GELDB_LIMB_TORSO 0x02
#define GELDB_LIMB_NECK 0x03
#define GELDB_LIMB_HEAD 0x06
#define GELDB_LIMB_R_UPPER_ARM 0x07
#define GELDB_LIMB_R_SWORD 0x0B
#define GELDB_LIMB_L_UPPER_ARM 0x0C
#define GELDB_LIMB_L_SWORD 0x10
#define GELDB_LIMB_L_FOOT 0x13
#define GELDB_LIMB_R_FOOT 0x16
#define GELDB_LIMB_WAIST 0x17
#define GELDB_LIMB_MAX 0x18

// Vanilla's numbered actions; damage is only taken from READY on.
typedef enum {
    GELDB_ACTION_WAIT = 0,
    GELDB_ACTION_DEFEAT = 1,
    GELDB_ACTION_DAMAGED = 2,
    GELDB_ACTION_JUMP = 3,
    GELDB_ACTION_ROLL_BACK = 4,
    GELDB_ACTION_READY = 5,
    GELDB_ACTION_BLOCK = 6,
    GELDB_ACTION_SLASH = 7,
    GELDB_ACTION_ADVANCE = 8,
    GELDB_ACTION_PIVOT = 9,
    GELDB_ACTION_CIRCLE = 10,
    GELDB_ACTION_SPIN_ATTACK = 12,
    GELDB_ACTION_SIDESTEP = 13,
    GELDB_ACTION_ROLL_FORWARD = 14,
    GELDB_ACTION_STUNNED = 15,
    GELDB_ACTION_SPIN_DODGE = 16,
} GerudoFighterAction;

typedef enum {
    GELDB_DMGEFF_NONE = 0x0,
    GELDB_DMGEFF_STUN = 0x1,
    GELDB_DMGEFF_FREEZE = 0xF,
} GerudoFighterDamageEffect;

#define GERUDO_FIGHTER_HEALTH 10
#define GERUDO_FIGHTER_DROP_HEIGHT 120.0f
#define GERUDO_FIGHTER_REVEAL_RANGE 300.0f
#define GERUDO_FIGHTER_STUN_FRAMES 80
#define GERUDO_FIGHTER_THAW_FRAMES 36
#define GERUDO_FIGHTER_BODYPART_COUNT 9
#define GERUDO_FIGHTER_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED)
#define GERUDO_FIGHTER_PROJECTILE_FLAGS \
    (DMG_HOOKSHOT | DMG_NORMAL_ARROW | DMG_FIRE_ARROW | DMG_ICE_ARROW | DMG_LIGHT_ARROW | DMG_DEKU_BUBBLE)
#define GERUDO_FIGHTER_BGCHECK_FLAGS \
    (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_8 | UPDBGCHECKINFO_FLAG_10)

static const S2HModApi* sApi;
static s32 sBgmUsers;

struct EnGeldB;
typedef void (*EnGeldBActionFunc)(struct EnGeldB*, PlayState*);

typedef struct EnGeldB {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[GELDB_LIMB_MAX];
    Vec3s morphTable[GELDB_LIMB_MAX];
    EnGeldBActionFunc actionFunc;
    ColliderCylinder bodyCollider;
    ColliderQuad swordCollider;
    ColliderTris blockCollider;
    ColliderTrisElement blockElements[2];
    Vec3f rightFootPos;
    Vec3f leftFootPos;
    Vec3f bodyPartsPos[GERUDO_FIGHTER_BODYPART_COUNT];
    f32 approachRate;
    f32 drawDmgEffAlpha;
    s32 action;
    s32 timer;
    s32 blureIndex;
    s16 lookTimer;
    s16 headRot;
    s16 meleeWeaponState;
    u8 damageEffect;
    u8 blinkState;
    u8 isHidden;
    u8 wasSpinBlocked;
    u8 isBgmUser;
} EnGeldB;

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
        ELEM_MATERIAL_UNK1,
        { 0x00000000, 0x00, 0x00 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_NONE,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 20, 50, 0, { 0, 0, 0 } },
};

static ColliderTrisElementInit sBlockElementsInit[2] = {
    {
        {
            ELEM_MATERIAL_UNK2,
            { 0x00000000, 0x00, 0x00 },
            { 0xF7CFFFFF, 0x00, 0x00 },
            ATELEM_NONE,
            ACELEM_ON,
            OCELEM_NONE,
        },
        { { { -10.0f, 14.0f, 2.0f }, { -10.0f, -6.0f, 2.0f }, { 9.0f, 14.0f, 2.0f } } },
    },
    {
        {
            ELEM_MATERIAL_UNK2,
            { 0x00000000, 0x00, 0x00 },
            { 0xF7CFFFFF, 0x00, 0x00 },
            ATELEM_NONE,
            ACELEM_ON,
            OCELEM_NONE,
        },
        { { { -10.0f, -6.0f, 2.0f }, { 9.0f, -6.0f, 2.0f }, { 9.0f, 14.0f, 2.0f } } },
    },
};

static ColliderTrisInit sBlockColliderInit = {
    {
        COL_MATERIAL_METAL,
        AT_NONE,
        AC_ON | AC_HARD | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_TRIS,
    },
    2,
    sBlockElementsInit,
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
        { 0xF7CFFFFF, 0x00, 0x08 },
        { 0x00000000, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL | ATELEM_UNK7,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static DamageTable sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, GELDB_DMGEFF_STUN),
    /* Deku Stick     */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* Horse trample  */ DMG_ENTRY(0, GELDB_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* Zora boomerang */ DMG_ENTRY(0, GELDB_DMGEFF_STUN),
    /* Normal arrow   */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, GELDB_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(0, GELDB_DMGEFF_STUN),
    /* Goron punch    */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* Sword          */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* Goron pound    */ DMG_ENTRY(2, GELDB_DMGEFF_NONE),
    /* Fire arrow     */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* Ice arrow      */ DMG_ENTRY(1, GELDB_DMGEFF_FREEZE),
    /* Light arrow    */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* Goron spikes   */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* Deku spin      */ DMG_ENTRY(0, GELDB_DMGEFF_STUN),
    /* Deku bubble    */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* Deku launch    */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, GELDB_DMGEFF_NONE),
    /* Zora barrier   */ DMG_ENTRY(0, GELDB_DMGEFF_STUN),
    /* Normal shield  */ DMG_ENTRY(0, GELDB_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, GELDB_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* Zora punch     */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* Spin attack    */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* Sword beam     */ DMG_ENTRY(1, GELDB_DMGEFF_NONE),
    /* Normal Roll    */ DMG_ENTRY(0, GELDB_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, GELDB_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, GELDB_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, GELDB_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, GELDB_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(2, GELDB_DMGEFF_NONE),
};

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(lockOnArrowOffset, 2000, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(gravity, -3000, ICHAIN_STOP),
};

static void EnGeldB_SetupReady(EnGeldB* this);
static void EnGeldB_SetupAdvance(EnGeldB* this);
static void EnGeldB_SetupPivot(EnGeldB* this);
static void EnGeldB_SetupRollForward(EnGeldB* this);
static void EnGeldB_SetupCircle(EnGeldB* this);
static void EnGeldB_SetupSpinDodge(EnGeldB* this, PlayState* play);
static void EnGeldB_SetupSlash(EnGeldB* this);
static void EnGeldB_SetupSpinAttack(EnGeldB* this);
static void EnGeldB_SetupRollBack(EnGeldB* this);
static void EnGeldB_SetupJump(EnGeldB* this);
static void EnGeldB_SetupBlock(EnGeldB* this);
static void EnGeldB_SetupSidestep(EnGeldB* this, PlayState* play);
static void EnGeldB_SetupDefeated(EnGeldB* this);
static bool EnGeldB_DodgeRanged(EnGeldB* this, PlayState* play);

static s16 AbsYawDiff(s16 a, s16 b) {
    s16 diff = a - b;

    return ABS(diff);
}

static bool IsOnGround(EnGeldB* this) {
    return (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static bool IsTouchingWall(EnGeldB* this) {
    return (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) != 0;
}

static void EnGeldB_FacePlayer(EnGeldB* this) {
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
}

static s16 EnGeldB_GetPlayerFacingDiff(EnGeldB* this, PlayState* play) {
    return AbsYawDiff(GET_PLAYER(play)->actor.shape.rot.y, this->actor.shape.rot.y);
}

static bool EnGeldB_CanAttack(EnGeldB* this, PlayState* play) {
    return !Actor_OtherIsTargeted(play, &this->actor);
}

static bool EnGeldB_IsPlayerJumpslashing(PlayState* play) {
    return GET_PLAYER(play)->meleeWeaponAnimation == PLAYER_MWA_JUMPSLASH_START;
}

static void EnGeldB_PlayBreath(EnGeldB* this, PlayState* play) {
    if ((play->gameplayFrames & 0x5F) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_PIRATE_BREATH);
    }
}

static void EnGeldB_PlayStepSfx(EnGeldB* this, s32 prevFrame, s32 secondStep) {
    s32 playSpeed = ABS(this->skelAnime.playSpeed);
    s32 afterStep = this->skelAnime.curFrame - ABS(this->skelAnime.playSpeed);
    s32 reach = playSpeed + prevFrame;

    if (prevFrame == (s32)this->skelAnime.curFrame) {
        return;
    }
    if (((afterStep < 0) && (reach > 0)) || ((afterStep < secondStep) && (reach > secondStep))) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_GERUDOFT_WALK);
    }
}

static void EnGeldB_StartBgm(EnGeldB* this) {
    if (this->isBgmUser) {
        return;
    }
    if (sBgmUsers == 0) {
        Audio_PlayBgm_StorePrevBgm(NA_BGM_MINI_BOSS);
    }
    sBgmUsers++;
    this->isBgmUser = true;
}

static void EnGeldB_StopBgm(EnGeldB* this) {
    if (!this->isBgmUser) {
        return;
    }
    this->isBgmUser = false;
    sBgmUsers--;
    if (sBgmUsers == 0) {
        Audio_RestorePrevBgm();
    }
}

static void EnGeldB_Thaw(EnGeldB* this, PlayState* play) {
    if (this->drawDmgEffAlpha == 0.0f) {
        return;
    }
    this->drawDmgEffAlpha = 0.0f;
    Actor_SpawnIceEffects(play, &this->actor, this->bodyPartsPos, GERUDO_FIGHTER_BODYPART_COUNT, 2, 0.3f, 0.2f);
}

static void EnGeldB_SpawnFeetDust(EnGeldB* this, PlayState* play) {
    Actor_SpawnFloorDustRing(play, &this->actor, &this->leftFootPos, 3.0f, 2, 2.0f, 0, 0, false);
    Actor_SpawnFloorDustRing(play, &this->actor, &this->rightFootPos, 3.0f, 2, 2.0f, 0, 0, false);
}

static void EnGeldB_SpawnTakeoffSparkles(PlayState* play, Vec3f* pos) {
    static Vec3f sVelocity = { 0.0f, -1.5f, 0.0f };
    static Vec3f sAccel = { 0.0f, -0.2f, 0.0f };
    Color_RGBA8 primColor = { 200, 160, 120, 255 };
    Color_RGBA8 envColor = { 130, 90, 50, 255 };

    EffectSsKirakira_SpawnSmall(play, pos, &sVelocity, &sAccel, &primColor, &envColor);
}

// Side effect: a floor edge without a wall reverses the strafe speed.
static bool EnGeldB_IsSideBlocked(EnGeldB* this, PlayState* play) {
    if (!IsTouchingWall(this) &&
        Actor_TestFloorInDirection(&this->actor, play, this->actor.speed, this->actor.shape.rot.y + 0x3E80)) {
        return false;
    }
    if (!IsTouchingWall(this)) {
        this->actor.speed *= -0.8f;
        return false;
    }
    s16 moveYaw =
        (this->actor.speed >= 0.0f) ? (s16)(this->actor.shape.rot.y + 0x3E80) : (s16)(this->actor.shape.rot.y - 0x3E80);
    return AbsYawDiff(this->actor.wallYaw, moveYaw) > 0x4000;
}

static void EnGeldB_BounceSideways(EnGeldB* this) {
    this->actor.speed *= -0.8f;
    this->actor.speed += (this->actor.speed < 0.0f) ? -0.5f : 0.5f;
}

static void EnGeldB_StrafeTowardPlayer(EnGeldB* this, s16 approachYaw) {
    Math_SmoothStepToF(&this->approachRate, (this->actor.xzDistToPlayer <= 45.0f) ? -4.0f : 4.0f, 1.0f, 1.5f, 0.0f);
    if (this->approachRate != 0.0f) {
        this->actor.world.pos.x += Math_SinS(approachYaw) * this->approachRate;
        this->actor.world.pos.z += Math_CosS(approachYaw) * this->approachRate;
    }
    f32 drive = (ABS(this->approachRate) < ABS(this->actor.speed)) ? this->actor.speed : this->approachRate;
    this->skelAnime.playSpeed = CLAMP(-drive * 0.5f, -3.0f, 3.0f);
}

static void EnGeldB_UpdateStrafeAnim(EnGeldB* this, PlayState* play) {
    s32 prevFrame = this->skelAnime.curFrame;

    SkelAnime_Update(&this->skelAnime);
    EnGeldB_PlayStepSfx(this, prevFrame, 5);
    EnGeldB_PlayBreath(this, play);
}

static bool EnGeldB_ReactToExplosive(EnGeldB* this, PlayState* play, bool isWallInFront) {
    Actor* bomb = Actor_FindNearby(play, &this->actor, -1, ACTORCAT_EXPLOSIVES, 80.0f);

    if (bomb == NULL) {
        return false;
    }
    EnGeldB_FacePlayer(this);
    if (!isWallInFront && (bomb->id != ACTOR_EN_BOM_CHU)) {
        EnGeldB_SetupRollBack(this);
        return true;
    }
    bool isChuCharging = (bomb->id == ACTOR_EN_BOM_CHU) && (Actor_WorldDistXYZToActor(&this->actor, bomb) < 80.0f) &&
                         ((s16)(this->actor.shape.rot.y - (bomb->world.rot.y - 0x8000)) < 0x3E80);
    if (isChuCharging) {
        EnGeldB_SetupJump(this);
    } else {
        EnGeldB_SetupSidestep(this, play);
    }
    return true;
}

static void EnGeldB_ChooseCloseAction(EnGeldB* this, PlayState* play) {
    if (AbsYawDiff(this->actor.yawTowardsPlayer, this->actor.shape.rot.y) >= 0x1B58) {
        EnGeldB_SetupSidestep(this, play);
        return;
    }
    bool isPlayerOpen = ((play->gameplayFrames & 7) != 0) || (EnGeldB_GetPlayerFacingDiff(this, play) < 0x38E0);
    if ((this->actor.xzDistToPlayer <= 45.0f) && EnGeldB_CanAttack(this, play) && isPlayerOpen) {
        EnGeldB_SetupSlash(this);
    } else {
        EnGeldB_SetupCircle(this);
    }
}

static bool EnGeldB_ReactToPlayer(EnGeldB* this, PlayState* play, bool mustAct) {
    bool isWallInFront = IsTouchingWall(this) && (AbsYawDiff(this->actor.wallYaw, this->actor.shape.rot.y) < 0x2EE0);

    if (func_800BE184(play, &this->actor, 100.0f, 0x2710, 0x3E80, this->actor.shape.rot.y)) {
        if (EnGeldB_IsPlayerJumpslashing(play)) {
            EnGeldB_SetupSpinDodge(this, play);
            return true;
        }
        if (play->gameplayFrames & 1) {
            EnGeldB_SetupBlock(this);
            return true;
        }
    }
    if (func_800BE184(play, &this->actor, 100.0f, 0x5DC0, 0x2AA8, this->actor.shape.rot.y)) {
        EnGeldB_FacePlayer(this);
        if (isWallInFront && (this->actor.xzDistToPlayer < 90.0f)) {
            EnGeldB_SetupJump(this);
        } else if (EnGeldB_IsPlayerJumpslashing(play)) {
            EnGeldB_SetupSpinDodge(this, play);
        } else if ((this->actor.xzDistToPlayer < 90.0f) && (play->gameplayFrames & 1)) {
            EnGeldB_SetupBlock(this);
        } else {
            EnGeldB_SetupRollBack(this);
        }
        return true;
    }
    if (EnGeldB_ReactToExplosive(this, play, isWallInFront)) {
        return true;
    }
    if (!mustAct) {
        return false;
    }
    EnGeldB_ChooseCloseAction(this, play);
    return true;
}

static void EnGeldB_Wait(EnGeldB* this, PlayState* play) {
    if (this->isHidden && (this->actor.xzDistToPlayer > GERUDO_FIGHTER_REVEAL_RANGE)) {
        EnGeldB_FacePlayer(this);
        this->actor.world.pos.y = this->actor.floorHeight + GERUDO_FIGHTER_DROP_HEIGHT;
    } else if (this->isHidden) {
        this->isHidden = false;
        this->actor.shape.shadowScale = 90.0f;
        EnGeldB_StartBgm(this);
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_GERUDOFT_DOWN);
        this->skelAnime.playSpeed = 1.0f;
        this->actor.world.pos.y = this->actor.floorHeight;
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        this->actor.focus.pos = this->actor.world.pos;
        this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND_TOUCH;
        this->actor.velocity.y = 0.0f;
        EnGeldB_SpawnFeetDust(this, play);
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        EnGeldB_SetupReady(this);
    }
}

static void EnGeldB_SetupWait(EnGeldB* this) {
    Animation_PlayOnceSetSpeed(&this->skelAnime, ANIM(sJumpAnim), 0.0f);
    this->actor.world.pos.y = this->actor.home.pos.y + GERUDO_FIGHTER_DROP_HEIGHT;
    this->isHidden = true;
    this->action = GELDB_ACTION_WAIT;
    this->actor.bgCheckFlags &= ~(BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH);
    this->actor.gravity = -2.0f;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actionFunc = EnGeldB_Wait;
}

static void EnGeldB_Flee(EnGeldB* this, PlayState* play) {
    if (this->skelAnime.curFrame == 10.0f) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_TEKU_JUMP);
    }
    if (this->skelAnime.curFrame == 2.0f) {
        this->actor.gravity = 0.0f;
        EnGeldB_SpawnFeetDust(this, play);
    }
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    Math_SmoothStepToF(&this->actor.world.pos.y, this->actor.floorHeight + 300.0f, 1.0f, 20.5f, 0.0f);
    this->timer--;
    if (this->timer == 0) {
        Actor_Kill(&this->actor);
    }
}

static void EnGeldB_SetupFlee(EnGeldB* this) {
    Animation_Change(&this->skelAnime, ANIM(sJumpAnim), -2.0f, LAST_FRAME(sJumpAnim), 0.0f, ANIMMODE_ONCE_INTERP,
                     -4.0f);
    this->timer = 20;
    this->isHidden = false;
    this->action = GELDB_ACTION_WAIT;
    EnGeldB_FacePlayer(this);
    this->actionFunc = EnGeldB_Flee;
}

static void EnGeldB_ChooseFromReady(EnGeldB* this, PlayState* play, s16 playerFacingDiff) {
    if (!Actor_IsFacingPlayer(&this->actor, 30 * 0x10000 / 360)) {
        EnGeldB_SetupPivot(this);
        return;
    }
    bool isLeapRange = (this->actor.xzDistToPlayer < 210.0f) && (this->actor.xzDistToPlayer > 150.0f);
    if (isLeapRange && (Rand_ZeroOne() < 0.3f)) {
        if (!EnGeldB_CanAttack(this, play) || (Rand_ZeroOne() > 0.5f) || (playerFacingDiff < 0x38E0)) {
            EnGeldB_SetupRollForward(this);
        } else {
            EnGeldB_SetupSpinAttack(this);
        }
    } else if (Rand_ZeroOne() > 0.3f) {
        EnGeldB_SetupAdvance(this);
    } else {
        EnGeldB_SetupCircle(this);
    }
}

static void EnGeldB_Ready(EnGeldB* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    SkelAnime_Update(&this->skelAnime);
    if (this->lookTimer != 0) {
        if (AbsYawDiff(this->actor.yawTowardsPlayer - this->actor.shape.rot.y, this->headRot) > 0x2000) {
            this->lookTimer--;
            return;
        }
        this->lookTimer = 0;
    }
    if (EnGeldB_DodgeRanged(this, play) || EnGeldB_ReactToPlayer(this, play, false)) {
        return;
    }
    s16 playerFacingDiff = EnGeldB_GetPlayerFacingDiff(this, play);
    if ((this->actor.xzDistToPlayer < 100.0f) && (player->meleeWeaponState != 0) && (playerFacingDiff >= 0x1F40)) {
        EnGeldB_FacePlayer(this);
        EnGeldB_SetupCircle(this);
        return;
    }
    this->timer--;
    if (this->timer != 0) {
        return;
    }
    EnGeldB_ChooseFromReady(this, play, playerFacingDiff);
    EnGeldB_PlayBreath(this, play);
}

static void EnGeldB_SetupReady(EnGeldB* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sNeutralAnim), -4.0f);
    this->action = GELDB_ACTION_READY;
    this->timer = Rand_ZeroOne() * 10.0f + 5.0f;
    this->actor.speed = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = EnGeldB_Ready;
}

static void EnGeldB_ChooseFromAdvance(EnGeldB* this, PlayState* play, s16 playerFacingDiff) {
    if (!Actor_IsFacingPlayer(&this->actor, 0x11C7)) {
        if (Rand_ZeroOne() > 0.5f) {
            EnGeldB_SetupCircle(this);
        } else {
            EnGeldB_SetupReady(this);
        }
        return;
    }
    if (this->actor.xzDistToPlayer >= 90.0f) {
        return;
    }
    bool canAttack = EnGeldB_CanAttack(this, play);
    bool isPlayerOpen = (this->actor.xzDistToPlayer <= 45.0f) && (playerFacingDiff < 0x38E0);
    if (canAttack && ((Rand_ZeroOne() > 0.03f) || isPlayerOpen)) {
        EnGeldB_SetupSlash(this);
    } else if (!canAttack && (Rand_ZeroOne() > 0.5f)) {
        EnGeldB_SetupRollBack(this);
    } else {
        EnGeldB_SetupCircle(this);
    }
}

static void EnGeldB_Advance(EnGeldB* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (EnGeldB_DodgeRanged(this, play)) {
        return;
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0x2EE, 0);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (this->actor.xzDistToPlayer <= 40.0f) {
        Math_SmoothStepToF(&this->actor.speed, -8.0f, 1.0f, 1.5f, 0.0f);
    } else if (this->actor.xzDistToPlayer > 55.0f) {
        Math_SmoothStepToF(&this->actor.speed, 8.0f, 1.0f, 1.5f, 0.0f);
    } else {
        Math_SmoothStepToF(&this->actor.speed, 0.0f, 1.0f, 6.65f, 0.0f);
    }
    this->skelAnime.playSpeed = this->actor.speed / 8.0f;
    s16 playerFacingDiff = EnGeldB_GetPlayerFacingDiff(this, play);
    if ((this->actor.xzDistToPlayer < 150.0f) && (player->meleeWeaponState != 0) && (playerFacingDiff >= 0x1F40)) {
        EnGeldB_FacePlayer(this);
        if (Rand_ZeroOne() > 0.7f) {
            EnGeldB_SetupCircle(this);
            return;
        }
    }
    s32 prevFrame = this->skelAnime.curFrame;
    SkelAnime_Update(&this->skelAnime);
    EnGeldB_ChooseFromAdvance(this, play, playerFacingDiff);
    // Vanilla still lets an incoming threat override the choice made above.
    if (EnGeldB_ReactToPlayer(this, play, false)) {
        return;
    }
    bool isLeapRange = (this->actor.xzDistToPlayer < 210.0f) && (this->actor.xzDistToPlayer > 150.0f);
    if (isLeapRange && Actor_IsFacingPlayer(&this->actor, 0x71C)) {
        if (!Actor_IsTargeted(play, &this->actor)) {
            EnGeldB_SetupCircle(this);
            return;
        }
        if (Rand_ZeroOne() > 0.5f) {
            EnGeldB_SetupRollForward(this);
        } else {
            EnGeldB_SetupSpinAttack(this);
        }
    }
    EnGeldB_PlayBreath(this, play);
    EnGeldB_PlayStepSfx(this, prevFrame, 4);
}

static void EnGeldB_SetupAdvance(EnGeldB* this) {
    Animation_Change(&this->skelAnime, ANIM(sWalkAnim), 1.0f, 0.0f, LAST_FRAME(sWalkAnim), ANIMMODE_LOOP_INTERP, -4.0f);
    this->action = GELDB_ACTION_ADVANCE;
    this->actionFunc = EnGeldB_Advance;
}

static void EnGeldB_RollForward(EnGeldB* this, PlayState* play) {
    s16 playerFacingDiff = EnGeldB_GetPlayerFacingDiff(this, play);

    if (SkelAnime_Update(&this->skelAnime)) {
        this->actor.speed = 0.0f;
        if (!Actor_IsFacingPlayer(&this->actor, 0x1554)) {
            EnGeldB_SetupReady(this);
            this->timer = (Rand_ZeroOne() * 5.0f) + 5.0f;
            if (playerFacingDiff < 0x38E0) {
                this->lookTimer = 20;
            }
        } else if (EnGeldB_CanAttack(this, play) && ((Rand_ZeroOne() > 0.5f) || (playerFacingDiff < 0x3FFC))) {
            EnGeldB_SetupSlash(this);
        } else {
            EnGeldB_SetupAdvance(this);
        }
    }
    EnGeldB_PlayBreath(this, play);
}

static void EnGeldB_SetupRollForward(EnGeldB* this) {
    Animation_Change(&this->skelAnime, ANIM(sFlipAnim), -1.0f, LAST_FRAME(sFlipAnim), 0.0f, ANIMMODE_ONCE, -3.0f);
    this->timer = 0;
    this->action = GELDB_ACTION_ROLL_FORWARD;
    EnGeldB_FacePlayer(this);
    this->actor.speed = 10.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_TEKU_JUMP);
    this->actionFunc = EnGeldB_RollForward;
}

static void EnGeldB_Pivot(EnGeldB* this, PlayState* play) {
    if (EnGeldB_DodgeRanged(this, play) || EnGeldB_ReactToPlayer(this, play, false)) {
        return;
    }
    s16 angleToPlayer = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
    s16 turnRate = (angleToPlayer > 0) ? ((angleToPlayer * 0.25f) + 2000.0f) : ((angleToPlayer * 0.25f) - 2000.0f);
    this->actor.shape.rot.y += turnRate;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    f32 playSpeed = turnRate * 0.5f;
    this->skelAnime.playSpeed = (angleToPlayer > 0) ? -CLAMP_MAX(playSpeed, 1.0f) : -CLAMP_MIN(playSpeed, -1.0f);
    SkelAnime_Update(&this->skelAnime);
    if (Actor_IsFacingPlayer(&this->actor, 30 * 0x10000 / 360)) {
        if (Rand_ZeroOne() > 0.8f) {
            EnGeldB_SetupCircle(this);
        } else {
            EnGeldB_SetupAdvance(this);
        }
    }
    EnGeldB_PlayBreath(this, play);
}

static void EnGeldB_SetupPivot(EnGeldB* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sSidestepAnim), -4.0f);
    this->action = GELDB_ACTION_PIVOT;
    this->actionFunc = EnGeldB_Pivot;
}

static void EnGeldB_Circle(EnGeldB* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xFA0, 1);
    if (EnGeldB_DodgeRanged(this, play) || EnGeldB_ReactToPlayer(this, play, false)) {
        return;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x3A98;
    s16 behindPlayer = player->actor.shape.rot.y + 0x8000;
    if (Math_SinS(behindPlayer - this->actor.shape.rot.y) >= 0.0f) {
        this->actor.speed = MAX(this->actor.speed - 0.25f, -8.0f);
    } else {
        this->actor.speed = MIN(this->actor.speed + 0.25f, 8.0f);
    }
    if (EnGeldB_IsSideBlocked(this, play)) {
        EnGeldB_BounceSideways(this);
    }
    EnGeldB_StrafeTowardPlayer(this, this->actor.shape.rot.y);
    EnGeldB_UpdateStrafeAnim(this, play);
    bool isBehindPlayer = Math_CosS(behindPlayer - this->actor.shape.rot.y) < -0.85f;
    if (isBehindPlayer && EnGeldB_CanAttack(this, play) && (this->actor.xzDistToPlayer <= 45.0f)) {
        EnGeldB_SetupSlash(this);
        return;
    }
    this->timer--;
    if (this->timer != 0) {
        return;
    }
    if (!EnGeldB_CanAttack(this, play) && (Rand_ZeroOne() > 0.5f)) {
        EnGeldB_SetupRollBack(this);
    } else {
        EnGeldB_SetupReady(this);
    }
}

static void EnGeldB_SetupCircle(EnGeldB* this) {
    Animation_Change(&this->skelAnime, ANIM(sSidestepAnim), 1.0f, 0.0f, LAST_FRAME(sSidestepAnim), ANIMMODE_LOOP_INTERP,
                     0.0f);
    this->actor.speed = Rand_CenteredFloat(12.0f);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->skelAnime.playSpeed = -this->actor.speed * 0.5f;
    this->timer = Rand_ZeroOne() * 30.0f + 30.0f;
    this->action = GELDB_ACTION_CIRCLE;
    this->approachRate = 0.0f;
    this->actionFunc = EnGeldB_Circle;
}

static void EnGeldB_SpinDodge(EnGeldB* this, PlayState* play) {
    this->actor.world.rot.y = this->actor.yawTowardsPlayer + 0x3A98;
    if (EnGeldB_IsSideBlocked(this, play)) {
        EnGeldB_SetupJump(this);
        return;
    }
    EnGeldB_StrafeTowardPlayer(this, this->actor.yawTowardsPlayer);
    EnGeldB_UpdateStrafeAnim(this, play);
    this->timer--;
    if (this->timer != 0) {
        this->actor.shape.rot.y += (this->actor.speed >= 0.0f) ? 0x4000 : -0x4000;
        return;
    }
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    if (EnGeldB_DodgeRanged(this, play)) {
        return;
    }
    if (EnGeldB_CanAttack(this, play) && (this->actor.xzDistToPlayer <= 70.0f)) {
        EnGeldB_SetupSlash(this);
    } else {
        EnGeldB_SetupRollBack(this);
    }
}

static f32 EnGeldB_GetDodgeSpeed(EnGeldB* this, PlayState* play, f32 speed) {
    f32 side = Math_SinS(GET_PLAYER(play)->actor.shape.rot.y - this->actor.shape.rot.y);

    if (side > 0.0f) {
        return -speed;
    }
    if (side < 0.0f) {
        return speed;
    }
    return (Rand_ZeroOne() > 0.5f) ? speed : -speed;
}

static void EnGeldB_SetupSpinDodge(EnGeldB* this, PlayState* play) {
    Animation_Change(&this->skelAnime, ANIM(sSidestepAnim), 1.0f, 0.0f, LAST_FRAME(sSidestepAnim), ANIMMODE_LOOP_INTERP,
                     0.0f);
    this->actor.speed = EnGeldB_GetDodgeSpeed(this, play, 10.0f);
    this->skelAnime.playSpeed = -this->actor.speed * 0.5f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->timer = 6;
    this->approachRate = 0.0f;
    this->action = GELDB_ACTION_SPIN_DODGE;
    this->actionFunc = EnGeldB_SpinDodge;
}

static void EnGeldB_ChooseAfterAttack(EnGeldB* this, PlayState* play) {
    if ((Rand_ZeroOne() > 0.7f) || (this->actor.xzDistToPlayer >= 120.0f)) {
        EnGeldB_SetupReady(this);
        this->timer = (Rand_ZeroOne() * 5.0f) + 5.0f;
        return;
    }
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    if (Rand_ZeroOne() > 0.7f) {
        EnGeldB_SetupSidestep(this, play);
    } else if (EnGeldB_GetPlayerFacingDiff(this, play) > 0x2710) {
        EnGeldB_SetupCircle(this);
    } else if (AbsYawDiff(this->actor.yawTowardsPlayer, this->actor.shape.rot.y) > 0x3E80) {
        this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        EnGeldB_SetupCircle(this);
    } else {
        EnGeldB_ReactToPlayer(this, play, true);
    }
}

static void EnGeldB_Slash(EnGeldB* this, PlayState* play) {
    this->actor.speed = 0.0f;
    if ((s32)this->skelAnime.curFrame == 1) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_PIRATE_ATTACK);
        this->meleeWeaponState = 1;
    } else if ((s32)this->skelAnime.curFrame == 6) {
        this->meleeWeaponState = -1;
    }
    if (this->swordCollider.base.atFlags & AT_BOUNCED) {
        this->meleeWeaponState = -1;
        this->swordCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
        EnGeldB_SetupRollBack(this);
        return;
    }
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (!Actor_IsFacingPlayer(&this->actor, 0x1554)) {
        EnGeldB_SetupReady(this);
        this->timer = (Rand_ZeroOne() * 5.0f) + 5.0f;
        if (AbsYawDiff(this->actor.yawTowardsPlayer, this->actor.shape.rot.y) > 0x4000) {
            this->lookTimer = 20;
        }
        return;
    }
    EnGeldB_ChooseAfterAttack(this, play);
}

static void EnGeldB_SetupSlash(EnGeldB* this) {
    Animation_PlayOnce(&this->skelAnime, ANIM(sSlashAnim));
    this->swordCollider.base.atFlags &= ~AT_BOUNCED;
    this->action = GELDB_ACTION_SLASH;
    this->wasSpinBlocked = false;
    this->actor.speed = 0.0f;
    AudioSfx_StopByPosAndId(&this->actor.projectedPos, NA_SE_EN_PIRATE_BREATH);
    this->actionFunc = EnGeldB_Slash;
}

// OoT's capture-to-jail on a spin hit is dropped: the hit just hurts the player.
static void EnGeldB_SpinAttack(EnGeldB* this, PlayState* play) {
    if (this->swordCollider.base.atFlags & AT_BOUNCED) {
        this->swordCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
        this->wasSpinBlocked = true;
        this->skelAnime.playSpeed = 1.5f;
    }
    s32 frame = this->skelAnime.curFrame;
    if (frame < 9) {
        EnGeldB_FacePlayer(this);
    } else if (frame == 13) {
        EnGeldB_SpawnFeetDust(this, play);
        this->meleeWeaponState = 1;
        this->actor.speed = 10.0f;
        Actor_PlaySfx(&this->actor, NA_SE_EN_PIRATE_ATTACK);
    } else if (frame == 21) {
        this->actor.speed = 0.0f;
    } else if (frame == 24) {
        this->meleeWeaponState = -1;
    }
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (!Actor_IsFacingPlayer(&this->actor, 0x1554)) {
        EnGeldB_SetupReady(this);
        this->timer = (Rand_ZeroOne() * 5.0f) + 5.0f;
        this->lookTimer = 46;
    } else if (this->wasSpinBlocked) {
        EnGeldB_SetupRollBack(this);
    } else {
        EnGeldB_ChooseAfterAttack(this, play);
    }
}

static void EnGeldB_SetupSpinAttack(EnGeldB* this) {
    Animation_Change(&this->skelAnime, ANIM(sSpinAttackAnim), 1.0f, 0.0f, LAST_FRAME(sSpinAttackAnim),
                     ANIMMODE_ONCE_INTERP, 0.0f);
    this->swordCollider.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    this->action = GELDB_ACTION_SPIN_ATTACK;
    this->wasSpinBlocked = false;
    this->actor.speed = 0.0f;
    this->actionFunc = EnGeldB_SpinAttack;
}

static void EnGeldB_RollBack(EnGeldB* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        bool isSpinRange = (this->actor.xzDistToPlayer < 170.0f) && (this->actor.xzDistToPlayer > 140.0f);
        if (EnGeldB_CanAttack(this, play) && isSpinRange && (Rand_ZeroOne() < 0.2f)) {
            EnGeldB_SetupSpinAttack(this);
        } else if (play->gameplayFrames & 1) {
            EnGeldB_SetupSidestep(this, play);
        } else {
            EnGeldB_SetupReady(this);
        }
    }
    if ((play->state.frames & 0x5F) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_PIRATE_BREATH);
    }
}

static void EnGeldB_SetupRollBack(EnGeldB* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sFlipAnim), -3.0f);
    this->timer = 0;
    this->action = GELDB_ACTION_ROLL_BACK;
    this->actor.speed = -8.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_TEKU_JUMP);
    EnGeldB_FacePlayer(this);
    this->actionFunc = EnGeldB_RollBack;
}

static void EnGeldB_SlowOnGround(EnGeldB* this) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
        this->actor.speed = 0.0f;
    }
    if (IsOnGround(this) && (this->actor.speed < 0.0f)) {
        this->actor.speed += 0.05f;
    }
}

static void EnGeldB_Stunned(EnGeldB* this, PlayState* play) {
    EnGeldB_SlowOnGround(this);
    if ((this->actor.colorFilterTimer != 0) || !IsOnGround(this)) {
        return;
    }
    EnGeldB_Thaw(this, play);
    if (this->actor.colChkInfo.health == 0) {
        EnGeldB_SetupDefeated(this);
    } else {
        EnGeldB_ReactToPlayer(this, play, true);
    }
}

static void EnGeldB_SetupStunned(EnGeldB* this) {
    if (IsOnGround(this)) {
        this->actor.speed = 0.0f;
    }
    if ((this->damageEffect != GELDB_DMGEFF_FREEZE) || (this->action == GELDB_ACTION_SPIN_ATTACK)) {
        Animation_PlayOnceSetSpeed(&this->skelAnime, ANIM(sDamageAnim), 0.0f);
    }
    if (this->damageEffect == GELDB_DMGEFF_FREEZE) {
        this->drawDmgEffAlpha = 1.0f;
    }
    Actor_PlaySfx(&this->actor, NA_SE_EN_COMMON_FREEZE);
    this->action = GELDB_ACTION_STUNNED;
    this->actionFunc = EnGeldB_Stunned;
}

static void EnGeldB_Damaged(EnGeldB* this, PlayState* play) {
    EnGeldB_SlowOnGround(this);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0x1194, 0);
    if (EnGeldB_DodgeRanged(this, play) || EnGeldB_ReactToPlayer(this, play, false)) {
        return;
    }
    if (!SkelAnime_Update(&this->skelAnime) || !IsOnGround(this)) {
        return;
    }
    bool isWallInFront = IsTouchingWall(this) && (AbsYawDiff(this->actor.wallYaw, this->actor.shape.rot.y) < 0x2EE0);
    if (isWallInFront && (this->actor.xzDistToPlayer < 90.0f)) {
        EnGeldB_SetupJump(this);
        return;
    }
    if (EnGeldB_DodgeRanged(this, play)) {
        return;
    }
    bool canSlash =
        (this->actor.xzDistToPlayer <= 45.0f) && EnGeldB_CanAttack(this, play) && ((play->gameplayFrames & 7) != 0);
    if (canSlash) {
        EnGeldB_SetupSlash(this);
    } else {
        EnGeldB_SetupRollBack(this);
    }
}

static void EnGeldB_SetupDamaged(EnGeldB* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sDamageAnim), -4.0f);
    if (IsOnGround(this)) {
        this->actor.speed = -4.0f;
    }
    this->lookTimer = 0;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    Actor_PlaySfx(&this->actor, NA_SE_EN_PIRATE_DAMAGE);
    this->action = GELDB_ACTION_DAMAGED;
    this->actionFunc = EnGeldB_Damaged;
}

static void EnGeldB_Jump(EnGeldB* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xFA0, 1);
    if (this->actor.velocity.y >= 5.0f) {
        EnGeldB_SpawnTakeoffSparkles(play, &this->leftFootPos);
        EnGeldB_SpawnTakeoffSparkles(play, &this->rightFootPos);
    }
    bool hasLanded = (this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH)) != 0;
    if (!SkelAnime_Update(&this->skelAnime) || !hasLanded) {
        return;
    }
    EnGeldB_FacePlayer(this);
    this->actor.shape.rot.x = 0;
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 0.0f;
    this->actor.world.pos.y = this->actor.floorHeight;
    if (EnGeldB_CanAttack(this, play)) {
        EnGeldB_SetupSlash(this);
    } else {
        EnGeldB_SetupReady(this);
    }
}

static void EnGeldB_SetupJump(EnGeldB* this) {
    Animation_Change(&this->skelAnime, ANIM(sFlipAnim), -1.0f, LAST_FRAME(sFlipAnim), 0.0f, ANIMMODE_ONCE, -3.0f);
    this->timer = 0;
    this->action = GELDB_ACTION_JUMP;
    this->actor.speed = 6.5f;
    this->actor.velocity.y = 15.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_TEKU_JUMP);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actionFunc = EnGeldB_Jump;
}

static bool EnGeldB_IsPlayerSwingingAtGuard(EnGeldB* this, PlayState* play) {
    return func_800BE184(play, &this->actor, 100.0f, 0x2710, 0x4000, this->actor.shape.rot.y);
}

static void EnGeldB_FinishBlock(EnGeldB* this, PlayState* play) {
    bool isPlayerInReach = (AbsYawDiff(this->actor.yawTowardsPlayer, this->actor.shape.rot.y) <= 0x4000) &&
                           (this->actor.xzDistToPlayer < 40.0f) && (ABS(this->actor.playerHeightRel) < 50.0f);

    if (!isPlayerInReach) {
        EnGeldB_SetupCircle(this);
        return;
    }
    if (EnGeldB_IsPlayerSwingingAtGuard(this, play)) {
        if (EnGeldB_IsPlayerJumpslashing(play)) {
            EnGeldB_SetupSpinDodge(this, play);
        } else if (play->gameplayFrames & 1) {
            EnGeldB_SetupBlock(this);
        } else {
            EnGeldB_SetupRollBack(this);
        }
        return;
    }
    bool isPlayerOpen = (play->gameplayFrames & 1) || (EnGeldB_GetPlayerFacingDiff(this, play) < 0x38E0);
    if (EnGeldB_CanAttack(this, play) && isPlayerOpen) {
        EnGeldB_SetupSlash(this);
    } else {
        EnGeldB_SetupCircle(this);
    }
}

static void EnGeldB_Block(EnGeldB* this, PlayState* play) {
    if (this->timer != 0) {
        this->timer--;
    } else {
        this->skelAnime.playSpeed = 1.0f;
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        EnGeldB_FinishBlock(this, play);
        return;
    }
    if ((this->timer != 0) || !EnGeldB_IsPlayerSwingingAtGuard(this, play)) {
        return;
    }
    if (EnGeldB_IsPlayerJumpslashing(play)) {
        EnGeldB_SetupSpinDodge(this, play);
    } else if (EnGeldB_DodgeRanged(this, play)) {
        return;
    } else if (!(play->gameplayFrames & 1)) {
        EnGeldB_SetupBlock(this);
    } else if ((this->actor.xzDistToPlayer < 100.0f) && (Rand_ZeroOne() > 0.7f)) {
        EnGeldB_SetupJump(this);
    } else {
        EnGeldB_SetupRollBack(this);
    }
}

static void EnGeldB_SetupBlock(EnGeldB* this) {
    if (this->meleeWeaponState != 0) {
        this->meleeWeaponState = -1;
    }
    this->actor.speed = 0.0f;
    this->action = GELDB_ACTION_BLOCK;
    this->timer = (s32)Rand_CenteredFloat(10.0f) + 10;
    Animation_Change(&this->skelAnime, ANIM(sBlockAnim), 0.0f, 0.0f, LAST_FRAME(sBlockAnim), ANIMMODE_ONCE, 0.0f);
    this->actionFunc = EnGeldB_Block;
}

static void EnGeldB_ChooseAfterSidestep(EnGeldB* this, PlayState* play) {
    s16 playerFacingDiff = EnGeldB_GetPlayerFacingDiff(this, play);

    if (playerFacingDiff >= 0x3A98) {
        EnGeldB_SetupReady(this);
        this->timer = (Rand_ZeroOne() * 5.0f) + 1.0f;
        return;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    bool isPlayerOpen = !(play->gameplayFrames & 3) || (playerFacingDiff < 0x38E0);
    bool isLeapRange = (this->actor.xzDistToPlayer < 210.0f) && (this->actor.xzDistToPlayer > 150.0f);
    if ((this->actor.xzDistToPlayer <= 45.0f) && EnGeldB_CanAttack(this, play) && isPlayerOpen) {
        EnGeldB_SetupSlash(this);
    } else if (isLeapRange && !(play->gameplayFrames & 1)) {
        if (!EnGeldB_CanAttack(this, play) || (Rand_ZeroOne() > 0.5f) || (playerFacingDiff < 0x38E0)) {
            EnGeldB_SetupRollForward(this);
        } else {
            EnGeldB_SetupSpinAttack(this);
        }
    } else {
        EnGeldB_SetupAdvance(this);
    }
}

static void EnGeldB_Sidestep(EnGeldB* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xBB8, 1);
    s16 behindPlayer = player->actor.shape.rot.y + 0x8000;
    if (Math_SinS(behindPlayer - this->actor.shape.rot.y) > 0.0f) {
        this->actor.speed += 0.125f;
    } else {
        this->actor.speed -= 0.125f;
    }
    if (EnGeldB_IsSideBlocked(this, play)) {
        EnGeldB_BounceSideways(this);
    }
    this->actor.world.rot.y =
        (this->actor.speed >= 0.0f) ? (this->actor.shape.rot.y + 0x3E80) : (this->actor.shape.rot.y - 0x3E80);
    EnGeldB_StrafeTowardPlayer(this, this->actor.shape.rot.y);
    s32 prevFrame = this->skelAnime.curFrame;
    SkelAnime_Update(&this->skelAnime);
    if (EnGeldB_DodgeRanged(this, play) || EnGeldB_ReactToPlayer(this, play, false)) {
        return;
    }
    this->timer--;
    if (this->timer == 0) {
        EnGeldB_ChooseAfterSidestep(this, play);
    }
    EnGeldB_PlayStepSfx(this, prevFrame, 5);
    EnGeldB_PlayBreath(this, play);
}

static void EnGeldB_SetupSidestep(EnGeldB* this, PlayState* play) {
    Animation_Change(&this->skelAnime, ANIM(sSidestepAnim), 1.0f, 0.0f, LAST_FRAME(sSidestepAnim), ANIMMODE_LOOP_INTERP,
                     0.0f);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xFA0, 1);
    f32 side = Math_SinS(GET_PLAYER(play)->actor.shape.rot.y - this->actor.shape.rot.y);
    if (side > 0.0f) {
        this->actor.speed = -6.0f;
    } else if (side < 0.0f) {
        this->actor.speed = 6.0f;
    } else {
        this->actor.speed = Rand_CenteredFloat(12.0f);
    }
    this->skelAnime.playSpeed = -this->actor.speed * 0.5f;
    this->approachRate = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x3FFF;
    this->timer = Rand_ZeroOne() * 10.0f + 5.0f;
    this->action = GELDB_ACTION_SIDESTEP;
    this->actionFunc = EnGeldB_Sidestep;
}

static void EnGeldB_Defeated(EnGeldB* this, PlayState* play) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
        this->actor.speed = 0.0f;
    }
    if (IsOnGround(this)) {
        Math_SmoothStepToF(&this->actor.speed, 0.0f, 1.0f, 0.5f, 0.0f);
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        EnGeldB_SetupFlee(this);
    } else if ((s32)this->skelAnime.curFrame == 10) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_GERUDOFT_DOWN);
        EnGeldB_StopBgm(this);
    }
}

static void EnGeldB_SetupDefeated(EnGeldB* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sDefeatAnim), -4.0f);
    EnGeldB_FacePlayer(this);
    if (IsOnGround(this)) {
        this->actor.speed = -6.0f;
    }
    this->action = GELDB_ACTION_DEFEAT;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Actor_PlaySfx(&this->actor, NA_SE_EN_PIRATE_DEAD);
    this->actionFunc = EnGeldB_Defeated;
}

static bool EnGeldB_DodgeRanged(EnGeldB* this, PlayState* play) {
    Actor* projectile = func_800BC270(play, &this->actor, 800.0f, GERUDO_FIGHTER_PROJECTILE_FLAGS);

    if (projectile == NULL) {
        return false;
    }
    s16 angleToProjectile = AbsYawDiff(Actor_WorldYawTowardActor(&this->actor, projectile), this->actor.shape.rot.y);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    // OoT's distance test squares an already rooted distance, so within this range it always passes.
    if (angleToProjectile < 0x2EE0) {
        if (projectile->id == ACTOR_ARMS_HOOK) {
            EnGeldB_SetupJump(this);
        } else {
            EnGeldB_SetupBlock(this);
        }
        return true;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y + 0x3FFF;
    if ((angleToProjectile < 0x2000) || (angleToProjectile > 0x5FFF)) {
        EnGeldB_SetupSidestep(this, play);
        this->actor.speed *= 3.0f;
    } else {
        EnGeldB_SetupRollBack(this);
    }
    return true;
}

static void EnGeldB_TurnHead(EnGeldB* this) {
    if ((this->action == GELDB_ACTION_READY) && (this->lookTimer != 0)) {
        this->headRot = Math_SinS(this->lookTimer * 0x1068) * 8920.0f;
        return;
    }
    if (this->action == GELDB_ACTION_STUNNED) {
        return;
    }
    if ((this->action == GELDB_ACTION_SLASH) || (this->action == GELDB_ACTION_SPIN_ATTACK)) {
        this->headRot = 0;
        return;
    }
    Math_SmoothStepToS(&this->headRot, this->actor.yawTowardsPlayer - this->actor.shape.rot.y, 1, 0x1F4, 0);
    this->headRot = CLAMP(this->headRot, -0x256F, 0x256F);
}

static void EnGeldB_TakeHit(EnGeldB* this, PlayState* play) {
    u8 effect = this->actor.colChkInfo.damageEffect;

    this->damageEffect = effect;
    Actor_SetDropFlag(&this->actor, &this->bodyCollider.elem);
    AudioSfx_StopByPosAndId(&this->actor.projectedPos, NA_SE_EN_PIRATE_BREATH);
    if ((effect == GELDB_DMGEFF_STUN) || (effect == GELDB_DMGEFF_FREEZE)) {
        if (this->action != GELDB_ACTION_STUNNED) {
            u16 frames = (effect == GELDB_DMGEFF_FREEZE) ? (GERUDO_FIGHTER_STUN_FRAMES + GERUDO_FIGHTER_THAW_FRAMES)
                                                         : GERUDO_FIGHTER_STUN_FRAMES;
            Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, frames);
            Actor_ApplyDamage(&this->actor);
            EnGeldB_SetupStunned(this);
        }
        return;
    }
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
    if (Actor_ApplyDamage(&this->actor) != 0) {
        EnGeldB_SetupDamaged(this);
        return;
    }
    EnGeldB_SetupDefeated(this);
    Enemy_StartFinishingBlow(play, &this->actor);
}

static void EnGeldB_UpdateDamage(EnGeldB* this, PlayState* play) {
    if (this->blockCollider.base.acFlags & AC_BOUNCED) {
        this->blockCollider.base.acFlags &= ~AC_BOUNCED;
        this->bodyCollider.base.acFlags &= ~AC_HIT;
        return;
    }
    if (!(this->bodyCollider.base.acFlags & AC_HIT) || (this->action < GELDB_ACTION_READY)) {
        return;
    }
    this->bodyCollider.base.acFlags &= ~AC_HIT;
    EnGeldB_TakeHit(this, play);
}

static void EnGeldB_UpdateCollision(EnGeldB* this, PlayState* play) {
    CollisionCheckContext* colChkCtx = &play->colChkCtx;
    bool isRedFlashing = (this->actor.colorFilterTimer != 0) && (this->actor.colorFilterParams & 0x4000);

    Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
    CollisionCheck_SetOC(play, colChkCtx, &this->bodyCollider.base);
    if ((this->action >= GELDB_ACTION_READY) && !isRedFlashing) {
        CollisionCheck_SetAC(play, colChkCtx, &this->bodyCollider.base);
    }
    if ((this->action == GELDB_ACTION_BLOCK) && (this->skelAnime.curFrame == 0.0f)) {
        CollisionCheck_SetAC(play, colChkCtx, &this->blockCollider.base);
    }
    if (this->meleeWeaponState > 0) {
        CollisionCheck_SetAT(play, colChkCtx, &this->swordCollider.base);
    }
}

static void EnGeldB_UpdateBlink(EnGeldB* this, PlayState* play) {
    if (this->blinkState != 0) {
        this->blinkState = (this->blinkState + 1) & 3;
        return;
    }
    if ((Rand_ZeroOne() < 0.1f) && ((play->gameplayFrames % 4) == 0)) {
        this->blinkState++;
    }
}

static void EnGeldB_Update(Actor* thisx, PlayState* play) {
    EnGeldB* this = (EnGeldB*)thisx;

    EnGeldB_UpdateDamage(this, play);
    Actor_MoveWithGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 15.0f, 30.0f, 60.0f, GERUDO_FIGHTER_BGCHECK_FLAGS);
    this->actionFunc(this, play);
    this->actor.focus.pos = this->actor.world.pos;
    this->actor.focus.pos.y += 40.0f;
    EnGeldB_TurnHead(this);
    EnGeldB_UpdateCollision(this, play);
    EnGeldB_UpdateBlink(this, play);
}

static void EnGeldB_InitSwordTrail(EnGeldB* this, PlayState* play) {
    EffectBlureInit1 slashBlure;

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
}

static void EnGeldB_Init(Actor* thisx, PlayState* play) {
    EnGeldB* this = (EnGeldB*)thisx;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    this->actor.attentionRangeType = ATTENTION_RANGE_3;
    this->actor.colChkInfo.damageTable = &sDamageTable;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawFeet, 0.0f);
    this->actor.colChkInfo.mass = MASS_HEAVY;
    this->actor.colChkInfo.health = GERUDO_FIGHTER_HEALTH;
    this->actor.colChkInfo.cylRadius = 50;
    this->actor.colChkInfo.cylHeight = 100;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, ANIM(sNeutralAnim), this->jointTable,
                       this->morphTable, GELDB_LIMB_MAX);
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyColliderInit);
    Collider_InitTris(play, &this->blockCollider);
    Collider_SetTris(play, &this->blockCollider, &this->actor, &sBlockColliderInit, this->blockElements);
    Collider_InitQuad(play, &this->swordCollider);
    Collider_SetQuad(play, &this->swordCollider, &this->actor, &sSwordColliderInit);
    EnGeldB_InitSwordTrail(this, play);
    Actor_SetScale(&this->actor, 0.0125f);
    EnGeldB_SetupWait(this);
}

static void EnGeldB_Destroy(Actor* thisx, PlayState* play) {
    EnGeldB* this = (EnGeldB*)thisx;

    EnGeldB_StopBgm(this);
    Effect_Destroy(play, this->blureIndex);
    Collider_DestroyTris(play, &this->blockCollider);
    Collider_DestroyCylinder(play, &this->bodyCollider);
    Collider_DestroyQuad(play, &this->swordCollider);
}

static s32 EnGeldB_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    EnGeldB* this = (EnGeldB*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    if (limbIndex == GELDB_LIMB_NECK) {
        rot->x += this->headRot;
    } else if (limbIndex == GELDB_LIMB_HEAD) {
        gDPSetEnvColor(POLY_OPA_DISP++, 80, 60, 10, 255);
    } else if ((limbIndex == GELDB_LIMB_R_SWORD) || (limbIndex == GELDB_LIMB_L_SWORD)) {
        gDPSetEnvColor(POLY_OPA_DISP++, 140, 170, 230, 255);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    } else {
        gDPSetEnvColor(POLY_OPA_DISP++, 140, 0, 0, 255);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    return false;
}

static void EnGeldB_UpdateSword(EnGeldB* this) {
    static Vec3f sTipOffset = { 0.0f, -3000.0f, 0.0f };
    static Vec3f sHiltOffset = { 400.0f, 0.0f, 0.0f };
    static Vec3f sQuadOffset0 = { -3000.0f, -2000.0f, 1300.0f };
    static Vec3f sQuadOffset1 = { 1600.0f, -4000.0f, 0.0f };
    static Vec3f sQuadOffset2 = { 1000.0f, 1000.0f, 0.0f };
    static Vec3f sQuadOffset3 = { -3000.0f, -2000.0f, -1300.0f };
    ColliderQuadDim* quad = &this->swordCollider.dim;
    Vec3f tip;
    Vec3f hilt;

    Matrix_MultVec3f(&sQuadOffset1, &quad->quad[1]);
    Matrix_MultVec3f(&sQuadOffset0, &quad->quad[0]);
    Matrix_MultVec3f(&sQuadOffset3, &quad->quad[3]);
    Matrix_MultVec3f(&sQuadOffset2, &quad->quad[2]);
    Collider_SetQuadVertices(&this->swordCollider, &quad->quad[0], &quad->quad[1], &quad->quad[2], &quad->quad[3]);
    Matrix_MultVec3f(&sTipOffset, &tip);
    Matrix_MultVec3f(&sHiltOffset, &hilt);
    bool isAttacking = (this->action == GELDB_ACTION_SLASH) || (this->action == GELDB_ACTION_SPIN_ATTACK);
    if ((this->meleeWeaponState < 0) || !isAttacking) {
        EffectBlure_AddSpace(Effect_GetByIndex(this->blureIndex));
        this->meleeWeaponState = 0;
    } else if (this->meleeWeaponState > 0) {
        EffectBlure_AddVertex(Effect_GetByIndex(this->blureIndex), &tip, &hilt);
    }
}

static s32 EnGeldB_GetBodyPart(s32 limbIndex) {
    switch (limbIndex) {
        case GELDB_LIMB_NECK:
            return 0;
        case GELDB_LIMB_L_SWORD:
            return 1;
        case GELDB_LIMB_R_SWORD:
            return 2;
        case GELDB_LIMB_L_UPPER_ARM:
            return 3;
        case GELDB_LIMB_R_UPPER_ARM:
            return 4;
        case GELDB_LIMB_TORSO:
            return 5;
        case GELDB_LIMB_WAIST:
            return 6;
        case GELDB_LIMB_L_FOOT:
            return 7;
        case GELDB_LIMB_R_FOOT:
            return 8;
        default:
            return -1;
    }
}

static void EnGeldB_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    static Vec3f sFootOffset = { 300.0f, 0.0f, 0.0f };
    static Vec3f sOrigin = { 0.0f, 0.0f, 0.0f };
    EnGeldB* this = (EnGeldB*)thisx;

    if (limbIndex == GELDB_LIMB_R_SWORD) {
        EnGeldB_UpdateSword(this);
    } else {
        Actor_SetFeetPos(&this->actor, limbIndex, GELDB_LIMB_L_FOOT, &sFootOffset, GELDB_LIMB_R_FOOT, &sFootOffset);
    }
    if (limbIndex == GELDB_LIMB_L_FOOT) {
        Matrix_MultVec3f(&sFootOffset, &this->leftFootPos);
    } else if (limbIndex == GELDB_LIMB_R_FOOT) {
        Matrix_MultVec3f(&sFootOffset, &this->rightFootPos);
    }
    s32 bodyPart = EnGeldB_GetBodyPart(limbIndex);
    if (bodyPart >= 0) {
        Matrix_MultVec3f(&sOrigin, &this->bodyPartsPos[bodyPart]);
    }
}

static void EnGeldB_UpdateBlockCollider(EnGeldB* this) {
    static Vec3f sTrisOffsets0[3] = {
        { -3000.0f, 6000.0f, 1600.0f },
        { -3000.0f, 0.0f, 1600.0f },
        { 3000.0f, 6000.0f, 1600.0f },
    };
    static Vec3f sTrisOffsets1[3] = {
        { -3000.0f, 0.0f, 1600.0f },
        { 3000.0f, 0.0f, 1600.0f },
        { 3000.0f, 6000.0f, 1600.0f },
    };
    Vec3f tris0[3];
    Vec3f tris1[3];

    for (s32 i = 0; i < 3; i++) {
        Matrix_MultVec3f(&sTrisOffsets0[i], &tris0[i]);
        Matrix_MultVec3f(&sTrisOffsets1[i], &tris1[i]);
    }
    Collider_SetTrisVertices(&this->blockCollider, 0, &tris0[0], &tris0[1], &tris0[2]);
    Collider_SetTrisVertices(&this->blockCollider, 1, &tris1[0], &tris1[1], &tris1[2]);
}

static void EnGeldB_Draw(Actor* thisx, PlayState* play) {
    EnGeldB* this = (EnGeldB*)thisx;

    if ((this->action == GELDB_ACTION_WAIT) && this->isHidden) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEyeTextures[this->blinkState]);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          EnGeldB_OverrideLimbDraw, EnGeldB_PostLimbDraw, &this->actor);
    CLOSE_DISPS(play->state.gfxCtx);
    if (this->action == GELDB_ACTION_BLOCK) {
        EnGeldB_UpdateBlockCollider(this);
    }
    Actor_DrawDamageEffects(play, &this->actor, this->bodyPartsPos, GERUDO_FIGHTER_BODYPART_COUNT, 0.5f, 0.75f,
                            this->drawDmgEffAlpha, ACTOR_DRAW_DMGEFF_FROZEN_NO_SFX);
}

static Gfx sSoulEmptyDL[] = {
    gsSPEndDisplayList(),
};

// The soul shows its enemy in an idle loop, as Majora's Mask souls do.
static void DrawSoulModel(PlayState* play) {
    static SkelAnime sSoulSkelAnime;
    static Vec3s sSoulJointTable[GELDB_LIMB_MAX];
    static Vec3s sSoulMorphTable[GELDB_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    if (!sIsSoulPosed) {
        SkelAnime_InitFlex(play, &sSoulSkelAnime, (FlexSkeletonHeader*)sSkel, ANIM(sNeutralAnim), sSoulJointTable,
                           sSoulMorphTable, GELDB_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(0.0125f, 0.0125f, 0.0125f, MTXMODE_APPLY);
    // Animations place the root far from the origin; the soul floats on its root instead.
    Matrix_Translate(-sSoulJointTable[0].x, -sSoulJointTable[0].y, -sSoulJointTable[0].z, MTXMODE_APPLY);
    // Eyes and effects read segments the actor fills; with no actor they point at harmless lists.
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEyeTextures[0]);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0B, (uintptr_t)sSoulEmptyDL);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)sSoulEmptyDL);
    SkelAnime_DrawFlexOpa(play, sSoulSkelAnime.skeleton, sSoulSkelAnime.jointTable, sSoulSkelAnime.dListCount, NULL,
                          NULL, NULL);
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
    gDPSetPrimColor(POLY_XLU_DISP++, 0xC0, 0xC0, 230, 120, 200, 0);
    gDPSetEnvColor(POLY_XLU_DISP++, 230, 120, 200, 0);
    Matrix_Scale(0.13333333f, 0.13333333f, 0.13333333f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gameplay_keep_DL_01ACF0);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Ship: a jumpslash (sword, stick or hammer; Goron here) or the bow.
static bool CanKillGerudoFighter(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RI_DEKU_STICK) ||
           sApi->LogicHasCapability("CanBeGoron") || sApi->LogicCanUseItem(RI_BOW);
}

static void RegisterSoul(void) {
    static S2HCustomItemRandomizer sSoulLogic = { 0 };
    S2HCustomItemDefinition soul = { 0 };

    // Pool membership follows the host's soul shuffle and soul pool; the mod only declares the item.
    sSoulLogic.structSize = sizeof(sSoulLogic);
    sSoulLogic.flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
    sSoulLogic.type = S2H_CUSTOM_ITEM_TYPE_ITEM;
    sSoulLogic.logicKey = GERUDO_FIGHTER_SOUL_KEY;
    sSoulLogic.hintKey = GERUDO_FIGHTER_SOUL_KEY;
    soul.structSize = sizeof(soul);
    soul.key = GERUDO_FIGHTER_SOUL_KEY;
    soul.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO;
    soul.getItemText = "You got the %rSoul of Gerudo Fighters%w!&They can be defeated now.";
    soul.getItemEntry.drawFunc = DrawSoul;
    soul.randomizer = &sSoulLogic;
    if (!sApi->RegisterCustomItem(&soul)) {
        sApi->Log("Could not register " GERUDO_FIGHTER_SOUL_KEY);
    }
}

static s16 RegisterGerudoFighter(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = GERUDO_FIGHTER_KEY;
    definition.description = "Gerudo Fighter";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = GERUDO_FIGHTER_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnGeldB);
    definition.init = EnGeldB_Init;
    definition.destroy = EnGeldB_Destroy;
    definition.update = EnGeldB_Update;
    definition.draw = EnGeldB_Draw;
    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_MINIBOSS | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    definition.naviHint =
        "Gerudo Fighter&A twin-blade thief who blocks, flips&and spins. Strike when she lowers her guard!";
    // The host finds the soul item as "soul." + soulKey, so this is the family key.
    definition.soulKey = GERUDO_FIGHTER_KEY;
    definition.logicKill = CanKillGerudoFighter;
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
    if (!RequireOotAsset(GELDB_OBJECT "gGerudoRedSkel")) {
        sApi->Log(GERUDO_FIGHTER_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    if (RegisterGerudoFighter() < 0) {
        sApi->Log("Could not register " GERUDO_FIGHTER_KEY);
        return;
    }
    RegisterSoul();
}
