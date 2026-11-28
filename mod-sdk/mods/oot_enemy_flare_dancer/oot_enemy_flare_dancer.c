/*
 * Ocarina of Time's Flare Dancer (En_Fd), its core (En_Fw) and its flames (En_Fd_Fire) on Majora's Mask,
 * drawn from the OoT Asset Loader's oot.o2r. Explosives, the hookshot or a Goron pound knock the core out;
 * defeating the core three times destroys it.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_Arms_Hook/z_arms_hook.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"

#define FLARE_DANCER_KEY "oot.flare_dancer"
#define FLARE_DANCER_CORE_KEY "oot.flare_dancer.core"
#define FLARE_DANCER_FIRE_KEY "oot.flare_dancer.fire"
#define FW_OBJECT "objects/oot_object_fw/"
#define FW_ASSET(name) "__OTR__" FW_OBJECT name

static const ALIGN_ASSET(2) char sSkel[] = FW_ASSET("gFlareDancerSkel");
static const ALIGN_ASSET(2) char sCoreSkel[] = FW_ASSET("gFlareDancerCoreSkel");
static const ALIGN_ASSET(2) char sCastingFireAnim[] = FW_ASSET("gFlareDancerCastingFireAnim");
static const ALIGN_ASSET(2) char sBackflipAnim[] = FW_ASSET("gFlareDancerBackflipAnim");
static const ALIGN_ASSET(2) char sGettingUpAnim[] = FW_ASSET("gFlareDancerGettingUpAnim");
static const ALIGN_ASSET(2) char sChasingAnim[] = FW_ASSET("gFlareDancerChasingAnim");
static const ALIGN_ASSET(2) char sTwirlAnim[] = FW_ASSET("gFlareDancerTwirlAnim");
static const ALIGN_ASSET(2) char sCoreInitRunAnim[] = FW_ASSET("gFlareDancerCoreInitRunCycleAnim");
static const ALIGN_ASSET(2) char sCoreRunAnim[] = FW_ASSET("gFlareDancerCoreRunCycleAnim");
static const ALIGN_ASSET(2) char sCoreEndRunAnim[] = FW_ASSET("gFlareDancerCoreEndRunCycleAnim");
static const ALIGN_ASSET(2) char sFlameSetupDL[] = FW_ASSET("gFlareDancerDL_7928");
static const ALIGN_ASSET(2) char sSquareParticleDL[] = FW_ASSET("gFlareDancerSquareParticleDL");
static const ALIGN_ASSET(2) char sDotSetupDL[] = FW_ASSET("gFlareDancerDL_79F8");
static const ALIGN_ASSET(2) char sTriangleParticleDL[] = FW_ASSET("gFlareDancerTriangleParticleDL");

// MM reuses the Flare Dancer's sfx ids for other enemies, so these are MM stand-ins.
#define FD_SFX_LAUGH NA_SE_EN_WIZ_LAUGH
#define FD_SFX_DAMAGE NA_SE_EN_WIZ_DAMAGE
#define FD_SFX_FIRE_ATTACK NA_SE_EN_BUBLEFALL_FIRE
#define FD_SFX_SPIN NA_SE_EN_WIZ_UNARI
#define FD_SFX_RUN NA_SE_EV_FLAME_OF_FIRE
#define FD_SFX_KICK NA_SE_EN_HIPLOOP_FOOT
#define FW_SFX_BOUNCE NA_SE_EN_STALKIDS_ONGND
#define FW_SFX_DAMAGE NA_SE_EN_STALKIDS_DAMAGE
#define FW_SFX_SURPRISED NA_SE_EN_STALKIDS_SURPRISED
#define FW_SFX_RUN NA_SE_EN_STALKIDS_WALK
#define FW_SFX_SLIDE NA_SE_PL_SLIP
#define FW_SFX_JUMP NA_SE_EN_STALKIDS_JUMP

static const char* sDustTextures[] = {
    gEffDust8Tex, gEffDust7Tex, gEffDust6Tex, gEffDust5Tex, gEffDust4Tex, gEffDust3Tex, gEffDust2Tex, gEffDust1Tex,
};

#define FD_LIMB_COUNT 27
#define FD_LIMB_HEAD 13
#define FD_LIMB_CORE 21
#define FW_LIMB_COUNT 11
#define FW_LIMB_BODY 2
#define FW_LIMB_HEAD 3

#define FD_HEALTH 24
#define FD_HEALTH_PER_CORE 8
#define FD_EFFECT_COUNT 200
#define FW_EFFECT_COUNT 20
#define FD_CORE_DONE 0x8000
#define FD_CORE_DEAD 0x4000
#define FD_FIRE_BLUE 0x8000
#define FD_FLAGS                                                                              \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED | \
     ACTOR_FLAG_HOOKSHOT_PULLS_ACTOR)
#define FD_FIRE_FLAGS (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED)

typedef enum {
    FD_EFFECT_NONE,
    FD_EFFECT_FLAME,
    FD_EFFECT_DOT,
} FlareDancerEffectType;

typedef enum {
    FD_ANIM_CASTING_FIRE,
    FD_ANIM_BACKFLIP,
    FD_ANIM_GETTING_UP,
    FD_ANIM_CHASING,
    FD_ANIM_TWIRL,
} FlareDancerAnimation;

typedef enum {
    FW_ANIM_INIT_RUN,
    FW_ANIM_RUN,
    FW_ANIM_END_RUN,
} FlareDancerCoreAnimation;

typedef struct {
    u8 type;
    u8 timer;
    u8 initialTimer;
    f32 scale;
    f32 scaleStep;
    Color_RGBA8 color;
    Vec3f pos;
    Vec3f velocity;
    Vec3f accel;
} FlareDancerEffect;

typedef struct {
    u8 type;
    u8 timer;
    f32 scale;
    f32 scaleStep;
    Vec3f pos;
    Vec3f accel;
} EffectSpawn;

typedef struct {
    Color_RGBA8 prim;
    Color_RGBA8 env;
    bool fadesWithTimer;
} FlameStyle;

typedef struct {
    f32 scaleStep;
    s32 count;
    f32 radius;
    f32 yAccel;
} DustBurst;

static const S2HModApi* sApi;
static s16 sFlareDancerId = -1;
static s16 sCoreId = -1;
static s16 sFireId = -1;
static s32 sBgmUsers = 0;

struct EnFd;
typedef void (*EnFdActionFunc)(struct EnFd*, PlayState*);

typedef struct EnFd {
    Actor actor;
    SkelAnime skelAnime;
    EnFdActionFunc actionFunc;
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[12];
    Vec3f corePos;
    f32 runRadius;
    f32 fadeAlpha;
    s16 initYawToHome;
    s16 curYawToHome;
    s16 runDir;
    s16 spinTimer;
    s16 circlesToComplete;
    s16 invincibilityTimer;
    s16 attackTimer;
    u8 isBgmUser;
    u8 hasStarted;
    Vec3s jointTable[FD_LIMB_COUNT];
    Vec3s morphTable[FD_LIMB_COUNT];
    FlareDancerEffect effects[FD_EFFECT_COUNT];
} EnFd;

struct EnFw;
typedef void (*EnFwActionFunc)(struct EnFw*, PlayState*);

typedef struct EnFw {
    Actor actor;
    SkelAnime skelAnime;
    EnFwActionFunc actionFunc;
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[1];
    Vec3f bombPos;
    f32 runRadius;
    s16 runDir;
    s16 bounceCount;
    s16 damageTimer;
    s16 explosionTimer;
    s16 slideTimer;
    s16 slideSfxTimer;
    s16 returnToParentTimer;
    u8 wasHookshotHit;
    u8 isTurningAround;
    Vec3s jointTable[FW_LIMB_COUNT];
    Vec3s morphTable[FW_LIMB_COUNT];
    FlareDancerEffect effects[FW_EFFECT_COUNT];
} EnFw;

struct EnFdFire;
typedef void (*EnFdFireActionFunc)(struct EnFdFire*, PlayState*);

typedef struct EnFdFire {
    Actor actor;
    EnFdFireActionFunc actionFunc;
    ColliderCylinder collider;
    f32 spawnRadius;
    f32 scale;
    s16 deathTimer;
    s16 scrollSpeed;
} EnFdFire;

static AnimationInfo sFdAnimations[] = {
    { (AnimationHeader*)sCastingFireAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 0.0f },
    { (AnimationHeader*)sBackflipAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, -10.0f },
    { (AnimationHeader*)sGettingUpAnim, 0.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, -10.0f },
    { (AnimationHeader*)sChasingAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP_INTERP, -10.0f },
    { (AnimationHeader*)sTwirlAnim, 0.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, -10.0f },
};

static AnimationInfo sFwAnimations[] = {
    { (AnimationHeader*)sCoreInitRunAnim, 0.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, 0.0f },
    { (AnimationHeader*)sCoreRunAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_ONCE_INTERP, -8.0f },
    { (AnimationHeader*)sCoreEndRunAnim, 1.0f, 0.0f, -1.0f, ANIMMODE_LOOP_INTERP, -8.0f },
};

#define FD_SPHERE_INFO(acFlags, acElemFlags)                                                                    \
    {                                                                                                           \
        ELEM_MATERIAL_UNK0, { 0xF7CFFFFF, 0x01, 0x04 }, { acFlags, 0x00, 0x00 }, ATELEM_ON | ATELEM_SFX_NORMAL, \
            acElemFlags, OCELEM_ON                                                                              \
    }
#define FD_SPHERE(limb, x, radius, scale, acFlags, acElemFlags) \
    {                                                           \
        FD_SPHERE_INFO(acFlags, acElemFlags), {                 \
            limb, { { x, 0, 0 }, radius }, scale                \
        }                                                       \
    }

#define FD_BODY_HIT_FLAGS (DMG_EXPLOSIVES | DMG_POWDER_KEG)

static ColliderJntSphElementInit sFdSpheresInit[12] = {
    FD_SPHERE(21, 1600, 5, 300, FD_BODY_HIT_FLAGS | DMG_HOOKSHOT, ACELEM_ON | ACELEM_HOOKABLE),
    FD_SPHERE(12, 1600, 5, 400, FD_BODY_HIT_FLAGS, ACELEM_ON),
    FD_SPHERE(14, 800, 4, 300, FD_BODY_HIT_FLAGS, ACELEM_ON),
    FD_SPHERE(15, 1600, 4, 300, FD_BODY_HIT_FLAGS, ACELEM_ON),
    FD_SPHERE(16, 2000, 4, 300, FD_BODY_HIT_FLAGS, ACELEM_ON),
    FD_SPHERE(17, 800, 4, 300, FD_BODY_HIT_FLAGS, ACELEM_ON),
    FD_SPHERE(18, 1600, 4, 300, FD_BODY_HIT_FLAGS, ACELEM_ON),
    FD_SPHERE(19, 2000, 4, 300, FD_BODY_HIT_FLAGS, ACELEM_ON),
    FD_SPHERE(4, 2200, 4, 400, FD_BODY_HIT_FLAGS, ACELEM_ON),
    FD_SPHERE(4, 5000, 4, 300, FD_BODY_HIT_FLAGS, ACELEM_ON),
    FD_SPHERE(8, 2200, 4, 400, FD_BODY_HIT_FLAGS, ACELEM_ON),
    FD_SPHERE(8, 5000, 4, 300, FD_BODY_HIT_FLAGS, ACELEM_ON),
};

static ColliderJntSphInit sFdColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_JNTSPH,
    },
    ARRAY_COUNT(sFdSpheresInit),
    sFdSpheresInit,
};

static ColliderJntSphElementInit sFwSpheresInit[1] = {
    {
        {
            ELEM_MATERIAL_UNK0,
            { 0x00000000, 0x00, 0x04 },
            { 0xF7CFFFFE, 0x00, 0x00 },
            ATELEM_NONE,
            ACELEM_ON | ACELEM_HOOKABLE,
            OCELEM_ON,
        },
        { FW_LIMB_BODY, { { 1200, 0, 0 }, 16 }, 100 },
    },
};

static ColliderJntSphInit sFwColliderInit = {
    {
        COL_MATERIAL_HIT6,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_JNTSPH,
    },
    ARRAY_COUNT(sFwSpheresInit),
    sFwSpheresInit,
};

static ColliderCylinderInit sFireColliderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x01, 0x08 },
        { DMG_EXPLOSIVES | DMG_SPIN_ATTACK | DMG_POWDER_KEG, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 12, 46, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit2 sFdColChkInit = { FD_HEALTH, 2, 25, 25, MASS_IMMOVABLE };
static CollisionCheckInfoInit2 sFwColChkInit = { FD_HEALTH_PER_CORE, 2, 25, 25, MASS_IMMOVABLE };
static CollisionCheckInfoInit2 sFireColChkInit = { 1, 0, 0, 0, MASS_IMMOVABLE };

// Vanilla's preset deals no damage; MM drops zero-damage hits, so the allowed weapons deal 1 that is never applied.
static DamageTable sFdDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, 0x0),
    /* Deku Stick     */ DMG_ENTRY(0, 0x0),
    /* Horse trample  */ DMG_ENTRY(0, 0x0),
    /* Explosives     */ DMG_ENTRY(1, 0x0),
    /* Zora boomerang */ DMG_ENTRY(0, 0x0),
    /* Normal arrow   */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, 0x0),
    /* Hookshot       */ DMG_ENTRY(1, 0x0),
    /* Goron punch    */ DMG_ENTRY(0, 0x0),
    /* Sword          */ DMG_ENTRY(0, 0x0),
    /* Goron pound    */ DMG_ENTRY(0, 0x0),
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
    /* Spin attack    */ DMG_ENTRY(0, 0x0),
    /* Sword beam     */ DMG_ENTRY(0, 0x0),
    /* Normal Roll    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, 0x0),
    /* Unblockable    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, 0x0),
    /* Powder Keg     */ DMG_ENTRY(1, 0x0),
};

