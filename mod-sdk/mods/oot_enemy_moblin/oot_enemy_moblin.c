/*
 * Ocarina of Time's Moblins (En_Mb) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * The spear guard and the club Moblin; the spear patrol needs a scene path and is left out.
 * The club turns to the player and checks height as Ship's enemy randomizer makes it do.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define MOBLIN_SPEAR_KEY "oot.moblin_spear"
#define MOBLIN_CLUB_KEY "oot.moblin_club"
#define MB_OBJECT "objects/oot_object_mb/"
#define MB_ASSET(name) "__OTR__" MB_OBJECT name

static const ALIGN_ASSET(2) char sSpearSkel[] = MB_ASSET("gEnMbSpearSkel");
static const ALIGN_ASSET(2) char sClubSkel[] = MB_ASSET("gEnMbClubSkel");
static const ALIGN_ASSET(2) char sSpearStandStillAnim[] = MB_ASSET("gEnMbSpearStandStillAnim");
static const ALIGN_ASSET(2) char sSpearLookAroundAnim[] = MB_ASSET("gEnMbSpearLookLeftAndRightAnim");
static const ALIGN_ASSET(2) char sSpearWalkAnim[] = MB_ASSET("gEnMbSpearWalkAnim");
static const ALIGN_ASSET(2) char sSpearPrepareChargeAnim[] = MB_ASSET("gEnMbSpearPrepareChargeAnim");
static const ALIGN_ASSET(2) char sSpearChargeAnim[] = MB_ASSET("gEnMbSpearChargeAnim");
static const ALIGN_ASSET(2) char sSpearSlowDownAnim[] = MB_ASSET("gEnMbSpearSlowDownAnim");
static const ALIGN_ASSET(2) char sSpearDamagedFromFrontAnim[] = MB_ASSET("gEnMbSpearDamagedFromFrontAnim");
static const ALIGN_ASSET(2) char sSpearDamagedFromBehindAnim[] = MB_ASSET("gEnMbSpearDamagedFromBehindAnim");
static const ALIGN_ASSET(2) char sSpearFallOnItsBackAnim[] = MB_ASSET("gEnMbSpearFallOnItsBackAnim");
static const ALIGN_ASSET(2) char sClubStandStillClubDownAnim[] = MB_ASSET("gEnMbClubStandStillClubDownAnim");
static const ALIGN_ASSET(2) char sClubLiftClubAnim[] = MB_ASSET("gEnMbClubLiftClubAnim");
static const ALIGN_ASSET(2) char sClubStrikeDownAnim[] = MB_ASSET("gEnMbClubStrikeDownAnim");
static const ALIGN_ASSET(2) char sClubDamagedKneelAnim[] = MB_ASSET("gEnMbClubDamagedKneelAnim");
static const ALIGN_ASSET(2) char sClubStandUpAnim[] = MB_ASSET("gEnMbClubStandUpAnim");
static const ALIGN_ASSET(2) char sClubFallOnItsBackAnim[] = MB_ASSET("gEnMbClubFallOnItsBackAnim");

// MM has no Moblins and reuses their sfx ids for Chuchus and Kurosuke, so these are MM stand-ins.
#define MB_SFX_VOICE NA_SE_EN_B_PAMET_VOICE
#define MB_SFX_ATTACK NA_SE_EN_PAMET_ROAR
#define MB_SFX_DEAD NA_SE_EN_DEBU_DEAD_VOICE
#define MB_SFX_WALK NA_SE_EN_IRONNACK_WALK
// MM's IRONNACK_DASH sfx script is garbage that slow-loads a bogus sample and crashes the audio thread.
#define MB_SFX_DASH NA_SE_EN_IRONNACK_WALK
#define MB_SFX_SLIDE NA_SE_PL_SLIP
#define MB_SFX_SPEAR_READY NA_SE_EN_IRONNACK_PULLOUT
#define MB_SFX_CLUB_LAND NA_SE_EN_MONBLIN_GNDWAVE

#define ANIM(asset) ((AnimationHeader*)(asset))
#define LAST_FRAME(asset) Animation_GetLastFrame((void*)(asset))

#define MB_LIMB_CHEST 6
#define MB_LIMB_HEAD 7
#define MB_LIMB_LSHOULDER 9
#define MB_LIMB_LHAND 12
#define MB_LIMB_RSHOULDER 14
#define MB_LIMB_RHAND 17
#define MB_LIMB_LTHIGH 20
#define MB_LIMB_LFOOT 22
#define MB_LIMB_RTHIGH 25
#define MB_LIMB_RFOOT 27
#define MB_LIMB_MAX 28

// Vanilla's numbered states: hits land from STUNNED on, the front shielding from IDLE on.
typedef enum {
    MB_STATE_SPEAR_DAMAGED = 0,
    MB_STATE_CLUB_DEAD = 1,
    MB_STATE_CLUB_KNEELING = 3,
    MB_STATE_STUNNED = 5,
    MB_STATE_IDLE = 6,
    MB_STATE_WALK = 9,
    MB_STATE_ATTACK = 10,
    MB_STATE_ATTACK_END = 11,
} MoblinState;

typedef enum {
    MB_DMGEFF_IGNORE = 0x0,
    MB_DMGEFF_STUN = 0x1,
    MB_DMGEFF_FREEZE = 0x5,
    MB_DMGEFF_STUN_ICE = 0x6,
    MB_DMGEFF_DEFAULT = 0xF,
} MoblinDamageEffect;

typedef enum {
    MB_ATTACK_NONE = 0,
    MB_ATTACK_SPEAR = 1,
    MB_ATTACK_CLUB_RIGHT = 1,
    MB_ATTACK_CLUB_MIDDLE = 2,
    MB_ATTACK_CLUB_LEFT = 3,
} MoblinAttack;

#define MOBLIN_STUN_FRAMES 80
#define MOBLIN_THAW_FRAMES 40
#define MOBLIN_BODYPART_COUNT 10
#define MOBLIN_SPEAR_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED)
#define MOBLIN_CLUB_FLAGS (ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED)
#define MOBLIN_BGCHECK_FLAGS \
    (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_8 | UPDBGCHECKINFO_FLAG_10)
#define MOBLIN_SPEAR_WALK_SPEED 0.59999996f

static const S2HModApi* sApi;

struct EnMb;
typedef void (*EnMbActionFunc)(struct EnMb*, PlayState*);

typedef struct EnMb {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[MB_LIMB_MAX];
    Vec3s morphTable[MB_LIMB_MAX];
    EnMbActionFunc actionFunc;
    ColliderCylinder hitbox;
    ColliderQuad attackCollider;
    ColliderTris frontShielding;
    ColliderTrisElement frontShieldingTris[2];
    Vec3f bodyPartsPos[MOBLIN_BODYPART_COUNT];
    Vec3f effSpawnPos;
    f32 maxHomeDist;
    f32 playerDetectionRange;
    f32 drawDmgEffAlpha;
    s32 state;
    s16 timer1;
    s16 timer2;
    s16 timer3;
    s16 attack;
    u8 damageEffect;
    u8 isClub;
} EnMb;

static ColliderCylinderInit sHitboxInit = {
    {
        COL_MATERIAL_HIT0,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
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

static ColliderTrisElementInit sFrontShieldingElementsInit[2] = {
    {
        {
            ELEM_MATERIAL_UNK2,
            { 0x00000000, 0x00, 0x00 },
            { 0xF7CFFFFF, 0x00, 0x00 },
            ATELEM_NONE,
            ACELEM_ON | ACELEM_HOOKABLE | ACELEM_NO_AT_INFO,
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
            ACELEM_ON | ACELEM_HOOKABLE | ACELEM_NO_AT_INFO,
            OCELEM_NONE,
        },
        { { { -10.0f, -6.0f, 2.0f }, { 9.0f, -6.0f, 2.0f }, { 9.0f, 14.0f, 2.0f } } },
    },
};

static ColliderTrisInit sFrontShieldingInit = {
    {
        COL_MATERIAL_METAL,
        AT_NONE,
        AC_ON | AC_HARD | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_TRIS,
    },
    2,
    sFrontShieldingElementsInit,
};

static ColliderQuadInit sAttackColliderInit = {
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
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static DamageTable sSpearDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, MB_DMGEFF_FREEZE),
    /* Deku Stick     */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Horse trample  */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Explosives     */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Zora boomerang */ DMG_ENTRY(0, MB_DMGEFF_STUN),
    /* Normal arrow   */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Hookshot       */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Goron punch    */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Sword          */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Goron pound    */ DMG_ENTRY(2, MB_DMGEFF_DEFAULT),
    /* Fire arrow     */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Ice arrow      */ DMG_ENTRY(2, MB_DMGEFF_STUN_ICE),
    /* Light arrow    */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Goron spikes   */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Deku spin      */ DMG_ENTRY(0, MB_DMGEFF_STUN),
    /* Deku bubble    */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Deku launch    */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Zora barrier   */ DMG_ENTRY(0, MB_DMGEFF_STUN),
    /* Normal shield  */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Light ray      */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Thrown object  */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Zora punch     */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Spin attack    */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Sword beam     */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Normal Roll    */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Unblockable    */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Powder Keg     */ DMG_ENTRY(2, MB_DMGEFF_DEFAULT),
};

