#ifndef S2H_CUSTOM_ITEM_REGISTRY_H
#define S2H_CUSTOM_ITEM_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "2s2h/ModApi/VanillaItems/VanillaItems.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Actor;
struct Player;
struct PlayState;

typedef enum {
    S2H_CUSTOM_ITEM_RANDO_ADVANCEMENT = 1 << 0,
    S2H_CUSTOM_ITEM_RANDO_PROGRESSIVE = 1 << 1,
} S2HCustomItemRandomizerFlags;

typedef enum {
    S2H_CUSTOM_ITEM_EQUIPPABLE = 1 << 0,
    S2H_CUSTOM_ITEM_INSTANT = 1 << 1,
    S2H_CUSTOM_ITEM_C_BUTTON = 1 << 2,
    S2H_CUSTOM_ITEM_DPAD = 1 << 3,
    S2H_CUSTOM_ITEM_B_BUTTON = 1 << 4,
    S2H_CUSTOM_ITEM_REPLACES_VANILLA = 1 << 5,
    S2H_CUSTOM_ITEM_WEARABLE = 1 << 6,
} S2HCustomItemFlags;

// One bit per PlayerTransformation; zero means every form may use the item.
#define S2H_CUSTOM_ITEM_FORM_BIT(form) (1u << (form))

typedef enum {
    S2H_ITEM_ICON_TEXTBOX = 1 << 0,
    S2H_ITEM_ICON_SAVE_EDITOR = 1 << 1,
    S2H_ITEM_ICON_INVENTORY = 1 << 2,
    S2H_ITEM_ICON_B_BUTTON = 1 << 3,
    S2H_ITEM_ICON_C_BUTTON = 1 << 4,
    S2H_ITEM_ICON_DPAD = 1 << 5,
    S2H_ITEM_NAME_TEXTURE = 1 << 6,
} S2HItemIconSurface;

typedef enum {
    S2H_ITEM_HIDE_FROM_SAVE_EDITOR = 1 << 0,
    S2H_ITEM_HIDE_FROM_KALEIDO = 1 << 1,
} S2HItemPresentationFlags;

typedef enum {
    S2H_ITEM_TEXT_GET_ITEM = 0,
    S2H_ITEM_TEXT_PAUSE = 1,
} S2HItemTextKind;

// Held buttons as Player.heldItemButton numbers them: B, C-Left, C-Down, C-Right, then the D-pad after EQUIP_SLOT_MAX.
#define S2H_ITEM_BUTTON_B 0
#define S2H_ITEM_BUTTON_C_FIRST 1
#define S2H_ITEM_BUTTON_C_LAST 3
#define S2H_ITEM_BUTTON_DPAD_FIRST 5
#define S2H_ITEM_BUTTON_DPAD_LAST 8
#define S2H_ITEM_BUTTON_COUNT 9

typedef bool (*S2HCustomItemCanUseFunc)(struct Player* player, struct PlayState* play);
typedef void (*S2HCustomItemInitFunc)(struct PlayState* play, struct Player* player);
typedef int32_t (*S2HCustomItemUpdateFunc)(struct Player* player, struct PlayState* play);
typedef void (*S2HCustomItemPlayerFunc)(struct Player* player, struct PlayState* play);
typedef void (*S2HCustomItemStateFunc)(const char* key);
typedef int32_t (*S2HCustomItemAmmoFunc)(const char* key);

typedef enum {
    S2H_CUSTOM_ITEM_TYPE_ITEM = 0,
    S2H_CUSTOM_ITEM_TYPE_EQUIP = 1,
    S2H_CUSTOM_ITEM_TYPE_SONG = 11,
    S2H_CUSTOM_ITEM_TYPE_SHOP = 12,
} S2HCustomItemPoolType;

typedef struct S2HGetItemEntry S2HGetItemEntry;
typedef void (*S2HGetItemDrawFunc)(struct PlayState* play, S2HGetItemEntry* entry);