static DamageTable sFwDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, 0x0),
    /* Deku Stick     */ DMG_ENTRY(1, 0x0),
    /* Horse trample  */ DMG_ENTRY(0, 0x0),
    /* Explosives     */ DMG_ENTRY(1, 0x0),
    /* Zora boomerang */ DMG_ENTRY(1, 0x0),
    /* Normal arrow   */ DMG_ENTRY(1, 0x0),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, 0x0),
    /* Hookshot       */ DMG_ENTRY(1, 0x0),
    /* Goron punch    */ DMG_ENTRY(1, 0x0),
    /* Sword          */ DMG_ENTRY(1, 0x0),
    /* Goron pound    */ DMG_ENTRY(1, 0x0),
    /* Fire arrow     */ DMG_ENTRY(2, 0x0),
    /* Ice arrow      */ DMG_ENTRY(2, 0x0),
    /* Light arrow    */ DMG_ENTRY(2, 0x0),
    /* Goron spikes   */ DMG_ENTRY(1, 0x0),
    /* Deku spin      */ DMG_ENTRY(1, 0x0),
    /* Deku bubble    */ DMG_ENTRY(1, 0x0),
    /* Deku launch    */ DMG_ENTRY(1, 0x0),
    /* UNK_DMG_0x12   */ DMG_ENTRY(0, 0x0),
    /* Zora barrier   */ DMG_ENTRY(1, 0x0),
    /* Normal shield  */ DMG_ENTRY(0, 0x0),
    /* Light ray      */ DMG_ENTRY(0, 0x0),
    /* Thrown object  */ DMG_ENTRY(1, 0x0),
    /* Zora punch     */ DMG_ENTRY(1, 0x0),
    /* Spin attack    */ DMG_ENTRY(1, 0x0),
    /* Sword beam     */ DMG_ENTRY(1, 0x0),
    /* Normal Roll    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, 0x0),
    /* Unblockable    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, 0x0),
    /* Powder Keg     */ DMG_ENTRY(2, 0x0),
};

static DamageTable sFireDamageTable = {
    /* Deku Nut       */ DMG_ENTRY(0, 0x0),
    /* Deku Stick     */ DMG_ENTRY(0, 0x0),
    /* Horse trample  */ DMG_ENTRY(0, 0x0),
    /* Explosives     */ DMG_ENTRY(1, 0x0),
    /* Zora boomerang */ DMG_ENTRY(0, 0x0),
    /* Normal arrow   */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x06   */ DMG_ENTRY(0, 0x0),
    /* Hookshot       */ DMG_ENTRY(0, 0x0),
    /* Goron punch    */ DMG_ENTRY(0, 0x0),
    /* Sword          */ DMG_ENTRY(0, 0x0),
    /* Goron pound    */ DMG_ENTRY(0, 0x0),
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
    /* Spin attack    */ DMG_ENTRY(1, 0x0),
    /* Sword beam     */ DMG_ENTRY(0, 0x0),
    /* Normal Roll    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1B   */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1C   */ DMG_ENTRY(0, 0x0),
    /* Unblockable    */ DMG_ENTRY(0, 0x0),
    /* UNK_DMG_0x1E   */ DMG_ENTRY(0, 0x0),
    /* Powder Keg     */ DMG_ENTRY(1, 0x0),
};