static DamageTable sClubDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, MB_DMGEFF_FREEZE),
    /* Deku Stick     */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Horse trample  */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Explosives     */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Zora boomerang */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Normal arrow   */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Hookshot       */ DMG_ENTRY(0, MB_DMGEFF_STUN),
    /* Goron punch    */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Sword          */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Goron pound    */ DMG_ENTRY(2, MB_DMGEFF_DEFAULT),
    /* Fire arrow     */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Ice arrow      */ DMG_ENTRY(2, MB_DMGEFF_STUN_ICE),
    /* Light arrow    */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Goron spikes   */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Deku spin      */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Deku bubble    */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Deku launch    */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Zora barrier   */ DMG_ENTRY(0, MB_DMGEFF_STUN),
    /* Normal shield  */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Light ray      */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Thrown object  */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Zora punch     */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Spin attack    */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Sword beam     */ DMG_ENTRY(1, MB_DMGEFF_DEFAULT),
    /* Normal Roll    */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Unblockable    */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, MB_DMGEFF_IGNORE),
    /* Powder Keg     */ DMG_ENTRY(2, MB_DMGEFF_DEFAULT),
};

static InitChainEntry sInitChain[] = {
    ICHAIN_F32_DIV1000(gravity, -1000, ICHAIN_CONTINUE),
    ICHAIN_F32(lockOnArrowOffset, 5300, ICHAIN_STOP),
};

static void EnMb_SetupSpearGuardWalk(EnMb* this);
static void EnMb_SetupClubWaitPlayerNear(EnMb* this);

static s16 AbsYawDiff(s16 a, s16 b) {
    s16 diff = a - b;

    return ABS(diff);
}

static bool EnMb_HasStepped(EnMb* this, s32 prevFrame, s32 step) {
    f32 playSpeed = ABS(this->skelAnime.playSpeed);
    s32 beforeCurFrame = this->skelAnime.curFrame - playSpeed;

    return (beforeCurFrame <= step) && ((prevFrame + (s32)playSpeed) >= step);
}

static void EnMb_PlayWalkSfx(EnMb* this, s32 prevFrame) {
    if (prevFrame == (s32)this->skelAnime.curFrame) {
        return;
    }
    if (EnMb_HasStepped(this, prevFrame, 1) || EnMb_HasStepped(this, prevFrame, 20)) {
        Actor_PlaySfx(&this->actor, MB_SFX_WALK);
    }
}