// Ship's GetItemEntry layout. MM reads textId, gid (a GID_* draw id) and drawFunc; the rest is the mod's own data.
struct S2HGetItemEntry {
    uint16_t itemId;
    uint16_t field;
    int16_t gi;
    uint16_t textId;
    uint16_t objectId;
    uint16_t modIndex;
    uint16_t tableId;
    int16_t getItemId;
    uint16_t gid;
    uint16_t collectable;
    int32_t getItemFrom;
    int32_t getItemCategory;
    uint16_t drawItemId;
    uint16_t drawModIndex;
    S2HGetItemDrawFunc drawFunc;
};

typedef struct {
    uint32_t structSize;
    uint32_t flags;
    uint32_t type;
    uint16_t price;
    uint16_t poolCount;
    const char* logicKey;
    const char* hintKey;
    const char* progressionGroup;
    const char* poolCountOption;
    // Optional seed pool selector; absent means this reward is always eligible.
    const char* poolModeOption;
    int32_t poolModeValue;
    bool poolModeInvert;
    // Optional native pool contributions (RandoItemId). Each copy chooses one source with the seed RNG.
    // An empty list preserves the custom item's own randomizer identity.
    const int32_t* poolSources;
    uint16_t poolSourceCount;
} S2HCustomItemRandomizer;

typedef void (*S2HPickupVariantReceiveFunc)(const char* itemKey, const char* variantKey);
typedef struct {
    uint32_t structSize;
    const char* key; // Stable acquisition identity, written into the seed.
    const char* itemKey; // The single equipable inventory item.
    const char* variantKey;
    const char* iconPath;
    const char* getItemModelPath;
    const char* getItemText;
    float getItemScale;
    S2HPickupVariantReceiveFunc onReceive;
    const S2HCustomItemRandomizer* randomizer;
    // Optional tail: render an enemy-soul flame instead of the item model.
    bool drawSoulFlame;
    uint8_t flameRed;
    uint8_t flameGreen;
    uint8_t flameBlue;
} S2HCustomPickupVariant;

#define S2H_CUSTOM_PICKUP_VARIANT_MIN_SIZE offsetof(S2HCustomPickupVariant, drawSoulFlame)

// How other players see this item on you, drawn while you hold it or wear it (SetCustomItemWorn).
typedef struct {
    const char* modelPath;
    uint8_t limb;      // PLAYER_LIMB_*; 0 = left hand when held, head when worn
    float rotation[3]; // degrees, applied Y, then X, then Z
    float offset[3];   // along the rotated axes, in world units
    float scale;       // 0 = draw the model in the limb's own space, untransformed
} S2HCustomItemAppearance;

bool CustomItemRegistry_RegisterPickupVariant(const S2HCustomPickupVariant* variant);
const S2HCustomPickupVariant* CustomItemRegistry_GetPickupVariant(const char* key);
const char* CustomItemRegistry_FindPickupKey(const char* itemKey, const char* variantKey);
void CustomItemRegistry_DrawAcquisition(const char* key, struct PlayState* play);
void CustomItemRegistry_PrepareAcquisitionMessage(const char* key);

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* iconPath;
    const char* namePath;
    const char* pauseText;
    uint32_t flags;
    int32_t priority;
    uint8_t preferredPage;
    uint8_t preferredSlot;
    uint8_t formMask;
    uint8_t modelGroup;
    S2HGetItemEntry getItemEntry;
    S2HCustomItemCanUseFunc canUse;
    S2HCustomItemInitFunc init;
    S2HCustomItemUpdateFunc update;
    S2HCustomItemPlayerFunc drawHeld;
    S2HCustomItemStateFunc onAcquire;
    S2HCustomItemStateFunc onRemove;
    const S2HCustomItemRandomizer* randomizer;
    uint32_t disabledIconSurfaces;
    uint32_t presentationFlags;
    uint16_t replacesItem;
    const char* getItemText;
    const char* heldModelPath;
    const char* getItemModelPath;
    S2HCustomItemPlayerFunc use;
    S2HCustomItemPlayerFunc putAway;
    uint8_t vanillaMode;
    int32_t randoItem;
    const char* firstPersonModelPath;
    S2HCustomItemStateFunc onReceive;
    uint8_t meleeWeapon;
    S2HCustomItemAmmoFunc getAmmo;
    // Optional pause action/state for passive effects; ownership and button use are independent.
    S2HCustomItemStateFunc onPauseToggle;
    bool (*isPauseActive)(const char* key);
    // Multiplier for the world and overhead acquisition model; zero preserves the default scale.
    float getItemScale;
    S2HCustomItemAppearance appearance;
    // Optional UTF-8 label for messages; omitted labels fall back to the registered key.
    const char* displayName;
} S2HCustomItemDefinition;