static Gfx sXluRenderModeDL[] = {
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU |
                                            FORCE_BL | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)),
    gsDPSetAlphaCompare(G_AC_THRESHOLD),
    gsSPEndDisplayList(),
};

static void EnFd_Reappear(struct EnFd* this, PlayState* play);
static void EnFd_SpinAndGrow(struct EnFd* this, PlayState* play);
static void EnFd_JumpToGround(struct EnFd* this, PlayState* play);
static void EnFd_SpinAndSpawnFire(struct EnFd* this, PlayState* play);
static void EnFd_Run(struct EnFd* this, PlayState* play);
static void EnFd_WaitForCore(struct EnFd* this, PlayState* play);

static s16 AbsYawDiff(s16 a, s16 b) {
    s16 diff = a - b;

    return ABS(diff);
}

static s16 YawOfDelta(Vec3f* delta) {
    return Math_FAtan2F(delta->x, delta->z) * (0x8000 / M_PIf);
}

static void AddEffect(FlareDancerEffect* effects, s32 count, const EffectSpawn* spawn) {
    for (s32 i = 0; i < count; i++) {
        FlareDancerEffect* effect = &effects[i];

        if (effect->type != FD_EFFECT_NONE) {
            continue;
        }
        effect->scale = spawn->scale;
        effect->scaleStep = spawn->scaleStep;
        effect->initialTimer = spawn->timer;
        effect->timer = spawn->timer;
        effect->type = spawn->type;
        effect->pos = spawn->pos;
        effect->accel = spawn->accel;
        effect->velocity = (Vec3f){ 0.0f, 0.0f, 0.0f };
        if (spawn->type == FD_EFFECT_DOT) {
            effect->color.a = 255;
            effect->timer = (s16)(Rand_ZeroOne() * 10.0f);
        }
        return;
    }
}

static void MoveEffect(FlareDancerEffect* effect) {
    effect->pos.x += effect->velocity.x;
    effect->pos.y += effect->velocity.y;
    effect->pos.z += effect->velocity.z;
    effect->velocity.x += effect->accel.x;
    effect->velocity.y += effect->accel.y;
    effect->velocity.z += effect->accel.z;
}

static void UpdateFlames(FlareDancerEffect* effects, s32 count) {
    for (s32 i = 0; i < count; i++) {
        FlareDancerEffect* effect = &effects[i];

        if (effect->type != FD_EFFECT_FLAME) {
            continue;
        }
        effect->timer--;
        if (effect->timer == 0) {
            effect->type = FD_EFFECT_NONE;
        }
        effect->accel.x = (Rand_ZeroOne() * 0.4f) - 0.2f;
        effect->accel.z = (Rand_ZeroOne() * 0.4f) - 0.2f;
        MoveEffect(effect);
        effect->scale += effect->scaleStep;
    }
}

static void UpdateDots(FlareDancerEffect* effects, s32 count) {
    static Color_RGB8 sDotColors[] = { { 255, 128, 0 }, { 255, 0, 0 }, { 255, 255, 0 }, { 255, 0, 0 } };

    for (s32 i = 0; i < count; i++) {
        FlareDancerEffect* effect = &effects[i];

        if (effect->type != FD_EFFECT_DOT) {
            continue;
        }
        MoveEffect(effect);
        effect->timer++;
        effect->color.r = sDotColors[effect->timer % 4].r;
        effect->color.g = sDotColors[effect->timer % 4].g;
        effect->color.b = sDotColors[effect->timer % 4].b;
        if (effect->color.a > 30) {
            effect->color.a -= 30;
        } else {
            effect->color.a = 0;
            effect->type = FD_EFFECT_NONE;
        }
    }
}

static void LoadBillboardMatrix(PlayState* play, Gfx** gfx, FlareDancerEffect* effect) {
    Matrix_Translate(effect->pos.x, effect->pos.y, effect->pos.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(effect->scale, effect->scale, 1.0f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD((*gfx)++, play->state.gfxCtx);
}

static void DrawFlames(PlayState* play, FlareDancerEffect* effects, s32 count, const FlameStyle* style) {
    bool isSetUp = false;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    for (s32 i = 0; i < count; i++) {
        FlareDancerEffect* effect = &effects[i];

        if (effect->type != FD_EFFECT_FLAME) {
            continue;
        }
        if (!isSetUp) {
            POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, SETUPDL_0);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sFlameSetupDL);
            gDPSetEnvColor(POLY_XLU_DISP++, style->env.r, style->env.g, style->env.b, style->env.a);
            isSetUp = true;
        }
        u8 alpha = style->fadesWithTimer ? (u8)(effect->timer * (255.0f / effect->initialTimer)) : style->prim.a;
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, style->prim.r, style->prim.g, style->prim.b, alpha);
        gDPPipeSync(POLY_XLU_DISP++);
        LoadBillboardMatrix(play, &POLY_XLU_DISP, effect);
        s16 index = effect->timer * (8.0f / effect->initialTimer);
        gSPSegment(POLY_XLU_DISP++, 0x08, (uintptr_t)sDustTextures[CLAMP_MAX(index, 7)]);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sSquareParticleDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawDots(PlayState* play, FlareDancerEffect* effects, s32 count, f32 fadeAlpha) {
    bool isSetUp = false;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    for (s32 i = 0; i < count; i++) {
        FlareDancerEffect* effect = &effects[i];

        if (effect->type != FD_EFFECT_DOT) {
            continue;
        }
        if (!isSetUp) {
            Gfx_SetupDL25_Xlu(play->state.gfxCtx);
            gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sDotSetupDL);
            isSetUp = true;
        }
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, effect->color.r, effect->color.g, effect->color.b,
                        (u8)(effect->color.a * (fadeAlpha / 255.0f)));
        gDPPipeSync(POLY_XLU_DISP++);
        LoadBillboardMatrix(play, &POLY_XLU_DISP, effect);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sTriangleParticleDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void EnFd_StartBgm(EnFd* this) {
    if (this->isBgmUser) {
        return;
    }
    if (sBgmUsers == 0) {
        Audio_PlayBgm_StorePrevBgm(NA_BGM_MINI_BOSS);
    }
    sBgmUsers++;
    this->isBgmUser = true;
}

static void EnFd_StopBgm(EnFd* this) {
    if (!this->isBgmUser) {
        return;
    }
    this->isBgmUser = false;
    sBgmUsers--;
    if (sBgmUsers == 0) {
        Audio_RestorePrevBgm();
    }
}

static void SwapHookshotAttachment(PlayState* play, Actor* from, Actor* to) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first; actor != NULL; actor = actor->next) {
        if (actor->id != ACTOR_ARMS_HOOK) {
            continue;
        }
        ArmsHook* hookshot = (ArmsHook*)actor;
        hookshot->attachedActor = to;
        hookshot->attachPointOffset = (Vec3f){ 0.0f, 0.0f, 0.0f };
        to->flags |= ACTOR_FLAG_HOOKSHOT_ATTACHED;
        from->flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
        return;
    }
}

static bool EnFd_SpawnCore(EnFd* this, PlayState* play) {
    if (this->invincibilityTimer != 0) {
        return false;
    }
    Actor* core = Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, sCoreId, this->corePos.x, this->corePos.y,
                                     this->corePos.z, 0, this->actor.shape.rot.y, 0, this->runDir);
    if (core == NULL) {
        return false;
    }
    core->colChkInfo.health = this->actor.colChkInfo.health % FD_HEALTH_PER_CORE;
    if (core->colChkInfo.health == 0) {
        core->colChkInfo.health = FD_HEALTH_PER_CORE;
    }
    if (CHECK_FLAG_ALL(this->actor.flags, ACTOR_FLAG_HOOKSHOT_ATTACHED)) {
        SwapHookshotAttachment(play, &this->actor, core);
    }
    return true;
}

static void EnFd_ReleaseCore(EnFd* this, PlayState* play) {
    this->invincibilityTimer = 30;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Actor_PlaySfx(&this->actor, FD_SFX_DAMAGE);
    Enemy_StartFinishingBlow(play, &this->actor);
}

static void EnFd_SpawnChildFire(EnFd* this, PlayState* play, s16 fireCount, bool isBlue) {
    for (s32 i = 0; i < fireCount; i++) {
        s16 angle = (s16)(((i * 360.0f) / fireCount) * (0x10000 / 360.0f)) + this->actor.yawTowardsPlayer;

        Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, sFireId, this->actor.world.pos.x,
                           this->actor.world.pos.y, this->actor.world.pos.z, 0, angle, 0,
                           (isBlue ? FD_FIRE_BLUE : 0) | i);
    }
}

