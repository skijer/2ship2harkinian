/*
 * Ocarina of Time's Dead Hand (En_Dh) and its hands (En_Dha) on Majora's Mask, drawn from the OoT Asset Loader's
 * oot.o2r. The body brings its own ring of hands and wakes on proximity too, so it works in any room.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define DEAD_HAND_KEY "oot.dead_hand"
#define DEAD_HAND_HAND_KEY "oot.dead_hand.hand"
#define DH_OBJECT "objects/oot_object_dh/"
#define DH_ASSET(name) "__OTR__" DH_OBJECT name

static const ALIGN_ASSET(2) char sBodySkel[] = DH_ASSET("object_dh_Skel_007E88");
static const ALIGN_ASSET(2) char sHandSkel[] = DH_ASSET("object_dh_Skel_000BD8");
static const ALIGN_ASSET(2) char sHandIdleAnim[] = DH_ASSET("object_dh_Anim_0015B0");
static const ALIGN_ASSET(2) char sBiteAnim[] = DH_ASSET("object_dh_Anim_001A3C");
static const ALIGN_ASSET(2) char sBurrowAnim[] = DH_ASSET("object_dh_Anim_002148");
static const ALIGN_ASSET(2) char sDeathAnim[] = DH_ASSET("object_dh_Anim_0032BC");
static const ALIGN_ASSET(2) char sDeadAnim[] = DH_ASSET("object_dh_Anim_00375C");
static const ALIGN_ASSET(2) char sWalkAnim[] = DH_ASSET("object_dh_Anim_003A8C");
static const ALIGN_ASSET(2) char sDamageAnim[] = DH_ASSET("object_dh_Anim_003D6C");
static const ALIGN_ASSET(2) char sLungeAnim[] = DH_ASSET("object_dh_Anim_004658");
static const ALIGN_ASSET(2) char sRetreatAnim[] = DH_ASSET("object_dh_Anim_005880");
static const ALIGN_ASSET(2) char sDirtWaveDL[] = DH_ASSET("object_dh_DL_007FC0");

#define ANIM(asset) ((AnimationHeader*)(asset))

#define DH_LIMB_COUNT 16
#define DH_HEAD_LIMB 13
#define DH_HAND_LIMB_COUNT 4
#define DH_HAND_COUNT 4
#define DH_HAND_RING_RADIUS 140.0f
#define DH_RELOCATE_RANGE 300.0f
#define DH_RELOCATE_TRIES 8
#define DH_FLOOR_STEP_MAX 40.0f
#define DH_WAKE_DISTANCE 150.0f
#define DH_WAKE_HEIGHT 100.0f
#define DH_REACH 100.0f
#define DH_FACING_ANGLE (60 * 0x10000 / 360)
#define DH_HEALTH 20
#define DH_HAND_HEALTH 8
#define DH_DMGEFF_HURT 0xF

#define DH_FLAGS                                                                              \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER)
#define DH_HAND_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED)

// The body's params double as its wake-up state: hands count down from 0 as they die, grabs and bombs set it.
typedef enum {
    DH_STATE_HANDS_KILLED_ALL = -DH_HAND_COUNT,
    DH_STATE_WAIT_UNDERGROUND = 0,
    DH_STATE_WAKE_BY_GRAB = 1,
    DH_STATE_WAKE_BY_BOMB = 5,
    DH_STATE_DEATH = 10,
} DeadHandState;

typedef enum {
    DH_ACTION_WAIT,
    DH_ACTION_RETREAT,
    DH_ACTION_BURROW,
    DH_ACTION_WALK,
    DH_ACTION_ATTACK,
    DH_ACTION_DEATH,
    DH_ACTION_DAMAGE,
} DeadHandAction;

static const S2HModApi* sApi;
static s16 sBodyActorId = -1;
static s16 sHandActorId = -1;

static Gfx sXluRenderModeDL[] = {
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU |
                                            FORCE_BL | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)),
    gsDPSetAlphaCompare(G_AC_THRESHOLD),
    gsSPEndDisplayList(),
};
static Gfx sEmptyDL[] = {
    gsSPEndDisplayList(),
};

struct EnDh;
typedef void (*EnDhActionFunc)(struct EnDh*, PlayState*);

typedef struct EnDh {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[DH_LIMB_COUNT];
    Vec3s morphTable[DH_LIMB_COUNT];
    EnDhActionFunc actionFunc;
    ColliderCylinder bodyCollider;
    ColliderJntSph headCollider;
    ColliderJntSphElement headElements[1];
    Vec3f headPos;
    f32 dirtWaveSpread;
    f32 dirtWaveHeight;
    f32 dirtWaveAlpha;
    s16 timer;
    s16 dirtWavePhase;
    u8 actionState;
    u8 retreat;
    u8 drawDirtWave;
    u8 lastHitCombo;
    u8 alpha;
    u8 curAction;
    u8 hasSpawnedHands;
    u8 isBgmPlaying;
} EnDh;

struct EnDha;
typedef void (*EnDhaActionFunc)(struct EnDha*, PlayState*);

typedef struct EnDha {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[DH_HAND_LIMB_COUNT];
    Vec3s morphTable[DH_HAND_LIMB_COUNT];
    EnDhaActionFunc actionFunc;
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[5];
    Vec3s handAngle;
    Vec3f handPos[2];
    Vec3f armPos;
    s16 actionTimer;
    s16 gripPhase;
    s16 limbAngleX[2];
    s16 limbAngleY;
    u8 ignoresHits;
    u8 isGripping;
} EnDha;

static DamageTable sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, 0x0),
    /* Deku Stick     */ DMG_ENTRY(1, DH_DMGEFF_HURT),
    /* Horse trample  */ DMG_ENTRY(0, 0x0),
    /* Explosives     */ DMG_ENTRY(0, 0x0),
    /* Zora boomerang */ DMG_ENTRY(0, 0x0),
    /* Normal arrow   */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, 0x0),
    /* Hookshot       */ DMG_ENTRY(0, 0x0),
    /* Goron punch    */ DMG_ENTRY(0, 0x0),
    /* Sword          */ DMG_ENTRY(2, DH_DMGEFF_HURT),
    /* Goron pound    */ DMG_ENTRY(1, DH_DMGEFF_HURT),
    /* Fire arrow     */ DMG_ENTRY(0, 0x0),
    /* Ice arrow      */ DMG_ENTRY(0, 0x0),
    /* Light arrow    */ DMG_ENTRY(0, 0x0),
    /* Goron spikes   */ DMG_ENTRY(0, 0x0),
    /* Deku spin      */ DMG_ENTRY(0, 0x0),
    /* Deku bubble    */ DMG_ENTRY(0, 0x0),
    /* Deku launch    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, 0x0),
    /* Zora barrier   */ DMG_ENTRY(0, 0x0),
    /* Normal shield  */ DMG_ENTRY(0, 0x0),
    /* Light ray      */ DMG_ENTRY(0, 0x0),
    /* Thrown object  */ DMG_ENTRY(0, 0x0),
    /* Zora punch     */ DMG_ENTRY(0, 0x0),
    /* Spin attack    */ DMG_ENTRY(1, DH_DMGEFF_HURT),
    /* Sword beam     */ DMG_ENTRY(1, DH_DMGEFF_HURT),
    /* Normal Roll    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, 0x0),
    /* Unblockable    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, 0x0),
    /* Powder Keg     */ DMG_ENTRY(0, 0x0),
};