static void EnMb_SpawnChargeDust(EnMb* this, PlayState* play) {
    Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, 5.0f, 3, 4.0f, 100, 15, false);
}

static void EnMb_SpawnDeathPuffs(PlayState* play, Vec3f* base, f32 spread, s16 scale) {
    static Vec3f sZero = { 0.0f, 0.0f, 0.0f };
    static Color_RGBA8 sPrimColor = { 255, 255, 255, 255 };
    static Color_RGBA8 sEnvColor = { 0, 255, 0, 255 };

    for (s32 i = 4; i >= 0; i--) {
        Vec3f pos;
        pos.x = Rand_CenteredFloat(spread) + base->x;
        pos.y = Rand_CenteredFloat(15.0f) + (base->y + 20.0f);
        pos.z = Rand_CenteredFloat(spread) + base->z;
        EffectSsDeadDb_Spawn(play, &pos, &sZero, &sZero, &sPrimColor, &sEnvColor, scale, 7, 9);
    }
}

static void EnMb_Thaw(EnMb* this, PlayState* play) {
    if (this->drawDmgEffAlpha == 0.0f) {
        return;
    }
    this->drawDmgEffAlpha = 0.0f;
    Actor_SpawnIceEffects(play, &this->actor, this->bodyPartsPos, MOBLIN_BODYPART_COUNT, 2, this->isClub ? 0.6f : 0.4f,
                          0.3f);
}

static void EnMb_TurnClubTowardPlayer(EnMb* this) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 3, 100, 0);
    Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 3, 100, 0);
}

static void EnMb_SpearGuardLookAround(EnMb* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer1 != 0) {
        this->timer1--;
    }
    if ((this->timer1 == 0) && Animation_OnFrame(&this->skelAnime, 0.0f)) {
        EnMb_SetupSpearGuardWalk(this);
    }
}

static void EnMb_SetupSpearGuardLookAround(EnMb* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sSpearLookAroundAnim), -4.0f);
    this->actor.speed = 0.0f;
    this->timer1 = Rand_S16Offset(30, 50);
    this->state = MB_STATE_IDLE;
    this->actionFunc = EnMb_SpearGuardLookAround;
}

static void EnMb_SpearEndChargeQuick(EnMb* this, PlayState* play) {
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 0.5f, 1.0f, 0.0f);
    if (this->actor.speed > 1.0f) {
        EnMb_SpawnChargeDust(this, play);
    }
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (this->timer1 != 0) {
        EnMb_SetupSpearGuardWalk(this);
        this->timer1 = 80;
        this->timer2 = 80;
        this->timer3 = 80;
        return;
    }
    this->timer3--;
    if (this->timer3 == 0) {
        // The charge animation in reverse lets go of the spear.
        Animation_Change(&this->skelAnime, ANIM(sSpearPrepareChargeAnim), -1.0f, LAST_FRAME(sSpearPrepareChargeAnim),
                         0.0f, ANIMMODE_ONCE, 0.0f);
        this->timer1 = 1;
        this->actor.speed = 0.0f;
        Actor_PlaySfx(&this->actor, MB_SFX_SPEAR_READY);
    }
}

static void EnMb_SetupSpearEndChargeQuick(EnMb* this) {
    Animation_PlayOnce(&this->skelAnime, ANIM(sSpearSlowDownAnim));
    this->state = MB_STATE_ATTACK_END;
    this->timer1 = 0;
    this->timer3 = 5;
    Actor_PlaySfx(&this->actor, MB_SFX_SLIDE);
    this->actionFunc = EnMb_SpearEndChargeQuick;
}

static void EnMb_SpearGuardPrepareAndCharge(EnMb* this, PlayState* play) {
    s16 relYawToPlayer = AbsYawDiff(this->actor.yawTowardsPlayer, this->actor.shape.rot.y);
    s32 prevFrame = this->skelAnime.curFrame;

    if (SkelAnime_Update(&this->skelAnime)) {
        Animation_PlayLoop(&this->skelAnime, ANIM(sSpearChargeAnim));
        Actor_PlaySfx(&this->actor, MB_SFX_ATTACK);
    }
    if (this->timer3 != 0) {
        this->timer3--;
        Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 1, 0xBB8, 0);
    } else {
        this->actor.speed = 10.0f;
        this->attack = MB_ATTACK_SPEAR;
        EnMb_SpawnChargeDust(this, play);
        s32 frame = this->skelAnime.curFrame;
        if ((prevFrame != frame) && ((frame == 2) || (frame == 6))) {
            Actor_PlaySfx(&this->actor, MB_SFX_DASH);
        }
    }
    if (relYawToPlayer > 0x1388) {
        this->attack = MB_ATTACK_NONE;
        EnMb_SetupSpearEndChargeQuick(this);
    }
}

static void EnMb_SetupSpearPrepareAndCharge(EnMb* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sSpearPrepareChargeAnim), -4.0f);
    this->state = MB_STATE_ATTACK;
    this->actor.speed = 0.0f;
    this->timer3 = (s16)LAST_FRAME(sSpearPrepareChargeAnim) + 6;
    Actor_PlaySfx(&this->actor, MB_SFX_SPEAR_READY);
    this->actionFunc = EnMb_SpearGuardPrepareAndCharge;
}

static void EnMb_WanderAroundHome(EnMb* this) {
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    bool isFarFromHome = Math_Vec3f_DistXZ(&this->actor.world.pos, &this->actor.home.pos) > this->maxHomeDist;
    if (isFarFromHome || (this->timer2 != 0)) {
        s16 yawTowardsHome = Math_Vec3f_Yaw(&this->actor.world.pos, &this->actor.home.pos);
        Math_SmoothStepToS(&this->actor.world.rot.y, yawTowardsHome, 1, 0x2EE, 0);
    }
    if (this->timer2 != 0) {
        this->timer2--;
    }
    if (this->timer3 != 0) {
        this->timer3--;
    }
    if (this->timer2 == 0) {
        Actor_PlaySfx(&this->actor, MB_SFX_VOICE);
    }
    this->timer1--;
    if (this->timer1 != 0) {
        return;
    }
    if (Rand_ZeroOne() > 0.7f) {
        this->timer1 = Rand_S16Offset(50, 70);
        this->timer2 = Rand_S16Offset(15, 40);
    } else {
        EnMb_SetupSpearGuardLookAround(this);
    }
}