static void EnFd_SpawnDot(EnFd* this) {
    if ((this->actionFunc != EnFd_Run) && (this->actionFunc != EnFd_SpinAndSpawnFire)) {
        return;
    }
    EffectSpawn spawn = {
        FD_EFFECT_FLAME,
        8,
        0.6f,
        0.2f,
        { this->actor.world.pos.x, this->actor.floorHeight + 4.0f, this->actor.world.pos.z },
        { (Rand_ZeroOne() - 0.5f) * 2.0f, ((Rand_ZeroOne() - 0.5f) * 0.2f) + 0.3f, (Rand_ZeroOne() - 0.5f) * 2.0f },
    };
    AddEffect(this->effects, FD_EFFECT_COUNT, &spawn);
}

static bool EnFd_IsShakenByPound(EnFd* this, PlayState* play) {
    if ((this->actionFunc == EnFd_Reappear) || (this->actionFunc == EnFd_SpinAndGrow) ||
        (this->actionFunc == EnFd_JumpToGround) || (this->actionFunc == EnFd_WaitForCore)) {
        return false;
    }
    return (play->actorCtx.unk2 != 0) && (this->actor.xzDistToPlayer < 300.0f) && (this->actor.playerHeightRel < 60.0f);
}

static void EnFd_CheckCollider(EnFd* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((this->collider.base.acFlags & AC_HIT) || EnFd_IsShakenByPound(this, play)) {
        this->collider.base.acFlags &= ~AC_HIT;
        if (this->invincibilityTimer != 0) {
            return;
        }
        ColliderElement* hookable = &this->collider.elements[0].base;
        if ((hookable->acHitElem != NULL) && (hookable->acHitElem->atDmgInfo.dmgFlags & DMG_HOOKSHOT)) {
            return;
        }
        if (EnFd_SpawnCore(this, play)) {
            EnFd_ReleaseCore(this, play);
        }
        return;
    }
    if ((DECR(this->attackTimer) != 0) || !(this->collider.base.atFlags & AT_HIT)) {
        return;
    }
    this->collider.base.atFlags &= ~AT_HIT;
    if ((this->invincibilityTimer != 0) || (this->collider.base.atFlags & AT_BOUNCED)) {
        return;
    }
    this->attackTimer = 30;
    Actor_PlaySfx(&player->actor, NA_SE_PL_BODY_HIT);
    func_800B8E1C(play, &this->actor, this->actor.speed + 2.0f, this->actor.yawTowardsPlayer, 6.0f);
}

static bool EnFd_CanSeeActor(EnFd* this, Actor* actor, PlayState* play) {
    CollisionPoly* poly;
    s32 bgId;
    Vec3f hitPos;

    if (Math_Vec3f_DistXYZ(&this->actor.world.pos, &actor->world.pos) > 400.0f) {
        return false;
    }
    s16 angle = (f32)Math_Vec3f_Yaw(&this->actor.world.pos, &actor->world.pos) - this->actor.shape.rot.y;
    if (ABS(angle) > 0x1C70) {
        return false;
    }
    return !BgCheck_EntityLineTest1(&play->colCtx, &this->actor.world.pos, &actor->world.pos, &hitPos, &poly, true,
                                    false, false, true, &bgId);
}

static Actor* EnFd_FindBomb(EnFd* this, PlayState* play) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_EXPLOSIVES].first; actor != NULL; actor = actor->next) {
        if ((actor->params == 0) && (actor->parent == NULL) && (actor->id == ACTOR_EN_BOM) &&
            EnFd_CanSeeActor(this, actor, play)) {
            return actor;
        }
    }
    return NULL;
}

static Actor* EnFd_FindThreat(EnFd* this, PlayState* play) {
    Actor* bomb = EnFd_FindBomb(this, play);

    if (bomb != NULL) {
        return bomb;
    }
    if (this->attackTimer != 0) {
        return NULL;
    }
    Player* player = GET_PLAYER(play);
    return EnFd_CanSeeActor(this, &player->actor, play) ? &player->actor : NULL;
}

static Vec3f GetStepAroundCircle(Actor* actor, Vec3f* center, f32 radius, s16 dir) {
    s16 angle = Math_Vec3f_Yaw(center, &actor->world.pos) + (dir * 0x1554);
    Vec3f step = {
        (Math_SinS(angle) * radius) + center->x - actor->world.pos.x,
        0.0f,
        (Math_CosS(angle) * radius) + center->z - actor->world.pos.z,
    };

    return step;
}

static bool EnFd_ShouldStopRunning(EnFd* this, PlayState* play) {
    CollisionPoly* poly;
    s32 bgId;
    Vec3f hitPos;
    Vec3f step = GetStepAroundCircle(&this->actor, &this->actor.home.pos, this->runRadius, this->runDir);
    Vec3f next = { this->actor.world.pos.x + step.x, this->actor.world.pos.y, this->actor.world.pos.z + step.z };
    if (BgCheck_EntityLineTest1(&play->colCtx, &this->actor.world.pos, &next, &hitPos, &poly, true, false, false, true,
                                &bgId)) {
        this->runDir = -this->runDir;
        return true;
    }
    if ((this->circlesToComplete != 0) || (DECR(this->spinTimer) != 0)) {
        return false;
    }
    if (Rand_ZeroOne() > 0.5f) {
        this->runDir = -this->runDir;
    }
    return true;
}

static void EnFd_Fade(EnFd* this) {
    if (this->invincibilityTimer == 0) {
        return;
    }
    Math_SmoothStepToF(&this->fadeAlpha, 0.0f, 0.3f, 10.0f, 0.0f);
    this->actor.shape.shadowAlpha = this->fadeAlpha;
    if (this->fadeAlpha >= 0.9f) {
        return;
    }
    this->invincibilityTimer = 0;
    this->spinTimer = 0;
    this->actionFunc = EnFd_WaitForCore;
    this->actor.speed = 0.0f;
}

static void EnFd_Reappear(EnFd* this, PlayState* play) {
    this->actor.world.pos = this->actor.home.pos;
    this->actor.params = 0;
    this->actor.shape.shadowAlpha = 255;
    this->actor.scale.y = 0.0f;
    this->fadeAlpha = 255.0f;
    Actor_ChangeAnimationByInfo(&this->skelAnime, sFdAnimations, FD_ANIM_CASTING_FIRE);
    Actor_PlaySfx(&this->actor, FD_SFX_LAUGH);
    this->actionFunc = EnFd_SpinAndGrow;
}

static void EnFd_SpinAndGrow(EnFd* this, PlayState* play) {
    if (!Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
        this->actor.scale.y = this->skelAnime.curFrame * (0.01f / this->skelAnime.animLength);
        this->actor.shape.rot.y += 0x2000;
        this->actor.world.rot.y = this->actor.shape.rot.y;
        return;
    }
    this->actor.velocity.y = 6.0f;
    this->actor.scale.y = 0.01f;
    this->actor.world.rot.y ^= 0x8000;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.speed = 8.0f;
    Actor_ChangeAnimationByInfo(&this->skelAnime, sFdAnimations, FD_ANIM_BACKFLIP);
    this->actionFunc = EnFd_JumpToGround;
}

static void EnFd_Land(EnFd* this, PlayState* play) {
    Math_SmoothStepToF(&this->skelAnime.playSpeed, 1.0f, 0.1f, 1.0f, 0.0f);
    if (!Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
        return;
    }
    this->spinTimer = Rand_S16Offset(60, 90);
    this->runRadius = Math_Vec3f_DistXYZ(&this->actor.world.pos, &this->actor.home.pos);
    Vec3f step = GetStepAroundCircle(&this->actor, &this->actor.home.pos, this->runRadius, this->runDir);
    this->actor.world.rot.y = YawOfDelta(&step);
    Actor_ChangeAnimationByInfo(&this->skelAnime, sFdAnimations, FD_ANIM_TWIRL);
    this->actionFunc = EnFd_SpinAndSpawnFire;
}

static void EnFd_JumpToGround(EnFd* this, PlayState* play) {
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (this->actor.velocity.y > 0.0f)) {
        return;
    }
    this->actor.velocity.y = 0.0f;
    this->actor.speed = 0.0f;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    Actor_ChangeAnimationByInfo(&this->skelAnime, sFdAnimations, FD_ANIM_GETTING_UP);
    this->actionFunc = EnFd_Land;
}

