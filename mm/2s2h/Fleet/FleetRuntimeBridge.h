#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
void FleetCycle_Capture(void);
void FleetCycle_Restore(void);

int32_t FleetRuntime_ResolveExit(int32_t entrance, int beforeNative);
int FleetRuntime_CompleteTransition(void);
int FleetRuntime_EntrancesEnabled(void);
void FleetRuntime_CancelTransition(void);
int32_t FleetRuntime_RecoveryEntrance(void);
int32_t FleetRuntime_ResolveTransition(int32_t entrance);

#ifdef __cplusplus
}
#endif
