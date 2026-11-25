/*
 * Ocarina of Time's flying floor tile (En_Yukabyun) on Majora's Mask, drawn from the OoT Asset Loader's oot.o2r.
 * Vanilla launches as soon as its room loads; this one waits until Link comes close, since it may stand anywhere.
 */
#include "2s2h/ModApi/ModApi.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define FLOOR_TILE_KEY "oot.floor_tile"
#define FT_OBJECT "objects/oot_object_yukabyun/"
#define FT_ASSET(name) "__OTR__" FT_OBJECT name

static const ALIGN_ASSET(2) char sTileDL[] = FT_ASSET("gFloorTileEnemyDL");
static const ALIGN_ASSET(2) char sFragmentDL[] = FT_ASSET("gFloorTileEnemyFragmentDL");
static const ALIGN_ASSET(2) char sTopTex[] = FT_ASSET("gFloorTileEnemyTopTex");

#define FT_WAKE_DISTANCE 400.0f
#define FT_LAUNCH_DELAY 30
#define FT_SPIN_LAUNCH 0x2000
#define FT_HOVER_HEIGHT 30.0f
#define FT_SPEED 10.0f
#define FT_LEASH 5000.0f

static const S2HModApi* sApi;
static s16 sTileActorId = -1;

struct EnYukabyun;
typedef void (*EnYukabyunActionFunc)(struct EnYukabyun*, PlayState*);

typedef struct EnYukabyun {
    Actor actor;
    EnYukabyunActionFunc actionFunc;
    ColliderCylinder collider;
    s16 timer;
} EnYukabyun;

static ColliderCylinderInit sCylinderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_NO_PUSH | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0xF7CFFFFF, 0x00, 0x04 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_HARD,
        ACELEM_ON,
        OCELEM_ON,
    },
    { 28, 8, 0, { 0, 0, 0 } },
};

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F(scale, 1, ICHAIN_CONTINUE),
    ICHAIN_F32(lockOnArrowOffset, 16, ICHAIN_STOP),
};

static void EnYukabyun_Fly(EnYukabyun* this, PlayState* play) {
    this->actor.shape.rot.y += this->timer;
    if (this->actor.xzDistToPlayer > FT_LEASH) {
        Actor_Kill(&this->actor);
        return;
    }
    Actor_PlaySfx_Flagged(&this->actor, NA_SE_IT_BOOMERANG_FLY - SFX_FLAG);
}

static void EnYukabyun_Rise(EnYukabyun* this, PlayState* play) {
    this->timer += 0xA0;
    this->actor.shape.rot.y += this->timer;
    if (this->timer >= FT_SPIN_LAUNCH) {
        this->actor.world.rot.y = this->actor.yawTowardsPlayer;
        this->actor.speed = FT_SPEED;
        this->actionFunc = EnYukabyun_Fly;
    }
    Math_StepToF(&this->actor.world.pos.y, this->actor.home.pos.y + FT_HOVER_HEIGHT, 1.0f);
    Actor_PlaySfx_Flagged(&this->actor, NA_SE_IT_BOOMERANG_FLY - SFX_FLAG);
}

static void EnYukabyun_Wait(EnYukabyun* this, PlayState* play) {
    if ((this->timer == FT_LAUNCH_DELAY) && (this->actor.xzDistToPlayer > FT_WAKE_DISTANCE)) {
        return;
    }
    this->timer--;
    if (this->timer == 0) {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_IGNORE_QUAKE;
        this->actionFunc = EnYukabyun_Rise;
    }
}

static void EnYukabyun_Break(EnYukabyun* this, PlayState* play) {
    EffectSsHahen_SpawnBurst(play, &this->actor.world.pos, 8.0f, 0, 1300, 300, 15, -1, 10, (Gfx*)sFragmentDL);
    Actor_Kill(&this->actor);
}

static bool EnYukabyun_HasCollided(EnYukabyun* this) {
    Collider* base = &this->collider.base;
    bool hitsOtherActor = (base->ocFlags1 & OC1_HIT) && (base->oc != NULL) && (base->oc->id != sTileActorId);
    bool hitsWall = (this->actionFunc == EnYukabyun_Fly) && (this->actor.bgCheckFlags & BGCHECKFLAG_WALL);

    return (base->atFlags & AT_HIT) || (base->acFlags & AC_HIT) || hitsOtherActor || hitsWall;
}

static void EnYukabyun_Init(Actor* thisx, PlayState* play) {
    EnYukabyun* this = (EnYukabyun*)thisx;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 0.4f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->timer = FT_LAUNCH_DELAY;
    this->actionFunc = EnYukabyun_Wait;
}

static void EnYukabyun_Destroy(Actor* thisx, PlayState* play) {
    EnYukabyun* this = (EnYukabyun*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void EnYukabyun_Update(Actor* thisx, PlayState* play) {
    EnYukabyun* this = (EnYukabyun*)thisx;

    if ((this->actionFunc != EnYukabyun_Break) && EnYukabyun_HasCollided(this)) {
        this->collider.base.atFlags &= ~AT_HIT;
        this->collider.base.acFlags &= ~AC_HIT;
        this->collider.base.ocFlags1 &= ~OC1_HIT;
        this->actor.flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 30, NA_SE_EN_OCTAROCK_ROCK);
        this->actionFunc = EnYukabyun_Break;
    }
    this->actionFunc(this, play);
    Actor_MoveWithGravity(&this->actor);
    if ((this->actionFunc != EnYukabyun_Wait) && (this->actionFunc != EnYukabyun_Break)) {
        Actor_UpdateBgCheckInfo(play, &this->actor, 5.0f, 20.0f, 8.0f, UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4);
        Collider_UpdateCylinder(&this->actor, &this->collider);
        this->actor.flags |= ACTOR_FLAG_SFX_FOR_PLAYER_BODY_HIT;
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
    Actor_SetFocus(&this->actor, 4.0f);
}

static void EnYukabyun_Draw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)sTopTex);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sTileDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s16 RegisterFloorTile(void) {
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = FLOOR_TILE_KEY;
    definition.description = "Flying Floor Tile";
    definition.category = ACTORCAT_ENEMY;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnYukabyun);
    definition.init = EnYukabyun_Init;
    definition.destroy = EnYukabyun_Destroy;
    definition.update = EnYukabyun_Update;
    definition.draw = EnYukabyun_Draw;
    definition.enemyFlags = S2H_ACTOR_ENEMY | S2H_ACTOR_ENEMY_NOT_IN_WATER;
    definition.naviHint = "Flying Floor Tile&It breaks on whatever it hits.&Guard with your shield!";
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
    if (!RequireOotAsset(FT_OBJECT "gFloorTileEnemyDL")) {
        sApi->Log(FLOOR_TILE_KEY ": OoT Asset Loader resources are missing; extract them to enable this enemy.");
        return;
    }
    sTileActorId = RegisterFloorTile();
    if (sTileActorId < 0) {
        sApi->Log("Could not register " FLOOR_TILE_KEY);
    }
}