static void EnFd_StopSpinning(EnFd* this, PlayState* play) {
    f32 remaining = (s16)(this->actor.world.rot.y - this->actor.shape.rot.y);
    f32 distance = fabsf(remaining);
    f32 rotSpeed = 0.0f;

    if (distance > 0.0f) {
        Math_ApproachF(&rotSpeed, distance, 0.6f, 0x2000);
        rotSpeed *= remaining / distance;
        this->actor.shape.rot.y += (s16)rotSpeed;
    }
    if ((s32)fabsf(rotSpeed) <= 300) {
        this->actor.shape.rot.y = this->actor.world.rot.y;
    }
    if (this->actor.shape.rot.y != this->actor.world.rot.y) {
        return;
    }
    this->initYawToHome = Math_Vec3f_Yaw(&this->actor.home.pos, &this->actor.world.pos);
    this->curYawToHome = (this->runDir < 0) ? 0xFFFF : 0;
    this->circlesToComplete = (play->state.frames & 7) + 2;
    this->spinTimer = Rand_S16Offset(30, 120);
    Actor_ChangeAnimationByInfo(&this->skelAnime, sFdAnimations, FD_ANIM_CHASING);
    this->actionFunc = EnFd_Run;
}

static void EnFd_SpinAndSpawnFire(EnFd* this, PlayState* play) {
    if ((this->spinTimer < 31) && (this->invincibilityTimer == 0)) {
        Actor_PlaySfx_Flagged(&this->actor, FD_SFX_FIRE_ATTACK - SFX_FLAG);
    } else {
        Actor_PlaySfx_Flagged(&this->actor, FD_SFX_SPIN - SFX_FLAG);
    }
    if (DECR(this->spinTimer) == 0) {
        EnFd_StopSpinning(this, play);
        return;
    }
    this->actor.shape.rot.y += this->runDir * 0x2000;
    if ((this->spinTimer == 30) && (this->invincibilityTimer == 0)) {
        EnFd_SpawnChildFire(this, play, 8, this->actor.xzDistToPlayer <= 160.0f);
    }
}

static void EnFd_CountCircles(EnFd* this) {
    s16 yawFromHome = Math_Vec3f_Yaw(&this->actor.home.pos, &this->actor.world.pos) - this->initYawToHome;
    bool hasWrapped = (this->runDir > 0) ? ((u16)this->curYawToHome > (u16)yawFromHome)
                                         : ((u16)this->curYawToHome < (u16)yawFromHome);

    if (hasWrapped) {
        this->circlesToComplete--;
    }
    this->circlesToComplete = MAX(this->circlesToComplete, 0);
    this->curYawToHome = yawFromHome;
}

static void EnFd_Run(EnFd* this, PlayState* play) {
    if (EnFd_ShouldStopRunning(this, play) && (this->invincibilityTimer == 0)) {
        this->actor.world.rot.y ^= 0x8000;
        this->actor.velocity.y = 6.0f;
        this->actor.speed = 0.0f;
        Actor_ChangeAnimationByInfo(&this->skelAnime, sFdAnimations, FD_ANIM_BACKFLIP);
        this->actionFunc = EnFd_JumpToGround;
        return;
    }
    EnFd_CountCircles(this);
    Actor* threat = EnFd_FindThreat(this, play);
    f32 targetRadius = ((threat != NULL) && (this->invincibilityTimer == 0))
                           ? Math_Vec3f_DistXYZ(&this->actor.home.pos, &threat->world.pos)
                           : 200.0f;
    Math_SmoothStepToF(&this->runRadius, targetRadius, 0.3f, 100.0f, 0.0f);
    Vec3f step = GetStepAroundCircle(&this->actor, &this->actor.home.pos, this->runRadius, this->runDir);
    Math_SmoothStepToS(&this->actor.shape.rot.y, YawOfDelta(&step), 4, 0xFA0, 1);
    this->actor.world.rot = this->actor.shape.rot;
    Actor_PlaySfx_Flagged(&this->actor, FD_SFX_RUN - SFX_FLAG);
    f32 frame = this->skelAnime.curFrame;
    if ((frame == 6.0f) || (frame == 13.0f) || (frame == 28.0f)) {
        Actor_PlaySfx(&this->actor, FD_SFX_KICK);
    }
    Math_SmoothStepToF(&this->actor.speed, 8.0f, 0.1f, 1.0f, 0.0f);
}

static void EnFd_WaitForCore(EnFd* this, PlayState* play) {
    if (this->spinTimer != 0) {
        this->spinTimer--;
        if (this->spinTimer == 0) {
            EnFd_StopBgm(this);
            Actor_Kill(&this->actor);
        }
    } else if (this->actor.params & FD_CORE_DONE) {
        this->actionFunc = EnFd_Reappear;
    } else if (this->actor.params & FD_CORE_DEAD) {
        this->actor.params = 0;
        this->spinTimer = 30;
    }
}

static void EnFd_UpdateCollision(EnFd* this, PlayState* play) {
    if ((this->actionFunc == EnFd_Reappear) || (this->actionFunc == EnFd_SpinAndGrow) ||
        (this->actionFunc == EnFd_WaitForCore)) {
        return;
    }
    if ((this->attackTimer == 0) && (this->invincibilityTimer == 0)) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if ((this->actionFunc == EnFd_Run) || (this->actionFunc == EnFd_SpinAndSpawnFire)) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void EnFd_Update(Actor* thisx, PlayState* play) {
    EnFd* this = (EnFd*)thisx;

    if (!this->hasStarted) {
        this->hasStarted = true;
        EnFd_StartBgm(this);
    }
    if (this->actionFunc != EnFd_Reappear) {
        SkelAnime_Update(&this->skelAnime);
        EnFd_SpawnDot(this);
    }
    if (CHECK_FLAG_ALL(this->actor.flags, ACTOR_FLAG_HOOKSHOT_ATTACHED)) {
        if (EnFd_SpawnCore(this, play)) {
            EnFd_ReleaseCore(this, play);
        } else {
            this->actor.flags &= ~ACTOR_FLAG_HOOKSHOT_ATTACHED;
        }
    } else if (this->actionFunc != EnFd_WaitForCore) {
        EnFd_CheckCollider(this, play);
    }
    Actor_MoveWithGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 0.0f, 0.0f, UPDBGCHECKINFO_FLAG_4);
    EnFd_Fade(this);
    this->actionFunc(this, play);
    UpdateDots(this->effects, FD_EFFECT_COUNT);
    UpdateFlames(this->effects, FD_EFFECT_COUNT);
    EnFd_UpdateCollision(this, play);
}

static void EnFd_Init(Actor* thisx, PlayState* play) {
    EnFd* this = (EnFd*)thisx;

    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, NULL, this->jointTable, this->morphTable,
                       FD_LIMB_COUNT);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 32.0f);
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sFdColliderInit, this->colliderElements);
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, &sFdDamageTable, &sFdColChkInit);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Actor_SetScale(&this->actor, 0.01f);
    this->actor.gravity = -1.0f;
    this->runDir = (Rand_ZeroOne() < 0.5f) ? -1 : 1;
    this->actionFunc = EnFd_Reappear;
}

static void EnFd_Destroy(Actor* thisx, PlayState* play) {
    EnFd* this = (EnFd*)thisx;

    EnFd_StopBgm(this);
    Collider_DestroyJntSph(play, &this->collider);
}

static s32 EnFd_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, Actor* thisx,
                                 Gfx** gfx) {
    EnFd* this = (EnFd*)thisx;

    if ((this->invincibilityTimer != 0) && ((limbIndex == FD_LIMB_HEAD) || (limbIndex == FD_LIMB_CORE))) {
        *dList = NULL;
    }
    return false;
}

static bool IsEmberLimb(s32 limbIndex) {
    switch (limbIndex) {
        case 3:
        case 6:
        case 7:
        case 10:
        case 14:
        case 15:
        case 17:
        case 18:
        case 20:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
            return true;
        default:
            return false;
    }
}

static void EnFd_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx, Gfx** gfx) {
    static Vec3f sOrigin = { 0.0f, 0.0f, 0.0f };
    EnFd* this = (EnFd*)thisx;

    if (limbIndex == FD_LIMB_CORE) {
        Matrix_MultVec3f(&sOrigin, &this->corePos);
    }
    if (limbIndex == FD_LIMB_HEAD) {
        Matrix_MultVec3f(&sOrigin, &this->actor.focus.pos);
    }
    if (IsEmberLimb(limbIndex) && ((play->state.frames % 2) != 0)) {
        EffectSpawn spawn = { FD_EFFECT_DOT, 0, 0.006f, 0.0f };

        Matrix_MultVec3f(&sOrigin, &spawn.pos);
        spawn.pos.x += (Rand_ZeroOne() - 0.5f) * 20.0f;
        spawn.pos.y += (Rand_ZeroOne() - 0.5f) * 40.0f;
        spawn.pos.z += (Rand_ZeroOne() - 0.5f) * 20.0f;
        spawn.accel = (Vec3f){ (Rand_ZeroOne() - 0.5f) * 0.4f, ((Rand_ZeroOne() - 0.5f) * 0.2f) + 0.6f,
                               (Rand_ZeroOne() - 0.5f) * 0.4f };
        AddEffect(this->effects, FD_EFFECT_COUNT, &spawn);
    }
    Collider_UpdateSpheres(limbIndex, &this->collider);
}