static ColliderCylinderInit sBodyCylinderInit = {
    {
        COL_MATERIAL_HIT0,
        AT_NONE,
        AC_ON | AC_HARD | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0x00000008, 0x00, 0x00 },
        ATELEM_NONE,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 35, 70, 0, { 0, 0, 0 } },
};

static ColliderJntSphElementInit sHeadElementsInit[1] = {
    {
        {
            ELEM_MATERIAL_UNK0,
            { 0x00000000, 0x00, 0x00 },
            { 0xF7CFFFFF, 0x00, 0x00 },
            ATELEM_NONE,
            ACELEM_ON,
            OCELEM_ON | OCELEM_UNK3,
        },
        { 1, { { 0, 0, 0 }, 20 }, 100 },
    },
};

static ColliderJntSphInit sHeadJntSphInit = {
    {
        COL_MATERIAL_HIT6,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER,
        OC2_TYPE_1,
        COLSHAPE_JNTSPH,
    },
    ARRAY_COUNT(sHeadElementsInit),
    sHeadElementsInit,
};

#define DH_HAND_ELEMENT(limb, x, radius, ocFlags)                                                                       \
    {                                                                                                                   \
        {                                                                                                               \
            ELEM_MATERIAL_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, ATELEM_NONE, ACELEM_ON, ocFlags \
        },                                                                                                              \
            { limb, { { x, 0, 0 }, radius }, 100 },                                                                     \
    }

static ColliderJntSphElementInit sHandElementsInit[5] = {
    DH_HAND_ELEMENT(1, 0, 12, OCELEM_NONE),  DH_HAND_ELEMENT(2, 3200, 10, OCELEM_ON),
    DH_HAND_ELEMENT(3, 1200, 10, OCELEM_ON), DH_HAND_ELEMENT(4, 2700, 10, OCELEM_NONE),
    DH_HAND_ELEMENT(5, 1200, 10, OCELEM_ON),
};

static ColliderJntSphInit sHandJntSphInit = {
    {
        COL_MATERIAL_HIT6,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER | OC1_TYPE_1,
        OC2_TYPE_1,
        COLSHAPE_JNTSPH,
    },
    ARRAY_COUNT(sHandElementsInit),
    sHandElementsInit,
};

static InitChainEntry sBodyInitChain[] = {
    ICHAIN_F32(lockOnArrowOffset, 2000, ICHAIN_CONTINUE),
    ICHAIN_VEC3F_DIV1000(scale, 10, ICHAIN_CONTINUE),
    ICHAIN_F32_DIV1000(gravity, -3500, ICHAIN_STOP),
};

static InitChainEntry sHandInitChain[] = {
    ICHAIN_F32(lockOnArrowOffset, 2000, ICHAIN_CONTINUE),
    ICHAIN_VEC3F_DIV1000(scale, 10, ICHAIN_STOP),
};

static void EnDh_SetupWait(EnDh* this, PlayState* play);
static void EnDh_SetupWalk(EnDh* this);
static void EnDh_SetupAttack(EnDh* this);
static void EnDh_SetupBurrow(EnDh* this);
static void EnDha_SetupWait(EnDha* this);
static void EnDha_SetupTakeDamage(EnDha* this);

static bool IsFloorStep(PlayState* play, Actor* actor, Vec3f* from, Vec3f* to, f32* floorY) {
    CollisionPoly* poly = NULL;
    s32 bgId = 0;
    Vec3f wallHit;
    Vec3f eyeFrom = { from->x, from->y + 30.0f, from->z };
    Vec3f eyeTo = { to->x, from->y + 30.0f, to->z };

    if (BgCheck_EntityLineTest1(&play->colCtx, &eyeFrom, &eyeTo, &wallHit, &poly, true, false, false, true, &bgId)) {
        return false;
    }
    Vec3f probe = { to->x, from->y + 50.0f, to->z };
    *floorY = BgCheck_EntityRaycastFloor5(&play->colCtx, &poly, &bgId, actor, &probe);
    return (*floorY > BGCHECK_Y_MIN) && (fabsf(*floorY - from->y) <= DH_FLOOR_STEP_MAX);
}

// Vanilla moved up to 300 units from home blindly; a spot behind a wall or over a pit is skipped here.
static bool FindFloorSpot(PlayState* play, Actor* actor, Vec3f* center, s16 yaw, f32 radius, Vec3f* spot) {
    spot->x = center->x + Math_SinS(yaw) * radius;
    spot->y = center->y;
    spot->z = center->z + Math_CosS(yaw) * radius;
    return IsFloorStep(play, actor, center, spot, &spot->y);
}

static Actor* FindLiveActor(PlayState* play, Actor* target, s16 actorId) {
    static const u8 kCategories[] = { ACTORCAT_ENEMY, ACTORCAT_PROP };

    for (u32 i = 0; i < ARRAY_COUNT(kCategories); i++) {
        for (Actor* actor = play->actorCtx.actorLists[kCategories[i]].first; actor != NULL; actor = actor->next) {
            if ((actor == target) && (actor->id == actorId)) {
                return actor;
            }
        }
    }
    return NULL;
}

