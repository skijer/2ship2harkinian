/*
 * Ocarina of Time's Torch Slug (En_Bw) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * Its flame burns on contact until a blow puts it out; a Goron pound flips it on its back.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define TORCH_SLUG_KEY "oot.torch_slug"
#define BW_OBJECT "objects/oot_object_bw/"
#define BW_ASSET(name) "__OTR__" BW_OBJECT name

static const ALIGN_ASSET(2) char sSkel[] = BW_ASSET("gTorchSlugSkel");
static const ALIGN_ASSET(2) char sEyestalkWaveAnim[] = BW_ASSET("gTorchSlugEyestalkWaveAnim");
static const ALIGN_ASSET(2) char sEyestalkRaiseAnim[] = BW_ASSET("gTorchSlugEyestalkRaiseAnim");
static const ALIGN_ASSET(2) char sEyestalkFlailAnim[] = BW_ASSET("gTorchSlugEyestalkFlailAnim");

// MM reuses OoT's ids of these for other actors.
#define BW_SFX_LAND NA_SE_EN_FALL_LAND
#define BW_SFX_FREEZE NA_SE_EN_COMMON_FREEZE

#define ANIM(asset) ((AnimationHeader*)(asset))

#define BW_LIMB_BODY 1
#define BW_LIMB_MAX 12

#define BW_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED)
#define BW_BGCHECK_FLAGS                                                                             \
    (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_8 | \
     UPDBGCHECKINFO_FLAG_10)
#define BW_BASE_SCALE 0.013f
#define BW_FLAME_LIT_SCALE 0.6f
#define BW_FLAME_OUT_SCALE 0.4f
#define BW_FLIP_RANGE 400.0f

typedef enum {
    BW_DMGEFF_NONE = 0x0,
    BW_DMGEFF_STUN = 0x1,
    BW_DMGEFF_ICE = 0xE,
    BW_DMGEFF_HIT = 0xF,
} TorchSlugDamageEffect;

typedef enum {
    BW_STATE_DYING,
    BW_STATE_DOUSED,
    BW_STATE_CRAWL,
    BW_STATE_AIM,
    BW_STATE_LUNGE,
    BW_STATE_FLIPPED,
    BW_STATE_FLIPPING_BACK,
} TorchSlugState;

// Doubles as the frame mask of its flame particles: 0 spawns every frame, 3 every fourth.
typedef enum {
    BW_MOOD_CHASE = 0,
    BW_MOOD_FLEE = 1,
    BW_MOOD_WANDER = 3,
    BW_MOOD_FLIPPED = 4,
} TorchSlugMood;

// Bits of the wall probe result.
#define BW_PROBE_FRONT (1 << 0)
#define BW_PROBE_RIGHT (1 << 1)
#define BW_PROBE_LEFT (1 << 2)

static const S2HModApi* sApi;
static s32 sNextProbeGroup = 0;

struct EnBw;
typedef void (*EnBwActionFunc)(struct EnBw*, PlayState*);

typedef struct EnBw {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[BW_LIMB_MAX];
    Vec3s morphTable[BW_LIMB_MAX];
    EnBwActionFunc actionFunc;
    ColliderCylinder flameCollider;
    ColliderCylinder bodyCollider;
    Color_RGBA8 bodyColor;
    Vec3f probeFront;
    Vec3f probeNear;
    Vec3f probeLeft;
    Vec3f probeRight;
    f32 wobblePhase;
    f32 wobbleSpeed;
    f32 wobbleAmount;
    f32 wobbleBoost;
    f32 flameScale;
    f32 jiggleAngle;
    f32 jiggleSpeed;
    f32 jiggleAmount;
    f32 drawDmgEffAlpha;
    s32 probeGroup;
    s16 timer;
    s16 moodTimer;
    s16 heading;
    s16 turnOffset;
    s16 hasFloorAhead;
    u8 state;
    u8 mood;
    u8 damageEffect;
    u8 stepPhase;
    u8 bodyScroll;
    u8 freezeTimer;
    u8 isFadingWide;
} EnBw;

static ColliderCylinderInit sFlameColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x01, 0x08 },
        { 0x00000000, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { 30, 65, 0, { 0, 0, 0 } },
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
        ELEM_MATERIAL_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_NONE,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 30, 35, 0, { 0, 0, 0 } },
};

// Vanilla's Kokiri Sword, sticks and boomerang bounce off; MM's one sword type takes the sword row.
static DamageTable sDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, BW_DMGEFF_STUN),
    /* Deku Stick     */ DMG_ENTRY(0, BW_DMGEFF_NONE),
    /* Horse trample  */ DMG_ENTRY(0, BW_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(2, BW_DMGEFF_HIT),
    /* Zora boomerang */ DMG_ENTRY(0, BW_DMGEFF_NONE),
    /* Normal arrow   */ DMG_ENTRY(2, BW_DMGEFF_HIT),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, BW_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(0, BW_DMGEFF_STUN),
    /* Goron punch    */ DMG_ENTRY(1, BW_DMGEFF_HIT),
    /* Sword          */ DMG_ENTRY(1, BW_DMGEFF_HIT),
    /* Goron pound    */ DMG_ENTRY(2, BW_DMGEFF_HIT),
    /* Fire arrow     */ DMG_ENTRY(2, BW_DMGEFF_HIT),
    /* Ice arrow      */ DMG_ENTRY(2, BW_DMGEFF_ICE),
    /* Light arrow    */ DMG_ENTRY(2, BW_DMGEFF_HIT),
    /* Goron spikes   */ DMG_ENTRY(1, BW_DMGEFF_HIT),
    /* Deku spin      */ DMG_ENTRY(0, BW_DMGEFF_STUN),
    /* Deku bubble    */ DMG_ENTRY(1, BW_DMGEFF_HIT),
    /* Deku launch    */ DMG_ENTRY(0, BW_DMGEFF_NONE),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, BW_DMGEFF_NONE),
    /* Zora barrier   */ DMG_ENTRY(0, BW_DMGEFF_STUN),
    /* Normal shield  */ DMG_ENTRY(0, BW_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, BW_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(1, BW_DMGEFF_HIT),
    /* Zora punch     */ DMG_ENTRY(1, BW_DMGEFF_HIT),
    /* Spin attack    */ DMG_ENTRY(1, BW_DMGEFF_HIT),
    /* Sword beam     */ DMG_ENTRY(1, BW_DMGEFF_HIT),
    /* Normal Roll    */ DMG_ENTRY(0, BW_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, BW_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, BW_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, BW_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, BW_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(2, BW_DMGEFF_HIT),
};

// OoT's D_80116280: the translucent render mode its fading body selects through segment 8.
static Gfx sXluRenderModeDL[] = {
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU |
                                            FORCE_BL | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)),
    gsDPSetAlphaCompare(G_AC_THRESHOLD),
    gsSPEndDisplayList(),
};

