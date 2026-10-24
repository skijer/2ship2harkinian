#ifndef S2H_MOD_API_MICROPHONE_H
#define S2H_MOD_API_MICROPHONE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    S2H_MICROPHONE_OPEN,
    S2H_MICROPHONE_PENDING,
    S2H_MICROPHONE_DENIED,
    S2H_MICROPHONE_UNAVAILABLE,
} S2HMicrophoneStatus;

S2HMicrophoneStatus Microphone_Open(uint32_t sampleRate);
uint32_t Microphone_Read(float* samples, uint32_t capacity);
void Microphone_Close(void);

#ifdef __cplusplus
}

void Microphone_Init();
#endif

#endif
