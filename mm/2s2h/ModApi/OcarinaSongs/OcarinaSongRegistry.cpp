#include "OcarinaSongRegistry.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include "2s2h/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "2s2h/ModApi/ModMessages/ModMessages.h"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "sequence.h"
#include "sfx.h"
}

namespace {

constexpr size_t MaxSongNotes = 20;
constexpr size_t MaxSongButtons = 8;
constexpr size_t CustomSongCount = OCARINA_SONG_CUSTOM_LAST - OCARINA_SONG_CUSTOM_FIRST + 1;
constexpr uint16_t StaffMessageId = 0x1B5B;

struct CustomSong {
    std::string itemKey;
    std::array<OcarinaNote, MaxSongNotes> notes{};
    S2HOcarinaSongDefinition definition{};
};

std::array<std::optional<CustomSong>, CustomSongCount> sSongs;

std::optional<CustomSong>* FindSong(uint8_t songIndex) {
    if ((songIndex < OCARINA_SONG_CUSTOM_FIRST) || (songIndex > OCARINA_SONG_CUSTOM_LAST)) {
        return nullptr;
    }
    std::optional<CustomSong>* song = &sSongs[songIndex - OCARINA_SONG_CUSTOM_FIRST];
    return song->has_value() ? song : nullptr;
}

// The pitched notes before the zero-length end, or nothing when the melody has no end or too many notes
std::optional<uint8_t> CountSongButtons(const OcarinaNote* notes) {
    uint8_t buttons = 0;
    for (size_t i = 0; i < MaxSongNotes; i++) {
        if (notes[i].length == 0) {
            return (buttons == 0 || buttons > MaxSongButtons) ? std::nullopt : std::optional<uint8_t>(buttons);
        }
        if (notes[i].pitch != OCARINA_PITCH_NONE) {
            buttons++;
        }
    }
    return std::nullopt;
}

std::optional<uint8_t> PickSlot(const char* itemKey) {
    std::optional<uint8_t> free;
    for (uint8_t slot = 0; slot < CustomSongCount; slot++) {
        if (!sSongs[slot].has_value()) {
            free = free.value_or(slot);
        } else if (sSongs[slot]->itemKey == itemKey) {
            return slot;
        }
    }
    return free;
}

} // namespace

int8_t OcarinaSongRegistry_Register(const S2HOcarinaSongDefinition* definition) {
    if ((definition == nullptr) || (definition->itemKey == nullptr) || (definition->notes == nullptr)) {
        return -1;
    }
    std::optional<uint8_t> buttons = CountSongButtons(definition->notes);
    std::optional<uint8_t> slot = PickSlot(definition->itemKey);
    if (!buttons.has_value() || !slot.has_value()) {
        return -1;
    }

    CustomSong song;
    song.itemKey = definition->itemKey;
    for (size_t i = 0; i < MaxSongNotes; i++) {
        song.notes[i] = definition->notes[i];
        if (definition->notes[i].length == 0) {
            break;
        }
    }
    song.definition = *definition;
    song.definition.itemKey = nullptr;
    song.definition.notes = nullptr;
    sSongs[*slot] = std::move(song);

    const uint8_t songIndex = OCARINA_SONG_CUSTOM_FIRST + *slot;
    AudioOcarina_SetCustomSong(songIndex, sSongs[*slot]->notes.data(), *buttons);
    return static_cast<int8_t>(songIndex);
}

int8_t OcarinaSongRegistry_FindByItem(const char* itemKey) {
    if (itemKey == nullptr) {
        return -1;
    }
    for (uint8_t slot = 0; slot < CustomSongCount; slot++) {
        if (sSongs[slot].has_value() && (sSongs[slot]->itemKey == itemKey)) {
            return static_cast<int8_t>(OCARINA_SONG_CUSTOM_FIRST + slot);
        }
    }
    return -1;
}

bool OcarinaSongRegistry_IsRegistered(uint8_t songIndex) {
    return FindSong(songIndex) != nullptr;
}

bool OcarinaSongRegistry_IsOwned(uint8_t songIndex) {
    std::optional<CustomSong>* song = FindSong(songIndex);
    return (song != nullptr) && CustomItemRegistry_IsOwned((*song)->itemKey.c_str());
}

// Runs when the ocarina staff opens, so free play recognises only the songs Link has learned
void OcarinaSongRegistry_UnlockOwnedSongs(void) {
    uint8_t unlocked = 0;
    for (uint8_t slot = 0; slot < CustomSongCount; slot++) {
        if (OcarinaSongRegistry_IsOwned(OCARINA_SONG_CUSTOM_FIRST + slot)) {
            unlocked |= 1 << slot;
        }
    }
    AudioOcarina_SetCustomSongsUnlocked(unlocked);
}

void OcarinaSongRegistry_Perform(PlayState* play, uint8_t songIndex) {
    std::optional<CustomSong>* song = FindSong(songIndex);
    if ((song == nullptr) || (play == nullptr)) {
        return;
    }
    const S2HOcarinaSongDefinition& definition = (*song)->definition;
    if (definition.effectActorId > 0) {
        Player* player = GET_PLAYER(play);
        play->msgCtx.ocarinaSongEffectActive = true;
        Actor_Spawn(&play->actorCtx, play, definition.effectActorId, player->actor.world.pos.x,
                    player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0, definition.effectParams);
    }
    if (definition.onPlayed != nullptr) {
        definition.onPlayed(play);
    }
}

void OcarinaSongRegistry_PlayFanfare(uint8_t songIndex, uint8_t ioData) {
    std::optional<CustomSong>* song = FindSong(songIndex);
    if ((song == nullptr) || ((*song)->definition.fanfareSeqId == 0)) {
        return;
    }
    Audio_PlayFanfareWithPlayerIOPort7((*song)->definition.fanfareSeqId, ioData);
    AudioSfx_MuteBanks(0x20);
}

uint16_t OcarinaSongRegistry_GetPlayedMessage(uint8_t songIndex) {
    std::optional<CustomSong>* song = FindSong(songIndex);
    if (song == nullptr) {
        return StaffMessageId;
    }
    const uint16_t messageId = (*song)->definition.playedMessageId;
    bool hasMessage = (messageId != 0) && (messageId != S2H_MOD_MESSAGE_INVALID);
    return hasMessage ? messageId : StaffMessageId;
}