static void EnMb_SpearGuardWalk(EnMb* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 relYawToPlayer = AbsYawDiff(this->actor.yawTowardsPlayer, this->actor.shape.rot.y);

    Math_SmoothStepToF(&this->actor.speed, MOBLIN_SPEAR_WALK_SPEED, 0.1f, 1.0f, 0.0f);
    this->skelAnime.playSpeed = this->actor.speed;
    s32 prevFrame = this->skelAnime.curFrame;
    SkelAnime_Update(&this->skelAnime);
    bool isPlayerNearPost =
        Math_Vec3f_DistXZ(&this->actor.home.pos, &player->actor.world.pos) < this->playerDetectionRange;
    if ((this->timer3 == 0) && isPlayerNearPost) {
        Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 1, 0x2EE, 0);
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        if ((this->actor.xzDistToPlayer < 500.0f) && (relYawToPlayer < 0x1388)) {
            EnMb_SetupSpearPrepareAndCharge(this);
        }
    } else {
        EnMb_WanderAroundHome(this);
    }
    EnMb_PlayWalkSfx(this, prevFrame);
    this->actor.shape.rot.y = this->actor.world.rot.y;
}

static void EnMb_SetupSpearGuardWalk(EnMb* this) {
    Animation_Change(&this->skelAnime, ANIM(sSpearWalkAnim), 0.0f, 0.0f, LAST_FRAME(sSpearWalkAnim), ANIMMODE_LOOP,
                     -4.0f);
    this->actor.speed = MOBLIN_SPEAR_WALK_SPEED;
    this->timer1 = Rand_S16Offset(50, 70);
    this->state = MB_STATE_WALK;
    this->actionFunc = EnMb_SpearGuardWalk;
}

static void EnMb_SpearDamaged(EnMb* this, PlayState* play) {
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 1.0f, 0.5f, 0.0f);
    if (SkelAnime_Update(&this->skelAnime)) {
        EnMb_SetupSpearGuardLookAround(this);
    }
}

static void EnMb_SetupSpearDamaged(EnMb* this) {
    if (AbsYawDiff(this->actor.yawTowardsPlayer, this->actor.shape.rot.y) <= 0x4000) {
        Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sSpearDamagedFromFrontAnim), -4.0f);
        this->actor.speed = -8.0f;
    } else {
        Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sSpearDamagedFromBehindAnim), -4.0f);
        this->actor.speed = 8.0f;
    }
    this->timer1 = 30;
    this->state = MB_STATE_SPEAR_DAMAGED;
    this->actor.shape.rot.y = this->actor.world.rot.y;
    Actor_PlaySfx(&this->actor, MB_SFX_DEAD);
    this->actionFunc = EnMb_SpearDamaged;
}

static void EnMb_SpearDead(EnMb* this, PlayState* play) {
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 1.0f, 0.5f, 0.0f);
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (this->timer1 <= 0) {
        Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xE0);
        Actor_Kill(&this->actor);
        return;
    }
    this->actor.shape.shadowScale = 0.0f;
    this->timer1--;
    EnMb_SpawnDeathPuffs(play, &this->actor.world.pos, 110.0f, 100);
}

static void EnMb_SetupSpearDead(EnMb* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sSpearFallOnItsBackAnim), -4.0f);
    bool isPlayerInFront = AbsYawDiff(this->actor.yawTowardsPlayer, this->actor.shape.rot.y) <= 0x4000;
    this->actor.speed = isPlayerInFront ? -8.0f : 8.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->timer1 = 30;
    this->state = MB_STATE_SPEAR_DAMAGED;
    Actor_PlaySfx(&this->actor, MB_SFX_DEAD);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actionFunc = EnMb_SpearDead;
}

static void EnMb_ClubWaitAfterAttack(EnMb* this, PlayState* play) {
    EnMb_TurnClubTowardPlayer(this);
    this->attack = MB_ATTACK_NONE;
    if (SkelAnime_Update(&this->skelAnime)) {
        EnMb_SetupClubWaitPlayerNear(this);
    }
}

static void EnMb_SetupClubWaitAfterAttack(EnMb* this) {
    this->state = MB_STATE_ATTACK_END;
    Animation_Change(&this->skelAnime, ANIM(sClubStandStillClubDownAnim), 5.0f, 0.0f,
                     LAST_FRAME(sClubStandStillClubDownAnim), ANIMMODE_ONCE_INTERP, 0.0f);
    this->actionFunc = EnMb_ClubWaitAfterAttack;
}

static void EnMb_SpawnClubDust(PlayState* play, Vec3f* base) {
    static Vec3f sVelocity = { 0.0f, 0.0f, 0.0f };
    static Vec3f sAccel = { 0.0f, 0.3f, 0.0f };
    static Color_RGBA8 sPrimColor = { 170, 130, 90, 255 };
    static Color_RGBA8 sEnvColor = { 100, 60, 20, 255 };

    for (s32 i = 3; i >= 0; i--) {
        Vec3f pos;
        pos.x = base->x + ((Rand_ZeroOne() - 0.5f) * 2.0f);
        pos.y = base->y + ((Rand_ZeroOne() - 0.5f) * 2.0f);
        pos.z = base->z + ((Rand_ZeroOne() - 0.5f) * 2.0f);
        s16 scale = (s16)((Rand_ZeroOne() * 300.0f) * 0.2f) + 300;
        EffectSsDust_Spawn(play, 5, &pos, &sVelocity, &sAccel, &sPrimColor, &sEnvColor, scale, 180, 10, 0);
    }
}

