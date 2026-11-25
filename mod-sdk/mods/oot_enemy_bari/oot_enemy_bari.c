/*
 * Ocarina of Time's Bari (En_Vali) and Biri (En_Bili) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * A beaten Bari splits into three Biri; both enemies share one soul, as in Ship.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define BARI_KEY "oot.bari"
#define BIRI_KEY "oot.biri"
#define BARI_SOUL_KEY "soul.oot.bari"
#define VALI_OBJECT "objects/oot_object_vali/"
#define VALI_ASSET(name) "__OTR__" VALI_OBJECT name
#define BL_OBJECT "objects/oot_object_bl/"
#define BL_ASSET(name) "__OTR__" BL_OBJECT name

static const ALIGN_ASSET(2) char sBariSkel[] = VALI_ASSET("gBariSkel");
static const ALIGN_ASSET(2) char sBariLurkingAnim[] = VALI_ASSET("gBariLurkingAnim");
static const ALIGN_ASSET(2) char sBariWaitingAnim[] = VALI_ASSET("gBariWaitingAnim");
static const ALIGN_ASSET(2) char sBariRetaliatingAnim[] = VALI_ASSET("gBariRetaliatingAnim");
static const ALIGN_ASSET(2) char sBariMovingArmsDownAnim[] = VALI_ASSET("gBariMovingArmsDownAnim");
static const ALIGN_ASSET(2) char sBariInnerHoodDL[] = VALI_ASSET("gBariInnerHoodDL");
static const ALIGN_ASSET(2) char sBariNucleusDL[] = VALI_ASSET("gBariNucleusDL");
static const ALIGN_ASSET(2) char sBariOuterHoodDL[] = VALI_ASSET("gBariOuterHoodDL");
static const ALIGN_ASSET(2) char sBiriSkel[] = BL_ASSET("gBiriSkel");
static const ALIGN_ASSET(2) char sBiriDefaultAnim[] = BL_ASSET("gBiriDefaultAnim");
static const ALIGN_ASSET(2) char sBiriDischargeLightningAnim[] = BL_ASSET("gBiriDischargeLightningAnim");
static const ALIGN_ASSET(2) char sBiriClimbAnim[] = BL_ASSET("gBiriClimbAnim");
static const ALIGN_ASSET(2) char sBiriTentacles0Tex[] = BL_ASSET("gBiriTentacles0Tex");
static const ALIGN_ASSET(2) char sBiriTentacles1Tex[] = BL_ASSET("gBiriTentacles1Tex");
static const ALIGN_ASSET(2) char sBiriTentacles2Tex[] = BL_ASSET("gBiriTentacles2Tex");
static const ALIGN_ASSET(2) char sBiriTentacles3Tex[] = BL_ASSET("gBiriTentacles3Tex");
static const ALIGN_ASSET(2) char sBiriTentacles4Tex[] = BL_ASSET("gBiriTentacles4Tex");
static const ALIGN_ASSET(2) char sBiriTentacles5Tex[] = BL_ASSET("gBiriTentacles5Tex");
static const ALIGN_ASSET(2) char sBiriTentacles6Tex[] = BL_ASSET("gBiriTentacles6Tex");
static const ALIGN_ASSET(2) char sBiriTentacles7Tex[] = BL_ASSET("gBiriTentacles7Tex");

static const char* const sBiriTentaclesTextures[] = {
    sBiriTentacles0Tex, sBiriTentacles1Tex, sBiriTentacles2Tex, sBiriTentacles3Tex,
    sBiriTentacles4Tex, sBiriTentacles5Tex, sBiriTentacles6Tex, sBiriTentacles7Tex,
};

// MM reuses these OoT ids for other sounds, so these are MM stand-ins.
#define JELLYFISH_SFX_LAND NA_SE_EN_DAIOCTA_LAND
#define BIRI_SFX_BUBBLE NA_SE_EN_COMMON_WATER_DEEP

#define ANIM(asset) ((AnimationHeader*)(asset))
#define LAST_FRAME(asset) Animation_GetLastFrame((void*)(asset))

#define VALI_LIMB_NUCLEUS 0x02
#define VALI_LIMB_LEFT_ARM_BASE 0x09
#define VALI_LIMB_LEFT_FOREARM_BASE 0x0B
#define VALI_LIMB_RIGHT_ARM_BASE 0x12
#define VALI_LIMB_RIGHT_FOREARM_BASE 0x14
#define VALI_LIMB_INNER_HOOD 0x1B
#define VALI_LIMB_OUTER_HOOD 0x1C
#define VALI_LIMB_MAX 0x1D

#define BILI_LIMB_INNER_HOOD 2
#define BILI_LIMB_OUTER_HOOD 3
#define BILI_LIMB_TENTACLES 4
#define BILI_LIMB_MAX 5

typedef enum {
    JELLYFISH_DMGEFF_NONE = 0x0,
    JELLYFISH_DMGEFF_STUN = 0x1,
    JELLYFISH_DMGEFF_FIRE = 0x2,
    JELLYFISH_DMGEFF_ICE = 0x3,
    JELLYFISH_DMGEFF_SWORD = 0xF,
} JellyfishDamageEffect;

typedef enum {
    BIRI_TYPE_NORMAL = -1,
    BIRI_TYPE_BARI_SPAWNED = 0,
    BIRI_TYPE_DYING = 1,
} BiriType;

#define BARI_FLAGS \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_IGNORE_QUAKE)
// Unlike vanilla it updates while culled: a randomized spot high above the camera froze it out of reach.
#define BIRI_FLAGS                                                                            \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_IGNORE_QUAKE | ACTOR_FLAG_CAN_ATTACH_TO_ARROW)
#define BIRI_BGCHECK_FLAGS (UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_4)
#define BARI_LURK_HEIGHT 150
#define ARROW_DMG_FLAGS (DMG_NORMAL_ARROW | DMG_FIRE_ARROW | DMG_ICE_ARROW | DMG_LIGHT_ARROW | DMG_DEKU_BUBBLE)

static const S2HModApi* sApi;
static s16 sBiriId = -1;

struct EnVali;
typedef void (*EnValiActionFunc)(struct EnVali*, PlayState*);

typedef struct EnVali {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[VALI_LIMB_MAX];
    Vec3s morphTable[VALI_LIMB_MAX];
    EnValiActionFunc actionFunc;
    ColliderQuad leftArmCollider;
    ColliderQuad rightArmCollider;
    ColliderCylinder bodyCollider;
    f32 armScale;
    f32 floatHomeHeight;
    f32 drawDmgEffAlpha;
    s16 timer;
    u8 lightningTimer;
    u8 slingshotReactionTimer;
    u8 wasHitBySword;
} EnVali;

struct EnBili;
typedef void (*EnBiliActionFunc)(struct EnBili*, PlayState*);

typedef struct EnBili {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[BILI_LIMB_MAX];
    Vec3s morphTable[BILI_LIMB_MAX];
    EnBiliActionFunc actionFunc;
    ColliderCylinder collider;
    f32 drawDmgEffAlpha;
    s16 timer;
    u8 tentaclesTexIndex;
    u8 playFlySound;
} EnBili;

static Gfx sChargedCombineDL[] = {
    gsDPSetCombineLERP(1, TEXEL0, SHADE, 0, TEXEL0, 0, PRIMITIVE, 0, COMBINED, 0, PRIMITIVE, 0, TEXEL1, 0,
                       PRIM_LOD_FRAC, COMBINED),
    gsSPEndDisplayList(),
};
static Gfx sNormalCombineDL[] = {
    gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, TEXEL0, 0, PRIMITIVE, 0, COMBINED, 0, PRIMITIVE, 0, TEXEL1, 0,
                       PRIM_LOD_FRAC, COMBINED),
    gsSPEndDisplayList(),
};

static ColliderQuadInit sBariArmColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_1,
        COLSHAPE_QUAD,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x07, 0x08 },
        { 0x00000000, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NONE,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } } },
};

static ColliderCylinderInit sBariBodyColliderInit = {
    {
        COL_MATERIAL_HIT8,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x07, 0x08 },
        { 0xF7CFFFFF, 0x01, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 17, 35, -15, { 0, 0, 0 } },
};

static ColliderCylinderInit sBiriColliderInit = {
    {
        COL_MATERIAL_HIT8,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x03, 0x08 },
        { 0xF7CFFFFF, 0x01, 0x00 },
        ATELEM_ON | ATELEM_SFX_NONE,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 9, 28, -20, { 0, 0, 0 } },
};

static CollisionCheckInfoInit sBariColChkInfoInit = { 2, 18, 32, MASS_HEAVY };
static CollisionCheckInfoInit2 sBiriColChkInfoInit = { 1, 9, 28, -20, 30 };

// The sword and the Deku, Zora and Goron melee touch the jellyfish and get shocked.
static DamageTable sBariDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, JELLYFISH_DMGEFF_STUN),
    /* Deku Stick     */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Horse trample  */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Zora boomerang */ DMG_ENTRY(0, JELLYFISH_DMGEFF_STUN),
    /* Normal arrow   */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Goron punch    */ DMG_ENTRY(1, JELLYFISH_DMGEFF_SWORD),
    /* Sword          */ DMG_ENTRY(1, JELLYFISH_DMGEFF_SWORD),
    /* Goron pound    */ DMG_ENTRY(2, JELLYFISH_DMGEFF_NONE),
    /* Fire arrow     */ DMG_ENTRY(2, JELLYFISH_DMGEFF_FIRE),
    /* Ice arrow      */ DMG_ENTRY(2, JELLYFISH_DMGEFF_ICE),
    /* Light arrow    */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Goron spikes   */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Deku spin      */ DMG_ENTRY(1, JELLYFISH_DMGEFF_SWORD),
    /* Deku bubble    */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Deku launch    */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Zora barrier   */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Normal shield  */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Zora punch     */ DMG_ENTRY(1, JELLYFISH_DMGEFF_SWORD),
    /* Spin attack    */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Sword beam     */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Normal Roll    */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(2, JELLYFISH_DMGEFF_NONE),
};