// The host does not export gEmptyDL to mods.
static Gfx sEmptyDL[] = {
    gsSPEndDisplayList(),
};

static void EnBw_Crawl(EnBw* this, PlayState* play);
static void EnBw_FlippedOver(EnBw* this, PlayState* play);
static void EnBw_FlipBack(EnBw* this, PlayState* play);

static void EnBw_SetSquashScale(EnBw* this, f32 sine, f32 xzAmount, f32 yAmount) {
    this->actor.scale.x = BW_BASE_SCALE + sine * xzAmount;
    this->actor.scale.y = BW_BASE_SCALE + sine * yAmount;
    this->actor.scale.z = BW_BASE_SCALE + sine * xzAmount;
}

static void EnBw_SetupCrawl(EnBw* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sEyestalkWaveAnim), -2.0f);
    this->state = BW_STATE_CRAWL;
    this->timer = Rand_ZeroOne() * 200.0f + 200.0f;
    this->stepPhase = 0;
    this->actor.speed = 0.0f;
    this->actionFunc = EnBw_Crawl;
}

static void EnBw_SettleAfterLanding(EnBw* this, PlayState* play) {
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 1.0f, 0.5f, 0.0f);
    this->timer -= 250;
    EnBw_SetSquashScale(this, Math_SinF(this->timer * 0.001f), 0.007f, -0.007f);
    if (this->timer == 0) {
        this->actor.world.rot.y = this->actor.shape.rot.y;
        EnBw_SetupCrawl(this);
    }
}

static void EnBw_StartFleeing(EnBw* this) {
    EnBw_SetupCrawl(this);
    this->bodyColor.r = 200;
    this->bodyColor.g = 200;
    this->bodyColor.b = 255;
    this->moodTimer = 600;
    this->mood = BW_MOOD_FLEE;
    this->wobbleBoost = 0.7f;
    this->heading++;
}

static void EnBw_Aim(EnBw* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        this->timer = (this->timer > 0) ? (this->timer - 1) : 100;
    }
    if (this->timer >= 20) {
        this->moodTimer -= 250;
    }
    EnBw_SetSquashScale(this, Math_SinF(this->moodTimer * 0.001f), -0.0035f, 0.0245f);
    if (this->moodTimer == 0) {
        EnBw_SetupCrawl(this);
        this->moodTimer = 200;
    }
}

static void EnBw_SetupAim(EnBw* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sEyestalkRaiseAnim), -2.0f);
    this->state = BW_STATE_AIM;
    this->mood = BW_MOOD_CHASE;
    this->wobbleBoost = 0.6f;
    this->timer = 20;
    this->moodTimer = 3000;
    this->actor.speed = 0.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_BUBLEWALK_AIM);
    this->actionFunc = EnBw_Aim;
}

static bool EnBw_IsDeadlyFloor(PlayState* play, Actor* actor) {
    FloorType floorType = SurfaceType_GetFloorType(&play->colCtx, actor->floorPoly, actor->floorBgId);

    return (floorType == FLOOR_TYPE_2) || (floorType == FLOOR_TYPE_3) || (floorType == FLOOR_TYPE_9);
}

static void EnBw_Lunge(EnBw* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Math_SmoothStepToF(&this->actor.speed, 0.0f, 1.0f, 0.5f, 0.0f);
    this->timer += 250;
    EnBw_SetSquashScale(this, Math_SinF(this->timer * 0.001f), -0.0035f, 0.0245f);
    if (this->flameCollider.base.atFlags & AT_HIT) {
        this->flameCollider.base.atFlags &= ~AT_HIT;
        this->actor.speed = -6.0f;
        this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        if ((&player->actor == this->flameCollider.base.at) && !(this->flameCollider.base.atFlags & AT_BOUNCED)) {
            Actor_PlaySfx(&player->actor, NA_SE_PL_BODY_HIT);
        }
    }
    SkelAnime_Update(&this->skelAnime);
    if (!(this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH))) {
        return;
    }
    if (EnBw_IsDeadlyFloor(play, &this->actor)) {
        Actor_Kill(&this->actor);
        return;
    }
    Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, 30.0f, 11, 4.0f, 0, 0, false);
    this->timer = 3000;
    this->actor.flags &= ~ACTOR_FLAG_SFX_FOR_PLAYER_BODY_HIT;
    this->actor.speed = 0.0f;
    Actor_PlaySfx(&this->actor, BW_SFX_LAND);
    this->actionFunc = EnBw_SettleAfterLanding;
}