// The smash only shoves the player, but a player still flinching from a hit takes the club's damage.
static void EnMb_ShoveHitPlayer(EnMb* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!(this->attackCollider.base.atFlags & AT_HIT)) {
        return;
    }
    this->attackCollider.base.atFlags &= ~AT_HIT;
    if (this->attackCollider.base.at != &player->actor) {
        return;
    }
    s8 prevInvincibilityTimer = player->invincibilityTimer;
    if (player->invincibilityTimer < 0) {
        bool isStillFlinching = player->invincibilityTimer > -40;
        player->invincibilityTimer = 0;
        if (isStillFlinching) {
            play->damagePlayer(play, -8);
        }
    }
    func_800B8E1C(play, &this->actor, (650.0f - this->actor.xzDistToPlayer) * 0.04f + 4.0f, this->actor.world.rot.y,
                  8.0f);
    player->invincibilityTimer = prevInvincibilityTimer;
}

static void EnMb_LandClub(EnMb* this, PlayState* play) {
    static Vec3f sZero = { 0.0f, 0.0f, 0.0f };
    Vec3f landPos = this->effSpawnPos;

    landPos.y = this->actor.floorHeight;
    Actor_PlaySfx(&this->actor, MB_SFX_CLUB_LAND);
    Rumble_Request(this->actor.xzDistToPlayer, 0xFF, 0x14, 0x96);
    EffectSsBlast_SpawnWhiteShockwave(play, &landPos, &sZero, &sZero);
    EnMb_SpawnClubDust(play, &landPos);
    EnMb_SetupClubWaitAfterAttack(this);
}

static void EnMb_ClubAttack(EnMb* this, PlayState* play) {
    EnMb_TurnClubTowardPlayer(this);
    EnMb_ShoveHitPlayer(this, play);
    if (!SkelAnime_Update(&this->skelAnime)) {
        if ((this->timer3 != 0) && (this->skelAnime.curFrame == 6.0f)) {
            Actor_PlaySfx(&this->actor, NA_SE_EN_MONBLIN_HAM_UP);
        } else if ((this->timer3 == 0) && (this->skelAnime.curFrame == 3.0f)) {
            Actor_PlaySfx(&this->actor, NA_SE_EN_MONBLIN_HAM_DOWN);
        }
        return;
    }
    if (this->timer3 == 0) {
        EnMb_LandClub(this, play);
        return;
    }
    this->timer3--;
    if (this->timer3 == 0) {
        Animation_Change(&this->skelAnime, ANIM(sClubStrikeDownAnim), 1.5f, 0.0f, LAST_FRAME(sClubStrikeDownAnim),
                         ANIMMODE_ONCE_INTERP, 0.0f);
    }
}

static void EnMb_SetupClubAttack(EnMb* this) {
    EnMb_TurnClubTowardPlayer(this);
    this->state = MB_STATE_ATTACK;
    Animation_Change(&this->skelAnime, ANIM(sClubLiftClubAnim), 3.0f, 0.0f, LAST_FRAME(sClubLiftClubAnim),
                     ANIMMODE_ONCE_INTERP, 0.0f);
    this->timer3 = 1;
    s16 relYawFromPlayer = this->actor.world.rot.y - this->actor.yawTowardsPlayer;
    if (ABS(relYawFromPlayer) <= 0x258) {
        this->attack = MB_ATTACK_CLUB_MIDDLE;
    } else if (relYawFromPlayer >= 0) {
        this->attack = MB_ATTACK_CLUB_RIGHT;
    } else {
        this->attack = MB_ATTACK_CLUB_LEFT;
    }
    this->actionFunc = EnMb_ClubAttack;
}

static void EnMb_ClubWaitPlayerNear(EnMb* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 relYawFromPlayer = AbsYawDiff(this->actor.world.rot.y, this->actor.yawTowardsPlayer);

    EnMb_TurnClubTowardPlayer(this);
    SkelAnime_Update(&this->skelAnime);
    bool isPlayerNear = Math_Vec3f_DistXZ(&this->actor.home.pos, &player->actor.world.pos) < this->playerDetectionRange;
    bool isPlayerFlinching = (player->stateFlags1 & PLAYER_STATE1_4000000) != 0;
    bool isPlayerLevel = ABS(this->actor.playerHeightRel) <= 100.0f;
    if (isPlayerNear && !isPlayerFlinching && (relYawFromPlayer < 0x3E80) && isPlayerLevel) {
        EnMb_SetupClubAttack(this);
    }
}

static void EnMb_SetupClubWaitPlayerNear(EnMb* this) {
    Animation_PlayLoop(&this->skelAnime, ANIM(sClubStandStillClubDownAnim));
    this->actor.speed = 0.0f;
    this->timer1 = Rand_S16Offset(30, 50);
    this->state = MB_STATE_IDLE;
    this->actionFunc = EnMb_ClubWaitPlayerNear;
}

static void EnMb_ClubDamaged(EnMb* this, PlayState* play) {
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (this->timer3 == 0) {
        EnMb_SetupClubWaitPlayerNear(this);
        return;
    }
    Animation_PlayOnce(&this->skelAnime, ANIM(sClubStandUpAnim));
    this->timer3 = 0;
    Rumble_Request(this->actor.xzDistToPlayer, 0xFF, 0x14, 0x96);
}

static void EnMb_SetupClubDamaged(EnMb* this) {
    Animation_PlayOnce(&this->skelAnime, ANIM(sClubDamagedKneelAnim));
    this->state = MB_STATE_CLUB_KNEELING;
    this->timer1 = 0;
    this->timer3 = 20;
    Actor_PlaySfx(&this->actor, MB_SFX_DEAD);
    this->actionFunc = EnMb_ClubDamaged;
}