static DamageTable sBiriDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, JELLYFISH_DMGEFF_STUN),
    /* Deku Stick     */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Horse trample  */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Explosives     */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Zora boomerang */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Normal arrow   */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Hookshot       */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Goron punch    */ DMG_ENTRY(1, JELLYFISH_DMGEFF_SWORD),
    /* Sword          */ DMG_ENTRY(1, JELLYFISH_DMGEFF_SWORD),
    /* Goron pound    */ DMG_ENTRY(2, JELLYFISH_DMGEFF_NONE),
    /* Fire arrow     */ DMG_ENTRY(2, JELLYFISH_DMGEFF_FIRE),
    /* Ice arrow      */ DMG_ENTRY(2, JELLYFISH_DMGEFF_ICE),
    /* Light arrow    */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Goron spikes   */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Deku spin      */ DMG_ENTRY(1, JELLYFISH_DMGEFF_SWORD),
    /* Deku bubble    */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Deku launch    */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Zora barrier   */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Normal shield  */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Light ray      */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Thrown object  */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Zora punch     */ DMG_ENTRY(1, JELLYFISH_DMGEFF_SWORD),
    /* Spin attack    */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Sword beam     */ DMG_ENTRY(1, JELLYFISH_DMGEFF_NONE),
    /* Normal Roll    */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Unblockable    */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, JELLYFISH_DMGEFF_NONE),
    /* Powder Keg     */ DMG_ENTRY(2, JELLYFISH_DMGEFF_NONE),
};

static InitChainEntry sBariInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 10, ICHAIN_CONTINUE),
    ICHAIN_F32(lockOnArrowOffset, 5000, ICHAIN_STOP),
};

static InitChainEntry sBiriInitChain[] = {
    ICHAIN_F32(lockOnArrowOffset, 2000, ICHAIN_STOP),
};

static void EnVali_Draw(Actor* thisx, PlayState* play);
static void EnVali_FloatIdle(EnVali* this, PlayState* play);
static void EnBili_SetupFloatIdle(EnBili* this);

static void SpawnDeathBubbles(PlayState* play, Vec3f* center, f32 spreadXZ, f32 spreadY) {
    static Vec3f sAccel = { 0.0f, 0.0f, 0.0f };

    for (s32 i = 0; i < 2; i++) {
        Vec3f velocity = { 0.0f, Rand_ZeroOne() + 1.0f, 0.0f };
        Vec3f pos;
        pos.x = center->x + Rand_CenteredFloat(spreadXZ);
        pos.y = center->y + Rand_CenteredFloat(spreadY);
        pos.z = center->z + Rand_CenteredFloat(spreadXZ);
        s16 colorProfile = (Rand_ZeroOne() < 0.7f) ? 2 : 0;
        EffectSsDtBubble_SpawnColorProfile(play, &pos, &velocity, &sAccel, Rand_S16Offset(40, 40), 25, colorProfile, 1);
    }
}

static void EnVali_Lurk(EnVali* this, PlayState* play);
static void EnVali_DropAppear(EnVali* this, PlayState* play);

static void EnVali_SetupLurk(EnVali* this) {
    Animation_PlayLoop(&this->skelAnime, ANIM(sBariLurkingAnim));
    this->actor.draw = NULL;
    this->bodyCollider.base.acFlags &= ~AC_ON;
    this->actionFunc = EnVali_Lurk;
}

static void EnVali_SetupFloatIdle(EnVali* this) {
    Animation_MorphToLoop(&this->skelAnime, ANIM(sBariWaitingAnim), -3.0f);
    Vec3f armRest = this->actor.world.pos;
    armRest.y -= 10.0f;
    for (s32 i = 0; i < 4; i++) {
        this->leftArmCollider.dim.quad[i] = armRest;
        this->rightArmCollider.dim.quad[i] = armRest;
    }
    this->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    this->bodyCollider.base.acFlags |= AC_ON;
    this->slingshotReactionTimer = 0;
    this->floatHomeHeight = this->actor.world.pos.y;
    this->actionFunc = EnVali_FloatIdle;
}

static void EnVali_DischargeLightning(EnVali* this, PlayState* play) {
    static Color_RGBA8 sPrimColor = { 255, 255, 255, 255 };
    static Color_RGBA8 sEnvColor = { 200, 255, 255, 255 };
    s16 camYaw = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play));
    f32 cos = -Math_CosS(camYaw);
    f32 sin = Math_SinS(camYaw);

    for (s32 i = 0; i < 4; i++) {
        if (((this->lightningTimer + (i << 1)) % 4) != 0) {
            continue;
        }
        s16 yaw = (s16)Rand_CenteredFloat(12288.0f) + (i * 0x4000) + 0x2000;
        Vec3f pos;
        pos.x = this->actor.world.pos.x + (Math_SinS(yaw) * 12.0f * cos);
        pos.y = this->actor.world.pos.y - (Math_CosS(yaw) * 12.0f) + 10.0f;
        pos.z = this->actor.world.pos.z + (Math_SinS(yaw) * 12.0f * sin);
        EffectSsLightning_Spawn(play, &pos, &sPrimColor, &sEnvColor, 17, yaw, 6, 2);
    }
    Actor_PlaySfx_Flagged(&this->actor, NA_SE_EN_BIRI_SPARK - SFX_FLAG);
}