static void EnBw_SetupLunge(EnBw* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sEyestalkFlailAnim), -1.0f);
    this->actor.speed = 7.0f;
    this->actor.world.rot.y = this->actor.yawTowardsPlayer;
    this->actor.shape.rot.y = this->actor.yawTowardsPlayer;
    this->state = BW_STATE_LUNGE;
    this->timer = 1000;
    this->actor.velocity.y = 11.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_TEKU_JUMP);
    this->actor.flags |= ACTOR_FLAG_SFX_FOR_PLAYER_BODY_HIT;
    this->actionFunc = EnBw_Lunge;
}

static s16 EnBw_PolyYaw(CollisionPoly* poly) {
    return Math_FAtan2F(poly->normal.x, poly->normal.z) * ((f32)0x8000 / M_PIf);
}

// Away from the player when fleeing, towards them otherwise.
static s16 EnBw_TurnTowardPlayerSide(EnBw* this, s16 referenceYaw) {
    s16 turn = ((s16)(this->actor.yawTowardsPlayer - referenceYaw) >= 0) ? 0x4000 : -0x4000;

    return (this->mood == BW_MOOD_FLEE) ? -turn : turn;
}

static void EnBw_PickTurnAtWall(EnBw* this, PlayState* play, u32 probe) {
    // Vanilla falls through from each "turn further" case into the pick of the next turn.
    switch (probe) {
        case 0:
            this->heading += this->turnOffset;
            // fallthrough
        case BW_PROBE_FRONT:
            if (this->mood == BW_MOOD_WANDER) {
                this->turnOffset = (play->gameplayFrames & 2) ? 0x4000 : -0x4000;
            } else {
                this->turnOffset = EnBw_TurnTowardPlayerSide(this, this->actor.shape.rot.y);
            }
            break;
        case BW_PROBE_RIGHT:
            this->heading += this->turnOffset;
            // fallthrough
        case BW_PROBE_FRONT | BW_PROBE_RIGHT:
            this->turnOffset = 0x4000;
            break;
        case BW_PROBE_LEFT:
            this->heading += this->turnOffset;
            // fallthrough
        case BW_PROBE_FRONT | BW_PROBE_LEFT:
            this->turnOffset = -0x4000;
            break;
        case BW_PROBE_FRONT | BW_PROBE_RIGHT | BW_PROBE_LEFT:
            this->turnOffset = 0;
            break;
        default:
            break;
    }
}

// Returns the wall probe result; zero while the way ahead is clear.
static u32 EnBw_ProbeWalls(EnBw* this, PlayState* play) {
    CollisionPoly* poly = NULL;
    Vec3f hitPos;
    u32 probe = 0;

    bool isProbeFrame = ((play->gameplayFrames % 4) == (u32)this->probeGroup) && (this->actor.speed != 0.0f);
    if (isProbeFrame) {
        probe = BgCheck_AnyLineTest2(&play->colCtx, &this->actor.world.pos, &this->probeFront, &hitPos, &poly, true,
                                     false, false, true);
    }
    if (probe || (this->timer == 0)) {
        s16 wallYaw = (poly != NULL) ? EnBw_PolyYaw(poly) : (s16)(this->actor.world.rot.y + 0x8000);
        if ((this->heading == wallYaw) && (probe != 0)) {
            return probe;
        }
        if (BgCheck_AnyLineTest2(&play->colCtx, &this->probeNear, &this->probeRight, &hitPos, &poly, true, false, false,
                                 true)) {
            probe |= BW_PROBE_RIGHT;
        }
        if (BgCheck_AnyLineTest2(&play->colCtx, &this->probeNear, &this->probeLeft, &hitPos, &poly, true, false, false,
                                 true)) {
            probe |= BW_PROBE_LEFT;
        }
        EnBw_PickTurnAtWall(this, play, probe);
        if (probe != (BW_PROBE_RIGHT | BW_PROBE_LEFT)) {
            this->heading = wallYaw;
        }
        this->timer = (Rand_ZeroOne() * 200.0f) + 200.0f;
        return probe;
    }
    if ((this->actor.speed == 0.0f) || !(this->actor.bgCheckFlags & BGCHECKFLAG_WALL)) {
        return probe;
    }
    if (this->heading != this->actor.wallYaw) {
        this->heading = this->actor.wallYaw;
        if (this->mood == BW_MOOD_WANDER) {
            this->turnOffset = (play->gameplayFrames & 0x20) ? 0x4000 : -0x4000;
            this->actor.bgCheckFlags &= ~BGCHECKFLAG_WALL;
            this->timer = (Rand_ZeroOne() * 20.0f) + 160.0f;
        } else {
            this->turnOffset = EnBw_TurnTowardPlayerSide(this, this->heading);
        }
        return BW_PROBE_FRONT;
    }
    if (this->mood != BW_MOOD_CHASE) {
        return probe;
    }
    probe = BgCheck_AnyLineTest2(&play->colCtx, &this->actor.world.pos, &GET_PLAYER(play)->actor.world.pos, &hitPos,
                                 &poly, true, false, false, true);
    if (probe == 0) {
        return probe;
    }
    s16 blockingYaw = EnBw_PolyYaw(poly);
    if (this->heading != blockingYaw) {
        this->turnOffset = ((s16)(this->actor.yawTowardsPlayer - blockingYaw) >= 0) ? 0x4000 : -0x4000;
        this->heading = blockingYaw;
    }
    return probe;
}