static void EnMb_ClubDead(EnMb* this, PlayState* play) {
    Vec3f effPos = this->actor.world.pos;

    effPos.x += Math_SinS(this->actor.shape.rot.y) * -70.0f;
    effPos.z += Math_CosS(this->actor.shape.rot.y) * -70.0f;
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 1.0f, 0.5f, 0.0f);
    if (SkelAnime_Update(&this->skelAnime)) {
        if (this->timer1 <= 0) {
            Item_DropCollectibleRandom(play, &this->actor, &effPos, 0xC0);
            Actor_Kill(&this->actor);
            return;
        }
        this->timer1--;
        EnMb_SpawnDeathPuffs(play, &effPos, 240.0f, 230);
        return;
    }
    s32 frame = this->skelAnime.curFrame;
    if ((frame == 15) || (frame == 22)) {
        Rumble_Request(this->actor.xzDistToPlayer, 0xFF, 0x14, 0x96);
        Actor_SpawnFloorDustRing(play, &this->actor, &effPos, 50.0f, 10, 3.0f, 400, 60, false);
        Actor_PlaySfx(&this->actor, NA_SE_EN_GERUDOFT_DOWN);
    }
}

static void EnMb_SetupClubDead(EnMb* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sClubFallOnItsBackAnim), -4.0f);
    this->state = MB_STATE_CLUB_DEAD;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->hitbox.dim.height = 80;
    this->hitbox.dim.radius = 95;
    this->timer1 = 30;
    this->actor.speed = 0.0f;
    Actor_PlaySfx(&this->actor, MB_SFX_DEAD);
    this->actionFunc = EnMb_ClubDead;
}

static void EnMb_Stunned(EnMb* this, PlayState* play) {
    if (this->actor.colorFilterTimer != 0) {
        return;
    }
    EnMb_Thaw(this, play);
    bool isDead = this->actor.colChkInfo.health == 0;
    if (this->isClub && isDead) {
        EnMb_SetupClubDead(this);
    } else if (this->isClub) {
        EnMb_SetupClubWaitPlayerNear(this);
    } else if (isDead) {
        EnMb_SetupSpearDead(this);
    } else {
        EnMb_SetupSpearDamaged(this);
    }
}

static void EnMb_SetupStunned(EnMb* this) {
    bool isIced = this->damageEffect == MB_DMGEFF_STUN_ICE;
    u16 frames = isIced ? (MOBLIN_STUN_FRAMES + MOBLIN_THAW_FRAMES) : MOBLIN_STUN_FRAMES;

    this->state = MB_STATE_STUNNED;
    this->actor.speed = 0.0f;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, frames);
    if (isIced) {
        this->drawDmgEffAlpha = 1.0f;
    } else {
        if (!this->isClub) {
            Animation_PlayOnceSetSpeed(&this->skelAnime, ANIM(sSpearDamagedFromFrontAnim), 0.0f);
        }
        Actor_PlaySfx(&this->actor, NA_SE_EN_COMMON_FREEZE);
    }
    this->actionFunc = EnMb_Stunned;
}

static void EnMb_TakeDamage(EnMb* this, PlayState* play) {
    Actor_ApplyDamage(&this->actor);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 250, COLORFILTER_BUFFLAG_OPA, 12);
    bool isDead = this->actor.colChkInfo.health == 0;
    if (isDead) {
        Enemy_StartFinishingBlow(play, &this->actor);
    }
    if (this->isClub && isDead) {
        EnMb_SetupClubDead(this);
    } else if (this->isClub && (this->state != MB_STATE_CLUB_KNEELING)) {
        EnMb_SetupClubDamaged(this);
    } else if (!this->isClub && isDead) {
        EnMb_SetupSpearDead(this);
    } else if (!this->isClub) {
        EnMb_SetupSpearDamaged(this);
    }
}

static void EnMb_UpdateDamage(EnMb* this, PlayState* play) {
    if (this->frontShielding.base.acFlags & AC_HIT) {
        this->frontShielding.base.acFlags &= ~(AC_HIT | AC_BOUNCED);
        this->hitbox.base.acFlags &= ~AC_HIT;
        return;
    }
    if (!(this->hitbox.base.acFlags & AC_HIT) || (this->state < MB_STATE_STUNNED)) {
        return;
    }
    this->hitbox.base.acFlags &= ~AC_HIT;
    u8 effect = this->actor.colChkInfo.damageEffect;
    if ((effect == MB_DMGEFF_IGNORE) || (effect == MB_DMGEFF_FREEZE)) {
        return;
    }
    this->damageEffect = effect;
    this->attack = MB_ATTACK_NONE;
    Actor_SetDropFlag(&this->actor, &this->hitbox.elem);
    if ((effect != MB_DMGEFF_STUN) && (effect != MB_DMGEFF_STUN_ICE)) {
        EnMb_TakeDamage(this, play);
        return;
    }
    if (this->state != MB_STATE_STUNNED) {
        Actor_ApplyDamage(&this->actor);
        EnMb_SetupStunned(this);
    }
}

static void EnMb_UpdateCollision(EnMb* this, PlayState* play) {
    CollisionCheckContext* colChkCtx = &play->colChkCtx;

    Collider_UpdateCylinder(&this->actor, &this->hitbox);
    if (this->actor.colChkInfo.health <= 0) {
        this->hitbox.dim.pos.x += Math_SinS(this->actor.shape.rot.y) * (-4400.0f * this->actor.scale.y);
        this->hitbox.dim.pos.z += Math_CosS(this->actor.shape.rot.y) * (-4400.0f * this->actor.scale.y);
    }
    CollisionCheck_SetOC(play, colChkCtx, &this->hitbox.base);
    if ((this->state >= MB_STATE_STUNNED) && (this->isClub || (this->state != MB_STATE_ATTACK))) {
        CollisionCheck_SetAC(play, colChkCtx, &this->hitbox.base);
    }
    if (!this->isClub && (this->state >= MB_STATE_IDLE)) {
        CollisionCheck_SetAC(play, colChkCtx, &this->frontShielding.base);
    }
    if (this->attack > MB_ATTACK_NONE) {
        CollisionCheck_SetAT(play, colChkCtx, &this->attackCollider.base);
    }
}