static void ReleasePlayerFrom(Actor* grabber, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!(player->stateFlags2 & PLAYER_STATE2_80) || (player->actor.parent != grabber)) {
        return;
    }
    player->stateFlags2 &= ~PLAYER_STATE2_80;
    player->actor.parent = NULL;
    player->av2.actionVar2 = 200;
}

static void GetPitchYaw(Vec3f* from, Vec3f* to, Vec3s* angle) {
    f32 dx = to->x - from->x;
    f32 dz = to->z - from->z;
    f32 dy = from->y - to->y;

    angle->y = Math_Atan2S_XY(dz, dx);
    angle->x = Math_Atan2S_XY(sqrtf(SQ(dx) + SQ(dz)), dy);
}

static void EnDh_StartBgm(EnDh* this) {
    if (this->isBgmPlaying) {
        return;
    }
    Audio_PlayBgm_StorePrevBgm(NA_BGM_MINI_BOSS);
    this->isBgmPlaying = true;
}

static void EnDh_StopBgm(EnDh* this) {
    if (!this->isBgmPlaying) {
        return;
    }
    Audio_RestorePrevBgm();
    this->isBgmPlaying = false;
}

static void EnDh_SetBite(EnDh* this, bool isBiting) {
    ColliderElement* head = &this->headElements[0].base;

    this->headCollider.base.atFlags = isBiting ? (AT_ON | AT_TYPE_ENEMY) : AT_NONE;
    head->atElemFlags = isBiting ? (ATELEM_ON | ATELEM_SFX_WOOD) : ATELEM_NONE;
    head->atDmgInfo.dmgFlags = isBiting ? 0xF7CFFFFF : 0;
    head->atDmgInfo.damage = isBiting ? 8 : 0;
}

static void EnDh_SetBurrowWave(EnDh* this, bool isHarmful) {
    this->bodyCollider.base.atFlags = isHarmful ? (AT_ON | AT_TYPE_ENEMY) : AT_NONE;
    this->bodyCollider.elem.atElemFlags = isHarmful ? (ATELEM_ON | ATELEM_SFX_WOOD) : ATELEM_NONE;
    this->bodyCollider.elem.atDmgInfo.dmgFlags = isHarmful ? 0xF7CFFFFF : 0;
    this->bodyCollider.elem.atDmgInfo.damage = isHarmful ? 4 : 0;
}

static void EnDh_SpawnDebris(PlayState* play, EnDh* this, f32 spread) {
    Vec3f velocity = { 0.0f, 8.0f + (Rand_ZeroOne() - 0.5f) * 4.0f, 0.0f };
    Vec3f accel = { (Rand_ZeroOne() - 0.5f) * 2.05f, -1.5f, (Rand_ZeroOne() - 0.5f) * 2.05f };
    f32 angle = (Rand_ZeroOne() - 0.5f) * 6.28f;
    Vec3f pos = { this->actor.world.pos.x + sinf(angle) * spread, this->actor.floorHeight,
                  this->actor.world.pos.z + cosf(angle) * spread };
    f32 scale = (Rand_ZeroOne() * 5.0f + 12.0f) * 1.2f;

    EffectSsHahen_Spawn(play, &pos, &velocity, &accel, 4, (s16)scale, -1, 10, NULL);
}

static void EnDh_SpawnHands(EnDh* this, PlayState* play) {
    for (s32 i = 0; i < DH_HAND_COUNT; i++) {
        s16 yaw = this->actor.home.rot.y + 0x2000 + i * 0x4000;
        Vec3f spot;
        bool hasSpot = FindFloorSpot(play, &this->actor, &this->actor.home.pos, yaw, DH_HAND_RING_RADIUS, &spot) ||
                       FindFloorSpot(play, &this->actor, &this->actor.home.pos, yaw, DH_HAND_RING_RADIUS * 0.5f, &spot);

        if (!hasSpot) {
            continue;
        }
        Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, sHandActorId, spot.x, spot.y, spot.z, 0, yaw, 0, 0);
    }
}

static void EnDh_Relocate(EnDh* this, PlayState* play) {
    for (s32 i = 0; i < DH_RELOCATE_TRIES; i++) {
        Vec3f spot;
        s16 yaw = (s16)(Rand_ZeroOne() * 0x10000);

        if (FindFloorSpot(play, &this->actor, &this->actor.home.pos, yaw, Rand_ZeroOne() * DH_RELOCATE_RANGE, &spot)) {
            this->actor.world.pos = spot;
            return;
        }
    }
    this->actor.world.pos = this->actor.home.pos;
}

static bool EnDh_IsPlayerNear(EnDh* this) {
    return (this->actor.xzDistToPlayer < DH_WAKE_DISTANCE) && (fabsf(this->actor.playerHeightRel) < DH_WAKE_HEIGHT);
}

static bool EnDh_WasBombed(EnDh* this) {
    Collider* body = &this->bodyCollider.base;

    if (!(body->acFlags & AC_HIT) || (body->ac == NULL) || (body->ac->category != ACTORCAT_EXPLOSIVES)) {
        return false;
    }
    body->acFlags &= ~AC_HIT;
    return true;
}

