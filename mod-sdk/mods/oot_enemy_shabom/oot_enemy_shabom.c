/*
 * Ocarina of Time's Shabom (En_Bubble) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define SHABOM_KEY "oot.shabom"
#define SHABOM_SOUL_KEY "soul.oot.shabom"
#define SHABOM_OBJECT "objects/oot_object_bubble/"

static const ALIGN_ASSET(2) char sBubbleDL[] = "__OTR__" SHABOM_OBJECT "gBubbleDL";

#define SHABOM_POP_FRAMES 6
#define SHABOM_POP_DAMAGE 4
#define SHABOM_PROBE_LENGTH 24.0f

static const S2HModApi* sApi;

struct EnBubble;
typedef void (*EnBubbleActionFunc)(struct EnBubble*, PlayState*);

typedef struct EnBubble {
    Actor actor;
    EnBubbleActionFunc actionFunc;
    ColliderJntSph collider;
    ColliderJntSphElement colliderElements[2];
    s16 popCooldown;
    s16 explosionCountdown;
    f32 graphicRotSpeed;
    f32 graphicEccentricity;
    f32 expansionWidth;
    f32 expansionHeight;
    u8 bounceCount;
    Vec3f bounceDirection;
    Vec3f velocityFromBounce;
    Vec3f velocityFromBump;
    f32 sinkSpeed;
} EnBubble;

// Element 0 pops on most hits; element 1 only gets pushed by arrows, as the slingshot and arrows do in OoT.
static ColliderJntSphElementInit sJntSphElementsInit[2] = {
    {
        {
            ELEM_MATERIAL_UNK0,
            { 0x00000000, 0x00, 0x04 },
            { 0xF7CFD753, 0x00, 0x00 },
            ATELEM_NONE,
            ACELEM_ON,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 16 }, 100 },
    },
    {
        {
            ELEM_MATERIAL_UNK0,
            { 0x00000000, 0x00, 0x00 },
            { 0x00002824, 0x00, 0x00 },
            ATELEM_NONE,
            ACELEM_ON | ACELEM_NO_AT_INFO | ACELEM_NO_DAMAGE | ACELEM_NO_SWORD_SFX | ACELEM_NO_HITMARK,
            OCELEM_NONE,
        },
        { 0, { { 0, 0, 0 }, 16 }, 100 },
    },
};

static ColliderJntSphInit sJntSphInit = {
    {
        COL_MATERIAL_HIT6,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_JNTSPH,
    },
    ARRAY_COUNT(sJntSphElementsInit),
    sJntSphElementsInit,
};

static CollisionCheckInfoInit2 sColChkInfoInit = { 1, 2, 25, 25, MASS_IMMOVABLE };

static Vec3f sEffectAccel = { 0.0f, -0.5f, 0.0f };
static Color_RGBA8 sEffectPrimColor = { 255, 255, 255, 255 };
static Color_RGBA8 sEffectEnvColor = { 150, 150, 150, 0 };

static void EnBubble_Normalize(Vec3f* vec) {
    f32 norm = sqrtf(SQ(vec->x) + SQ(vec->y) + SQ(vec->z));

    if (norm == 0.0f) {
        *vec = (Vec3f){ 0.0f, 0.0f, 0.0f };
        return;
    }
    vec->x /= norm;
    vec->y /= norm;
    vec->z /= norm;
}

static void EnBubble_Bounce(EnBubble* this, Vec3f* direction, Vec3f* normal) {
    Math3D_Vec3fReflect(direction, normal, &this->bounceDirection);
    EnBubble_Normalize(&this->bounceDirection);
    this->bounceCount++;
    if (this->bounceCount > (s16)(Rand_ZeroOne() * 10.0f)) {
        this->bounceCount = 0;
    }
    f32 bounceSpeed = (this->bounceCount == 0) ? 3.6f : 3.0f;
    this->velocityFromBump = (Vec3f){ 0.0f, 0.0f, 0.0f };
    this->velocityFromBounce.x = this->bounceDirection.x * bounceSpeed;
    this->velocityFromBounce.y = this->bounceDirection.y * bounceSpeed;
    this->velocityFromBounce.z = this->bounceDirection.z * bounceSpeed;
    this->sinkSpeed = 0.0f;
    Actor_PlaySfx(&this->actor, NA_SE_EN_AWA_BOUND);
    this->graphicRotSpeed = 128.0f;
    this->graphicEccentricity = 0.48f;
}

static void EnBubble_Fly(EnBubble* this, PlayState* play) {
    if (this->collider.elements[1].base.acElemFlags & ACELEM_HIT) {
        Vec3f push = this->collider.base.ac->velocity;

        EnBubble_Normalize(&push);
        this->velocityFromBump.x += push.x * 3.0f;
        this->velocityFromBump.y += push.y * 3.0f;
        this->velocityFromBump.z += push.z * 3.0f;
    }
    this->sinkSpeed = MAX(this->sinkSpeed - 0.1f, this->actor.terminalVelocity);
    Vec3f direction = { this->velocityFromBounce.x + this->velocityFromBump.x,
                        this->velocityFromBounce.y + this->velocityFromBump.y + this->sinkSpeed,
                        this->velocityFromBounce.z + this->velocityFromBump.z };
    EnBubble_Normalize(&direction);
    Vec3f center = { this->actor.world.pos.x, this->actor.world.pos.y + this->actor.shape.yOffset,
                     this->actor.world.pos.z };
    Vec3f probe = { center.x + direction.x * SHABOM_PROBE_LENGTH, center.y + direction.y * SHABOM_PROBE_LENGTH,
                    center.z + direction.z * SHABOM_PROBE_LENGTH };
    Vec3f hit;
    CollisionPoly* poly = NULL;
    s32 bgId = 0;
    if (BgCheck_EntityLineTest1(&play->colCtx, &center, &probe, &hit, &poly, true, true, true, false, &bgId)) {
        Vec3f normal = { COLPOLY_GET_NORMAL(poly->normal.x), COLPOLY_GET_NORMAL(poly->normal.y),
                         COLPOLY_GET_NORMAL(poly->normal.z) };
        EnBubble_Bounce(this, &direction, &normal);
    } else if ((this->actor.bgCheckFlags & BGCHECKFLAG_WATER) && (direction.y < 0.0f)) {
        Vec3f up = { 0.0f, 1.0f, 0.0f };
        EnBubble_Bounce(this, &direction, &up);
    }
    this->actor.velocity.x = this->velocityFromBounce.x + this->velocityFromBump.x;
    this->actor.velocity.y = this->velocityFromBounce.y + this->velocityFromBump.y + this->sinkSpeed;
    this->actor.velocity.z = this->velocityFromBounce.z + this->velocityFromBump.z;
    Math_ApproachF(&this->velocityFromBump.x, 0.0f, 0.3f, 0.1f);
    Math_ApproachF(&this->velocityFromBump.y, 0.0f, 0.3f, 0.1f);
    Math_ApproachF(&this->velocityFromBump.z, 0.0f, 0.3f, 0.1f);
}

static bool EnBubble_WasPopped(EnBubble* this) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return false;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    if (this->collider.elements[1].base.acElemFlags & ACELEM_HIT) {
        this->graphicRotSpeed = 128.0f;
        this->graphicEccentricity = 0.48f;
        return false;
    }
    this->popCooldown = 8;
    return true;
}

static bool EnBubble_ShouldPop(EnBubble* this, PlayState* play) {
    if (DECR(this->popCooldown) != 0) {
        return false;
    }
    if (!(this->collider.base.ocFlags2 & OC2_HIT_PLAYER)) {
        return EnBubble_WasPopped(this);
    }
    this->collider.base.ocFlags2 &= ~OC2_HIT_PLAYER;
    play->damagePlayer(play, -SHABOM_POP_DAMAGE);
    func_800B8E1C(play, &this->actor, 6.0f, this->actor.yawTowardsPlayer, 6.0f);
    this->popCooldown = 8;
    return true;
}

static void EnBubble_Pop(EnBubble* this, PlayState* play) {
    Math_SmoothStepToF(&this->expansionWidth, 4.0f, 0.1f, 1000.0f, 0.0f);
    Math_SmoothStepToF(&this->expansionHeight, 4.0f, 0.1f, 1000.0f, 0.0f);
    Math_SmoothStepToF(&this->graphicRotSpeed, 54.0f, 0.1f, 1000.0f, 0.0f);
    Math_SmoothStepToF(&this->graphicEccentricity, 0.2f, 0.1f, 1000.0f, 0.0f);
    this->actor.shape.yOffset = (this->expansionHeight + 1.0f) * 16.0f;
    if (DECR(this->explosionCountdown) != 0) {
        return;
    }
    Vec3f pos = { this->actor.world.pos.x, this->actor.world.pos.y + this->actor.shape.yOffset,
                  this->actor.world.pos.z };
    for (s32 i = 0; i < 20; i++) {
        Vec3f velocity = { (Rand_ZeroOne() - 0.5f) * 7.0f, Rand_ZeroOne() * 7.0f, (Rand_ZeroOne() - 0.5f) * 7.0f };

        EffectSsDtBubble_SpawnCustomColor(play, &pos, &velocity, &sEffectAccel, &sEffectPrimColor, &sEffectEnvColor,
                                          Rand_S16Offset(100, 50), 25, 0);
    }
    Item_DropCollectibleRandom(play, NULL, &this->actor.world.pos, 0x50);
    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 60, NA_SE_EN_AWA_BREAK);
    Actor_Kill(&this->actor);
}

static void EnBubble_StartPop(EnBubble* this) {
    ColliderElement* body = &this->collider.elements[0].base;

    body->atDmgInfo.dmgFlags = DMG_EXPLOSIVES;
    body->atDmgInfo.effect = 0;
    body->atDmgInfo.damage = SHABOM_POP_DAMAGE;
    body->atElemFlags = ATELEM_ON;
    this->actor.velocity.y = 0.0f;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->explosionCountdown = SHABOM_POP_FRAMES;
    this->actionFunc = EnBubble_Pop;
}

static void EnBubble_Wait(EnBubble* this, PlayState* play) {
    if (EnBubble_ShouldPop(this, play)) {
        EnBubble_StartPop(this);
        return;
    }
    EnBubble_Fly(this, play);
    this->actor.shape.yOffset = (this->expansionHeight + 1.0f) * 16.0f;
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void EnBubble_UpdateColliders(EnBubble* this) {
    ColliderJntSphElementDim* dim = &this->collider.elements[0].dim;

    Matrix_MultVec3f(&dim->modelSphere.center, &dim->worldSphere.center);
    dim->worldSphere.radius = dim->modelSphere.radius * (1.0f + this->expansionWidth);
    this->collider.elements[1].dim = *dim;
}

static void EnBubble_Init(Actor* thisx, PlayState* play) {
    EnBubble* this = (EnBubble*)thisx;

    ActorShape_Init(&this->actor.shape, 16.0f, ActorShadow_DrawCircle, 0.2f);
    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sJntSphInit, this->colliderElements);
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, NULL, &sColChkInfoInit);
    this->bounceDirection = (Vec3f){ Rand_ZeroOne(), Rand_ZeroOne(), Rand_ZeroOne() };
    EnBubble_Normalize(&this->bounceDirection);
    this->velocityFromBounce.x = this->bounceDirection.x * 3.0f;
    this->velocityFromBounce.y = this->bounceDirection.y * 3.0f;
    this->velocityFromBounce.z = this->bounceDirection.z * 3.0f;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    Actor_SetScale(&this->actor, 1.0f);
    this->actor.shape.yOffset = 16.0f;
    this->graphicRotSpeed = 16.0f;
    this->graphicEccentricity = 0.08f;
    this->actionFunc = EnBubble_Wait;
}

static void EnBubble_Destroy(Actor* thisx, PlayState* play) {
    EnBubble* this = (EnBubble*)thisx;

    Collider_DestroyJntSph(play, &this->collider);
}

static void EnBubble_Update(Actor* thisx, PlayState* play) {
    EnBubble* this = (EnBubble*)thisx;

    Actor_UpdatePos(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 16.0f, 16.0f, 0.0f,
                            UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_2 | UPDBGCHECKINFO_FLAG_4);
    this->actionFunc(this, play);
    Actor_SetFocus(&this->actor, this->actor.shape.yOffset);
}

static void EnBubble_Draw(Actor* thisx, PlayState* play) {
    EnBubble* this = (EnBubble*)thisx;
    f32 wobble = (f32)play->state.frames * (M_PIf / 180.0f) * this->graphicRotSpeed;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Math_SmoothStepToF(&this->graphicRotSpeed, 16.0f, 0.2f, 1000.0f, 0.0f);
    Math_SmoothStepToF(&this->graphicEccentricity, 0.08f, 0.2f, 1000.0f, 0.0f);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(this->expansionWidth + 1.0f, this->expansionHeight + 1.0f, 1.0f, MTXMODE_APPLY);
    Matrix_RotateZF(wobble, MTXMODE_APPLY);
    Matrix_Scale(this->graphicEccentricity + 1.0f, 1.0f, 1.0f, MTXMODE_APPLY);
    Matrix_RotateZF(-wobble, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBubbleDL);
    CLOSE_DISPS(play->state.gfxCtx);
    this->actor.shape.shadowScale = (this->expansionWidth + 1.0f) * 0.2f;
    EnBubble_UpdateColliders(this);
}

// The soul shows the bubble, as Majora's Mask souls show their enemy.
static void DrawSoulModel(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(1.5f, 1.5f, 1.5f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sBubbleDL);
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
    gDPSetPrimColor(POLY_XLU_DISP++, 0xC0, 0xC0, 200, 230, 255, 0);
    gDPSetEnvColor(POLY_XLU_DISP++, 200, 230, 255, 0);
    Matrix_Scale(0.13333333f, 0.13333333f, 0.13333333f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gameplay_keep_DL_01ACF0);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Ship: boomerang, nuts, a jumpslash, Din's Fire or Ice Arrows; Zora stands in for the boomerang.
static bool CanKillShabom(void* userData) {
    return sApi->LogicHasCapability("CanUseSword") || sApi->LogicCanUseItem(RI_DEKU_STICK) ||
           sApi->LogicCanUseItem(RI_DEKU_NUT) || sApi->LogicHasCapability("CanBeZora") ||
           sApi->LogicHasCapability("CanBeGoron") || sApi->LogicHasCapability("CanUseIceArrow");
}

static void RegisterSoul(void) {
    static S2HCustomItemRandomizer sSoulLogic = { 0 };
    S2HCustomItemDefinition soul = { 0 };

    // Pool membership follows the host's soul shuffle and soul pool; the mod only declares the item.
    sSoulLogic.structSize = sizeof(sSoulLogic);
    sSoulLogic.flags = S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT;
    sSoulLogic.type = S2H_CUSTOM_ITEM_TYPE_ITEM;
    sSoulLogic.logicKey = SHABOM_SOUL_KEY;
    sSoulLogic.hintKey = SHABOM_SOUL_KEY;
    soul.structSize = sizeof(soul);
    soul.key = SHABOM_SOUL_KEY;
    soul.presentationFlags = S2H_ITEM_HIDE_FROM_KALEIDO;
    soul.getItemText = "You got the %rSoul of Shaboms%w!&They can be defeated now.";
    soul.getItemEntry.drawFunc = DrawSoul;
    soul.randomizer = &sSoulLogic;
    if (!sApi->RegisterCustomItem(&soul)) {
        sApi->Log("Could not register " SHABOM_SOUL_KEY);
    }
}

static s16 RegisterShabom(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = SHABOM_KEY;
    definition.description = "Shabom";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnBubble);
    definition.init = EnBubble_Init;
    definition.destroy = EnBubble_Destroy;
    definition.update = EnBubble_Update;
    definition.draw = EnBubble_Draw;
    definition.enemyFlags = S2H_ACTOR_ENEMY;
    definition.enemySpawnHeight = 40;
    definition.naviHint = "Shabom&A bubble that bursts on contact.&Pop it from a distance!";
    // The host finds the soul item as "soul." + soulKey, so this is the family key.
    definition.soulKey = SHABOM_KEY;
    definition.logicKill = CanKillShabom;
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
    if (!RequireOotAsset(SHABOM_OBJECT "gBubbleDL")) {
        sApi->Log(SHABOM_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    if (RegisterShabom() < 0) {
        sApi->Log("Could not register " SHABOM_KEY);
        return;
    }
    RegisterSoul();
}
