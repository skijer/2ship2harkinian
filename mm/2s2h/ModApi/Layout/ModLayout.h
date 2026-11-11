#ifndef S2H_MOD_LAYOUT_H
#define S2H_MOD_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>
#include "ultra64.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

typedef enum {
    S2H_LAYOUT_ITEMS,
    S2H_LAYOUT_EQUIPMENT,
    S2H_LAYOUT_COLLECTABLES,
    S2H_LAYOUT_COUNT,
} S2HLayoutKind;

typedef struct {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
} S2HLayoutCell;

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* title;
    uint8_t kind;
    uint8_t rows;
    uint8_t columns;
    uint8_t cellCount;
    const S2HLayoutCell* cells;
    // Optional presentation tail. Background sections use Kaleido's 3 columns x 5 rows (IA8).
    const char* const* backgroundSections;
    uint16_t targetPage; // Used only with S2H_LAYOUT_FIXED_PAGE.
    uint16_t flags;
    // Optional 16x16 IA4 cursor corners: top-left, top-right, bottom-left, bottom-right.
    const char* const* cursorCorners;
} S2HLayoutPage;

#define S2H_LAYOUT_FIXED_PAGE 1
#define S2H_LAYOUT_EQUIP_CONTROLS 2

void** ModLayout_GetBackground(uint16_t page, void** fallback);
uint16_t ModLayout_GetFlags(uint16_t page);
const char* ModLayout_GetPageKey(uint16_t page);
const char* const* ModLayout_GetCursorCorners(uint16_t page);

typedef struct {
    const char* key;
    const char* pageKey;
    uint8_t sourceKind;
    uint8_t source;
    uint8_t cell;
} S2HLayoutVanilla;

void ModLayout_ApplyCells(struct PlayState* play, uint16_t page, Vtx* vertices, uint8_t count);
bool ModLayout_GetCell(uint16_t page, uint8_t slot, S2HLayoutCell* cell);
bool ModLayout_SetCell(uint16_t page, uint8_t slot, const S2HLayoutCell* cell);
// Cells provide default geometry; user edits take precedence. Equipment pages are shown over Masks.
// Existing callers without the optional presentation tail remain supported through structSize.
bool ModLayout_RegisterPage(const S2HLayoutPage* page);
// "vanilla" places on the vanilla item grid or quest page (by the item's own page) but never on native entries.
bool ModLayout_PlaceItem(const char* itemKey, const char* pageKey, uint8_t cell);
// Native slot references need the vanilla grid to be rearrangeable, which MM's pause menu is not.
bool ModLayout_RegisterVanilla(const S2HLayoutVanilla* entry);

// Pause pages are cycled and drawn by the item, collectable and mask equipment managers.
bool ModLayout_CyclePage(struct PlayState* play, uint8_t kind);
bool ModLayout_DrawPage(struct PlayState* play, uint8_t kind);
bool ModLayout_IsActive(uint8_t kind);
bool ModLayout_BeginCustomEquip(struct PlayState* play, const char* key, uint8_t button, int16_t x, int16_t y);
void ModLayout_CompleteCustomEquip(struct PlayState* play);
const char* ModLayout_GetEquipIcon(void);
bool ModLayout_HasMods(void);

#ifdef __cplusplus
}

void ModLayout_DrawEditor(uint8_t kind);
#endif

#endif
