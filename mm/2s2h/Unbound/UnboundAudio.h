#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint16_t Unbound_RegisterSequence(const char* path);
int32_t Unbound_RegisterSoundFont(const char* path);
uint16_t Unbound_SequenceIdForPath(const char* path);
int UnboundAudio_GrowSequenceMap(uint32_t needed);
int UnboundAudio_GrowFontMap(uint32_t needed);
void ModApi_InitializeRuntime(void);
#ifdef __cplusplus
}
#endif
