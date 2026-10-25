#include "ModMessages.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"

extern "C" {
#include "z64.h"
#include "variables.h"
}

namespace {

// Above every vanilla text id and below 0x4E20, where Message_OpenText switches to the credits font.
constexpr uint16_t FirstModMessageId = 0x4000;
constexpr uint16_t LastModMessageId = 0x4DFF;

// Vanilla Tatl hints stop at TATL_HINT_ID_MUSHROOM (0x64); mods take the top of the range, below TATL_HINT_ID_NONE.
constexpr uint16_t TatlHintTextBase = 0x1900;
constexpr uint8_t FirstModTatlHintId = 0xA0;
constexpr uint8_t LastModTatlHintId = 0xFE;
constexpr uint8_t NoTatlHintId = 0xFF;

struct ModMessage {
    CustomMessage::Entry entry;
    std::string translations[LANGUAGE_MAX];
};

std::unordered_map<uint16_t, ModMessage> sMessages;
std::unordered_map<std::string, uint16_t> sMessageIdsByKey;
std::unordered_map<std::string, uint8_t> sTatlHintIdsByActor;
uint16_t sNextMessageId = FirstModMessageId;
uint8_t sNextTatlHintId = FirstModTatlHintId;

void LoadMessage(u16* textId, bool* loadFromMessageTable) {
    auto entry = sMessages.find(*textId);
    if (entry == sMessages.end()) {
        return;
    }
    *loadFromMessageTable = false;
    const u8 language = gSaveContext.options.language;
    CustomMessage::Entry localized = entry->second.entry;
    if (language < LANGUAGE_MAX && !entry->second.translations[language].empty()) {
        localized.msg = entry->second.translations[language];
    }
    CustomMessage::LoadCustomMessageIntoFont(localized);
}

bool IsValidMessage(const S2HModMessage* message) {
    return message != nullptr && message->structSize >= S2H_MOD_MESSAGE_MIN_SIZE && message->key != nullptr &&
           message->text != nullptr;
}

// Copies the fields the mod's struct has, so an older, shorter one leaves the translations empty.
S2HModMessage ReadMessage(const S2HModMessage* message) {
    S2HModMessage copy = {};
    std::memcpy(&copy, message, std::min<size_t>(message->structSize, sizeof(copy)));
    return copy;
}

} // namespace

uint16_t ModMessages_GetId(const char* key) {
    if (key == nullptr) {
        return S2H_MOD_MESSAGE_INVALID;
    }
    auto entry = sMessageIdsByKey.find(key);
    return entry == sMessageIdsByKey.end() ? S2H_MOD_MESSAGE_INVALID : entry->second;
}

uint16_t ModMessages_Register(const S2HModMessage* message) {
    if (!IsValidMessage(message)) {
        return S2H_MOD_MESSAGE_INVALID;
    }
    const uint16_t existing = ModMessages_GetId(message->key);
    if (existing != S2H_MOD_MESSAGE_INVALID) {
        return existing;
    }
    if (sNextMessageId > LastModMessageId) {
        SPDLOG_ERROR("[ModMessages] No text ids left for '{}'", message->key);
        return S2H_MOD_MESSAGE_INVALID;
    }
    const uint16_t textId = sNextMessageId++;
    const S2HModMessage source = ReadMessage(message);

    ModMessage stored;
    stored.entry.textboxType = source.textboxType;
    stored.entry.textboxYPos = source.textboxPosition;
    stored.entry.icon = source.icon;
    stored.entry.nextMessageID = source.nextMessageId;
    stored.entry.autoFormat = source.autoFormat;
    stored.entry.msg = source.text;
    stored.translations[LANGUAGE_GER] = source.textGerman != nullptr ? source.textGerman : "";
    stored.translations[LANGUAGE_FRE] = source.textFrench != nullptr ? source.textFrench : "";
    stored.translations[LANGUAGE_SPA] = source.textSpanish != nullptr ? source.textSpanish : "";
    sMessages.emplace(textId, std::move(stored));
    sMessageIdsByKey.emplace(message->key, textId);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(textId, LoadMessage);
    return textId;
}

uint8_t ModMessages_RegisterTatlHint(const char* actorKey, const char* hint) {
    if (actorKey == nullptr || hint == nullptr) {
        return NoTatlHintId;
    }
    auto existing = sTatlHintIdsByActor.find(actorKey);
    if (existing != sTatlHintIdsByActor.end()) {
        return existing->second;
    }
    if (sNextTatlHintId > LastModTatlHintId) {
        SPDLOG_ERROR("[ModMessages] No Tatl hint ids left for '{}'", actorKey);
        return NoTatlHintId;
    }
    const uint8_t hintId = sNextTatlHintId++;
    const uint16_t textId = TatlHintTextBase + hintId;

    ModMessage stored;
    stored.entry.msg = hint;
    sMessages.emplace(textId, std::move(stored));
    sTatlHintIdsByActor.emplace(actorKey, hintId);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(textId, LoadMessage);
    return hintId;
}