static void EnVali_Retaliate(EnVali* this, PlayState* play);
static void EnVali_MoveArmsDown(EnVali* this, PlayState* play);
static void EnVali_DivideAndDie(EnVali* this, PlayState* play);

static void EnVali_SetupRetaliate(EnVali* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sBariRetaliatingAnim), -5.0f);
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 150, COLORFILTER_BUFFLAG_XLU, 30);
    this->wasHitBySword = false;
    this->bodyCollider.base.acFlags &= ~AC_ON;
    this->actionFunc = EnVali_Retaliate;
}

// Touching the player and a sword blow both discharge; only the sword blow retaliates afterwards.
static void EnVali_Attacked(EnVali* this, PlayState* play) {
    if (this->lightningTimer != 0) {
        this->lightningTimer--;
    }
    EnVali_DischargeLightning(this, play);
    if (this->lightningTimer == 0) {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        this->bodyCollider.base.acFlags |= AC_ON;
        if (this->wasHitBySword) {
            EnVali_SetupRetaliate(this);
        } else {
            this->actionFunc = EnVali_FloatIdle;
        }
    } else if ((this->lightningTimer % 2) != 0) {
        this->actor.world.pos.y += 1.0f;
    } else {
        this->actor.world.pos.y -= 1.0f;
    }
}

static void EnVali_SetupAttacked(EnVali* this) {
    this->lightningTimer = 20;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->bodyCollider.base.acFlags &= ~AC_ON;
    this->actionFunc = EnVali_Attacked;
}

static void EnVali_SetupDivideAndDie(EnVali* this, PlayState* play) {
    for (s32 i = 0; i < 3; i++) {
        Actor_Spawn(&play->actorCtx, play, sBiriId, this->actor.world.pos.x, this->actor.world.pos.y,
                    this->actor.world.pos.z, 0, this->actor.world.rot.y, 0, BIRI_TYPE_BARI_SPAWNED);
        this->actor.world.rot.y += 0x10000 / 3;
    }
    Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x50);
    this->timer = Rand_S16Offset(10, 10);
    this->bodyCollider.base.acFlags &= ~AC_ON;
    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EN_BARI_SPLIT);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.draw = NULL;
    this->actionFunc = EnVali_DivideAndDie;
}

static void EnVali_Retaliate(EnVali* this, PlayState* play) {
    if (!SkelAnime_Update(&this->skelAnime)) {
        return;
    }
    if (this->actor.colChkInfo.health == 0) {
        EnVali_SetupDivideAndDie(this, play);
        return;
    }
    Animation_PlayOnce(&this->skelAnime, ANIM(sBariMovingArmsDownAnim));
    this->actionFunc = EnVali_MoveArmsDown;
}

static void EnVali_MoveArmsDown(EnVali* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        EnVali_SetupFloatIdle(this);
    }
}

static void EnVali_Burnt(EnVali* this, PlayState* play) {
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer == 0) {
        EnVali_SetupDivideAndDie(this, play);
    }
}

static void EnVali_DivideAndDie(EnVali* this, PlayState* play) {
    if (this->timer != 0) {
        this->timer--;
    }
    SpawnDeathBubbles(play, &this->actor.world.pos, 20.0f, 8.0f);
    if (this->timer == 0) {
        Actor_Kill(&this->actor);
    }
}

static bool EnVali_SinkToFloor(EnVali* this) {
    return Math_StepToF(&this->actor.world.pos.y, this->actor.floorHeight, this->actor.velocity.y);
}

static void EnVali_Stunned(EnVali* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->actor.velocity.y != 0.0f) {
        if (EnVali_SinkToFloor(this)) {
            this->actor.velocity.y = 0.0f;
            Actor_PlaySfx(&this->actor, JELLYFISH_SFX_LAND);
        } else {
            this->actor.velocity.y += 1.0f;
        }
    }
    if (this->timer == 0) {
        this->bodyCollider.elem.acDmgInfo.effect = 1;
        EnVali_SetupFloatIdle(this);
    }
}

static void EnVali_Frozen(EnVali* this, PlayState* play) {
    if (this->timer != 0) {
        this->timer--;
    }
    this->actor.colorFilterTimer = 36;
    if (this->timer != 0) {
        return;
    }
    this->actor.velocity.y += 1.0f;
    if (EnVali_SinkToFloor(this)) {
        this->drawDmgEffAlpha = 0.0f;
        Actor_SpawnIceEffects(play, &this->actor, &this->actor.world.pos, 1, 6, 0.6f, 0.4f);
        EnVali_SetupDivideAndDie(this, play);
        this->actor.colorFilterTimer = 0;
    }
}

static void EnVali_ReturnToLurk(EnVali* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Math_SmoothStepToF(&this->actor.world.pos.y, this->actor.home.pos.y, 0.5f, 15.0f, 0.1f) < 0.01f) {
        EnVali_SetupLurk(this);
    }
}

static void EnVali_Lurk(EnVali* this, PlayState* play) {
    if (this->actor.xzDistToPlayer >= 150.0f) {
        return;
    }
    this->actor.draw = EnVali_Draw;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.velocity.y = 1.0f;
    this->actionFunc = EnVali_DropAppear;
}

static void EnVali_DropAppear(EnVali* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->actor.velocity.y = CLAMP_MAX(this->actor.velocity.y * 1.5f, 40.0f);
    if (EnVali_SinkToFloor(this)) {
        Actor_PlaySfx(&this->actor, JELLYFISH_SFX_LAND);
        EnVali_SetupFloatIdle(this);
    }
}

static void EnVali_PlayRollSfx(EnVali* this, s32 curFrame) {
    if (this->slingshotReactionTimer != 0) {
        if (((this->slingshotReactionTimer % 6) == 0) && (curFrame > 15) && (curFrame <= 55)) {
            Actor_PlaySfx(&this->actor, NA_SE_EN_BARI_ROLL);
        }
    } else if ((curFrame == 16) || (curFrame == 30) || (curFrame == 42) || (curFrame == 55)) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_BARI_ROLL);
    }
}

static void EnVali_FloatIdle(EnVali* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->slingshotReactionTimer != 0) {
        this->slingshotReactionTimer--;
    }
    s32 curFrame = this->skelAnime.curFrame;
    Math_StepToF(&this->floatHomeHeight, this->actor.floorHeight + 40.0f, 1.2f);
    this->actor.world.pos.y = this->floatHomeHeight - (sinf(curFrame * M_PIf * 0.0125f) * 8.0f);
    if (this->slingshotReactionTimer != 0) {
        this->actor.shape.rot.y += 0x800;
    }
    EnVali_PlayRollSfx(this, curFrame);
    s32 spinFrame = (curFrame > 40) ? (80 - curFrame) : curFrame;
    this->actor.shape.rot.y += (s16)((spinFrame + 4) * 0.4f * (0x10000 / 360.0f));
    if (this->actor.xzDistToPlayer > 250.0f) {
        Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sBariLurkingAnim), 10.0f);
        this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        this->actionFunc = EnVali_ReturnToLurk;
    }
}