static void EnBw_UpdateWobble(EnBw* this) {
    this->wobbleSpeed = this->wobbleBoost + 0.1f;
    f32 previousCos = Math_CosF(this->wobblePhase);
    this->wobblePhase += this->wobbleSpeed;
    if (this->wobbleAmount < 0.8f) {
        this->wobbleAmount += 0.1f;
    }
    f32 sine = Math_SinF(this->wobblePhase);
    this->actor.scale.x = BW_BASE_SCALE - sine * (this->wobbleAmount * 0.004f);
    this->actor.scale.y = BW_BASE_SCALE - sine * (this->wobbleAmount * 0.004f);
    this->actor.scale.z = BW_BASE_SCALE + sine * (this->wobbleAmount * 0.004f);
    f32 cosine = Math_CosF(this->wobblePhase);
    if (this->stepPhase == 0) {
        if (ABS(previousCos) < ABS(cosine)) {
            this->stepPhase++;
        }
    } else if (ABS(previousCos) > ABS(cosine)) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_BUBLEWALK_WALK);
        this->stepPhase = 0;
    }
    this->actor.speed = ABS(cosine * this->wobbleAmount * (10.0f * this->wobbleSpeed));
    if (this->mood != BW_MOOD_FLEE) {
        this->bodyColor.g = ABS(Math_SinF(this->wobblePhase)) * 85.0f;
    }
}

static void EnBw_SteerWander(EnBw* this) {
    Math_SmoothStepToF(&this->flameScale, BW_FLAME_LIT_SCALE, 1.0f, 0.05f, 0.0f);
    bool canSeePlayer = (this->moodTimer == 0) && (this->actor.xzDistToPlayer < 200.0f) &&
                        (ABS(this->actor.playerHeightRel) < 50.0f) && Actor_IsFacingPlayer(&this->actor, 0x1C70);
    if (canSeePlayer) {
        EnBw_SetupAim(this);
        return;
    }
    Math_SmoothStepToS(&this->actor.world.rot.y, this->heading + this->turnOffset, 1, this->actor.speed * 1000.0f, 0);
}

static void EnBw_SteerChase(EnBw* this, PlayState* play, u32 probe) {
    Math_SmoothStepToF(&this->flameScale, BW_FLAME_LIT_SCALE, 1.0f, 0.05f, 0.0f);
    if (probe == 0) {
        Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 1, this->actor.speed * 1000.0f, 0);
        bool canLunge = (this->actor.xzDistToPlayer < 90.0f) && (this->actor.playerHeightRel < 50.0f) &&
                        Actor_IsFacingPlayer(&this->actor, 0x1554) &&
                        Actor_TestFloorInDirection(&this->actor, play, 71.24802f, this->actor.yawTowardsPlayer);
        if (canLunge) {
            EnBw_SetupLunge(this);
        }
    } else {
        Math_SmoothStepToS(&this->actor.world.rot.y, this->heading + this->turnOffset, 1, this->actor.speed * 1000.0f,
                           0);
    }
    // MM's unnamed bits 13 and 14 are OoT's hanging off and climbing a ledge.
    bool isPlayerOnLedge = GET_PLAYER(play)->stateFlags1 & (PLAYER_STATE1_2000 | PLAYER_STATE1_4000);
    if ((this->moodTimer == 0) || (ABS(this->actor.playerHeightRel) > 60.0f) || isPlayerOnLedge) {
        this->mood = BW_MOOD_WANDER;
        this->moodTimer = 150;
        this->wobbleBoost = 0.0f;
    }
}

static void EnBw_SteerFlee(EnBw* this, u32 probe) {
    bool isFacingPlayer = Actor_IsFacingPlayer(&this->actor, 0x1C70);

    if (((probe == 0) && !(this->actor.bgCheckFlags & BGCHECKFLAG_WALL)) || isFacingPlayer) {
        if (isFacingPlayer) {
            this->turnOffset = -this->turnOffset;
        }
        Math_SmoothStepToS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer - 0x8000, 1,
                           this->actor.speed * 1000.0f, 0);
    } else {
        Math_SmoothStepToS(&this->actor.world.rot.y, this->heading + this->turnOffset, 1, this->actor.speed * 1000.0f,
                           0);
    }
    if (this->moodTimer > 200) {
        return;
    }
    s16 flicker = Math_SinS(this->moodTimer * (0x960 - this->moodTimer)) * 55.0f;
    this->bodyColor.r = 255 - ABS(flicker);
    flicker = Math_SinS(this->moodTimer * (0x960 - this->moodTimer)) * 115.0f;
    this->bodyColor.g = ABS(flicker) + 85;
    flicker = Math_SinS(0x960 - this->moodTimer) * 255.0f;
    this->bodyColor.b = ABS(flicker);
    if (this->moodTimer == 0) {
        this->mood = BW_MOOD_WANDER;
        this->wobbleBoost = 0.0f;
    }
}

static void EnBw_Crawl(EnBw* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    EnBw_UpdateWobble(this);
    u32 probe = EnBw_ProbeWalls(this, play);
    this->timer--;
    if (this->moodTimer != 0) {
        this->moodTimer--;
    }
    bool isLedgeAhead = (this->hasFloorAhead == 0) &&
                        !Actor_TestFloorInDirection(&this->actor, play, 50.0f, this->heading + this->turnOffset);
    if (isLedgeAhead) {
        this->turnOffset = (this->turnOffset != 0x4000) ? 0x4000 : -0x4000;
    }
    switch (this->mood) {
        case BW_MOOD_WANDER:
            EnBw_SteerWander(this);
            break;
        case BW_MOOD_CHASE:
            EnBw_SteerChase(this, play, probe);
            break;
        case BW_MOOD_FLEE:
            EnBw_SteerFlee(this, probe);
            break;
        default:
            break;
    }
    this->actor.shape.rot.y = this->actor.world.rot.y;
}

