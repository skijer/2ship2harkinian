/**
 * Shield of Ikana: Majora's Mask's Mirror Shield. A guard landed right after raising it drains the attacker's soul
 * into Link, and once per area it refuses to let him die.
 */

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define IKANA_SHIELD_KEY "nei.equip.ikana_shield"
#define GUARD_WINDOW 12
#define SOUL_DRAIN 4
#define SOUL_HEAL 8
#define REVIVE_HEALTH (3 * 16)
#define REVIVE_FLASH_FRAMES 20

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__icon_item_static_yar/gItemIconMirrorShieldTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gShieldOfIkanaNameTex";

static const S2HModApi* sApi;

static struct {
    s16 guardTimer;
    bool isGuarding;
    bool isDeathSaveUsed;
    s16 flashTimer;
} sIkana;

static bool IsWorn(void) {
    return CustomEquipRegistry_IsWorn(IKANA_SHIELD_KEY);
}

static void TrackGuard(Player* player) {
    bool isGuarding = (player->stateFlags1 & PLAYER_STATE1_400000) != 0;

    if (isGuarding && !sIkana.isGuarding) {
        sIkana.guardTimer = 0;
    } else if (isGuarding) {
        sIkana.guardTimer++;
    } else {
        sIkana.guardTimer = 0;
    }
    sIkana.isGuarding = isGuarding;
}

static void FadeReviveFlash(PlayState* play) {
    if (sIkana.flashTimer <= 0 || --sIkana.flashTimer > 0) {
        return;
    }
    play->envCtx.fillScreen = false;
    play->envCtx.screenFillColor[3] = 0;
}

static void TickShield(Player* player) {
    PlayState* play = gPlayState;

    if (play == NULL) {
        return;
    }
    FadeReviveFlash(play);
    if (!IsWorn()) {
        return;
    }
    if ((player->stateFlags1 & PLAYER_STATE1_DEAD) || Player_InBlockingCsMode(play, player)) {
        return;
    }
    TrackGuard(player);
}

// Zeroing health is not enough: most enemies only die from their own AC_HIT handling, which the drain bypasses,
// so the kill is driven by hand. Bosses keep their scripted death and need a real last hit.
static void DrainSoul(PlayState* play, Player* player, Actor* attacker) {
    if (attacker->colChkInfo.health > SOUL_DRAIN) {
        attacker->colChkInfo.health -= SOUL_DRAIN;
    } else {
        attacker->colChkInfo.health = 0;
        if (attacker->category != ACTORCAT_BOSS) {
            Enemy_StartFinishingBlow(play, attacker);
            Item_DropCollectibleRandom(play, attacker, &attacker->world.pos, 0xA0);
            Actor_Kill(attacker);
        }
    }
    Audio_PlaySfx_AtPos(&player->actor.world.pos, NA_SE_IT_SHIELD_REFLECT_MG);
    Health_ChangeBy(play, SOUL_HEAL);
}

static void DrainOnGuard(PlayState* play, Player* player, Actor* attacker) {
    if (!IsWorn() || attacker == NULL || attacker == &player->actor) {
        return;
    }
    if (sIkana.isGuarding && sIkana.guardTimer <= GUARD_WINDOW) {
        DrainSoul(play, player, attacker);
    }
}

// Health_ChangeBy reports the loss before anything reads it, so a revive here means the game never sees a death.
static void SaveFromDeath(s16 amount) {
    PlayState* play = gPlayState;

    if (amount >= 0 || play == NULL || !IsWorn() || sIkana.isDeathSaveUsed ||
        gSaveContext.save.saveInfo.playerData.health > 0) {
        return;
    }
    sIkana.isDeathSaveUsed = true;
    gSaveContext.save.saveInfo.playerData.health = REVIVE_HEALTH;
    play->envCtx.fillScreen = true;
    play->envCtx.screenFillColor[0] = 80;
    play->envCtx.screenFillColor[1] = 0;
    play->envCtx.screenFillColor[2] = 120;
    play->envCtx.screenFillColor[3] = 200;
    sIkana.flashTimer = REVIVE_FLASH_FRAMES;
    Audio_PlaySfx_AtPos(&gSfxDefaultPos, NA_SE_EN_PO_LAUGH);
}

static void RenewDeathSave(s16 sceneId, s8 spawnNum) {
    sIkana.isDeathSaveUsed = false;
    sIkana.flashTimer = 0;
}

static void ForgetGuard(const char* key) {
    sIkana.guardTimer = 0;
    sIkana.isGuarding = false;
}

static const char* const sRequiredHooks[] = { "OnPlayerUpdate", "OnPlayerShieldBlocked", "OnPlayerHealthChange",
                                              "OnSceneInit" };

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
    definition.key = IKANA_SHIELD_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rShield of Ikana&%wBlock with %y\xA3%w right as you raise it to drain the attacker's "
                           "soul. Once per area it holds death off.";
    definition.getItemText = "You got the %rShield of Ikana%w!&The mirror of a kingdom of ghosts. It drinks the soul of "
                             "whatever strikes it in time, and once in every place it will not let you fall.";
    definition.slot = S2H_EQUIP_SLOT_SHIELD;
    definition.page = 0;
    definition.row = S2H_EQUIP_SLOT_SHIELD;
    definition.column = 3;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.vanillaBase = PLAYER_SHIELD_MIRROR_SHIELD;
    definition.toggles = 1;
    definition.onUnequip = ForgetGuard;
    if (!CustomEquipRegistry_Register(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickShield);
    S2H_REGISTER_HOOK(sApi, OnPlayerShieldBlocked, DrainOnGuard);
    S2H_REGISTER_HOOK(sApi, OnPlayerHealthChange, SaveFromDeath);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, RenewDeathSave);
}