static void EnVali_ReactToHit(EnVali* this, PlayState* play) {
    u8 effect = this->actor.colChkInfo.damageEffect;
    bool isStunned = this->actionFunc == EnVali_Stunned;

    if (effect == JELLYFISH_DMGEFF_STUN) {
        if (isStunned) {
            return;
        }
        Animation_MorphToPlayOnce(&this->skelAnime, ANIM(sBariWaitingAnim), 10.0f);
        this->timer = 80;
        Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 255, COLORFILTER_BUFFLAG_XLU, 80);
        this->bodyCollider.elem.acDmgInfo.effect = 0;
        Actor_PlaySfx(&this->actor, NA_SE_EN_COMMON_FREEZE);
        this->actor.velocity.y = 1.0f;
        this->actionFunc = EnVali_Stunned;
    } else if ((effect == JELLYFISH_DMGEFF_SWORD) && !isStunned) {
        Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 150, COLORFILTER_BUFFLAG_XLU, 30);
        EnVali_SetupAttacked(this);
        this->wasHitBySword = true;
    } else if (effect == JELLYFISH_DMGEFF_FIRE) {
        this->timer = 2;
        this->bodyCollider.base.acFlags &= ~AC_ON;
        Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 150, COLORFILTER_BUFFLAG_XLU, 30);
        this->actionFunc = EnVali_Burnt;
    } else if (effect == JELLYFISH_DMGEFF_ICE) {
        this->actor.velocity.y = 0.0f;
        Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 255, COLORFILTER_BUFFLAG_XLU, 36);
        this->bodyCollider.base.acFlags &= ~AC_ON;
        this->timer = 36;
        this->drawDmgEffAlpha = 1.0f;
        this->actionFunc = EnVali_Frozen;
    } else {
        EnVali_SetupRetaliate(this);
    }
}

static void EnVali_UpdateDamage(EnVali* this, PlayState* play) {
    if (!(this->bodyCollider.base.acFlags & AC_HIT)) {
        return;
    }
    this->bodyCollider.base.acFlags &= ~AC_HIT;
    Actor_SetDropFlag(&this->actor, &this->bodyCollider.elem);
    u8 effect = this->actor.colChkInfo.damageEffect;
    if ((effect == JELLYFISH_DMGEFF_NONE) && (this->actor.colChkInfo.damage == 0)) {
        return;
    }
    if (Actor_ApplyDamage(&this->actor) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_BARI_DEAD);
        Enemy_StartFinishingBlow(play, &this->actor);
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    } else if (effect != JELLYFISH_DMGEFF_STUN) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_BARI_DAMAGE);
    }
    EnVali_ReactToHit(this, play);
}

static void EnVali_Update(Actor* thisx, PlayState* play) {
    EnVali* this = (EnVali*)thisx;
    bool hasTouchedPlayer = (this->bodyCollider.base.atFlags & AT_HIT) ||
                            (this->leftArmCollider.base.atFlags & AT_HIT) ||
                            (this->rightArmCollider.base.atFlags & AT_HIT);

    if (hasTouchedPlayer) {
        this->leftArmCollider.base.atFlags &= ~AT_HIT;
        this->rightArmCollider.base.atFlags &= ~AT_HIT;
        this->bodyCollider.base.atFlags &= ~AT_HIT;
        EnVali_SetupAttacked(this);
    }
    EnVali_UpdateDamage(this, play);
    this->actionFunc(this, play);
    if ((this->actionFunc == EnVali_DivideAndDie) || (this->actionFunc == EnVali_Lurk)) {
        return;
    }
    Collider_UpdateCylinder(&this->actor, &this->bodyCollider);
    if (this->actionFunc == EnVali_FloatIdle) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->leftArmCollider.base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->rightArmCollider.base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->bodyCollider.base);
    }
    if (this->bodyCollider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->bodyCollider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->bodyCollider.base);
    Actor_SetFocus(&this->actor, 0.0f);
}

static void EnVali_Init(Actor* thisx, PlayState* play) {
    EnVali* this = (EnVali*)thisx;
    CollisionPoly* floorPoly;
    s32 bgId;

    Actor_ProcessInitChain(&this->actor, sBariInitChain);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 27.0f);
    this->actor.shape.shadowAlpha = 155;
    this->actor.attentionRangeType = ATTENTION_RANGE_3;
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sBariSkel, ANIM(sBariLurkingAnim), this->jointTable,
                   this->morphTable, VALI_LIMB_MAX);
    Collider_InitQuad(play, &this->leftArmCollider);
    Collider_SetQuad(play, &this->leftArmCollider, &this->actor, &sBariArmColliderInit);
    Collider_InitQuad(play, &this->rightArmCollider);
    Collider_SetQuad(play, &this->rightArmCollider, &this->actor, &sBariArmColliderInit);
    Collider_InitCylinder(play, &this->bodyCollider);
    Collider_SetCylinder(play, &this->bodyCollider, &this->actor, &sBariBodyColliderInit);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sBariDamageTable, &sBariColChkInfoInit);
    EnVali_SetupLurk(this);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.floorHeight =
        BgCheck_EntityRaycastFloor5(&play->colCtx, &floorPoly, &bgId, &this->actor, &this->actor.world.pos);
    this->actor.floorPoly = floorPoly;
    if (this->actor.floorHeight == BGCHECK_Y_MIN) {
        Actor_Kill(&this->actor);
    }
}

static void EnVali_Destroy(Actor* thisx, PlayState* play) {
    EnVali* this = (EnVali*)thisx;

    Collider_DestroyQuad(play, &this->leftArmCollider);
    Collider_DestroyQuad(play, &this->rightArmCollider);
    Collider_DestroyCylinder(play, &this->bodyCollider);
}

static f32 EnVali_GetPulse(EnVali* this, f32 curFrame) {
    if (this->actionFunc == EnVali_Retaliate) {
        return sinf((M_PIf / 10) * curFrame);
    }
    return cosf((M_PIf / 50) * curFrame);
}

// Scales a hood: outer and inner differ only in amplitude.
static void EnVali_PulseHood(EnVali* this, f32 curFrame, Vec3f* scale, f32 idleAmp, f32 squashY, f32 squashXZ,
                             f32 stunSign) {
    if (this->actionFunc == EnVali_Attacked) {
        s32 scalePhase = 20 - (this->lightningTimer % 20);
        if (scalePhase >= 10) {
            scalePhase -= 10;
        }
        scale->y -= idleAmp * sinf((M_PIf / 10) * scalePhase);
    } else if ((this->actionFunc == EnVali_Retaliate) || (this->actionFunc == EnVali_MoveArmsDown)) {
        f32 pulse = EnVali_GetPulse(this, curFrame);
        scale->y -= squashY * pulse;
        scale->x -= squashXZ * pulse;
        scale->z = scale->x;
    } else if (this->actionFunc == EnVali_Stunned) {
        f32 wobble = sinf((M_PIf / 10) * this->timer) * 0.08f * stunSign;
        scale->x += wobble;
        scale->y -= wobble;
        scale->z += wobble;
    } else {
        f32 frame = (curFrame >= 40.0f) ? (curFrame - 40.0f) : curFrame;
        scale->y -= idleAmp * sinf((M_PIf / 40) * frame);
    }
}

static bool EnVali_UpdateArmLength(EnVali* this, f32 curFrame) {
    f32 targetArmScale = 1.0f;

    if (this->actionFunc == EnVali_FloatIdle) {
        if (curFrame <= 10.0f) {
            targetArmScale = curFrame * 0.05f + 1.0f;
        } else if (curFrame > 70.0f) {
            targetArmScale = (80.0f - curFrame) * 0.05f + 1.0f;
        } else {
            targetArmScale = 1.5f;
        }
    } else if ((this->actionFunc == EnVali_Retaliate) || (this->actionFunc == EnVali_MoveArmsDown)) {
        targetArmScale = 1.0f - EnVali_GetPulse(this, curFrame) * 0.35f;
    } else if ((this->actionFunc == EnVali_Attacked) || (this->actionFunc == EnVali_Frozen)) {
        targetArmScale = this->armScale;
    }
    Math_StepToF(&this->armScale, targetArmScale, 0.1f);
    return this->armScale != 1.0f;
}