static void EnDh_Wait(EnDh* this, PlayState* play) {
    if (EnDh_WasBombed(this)) {
        this->actor.params = DH_STATE_WAKE_BY_BOMB;
    }
    if ((this->actor.params == DH_STATE_WAIT_UNDERGROUND) && EnDh_IsPlayerNear(this)) {
        this->actor.params = DH_STATE_WAKE_BY_GRAB;
    }
    bool isWaking = (this->actor.params >= DH_STATE_WAKE_BY_GRAB) || (this->actor.params <= DH_STATE_HANDS_KILLED_ALL);
    if (!isWaking) {
        return;
    }
    if (this->actionState == 0) {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        this->actor.flags &= ~ACTOR_FLAG_REACT_TO_LENS;
        this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
        this->actionState = 1;
        this->drawDirtWave = true;
        EnDh_StartBgm(this);
        Actor_PlaySfx(&this->actor, NA_SE_EN_STALKIDS_APPEAR);
    }
    if (this->actionState == 1) {
        this->dirtWavePhase += 0x3A7;
        Math_SmoothStepToF(&this->dirtWaveSpread, 300.0f, 1.0f, 5.0f, 0.0f);
        this->dirtWaveHeight = Math_SinS(this->dirtWavePhase) * 55.0f;
        this->dirtWaveAlpha = (s16)(Math_SinS(this->dirtWavePhase) * 255.0f);
        EnDh_SpawnDebris(play, this, this->dirtWaveSpread);
        if (this->actor.shape.yOffset == 0.0f) {
            this->drawDirtWave = false;
            this->actionState = 2;
        } else if (this->dirtWavePhase > 0x12C0) {
            this->actor.shape.yOffset += 500.0f;
        }
    } else if (this->actionState == 2) {
        EnDh_SetupWalk(this);
        return;
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0x7D0, 0);
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.params != DH_STATE_WAKE_BY_BOMB) {
        Player_SetAutoLockOnActor(play, &this->actor);
    }
}

static void EnDh_SetupWait(EnDh* this, PlayState* play) {
    Animation_PlayLoop(&this->skelAnime, ANIM(sWalkAnim));
    this->curAction = DH_ACTION_WAIT;
    EnDh_Relocate(this, play);
    this->actor.shape.yOffset = -15000.0f;
    this->dirtWaveSpread = 0.0f;
    this->actor.speed = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.flags |= ACTOR_FLAG_REACT_TO_LENS;
    this->dirtWavePhase = 0;
    this->actionState = 0;
    this->actor.params = DH_STATE_WAIT_UNDERGROUND;
    this->actionFunc = EnDh_Wait;
}

static void EnDh_Walk(EnDh* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0xFA, 0);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    SkelAnime_Update(&this->skelAnime);
    if (((s32)this->skelAnime.curFrame % 8) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_STAL_WALK);
    }
    if ((play->gameplayFrames & 0x5F) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_STAL_WARAU);
    }
    if (this->actor.xzDistToPlayer <= DH_REACH) {
        this->actor.speed = 0.0f;
        if (Actor_IsFacingPlayer(&this->actor, DH_FACING_ANGLE)) {
            EnDh_SetupAttack(this);
        }
        return;
    }
    this->timer--;
    if (this->timer == 0) {
        EnDh_SetupBurrow(this);
    }
}

static void EnDh_SetupWalk(EnDh* this) {
    Animation_Change(&this->skelAnime, ANIM(sWalkAnim), 1.0f, 0.0f, Animation_GetLastFrame((void*)sWalkAnim) - 3.0f,
                     ANIMMODE_LOOP, -6.0f);
    this->curAction = DH_ACTION_WALK;
    this->timer = 300;
    this->actor.speed = 1.0f;
    this->actionFunc = EnDh_Walk;
}

static void EnDh_Retreat(EnDh* this, PlayState* play) {
    this->timer--;
    if (this->timer == 0) {
        this->retreat = false;
        EnDh_SetupBurrow(this);
    } else {
        Math_SmoothStepToS(&this->actor.shape.rot.y, (s16)(this->actor.yawTowardsPlayer + 0x8000), 1, 0xBB8, 0);
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    SkelAnime_Update(&this->skelAnime);
}

static void EnDh_SetupRetreat(EnDh* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sRetreatAnim), -4.0f);
    this->curAction = DH_ACTION_RETREAT;
    this->timer = 70;
    this->actor.speed = 1.0f;
    this->actionFunc = EnDh_Retreat;
}

static void EnDh_CancelLunge(EnDh* this) {
    Animation_Change(&this->skelAnime, ANIM(sLungeAnim), -1.0f, this->skelAnime.curFrame, 0.0f, ANIMMODE_ONCE, -4.0f);
    this->actionState = 4;
    EnDh_SetBite(this, false);
}

static void EnDh_Attack(EnDh* this, PlayState* play) {
    bool canReach = (this->actor.xzDistToPlayer <= DH_REACH) && Actor_IsFacingPlayer(&this->actor, DH_FACING_ANGLE);

    if (SkelAnime_Update(&this->skelAnime)) {
        this->actionState++;
    } else if (!canReach && (this->actionState < 4)) {
        EnDh_CancelLunge(this);
    }
    switch (this->actionState) {
        case 0:
            Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0x5DC, 0);
            break;
        case 1:
            Animation_PlayOnce(&this->skelAnime, ANIM(sBiteAnim));
            this->actionState = 2;
            Actor_PlaySfx(&this->actor, NA_SE_EN_REDEAD_ATTACK);
            Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 1, 0x5DC, 0);
            break;
        case 2:
            if (this->skelAnime.curFrame >= 4.0f) {
                EnDh_SetBite(this, true);
            }
            if (this->headCollider.base.atFlags & AT_BOUNCED) {
                EnDh_SetBite(this, false);
                this->actionState = 3;
            } else if (this->headCollider.base.atFlags & AT_HIT) {
                this->headCollider.base.atFlags &= ~AT_HIT;
                func_800B8D98(play, &this->actor, 8.0f, this->actor.shape.rot.y, 8.0f);
            }
            break;
        case 3:
            if (canReach) {
                Animation_Change(&this->skelAnime, ANIM(sLungeAnim), 1.0f, 20.0f,
                                 Animation_GetLastFrame((void*)sLungeAnim), ANIMMODE_ONCE, -6.0f);
                this->actionState = 0;
            } else {
                Animation_Change(&this->skelAnime, ANIM(sLungeAnim), -1.0f, Animation_GetLastFrame((void*)sLungeAnim),
                                 0.0f, ANIMMODE_ONCE, -4.0f);
                this->actionState = 4;
                EnDh_SetBite(this, false);
            }
            break;
        case 4:
            break;
        case 5:
            EnDh_SetupWalk(this);
            break;
        default:
            EnDh_SetBite(this, false);
            EnDh_SetupWalk(this);
            break;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
}

