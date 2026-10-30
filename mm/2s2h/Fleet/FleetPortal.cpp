#include "FleetPortal.h"
#include "FleetGame.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/SaveManager/SaveManager.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "variables.h"
#include "overlays/gamestates/ovl_file_choose/z_file_select.h"
}

namespace {

constexpr const char* kMarketPortal = "market";
// "Link" in the file select font: capitals from 0x0A, small letters from 0x24, 0x3E pads.
constexpr u8 kDefaultPlayerName[8] = { 0x15, 0x2C, 0x31, 0x2E, 0x3E, 0x3E, 0x3E, 0x3E };

struct Arrival {
    s32 entrance;
    s32 slot;
};

std::optional<std::string> sDeparture;
std::optional<Arrival> sArrival;

std::optional<s32> EntranceForPortal(const std::string& portal) {
    if (portal == kMarketPortal) {
        return ENTRANCE(SOUTH_CLOCK_TOWN, 0);
    }
    return std::nullopt;
}

bool IsPlaying() {
    return gPlayState != nullptr && gGameState == &gPlayState->state && gSaveContext.gameMode == GAMEMODE_NORMAL;
}

void Depart(const std::string& portal) {
    const std::string payload = nlohmann::json{ { "portal", portal }, { "slot", gSaveContext.fileNum } }.dump();
    const FleetDestination destination = { .game = nullptr, .entrance = 0, .payloadJson = payload.c_str() };
    FleetGame_RequestSwitch(&destination);
}

// Only the owl flag of the file select is read, so a blank one opens the slot's cycle save.
void OpenSave() {
    FileSelectState* fileSelect = static_cast<FileSelectState*>(std::calloc(1, sizeof(FileSelectState)));
    std::vector<u8> buffer(SAVE_BUFFER_SIZE);
    SramContext sram = {};
    sram.saveBuf = buffer.data();
    Sram_OpenSave(fileSelect, &sram);
    std::free(fileSelect);
}

// The first arrival can find 2ship outside gameplay: it opens the slot Fleet names, or starts a new file there.
void BootIntoPlay(const Arrival& arrival) {
    gSaveContext.fileNum = arrival.slot;
    if (SaveManager_SlotExists(arrival.slot)) {
        OpenSave();
    } else {
        Sram_InitNamedSave(arrival.slot, kDefaultPlayerName);
    }
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    Sram_PrepareLoadedSave();
    gSaveContext.save.entrance = arrival.entrance;
    STOP_GAMESTATE(gGameState);
    SET_NEXT_GAMESTATE(gGameState, Play_Init, sizeof(PlayState));
}

void EnterArrival() {
    if (IsPlaying()) {
        if (gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
            return;
        }
        gPlayState->nextEntrance = sArrival->entrance;
        gPlayState->transitionTrigger = TRANS_TRIGGER_START;
        gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
    } else if (gGameState != nullptr && gGameState->running) {
        BootIntoPlay(*sArrival);
    } else {
        return;
    }
    sArrival.reset();
}

void RegisterPortalHooks() {
    if (!FleetGame_IsHosted()) {
        return;
    }
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s16 sceneId, s8 spawnNum) {
        // Spawn 1 is the door from South Clock Town; the cycle resets land on the tower's other spawns.
        if (sceneId == SCENE_INSIDETOWER && spawnNum == 1 && gSaveContext.gameMode == GAMEMODE_NORMAL) {
            sDeparture = kMarketPortal;
        }
    });
}

static RegisterShipInitFunc sPortalInit(RegisterPortalHooks, {});

} // namespace

extern "C" void FleetPortal_Arrive(const FleetDestination* arrival) {
    if (arrival == nullptr || arrival->payloadJson == nullptr) {
        return;
    }
    const nlohmann::json payload = nlohmann::json::parse(arrival->payloadJson, nullptr, false);
    if (!payload.is_object()) {
        return;
    }
    if (const std::optional<s32> entrance = EntranceForPortal(payload.value("portal", ""))) {
        sArrival = Arrival{ .entrance = *entrance, .slot = payload.value("slot", 0) };
    }
}

extern "C" void FleetPortal_FrameEnd(void) {
    if (sDeparture) {
        const std::string portal = *sDeparture;
        sDeparture.reset();
        Depart(portal);
    }
    if (sArrival) {
        EnterArrival();
    }
}
