#ifndef UNBOUND_Z64ITEMS_H
#define UNBOUND_Z64ITEMS_H

#include "2s2h/ModApi/ModApi.h"

// PlayerMeleeWeapon indices of Majora's Mask: the engine takes the blade length and damage of that weapon.
#define Z64ITEMS_MELEE_NONE 0
#define Z64ITEMS_MELEE_KOKIRI_SWORD 1
#define Z64ITEMS_MELEE_RAZOR_SWORD 2
#define Z64ITEMS_MELEE_GILDED_SWORD 3
#define Z64ITEMS_MELEE_GREAT_FAIRY_SWORD 4
#define Z64ITEMS_MELEE_DEKU_STICK 5
#define Z64ITEMS_MELEE_ZORA_BOOMERANG 6

static inline S2HCustomItemDefinition Z64Items_Define(const char* key, const char* iconPath,
                                                      const char* itemNameTexturePath) {
    S2HCustomItemDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = key;
    definition.iconPath = iconPath;
    definition.namePath = itemNameTexturePath;
    return definition;
}

static inline void Z64Items_SetTextbox(S2HCustomItemDefinition* definition, const char* getItemText) {
    definition->getItemText = getItemText;
}

static inline S2HCustomPickupVariant Z64Items_DefinePickup(const char* itemKey, const char* variantKey,
                                                         const char* acquisitionKey) {
    S2HCustomPickupVariant pickup = { 0 };
    pickup.structSize = sizeof(pickup);
    pickup.key = acquisitionKey;
    pickup.itemKey = itemKey;
    pickup.variantKey = variantKey;
    return pickup;
}

static inline bool Z64Items_RegisterPickup(const S2HModApi* api, const S2HCustomPickupVariant* pickup) {
    return S2H_MOD_API_HAS(api, RegisterPickupVariant) && api->RegisterPickupVariant(pickup);
}

static inline void Z64Items_SetPauseText(S2HCustomItemDefinition* definition, const char* pauseText) {
    definition->pauseText = pauseText;
}

static inline bool Z64Items_UpdateText(const S2HModApi* api, const char* key, S2HItemTextKind kind,
                                       const char* text) {
    return S2H_MOD_API_HAS(api, SetCustomItemText) && api->SetCustomItemText(key, kind, text);
}

static inline void Z64Items_SetHeldModel(S2HCustomItemDefinition* definition, const char* displayListPath,
                                         uint8_t modelGroup) {
    definition->heldModelPath = displayListPath;
    definition->modelGroup = modelGroup;
}

static inline void Z64Items_SetGetItemModel(S2HCustomItemDefinition* definition, const char* displayListPath) {
    definition->getItemModelPath = displayListPath;
}

static inline void Z64Items_SetFirstPersonModel(S2HCustomItemDefinition* definition, const char* displayListPath) {
    definition->firstPersonModelPath = displayListPath;
}

static inline void Z64Items_KeepHeld(const S2HModApi* api) {
    if (S2H_MOD_API_HAS(api, KeepCustomItemHeld)) {
        api->KeepCustomItemHeld();
    }
}

static inline void Z64Items_SetMeleeWeapon(S2HCustomItemDefinition* definition, uint8_t vanillaWeapon) {
    definition->meleeWeapon = vanillaWeapon;
}

static inline void Z64Items_SetAmmo(S2HCustomItemDefinition* definition, S2HCustomItemAmmoFunc getAmmo) {
    definition->getAmmo = getAmmo;
}

static inline void Z64Items_SetAction(S2HCustomItemDefinition* definition, S2HCustomItemInitFunc init,
                                      S2HCustomItemUpdateFunc upperAction) {
    definition->init = init;
    definition->update = upperAction;
}

static inline void Z64Items_SetCanUse(S2HCustomItemDefinition* definition, S2HCustomItemCanUseFunc canUse) {
    definition->canUse = canUse;
}

static inline void Z64Items_SetHeldCallbacks(S2HCustomItemDefinition* definition, S2HCustomItemPlayerFunc use,
                                             S2HCustomItemPlayerFunc putAway, S2HCustomItemPlayerFunc drawHeld) {
    definition->use = use;
    definition->putAway = putAway;
    definition->drawHeld = drawHeld;
}

static inline void Z64Items_SetButtons(S2HCustomItemDefinition* definition, uint32_t buttonFlags) {
    definition->flags |= S2H_CUSTOM_ITEM_EQUIPPABLE | buttonFlags;
}

// Legacy mask of Majora's Mask's own forms, one bit per S2H_CUSTOM_ITEM_FORM_BIT; Ship has no counterpart.
static inline void Z64Items_SetForms(S2HCustomItemDefinition* definition, uint8_t formMask) {
    definition->formMask = formMask;
}

static inline void Z64Items_SetAge(S2HCustomItemDefinition* definition, S2HCustomItemAge age) {
    definition->ageRequirement = (uint8_t)age;
}

// The game copies the keys when the item registers; until then the array must stay alive.
static inline void Z64Items_SetAllowedForms(S2HCustomItemDefinition* definition, const char* const* formKeys,
                                            uint32_t formKeyCount) {
    definition->allowedForms = formKeys;
    definition->allowedFormCount = formKeyCount;
}

static inline void Z64Items_SetEquipAge(S2HCustomEquipDefinition* definition, S2HCustomItemAge age) {
    definition->ageRequirement = (uint8_t)age;
}

static inline void Z64Items_SetEquipAllowedForms(S2HCustomEquipDefinition* definition, const char* const* formKeys,
                                                 uint32_t formKeyCount) {
    definition->allowedForms = formKeys;
    definition->allowedFormCount = formKeyCount;
}

static inline void Z64Items_SetReplaces(S2HCustomItemDefinition* definition, uint16_t vanillaItem) {
    definition->flags |= S2H_CUSTOM_ITEM_REPLACES_VANILLA;
    definition->replacesItem = vanillaItem;
}

static inline void Z64Items_SetVanillaMode(S2HCustomItemDefinition* definition, S2HVanillaItemMode mode,
                                           int32_t randoItem) {
    definition->vanillaMode = (uint8_t)mode;
    definition->randoItem = randoItem;
}

static inline void Z64Items_SetPlacement(S2HCustomItemDefinition* definition, uint8_t page, uint8_t slot,
                                         int32_t wheelPriority) {
    definition->preferredPage = page;
    definition->preferredSlot = slot;
    definition->priority = wheelPriority;
}

static inline void Z64Items_SetLogic(S2HCustomItemDefinition* definition, const S2HCustomItemRandomizer* logic) {
    definition->randomizer = logic;
}

static inline bool Z64Items_Register(const S2HModApi* api, const S2HCustomItemDefinition* definition) {
    return S2H_MOD_API_HAS(api, RegisterCustomItem) && api->RegisterCustomItem(definition);
}

static inline const char* Z64Items_Texture(const S2HModApi* api, const char* key, S2HItemIconSurface surface) {
    return S2H_MOD_API_HAS(api, ResolveCustomItemTexture) ? api->ResolveCustomItemTexture(key, surface) : NULL;
}

#endif
