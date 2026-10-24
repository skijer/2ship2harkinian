#ifndef S2H_MOD_API_RANDO_OPTIONS_H
#define S2H_MOD_API_RANDO_OPTIONS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    S2H_MOD_RANDO_WIDGET_COMBOBOX = 0,
    S2H_MOD_RANDO_WIDGET_SLIDER = 1,
} S2HModRandoWidget;

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* label;
    const char* tooltip;
    const char* const* values;
    uint32_t valueCount;
    uint8_t defaultValue;
    uint8_t widget;
} S2HModRandoOption;

#define S2H_MOD_RANDO_OPTION_MIN_SIZE \
    (offsetof(S2HModRandoOption, defaultValue) + sizeof(((S2HModRandoOption*)0)->defaultValue))

bool RandoOptions_Register(const S2HModRandoOption* option);
uint8_t RandoOptions_Get(const char* key);
uint8_t RandoOptions_GetForSeed(const char* key);

#ifdef __cplusplus
}

void RandoOptions_Init();

// While a seed is being generated the menu choice is the seed's value; afterwards the value captured with the save is.
void RandoOptions_BeginSeedGeneration();
void RandoOptions_EndSeedGeneration();
#endif

#endif