static void EnBw_SetupFlipOver(EnBw* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sEyestalkFlailAnim), -1.0f);
    this->state = BW_STATE_FLIPPED;
    this->timer = 1000;
    this->jiggleAmount = 0.0f;
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 11.0f;
    this->jiggleSpeed = Rand_ZeroOne() * 0.25f + 1.0f;
    this->moodTimer = 3000;
    Actor_PlaySfx(&this->actor, NA_SE_EN_BUBLEWALK_REVERSE);
    this->actionFunc = EnBw_FlippedOver;
}

static void EnBw_SetupFlipBack(EnBw* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sEyestalkFlailAnim), -1.0f);
    this->state = BW_STATE_FLIPPING_BACK;
    this->timer = 1000;
    this->mood = BW_MOOD_WANDER;
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 11.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_BUBLEWALK_REVERSE);
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    this->actionFunc = EnBw_FlipBack;
}

static void EnBw_FadeTowardsBlue(EnBw* this) {
    this->bodyColor.r = MAX(this->bodyColor.r - 8, 200);
    this->bodyColor.g = MIN(this->bodyColor.g + 32, 200);
    this->bodyColor.b = (this->bodyColor.b > 235) ? 255 : (this->bodyColor.b + 40);
}

static void EnBw_FlippedOver(EnBw* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.z, 0x7FFF, 1, 0xFA0, 0);
    Math_SmoothStepToF(&this->flameScale, 0.0f, 1.0f, 0.05f, 0.0f);
    SkelAnime_Update(&this->skelAnime);
    if (!(this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH))) {
        EnBw_FadeTowardsBlue(this);
        if (this->actor.shape.yOffset < 1000.0f) {
            this->actor.shape.yOffset += 200.0f;
        }
        return;
    }
    if ((play->gameplayFrames % 0x80) == 0) {
        this->jiggleSpeed = (Rand_ZeroOne() * 0.25f) + 0.7f;
    }
    this->mood = BW_MOOD_FLIPPED;
    this->jiggleAngle += this->jiggleSpeed;
    Math_SmoothStepToF(&this->jiggleAmount, 0.075f, 1.0f, 0.005f, 0.0f);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
        Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, 30.0f, 11, 4.0f, 0, 0, false);
        Actor_PlaySfx(&this->actor, BW_SFX_LAND);
    }
    if (this->moodTimer != 0) {
        this->moodTimer -= 250;
        EnBw_SetSquashScale(this, Math_SinF(this->moodTimer * 0.001f), 0.007f, -0.007f);
    }
    this->timer--;
    if (this->timer == 0) {
        EnBw_SetupFlipBack(this);
    }
}

static void EnBw_FlipBack(EnBw* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.shape.rot.z, 0, 1, 0xFA0, 0);
    Math_SmoothStepToF(&this->flameScale, BW_FLAME_LIT_SCALE, 1.0f, 0.05f, 0.0f);
    SkelAnime_Update(&this->skelAnime);
    if (this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH)) {
        Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, 30.0f, 11, 4.0f, 0, 0, false);
        this->timer = 3000;
        this->wobbleBoost = 0.0f;
        Actor_PlaySfx(&this->actor, BW_SFX_LAND);
        this->actionFunc = EnBw_SettleAfterLanding;
    }
    this->bodyColor.r = MIN(this->bodyColor.r + 8, 255);
    this->bodyColor.g = MAX(this->bodyColor.g - 32, 0);
    this->bodyColor.b = MAX(this->bodyColor.b - 40, 0);
    if (this->actor.shape.yOffset > 0.0f) {
        this->actor.shape.yOffset -= 200.0f;
    }
}

static void EnBw_Dying(EnBw* this, PlayState* play) {
    if (this->isFadingWide) {
        this->actor.scale.x += 0.0002f;
        this->actor.scale.y -= 0.0002f;
        this->actor.scale.z += 0.0002f;
    }
    this->bodyColor.a -= 6;
    this->actor.shape.shadowAlpha = this->bodyColor.a;
    this->timer--;
    if (this->timer <= 0) {
        Actor_Kill(&this->actor);
    }
}

static void EnBw_SetupDying(EnBw* this) {
    this->state = BW_STATE_DYING;
    this->timer = 40;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.speed = 0.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_BUBLEWALK_DEAD);
    this->actionFunc = EnBw_Dying;
}

static void EnBw_Squash(EnBw* this, PlayState* play) {
    this->isFadingWide = false;
    this->actor.scale.y -= 0.009f;
    Actor_SpawnFloorDustRing(play, &this->actor, &this->actor.world.pos, 30.0f, 11, 4.0f, 0, 0, false);
}

static void EnBw_Die(EnBw* this, PlayState* play) {
    this->isFadingWide = true;
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x90);
    EnBw_SetupDying(this);
}

