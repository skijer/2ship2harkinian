#ifndef S2H_MOD_API_VANILLA_ITEMS_H
#define S2H_MOD_API_VANILLA_ITEMS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct S2HGetItemEntry;

typedef enum {
    S2H_VANILLA_ITEM_UPGRADE = 0,
    S2H_VANILLA_ITEM_REPLACE,
    S2H_VANILLA_ITEM_BLOCK,
} S2HVanillaItemMode;

bool VanillaItems_Block(uint16_t vanillaItem, int32_t randoItem);
const char* VanillaItems_GetCustomKey(uint16_t vanillaItem);
const char* VanillaItems_GetCustomKeyForRandoItem(int32_t randoItem);
bool VanillaItems_GrantRandoItem(int32_t randoItem);
bool VanillaItems_Give(struct PlayState* play, uint16_t vanillaItem);
bool VanillaItems_Drop(struct PlayState* play, uint16_t vanillaItem);
bool VanillaItems_IsVanillaSuppressed(uint16_t vanillaItem);
bool VanillaItems_IsRandoItemSuppressed(int32_t randoItem);
// Fills entry from MM's get-item table; false (entry zeroed) for GI_NONE or an id past GI_MAX.
bool VanillaItems_GetItemEntry(int16_t getItemId, struct S2HGetItemEntry* entry);

#ifdef __cplusplus
}

void VanillaItems_Init();
#endif

#endif
