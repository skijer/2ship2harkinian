#ifndef UNBOUND_Z64LOGIC_H
#define UNBOUND_Z64LOGIC_H

// A mod item inherits Majora's Mask randomizer logic: while the seed's simulated inventory holds it, it counts
// as a native capability or item. Conditions ask the logic bank, never the save.
#include "2s2h/ModApi/ModApi.h"

typedef struct {
    // The item, or any stage of its progressive chain: a stage also answers for every later one.
    const char* key;
    // Zero, or a native RandoItemId the item needs too, such as the Bow for arrows.
    int32_t requiredItem;
    bool needsMagic;
} Z64LogicSource;

static const S2HModApi* sZ64LogicApi;

static inline bool Z64Logic_Init(const S2HModApi* api) {
    if (!S2H_MOD_API_HAS(api, GrantLogic) || !S2H_MOD_API_HAS(api, GrantLogicCapability) ||
        !S2H_MOD_API_HAS(api, GetRandoItem) || !S2H_MOD_API_HAS(api, LogicCanUseItem) ||
        !S2H_MOD_API_HAS(api, LogicHasItem) || !S2H_MOD_API_HAS(api, LogicHasCapability)) {
        return false;
    }
    sZ64LogicApi = api;
    return true;
}

// An item outside the pool has no rando entry; the simulation starts from the save, so its ownership stands.
static inline bool Z64Logic_HasKey(const char* key) {
    const int32_t item = sZ64LogicApi->GetRandoItem(key);
    if (item > 0) {
        return sZ64LogicApi->LogicCanUseItem(item);
    }
    return sZ64LogicApi->IsCustomItemOwned(key);
}

static inline bool Z64Logic_DoesSourceHold(void* userData) {
    const Z64LogicSource* source = (const Z64LogicSource*)userData;
    if (!Z64Logic_HasKey(source->key)) {
        return false;
    }
    if (source->requiredItem != 0 && !sZ64LogicApi->LogicHasItem(source->requiredItem)) {
        return false;
    }
    return !source->needsMagic || sZ64LogicApi->LogicHasCapability("HasMagic");
}

// Assumes Z64Logic_Init succeeded and the source outlives the mod; capabilities ends with NULL.
static inline void Z64Logic_GrantCapabilities(const Z64LogicSource* source, const char* const* capabilities) {
    for (uint32_t i = 0; capabilities[i] != NULL; i++) {
        sZ64LogicApi->GrantLogicCapability(source->key, capabilities[i], Z64Logic_DoesSourceHold, (void*)source);
    }
}

static inline S2HLogicSelector Z64Logic_SelectAny(void) {
    S2HLogicSelector selector = { 0 };
    selector.structSize = sizeof(selector);
    selector.checkType = S2H_LOGIC_ANY;
    selector.region = S2H_LOGIC_ANY;
    selector.scene = S2H_LOGIC_ANY;
    selector.entranceTo = S2H_LOGIC_ANY;
    selector.logicEvent = S2H_LOGIC_ANY;
    return selector;
}

static inline void Z64Logic_GrantScoped(const Z64LogicSource* source, const S2HLogicGrant* scope) {
    S2HLogicGrant grant = *scope;
    grant.structSize = sizeof(grant);
    grant.owner = source->key;
    grant.condition = Z64Logic_DoesSourceHold;
    grant.userData = (void*)source;
    sZ64LogicApi->GrantLogic(&grant);
}

// Carrying fire from a lit torch to another is where MM's logic asks for a Deku Stick.
static inline void Z64Logic_GrantTorchLighting(const Z64LogicSource* source) {
    S2HLogicSelector torches = Z64Logic_SelectAny();
    torches.capability = "HasFireSourceWithTorch";
    S2HLogicGrant scope = { 0 };
    scope.kind = S2H_LOGIC_GRANT_ITEM;
    scope.value = RI_DEKU_STICK;
    scope.include = &torches;
    scope.includeCount = 1;
    Z64Logic_GrantScoped(source, &scope);
}

// Boulders and snowballs break on Goron punch or pound damage (ObjBombiwa 0x500, ObjSnowball 0x400|0x100);
// MM's logic spells exactly those obstacles as explosives or Goron.
static inline void Z64Logic_GrantBoulderSmashing(const Z64LogicSource* source) {
    S2HLogicSelector obstacles[2] = { Z64Logic_SelectAny(), Z64Logic_SelectAny() };
    obstacles[0].conditionText = "CAN_USE_EXPLOSIVE || CAN_BE_GORON";
    obstacles[1].conditionText = "CAN_BE_GORON || CAN_USE_EXPLOSIVE";
    S2HLogicGrant scope = { 0 };
    scope.kind = S2H_LOGIC_GRANT_CAPABILITY;
    scope.capability = "CanBeGoron";
    scope.include = obstacles;
    scope.includeCount = sizeof(obstacles) / sizeof(obstacles[0]);
    Z64Logic_GrantScoped(source, &scope);
}

#endif
