#ifndef S2H_MOD_API_PLAYER_INPUT_H
#define S2H_MOD_API_PLAYER_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Hides buttons and the stick from Player only; the mod still reads the real pad through play->state.input.
bool PlayerInput_Block(const char* owner, uint32_t buttons, bool blockStick);
void PlayerInput_Release(const char* owner);
uint32_t PlayerInput_GetBlockedButtons(void);
bool PlayerInput_IsStickBlocked(void);

#ifdef __cplusplus
}

void PlayerInput_Init();
#endif

#endif