static void EnDh_SetupAttack(EnDh* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sLungeAnim), -6.0f);
    this->timer = 0;
    this->actionState = 0;
    this->curAction = DH_ACTION_ATTACK;
    this->actor.speed = 0.0f;
    this->actionFunc = EnDh_Attack;
}

static void EnDh_Burrow(EnDh* this, PlayState* play) {
    if (this->actionState == 0) {
        this->actionState = 1;
        this->drawDirtWave = true;
        EnDh_SetBurrowWave(this, true);
    }
    if (this->actionState == 1) {
        this->dirtWavePhase += 0x47E;
        Math_SmoothStepToF(&this->dirtWaveSpread, 300.0f, 1.0f, 8.0f, 0.0f);
        this->dirtWaveHeight = Math_SinS(this->dirtWavePhase) * 55.0f;
        this->dirtWaveAlpha = (s16)(Math_SinS(this->dirtWavePhase) * 255.0f);
        EnDh_SpawnDebris(play, this, this->dirtWaveSpread);
        this->bodyCollider.dim.radius = this->dirtWaveSpread * 0.6f;
        if (SkelAnime_Update(&this->skelAnime)) {
            this->actionState = 2;
        }
        return;
    }
    this->drawDirtWave = false;
    this->bodyCollider.dim.radius = 35;
    EnDh_SetBurrowWave(this, false);
    EnDh_SetupWait(this, play);
}

static void EnDh_SetupBurrow(EnDh* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sBurrowAnim), -6.0f);
    this->curAction = DH_ACTION_BURROW;
    this->dirtWaveSpread = 0.0f;
    this->actor.speed = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->dirtWavePhase = 0;
    this->actionState = 0;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Actor_PlaySfx(&this->actor, NA_SE_EN_STALKIDS_APPEAR);
    this->actionFunc = EnDh_Burrow;
}

static void EnDh_Damage(EnDh* this, PlayState* play) {
    if (this->actor.speed < 0.0f) {
        this->actor.speed += 0.15f;
    }
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->lastHitCombo = 255;
    if (this->retreat) {
        EnDh_SetupRetreat(this);
        return;
    }
    if ((this->actor.xzDistToPlayer <= DH_REACH + 5.0f) && Actor_IsFacingPlayer(&this->actor, DH_FACING_ANGLE)) {
        EnDh_SetupAttack(this);
        Animation_Change(&this->skelAnime, ANIM(sLungeAnim), 1.0f, 20.0f, Animation_GetLastFrame((void*)sLungeAnim),
                         ANIMMODE_ONCE, -6.0f);
        return;
    }
    EnDh_SetupWalk(this);
}

static void EnDh_SetupDamage(EnDh* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sDamageAnim), -6.0f);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        this->actor.speed = -1.0f;
    }
    Actor_PlaySfx(&this->actor, NA_SE_EN_REDEAD_DAMAGE);
    this->curAction = DH_ACTION_DAMAGE;
    this->actionFunc = EnDh_Damage;
}

static void EnDh_Death(EnDh* this, PlayState* play) {
    bool hasFallen = this->timer != 300;

    if (!SkelAnime_Update(&this->skelAnime) && !hasFallen) {
        s32 frame = (s32)this->skelAnime.curFrame;

        if ((frame == 53) || (frame == 56) || (frame == 61)) {
            Actor_PlaySfx(&this->actor, NA_SE_EN_GERUDOFT_DOWN);
        }
        if (frame == 61) {
            Actor_ChangeCategory(play, &play->actorCtx, &this->actor, ACTORCAT_PROP);
        }
        return;
    }
    if (!hasFallen) {
        Animation_PlayLoop(&this->skelAnime, ANIM(sDeadAnim));
    }
    this->timer--;
    if (this->timer >= 150) {
        return;
    }
    if (this->alpha == 0) {
        Actor_Kill(&this->actor);
        return;
    }
    this->actor.scale.y -= 0.000075f;
    this->alpha -= 5;
    this->actor.shape.shadowAlpha = this->alpha;
}

static void EnDh_SetupDeath(EnDh* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sDeathAnim), -1.0f);
    this->curAction = DH_ACTION_DEATH;
    this->timer = 300;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.speed = 0.0f;
    this->actor.params = DH_STATE_DEATH;
    EnDh_StopBgm(this);
    Actor_PlaySfx(&this->actor, NA_SE_EN_REDEAD_DEAD);
    this->actionFunc = EnDh_Death;
}

static void EnDh_CheckDamage(EnDh* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!(this->headCollider.base.acFlags & AC_HIT) || this->retreat) {
        return;
    }
    this->headCollider.base.acFlags &= ~AC_HIT;
    if (this->actor.colChkInfo.damageEffect != DH_DMGEFF_HURT) {
        return;
    }
    EnDh_SetBite(this, false);
    if (player->unk_ADC != 0) {
        this->lastHitCombo = player->unk_ADD;
    }
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
    s32 lastHealth = this->actor.colChkInfo.health;
    if (Actor_ApplyDamage(&this->actor) == 0) {
        EnDh_SetupDeath(this);
        Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x90);
        return;
    }
    s32 health = this->actor.colChkInfo.health;
    if (((lastHealth >= 15) && (health < 15)) || ((lastHealth >= 9) && (health < 9)) ||
        ((lastHealth >= 3) && (health < 3))) {
        this->retreat = true;
    }
    EnDh_SetupDamage(this);
}

static void EnDh_Init(Actor* thisx, PlayState* play) {
    EnDh* this = (EnDh*)thisx;

    Actor_ProcessInitChain(&this->actor, sBodyInitChain);
    this->actor.colChkInfo.damageTable = &sDamageTable;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sBodySkel, ANIM(sRetreatAnim), this->jointTable,
                       this->morphTable, DH_LIMB_COUNT);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 64.0f);
    this->actor.colChkInfo.mass = MASS_HEAVY;
    this->actor.colChkInfo.health = DH_HEALTH;
    this->alpha = 255;
    this->lastHitCombo = 255;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyCylinderInit);
    Collider_InitJntSph(play, &this->headCollider);
    Collider_SetJntSph(play, &this->headCollider, &this->actor, &sHeadJntSphInit, this->headElements);
    EnDh_SetupWait(this, play);
}

