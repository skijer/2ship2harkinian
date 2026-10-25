/**
 * Magic Cape: owning it halves the price of every spell and makes the Lens of Truth drain at half speed.
 * Majora's Mask ships no cloak mesh to hang from Link's shoulders, so only the get-item model survives.
 */

#include "z64items.h"

#include "align_asset_macro.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"

#define MAGIC_CAPE_KEY "nei.equip.magic_cape"
#define LENS_DRAIN_INTERVAL 80

static const ALIGN_ASSET(2) char sIcon[] = "__OTR__textures/icon_item_custom/gItemIconMagicCapeTex";
static const ALIGN_ASSET(2) char sName[] = "__OTR__textures/item_name_custom/gMagicCapeNameTex";
static const ALIGN_ASSET(2) char sGetItemCapeDL[] = "__OTR__objects/object_nei_magic_cape/gNeiMagicCapeDL";

static const char* const sRequiredHooks[] = { "OnPlayerUpdate",   "OnMagicResolveCost", "ShouldActorInit",
                                              "OnActorInit",       "ShouldActorUpdate",  "OnActorUpdate",
                                              "OnSceneInit" };

static const S2HModRequirements sRequirements = { sizeof(S2HModRequirements), sRequiredHooks,
                                                  ARRAY_COUNT(sRequiredHooks) };

static const S2HModApi* sApi;
static bool sIsSpinAttackCharging;
static s16 sLensTimerSeen;

static bool IsActive(void) {
    return sApi->IsEquipmentOwned(MAGIC_CAPE_KEY) && sApi->IsEquipmentToggleOn(MAGIC_CAPE_KEY);
}

// The spin attack keeps its full price: its charge previews one cost and its release pays another.
static void HalveSpellCost(int16_t* amount) {
    if (IsActive() && !sIsSpinAttackCharging && *amount > 0) {
        *amount /= 2;
    }
}

static void EnterSpinAttackCost(Actor* actor, bool* should) {
    sIsSpinAttackCharging = true;
}

static void LeaveSpinAttackCost(Actor* actor) {
    sIsSpinAttackCharging = false;
}

// The lens pays one point each time its timer runs out; the timer jumping back up is that payment.
static void HalveLensDrain(PlayState* play) {
    s16 timer = play->interfaceCtx.magicConsumptionTimer;

    if (IsActive() && gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS && timer > sLensTimerSeen &&
        timer <= LENS_DRAIN_INTERVAL) {
        timer += LENS_DRAIN_INTERVAL;
        play->interfaceCtx.magicConsumptionTimer = timer;
    }
    sLensTimerSeen = timer;
}

static void TickCape(Player* player) {
    // A skipped update never reaches OnActorUpdate, which would leave every later spell at full price.
    sIsSpinAttackCharging = false;
    if (gPlayState != NULL) {
        HalveLensDrain(gPlayState);
    }
}

static void ForgetSceneState(s16 sceneId, s8 spawnNum) {
    sIsSpinAttackCharging = false;
}

S2H_MOD_EXPORT void ModSetApi(const S2HModApi* api) {
    sApi = api;
}

S2H_MOD_EXPORT const S2HModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

S2H_MOD_EXPORT void ModInit(void) {
    S2HCustomEquipDefinition definition = { 0 };

    definition.structSize = sizeof(definition);
    definition.key = MAGIC_CAPE_KEY;
    definition.iconPath = sIcon;
    definition.namePath = sName;
    definition.pauseText = "%rMagic Cape&%wA toggles the cape. While active, spells cost half their magic.";
    definition.getItemText = "You got the %rMagic Cape%w!&Cloth cut from the King of Evil's own mantle. Magic "
                             "flows through it so freely that every spell costs you half, rounded down.";
    definition.slot = S2H_EQUIP_SLOT_UPGRADE;
    definition.page = 0;
    definition.row = 0;
    definition.column = 0;
    definition.toggles = 1;
    definition.formMask = S2H_CUSTOM_ITEM_FORM_BIT(PLAYER_FORM_HUMAN);
    definition.getItemModelPath = sGetItemCapeDL;
    if (!sApi->RegisterCustomEquipment(&definition)) {
        return;
    }

    S2H_REGISTER_HOOK(sApi, OnPlayerUpdate, TickCape);
    S2H_REGISTER_HOOK(sApi, OnMagicResolveCost, HalveSpellCost);
    S2H_REGISTER_HOOK(sApi, OnSceneInit, ForgetSceneState);
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorInit, ACTOR_EN_M_THUNDER, EnterSpinAttackCost);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorInit, ACTOR_EN_M_THUNDER, LeaveSpinAttackCost);
    S2H_REGISTER_HOOK_FOR_ID(sApi, ShouldActorUpdate, ACTOR_EN_M_THUNDER, EnterSpinAttackCost);
    S2H_REGISTER_HOOK_FOR_ID(sApi, OnActorUpdate, ACTOR_EN_M_THUNDER, LeaveSpinAttackCost);
}
