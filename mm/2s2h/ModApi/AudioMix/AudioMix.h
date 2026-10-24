#ifndef S2H_MOD_API_AUDIO_MIX_H
#define S2H_MOD_API_AUDIO_MIX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*S2HAudioMixFunc)(int16_t* interleavedStereoSamples, uint32_t frameCount);

typedef enum {
    S2H_AUDIO_GROUP_SFX,
    S2H_AUDIO_GROUP_MUSIC,
    S2H_AUDIO_GROUP_SUB_MUSIC,
    S2H_AUDIO_GROUP_FANFARE,
    S2H_AUDIO_GROUP_MAX,
} S2HAudioGroup;

bool AudioMix_Register(S2HAudioMixFunc callback);
// volumeCvar is an optional float CVar (0..1) the mod exposes for its own volume.
bool AudioMix_RegisterInGroup(S2HAudioMixFunc callback, uint8_t group, const char* volumeCvar);
// Runs on the audio thread.
void AudioMix_Run(int16_t* samples, uint32_t frameCount);
void AudioMix_Init(void);

#ifdef __cplusplus
}
#endif

#endif
