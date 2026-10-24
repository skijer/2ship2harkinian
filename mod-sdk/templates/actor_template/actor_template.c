#include "2s2h/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "sfx.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#define RUPEE_SPRITE_KEY "template.rupee_sprite"
#define RUPEE_SPRITE_SCALE 0.03f
#define RUPEE_SPRITE_SPIN_SPEED 0x400
#define RUPEE_SPRITE_BOB_HEIGHT 6.0f
#define RUPEE_SPRITE_BOB_SPEED 0x800
#define RUPEE_SPRITE_REWARD_COUNT 3

typedef struct RupeeSprite {
    Actor actor;
    ColliderCylinder collider;
    Vec3f home;
    s16 bobPhase;
} RupeeSprite;

static ColliderCylinderInit sCylinderInit = {
    {
        COL_MATERIAL_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xF7CFFFFF, 0x00, 0x00 },
        ATELEM_NONE | ATELEM_SFX_NORMAL,
        ACELEM_ON,
        OCELEM_NONE,
    },
    { 15, 30, -10, { 0, 0, 0 } },
};

static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), NULL, 0 };

static const S2HModApi* sApi;

static bool WasHit(RupeeSprite* this) {
    return (this->collider.base.acFlags & AC_HIT) != 0;
}

static void DropReward(RupeeSprite* this, PlayState* play) {
    for (int32_t i = 0; i < RUPEE_SPRITE_REWARD_COUNT; i++) {
        Item_DropCollectible(play, &this->actor.world.pos, ITEM00_RUPEE_RED);
    }
    EffectSsHitmark_SpawnFixedScale(play, 0, &this->actor.world.pos);
    Audio_PlaySfx(NA_SE_SY_GET_RUPY);
}

static void RupeeSprite_Init(Actor* thisx, PlayState* play) {
    RupeeSprite* this = (RupeeSprite*)thisx;

    Actor_SetScale(&this->actor, RUPEE_SPRITE_SCALE);
    this->home = this->actor.world.pos;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
}

static void RupeeSprite_Destroy(Actor* thisx, PlayState* play) {
    RupeeSprite* this = (RupeeSprite*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void RupeeSprite_Update(Actor* thisx, PlayState* play) {
    RupeeSprite* this = (RupeeSprite*)thisx;

    if (WasHit(this)) {
        DropReward(this, play);
        Actor_Kill(&this->actor);
        return;
    }
    this->bobPhase += RUPEE_SPRITE_BOB_SPEED;
    this->actor.world.pos.y = this->home.y + Math_SinS(this->bobPhase) * RUPEE_SPRITE_BOB_HEIGHT;
    this->actor.shape.rot.y += RUPEE_SPRITE_SPIN_SPEED;
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
}

static void RupeeSprite_Draw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, Lib_SegmentedToVirtual(gRupeeRedTex));
    gSPDisplayList(POLY_OPA_DISP++, gRupeeDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterActor)) {
        return;
    }
    S2HActorDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = RUPEE_SPRITE_KEY;
    definition.description = "Rupee Sprite";
    definition.category = ACTORCAT_MISC;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(RupeeSprite);
    definition.init = RupeeSprite_Init;
    definition.destroy = RupeeSprite_Destroy;
    definition.update = RupeeSprite_Update;
    definition.draw = RupeeSprite_Draw;
    sApi->RegisterActor(&definition);
}