static void EnDh_Destroy(Actor* thisx, PlayState* play) {
    EnDh* this = (EnDh*)thisx;

    EnDh_StopBgm(this);
    Collider_DestroyCylinder(play, &this->bodyCollider);
    Collider_DestroyJntSph(play, &this->headCollider);
}

static void EnDh_Update(Actor* thisx, PlayState* play) {
    EnDh* this = (EnDh*)thisx;
    Player* player = GET_PLAYER(play);

    // Spawned here, not in Init: a body removed right after Init (missing soul) must not leave hands behind.
    if (!this->hasSpawnedHands) {
        this->hasSpawnedHands = true;
        EnDh_SpawnHands(this, play);
    }
    EnDh_CheckDamage(this, play);
    this->actionFunc(this, play);
    Actor_MoveWithGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 45.0f, 45.0f,
                            UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_8 |
                                UPDBGCHECKINFO_FLAG_10);
    this->actor.focus.pos = this->headPos;
    Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
    if (this->actor.colChkInfo.health == 0) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->headCollider.base);
        return;
    }
    if (this->curAction == DH_ACTION_WAIT) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
    } else {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
    }
    bool isNewSwing = (player->unk_ADC != 0) && ((u8)player->unk_ADD != this->lastHitCombo);
    bool isAboveGround = (this->curAction != DH_ACTION_DAMAGE) && (this->actor.shape.yOffset == 0.0f);
    if (isAboveGround || isNewSwing) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->headCollider.base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->headCollider.base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->bodyCollider.base);
    }
}

static void EnDh_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx, Gfx** gfx) {
    EnDh* this = (EnDh*)thisx;
    Vec3f headOffset = { 2000.0f, 1000.0f, 0.0f };

    if (limbIndex != DH_HEAD_LIMB) {
        return;
    }
    Matrix_MultVec3f(&headOffset, &this->headPos);
    Matrix_Push();
    Matrix_Translate(headOffset.x, headOffset.y, headOffset.z, MTXMODE_APPLY);
    Collider_UpdateSpheres(1, &this->headCollider);
    Matrix_Pop();
}

static void EnDh_DrawDirtWave(EnDh* this, PlayState* play) {
    u32 frames = play->state.frames;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, 85, 55, 0, 130);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (frames * -3) % 0x80, 0, 0x20, 0x40, 1,
                                             (frames * -10) % 0x80, (frames * -20) % 0x100, 0x20, 0x40, -3, 0, -10,
                                             -20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 0, 0, 0, (u8)this->dirtWaveAlpha);
    Matrix_Translate(0.0f, -this->actor.shape.yOffset, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(this->dirtWaveSpread * 0.01f, this->dirtWaveHeight * 0.01f, this->dirtWaveSpread * 0.01f,
                 MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sDirtWaveDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnDh_Draw(Actor* thisx, PlayState* play) {
    EnDh* this = (EnDh*)thisx;
    SkelAnime* skelAnime = &this->skelAnime;

    OPEN_DISPS(play->state.gfxCtx);
    if (this->alpha == 255) {
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, this->alpha);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEmptyDL);
        POLY_OPA_DISP = SkelAnime_DrawFlex(play, skelAnime->skeleton, skelAnime->jointTable, skelAnime->dListCount,
                                           NULL, EnDh_PostLimbDraw, &this->actor, POLY_OPA_DISP);
    } else {
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 0, this->alpha);
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)sXluRenderModeDL);
        POLY_XLU_DISP = SkelAnime_DrawFlex(play, skelAnime->skeleton, skelAnime->jointTable, skelAnime->dListCount,
                                           NULL, NULL, &this->actor, POLY_XLU_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    if (this->drawDirtWave) {
        EnDh_DrawDirtWave(this, play);
    }
}

static void EnDha_Wait(EnDha* this, PlayState* play) {
    Vec3f elbowFromHand = { 0.0f, 0.0f, -54.0f };
    Vec3f elbowFromShoulder = { 0.0f, 0.0f, 55.0f };
    Player* player = GET_PLAYER(play);
    Vec3f grabPos = player->actor.world.pos;
    Vec3s angle;

    grabPos.x += Math_SinS(player->actor.shape.rot.y) * -5.0f;
    grabPos.z += Math_CosS(player->actor.shape.rot.y) * -5.0f;
    grabPos.y += Player_GetHeight(player) * 0.85f;

    if (this->actor.xzDistToPlayer > DH_REACH) {
        ReleasePlayerFrom(&this->actor, play);
        this->actor.home.rot.z = 1;
        Math_SmoothStepToS(&this->limbAngleX[1], 0, 1, 0x3E8, 0);
        Math_SmoothStepToS(&this->limbAngleX[0], -0x4000, 1, 0x3E8, 0);
        SkelAnime_Update(&this->skelAnime);
        return;
    }
    this->handAngle.y = 0;
    this->handAngle.x = 0;
    this->limbAngleY = 0;
    if (Math_Vec3f_DistXYZ(&grabPos, &this->handPos[0]) <= 12.0f) {
        if (!this->isGripping) {
            if (play->grabPlayer(play, player)) {
                this->gripPhase = 0;
                this->isGripping = true;
                if (this->actor.parent != NULL) {
                    this->actor.parent->params = DH_STATE_WAKE_BY_GRAB;
                }
                Actor_PlaySfx(&this->actor, NA_SE_EN_HANDW_GET);
            }
        } else {
            this->gripPhase += 0x1194;
            this->limbAngleY = Math_SinS(this->gripPhase) * 1820.0f;
            if (!(player->stateFlags2 & PLAYER_STATE2_80)) {
                this->isGripping = false;
                EnDha_SetupTakeDamage(this);
                return;
            }
            if (this->gripPhase < -0x6E6B) {
                Actor_PlaySfx(&this->actor, NA_SE_EN_HANDW_GET);
            }
        }
        GetPitchYaw(&this->handPos[1], &grabPos, &this->handAngle);
        this->handAngle.y -= this->actor.shape.rot.y + this->limbAngleY;
        this->handAngle.x -= this->actor.shape.rot.x + this->limbAngleX[0] + this->limbAngleX[1];
    } else {
        ReleasePlayerFrom(&this->actor, play);
        if (this->actor.home.rot.z != 0) {
            Actor_PlaySfx(&this->actor, NA_SE_EN_HANDW_RELEASE);
            this->actor.home.rot.z = 0;
        }
    }
    this->actor.shape.rot.y = Math_Vec3f_Yaw(&this->actor.world.pos, &grabPos);
    Math_SmoothStepToF(&this->handPos[0].x, grabPos.x, 1.0f, 16.0f, 0.0f);
    Math_SmoothStepToF(&this->handPos[0].y, grabPos.y, 1.0f, 16.0f, 0.0f);
    Math_SmoothStepToF(&this->handPos[0].z, grabPos.z, 1.0f, 16.0f, 0.0f);

    GetPitchYaw(&this->armPos, &this->handPos[0], &angle);
    Matrix_Translate(this->handPos[0].x, this->handPos[0].y, this->handPos[0].z, MTXMODE_NEW);
    Matrix_RotateZYX(angle.x, angle.y, 0, MTXMODE_APPLY);
    Matrix_MultVec3f(&elbowFromHand, &this->armPos);
    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
    GetPitchYaw(&this->actor.world.pos, &this->armPos, &angle);
    Matrix_RotateZYX(angle.x, angle.y, 0, MTXMODE_APPLY);
    Matrix_MultVec3f(&elbowFromShoulder, &this->armPos);
    this->limbAngleX[0] = Math_Vec3f_Pitch(&this->actor.world.pos, &this->armPos);
    s16 yaw = Math_Vec3f_Yaw(&this->actor.world.pos, &this->armPos) - this->actor.shape.rot.y;
    if (ABS(yaw) >= 0x4000) {
        this->limbAngleX[0] = -0x8000 - this->limbAngleX[0];
    }
    this->limbAngleX[1] = Math_Vec3f_Pitch(&this->armPos, &this->handPos[0]) - this->limbAngleX[0];
    if (this->limbAngleX[1] < 0) {
        this->limbAngleX[0] += this->limbAngleX[1] * 2;
        this->limbAngleX[1] *= -2;
    }
}