static s32 EnVali_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx,
                                   Gfx** gfx) {
    EnVali* this = (EnVali*)thisx;

    if ((limbIndex == VALI_LIMB_NUCLEUS) || (limbIndex == VALI_LIMB_OUTER_HOOD) ||
        (limbIndex == VALI_LIMB_INNER_HOOD)) {
        *dList = NULL;
        return false;
    }
    bool isArmBase = (limbIndex == VALI_LIMB_LEFT_ARM_BASE) || (limbIndex == VALI_LIMB_RIGHT_ARM_BASE);
    if (isArmBase && EnVali_UpdateArmLength(this, this->skelAnime.curFrame)) {
        Matrix_Scale(this->armScale, 1.0f, 1.0f, MTXMODE_APPLY);
    }
    return false;
}

static void EnVali_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx, Gfx** gfx) {
    static Vec3f sClawOffset = { 3000.0f, 0.0f, 0.0f };
    static Vec3f sElbowOffset = { -1000.0f, 0.0f, 0.0f };
    EnVali* this = (EnVali*)thisx;

    if (this->actionFunc != EnVali_FloatIdle) {
        return;
    }
    ColliderQuad* arm = NULL;
    if (limbIndex == VALI_LIMB_LEFT_FOREARM_BASE) {
        arm = &this->leftArmCollider;
    } else if (limbIndex == VALI_LIMB_RIGHT_FOREARM_BASE) {
        arm = &this->rightArmCollider;
    }
    if (arm == NULL) {
        return;
    }
    Vec3f claw;
    Vec3f elbow;
    Matrix_MultVec3f(&sClawOffset, &claw);
    Matrix_MultVec3f(&sElbowOffset, &elbow);
    Collider_SetQuadVertices(arm, &elbow, &claw, &arm->dim.quad[0], &arm->dim.quad[1]);
}

static void EnVali_DrawPart(PlayState* play, const char* dList) {
    OPEN_DISPS(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)dList);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnVali_DrawBody(EnVali* this, PlayState* play) {
    MtxF mtx;
    f32 curFrame = this->skelAnime.curFrame;
    Vec3f scale = { 1.0f, 1.0f, 1.0f };

    Matrix_Get(&mtx);
    EnVali_PulseHood(this, curFrame, &scale, 0.13f, 0.18f, 0.1f, -1.0f);
    Matrix_Scale(scale.x, scale.y, scale.z, MTXMODE_APPLY);
    EnVali_DrawPart(play, sBariInnerHoodDL);
    Matrix_Put(&mtx);
    Matrix_RotateYS(-this->actor.shape.rot.y, MTXMODE_APPLY);
    f32 cos = Math_CosS(this->actor.shape.rot.y);
    f32 sin = Math_SinS(this->actor.shape.rot.y);
    EnVali_DrawPart(play, sBariNucleusDL);
    Matrix_Translate((506.0f * cos) + (372.0f * sin), 1114.0f, (372.0f * cos) - (506.0f * sin), MTXMODE_APPLY);
    EnVali_DrawPart(play, sBariNucleusDL);
    Matrix_Translate((-964.0f * cos) - (804.0f * sin), -108.0f, (-804.0f * cos) + (964.0f * sin), MTXMODE_APPLY);
    EnVali_DrawPart(play, sBariNucleusDL);
    Matrix_Put(&mtx);
    scale.x = 1.0f;
    scale.y = 1.0f;
    scale.z = 1.0f;
    EnVali_PulseHood(this, curFrame, &scale, 0.2f, 0.24f, 0.13f, 1.0f);
    Matrix_Scale(scale.x, scale.y, scale.z, MTXMODE_APPLY);
    EnVali_DrawPart(play, sBariOuterHoodDL);
    Matrix_Put(&mtx);
}

static void EnVali_Draw(Actor* thisx, PlayState* play) {
    EnVali* this = (EnVali*)thisx;
    Gfx* combineDL = ((this->lightningTimer % 2) != 0) ? sChargedCombineDL : sNormalCombineDL;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    u32 scrollY = (127 - (play->gameplayFrames * 12)) % 128;
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)Gfx_TexScrollEx(play->state.gfxCtx, 0, scrollY, 32, 32, 0, -12));
    gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)combineDL);
    EnVali_DrawBody(this, play);
    POLY_XLU_DISP = SkelAnime_Draw(play, this->skelAnime.skeleton, this->skelAnime.jointTable, EnVali_OverrideLimbDraw,
                                   EnVali_PostLimbDraw, &this->actor, POLY_XLU_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
    Actor_DrawDamageEffects(play, &this->actor, &this->actor.world.pos, 1, 0.8f, 0.6f, this->drawDmgEffAlpha,
                            ACTOR_DRAW_DMGEFF_FROZEN_NO_SFX);
}

static void EnBili_FloatIdle(EnBili* this, PlayState* play);
static void EnBili_SetNewHomeHeight(EnBili* this, PlayState* play);
static void EnBili_ApproachPlayer(EnBili* this, PlayState* play);
static void EnBili_Recoil(EnBili* this, PlayState* play);
static void EnBili_DischargeLightning(EnBili* this, PlayState* play);
static void EnBili_Climb(EnBili* this, PlayState* play);
static void EnBili_Stunned(EnBili* this, PlayState* play);
static void EnBili_Die(EnBili* this, PlayState* play);

static bool EnBili_IsAttachedToArrow(EnBili* this) {
    return (this->actor.flags & ACTOR_FLAG_ATTACHED_TO_ARROW) != 0;
}

static void EnBili_UpdateFloating(EnBili* this) {
    f32 playerHeight = this->actor.world.pos.y + this->actor.playerHeightRel;
    f32 heightOffset = (this->actionFunc == EnBili_SetNewHomeHeight) ? 100.0f : 40.0f;
    f32 baseHeight = MAX(this->actor.floorHeight, playerHeight);

    Math_StepToF(&this->actor.home.pos.y, baseHeight + heightOffset, 1.0f);
    this->actor.world.pos.y = this->actor.home.pos.y + (sinf(this->timer * (M_PIf / 16)) * 3.0f);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->actor.world.rot.y = this->actor.wallYaw;
    }
}

static void EnBili_TickFloatTimer(EnBili* this) {
    if (this->timer != 0) {
        this->timer--;
    }
    EnBili_UpdateFloating(this);
    if (this->timer == 0) {
        this->timer = 32;
    }
}

static void EnBili_SetupDie(EnBili* this) {
    this->timer = 18;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actionFunc = EnBili_Die;
    this->actor.speed = 0.0f;
}

static void EnBili_Die(EnBili* this, PlayState* play) {
    if (this->actor.draw != NULL) {
        if (EnBili_IsAttachedToArrow(this)) {
            return;
        }
        this->actor.draw = NULL;
        Item_DropCollectibleRandom(play, &this->actor, &this->actor.world.pos, 0x50);
    }
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer == 0) {
        Actor_Kill(&this->actor);
        return;
    }
    SpawnDeathBubbles(play, &this->actor.world.pos, 10.0f, 5.0f);
    if (this->timer == 14) {
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, BIRI_SFX_BUBBLE);
    }
}

static void EnBili_FloatIdle(EnBili* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if ((this->timer % 4) == 0) {
        this->actor.world.rot.y += Rand_CenteredFloat(1820.0f);
    }
    EnBili_TickFloatTimer(this);
    if ((this->actor.xzDistToPlayer < 160.0f) && (fabsf(this->actor.playerHeightRel) < 45.0f)) {
        this->actor.speed = 1.2f;
        this->actionFunc = EnBili_ApproachPlayer;
    }
}

static void EnBili_SpawnedFlyApart(EnBili* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer == 0) {
        EnBili_SetupFloatIdle(this);
    }
}

