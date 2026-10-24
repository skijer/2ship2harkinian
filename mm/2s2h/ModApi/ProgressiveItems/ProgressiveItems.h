#ifndef S2H_PROGRESSIVE_ITEMS_H
#define S2H_PROGRESSIVE_ITEMS_H
#include <stdint.h>
#include <stdbool.h>
#include "z64.h"
#include "2s2h/ModApi/RandoInfCustom/RandoInfCustom.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef uint16_t (*S2HProgressiveReadLevel)(const SaveContext* save);
typedef void (*S2HProgressiveApplyLevel)(SaveContext* save, uint16_t level);
typedef struct {
    const char* itemKey; // Registered custom presentation for this level.
} S2HProgressiveLevel;
typedef struct {
    int32_t randoItem;
    uint16_t vanillaItem; // UINT16_MAX when this alias has no Item_Give identity.
    uint16_t firstLevel;  // BGS can start at level 2; Knife at level 1.
} S2HProgressiveAlias;
typedef struct {
    uint32_t structSize;
    const char* key;
    const char* pickupKey; // Repeated custom pool/drop pickup; copies always advance.
    const S2HProgressiveLevel* levels;
    uint16_t levelCount;
    uint16_t firstLevel;
    const S2HProgressiveAlias* aliases;
    uint16_t aliasCount;
    S2HProgressiveReadLevel readLevel;
    S2HProgressiveApplyLevel applyLevel;
    // Optional seed policy; 0 retains automatic alias/pool inference.
    uint16_t (*readFirstLevel)(void);
} S2HProgressiveChain;
bool ProgressiveItems_Register(const S2HProgressiveChain* chain);
uint16_t ProgressiveItems_GetLevel(const char* key, S2HRandoInfBank bank);
const char* ProgressiveItems_GetNextItem(const char* key, S2HRandoInfBank bank);
const char* ProgressiveItems_ResolvePickup(const char* key, uint16_t vanillaItem);
bool ProgressiveItems_ReceiveCustom(const char* key);
bool ProgressiveItems_GiveVanilla(uint16_t item);
bool ProgressiveItems_GiveRando(int32_t item);
void ProgressiveItems_ResetLogic(void);
#ifdef __cplusplus
}
#include <vector>
#include <nlohmann/json.hpp>
#include "2s2h/Rando/Types.h"
void ProgressiveItems_Init();
nlohmann::json ProgressiveItems_NetworkState();
void ProgressiveItems_MergeNetworkState(const nlohmann::json& state);
bool ProgressiveItems_ChangeRando(int32_t item, bool state);
bool ProgressiveItems_HasLogicItem(int32_t item, bool* result);
const char* ProgressiveItems_ResolveRando(int32_t item, S2HRandoInfBank bank);
void ProgressiveItems_ConfigurePool(const std::vector<RandoItemId>& pool);
void ProgressiveItems_ResetLogic();
bool ProgressiveItems_IsSource(int32_t item);
bool ProgressiveItems_CheckObtainability(int32_t item, bool* result);
#endif
#endif