#define S2H_CUSTOM_ITEM_RANDOMIZER_MIN_SIZE \
    (offsetof(S2HCustomItemRandomizer, progressionGroup) + sizeof(((S2HCustomItemRandomizer*)0)->progressionGroup))
#define S2H_CUSTOM_ITEM_DEFINITION_MIN_SIZE \
    (offsetof(S2HCustomItemDefinition, randomizer) + sizeof(((S2HCustomItemDefinition*)0)->randomizer))
#define S2H_CUSTOM_ITEM_REPLACEMENT_MIN_SIZE \
    (offsetof(S2HCustomItemDefinition, replacesItem) + sizeof(((S2HCustomItemDefinition*)0)->replacesItem))

static inline bool CustomItemRegistry_IsVanillaUpgrade(const S2HCustomItemDefinition* definition) {
    return definition != NULL && (definition->flags & S2H_CUSTOM_ITEM_REPLACES_VANILLA) != 0 &&
           definition->vanillaMode == S2H_VANILLA_ITEM_UPGRADE;
}

bool CustomItemRegistry_Register(const S2HCustomItemDefinition* definition);
const S2HCustomItemDefinition* CustomItemRegistry_FindReplacement(uint16_t vanillaItem);
bool CustomItemRegistry_IsFormAllowed(const char* key);
bool CustomItemRegistry_IsPauseItemFormAllowed(void);
const char* CustomItemRegistry_ResolveTexture(const char* key, S2HItemIconSurface surface);
bool CustomItemRegistry_IsEditorVisible(const char* key);

typedef struct {
    uint16_t vanillaItem;
    const char* customKey;
} S2HItemPoolEntry;

#define S2H_ITEM_PAGE_VANILLA 0xFF
#define S2H_ITEM_PAGE_QUEST_VANILLA 0xE0
#define S2H_ITEM_PAGE_EQUIPMENT_FIRST 0x100

static inline bool CustomItemRegistry_IsQuestPage(uint16_t page) {
    return page >= S2H_ITEM_PAGE_QUEST_VANILLA && page < S2H_ITEM_PAGE_VANILLA;
}

typedef struct {
    uint16_t page;
    uint8_t slot;
} S2HCustomItemPlacement;

bool KaleidoItemManager_RegisterPool(const char* key, const S2HItemPoolEntry* entries, uint32_t count);
uint32_t KaleidoItemManager_GetPoolCount(void);
const char* KaleidoItemManager_GetPoolKey(uint32_t index);
uint32_t KaleidoItemManager_GetPoolSize(const char* key);
bool KaleidoItemManager_GetPoolEntry(const char* key, uint32_t index, S2HItemPoolEntry* entry);
bool KaleidoItemManager_GetPlacement(const char* key, S2HCustomItemPlacement* placement);
bool KaleidoItemManager_SetPlacement(const char* key, const S2HCustomItemPlacement* placement);
void KaleidoItemManager_ResetPlacement(const char* key);
uint32_t KaleidoItemManager_GetSlotItemCount(uint16_t page, uint8_t slot);
const char* KaleidoItemManager_GetSlotItemAt(uint16_t page, uint8_t slot, uint32_t index);
bool KaleidoItemManager_IsVanillaPageShown(void);
uint16_t KaleidoItemManager_GetSlotItem(uint8_t slot);
uint8_t KaleidoItemManager_GetVanillaSlot(uint8_t cell);

bool KaleidoQuestManager_IsVanillaPageShown(void);
bool KaleidoQuestManager_IsPointClaimed(uint8_t point);
bool KaleidoQuestManager_CycleQuestPage(struct PlayState* play);
bool KaleidoQuestManager_EquipPoint(struct PlayState* play);
void KaleidoQuestManager_DrawPoints(struct PlayState* play);
void KaleidoQuestManager_Init(void);
void* KaleidoItemManager_GetSlotIcon(uint8_t slot);
bool KaleidoItemManager_IsSlotFormAllowed(uint8_t slot);
bool KaleidoItemManager_CycleItemPage(struct PlayState* play);
void KaleidoItemManager_HandleWheel(struct PlayState* play);
void KaleidoItemManager_DrawWheels(struct PlayState* play);

