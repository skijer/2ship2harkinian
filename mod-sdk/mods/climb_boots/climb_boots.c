/**
 * Climb Boots: full traction. Ice keeps no momentum, a steep slope neither slides Link down nor slows him
 * climbing it, and A still jumps from one.
 */

#include "z64items.h"

#include "align_asset_macro.h"
#include "attributes.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define CLIMB_BOOTS_KEY "nei.equip.climb_boots"

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconClimbBootsTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gClimbBootsNameTex";

// Imported by address: a plain declaration would compare actionFunc against the DLL's own thunk.
extern HOST_DATA void Player_Action_SlideOnSlope(Player* player, PlayState* play);
bool Player_IsZTargeting(Player* player);
bool Player_CanUpdateItems(Player* player);
void func_808369F4(Player* player, PlayState* play);
void func_80836B3C(PlayState* play, Player* player, f32 arg2);
s32 func_80839A84(PlayState* play, Player* player);
void func_80839860(Player* player, PlayState* play, s32 controlStickDirection);
void func_808395F0(PlayState* play, Player* player, PlayerMeleeWeaponAnimation meleeWeaponAnim, f32 linearVelocity,
                   f32 yVelocity);

static const S2HModApi* sApi;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(CLIMB_BOOTS_KEY);
}

static bool IsGrounded(Player* player) {
    return (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
}

static bool IsOnSteepSlope(Player* player, PlayState* play) {
    CollisionPoly* floor = player->actor.floorPoly;

    return floor != NULL &&
           SurfaceType_GetFloorEffect(&play->colCtx, floor, player->actor.floorBgId) == FLOOR_EFFECT_1;
}

// Skipping the assignment keeps the previous, non-ice floor type: that is what stops the skid.
static void KeepGripOnIce(bool* should, va_list args) {
    Player* player = va_arg(args, Player*);
    PlayState* play = gPlayState;

    if (!IsWorn() || play == NULL || !IsGrounded(player) || player->actor.floorPoly == NULL) {
        return;
    }
    if (SurfaceType_GetFloorType(&play->colCtx, player->actor.floorPoly, player->actor.floorBgId) == FLOOR_TYPE_5) {
        *should = false;
    }
}

// The slide starts inside the scene collision; ending it here, after the whole update, costs no frame. Walking
// downhill restarts it every frame, so the walk's own speed and heading are handed back each time.
static void KeepFootingOnSlopes(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL || !IsWorn() || !IsGrounded(player)) {
        return;
    }
    if (player->actionFunc == Player_Action_SlideOnSlope) {
        f32 speed = player->speedXZ;
        s16 heading = player->yaw;

        func_808369F4(player, play);
        player->speedXZ = speed;
        player->yaw = heading;
    }
    if (IsOnSteepSlope(player, play)) {
        player->pushedSpeed = 0.0f;
    }
}

// Player_ActionHandler_10 without its steep-slope gate, which is the only thing standing between a slope
// and the hop, backflip or jump slash.
static bool StartSlopeJump(Player* player, PlayState* play) {
    s32 stickDirection = player->controlStickDirections[player->controlStickDataIndex];

    if (stickDirection > PLAYER_STICK_DIR_FORWARD) {
        func_80839860(player, play, stickDirection);
        return true;
    }
    if (!Player_IsZTargeting(player)) {
        return false;
    }
    if (!(player->stateFlags1 & PLAYER_STATE1_8000000) && Player_GetMeleeWeaponHeld(player) != PLAYER_MELEEWEAPON_NONE &&
        Player_CanUpdateItems(player) && player->transformation != PLAYER_FORM_GORON) {
        func_808395F0(play, player, PLAYER_MWA_JUMPSLASH_START, 5.0f, 5.0f);
    } else if (!func_80839A84(play, player)) {
        func_80836B3C(play, player, 0.0f);
    }
    return true;
}

static void JumpFromSteepSlope(PlayState* play, Player* player, s32 action, bool* consumed, bool* startedAction) {
    if (*consumed || !IsWorn() || !IsOnSteepSlope(player, play)) {
        return;
    }
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) ||
        play->roomCtx.curRoom.type == ROOM_TYPE_INDOORS) {
        return;
    }
    *consumed = true;
    *startedAction = StartSlopeJump(player, play);
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayerActionHandler" };

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
    definition.key = CLIMB_BOOTS_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rClimb Boots&%wIce and steep slopes no longer move you, and %y\x9F%w jumps even from "
                           "a slope.";
    definition.getItemText = "You got the %rClimb Boots%w!&Iron soles with a miner's grip. Wear them and neither "
                             "ice nor a steep slope will carry you away.";
    definition.slot = S2H_EQUIP_SLOT_BOOTS;
    definition.page = 0;
    definition.row = S2H_EQUIP_SLOT_BOOTS;
    definition.column = 2;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.toggles = 1;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, KeepFootingOnSlopes);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnPlayerActionHandler, SOH_PLAYER_ACTION_ZTARGET_A, JumpFromSteepSlope);
    sApi->RegisterVB(VB_SET_STATIC_FLOOR_TYPE, KeepGripOnIce);
}