static void EnBili_SetupFloatIdle(EnBili* this) {
    this->actor.speed = 0.7f;
    this->collider.elem.acDmgInfo.effect = 1;
    this->timer = 32;
    this->collider.base.atFlags |= AT_ON;
    this->collider.base.acFlags |= AC_ON;
    this->actionFunc = EnBili_FloatIdle;
    this->actor.home.pos.y = this->actor.world.pos.y;
    this->actor.gravity = 0.0f;
    this->actor.velocity.y = 0.0f;
}

static void EnBili_SetupDischargeLightning(EnBili* this) {
    Animation_PlayLoop(&this->skelAnime, ANIM(sBiriDischargeLightningAnim));
    this->timer = 10;
    this->actionFunc = EnBili_DischargeLightning;
    this->actor.speed = 0.0f;
    this->actor.velocity.y = -1.0f;
}

static void EnBili_DischargeLightning(EnBili* this, PlayState* play) {
    static Color_RGBA8 sPrimColor = { 255, 255, 255, 255 };
    static Color_RGBA8 sEnvColor = { 200, 255, 255, 255 };

    for (s32 i = 0; i < 4; i++) {
        if (((this->timer + (i << 1)) % 4) != 0) {
            continue;
        }
        s16 yaw = (s16)Rand_CenteredFloat(12288.0f) + (i * 0x4000) + 0x2000;
        Vec3f pos;
        pos.x = Rand_CenteredFloat(5.0f) + this->actor.world.pos.x;
        pos.y = (Rand_ZeroOne() * 5.0f) + this->actor.world.pos.y + 2.5f;
        pos.z = Rand_CenteredFloat(5.0f) + this->actor.world.pos.z;
        EffectSsLightning_Spawn(play, &pos, &sPrimColor, &sEnvColor, 15, yaw, 6, 2);
    }
    SkelAnime_Update(&this->skelAnime);
    Actor_PlaySfx_Flagged(&this->actor, NA_SE_EN_BIRI_SPARK - SFX_FLAG);
    if (this->timer != 0) {
        this->timer--;
    }
    this->actor.velocity.y *= -1.0f;
    if ((this->timer != 0) || !Animation_OnFrame(&this->skelAnime, 0.0f)) {
        return;
    }
    if (this->actor.params == BIRI_TYPE_DYING) {
        EnBili_SetupDie(this);
        return;
    }
    Animation_PlayOnce(&this->skelAnime, ANIM(sBiriClimbAnim));
    this->collider.base.atFlags &= ~AT_ON;
    this->actionFunc = EnBili_Climb;
    this->actor.speed = 0.0f;
    this->actor.velocity.y = 0.0f;
}

static void EnBili_Climb(EnBili* this, PlayState* play) {
    bool isDone = SkelAnime_Update(&this->skelAnime);

    if (Animation_OnFrame(&this->skelAnime, 9.0f)) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_BIRI_JUMP);
    }
    if (this->skelAnime.curFrame > 9.0f) {
        Math_ApproachF(&this->actor.world.pos.y, this->actor.world.pos.y + this->actor.playerHeightRel + 100.0f, 0.5f,
                       5.0f);
    }
    if (!isDone) {
        return;
    }
    Animation_PlayLoop(&this->skelAnime, ANIM(sBiriDefaultAnim));
    this->timer = 96;
    this->actor.speed = 0.9f;
    this->collider.base.atFlags |= AT_ON;
    this->actionFunc = EnBili_SetNewHomeHeight;
    this->actor.home.pos.y = this->actor.world.pos.y;
}

static void EnBili_ApproachPlayer(EnBili* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_ApproachS(&this->actor.world.rot.y, this->actor.yawTowardsPlayer, 2, 1820);
    EnBili_TickFloatTimer(this);
    if (this->actor.xzDistToPlayer > 200.0f) {
        EnBili_SetupFloatIdle(this);
    }
}

static void EnBili_SetNewHomeHeight(EnBili* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->timer != 0) {
        this->timer--;
    }
    Math_ScaledStepToS(&this->actor.world.rot.y, (s16)(this->actor.yawTowardsPlayer + 0x8000), 910);
    EnBili_UpdateFloating(this);
    if (this->timer == 0) {
        EnBili_SetupFloatIdle(this);
    }
}

static void EnBili_Recoil(EnBili* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Math_StepToF(&this->actor.speed, 0.0f, 0.3f)) {
        this->actor.world.rot.y += 0x8000;
        EnBili_SetupFloatIdle(this);
    }
}

static void EnBili_Burnt(EnBili* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (EnBili_IsAttachedToArrow(this)) {
        this->actor.colorFilterTimer = 20;
        return;
    }
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->timer == 0) {
        EnBili_SetupDie(this);
    }
}

static void EnBili_SetupBurnt(EnBili* this) {
    if (this->actionFunc == EnBili_Climb) {
        Animation_PlayLoop(&this->skelAnime, ANIM(sBiriDefaultAnim));
    }
    this->timer = 20;
    this->collider.base.atFlags &= ~AT_ON;
    this->collider.base.acFlags &= ~AC_ON;
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    this->actor.speed = 0.0f;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 200, COLORFILTER_BUFFLAG_XLU, 20);
    this->actionFunc = EnBili_Burnt;
}

static void EnBili_Stunned(EnBili* this, PlayState* play) {
    if (this->timer != 0) {
        this->timer--;
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH) {
        Actor_PlaySfx(&this->actor, JELLYFISH_SFX_LAND);
    }
    if (this->timer == 0) {
        EnBili_SetupFloatIdle(this);
    }
}

static void EnBili_Frozen(EnBili* this, PlayState* play) {
    if (this->timer != 0) {
        this->timer--;
    }
    if (!EnBili_IsAttachedToArrow(this)) {
        this->actor.gravity = -1.0f;
    }
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (this->actor.floorHeight != BGCHECK_Y_MIN)) {
        this->actor.colorFilterTimer = 10;
        return;
    }
    this->actor.colorFilterTimer = 0;
    this->drawDmgEffAlpha = 0.0f;
    Actor_SpawnIceEffects(play, &this->actor, &this->actor.world.pos, 1, 4, 0.4f, 0.3f);
    EnBili_SetupDie(this);
}

static void EnBili_SetupFrozen(EnBili* this) {
    if (!EnBili_IsAttachedToArrow(this)) {
        this->actor.gravity = -1.0f;
    }
    this->actor.velocity.y = 0.0f;
    this->actor.speed = 0.0f;
    this->drawDmgEffAlpha = 1.0f;
    Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 150, COLORFILTER_BUFFLAG_XLU, 10);
    this->collider.base.atFlags &= ~AT_ON;
    this->collider.base.acFlags &= ~AC_ON;
    this->timer = 300;
    this->actionFunc = EnBili_Frozen;
}

static void EnBili_ReactToHit(EnBili* this, u8 effect) {
    bool isStunned = this->actionFunc == EnBili_Stunned;

    if (effect == JELLYFISH_DMGEFF_STUN) {
        if (isStunned) {
            return;
        }
        this->timer = 80;
        this->collider.elem.acDmgInfo.effect = 0;
        this->actor.gravity = -1.0f;
        this->actor.speed = 0.0f;
        Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_BLUE, 150, COLORFILTER_BUFFLAG_XLU, 80);
        Actor_PlaySfx(&this->actor, NA_SE_EN_COMMON_FREEZE);
        this->collider.base.atFlags &= ~AT_ON;
        this->actionFunc = EnBili_Stunned;
    } else if ((effect == JELLYFISH_DMGEFF_SWORD) && !isStunned) {
        Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 200, COLORFILTER_BUFFLAG_XLU, 10);
        if (this->actor.colChkInfo.health == 0) {
            this->actor.params = BIRI_TYPE_DYING;
        }
        EnBili_SetupDischargeLightning(this);
    } else if (effect == JELLYFISH_DMGEFF_FIRE) {
        EnBili_SetupBurnt(this);
        this->timer = 2;
    } else if (effect == JELLYFISH_DMGEFF_ICE) {
        EnBili_SetupFrozen(this);
    } else {
        EnBili_SetupBurnt(this);
    }
}

