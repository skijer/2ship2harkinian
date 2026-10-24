#ifndef S2H_MOD_API_MOD_MESSAGES_H
#define S2H_MOD_API_MOD_MESSAGES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define S2H_MOD_MESSAGE_INVALID 0xFFFF

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* text;
    uint8_t textboxType;
    uint8_t textboxPosition;
    uint8_t icon;
    uint16_t nextMessageId;
    bool autoFormat;
    // Optional translations; a missing one falls back to `text`.
    const char* textGerman;
    const char* textFrench;
    const char* textSpanish;
} S2HModMessage;

#define S2H_MOD_MESSAGE_MIN_SIZE (offsetof(S2HModMessage, autoFormat) + sizeof(bool))

uint16_t ModMessages_Register(const S2HModMessage* message);
uint16_t ModMessages_GetId(const char* key);
// The Actor.hintId whose Tatl line (text 0x1900 + id) is this hint; 0xFF once every mod id is taken.
uint8_t ModMessages_RegisterTatlHint(const char* actorKey, const char* hint);

#ifdef __cplusplus
}
#endif

#endif
