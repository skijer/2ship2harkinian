/**
 * Roc's Boots: half gravity, and a floor wherever Link lands on water or on lava, quicksand and the void.
 */

#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define ROC_BOOTS_KEY "nei.equip.roc_boots"
#define DEKU_LEAF_KEY "nei.deku_leaf"
#define STATES_WITHOUT_GRAVITY                                                                              \
    (PLAYER_STATE1_8000000 | PLAYER_STATE1_4 | PLAYER_STATE1_2000 | PLAYER_STATE1_4000 | PLAYER_STATE1_200000 | \
     PLAYER_STATE1_800000 | PLAYER_STATE1_DEAD)

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconRocBootsTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gRocBootsNameTex";

s32 func_808340AC(FloorType floorType);
bool func_808340D4(FloorType floorType);

static const S2HModApi* sApi;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(ROC_BOOTS_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

// Hot floors burn on a timer (func_808340AC); the other three sink Link or void him out (func_808340D4).
static bool IsDeadlyFloor(FloorType floorType) {
    return func_808340AC(floorType) >= 0 || func_808340D4(floorType);
}

static bool IsIntegratingGravity(PlayState* play, Player* player) {
    return !(player->stateFlags1 & STATES_WITHOUT_GRAVITY) && !Player_InCsMode(play) &&
           !(player->stateFlags3 & PLAYER_STATE3_FLYING_WITH_HOOKSHOT);
}

// The leaf pins the fall speed itself and halves it for these boots.
static bool IsGliding(void) {
    const char* held = CustomItemRegistry_GetHeldKey();

    return held != NULL && strcmp(held, DEKU_LEAF_KEY) == 0;
}

// Link's gravity is rewritten every frame before it is integrated, so the half is given back afterwards: half of
// g to the velocity and half of g's step to the position leave both exactly where g/2 would have put them.
static void HalveGravity(Player* player) {
    PlayState* play = gPlayState;
    f32 gravity;

    if (play == NULL || !IsWorn()) {
        return;
    }
    gravity = player->actor.gravity;
    if (gravity >= 0.0f || IsGrounded(player) || !IsIntegratingGravity(play, player) || IsGliding()) {
        return;
    }
    if (player->actor.velocity.y <= player->actor.terminalVelocity) {
        return;
    }
    player->actor.velocity.y -= gravity * 0.5f;
    player->actor.world.pos.y -= gravity * 0.5f * (play->state.framerateDivisor * 0.5f);
}

// While Link is out of the water and not rising, its surface is his floor.
static void WalkOnWater(CollisionContext* colCtx, Vec3f* pos, Actor* actor, CollisionPoly** poly, int32_t* bgId,
                        float* floorY) {
    PlayState* play = gPlayState;
    WaterBox* waterBox;
    f32 surface;

    if (play == NULL || !IsWorn() || actor != &GET_PLAYER(play)->actor) {
        return;
    }
    if ((((Player*)actor)->stateFlags1 & PLAYER_STATE1_8000000) || actor->velocity.y > 0.0f) {
        return;
    }
    if (!WaterBox_GetSurface1(play, colCtx, pos->x, pos->z, &surface, &waterBox) || surface <= *floorY) {
        return;
    }
    *floorY = surface;
}

// Skipping the assignment keeps the previous floor type, which these boots never let be a deadly one.
static void StandOnDeadlyFloors(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);
    PlayState* play = gPlayState;

    if (!IsWorn() || play == NULL || !IsGrounded(player) || player->actor.floorPoly == NULL) {
        return;
    }
    if (IsDeadlyFloor(SurfaceType_GetFloorType(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId))) {
        *should = false;
    }
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnBgCheckRaycastFloor" };

static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    S2HCustomEquipDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = ROC_BOOTS_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rRoc's Boots&%wYou fall at half speed, and water, lava and quicksand hold you up.";
    definition.getItemText = "You got the %rRoc's Boots%w!&Boots as light as the great bird's feathers. Every jump "
                             "carries further, and you can walk across water and lava.";
    definition.slot = S2H_EQUIP_SLOT_BOOTS;
    definition.page = 0;
    definition.row = S2H_EQUIP_SLOT_BOOTS;
    definition.column = 3;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.toggles = 1;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, HalveGravity);
    S2H_REGISTER_HOOK(sApi, OnBgCheckRaycastFloor, WalkOnWater);
    sApi->RegisterVB(VB_SET_STATIC_FLOOR_TYPE, StandOnDeadlyFloors);
}
