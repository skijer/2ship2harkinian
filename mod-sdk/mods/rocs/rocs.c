#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define ROCS_FEATHER_KEY "nei.rocs_feather"
#define ROCS_CAPE_KEY "nei.rocs_cape"
#define ROCS_JUMP_VELOCITY 11.0f
#define ROCS_WATER_JUMP_VELOCITY 5.5f

#define ROCS_BLOCKING_STATES                                                                                   \
    (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_4 | PLAYER_STATE1_2000 | PLAYER_STATE1_4000 | \
     PLAYER_STATE1_200000 | PLAYER_STATE1_800000 | PLAYER_STATE1_CARRYING_ACTOR)

static const ALIGN_ASSET(2) char sFeatherIconTex[] = "__OTR__textures/icon_item_custom/gItemIconRocsFeatherTex";
static const ALIGN_ASSET(2) char sFeatherNameTex[] = "__OTR__textures/item_name_custom/gRocsFeatherNameTex";
static const ALIGN_ASSET(2) char sCapeIconTex[] = "__OTR__textures/icon_item_custom/gItemIconRocsCapeTex";
static const ALIGN_ASSET(2) char sCapeNameTex[] = "__OTR__textures/item_name_custom/gRocsCapeNameTex";
static const ALIGN_ASSET(2) char sFeatherDL[] = "__OTR__objects/object_nei_rocs_feather/rocs_feather_dl";
static const ALIGN_ASSET(2) char sCapeDL[] = "__OTR__objects/object_nei_rocs_cape/rocs_cape_mesh_dl";
static const ALIGN_ASSET(2) char sJumpAnim[] = "__OTR__objects/object_nei_rocs/gPlayerAnim_nei_rocs_jump";
static const ALIGN_ASSET(2) char sAirJumpAnim[] = "__OTR__objects/object_nei_rocs/gPlayerAnim_nei_rocs_front_flip";

// Indexed by Player.heldItemButton; the D-pad starts at 5 and slot 4 is unused.
static const u16 sItemButtons[S2H_ITEM_BUTTON_COUNT] = { BTN_B,      BTN_CLEFT,  BTN_CDOWN, BTN_CRIGHT, 0,
                                                         BTN_DRIGHT, BTN_DLEFT,  BTN_DDOWN, BTN_DUP };

static const char* const sRequiredHooks[] = { "OnPlayerUpdate" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static bool sHasDoubleJumped;
static u32 sGroundJumpFrame;

void func_80834D50(PlayState* play, Player* player, PlayerAnimationHeader* anim, f32 speed, u16 sfxId);

static bool IsOwned(const char* key) {
    return sApi->IsCustomItemOwned(key);
}

static bool IsEquippedOn(u8 button, const char* key) {
    const char* equipped = sApi->GetEquippedCustomItem(button);
    return equipped != NULL && strcmp(equipped, key) == 0;
}

static bool IsAimingFirstPerson(Player* player) {
    return player->unk_AA5 >= PLAYER_UNKAA5_1 && player->unk_AA5 <= PLAYER_UNKAA5_3;
}

static bool IsPlayerBlocked(PlayState* play, Player* player) {
    return (player->stateFlags1 & ROCS_BLOCKING_STATES) != 0 || Player_InCsMode(play) || IsAimingFirstPerson(player);
}

static void SpawnSparkles(Player* player, PlayState* play) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 255, 255, 200, 255 };
    Color_RGBA8 env = { 200, 200, 100, 0 };

    for (s32 i = 0; i < 5; i++) {
        s16 angle = (s16)(Rand_ZeroOne() * 0xFFFF);
        f32 distance = 5.0f + Rand_ZeroOne() * 5.0f;
        Vec3f pos = { player->actor.world.pos.x + Math_SinS(angle) * distance, player->actor.world.pos.y + 5.0f,
                      player->actor.world.pos.z + Math_CosS(angle) * distance };

        EffectSsKirakira_SpawnSmall(play, &pos, &zero, &zero, &prim, &env);
    }
}

static void SpawnDoubleJumpShockwave(Player* player, PlayState* play) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f pos = { player->actor.world.pos.x, player->actor.floorHeight + 2.0f, player->actor.world.pos.z };

    EffectSsBlast_SpawnWhiteCustomScale(play, &pos, &zero, &zero, 60, 150, 8);
}

static void KeepLedgesGrabbable(Player* player) {
    player->stateFlags2 &= ~PLAYER_STATE2_80000;
}

static void Jump(Player* player, PlayState* play, const char* animation) {
    func_80834D50(play, player, (PlayerAnimationHeader*)animation, ROCS_JUMP_VELOCITY, NA_SE_VO_LI_AUTO_JUMP);
    KeepLedgesGrabbable(player);
    SpawnSparkles(player, play);
}