static void EnBw_Doused(EnBw* this, PlayState* play) {
    this->bodyColor.r = MAX(this->bodyColor.r - 1, 200);
    this->bodyColor.g = MIN(this->bodyColor.g + 4, 200);
    this->bodyColor.b = MIN(this->bodyColor.b + 5, 230);
    if (COLORFILTER_GET_COLORFLAG(this->actor.colorFilterParams) == COLORFILTER_COLORFLAG_RED) {
        if ((play->gameplayFrames % 0x80) == 0) {
            this->jiggleSpeed = 0.5f + Rand_ZeroOne() * 0.25f;
        }
        this->jiggleAngle += this->jiggleSpeed;
        Math_SmoothStepToF(&this->jiggleAmount, 0.075f, 1.0f, 0.005f, 0.0f);
    }
    this->timer--;
    if (this->timer == 0) {
        EnBw_StartFleeing(this);
    }
}

static void EnBw_SetupDoused(EnBw* this) {
    this->state = BW_STATE_DOUSED;
    this->actor.speed = 0.0f;
    this->jiggleSpeed = (Rand_ZeroOne() * 0.25f) + 1.0f;
    this->jiggleAmount = 0.0f;
    bool isHurt = COLORFILTER_GET_COLORFLAG(this->actor.colorFilterParams) == COLORFILTER_COLORFLAG_RED;
    this->timer = isHurt ? 25 : 80;
    this->actionFunc = EnBw_Doused;
}

static void EnBw_Frozen(EnBw* this, PlayState* play) {
    if (this->actor.colorFilterTimer != 0) {
        return;
    }
    this->freezeTimer = 0;
    if (this->drawDmgEffAlpha != 0.0f) {
        this->drawDmgEffAlpha = 0.0f;
        Actor_SpawnIceEffects(play, &this->actor, &this->actor.world.pos, 1, 4, 0.5f, 0.4f);
    }
    if (this->actor.colChkInfo.health == 0) {
        Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x90);
        this->isFadingWide = true;
        EnBw_SetupDying(this);
    } else if (this->state == BW_STATE_FLIPPED) {
        EnBw_SetupFlipBack(this);
    } else if (this->state == BW_STATE_FLIPPING_BACK) {
        this->actionFunc = EnBw_FlipBack;
    } else {
        EnBw_StartFleeing(this);
    }
}

static void EnBw_SetupFrozen(EnBw* this) {
    this->actor.speed = 0.0f;
    if (this->damageEffect == BW_DMGEFF_ICE) {
        this->drawDmgEffAlpha = 1.0f;
    }
    this->freezeTimer = this->actor.colorFilterTimer;
    Actor_PlaySfx(&this->actor, BW_SFX_FREEZE);
    this->actionFunc = EnBw_Frozen;
}

static void EnBw_ApplyHit(EnBw* this, PlayState* play) {
    this->bodyCollider.base.acFlags &= ~AC_HIT;
    if ((this->actor.colChkInfo.damageEffect == BW_DMGEFF_NONE) || (this->state == BW_STATE_FLIPPING_BACK)) {
        return;
    }
    this->damageEffect = this->actor.colChkInfo.damageEffect;
    Actor_SetDropFlag(&this->actor, &this->bodyCollider.elem);
    if ((this->damageEffect == BW_DMGEFF_STUN) || (this->damageEffect == BW_DMGEFF_ICE)) {
        if (this->freezeTimer == 0) {
            Actor_ApplyDamage(&this->actor);
            Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 120, COLORFILTER_BUFFLAG_OPA, 80);
            EnBw_SetupFrozen(this);
            this->flameScale = 0.0f;
        }
        return;
    }
    // A lit slug only loses its flame; only a doused one fleeing or flipped takes damage, so 0 health always dies.
    bool isVulnerable = (this->flameScale == 0.0f) && ((this->mood == BW_MOOD_FLEE) || (this->mood == BW_MOOD_FLIPPED));
    if (isVulnerable) {
        Actor_ApplyDamage(&this->actor);
    }
    if (isVulnerable && (this->actor.colChkInfo.health == 0)) {
        if (this->state != BW_STATE_DYING) {
            Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
            Enemy_StartFinishingBlow(play, &this->actor);
            EnBw_Die(this, play);
        }
        return;
    }
    if ((this->state != BW_STATE_DOUSED) && (this->state != BW_STATE_FLIPPING_BACK)) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_BUBLEWALK_DAMAGE);
        Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 255, COLORFILTER_BUFFLAG_OPA, 8);
        if (this->state != BW_STATE_FLIPPED) {
            EnBw_SetupDoused(this);
        }
        this->flameScale = 0.0f;
    }
}

static void EnBw_CheckDamage(EnBw* this, PlayState* play) {
    bool isCrushed =
        (this->actor.bgCheckFlags & BGCHECKFLAG_CEILING) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND);
    if (isCrushed && (this->state != BW_STATE_DYING)) {
        EnBw_Squash(this, play);
        EnBw_SetupDying(this);
        return;
    }
    if (this->bodyCollider.base.acFlags & AC_HIT) {
        EnBw_ApplyHit(this, play);
    }
    // OoT's hammer quake flips it over; in MM the Goron pound raises the same flag.
    bool isQuakeNear = (play->actorCtx.unk2 != 0) && (this->actor.xzDistToPlayer <= BW_FLIP_RANGE) &&
                       (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND);
    if (!isQuakeNear) {
        return;
    }
    if (this->state == BW_STATE_FLIPPED) {
        this->freezeTimer = 0;
        EnBw_SetupFlipBack(this);
    } else if (this->state != BW_STATE_DYING) {
        this->freezeTimer = 0;
        EnBw_SetupFlipOver(this);
    }
}