static void EnFd_Draw(Actor* thisx, PlayState* play) {
    static Color_RGB8 sPrimColors[] = { { 255, 255, 200 }, { 200, 200, 200 }, { 255, 255, 0 } };
    static Color_RGB8 sEnvColors[] = { { 0, 255, 0 }, { 0, 0, 255 }, { 255, 0, 0 } };
    EnFd* this = (EnFd*)thisx;
    u32 frames = play->state.frames;
    u8 alpha = (u8)((this->fadeAlpha / 255.0f) * 255);
    FlameStyle flames = { { 255, 255, 0, alpha }, { 255, 10, 0, alpha }, false };

    Matrix_Push();
    DrawDots(play, this->effects, FD_EFFECT_COUNT, this->fadeAlpha);
    DrawFlames(play, this->effects, FD_EFFECT_COUNT, &flames);
    Matrix_Pop();
    if ((this->actionFunc == EnFd_Reappear) || (this->fadeAlpha < 0.9f)) {
        return;
    }
    s32 colorIndex = CLAMP(this->actor.colChkInfo.health - 1, 0, 23) / 8;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 128, sPrimColors[colorIndex].r, sPrimColors[colorIndex].g,
                    sPrimColors[colorIndex].b, (u8)this->fadeAlpha);
    gDPSetEnvColor(POLY_XLU_DISP++, sEnvColors[colorIndex].r, sEnvColors[colorIndex].g, sEnvColors[colorIndex].b,
                   (u8)this->fadeAlpha);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0, 0xFF - (u8)(frames * 6), 8,
                                             0x40, 0, 0, 0, -6));
    gDPPipeSync(POLY_XLU_DISP++);
    gSPSegment(POLY_XLU_DISP++, 0x09, (uintptr_t)sXluRenderModeDL);
    POLY_XLU_DISP =
        SkelAnime_DrawFlex(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                           EnFd_OverrideLimbDraw, EnFd_PostLimbDraw, &this->actor, POLY_XLU_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
}

static bool EnFw_DoBounce(EnFw* this, s32 totalBounces, f32 yVelocity) {
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (this->actor.velocity.y > 0.0f)) {
        return false;
    }
    Actor_PlaySfx(&this->actor, FW_SFX_BOUNCE);
    this->bounceCount--;
    if (this->bounceCount <= 0) {
        if (this->bounceCount == 0) {
            this->actor.velocity.y = 0.0f;
            return true;
        }
        this->bounceCount = totalBounces;
    }
    this->actor.velocity.y = yVelocity * ((f32)this->bounceCount / totalBounces);
    return true;
}

static bool EnFw_IsPlayerAhead(EnFw* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    CollisionPoly* poly;
    s32 bgId;
    Vec3f hitPos;

    if (this->actor.xzDistToPlayer > 300.0f) {
        return false;
    }
    if (ABS((s16)((f32)this->actor.yawTowardsPlayer - (f32)this->actor.shape.rot.y)) > 0x1C70) {
        return false;
    }
    return !BgCheck_EntityLineTest1(&play->colCtx, &this->actor.world.pos, &player->actor.world.pos, &hitPos, &poly,
                                    true, false, false, true, &bgId);
}

static bool EnFw_CheckCollider(EnFw* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return false;
    }
    ColliderElement* elem = &this->collider.elements[0].base;
    this->wasHookshotHit = (elem->acHitElem != NULL) && (elem->acHitElem->atDmgInfo.dmgFlags & DMG_HOOKSHOT);
    this->collider.base.acFlags &= ~AC_HIT;
    if (Actor_ApplyDamage(&this->actor) > 0) {
        return true;
    }
    Actor* flareDancer = this->actor.parent;
    if (flareDancer->colChkInfo.health <= FD_HEALTH_PER_CORE) {
        Enemy_StartFinishingBlow(play, &this->actor);
        flareDancer->colChkInfo.health = 0;
    } else {
        flareDancer->colChkInfo.health -= FD_HEALTH_PER_CORE;
    }
    this->returnToParentTimer = 0;
    return true;
}

static void EnFw_SpawnDust(EnFw* this, const DustBurst* burst) {
    EffectSpawn spawn = { FD_EFFECT_FLAME, 8, 0.16f, burst->scaleStep };
    s16 angle = (Rand_ZeroOne() - 0.5f) * 0x10000;

    spawn.pos.y = this->actor.floorHeight + 2.0f;
    for (s32 i = burst->count; i >= 0; i--) {
        spawn.accel = (Vec3f){ (Rand_ZeroOne() - 0.5f) * 20.0f, burst->yAccel, (Rand_ZeroOne() - 0.5f) * 20.0f };
        spawn.pos.x = (Math_SinS(angle) * burst->radius) + this->actor.world.pos.x;
        spawn.pos.z = (Math_CosS(angle) * burst->radius) + this->actor.world.pos.z;
        AddEffect(this->effects, FW_EFFECT_COUNT, &spawn);
        angle += (s16)(0x10000 / burst->count);
    }
}

static void EnFw_JumpToParentHome(EnFw* this, PlayState* play) {
    Actor* flareDancer = this->actor.parent;

    if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (this->actor.velocity.y <= 0.0f)) {
        flareDancer->params |= FD_CORE_DONE;
        Actor_Kill(&this->actor);
        return;
    }
    Math_SmoothStepToF(&this->actor.world.pos.x, flareDancer->home.pos.x, 0.6f, 8.0f, 0.0f);
    Math_SmoothStepToF(&this->actor.world.pos.z, flareDancer->home.pos.z, 0.6f, 8.0f, 0.0f);
}

static void EnFw_TurnToParentHome(EnFw* this, PlayState* play) {
    s16 yawToHome = Math_Vec3f_Yaw(&this->actor.world.pos, &this->actor.parent->home.pos);

    Math_SmoothStepToS(&this->actor.shape.rot.y, yawToHome, 4, 0xFA0, 1);
    if (AbsYawDiff(yawToHome, this->actor.shape.rot.y) >= 0x65) {
        return;
    }
    this->actor.world.rot = this->actor.shape.rot;
    this->actor.velocity.y = 14.0f;
    this->actor.home.pos = this->actor.world.pos;
    Actor_PlaySfx(&this->actor, FW_SFX_JUMP);
    Actor_ChangeAnimationByInfo(&this->skelAnime, sFwAnimations, FW_ANIM_RUN);
    this->actionFunc = EnFw_JumpToParentHome;
}

static void EnFw_Explode(EnFw* this, PlayState* play) {
    this->skelAnime.playSpeed = 0.0f;
    Math_SmoothStepToF(&this->actor.scale.x, 0.025f, 0.08f, 0.6f, 0.0f);
    Actor_SetScale(&this->actor, this->actor.scale.x);
    if (this->actor.colorFilterTimer == 0) {
        Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 200, COLORFILTER_BUFFLAG_OPA,
                             this->explosionTimer);
        this->explosionTimer--;
    }
    if (this->explosionTimer != 0) {
        return;
    }
    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, this->bombPos.x, this->bombPos.y, this->bombPos.z,
                BOMB_EXPLOSIVE_TYPE_BOMB, 0, 0, BOMB_TYPE_EXPLOSION);
    this->actor.parent->params |= FD_CORE_DEAD;
    Item_DropCollectibleRandom(play, NULL, &this->actor.world.pos, 0xA0);
    Actor_Kill(&this->actor);
}

static void EnFw_TurnAround(EnFw* this) {
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 0.1f, 1.0f, 0.0f);
    f32 facing = this->actor.shape.rot.y;
    f32 remaining = Math_SmoothStepToF(&facing, (s16)(this->actor.world.rot.y ^ 0x8000), 0.1f, 10000.0f, 0.0f);
    this->actor.shape.rot.y = facing;
    if (remaining <= 0x1554) {
        this->isTurningAround = false;
    }
}