const S2HCustomItemDefinition* CustomItemRegistry_Find(const char* key);
const S2HGetItemEntry* CustomItemRegistry_FindGetItemEntry(const char* key);
uint16_t CustomItemRegistry_GetRuntimeId(const char* key);
const S2HCustomItemDefinition* CustomItemRegistry_FindByRuntimeId(uint16_t runtimeId);
uint32_t CustomItemRegistry_GetCount(void);
const S2HCustomItemDefinition* CustomItemRegistry_GetAt(uint32_t index);
bool CustomItemRegistry_SetOwned(const char* key, bool owned);
void CustomItemRegistry_ReceiveGetItem(const char* key);
bool CustomItemRegistry_IsOwned(const char* key);
bool CustomItemRegistry_SetWorn(const char* key, bool worn);
bool CustomItemRegistry_IsWorn(const char* key);
bool CustomItemRegistry_Equip(uint8_t button, const char* key);
void CustomItemRegistry_Unequip(uint8_t button);
void CustomItemRegistry_CopyEquippedKey(uint8_t fromButton, uint8_t toButton);
const char* CustomItemRegistry_GetEquippedKey(uint8_t button);
uint16_t CustomItemRegistry_GetEquippedRuntimeId(uint8_t button);
// Ship also takes the pressed vanilla item for replacements; MM's Player_UseItem only routes ITEM_CUSTOM here.
bool CustomItemRegistry_PressButtonItem(struct PlayState* play, struct Player* player, uint8_t button);
bool CustomItemRegistry_SelectPauseItem(const char* key);
const S2HCustomItemDefinition* CustomItemRegistry_GetPauseItem(void);
void CustomItemRegistry_PreparePauseMessage(void);
const S2HCustomItemDefinition* CustomItemRegistry_GetMessageItem(void);
const S2HCustomItemDefinition* CustomItemRegistry_GetActive(void);
const char* CustomItemRegistry_GetHeldKey(void);
bool CustomItemRegistry_SetIconPath(const char* key, const char* path);
bool CustomItemRegistry_SetText(const char* key, S2HItemTextKind kind, const char* text);
int32_t CustomItemRegistry_GetAmmo(const char* key);
uint8_t CustomItemRegistry_GetActiveMeleeWeapon(void);
uint8_t CustomItemRegistry_GetHeldMeleeWeapon(void);
void CustomItemRegistry_InitHeld(struct PlayState* play, struct Player* player);
int32_t CustomItemRegistry_UpdateHeld(struct Player* player, struct PlayState* play);
void CustomItemRegistry_DrawHeld(struct Player* player, struct PlayState* play);
const char* CustomItemRegistry_GetFirstPersonModel(void);
void CustomItemRegistry_HoldHeld(void);
bool CustomItemRegistry_BindActor(struct Actor* actor, const char* key);
void CustomItemRegistry_UnbindActor(struct Actor* actor);
const char* CustomItemRegistry_GetActorItem(struct Actor* actor);
bool CustomItemRegistry_Give(struct PlayState* play, const char* key);
bool CustomItemRegistry_ShowTextbox(struct PlayState* play, const char* text, bool autoFormat);

#ifdef __cplusplus
}

#include <string>
#include <vector>

void CustomItemRegistry_Init();
void CustomItemRegistry_RegisterAppearanceDrawing();
std::string CustomItemRegistry_GetOwner(const char* key);
std::vector<std::string> CustomItemRegistry_ListWorn();
std::vector<std::string> CustomItemRegistry_ListOwned();
void CustomItemRegistry_SetDrawingPlayer(Player* player);
Player* CustomItemRegistry_GetDrawingPlayer();
void KaleidoItemManager_Init();
void CustomItemRegistry_SetCurrentMod(const std::string& name);
void CustomItemRegistry_ClearCurrentMod();
#endif

#endif
