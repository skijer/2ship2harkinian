#ifndef S2H_MOD_API_RANDO_ITEMS_H
#define S2H_MOD_API_RANDO_ITEMS_H

#include <stdbool.h>
#include <stdint.h>

#include "2s2h/ModApi/RandoInfCustom/RandoInfCustom.h"

#ifdef __cplusplus
extern "C" {
#endif

// A mod item's randoItem is a RandoItemId above RI_MAX, derived from its key so saves survive a changed mod order.
int32_t RandoItems_GetRandoGet(const char* key);
const char* RandoItems_GetKey(int32_t randoItem);
bool RandoItems_IsModItem(int32_t randoItem);
uint32_t RandoItems_GetCount(void);
int32_t RandoItems_GetAt(uint32_t index);

bool RandoItems_IsOwned(int32_t randoItem, S2HRandoInfBank bank);
void RandoItems_SetOwned(int32_t randoItem, S2HRandoInfBank bank, bool state);

#ifdef __cplusplus
}
#ifndef S2H_MOD
#include <vector>

#include "2s2h/Rando/Types.h"

int32_t RandoItems_Register(const char* key);
void RandoItems_BuildItemTable();
void RandoItems_AddToPool(std::vector<RandoItemId>& itemPool);

// Rando::GiveItem and RemoveItem route here: the active bank decides between the real save and a logic simulation.
void RandoItems_Give(RandoItemId randoItemId);
void RandoItems_Remove(RandoItemId randoItemId);
void RandoItems_SeedLogicBank();
const char* RandoItems_GetIconPath(RandoItemId randoItemId);
void RandoItems_Draw(RandoItemId randoItemId);
#endif
#endif

#endif