static bool CanJumpFromGround(Player* player, PlayState* play) {
    // A transformation which allows Roc owns its form-specific animation and
    // movement. This item mod supplies vanilla Link's behavior only.
    return sApi->GetActiveForm() == NULL && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
           !IsPlayerBlocked(play, player);
}

static void JumpFromGround(PlayState* play, Player* player) {
    sHasDoubleJumped = false;
    sGroundJumpFrame = play->state.frames;
    Jump(player, play, sJumpAnim);
}

static s32 FindEquippedTier(const char** key) {
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        if (IsEquippedOn(button, ROCS_CAPE_KEY)) {
            *key = ROCS_CAPE_KEY;
            return button;
        }
        if (IsEquippedOn(button, ROCS_FEATHER_KEY)) {
            *key = ROCS_FEATHER_KEY;
            return button;
        }
    }
    return -1;
}

static void JumpFromAirOrWater(Player* player, PlayState* play) {
    const char* tier = NULL;
    s32 button = FindEquippedTier(&tier);

    if (button < 0 || !(play->state.input[0].press.button & sItemButtons[button]) ||
        play->state.frames == sGroundJumpFrame || IsPlayerBlocked(play, player)) {
        return;
    }
    if (player->stateFlags1 & PLAYER_STATE1_8000000) {
        player->actor.velocity.y = ROCS_WATER_JUMP_VELOCITY;
        Player_PlaySfx(player, NA_SE_PL_SKIP);
        SpawnSparkles(player, play);
        return;
    }
    if (strcmp(tier, ROCS_CAPE_KEY) != 0 || sHasDoubleJumped) {
        return;
    }
    sHasDoubleJumped = true;
    Jump(player, play, sAirJumpAnim);
    SpawnDoubleJumpShockwave(player, play);
}

static void UpgradeFeatherToCape(void) {
    if (!IsOwned(ROCS_CAPE_KEY) || !IsOwned(ROCS_FEATHER_KEY)) {
        return;
    }
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        if (IsEquippedOn(button, ROCS_FEATHER_KEY)) {
            sApi->EquipCustomItem(button, ROCS_CAPE_KEY);
        }
    }
    sApi->SetCustomItemOwned(ROCS_FEATHER_KEY, false);
}

static void UpdateRocs(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL) {
        return;
    }
    UpgradeFeatherToCape();
    if (sApi->GetActiveForm() != NULL || (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        sHasDoubleJumped = false;
        return;
    }
    JumpFromAirOrWater(player, play);
}

static bool RegisterTier(S2HCustomItemDefinition* tier, const char* getItemModel, int32_t wheelPriority) {
    Z64Items_SetButtons(tier, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetForms(tier, S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN));
    Z64Items_SetPlacement(tier, 0, 0, wheelPriority);
    Z64Items_SetCanUse(tier, CanJumpFromGround);
    Z64Items_SetAction(tier, JumpFromGround, NULL);
    Z64Items_SetGetItemModel(tier, getItemModel);
    tier->flags |= S2H_CUSTOM_ITEM_INSTANT;
    return Z64Items_Register(sApi, tier);
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, SetCustomItemOwned) || !S2H_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }

    S2HCustomItemDefinition feather = Z64Items_Define(ROCS_FEATHER_KEY, sFeatherIconTex, sFeatherNameTex);
    Z64Items_SetTextbox(&feather, "You got %rRoc's Feather%w!&A plume of the giant bird Roc,&light as the wind "
                                  "itself.^"
                                  "Press %y\xA1%w to leap higher than&any run-up could carry you,&or to hop up out of "
                                  "the %bwater%w.");
    Z64Items_SetPauseText(&feather, "%rRoc's Feather&%wPress %y\xA1%w to jump, or to hop up&out of the water.");

    S2HCustomItemDefinition cape = Z64Items_Define(ROCS_CAPE_KEY, sCapeIconTex, sCapeNameTex);
    Z64Items_SetTextbox(&cape, "You got %rRoc's Cape%w!&Roc's feathers woven whole, so&the wind carries you twice.^"
                               "Press %y\xA1%w to jump, then %y\xA1%w again&in midair to ride the wind a&%gsecond "
                               "time%w, even as you fall.");
    Z64Items_SetPauseText(&cape, "%rRoc's Cape&%wPress %y\xA1%w to jump, again in midair&for a second jump.");

    if (!RegisterTier(&feather, sFeatherDL, 0) || !RegisterTier(&cape, sCapeDL, 1)) {
        return;
    }
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateRocs);
}