static void EnDha_TakeDamage(EnDha* this, PlayState* play) {
    ReleasePlayerFrom(&this->actor, play);
    Math_SmoothStepToS(&this->limbAngleX[1], 0, 1, 2000, 0);
    Math_SmoothStepToS(&this->limbAngleY, 0, 1, 600, 0);
    Math_SmoothStepToS(&this->limbAngleX[0], -0x4000, 1, 2000, 0);
    SkelAnime_Update(&this->skelAnime);
    this->actionTimer--;
    if (this->actionTimer == 0) {
        EnDha_SetupWait(this);
    }
}

static void EnDha_SetupTakeDamage(EnDha* this) {
    this->actionTimer = 15;
    this->actionFunc = EnDha_TakeDamage;
}

static void EnDha_SpawnDust(PlayState* play, Vec3f* base) {
    Vec3f velocity = { 0.0f, 0.0f, 0.0f };
    Vec3f accel = { 0.0f, 0.3f, 0.0f };

    for (s32 i = 0; i < 2; i++) {
        Vec3f pos = { base->x + (Rand_ZeroOne() - 0.5f) * 7.0f, base->y + (Rand_ZeroOne() - 0.5f) * 7.0f,
                      base->z + (Rand_ZeroOne() - 0.5f) * 7.0f };
        s16 scale = (s16)(Rand_ZeroOne() * 90.0f * 0.2f) + 90;

        func_800B1210(play, &pos, &velocity, &accel, scale, 20);
    }
}

static bool EnDha_IsBodyDead(EnDha* this) {
    return (this->actor.parent != NULL) && (this->actor.parent->params == DH_STATE_DEATH);
}

static void EnDha_Die(EnDha* this, PlayState* play) {
    ReleasePlayerFrom(&this->actor, play);
    Math_SmoothStepToS(&this->limbAngleX[1], 0, 1, 0x7D0, 0);
    s16 remaining = Math_SmoothStepToS(&this->limbAngleX[0], -0x4000, 1, 0x7D0, 0);
    SkelAnime_Update(&this->skelAnime);
    if (remaining != 0) {
        return;
    }
    Vec3f dustPos = this->actor.world.pos;
    if (this->actionTimer == 0) {
        this->actor.shape.yOffset += 500.0f;
        EnDha_SpawnDust(play, &dustPos);
        if (this->actor.shape.yOffset == 0.0f) {
            EnDha_SetupWait(this);
        }
        return;
    }
    if (this->actor.shape.yOffset > -12000.0f) {
        this->actor.shape.yOffset -= 1000.0f;
        EnDha_SpawnDust(play, &dustPos);
        return;
    }
    this->actionTimer--;
    // Without a body (killed, or never spawned with it) a buried hand has nothing to come back for.
    if (EnDha_IsBodyDead(this) || (this->actor.parent == NULL)) {
        Actor_Kill(&this->actor);
    }
}

static void EnDha_SetupDeath(EnDha* this) {
    this->ignoresHits = true;
    this->actionTimer = 300;
    if (this->actor.parent != NULL) {
        if (this->actor.parent->params != DH_STATE_DEATH) {
            Actor_PlaySfx(&this->actor, NA_SE_EN_STALKID_DEAD);
        }
        if (this->actor.parent->params <= DH_STATE_WAIT_UNDERGROUND) {
            this->actor.parent->params--;
        }
    }
    this->actionFunc = EnDha_Die;
}

static void EnDha_SetupWait(EnDha* this) {
    Animation_PlayLoop(&this->skelAnime, ANIM(sHandIdleAnim));
    this->ignoresHits = false;
    this->actionTimer = (s16)(Rand_ZeroOne() * 10.0f + 5.0f);
    this->actor.speed = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    this->actor.home.rot.z = 1;
    this->actionFunc = EnDha_Wait;
}

