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
void** KaleidoItemManager_GetBackground(void** vanilla);
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
bool KaleidoItemManager_SetDefaultPlacement(const char* key, const S2HCustomItemPlacement* placement);
void KaleidoItemManager_SetPauseVisible(const char* key, bool visible);
bool KaleidoItemManager_SetWheelSelection(uint16_t page, uint8_t slot, const char* key);
bool KaleidoQuestManager_UpdateExtra(struct PlayState* play);
bool KaleidoQuestManager_DrawExtra(struct PlayState* play);
bool KaleidoMaskEquipment_Update(struct PlayState* play);
uint16_t KaleidoMaskEquipment_GetShownPage(void);
void** KaleidoMaskEquipment_GetBackground(void** vanilla);
bool KaleidoMaskEquipment_Draw(struct PlayState* play);

// Screen rectangle (320x240 space) a pause page shows the live player in; the image is 64:112 like OoT's.
typedef struct {
    uint32_t structSize;
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
} S2HPlayerPreview;

// The layout page shown on the mask face of the pause cube, or NULL when it shows MM's own masks.
const char* KaleidoItemManager_GetShownLayoutPage(void);
// Call from AfterKaleidoDrawPage. Draws at most once per frame, and only while the page faces the camera still.
bool KaleidoItemManager_DrawPlayerPreview(struct PlayState* play, const S2HPlayerPreview* preview);

// Quest status page: ITEM_CUSTOM for a point a custom item claims, otherwise the vanilla cursor item.
uint16_t KaleidoQuestManager_ResolveCursorItem(uint8_t point, uint16_t vanillaItem);
void KaleidoQuestManager_Update(struct PlayState* play);
bool KaleidoQuestManager_ShowDescription(struct PlayState* play);

#ifdef __cplusplus
}
#endif

#endif
