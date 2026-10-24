#ifndef S2H_MOD_API_TIMERS_H
#define S2H_MOD_API_TIMERS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Timers_RequestHazard(const char* owner, int32_t priority, int16_t hazard, uint8_t requiredTunic);
void Timers_ReleaseHazard(const char* owner);
int16_t Timers_GetHazard(void);
// Called by the interface update once the vanilla hazard is known; returns the one the HUD must act on.
int16_t Timers_ResolveHazard(int16_t vanillaHazard);

bool Timers_StartCountdown(const char* owner, uint16_t seconds);
void Timers_StopCountdown(const char* owner);
uint16_t Timers_GetCountdown(const char* owner);
bool Timers_IsCountdownRunning(const char* owner);

#ifdef __cplusplus
}

void Timers_Init();
#endif

#endif