static void EnFw_RunAround(EnFw* this, PlayState* play) {
    DECR(this->damageTimer);
    if (((200.0f - this->runRadius) < 0.9f) && (DECR(this->returnToParentTimer) == 0)) {
        this->actor.speed = 0.0f;
        this->actionFunc = EnFw_TurnToParentHome;
        return;
    }
    Math_SmoothStepToF(&this->runRadius, 200.0f, 0.3f, 100.0f, 0.0f);
    if (this->isTurningAround) {
        EnFw_TurnAround(this);
        if (this->isTurningAround) {
            return;
        }
    } else {
        Vec3f step = GetStepAroundCircle(&this->actor, &this->actor.parent->home.pos, this->runRadius, this->runDir);
        Math_SmoothStepToS(&this->actor.shape.rot.y, YawOfDelta(&step), 4, 0xFA0, 1);
    }
    this->actor.world.rot = this->actor.shape.rot;
    if ((this->slideTimer == 0) && EnFw_IsPlayerAhead(this, play)) {
        Actor_PlaySfx(&this->actor, FW_SFX_SURPRISED);
        this->slideSfxTimer = 8;
        this->slideTimer = 8;
    }
    if (this->slideTimer == 0) {
        Math_SmoothStepToF(&this->actor.speed, 6.0f, 0.1f, 1.0f, 0.0f);
        s16 frame = this->skelAnime.curFrame;
        if ((frame == 1) || (frame == 4)) {
            Actor_PlaySfx(&this->actor, FW_SFX_RUN);
            EnFw_SpawnDust(this, &(DustBurst){ 0.1f, 1, 0.0f, 0.0f });
        }
        return;
    }
    if (DECR(this->slideSfxTimer) == 0) {
        Actor_PlaySfx(&this->actor, FW_SFX_SLIDE);
        this->slideSfxTimer = 4;
    }
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 0.1f, 1.0f, 0.0f);
    this->skelAnime.playSpeed = 0.0f;
    EnFw_SpawnDust(this, &(DustBurst){ 0.2f, 3, 8.0f, ((Rand_ZeroOne() - 0.5f) * 0.2f) + 0.3f });
    this->slideTimer--;
    if (this->slideTimer == 0) {
        this->isTurningAround = true;
        this->runDir = -this->runDir;
    }
}

static void EnFw_Run(EnFw* this, PlayState* play) {
    Math_SmoothStepToF(&this->skelAnime.playSpeed, 1.0f, 0.1f, 1.0f, 0.0f);
    if (this->skelAnime.animation == (AnimationHeader*)sCoreInitRunAnim) {
        if (!Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
            this->runRadius = Math_Vec3f_DistXYZ(&this->actor.world.pos, &this->actor.parent->world.pos);
            Actor_ChangeAnimationByInfo(&this->skelAnime, sFwAnimations, FW_ANIM_END_RUN);
        }
        return;
    }
    if ((this->damageTimer == 0) && (this->explosionTimer == 0) && EnFw_CheckCollider(this, play)) {
        Actor_PlaySfx(&this->actor, FW_SFX_DAMAGE);
        if (this->actor.parent->colChkInfo.health > 0) {
            if (!this->wasHookshotHit) {
                this->actor.velocity.y = 6.0f;
            }
            this->damageTimer = 20;
        } else {
            this->explosionTimer = 6;
        }
        this->actor.speed = 0.0f;
    }
    if (this->explosionTimer != 0) {
        EnFw_Explode(this, play);
        return;
    }
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (this->actor.velocity.y > 0.0f)) {
        Actor_SetColorFilter(&this->actor, COLORFILTER_COLORFLAG_RED, 200, COLORFILTER_BUFFLAG_OPA, this->damageTimer);
        return;
    }
    EnFw_RunAround(this, play);
}

static void EnFw_Bounce(EnFw* this, PlayState* play) {
    if (EnFw_DoBounce(this, 3, 8.0f) && (this->bounceCount == 0)) {
        this->returnToParentTimer = Rand_S16Offset(300, 150);
        this->actionFunc = EnFw_Run;
    }
}

static void EnFw_Init(Actor* thisx, PlayState* play) {
    EnFw* this = (EnFw*)thisx;

    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sCoreSkel, NULL, this->jointTable, this->morphTable,
                       FW_LIMB_COUNT);
    Actor_ChangeAnimationByInfo(&this->skelAnime, sFwAnimations, FW_ANIM_INIT_RUN);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 20.0f);
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sFwColliderInit, this->colliderElements);
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, &sFwDamageTable, &sFwColChkInit);
    Actor_SetScale(&this->actor, 0.01f);
    this->runDir = -this->actor.params;
    this->actor.gravity = -1.0f;
    this->actionFunc = EnFw_Bounce;
}

static void EnFw_Destroy(Actor* thisx, PlayState* play) {
    EnFw* this = (EnFw*)thisx;

    Collider_DestroyJntSph(play, &this->collider);
}

static void EnFw_Update(Actor* thisx, PlayState* play) {
    EnFw* this = (EnFw*)thisx;

    // Assumes a live Flare Dancer parent; a core left behind by a removed one has nothing to return to.
    if ((this->actor.parent == NULL) || (this->actor.parent->update == NULL)) {
        Actor_Kill(&this->actor);
        return;
    }
    SkelAnime_Update(&this->skelAnime);
    if (CHECK_FLAG_ALL(this->actor.flags, ACTOR_FLAG_HOOKSHOT_ATTACHED)) {
        return;
    }
    Actor_MoveWithGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 10.0f, 20.0f, 0.0f, UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4);
    this->actionFunc(this, play);
    if ((this->damageTimer == 0) && (this->explosionTimer == 0) && (this->actionFunc == EnFw_Run)) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void EnFw_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, Actor* thisx) {
    static Vec3f sOrigin = { 0.0f, 0.0f, 0.0f };
    EnFw* this = (EnFw*)thisx;

    if (limbIndex == FW_LIMB_BODY) {
        Matrix_MultVec3f(&sOrigin, &this->bombPos);
    }
    if (limbIndex == FW_LIMB_HEAD) {
        Matrix_MultVec3f(&sOrigin, &this->actor.focus.pos);
    }
    Collider_UpdateSpheres(limbIndex, &this->collider);
}

static void EnFw_Draw(Actor* thisx, PlayState* play) {
    EnFw* this = (EnFw*)thisx;
    static const FlameStyle sDust = { { 170, 130, 90, 0 }, { 100, 60, 20, 0 }, true };

    UpdateFlames(this->effects, FW_EFFECT_COUNT);
    Matrix_Push();
    DrawFlames(play, this->effects, FW_EFFECT_COUNT, &sDust);
    Matrix_Pop();
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          EnFw_PostLimbDraw, &this->actor);
}

static void EnFdFire_UpdatePos(EnFdFire* this, Vec3f* targetPos) {
    f32 xDiff = targetPos->x - this->actor.world.pos.x;
    f32 yDiff = targetPos->y - this->actor.world.pos.y;
    f32 zDiff = targetPos->z - this->actor.world.pos.z;
    f32 dist = sqrtf(SQ(xDiff) + SQ(yDiff) + SQ(zDiff));

    if (fabsf(dist) > fabsf(this->actor.speed)) {
        this->actor.velocity.x = (xDiff / dist) * this->actor.speed;
        this->actor.velocity.z = (zDiff / dist) * this->actor.speed;
    } else {
        this->actor.velocity.x = 0.0f;
        this->actor.velocity.z = 0.0f;
    }
    this->actor.velocity.y += this->actor.gravity;
    this->actor.velocity.y = MAX(this->actor.velocity.y, this->actor.terminalVelocity);
}

static void EnFdFire_Disappear(EnFdFire* this, PlayState* play) {
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 0.6f, 9.0f, 0.0f);
    Actor_UpdateVelocityWithGravity(&this->actor);
    Math_SmoothStepToF(&this->scale, 0.0f, 0.3f, 0.1f, 0.0f);
    this->actor.shape.shadowScale = 20.0f * (this->scale / 3.0f);
    if (this->scale <= 0.01f) {
        Actor_Kill(&this->actor);
    }
}

static void EnFdFire_WaitToDie(EnFdFire* this, PlayState* play) {
    if (DECR(this->deathTimer) == 0) {
        this->actionFunc = EnFdFire_Disappear;
    }
}

static void EnFdFire_DanceTowardsPlayer(EnFdFire* this, PlayState* play) {
    static f32 sAngles[] = {
        0.0f, 210.0f, 60.0f, 270.0f, 120.0f, 330.0f, 180.0f, 30.0f, 240.0f, 90.0f, 300.0f, 150.0f
    };
    Player* player = GET_PLAYER(play);
    s16 index = ((play->state.frames / 10) + (this->actor.params & 0x7FFF)) % ARRAY_COUNT(sAngles);
    Vec3f pos = player->actor.world.pos;

    // Vanilla feeds these degree values to sinf and cosf as radians; kept for the same dance.
    pos.x += 120.0f * sinf(sAngles[index]);
    pos.z += 120.0f * cosf(sAngles[index]);
    if (DECR(this->deathTimer) == 0) {
        this->actionFunc = EnFdFire_Disappear;
        return;
    }
    Math_SmoothStepToS(&this->actor.world.rot.y, Math_Vec3f_Yaw(&this->actor.world.pos, &pos), 8, 0xFA0, 1);
    Math_SmoothStepToF(&this->actor.speed, 0.0f, 0.4f, 1.0f, 0.0f);
    if (this->actor.speed < 0.1f) {
        this->actor.speed = 5.0f;
    }
    Actor_UpdateVelocityWithGravity(&this->actor);
}

