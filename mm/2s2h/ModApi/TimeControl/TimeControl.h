#ifndef S2H_MOD_API_TIME_CONTROL_H
#define S2H_MOD_API_TIME_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The highest priority claim wins; worldSpeed 0 stops every actor but the player, below 1 slows them.
bool TimeControl_Request(const char* owner, int32_t priority, float worldSpeed, bool freezeClock);
void TimeControl_Release(const char* owner);
float TimeControl_GetWorldSpeed(void);

#ifdef __cplusplus
}

void TimeControl_Init();
#endif

#endif