static void EnBili_UpdateDamage(EnBili* this, PlayState* play) {
    if ((this->actor.colChkInfo.health == 0) || !(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    Actor_SetDropFlag(&this->actor, &this->collider.elem);
    u8 effect = this->actor.colChkInfo.damageEffect;
    if ((effect == JELLYFISH_DMGEFF_NONE) && (this->actor.colChkInfo.damage == 0)) {
        return;
    }
    if (Actor_ApplyDamage(&this->actor) == 0) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_BIRI_DEAD);
        Enemy_StartFinishingBlow(play, &this->actor);
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    }
    EnBili_ReactToHit(this, effect);
    ColliderElement* hitBy = this->collider.elem.acHitElem;
    if ((hitBy != NULL) && (hitBy->atDmgInfo.dmgFlags & ARROW_DMG_FLAGS)) {
        this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    }
}

static void EnBili_UpdateTentaclesIndex(EnBili* this) {
    s16 curFrame = this->skelAnime.curFrame;

    if (this->actionFunc == EnBili_DischargeLightning) {
        s16 offset = 3 - curFrame;
        this->tentaclesTexIndex = (ABS(offset) + 5) % 8;
    } else if (this->actionFunc != EnBili_Climb) {
        this->tentaclesTexIndex = curFrame >> 1;
    } else if (curFrame <= 9) {
        this->tentaclesTexIndex = MIN(curFrame >> 1, 3);
    } else if (curFrame <= 18) {
        this->tentaclesTexIndex = MAX(17 - curFrame, 0) >> 1;
    } else if (curFrame <= 36) {
        this->tentaclesTexIndex = ((36 - curFrame) / 3) + 2;
    } else {
        this->tentaclesTexIndex = (40 - curFrame) >> 1;
    }
}

static bool EnBili_IsDrifting(EnBili* this) {
    return (this->actionFunc == EnBili_FloatIdle) || (this->actionFunc == EnBili_SetNewHomeHeight) ||
           (this->actionFunc == EnBili_ApproachPlayer) || (this->actionFunc == EnBili_Recoil);
}

static void EnBili_PlayFlySfx(EnBili* this) {
    if (!Animation_OnFrame(&this->skelAnime, 9.0f) || !EnBili_IsDrifting(this)) {
        return;
    }
    if (this->playFlySound) {
        Actor_PlaySfx(&this->actor, NA_SE_EN_BIRI_FLY);
    }
    this->playFlySound = !this->playFlySound;
}

static void EnBili_Update(Actor* thisx, PlayState* play) {
    EnBili* this = (EnBili*)thisx;

    if (this->collider.base.atFlags & AT_HIT) {
        this->collider.base.atFlags &= ~AT_HIT;
        EnBili_SetupDischargeLightning(this);
    }
    EnBili_UpdateDamage(this, play);
    this->actionFunc(this, play);
    if (this->actionFunc == EnBili_Die) {
        return;
    }
    EnBili_UpdateTentaclesIndex(this);
    EnBili_PlayFlySfx(this);
    if (this->actionFunc == EnBili_Recoil) {
        Actor_MoveWithoutGravity(&this->actor);
    } else {
        Actor_MoveWithGravity(&this->actor);
    }
    Actor_UpdateBgCheckInfo(play, &this->actor, 5.0f, this->collider.dim.radius, this->collider.dim.height,
                            BIRI_BGCHECK_FLAGS);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    if (this->collider.base.atFlags & AT_ON) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if (this->collider.base.acFlags & AC_ON) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    Actor_SetFocus(&this->actor, 0.0f);
}

static void EnBili_Init(Actor* thisx, PlayState* play) {
    EnBili* this = (EnBili*)thisx;

    Actor_ProcessInitChain(&this->actor, sBiriInitChain);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 17.0f);
    this->actor.shape.shadowAlpha = 155;
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sBiriSkel, ANIM(sBiriDefaultAnim), this->jointTable,
                   this->morphTable, BILI_LIMB_MAX);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sBiriColliderInit);
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, &sBiriDamageTable, &sBiriColChkInfoInit);
    this->playFlySound = false;
    if (this->actor.params == BIRI_TYPE_NORMAL) {
        EnBili_SetupFloatIdle(this);
        return;
    }
    Animation_PlayLoop(&this->skelAnime, ANIM(sBiriDefaultAnim));
    this->timer = 25;
    this->actor.velocity.y = 6.0f;
    this->actor.gravity = -0.3f;
    this->collider.base.atFlags &= ~AT_ON;
    this->actionFunc = EnBili_SpawnedFlyApart;
    this->actor.speed = 3.0f;
}

static void EnBili_Destroy(Actor* thisx, PlayState* play) {
    EnBili* this = (EnBili*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void EnBili_PulseOuterHood(EnBili* this, f32 frame, Vec3f* scale) {
    if (this->actionFunc == EnBili_DischargeLightning) {
        scale->y = 1.0f - (sinf((M_PIf * 0.16667f) * frame) * 0.26f);
    } else if (this->actionFunc == EnBili_Climb) {
        if (frame <= 8.0f) {
            scale->y = (cosf((M_PIf * 0.125f) * frame) * 0.15f) + 0.85f;
        } else if (frame <= 18.0f) {
            f32 cos = cosf((frame - 8.0f) * (M_PIf * 0.1f));
            scale->y = 1.0f - (0.3f * cos);
            scale->x = (0.2f * cos) + 0.8f;
        } else {
            f32 cos = cosf((frame - 18.0f) * (M_PIf * 0.0227f));
            scale->y = (0.31f * cos) + 1.0f;
            scale->x = 1.0f - (0.4f * cos);
        }
        scale->z = scale->x;
    } else if (this->actionFunc == EnBili_Stunned) {
        f32 wobble = sinf((M_PIf * 0.1f) * this->timer) * 0.08f;
        scale->x -= wobble;
        scale->y += wobble;
        scale->z -= wobble;
    } else {
        scale->y = (cosf((M_PIf * 0.125f) * frame) * 0.13f) + 0.87f;
    }
}

static void EnBili_PulseInnerHood(EnBili* this, f32 frame, Vec3f* scale) {
    if (this->actionFunc == EnBili_DischargeLightning) {
        scale->y = (sinf((M_PIf * 0.16667f) * frame) * 0.2f) + 1.0f;
    } else if (this->actionFunc == EnBili_Climb) {
        if (frame <= 8.0f) {
            scale->x = 1.125f - (cosf((M_PIf * 0.125f) * frame) * 0.125f);
        } else if (frame <= 18.0f) {
            f32 cos = cosf((frame - 8.0f) * (M_PIf * 0.1f));
            scale->x = (0.275f * cos) + 0.975f;
            scale->y = 1.25f - (0.25f * cos);
        } else {
            f32 cos = cosf((frame - 18.0f) * (M_PIf * 0.0227f));
            scale->x = 1.0f - (0.3f * cos);
            scale->y = (0.48f * cos) + 1.0f;
        }
        scale->z = scale->x;
    } else if (this->actionFunc == EnBili_Stunned) {
        f32 wobble = sinf((M_PIf * 0.1f) * this->timer) * 0.08f;
        scale->x += wobble;
        scale->y -= wobble;
        scale->z += wobble;
    } else {
        scale->y = 1.1f - (cosf((M_PIf * 0.125f) * frame) * 0.1f);
    }
}

static void EnBili_PulseTentacles(EnBili* this, f32 frame, Vec3f* scale) {
    if (this->actionFunc != EnBili_Climb) {
        return;
    }
    if (frame <= 8.0f) {
        f32 cos = cosf((M_PIf * 0.125f) * frame);
        scale->x = 1.125f - (0.125f * cos);
        scale->y = (0.3f * cos) + 0.7f;
    } else if (frame <= 18.0f) {
        f32 cos = cosf((frame - 8.0f) * (M_PIf * 0.1f));
        scale->x = (0.325f * cos) + 0.925f;
        scale->y = 0.95f - (0.55f * cos);
    } else {
        f32 cos = cosf((frame - 18.0f) * (M_PIf * 0.0227f));
        scale->x = 1.0f - (0.4f * cos);
        scale->y = (0.52f * cos) + 1.0f;
    }
    scale->z = scale->x;
}

static s32 EnBili_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx,
                                   Gfx** gfx) {
    EnBili* this = (EnBili*)thisx;
    Vec3f limbScale = { 1.0f, 1.0f, 1.0f };
    f32 curFrame = this->skelAnime.curFrame;

    if (limbIndex == BILI_LIMB_OUTER_HOOD) {
        EnBili_PulseOuterHood(this, curFrame, &limbScale);
    } else if (limbIndex == BILI_LIMB_INNER_HOOD) {
        EnBili_PulseInnerHood(this, curFrame, &limbScale);
    } else if (limbIndex == BILI_LIMB_TENTACLES) {
        EnBili_PulseTentacles(this, curFrame, &limbScale);
        rot->y = (Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) - this->actor.shape.rot.y) + 0x8000;
    }
    Matrix_Scale(limbScale.x, limbScale.y, limbScale.z, MTXMODE_APPLY);
    return false;
}

