#pragma once

#include <stdbool.h>
#include <stdint.h>

// A randomizer location a mod adds to a vanilla area. It joins the seed only when its option's chosen value is one of
// optionValues (bit n set selects value n), and logic reaches it through the named region plus the extra condition.
typedef struct {
    uint32_t structSize;
    const char* key;
    const char* name;
    int16_t scene;
    const char* region;
    const char* logic;
    const char* optionKey;
    uint32_t optionValues;
} S2HRandoCheckDefinition;

#ifdef __cplusplus
extern "C" {
#endif

// Call from ModInit, before any seed is generated. Returns the check id, or -1 when the definition is rejected.
int32_t RandoChecks_Register(const S2HRandoCheckDefinition* definition);
int32_t RandoChecks_GetId(const char* key);
bool RandoChecks_IsObtained(int32_t check);
// Queues the placed item through the randomizer's own give path; false if the check is not shuffled in the loaded
// seed, was already obtained, or is already queued.
bool RandoChecks_Award(int32_t check);
// The RandoItemType of the item placed at the check, or RITYPE_MAX when the check is not shuffled in the loaded seed.
uint8_t RandoChecks_GetItemCategory(int32_t check);

#ifdef __cplusplus
}
#endif
