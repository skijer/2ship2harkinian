#ifndef S2H_MOD_API_FORM_MODEL_H
#define S2H_MOD_API_FORM_MODEL_H

#include <stdbool.h>

#include "2s2h/ModApi/Forms/FormRegistry.h"

#ifdef __cplusplus
extern "C" {
#endif

bool FormModel_IsActive(void);

// Player_DrawGameplay (z_player.c), right before Player_DrawImpl: wraps the limb override and forces the near LOD.
void FormModel_BeginPlayerDraw(Player* player, s32* lod, OverrideLimbDrawFlex* overrideLimbDraw);

#ifdef __cplusplus
}

void FormModel_Init();
void FormModel_BeginOtherPlayerDraw(Player* player, const char* key);
void FormModel_EndOtherPlayerDraw();
void FormModel_SetActive(const S2HFormDefinition* form);
#endif

#endif