static void EnMb_Update(Actor* thisx, PlayState* play) {
    EnMb* this = (EnMb*)thisx;

    EnMb_UpdateDamage(this, play);
    // A nut's freeze effect skips this frame entirely, as vanilla does.
    if (this->actor.colChkInfo.damageEffect == MB_DMGEFF_FREEZE) {
        return;
    }
    this->actionFunc(this, play);
    Actor_MoveWithGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 40.0f, 40.0f, 70.0f, MOBLIN_BGCHECK_FLAGS);
    Actor_SetFocus(&this->actor, this->actor.scale.x * 4500.0f);
    EnMb_UpdateCollision(this, play);
}

static void EnMb_InitColliders(EnMb* this, PlayState* play) {
    Collider_InitCylinder(play, &this->hitbox);
    Collider_SetCylinder(play, &this->hitbox, &this->actor, &sHitboxInit);
    Collider_InitTris(play, &this->frontShielding);
    Collider_SetTris(play, &this->frontShielding, &this->actor, &sFrontShieldingInit, this->frontShieldingTris);
    Collider_InitQuad(play, &this->attackCollider);
    Collider_SetQuad(play, &this->attackCollider, &this->actor, &sAttackColliderInit);
}

static void EnMb_SpearInit(Actor* thisx, PlayState* play) {
    EnMb* this = (EnMb*)thisx;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 46.0f);
    this->actor.attentionRangeType = ATTENTION_RANGE_3;
    this->actor.colChkInfo.damageTable = &sSpearDamageTable;
    EnMb_InitColliders(this, play);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSpearSkel, ANIM(sSpearStandStillAnim),
                       this->jointTable, this->morphTable, MB_LIMB_MAX);
    this->actor.colChkInfo.health = 2;
    this->actor.colChkInfo.mass = MASS_HEAVY;
    this->maxHomeDist = 1000.0f;
    this->playerDetectionRange = 1750.0f;
    EnMb_SetupSpearGuardLookAround(this);
}

static void EnMb_ClubInit(Actor* thisx, PlayState* play) {
    EnMb* this = (EnMb*)thisx;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawFeet, 90.0f);
    this->isClub = true;
    this->actor.colChkInfo.damageTable = &sClubDamageTable;
    EnMb_InitColliders(this, play);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sClubSkel, ANIM(sClubStandStillClubDownAnim),
                       this->jointTable, this->morphTable, MB_LIMB_MAX);
    this->actor.colChkInfo.health = 6;
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    Actor_SetScale(&this->actor, 0.02f);
    this->hitbox.dim.height = 170;
    this->hitbox.dim.radius = 45;
    this->actor.cullingVolumeDistance = 4000.0f;
    this->actor.cullingVolumeScale = 800.0f;
    this->actor.cullingVolumeDownward = 1800.0f;
    this->playerDetectionRange = 710.0f;
    this->attackCollider.elem.atDmgInfo.dmgFlags = DMG_UNBLOCKABLE;
    EnMb_SetupClubWaitPlayerNear(this);
}

static void EnMb_Destroy(Actor* thisx, PlayState* play) {
    EnMb* this = (EnMb*)thisx;

    Collider_DestroyTris(play, &this->frontShielding);
    Collider_DestroyCylinder(play, &this->hitbox);
    Collider_DestroyQuad(play, &this->attackCollider);
}

static void EnMb_UpdateSpearAttackCollider(EnMb* this) {
    static Vec3f sQuad0 = { 1000.0f, 1500.0f, 0.0f };
    static Vec3f sQuad1 = { -1000.0f, 1500.0f, 0.0f };
    static Vec3f sQuad2 = { 1000.0f, 1500.0f, 4500.0f };
    static Vec3f sQuad3 = { -1000.0f, 1500.0f, 4500.0f };
    ColliderQuadDim* quad = &this->attackCollider.dim;

    Matrix_MultVec3f(&sQuad0, &quad->quad[1]);
    Matrix_MultVec3f(&sQuad1, &quad->quad[0]);
    Matrix_MultVec3f(&sQuad2, &quad->quad[3]);
    Matrix_MultVec3f(&sQuad3, &quad->quad[2]);
    Collider_SetQuadVertices(&this->attackCollider, &quad->quad[0], &quad->quad[1], &quad->quad[2], &quad->quad[3]);
}

static void EnMb_UpdateClubAttackCollider(EnMb* this) {
    static Vec3f sQuad[] = {
        { 1000.0f, 0.0f, 0.0f },
        { 1000.0f, 0.0f, 0.0f },
        { 1000.0f, -8000.0f, -1500.0f },
        { 1000.0f, -9000.0f, 2000.0f },
    };
    ColliderQuadDim* quad = &this->attackCollider.dim;

    Matrix_MultVec3f(&sQuad[0], &quad->quad[1]);
    Matrix_MultVec3f(&sQuad[1], &quad->quad[0]);
    Matrix_MultVec3f(&sQuad[2], &quad->quad[3]);
    Matrix_MultVec3f(&sQuad[3], &quad->quad[2]);
    Collider_SetQuadVertices(&this->attackCollider, &quad->quad[0], &quad->quad[1], &quad->quad[2], &quad->quad[3]);
}

