#include "FleetPortal.h"
#include "FleetGame.h"
#include "FleetPassages.h"
#include "FleetRuntime.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/ModApi.h"
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
constexpr const char* kOwlPortalPrefix = "owl:";
constexpr s32 kOwlWarpEntrances[OWL_WARP_ENTRANCE] = {
    ENTRANCE(GREAT_BAY_COAST, 11), ENTRANCE(ZORA_CAPE, 6),
    ENTRANCE(SNOWHEAD, 3),         ENTRANCE(MOUNTAIN_VILLAGE_WINTER, 8),
    ENTRANCE(SOUTH_CLOCK_TOWN, 9), ENTRANCE(MILK_ROAD, 4),
    ENTRANCE(WOODFALL, 4),         ENTRANCE(SOUTHERN_SWAMP_POISONED, 10),
    ENTRANCE(IKANA_CANYON, 4),     ENTRANCE(STONE_TOWER, 3),
};
// "Link" in the file select font: capitals from 0x0A, small letters from 0x24, 0x3E pads.
constexpr u8 kDefaultPlayerName[8] = { 0x15, 0x2C, 0x31, 0x2E, 0x3E, 0x3E, 0x3E, 0x3E };

struct Arrival {
    s32 entrance;
    s32 slot;
    bool cross = false;
    s32 owlWarp = -1;
};

std::optional<std::string> sDeparture;
std::optional<Arrival> sArrival;

std::optional<s32> OwlWarpForPortal(const std::string& portal) {
    if (portal.rfind(kOwlPortalPrefix, 0) != 0) {
        return std::nullopt;
    }
    const s32 owlWarp = std::atoi(portal.c_str() + std::strlen(kOwlPortalPrefix));
    return owlWarp >= 0 && owlWarp < OWL_WARP_ENTRANCE ? std::optional<s32>(owlWarp) : std::nullopt;
}

std::optional<Arrival> ArrivalForPortal(const std::string& portal, s32 slot) {
    if (portal == kMarketPortal) {
        return Arrival{ ENTRANCE(SOUTH_CLOCK_TOWN, 0), slot };
    }
    if (const auto owlWarp = OwlWarpForPortal(portal)) {
        return Arrival{ .entrance = kOwlWarpEntrances[*owlWarp], .slot = slot, .owlWarp = *owlWarp };
    }
    if (const auto entrance = FleetPassages_Entrance(portal)) {
        return Arrival{ *entrance, slot };
    }
    return std::nullopt;
}

s32 OwlWarpEntrance(s32 owlWarp) {
    const s32 entrance = kOwlWarpEntrances[owlWarp];
    if (entrance == ENTRANCE(SOUTHERN_SWAMP_POISONED, 10) && CHECK_WEEKEVENTREG(WEEKEVENTREG_CLEARED_WOODFALL_TEMPLE)) {
        return ENTRANCE(SOUTHERN_SWAMP_CLEARED, 10);
    }
    if (entrance == ENTRANCE(MOUNTAIN_VILLAGE_WINTER, 8) && CHECK_WEEKEVENTREG(WEEKEVENTREG_CLEARED_SNOWHEAD_TEMPLE)) {
        return ENTRANCE(MOUNTAIN_VILLAGE_SPRING, 8);
    }
    return entrance;
}

s32 ArrivalEntrance(const Arrival& arrival) {
    if (arrival.cross) {
        return FleetRuntime_ConsumeArrival();
    }
    if (arrival.owlWarp >= 0) {
        // Off, the statue's owl start mode flies Link in instead of replaying an owl save.
        gSaveContext.save.isOwlSave = false;
        return FleetRuntime_Materialize(OwlWarpEntrance(arrival.owlWarp));
    }
    return FleetRuntime_Materialize(arrival.entrance);
}

bool IsPlaying() {
    return gPlayState != nullptr && gGameState == &gPlayState->state && gSaveContext.gameMode == GAMEMODE_NORMAL;
}

void Depart(const std::string& portal) {
    if (FleetRuntime_HasSession()) {
        FleetRuntime_Save();
        FleetRuntime_FinishSaving();
    }
    const std::string payload = nlohmann::json{ { "portal", portal }, { "slot", gSaveContext.fileNum } }.dump();
    const FleetDestination destination = { .game = nullptr, .entrance = 0, .payloadJson = payload.c_str() };
    FleetGame_RequestSwitch(&destination);
}

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

void BootIntoPlay(const Arrival& arrival) {
    FleetRuntime_Initialize();
    // A title demo cutscene (0xFFF0+) left queued would block pausing; the load below queues its own.
    gSaveContext.nextCutsceneIndex = 0xFFEF;
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
        FleetRuntime_FinishSaving();
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
        gSaveContext.save.entrance = ArrivalEntrance(arrival);
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
    ModApi_PublishHostService("fleet.depart", reinterpret_cast<void*>(FleetPortal_RequestDeparture));
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](s16 sceneId, s8 spawnNum) {
        FleetRuntime_ObserveScene(sceneId);
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

// Leaving Play at once also stops a parked game from catching up a save the host is about to restore.
extern "C" void FleetPortal_EndSession(void) {
    FleetRuntime_EndSession();
    sDeparture.reset();
    sArrival.reset();
    S2H_ResetToTitle();
}

extern "C" bool FleetPortal_LoadPending(void) {
    if (!sArrival) {
        return false;
    }
    BootIntoPlay(*sArrival);
    sArrival.reset();
    return true;
}

// A game entered without a destination boots the session's save, as on its first start, unless it is playing it.
static void ResumeSessionIfIdle() {
    if (gGameState == nullptr || (IsPlaying() && gGameState->running) || !FleetRuntime_HasSession()) {
        return;
    }
    sArrival = Arrival{ -1, FleetRuntime_Slot() };
    EnterArrival();
}

extern "C" void FleetPortal_Arrive(const FleetDestination* arrival) {
    if (arrival == nullptr || arrival->payloadJson == nullptr) {
        ResumeSessionIfIdle();
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
    } else {
        sArrival = ArrivalForPortal(payload.value("portal", ""), slot);
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