static void EnBili_Draw(Actor* thisx, PlayState* play) {
    EnBili* this = (EnBili*)thisx;
    bool isCharged = (this->actionFunc == EnBili_DischargeLightning) && ((this->timer & 1) != 0);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    this->tentaclesTexIndex = MIN(this->tentaclesTexIndex, 7);
    gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)sBiriTentaclesTextures[this->tentaclesTexIndex]);
    gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)(isCharged ? sChargedCombineDL : sNormalCombineDL));
    POLY_XLU_DISP = SkelAnime_Draw(play, this->skelAnime.skeleton, this->skelAnime.jointTable, EnBili_OverrideLimbDraw,
                                   NULL, &this->actor, POLY_XLU_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
    Actor_DrawDamageEffects(play, &this->actor, &this->actor.world.pos, 1, 0.4f, 0.3f, this->drawDmgEffAlpha,
                            ACTOR_DRAW_DMGEFF_FROZEN_NO_SFX);
}

static Gfx sSoulEmptyDL[] = {
    gsSPEndDisplayList(),
};

// The soul shows its enemy in an idle loop, as Majora's Mask souls do.
static void DrawSoulModel(PlayState* play) {
    static SkelAnime sSoulSkelAnime;
    static Vec3s sSoulJointTable[VALI_LIMB_MAX];
    static Vec3s sSoulMorphTable[VALI_LIMB_MAX];
    static bool sIsSoulPosed = false;
    static u32 sSoulFrame = 0;

    if (!sIsSoulPosed) {
        SkelAnime_Init(play, &sSoulSkelAnime, (SkeletonHeader*)sBariSkel, ANIM(sBariLurkingAnim), sSoulJointTable,
                       sSoulMorphTable, VALI_LIMB_MAX);
        sIsSoulPosed = true;
    }
    if (sSoulFrame != play->state.frames) {
        sSoulFrame = play->state.frames;
        SkelAnime_Update(&sSoulSkelAnime);
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
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
    gDPSetPrimColor(POLY_XLU_DISP++, 0xC0, 0xC0, 120, 220, 255, 0);
    gDPSetEnvColor(POLY_XLU_DISP++, 120, 220, 255, 0);
    Matrix_Scale(0.13333333f, 0.13333333f, 0.13333333f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gameplay_keep_DL_01ACF0);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Ship (RE_BARI, both jellyfish): hookshot or boomerang, bow, explosives, hammer, sticks or a shocked sword.
static bool CanKillJellyfish(void* userData) {
    return sApi->LogicHasCapability("HookshotOrBoomerang") || sApi->LogicCanUseItem(RI_BOW) ||
           sApi->LogicHasCapability("HasExplosives") || sApi->LogicHasCapability("CanBeGoron") ||
           sApi->LogicCanUseItem(RI_DEKU_STICK) || sApi->LogicHasCapability("CanUseSword");
}

static void RegisterSoul(void) {
    static S2HCustomItemRandomizer sSoulLogic = { 0 };
    S2HCustomItemDefinition soul = { 0 };

    // Pool membership follows the host's soul shuffle and soul pool; the mod only declares the item.
    sSoulLogic.structSize = sizeof(sSoulLogic);
    sSoulLogic.flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
    sSoulLogic.type = S2H_CUSTOM_ITEM_TYPE_ITEM;
    sSoulLogic.logicKey = BARI_SOUL_KEY;
    sSoulLogic.hintKey = BARI_SOUL_KEY;
    soul.structSize = sizeof(soul);
    soul.key = BARI_SOUL_KEY;
    soul.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO;
    soul.getItemText = "You got the %rSoul of Bari%w!&Bari and Biri can be defeated now.";
    soul.getItemEntry.drawFunc = DrawSoul;
    soul.randomizer = &sSoulLogic;
    if (!sApi->RegisterCustomItem(&soul)) {
        sApi->Log("Could not register " BARI_SOUL_KEY);
    }
}

static S2HActorDefinition DefineJellyfish(const char* key, const char* description, u32 flags, u32 size) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = key;
    definition.description = description;
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = flags;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = size;
    // They hover the same in water, where Zora's punches and barrier still hurt them.
    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_UNDERWATER;
    // Both share the Bari family soul; the host finds the item as "soul." + soulKey.
    definition.soulKey = BARI_KEY;
    definition.logicKill = CanKillJellyfish;
    return definition;
}

static s16 RegisterBiri(void) {
    S2HActorDefinition definition = DefineJellyfish(BIRI_KEY, "Biri", BIRI_FLAGS, sizeof(EnBili));

    definition.init = EnBili_Init;
    definition.destroy = EnBili_Destroy;
    definition.update = EnBili_Update;
    definition.draw = EnBili_Draw;
    definition.enemyParams = BIRI_TYPE_NORMAL;
    definition.naviHint = "Biri&A small electric jellyfish. Don't touch it while&it sparks!";
    return sApi->RegisterActor(&definition);
}

static s16 RegisterBari(void) {
    S2HActorDefinition definition = DefineJellyfish(BARI_KEY, "Bari", BARI_FLAGS, sizeof(EnVali));

    definition.init = EnVali_Init;
    definition.destroy = EnVali_Destroy;
    definition.update = EnVali_Update;
    definition.draw = EnVali_Draw;
    // Vanilla Bari hang from the ceiling and drop in; the spawn is raised so it lurks above the floor.
    definition.enemySpawnHeight = BARI_LURK_HEIGHT;
    definition.naviHint = "Bari&A big electric jellyfish that splits when beaten.&Attack from afar when it sparks!";
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
    if (!RequireOotAsset(VALI_OBJECT "gBariSkel") || !RequireOotAsset(BL_OBJECT "gBiriSkel")) {
        sApi->Log(BARI_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    sBiriId = RegisterBiri();
    if (sBiriId < 0) {
        sApi->Log("Could not register " BIRI_KEY);
        return;
    }
    if (RegisterBari() < 0) {
        sApi->Log("Could not register " BARI_KEY);
    }
    RegisterSoul();
}