static void EnMb_UpdateFrontShielding(EnMb* this) {
    static Vec3f sTri0[] = {
        { 4000.0f, 7000.0f, 3500.0f },
        { 4000.0f, 0.0f, 3500.0f },
        { -4000.0f, 7000.0f, 3500.0f },
    };
    static Vec3f sTri1[] = {
        { -4000.0f, 7000.0f, 3500.0f },
        { -4000.0f, 0.0f, 3500.0f },
        { 4000.0f, 0.0f, 3500.0f },
    };
    Vec3f tri0[3];
    Vec3f tri1[3];

    for (s32 i = 0; i < 3; i++) {
        Matrix_MultVec3f(&sTri0[i], &tri0[i]);
        Matrix_MultVec3f(&sTri1[i], &tri1[i]);
    }
    Collider_SetTrisVertices(&this->frontShielding, 0, &tri0[0], &tri0[1], &tri0[2]);
    Collider_SetTrisVertices(&this->frontShielding, 1, &tri1[0], &tri1[1], &tri1[2]);
}

static s32 EnMb_GetBodyPart(s32 limbIndex) {
    switch (limbIndex) {
        case MB_LIMB_HEAD:
            return 0;
        case MB_LIMB_LHAND:
            return 1;
        case MB_LIMB_RHAND:
            return 2;
        case MB_LIMB_LSHOULDER:
            return 3;
        case MB_LIMB_RSHOULDER:
            return 4;
        case MB_LIMB_CHEST:
            return 5;
        case MB_LIMB_LTHIGH:
            return 6;
        case MB_LIMB_RTHIGH:
            return 7;
        case MB_LIMB_LFOOT:
            return 8;
        case MB_LIMB_RFOOT:
            return 9;
        default:
            return -1;
    }
}

static void EnMb_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    static Vec3f sOrigin = { 0.0f, 0.0f, 0.0f };
    static Vec3f sClubTipOffset = { 0.0f, -8000.0f, 0.0f };
    EnMb* this = (EnMb*)thisx;

    if (this->isClub) {
        if (limbIndex == MB_LIMB_LHAND) {
            Matrix_MultVec3f(&sClubTipOffset, &this->effSpawnPos);
            if (this->attack > MB_ATTACK_NONE) {
                EnMb_UpdateClubAttackCollider(this);
            }
        }
        Actor_SetFeetPos(&this->actor, limbIndex, MB_LIMB_LFOOT, &sOrigin, MB_LIMB_RFOOT, &sOrigin);
    }
    s32 bodyPart = EnMb_GetBodyPart(limbIndex);
    if (bodyPart >= 0) {
        Matrix_MultVec3f(&sOrigin, &this->bodyPartsPos[bodyPart]);
    }
}

static void EnMb_Draw(Actor* thisx, PlayState* play) {
    EnMb* this = (EnMb*)thisx;

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          EnMb_PostLimbDraw, &this->actor);
    if (!this->isClub) {
        if (this->attack > MB_ATTACK_NONE) {
            EnMb_UpdateSpearAttackCollider(this);
        }
        EnMb_UpdateFrontShielding(this);
    }
    f32 iceScale = this->isClub ? 1.0f : 0.6f;
    Actor_DrawDamageEffects(play, &this->actor, this->bodyPartsPos, MOBLIN_BODYPART_COUNT, iceScale, iceScale * 1.5f,
                            this->drawDmgEffAlpha, ACTOR_DRAW_DMGEFF_FROZEN_NO_SFX);
}

// Ship leaves Moblins out of logic; these are the weapons their damage tables accept.
static bool CanKillSpearMoblin(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RI_DEKU_STICK) ||
           sApi->LogicHasCapability("CanBeGoron") || sApi->LogicHasCapability("CanBeZora") ||
           sApi->LogicHasCapability("HasExplosives") || sApi->LogicCanUseItem(RI_BOW) ||
           sApi->LogicCanUseItem(RI_HOOKSHOT);
}

static bool CanKillClubMoblin(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RI_DEKU_STICK) ||
           sApi->LogicHasCapability("CanBeGoron") || sApi->LogicHasCapability("CanBeZora") ||
           sApi->LogicHasCapability("HasExplosives") || sApi->LogicCanUseItem(RI_BOW);
}

static S2HActorDefinition DefineMoblin(const char* key, const char* description, u32 flags) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = key;
    definition.description = description;
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = flags;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnMb);
    definition.destroy = EnMb_Destroy;
    definition.update = EnMb_Update;
    definition.draw = EnMb_Draw;
    // Ship's catalog gives Moblins no soul, so they have none here either.
    definition.soulKey = NULL;
    return definition;
}

static s16 RegisterSpearMoblin(void) {
    S2HActorDefinition definition = DefineMoblin(MOBLIN_SPEAR_KEY, "Spear Moblin", MOBLIN_SPEAR_FLAGS);

    definition.init = EnMb_SpearInit;
    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    definition.naviHint = "Spear Moblin&A brutish guard that charges anything&it spots. Sidestep, then strike!";
    definition.logicKill = CanKillSpearMoblin;
    return sApi->RegisterActor(&definition);
}

static s16 RegisterClubMoblin(void) {
    S2HActorDefinition definition = DefineMoblin(MOBLIN_CLUB_KEY, "Club Moblin", MOBLIN_CLUB_FLAGS);

    definition.init = EnMb_ClubInit;
    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    definition.naviHint = "Club Moblin&A giant whose club sends shockwaves&along the ground. Get in close!";
    definition.logicKill = CanKillClubMoblin;
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
    if (!RequireOotAsset(MB_OBJECT "gEnMbSpearSkel") || !RequireOotAsset(MB_OBJECT "gEnMbClubSkel")) {
        sApi->Log(MOBLIN_SPEAR_KEY ": OoT Asset Loader resources are missing; extract them to enable Moblins.");
        return;
    }
    if (RegisterSpearMoblin() < 0) {
        sApi->Log("Could not register " MOBLIN_SPEAR_KEY);
    }
    if (RegisterClubMoblin() < 0) {
        sApi->Log("Could not register " MOBLIN_CLUB_KEY);
    }
}
