#ifndef S2H_MOD_API_FORM_TRANSFORM_H
#define S2H_MOD_API_FORM_TRANSFORM_H

#include <stdbool.h>

#include "2s2h/ModApi/Forms/FormRegistry.h"

#ifdef __cplusplus
extern "C" {
#endif

bool FormTransform_IsActive(void);

#ifdef __cplusplus
}

void FormTransform_Init();
void FormTransform_DrawMask(Player* player, s32 limbIndex);
bool FormTransform_Begin(const S2HFormDefinition* from, const S2HFormDefinition* to);
#endif

#endif