static void EnFdFire_Launch(EnFdFire* this, PlayState* play) {
    Vec3f targetPos = this->actor.parent->world.pos;

    targetPos.x += this->spawnRadius * Math_SinS(this->actor.world.rot.y);
    targetPos.z += this->spawnRadius * Math_CosS(this->actor.world.rot.y);
    EnFdFire_UpdatePos(this, &targetPos);
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (this->actor.velocity.y > 0.0f)) {
        return;
    }
    this->actor.velocity = (Vec3f){ 0.0f, 0.0f, 0.0f };
    this->actor.speed = 0.0f;
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    if (this->actor.params & FD_FIRE_BLUE) {
        this->deathTimer = 200;
        this->actionFunc = EnFdFire_DanceTowardsPlayer;
    } else {
        this->deathTimer = 300;
        this->actionFunc = EnFdFire_WaitToDie;
    }
}

static bool EnFdFire_WasTouched(EnFdFire* this) {
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        return true;
    }
    if (this->collider.base.ocFlags2 & OC2_HIT_PLAYER) {
        this->collider.base.ocFlags2 &= ~OC2_HIT_PLAYER;
        return true;
    }
    return false;
}

static void EnFdFire_Init(Actor* thisx, PlayState* play) {
    EnFdFire* this = (EnFdFire*)thisx;
    Player* player = GET_PLAYER(play);

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 20.0f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sFireColliderInit);
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, &sFireDamageTable, &sFireColChkInit);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.gravity = -0.6f;
    this->actor.speed = 5.0f;
    this->actor.velocity.y = 12.0f;
    this->spawnRadius = Math_Vec3f_DistXYZ(&this->actor.world.pos, &player->actor.world.pos);
    this->scale = 3.0f;
    this->scrollSpeed = (s16)Rand_ZeroFloat(5.0f) - 25;
    this->actionFunc = EnFdFire_Launch;
}

static void EnFdFire_Destroy(Actor* thisx, PlayState* play) {
    EnFdFire* this = (EnFdFire*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void EnFdFire_Update(Actor* thisx, PlayState* play) {
    EnFdFire* this = (EnFdFire*)thisx;

    if (this->actionFunc != EnFdFire_Disappear) {
        // The engine clears parent once the Flare Dancer is gone, so its flames die out instead of reading it.
        bool isOrphaned = (this->actor.parent == NULL) || (this->actor.parent->update == NULL);
        if (isOrphaned || EnFdFire_WasTouched(this)) {
            this->actionFunc = EnFdFire_Disappear;
        }
    }
    Actor_UpdatePos(&this->actor);
    this->actionFunc(this, play);
    Actor_UpdateBgCheckInfo(play, &this->actor, 12.0f, 10.0f, 0.0f, UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4);
    if (this->actionFunc == EnFdFire_Disappear) {
        return;
    }
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
}

static void EnFdFire_Draw(Actor* thisx, PlayState* play) {
    static Color_RGB8 sPrimColors[] = { { 255, 255, 0 }, { 255, 255, 255 } };
    static Color_RGB8 sEnvColors[] = { { 255, 10, 0 }, { 0, 10, 255 } };
    EnFdFire* this = (EnFdFire*)thisx;
    s32 colorIndex = (this->actor.params & FD_FIRE_BLUE) ? 1 : 0;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    s16 camYaw = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play));
    s16 moveYaw = Math_Vec3f_Yaw(&zero, &this->actor.velocity) - camYaw;
    f32 sway = Math_SinS(moveYaw);
    f32 speed = Math_Vec3f_DistXZ(&zero, &this->actor.velocity) / 1.5f;
    f32 stretch = MAX((speed * ((0.01f * -15.0f) * fabsf(Math_CosS(moveYaw)))) + 1.0f, 0.1f);

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateYF((s16)(camYaw + 0x8000) * (M_PIf / 0x8000), MTXMODE_APPLY);
    Matrix_RotateZF(((sway * -10.0f) * speed) * (M_PIf / 180.0f), MTXMODE_APPLY);
    Matrix_Scale(this->scale * 0.001f, this->scale * 0.001f, this->scale * 0.001f, MTXMODE_APPLY);
    Matrix_Scale(1.0f, stretch, 1.0f / stretch, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0,
                                             play->state.frames * this->scrollSpeed, 0x20, 0x80, 0, 0, 0,
                                             this->scrollSpeed));
    gDPSetPrimColor(POLY_XLU_DISP++, 128, 128, sPrimColors[colorIndex].r, sPrimColors[colorIndex].g,
                    sPrimColors[colorIndex].b, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, sEnvColors[colorIndex].r, sEnvColors[colorIndex].g, sEnvColors[colorIndex].b, 255);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gEffFire1DL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Ship: the hammer (a Goron pound here) or the hookshot, or explosives plus a weapon for the core.
static bool CanKillFlareDancer(void* userData) {
    bool canHitCore = sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RI_DEKU_STICK) ||
                      sApi->LogicCanUseItem(RI_BOW) || sApi->LogicHasCapability("HookshotOrBoomerang");

    return sApi->LogicHasCapability("CanBeGoron") || sApi->LogicCanUseItem(RI_HOOKSHOT) ||
           (sApi->LogicHasCapability("HasExplosives") && canHitCore);
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

static s16 RegisterCore(void) {
    S2HActorDefinition core = DefineEnemyActor(FLARE_DANCER_CORE_KEY, "Flare Dancer's core", FD_FLAGS, sizeof(EnFw));

    core.init = EnFw_Init;
    core.destroy = EnFw_Destroy;
    core.update = EnFw_Update;
    core.draw = EnFw_Draw;
    // The core is what takes the hits, so a missing soul has to protect it too.
    core.soulKey = FLARE_DANCER_KEY;
    return sApi->RegisterActor(&core);
}

static s16 RegisterFire(void) {
    S2HActorDefinition fire =
        DefineEnemyActor(FLARE_DANCER_FIRE_KEY, "Flare Dancer's flame", FD_FIRE_FLAGS, sizeof(EnFdFire));

    fire.init = EnFdFire_Init;
    fire.destroy = EnFdFire_Destroy;
    fire.update = EnFdFire_Update;
    fire.draw = EnFdFire_Draw;
    return sApi->RegisterActor(&fire);
}

static s16 RegisterFlareDancer(void) {
    S2HActorDefinition body = DefineEnemyActor(FLARE_DANCER_KEY, "Flare Dancer", FD_FLAGS, sizeof(EnFd));

    body.init = EnFd_Init;
    body.destroy = EnFd_Destroy;
    body.update = EnFd_Update;
    body.draw = EnFd_Draw;
    body.enemyFlags =
        S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_MINIBOSS | S2H_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    body.naviHint = "Flare Dancer&A whirling dancer of flame. Blast or&hook its fire away, then strike the core!";
    // The host finds the soul item as "soul." + soulKey, so this is the family key.
    body.soulKey = FLARE_DANCER_KEY;
    body.logicKill = CanKillFlareDancer;
    body.enemyCategory = S2H_ENEMY_CATEGORY_MINIBOSS;
    body.soulModel.skeleton = sSkel;
    body.soulModel.idleAnimation = sTwirlAnim;
    body.soulModel.isFlex = true;
    body.soulModel.limbCount = FD_LIMB_COUNT;
    body.soulModel.scale = 0.01f;
    body.soulModel.flameRed = 255;
    body.soulModel.flameGreen = 150;
    body.soulModel.flameBlue = 60;
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
    if (!RequireOotAsset(FW_OBJECT "gFlareDancerSkel") || !RequireOotAsset(FW_OBJECT "gFlareDancerCoreSkel")) {
        sApi->Log(FLARE_DANCER_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    // The first definition sharing a soul names it and sets its category, so the dancer comes before its core.
    sFlareDancerId = RegisterFlareDancer();
    if (sFlareDancerId < 0) {
        sApi->Log("Could not register " FLARE_DANCER_KEY);
        return;
    }
    sCoreId = RegisterCore();
    sFireId = RegisterFire();
    if ((sCoreId < 0) || (sFireId < 0)) {
        sApi->Log("Could not register " FLARE_DANCER_CORE_KEY " or " FLARE_DANCER_FIRE_KEY);
    }
}