static void EnBw_SpawnFlameParticles(EnBw* this, PlayState* play) {
    static Color_RGBA8 sFlamePrim = { 255, 200, 0, 255 };
    static Color_RGBA8 sFlameEnv = { 255, 80, 0, 255 };
    Vec3f velocity = { Rand_CenteredFloat(this->flameScale * 24.0f), this->flameScale * 30.0f,
                       Rand_CenteredFloat(this->flameScale * 24.0f) };
    Vec3f accel = { velocity.x * -0.075f, -3.0f, velocity.z * -0.075f };

    func_800B0EB0(play, &this->actor.world.pos, &velocity, &accel, &sFlamePrim, &sFlameEnv, 60, 0, 20);
}

static void EnBw_SpawnSmoke(EnBw* this, PlayState* play) {
    Color_RGBA8 smokePrim = { 0, 0, 0, this->bodyColor.a };
    Color_RGBA8 smokeEnv = { 0, 0, 0, (smokePrim.a >= 30) ? (smokePrim.a - 30) : 0 };
    Vec3f velocity = { Rand_CenteredFloat(4.0f), Rand_CenteredFloat(2.0f) + 6.0f, Rand_CenteredFloat(4.0f) };
    Vec3f accel = { velocity.x * -0.1f, -0.1f, velocity.z * -0.1f };

    func_800B0EB0(play, &this->actor.world.pos, &velocity, &accel, &smokePrim, &smokeEnv, 180, 40,
                  20.0f - (this->flameScale * 40.0f));
}

static void EnBw_UpdateFlame(EnBw* this, PlayState* play) {
    if ((play->gameplayFrames & this->mood) == 0) {
        EnBw_SpawnFlameParticles(this, play);
    }
    if (this->flameScale > BW_FLAME_OUT_SCALE) {
        this->flameCollider.elem.atDmgInfo.effect = 1;
        return;
    }
    this->flameCollider.elem.atDmgInfo.effect = 0;
    if (((play->gameplayFrames & 1) == 0) && (this->state < BW_STATE_FLIPPED) && (this->freezeTimer == 0)) {
        EnBw_SpawnSmoke(this, play);
    }
}

static void EnBw_Update(Actor* thisx, PlayState* play) {
    EnBw* this = (EnBw*)thisx;

    EnBw_CheckDamage(this, play);
    this->actionFunc(this, play);
    if (this->freezeTimer == 0) {
        this->bodyScroll = (this->bodyScroll + 4) & 0x7F;
    }
    EnBw_UpdateFlame(this, play);
    this->hasFloorAhead = Actor_TestFloorInDirection(thisx, play, 50.0f, thisx->world.rot.y);
    bool ignoresLedges = (this->state == BW_STATE_LUNGE) || (this->state == BW_STATE_FLIPPED) ||
                         (this->state == BW_STATE_FLIPPING_BACK) || (this->state == BW_STATE_DOUSED);
    if (ignoresLedges || (this->hasFloorAhead != 0)) {
        Actor_MoveWithGravity(thisx);
    }
    Actor_UpdateBgCheckInfo(play, thisx, 20.0f, 30.0f, 21.0f, BW_BGCHECK_FLAGS);
    Collider_UpdateCylinder(thisx, &this->bodyCollider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
    bool isHurtFiltered = (thisx->colorFilterTimer != 0) &&
                          (COLORFILTER_GET_COLORFLAG(thisx->colorFilterParams) == COLORFILTER_COLORFLAG_RED);
    if ((this->state != BW_STATE_DYING) && !isHurtFiltered) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
    }
    if ((this->mood != BW_MOOD_FLEE) && (this->state < BW_STATE_FLIPPED) && (this->flameScale > BW_FLAME_OUT_SCALE)) {
        Collider_UpdateCylinder(thisx, &this->flameCollider);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->flameCollider.base);
    }
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 5.0f;
}

static void EnBw_Init(Actor* thisx, PlayState* play) {
    EnBw* this = (EnBw*)thisx;

    Actor_SetScale(&this->actor, BW_BASE_SCALE);
    this->actor.gravity = -2.0f;
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sSkel, ANIM(sEyestalkWaveAnim), this->jointTable,
                   this->morphTable, BW_LIMB_MAX);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 40.0f);
    this->actor.colChkInfo.damageTable = &sDamageTable;
    this->actor.colChkInfo.health = 6;
    this->actor.colChkInfo.mass = MASS_HEAVY;
    this->actor.focus.pos = this->actor.world.pos;
    EnBw_SetupCrawl(this);
    this->bodyColor.r = 255;
    this->bodyColor.a = 255;
    this->flameScale = BW_FLAME_LIT_SCALE;
    this->mood = BW_MOOD_WANDER;
    Collider_InitCylinder(play, &this->flameCollider);
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->flameCollider, &this->actor, &sFlameColliderInit);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBodyColliderInit);
    this->heading = this->actor.world.rot.y;
    // Spreads the costly wall probes of a group of slugs over four frames.
    this->probeGroup = sNextProbeGroup;
    sNextProbeGroup = (sNextProbeGroup + 1) & 3;
}

static void EnBw_Destroy(Actor* thisx, PlayState* play) {
    EnBw* this = (EnBw*)thisx;

    Collider_DestroyCylinder(play, &this->flameCollider);
    Collider_DestroyCylinder(play, &this->bodyCollider);
}

