#ifndef S2H_MOD_API_OCARINA_SONG_REGISTRY_H
#define S2H_MOD_API_OCARINA_SONG_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "z64ocarina.h"
#ifdef __cplusplus
}
#endif

struct PlayState;

// A song MM's ocarina recognises, plays back and shows on a quest page, like its own
typedef struct {
    const char* itemKey;      // the custom item that teaches it; the song is playable while it is owned
    const OcarinaNote* notes; // ends with a zero-length note; at most 20, with 1 to 8 pitched ones
    uint16_t playedMessageId; // "You played..." from RegisterMessage; 0 for MM's plain staff box
    int16_t effectActorId;    // spawned on Link once it is played; 0 for none
    int16_t effectParams;
    uint16_t fanfareSeqId; // played under the replay, as MM's song fanfares; 0 for none
    void (*onPlayed)(struct PlayState* play);
} S2HOcarinaSongDefinition;

#ifdef __cplusplus
extern "C" {
#endif

// The song's ocarina slot (OCARINA_SONG_CUSTOM_FIRST onward), or -1 when it is invalid or no slot is left
int8_t OcarinaSongRegistry_Register(const S2HOcarinaSongDefinition* definition);
int8_t OcarinaSongRegistry_FindByItem(const char* itemKey);
bool OcarinaSongRegistry_IsRegistered(uint8_t songIndex);
bool OcarinaSongRegistry_IsOwned(uint8_t songIndex);
void OcarinaSongRegistry_UnlockOwnedSongs(void);
void OcarinaSongRegistry_Perform(struct PlayState* play, uint8_t songIndex);
void OcarinaSongRegistry_PlayFanfare(uint8_t songIndex, uint8_t ioData);
uint16_t OcarinaSongRegistry_GetPlayedMessage(uint8_t songIndex);

#ifdef __cplusplus
}
#endif

#endif