static void EnDha_CheckDamage(EnDha* this, PlayState* play) {
    if (!this->ignoresHits && (this->collider.base.acFlags & AC_HIT)) {
        this->collider.base.acFlags &= ~AC_HIT;
        if (this->actor.colChkInfo.damageEffect == DH_DMGEFF_HURT) {
            Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
            if (Actor_ApplyDamage(&this->actor) == 0) {
                EnDha_SetupDeath(this);
                this->actor.colChkInfo.health = DH_HAND_HEALTH;
                Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0xE0);
            } else {
                Actor_PlaySfx(&this->actor, NA_SE_EN_REDEAD_DAMAGE);
                this->ignoresHits = true;
                EnDha_SetupTakeDamage(this);
            }
        }
    }
    if (EnDha_IsBodyDead(this) && (this->actionFunc != EnDha_Die)) {
        EnDha_SetupDeath(this);
    }
}

static void EnDha_Init(Actor* thisx, PlayState* play) {
    EnDha* this = (EnDha*)thisx;

    Actor_ProcessInitChain(&this->actor, sHandInitChain);
    this->actor.colChkInfo.damageTable = &sDamageTable;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sHandSkel, ANIM(sHandIdleAnim), this->jointTable,
                       this->morphTable, DH_HAND_LIMB_COUNT);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawFeet, 90.0f);
    this->actor.focus.pos = this->actor.world.pos;
    this->actor.focus.pos.y += 50.0f;
    this->actor.colChkInfo.mass = MASS_HEAVY;
    this->actor.colChkInfo.health = DH_HAND_HEALTH;
    this->limbAngleX[0] = -0x4000;
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sHandJntSphInit, this->colliderElements);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    EnDha_SetupWait(this);
}

static void EnDha_Destroy(Actor* thisx, PlayState* play) {
    EnDha* this = (EnDha*)thisx;

    ReleasePlayerFrom(&this->actor, play);
    Collider_DestroyJntSph(play, &this->collider);
}

static void EnDha_Update(Actor* thisx, PlayState* play) {
    EnDha* this = (EnDha*)thisx;

    // The body may be gone (unloaded, or removed for a missing soul); never follow a stale parent pointer.
    if (FindLiveActor(play, this->actor.parent, sBodyActorId) == NULL) {
        this->actor.parent = NULL;
    }
    EnDha_CheckDamage(this, play);
    this->actionFunc(this, play);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static s32 EnDha_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx) {
    EnDha* this = (EnDha*)thisx;

    if (limbIndex == 1) {
        rot->y = -(s16)(this->limbAngleX[0] + 0x4000);
        rot->z += this->limbAngleY;
    } else if (limbIndex == 2) {
        rot->z = this->limbAngleX[1];
        rot->y -= this->limbAngleY;
    } else if (limbIndex == 3) {
        rot->y = -this->handAngle.y;
        rot->z = -this->handAngle.x;
    }
    return false;
}

static void EnDha_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    EnDha* this = (EnDha*)thisx;
    Vec3f fingertip = { 1100.0f, 0.0f, 0.0f };
    Vec3f origin = { 0.0f, 0.0f, 0.0f };

    switch (limbIndex) {
        case 1:
            Collider_UpdateSpheres(2, &this->collider);
            Collider_UpdateSpheres(3, &this->collider);
            break;
        case 2:
            Collider_UpdateSpheres(4, &this->collider);
            Collider_UpdateSpheres(5, &this->collider);
            Matrix_MultVec3f(&origin, &this->armPos);
            break;
        case 3:
            Collider_UpdateSpheres(1, &this->collider);
            Matrix_MultVec3f(&fingertip, &this->handPos[0]);
            Matrix_MultVec3f(&origin, &this->handPos[1]);
            break;
        default:
            break;
    }
}

static void EnDha_Draw(Actor* thisx, PlayState* play) {
    EnDha* this = (EnDha*)thisx;

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          EnDha_OverrideLimbDraw, EnDha_PostLimbDraw, &this->actor);
}

static bool CanKillDeadHand(void* userData) {
    return sApi->LogicHasCapability("CanUseSword");
}

static S2HActorDefinition DefineEnemyActor(const char* key, const char* description, u32 flags, u32 size) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = key;
    definition.description = description;
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = flags;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = size;
    return definition;
}

static s16 RegisterHand(void) {
    S2HActorDefinition hand = DefineEnemyActor(DEAD_HAND_HAND_KEY, "Dead Hand's hand", DH_HAND_FLAGS, sizeof(EnDha));

    hand.init = EnDha_Init;
    hand.destroy = EnDha_Destroy;
    hand.update = EnDha_Update;
    hand.draw = EnDha_Draw;
    return sApi->RegisterActor(&hand);
}

static s16 RegisterBody(void) {
    S2HActorDefinition body = DefineEnemyActor(DEAD_HAND_KEY, "Dead Hand", DH_FLAGS, sizeof(EnDh));

    body.init = EnDh_Init;
    body.destroy = EnDh_Destroy;
    body.update = EnDh_Update;
    body.draw = EnDh_Draw;
    body.enemyFlags =
        S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_MINIBOSS | S2H_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    body.naviHint = "Dead Hand&It hides underground and sends its&hands to grab you. Slash its head!";
    // The host finds the soul item as "soul." + soulKey, so this is the family key.
    body.soulKey = DEAD_HAND_KEY;
    body.logicKill = CanKillDeadHand;
    body.enemyCategory = S2H_ENEMY_CATEGORY_MINIBOSS;
    body.soulModel.skeleton = sBodySkel;
    body.soulModel.idleAnimation = sRetreatAnim;
    body.soulModel.isFlex = true;
    body.soulModel.limbCount = DH_LIMB_COUNT;
    body.soulModel.scale = 0.007f;
    body.soulModel.flameRed = 120;
    body.soulModel.flameGreen = 40;
    body.soulModel.flameBlue = 160;
    return sApi->RegisterActor(&body);
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
    if (!RequireOotAsset(DH_OBJECT "object_dh_Skel_007E88") || !RequireOotAsset(DH_OBJECT "object_dh_Skel_000BD8")) {
        sApi->Log(DEAD_HAND_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    sHandActorId = RegisterHand();
    if (sHandActorId < 0) {
        sApi->Log("Could not register " DEAD_HAND_HAND_KEY);
        return;
    }
    sBodyActorId = RegisterBody();
    if (sBodyActorId < 0) {
        sApi->Log("Could not register " DEAD_HAND_KEY);
    }
}