static s32 EnBw_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx,
                                 Gfx** gfx) {
    EnBw* this = (EnBw*)thisx;

    if (limbIndex != BW_LIMB_BODY) {
        return false;
    }
    gSPSegment((*gfx)++, 0x09,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x20, 1, 0, this->bodyScroll, 0x20,
                                             0x20, 0, 0, 0, 1));
    if ((this->state != BW_STATE_DOUSED) && (this->state != BW_STATE_FLIPPED)) {
        return false;
    }
    Matrix_Push();
    Matrix_RotateXF(this->jiggleAngle * 0.115f, MTXMODE_APPLY);
    Matrix_RotateYF(this->jiggleAngle * 0.13f, MTXMODE_APPLY);
    Matrix_RotateZF(this->jiggleAngle * 0.1f, MTXMODE_APPLY);
    Matrix_Scale(1.0f - this->jiggleAmount, 1.0f + this->jiggleAmount, 1.0f - this->jiggleAmount, MTXMODE_APPLY);
    Matrix_RotateZF(-(this->jiggleAngle * 0.1f), MTXMODE_APPLY);
    Matrix_RotateYF(-(this->jiggleAngle * 0.13f), MTXMODE_APPLY);
    Matrix_RotateXF(-(this->jiggleAngle * 0.115f), MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD((*gfx)++, play->state.gfxCtx);
    gSPDisplayList((*gfx)++, *dList);
    Matrix_Pop();
    return true;
}

static void EnBw_UpdateProbes(EnBw* this, PlayState* play) {
    Vec3f offset = { 0.0f, 0.0f, 0.0f };

    if (((play->gameplayFrames + 1) % 4) != (u32)this->probeGroup) {
        return;
    }
    offset.z = this->actor.scale.z * 375000.0f;
    Matrix_MultVec3f(&offset, &this->probeFront);
    offset.z = this->actor.scale.z * 150000.0f;
    Matrix_MultVec3f(&offset, &this->probeNear);
    offset.x = this->actor.scale.x * 550000.0f;
    Matrix_MultVec3f(&offset, &this->probeRight);
    offset.x = -offset.x;
    Matrix_MultVec3f(&offset, &this->probeLeft);
}

static void EnBw_DrawBody(EnBw* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    if (this->bodyColor.a == 255) {
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        gDPSetEnvColor(POLY_OPA_DISP++, this->bodyColor.r, this->bodyColor.g, this->bodyColor.b, this->bodyColor.a);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sEmptyDL);
        POLY_OPA_DISP = SkelAnime_Draw(play, this->skelAnime.skeleton, this->skelAnime.jointTable,
                                       EnBw_OverrideLimbDraw, NULL, &this->actor, POLY_OPA_DISP);
    } else {
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 0, 0, 0, this->bodyColor.a);
        gDPSetEnvColor(POLY_XLU_DISP++, this->bodyColor.r, this->bodyColor.g, this->bodyColor.b, this->bodyColor.a);
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)sXluRenderModeDL);
        POLY_XLU_DISP = SkelAnime_Draw(play, this->skelAnime.skeleton, this->skelAnime.jointTable,
                                       EnBw_OverrideLimbDraw, NULL, &this->actor, POLY_XLU_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnBw_DrawFlame(EnBw* this, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(this->actor.world.pos.x,
                     this->actor.world.pos.y + ((this->actor.scale.y - BW_BASE_SCALE) * 1000.0f),
                     this->actor.world.pos.z, MTXMODE_NEW);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, 0);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0,
                                             (play->gameplayFrames * -20) % 0x200, 0x20, 0x80, 0, 0, 0, -20));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 0, 255);
    Matrix_Scale(this->flameScale * 0.01f, this->flameScale * 0.01f, this->flameScale * 0.01f, MTXMODE_APPLY);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnBw_Draw(Actor* thisx, PlayState* play) {
    EnBw* this = (EnBw*)thisx;

    EnBw_DrawBody(this, play);
    EnBw_UpdateProbes(this, play);
    EnBw_DrawFlame(this, play);
    Actor_DrawDamageEffects(play, &this->actor, &this->actor.world.pos, 1, 0.5f, 0.4f, this->drawDmgEffAlpha,
                            ACTOR_DRAW_DMGEFF_FROZEN_NO_SFX);
}

// Ship (RE_TORCH_SLUG): a jumpslash (sword or Goron here), explosives or the bow.
static bool CanKillTorchSlug(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicHasCapability("CanBeGoron") ||
           sApi->LogicHasCapability("HasExplosives") || sApi->LogicCanUseItem(RI_BOW);
}

static s16 RegisterTorchSlug(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = TORCH_SLUG_KEY;
    definition.description = "Torch Slug";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = BW_FLAGS;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnBw);
    definition.init = EnBw_Init;
    definition.destroy = EnBw_Destroy;
    definition.update = EnBw_Update;
    definition.draw = EnBw_Draw;
    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    definition.naviHint = "Torch Slug&Its flame burns you. Put the fire out&with a blow, then strike again!";
    definition.soulKey = TORCH_SLUG_KEY;
    definition.logicKill = CanKillTorchSlug;
    definition.enemyCategory = S2H_ENEMY_CATEGORY_REGULAR;
    definition.soulModel.skeleton = sSkel;
    definition.soulModel.idleAnimation = sEyestalkWaveAnim;
    definition.soulModel.limbCount = BW_LIMB_MAX;
    definition.soulModel.scale = 0.013f;
    definition.soulModel.flameRed = 255;
    definition.soulModel.flameGreen = 120;
    definition.soulModel.flameBlue = 40;
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
    if (!RequireOotAsset(BW_OBJECT "gTorchSlugSkel")) {
        sApi->Log(TORCH_SLUG_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    if (RegisterTorchSlug() < 0) {
        sApi->Log("Could not register " TORCH_SLUG_KEY);
    }
}
