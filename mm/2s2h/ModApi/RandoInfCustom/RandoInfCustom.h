#ifndef S2H_MOD_API_RANDO_INF_CUSTOM_H
#define S2H_MOD_API_RANDO_INF_CUSTOM_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    S2H_RANDO_INF_SAVE,
    S2H_RANDO_INF_LOGIC,
    S2H_RANDO_INF_BANK_MAX,
} S2HRandoInfBank;

#ifdef __cplusplus
extern "C" {
#endif

#define RANDO_INF_CUSTOM_NONE 0

uint16_t RandoInfCustom_Register(const char* key);
uint16_t RandoInfCustom_Find(const char* key);
const char* RandoInfCustom_GetKey(uint16_t flag);
uint16_t RandoInfCustom_GetCount(void);

bool RandoInfCustom_Get(S2HRandoInfBank bank, uint16_t flag);
void RandoInfCustom_Set(S2HRandoInfBank bank, uint16_t flag, bool state);
void RandoInfCustom_ClearBank(S2HRandoInfBank bank);

bool RandoInfCustom_GetByKey(const char* key);
void RandoInfCustom_SetByKey(const char* key, bool state);
bool RandoInfCustom_IsLogicSimulation(void);

#ifdef __cplusplus
}

void RandoInfCustom_Init();
#include <string>
#include <vector>
std::vector<std::string> RandoInfCustom_ListSet();

// 2Ship's generator simulates progress on gSaveContext itself, so the logic bank is a scratch copy of the save bank.
void RandoInfCustom_BeginLogicSimulation();
void RandoInfCustom_EndLogicSimulation();
S2HRandoInfBank RandoInfCustom_GetActiveBank();
#endif

#endif
