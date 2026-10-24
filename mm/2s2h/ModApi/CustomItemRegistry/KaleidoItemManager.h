#ifndef S2H_KALEIDO_ITEM_MANAGER_H
#define S2H_KALEIDO_ITEM_MANAGER_H

#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/Layout/ModLayout.h"

#ifdef __cplusplus
extern "C" {
#endif

uint16_t KaleidoQuestManager_GetShownPage(void);
void** KaleidoQuestManager_GetBackground(void** vanilla);

// Engine glue of the pause screen. Ship's kaleido files call the Kaleido*Manager_* API of CustomItemRegistry.h
// directly; MM's pages are patched with these single dispatch points instead.

// ITEM_CUSTOM when a custom item is shown in the cell, ITEM_NONE when a custom page hides the vanilla one,
// otherwise the vanilla item handed in.
uint16_t KaleidoItemManager_ResolveSlotItem(uint8_t slot, uint16_t vanillaItem);
void KaleidoItemManager_ApplyGeometry(struct PlayState* play);
void KaleidoItemManager_Update(struct PlayState* play);
const char* KaleidoItemManager_GetPauseNameTexture(void);
bool KaleidoItemManager_OpenPauseDescription(struct PlayState* play, const S2HCustomItemDefinition* definition,
                                             int16_t cursorRow);
bool KaleidoItemManager_IsSlotEffectActive(uint8_t slot);
// The custom item button (S2H_ITEM_BUTTON_*) for a pause equip target (PAUSE_EQUIP_*).
uint8_t KaleidoItemManager_ToHeldButton(uint16_t equipTargetCBtn);
bool KaleidoItemManager_IsPauseEffectActive(const S2HCustomItemDefinition* definition);
void KaleidoItemManager_ToggleEquipment(const S2HCustomItemDefinition* definition);
void KaleidoItemManager_SetPauseVisible(const char* key, bool visible);
bool KaleidoItemManager_SetWheelSelection(uint16_t page, uint8_t slot, const char* key);
bool KaleidoQuestManager_UpdateExtra(struct PlayState* play);
bool KaleidoQuestManager_DrawExtra(struct PlayState* play);
bool KaleidoMaskEquipment_Update(struct PlayState* play);
bool KaleidoMaskEquipment_Draw(struct PlayState* play);

// Quest status page: ITEM_CUSTOM for a point a custom item claims, otherwise the vanilla cursor item.
uint16_t KaleidoQuestManager_ResolveCursorItem(uint8_t point, uint16_t vanillaItem);
void KaleidoQuestManager_Update(struct PlayState* play);
bool KaleidoQuestManager_ShowDescription(struct PlayState* play);

#ifdef __cplusplus
}
#endif

#endif
