#ifndef S2H_CUSTOM_EQUIP_REGISTRY_H
#define S2H_CUSTOM_EQUIP_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"

#ifdef __cplusplus
#include <string>
#include <vector>
extern "C" {
#endif

struct PlayState;

typedef enum {
    S2H_EQUIP_SLOT_SWORD,
    S2H_EQUIP_SLOT_SHIELD,
    S2H_EQUIP_SLOT_TUNIC,
    S2H_EQUIP_SLOT_BOOTS,
    S2H_EQUIP_SLOT_UPGRADE,
    S2H_EQUIP_SLOT_MAX,
} S2HEquipSlot;

typedef void (*S2HEquipStateFunc)(const char* key);

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* iconPath;
    const char* namePath;
    const char* pauseText;
    const char* getItemText;
    uint8_t slot;
    uint8_t page;
    uint8_t row;
    uint8_t column;
    uint8_t formMask;
    uint16_t vanillaBase;
    uint8_t toggles;
    S2HEquipStateFunc onEquip;
    S2HEquipStateFunc onUnequip;
    S2HGetItemEntry getItemEntry;
    const char* getItemModelPath;
    const S2HCustomItemRandomizer* randomizer;
    // A PlayerMeleeWeapon for a sword piece that goes on Link's B button; zero keeps the vanilla sword on B.
    uint8_t meleeWeapon;
    // Optional tail field: presentation-only equipment cannot be worn or assigned to buttons.
    uint64_t presentationFlags;
} S2HCustomEquipDefinition;

#define S2H_EQUIP_OWNERSHIP_ONLY UINT64_C(1)

bool CustomEquipRegistry_Register(const S2HCustomEquipDefinition* definition);
const char* CustomEquipRegistry_GetWorn(uint8_t slot);
bool CustomEquipRegistry_IsWorn(const char* key);
bool CustomEquipRegistry_IsOwned(const char* key);
bool CustomEquipRegistry_SetOwned(const char* key, bool owned);
bool CustomEquipRegistry_Wear(const char* key);
void CustomEquipRegistry_TakeOff(uint8_t slot);
uint16_t CustomEquipRegistry_GetVanillaBase(uint8_t slot);
bool CustomEquipRegistry_IsToggleOn(const char* key);
bool CustomEquipRegistry_SetToggle(const char* key, bool on);

#define S2H_EQUIP_PAGE_VANILLA 0xFF

bool KaleidoEquipManager_IsVanillaPageShown(void);
bool KaleidoEquipManager_CycleEquipPage(struct PlayState* play);
bool KaleidoEquipManager_IsCellClaimed(uint8_t row, uint8_t column);
void* KaleidoEquipManager_GetCellIcon(uint8_t row, uint8_t column);
bool KaleidoEquipManager_IsCellFormAllowed(uint8_t row, uint8_t column);
bool KaleidoEquipManager_IsCellEquipped(uint8_t row, uint8_t column);
bool KaleidoEquipManager_EquipCell(struct PlayState* play, uint8_t row, uint8_t column);
void KaleidoEquipManager_Init(void);

// Engine glue of the quest status page, where MM shows the sword and the shield.
void KaleidoEquipManager_Update(struct PlayState* play);
void KaleidoEquipManager_DrawPoints(struct PlayState* play);

uint32_t CustomEquipRegistry_GetCount(void);
const S2HCustomEquipDefinition* CustomEquipRegistry_GetAt(uint32_t index);
const S2HCustomEquipDefinition* CustomEquipRegistry_Find(const char* key);
bool CustomEquipRegistry_IsFormAllowed(const char* key);
void CustomEquipRegistry_Init(void);

#ifdef __cplusplus
}

void CustomEquipRegistry_SetCurrentMod(const std::string& name);
#endif

#endif

#ifdef __cplusplus
#include <string>
#include <vector>
std::vector<std::string> CustomEquipRegistry_ListOwned();
#endif
