#include <stdarg.h>
#include <string.h>

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define MITTS_KEY "nei.mogma_mitts"
#define MITTS_MAGIC_PER_DRAIN 1
#define MITTS_DRAIN_INTERVAL 10

static const ALIGN_ASSET(2) char sIconTex[] = "__OTR__textures/icon_item_custom/gItemIconMogmaMittsTex";
static const ALIGN_ASSET(2) char sNameTex[] = "__OTR__textures/item_name_custom/gMogmaMittsNameTex";
static const ALIGN_ASSET(2) char sGiveDL[] = "__OTR__objects/object_nei_mogma_mitts/gMogmaMittsGiveDL";

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnLoadGame" };
static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static bool sIsGripping;
static s16 sDrainTimer;
static s8 sPreviousInvincibility;

static bool IsEquipped(void) {
    for (u8 button = 0; button < S2H_ITEM_BUTTON_COUNT; button++) {
        const char* equipped = sApi->GetEquippedCustomItem(button);
        if (equipped != NULL && strcmp(equipped, MITTS_KEY) == 0) {
            return true;
        }
    }
    return false;
}

static bool HasMagicToGrip(void) {
    return gSaveContext.save.saveInfo.playerData.isMagicAcquired &&
           gSaveContext.save.saveInfo.playerData.magic >= MITTS_MAGIC_PER_DRAIN;
}

static void StartGripping(Player* player) {
    sIsGripping = true;
    sDrainTimer = 0;
    sPreviousInvincibility = player->invincibilityTimer;
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_SY_LOCK_ON);
}

static void StopGripping(void) {
    sIsGripping = false;
}

static void ToggleGrip(PlayState* play, Player* player) {
    if (sIsGripping) {
        StopGripping();
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_SY_CANCEL);
        return;
    }
    if (!HasMagicToGrip()) {
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_SY_ERROR);
        return;
    }
    StartGripping(player);
}

// While gripping, every wall counts as climbable.
static void GrantClimbing(bool* should, va_list args) {
    if (sIsGripping) {
        *should = true;
    }
}

static bool WasJustHurt(Player* player) {
    bool isHurt = player->invincibilityTimer > 0 && sPreviousInvincibility == 0;

    sPreviousInvincibility = player->invincibilityTimer;
    return isHurt;
}

static void DrainMagic(Player* player) {
    if (++sDrainTimer < MITTS_DRAIN_INTERVAL) {
        return;
    }
    sDrainTimer = 0;
    if (!HasMagicToGrip()) {
        StopGripping();
        Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_SY_ERROR);
        return;
    }
    gSaveContext.save.saveInfo.playerData.magic -= MITTS_MAGIC_PER_DRAIN;
}

static void UpdateMitts(Player* player) {
    if (!sIsGripping || player == NULL) {
        return;
    }
    if (!IsEquipped() || WasJustHurt(player)) {
        StopGripping();
        return;
    }
    DrainMagic(player);
}

static void StopGrippingOnLoad(s32 fileNum) {
    StopGripping();
}

// Shuffled once by the randomizer; no vanilla logic needs it.
static const S2HCustomItemRandomizer sRandomizerItem = {
    sizeof(S2HCustomItemRandomizer), 0, S2H_CUSTOM_ITEM_TYPE_ITEM, 0, 1, MITTS_KEY, MITTS_KEY, "",
};

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    if (!S2H_MOD_API_HAS(sApi, RegisterCustomItem)) {
        return;
    }

    S2HCustomItemDefinition mitts = Z64Items_Define(MITTS_KEY, sIconTex, sNameTex);

    Z64Items_SetButtons(&mitts, S2H_CUSTOM_ITEM_C_BUTTON | S2H_CUSTOM_ITEM_DPAD);
    Z64Items_SetPlacement(&mitts, 0, 13, 0);
    Z64Items_SetTextbox(&mitts, "You got the %rMogma Mitts%w!\nDigging claws of the Mogma tribe,\ntough enough to "
                                "bite into stone.\x10Press a %yC button%w to grip. A climber grips %yany wall%w.\x10"
                                "The grip spends %gmagic%w while\nit holds.");
    Z64Items_SetPauseText(&mitts, "%rMogma Mitts\n%wPress a %yC button%w to grip any wall.\nPress again to stop. "
                                  "Drains magic.");
    Z64Items_SetAction(&mitts, ToggleGrip, NULL);
    Z64Items_SetGetItemModel(&mitts, sGiveDL);
    mitts.flags |= S2H_CUSTOM_ITEM_INSTANT;

    Z64Items_SetLogic(&mitts, &sRandomizerItem);
    if (!Z64Items_Register(sApi, &mitts)) {
        return;
    }
    sApi->RegisterVB(VB_BE_CLIMBABLE_SURFACE, GrantClimbing);
    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, UpdateMitts);
    S2H_REGISTER_HOOK(sApi, OnLoadGame, StopGrippingOnLoad);
}
