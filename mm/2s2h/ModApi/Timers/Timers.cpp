#include "Timers.h"

#include <map>
#include <string>

#include "2s2h/GameInteractor/GameInteractor.h"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
}

namespace {

// The generic minigame timer: the only HUD timer MM leaves free outside its own minigames.
constexpr int CountdownTimerId = TIMER_ID_MINIGAME_2;

struct HazardClaim {
    int32_t priority;
    int16_t hazard;
    uint8_t requiredTunic;
};

std::map<std::string, HazardClaim> sHazardClaims;
std::string sCountdownOwner;
int16_t sHazard = PLAYER_ENV_HAZARD_NONE;

bool WearsTunic(uint8_t tunic) {
    return GET_CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC) == tunic;
}

const HazardClaim* WinningClaim() {
    const HazardClaim* winner = nullptr;

    for (const auto& [owner, claim] : sHazardClaims) {
        if (claim.requiredTunic != EQUIP_VALUE_TUNIC_KOKIRI && WearsTunic(claim.requiredTunic)) {
            continue;
        }
        if (winner == nullptr || claim.priority > winner->priority) {
            winner = &claim;
        }
    }
    return winner;
}

void DropCountdownOnSceneChange(s8 sceneId, s8 spawnNum) {
    sCountdownOwner.clear();
}

} // namespace

extern "C" bool Timers_RequestHazard(const char* owner, int32_t priority, int16_t hazard, uint8_t requiredTunic) {
    if (owner == nullptr) {
        return false;
    }
    sHazardClaims[owner] = { priority, hazard, requiredTunic };
    return true;
}

extern "C" void Timers_ReleaseHazard(const char* owner) {
    if (owner != nullptr) {
        sHazardClaims.erase(owner);
    }
}

extern "C" int16_t Timers_GetHazard(void) {
    return sHazard;
}

extern "C" int16_t Timers_ResolveHazard(int16_t vanillaHazard) {
    const HazardClaim* claim = WinningClaim();

    sHazard = claim != nullptr ? claim->hazard : vanillaHazard;
    return sHazard;
}

extern "C" bool Timers_StartCountdown(const char* owner, uint16_t seconds) {
    const bool isTimerBusy = gSaveContext.timerStates[CountdownTimerId] != TIMER_STATE_OFF;
    if (owner == nullptr || (!sCountdownOwner.empty() && sCountdownOwner != owner) ||
        (sCountdownOwner.empty() && isTimerBusy)) {
        return false;
    }
    sCountdownOwner = owner;
    Interface_StartTimer(CountdownTimerId, seconds);
    return true;
}

extern "C" void Timers_StopCountdown(const char* owner) {
    if (owner == nullptr || sCountdownOwner != owner) {
        return;
    }
    gSaveContext.timerStates[CountdownTimerId] = TIMER_STATE_OFF;
    gSaveContext.timerCurTimes[CountdownTimerId] = SECONDS_TO_TIMER(0);
    sCountdownOwner.clear();
}

extern "C" uint16_t Timers_GetCountdown(const char* owner) {
    return Timers_IsCountdownRunning(owner) ? gSaveContext.timerCurTimes[CountdownTimerId] / SECONDS_TO_TIMER(1) : 0;
}

extern "C" bool Timers_IsCountdownRunning(const char* owner) {
    return owner != nullptr && sCountdownOwner == owner &&
           gSaveContext.timerStates[CountdownTimerId] != TIMER_STATE_OFF;
}

void Timers_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(DropCountdownOnSceneChange);
}
