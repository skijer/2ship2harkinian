#include "FleetPortal.h"
#include "FleetGame.h"
#include "FleetPassages.h"
#include "FleetRuntime.h"

#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/SaveManager/SaveManager.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "variables.h"
#include "z_title_setup.h"
#include "overlays/gamestates/ovl_file_choose/z_file_select.h"
}

namespace {

constexpr const char* kMarketPortal = "market";
// "Link" in the file select font: capitals from 0x0A, small letters from 0x24, 0x3E pads.
constexpr u8 kDefaultPlayerName[8] = { 0x15, 0x2C, 0x31, 0x2E, 0x3E, 0x3E, 0x3E, 0x3E };

struct Arrival {
    s32 entrance;
    s32 slot;
    bool cross = false;
};

std::optional<std::string> sDeparture;
std::optional<Arrival> sArrival;

std::optional<s32> EntranceForPortal(const std::string& portal) {
    if (portal == kMarketPortal) {
        return ENTRANCE(SOUTH_CLOCK_TOWN, 0);
    }
    return FleetPassages_Entrance(portal);
}

bool IsPlaying() {
    return gPlayState != nullptr && gGameState == &gPlayState->state && gSaveContext.gameMode == GAMEMODE_NORMAL;
}

void Depart(const std::string& portal) {
    if (FleetRuntime_HasSession()) {
        FleetRuntime_Save();
    }
    const std::string payload = nlohmann::json{ { "portal", portal }, { "slot", gSaveContext.fileNum } }.dump();
    const FleetDestination destination = { .game = nullptr, .entrance = 0, .payloadJson = payload.c_str() };
    FleetGame_RequestSwitch(&destination);
}

// Only the owl flag of the file select is read, so a blank one opens the slot's cycle save.
void OpenSave() {
    FileSelectState* fileSelect = static_cast<FileSelectState*>(std::calloc(1, sizeof(FileSelectState)));
    nlohmann::json saveFile;
    if (SaveManager_ReadSaveFile(SaveManager_GetFileName(gSaveContext.fileNum + 1), saveFile) != 0) {
        std::free(fileSelect);
        throw std::runtime_error("cannot open MM Fleet save");
    }
    fileSelect->isOwlSave[gSaveContext.fileNum + FILE_NUM_OWL_SAVE_OFFSET] =
        saveFile.is_object() && saveFile.contains("owlSave");
    std::vector<u8> buffer(SAVE_BUFFER_SIZE);
    SramContext sram = {};
    sram.saveBuf = buffer.data();
    Sram_OpenSave(fileSelect, &sram);
    std::free(fileSelect);
}

// The first arrival can find 2ship outside gameplay: it opens the slot Fleet names, or starts a new file there.
void BootIntoPlay(const Arrival& arrival) {
    FleetRuntime_Initialize();
    gSaveContext.fileNum = arrival.slot;
    if (SaveManager_SlotExists(arrival.slot)) {
        OpenSave();
    } else {
        FleetAutomaticSave automatic;
        Sram_InitNamedSave(arrival.slot, kDefaultPlayerName);
        if (FleetRuntime_HasSession() && !IS_RANDO) {
            throw std::runtime_error("MM could not create the combined randomizer save");
        }
        FleetRuntime_Save();
    }
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    if (FleetRuntime_HasSession() && (!IS_RANDO || gSaveContext.save.shipSaveInfo.rando.finalSeed !=
                                                       FleetRuntime_Seed().at("masterSeed").get<uint32_t>())) {
        throw std::runtime_error("MM save does not belong to this Fleet seed");
    }
    Sram_PrepareLoadedSave();
    if (arrival.entrance >= 0) {
        if (arrival.cross && !FleetRuntime_QueueArrival(arrival.entrance)) {
            throw std::runtime_error("MM rejected an arrival absent from this seed");
        }
        gSaveContext.respawnFlag = 0;
        gSaveContext.nextCutsceneIndex = 0xFFEF;
        gSaveContext.save.cutsceneIndex = 0;
        gSaveContext.save.entrance =
            arrival.cross ? FleetRuntime_ConsumeArrival() : FleetRuntime_Materialize(arrival.entrance);
        gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    }
    STOP_GAMESTATE(gGameState);
    SET_NEXT_GAMESTATE(gGameState, Play_Init, sizeof(PlayState));
}

void EnterArrival() {
    if (gGameState != nullptr && gGameState->running) {
        // Setup reloads natively first; the composed destination is consumed after that load.
        STOP_GAMESTATE(gGameState);
        SET_NEXT_GAMESTATE(gGameState, Setup_Init, sizeof(SetupState));
    }
}

void RegisterPortalHooks() {
    if (!FleetGame_IsHosted()) {
        return;
    }
    FleetRuntime_Initialize();
    if (FleetRuntime_HasSession() && !sArrival) {
        sArrival = Arrival{ -1, FleetRuntime_Slot() };
    }
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s16 sceneId, s8 spawnNum) {
        FleetRuntime_ObserveScene(sceneId);
        // Spawn 1 is the door from South Clock Town; the cycle resets land on the tower's other spawns.
        if (sceneId == SCENE_INSIDETOWER && spawnNum == 1 && gSaveContext.gameMode == GAMEMODE_NORMAL) {
            sDeparture = kMarketPortal;
        }
    });
}

static RegisterShipInitFunc sPortalInit(RegisterPortalHooks, {});

} // namespace

extern "C" void FleetPortal_RequestDeparture(const char* portal) {
    sDeparture = portal;
}

extern "C" bool FleetPortal_LoadPending(void) {
    if (!sArrival) {
        return false;
    }
    BootIntoPlay(*sArrival);
    sArrival.reset();
    return true;
}

extern "C" void FleetPortal_Arrive(const FleetDestination* arrival) {
    if (arrival == nullptr || arrival->payloadJson == nullptr) {
        return;
    }
    const nlohmann::json payload = nlohmann::json::parse(arrival->payloadJson, nullptr, false);
    if (!payload.is_object()) {
        return;
    }
    const int slot = payload.value("slot", 0);
    if (slot < 0 || slot > 2) {
        return;
    }
    if (payload.value("session", false)) {
        sArrival = Arrival{ -1, slot };
    } else if (payload.value("cross", false)) {
        sArrival = Arrival{ payload.at("entrance").get<s32>(), slot, true };
    } else if (const std::optional<s32> entrance = EntranceForPortal(payload.value("portal", ""))) {
        sArrival = Arrival{ .entrance = *entrance, .slot = slot };
    }
    if (sArrival) {
        EnterArrival();
    }
}

extern "C" void FleetPortal_FrameEnd(void) {
    FleetDestination destination = {};
    std::string payload;
    if (FleetRuntime_TakeDeparture(destination, payload)) {
        FleetGame_RequestSwitch(&destination);
        return;
    }
    if (sDeparture) {
        const std::string portal = *sDeparture;
        sDeparture.reset();
        Depart(portal);
    }
    if (sArrival) {
        EnterArrival();
    }
}
