#include "FleetRuntime.h"
#include "FleetRuntimeBridge.h"
#include "2s2h/Rando/Rando.h"
#include <memory>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

namespace {
std::unique_ptr<SaveContext> sCycle;
ActorContextSceneFlags sScene;
std::vector<std::pair<RandoCheckId, bool>> sChecks;

template <class T, size_t N> void Keep(T (&target)[N], const T (&source)[N]) {
    for (size_t i = 0; i < N; i++) {
        target[i] |= source[i];
    }
}
} // namespace

extern "C" void FleetCycle_Capture(void) {
    sCycle.reset();
    sChecks.clear();
    if (!FleetRuntime_HasSession() || !IS_RANDO || gPlayState == nullptr) {
        return;
    }
    Play_SaveCycleSceneFlags(gPlayState);
    sCycle = std::make_unique<SaveContext>(gSaveContext);
    sScene = gPlayState->actorCtx.sceneFlags;
    for (const auto& [id, check] : Rando::StaticData::Checks) {
        sChecks.emplace_back(id, RANDO_SAVE_CHECKS[id].cycleObtained);
    }
}

extern "C" void FleetCycle_Restore(void) {
    if (!sCycle) {
        return;
    }
    Keep(gSaveContext.save.saveInfo.weekEventReg, sCycle->save.saveInfo.weekEventReg);
    for (size_t i = 0; i < ARRAY_COUNT(gSaveContext.cycleSceneFlags); i++) {
        auto& flags = gSaveContext.cycleSceneFlags[i];
        const auto& old = sCycle->cycleSceneFlags[i];
        flags.chest |= old.chest;
        flags.switch0 |= old.switch0;
        flags.switch1 |= old.switch1;
        flags.clearedRoom |= old.clearedRoom;
        flags.collectible |= old.collectible;
        gSaveContext.save.saveInfo.permanentSceneFlags[i].chest |= flags.chest;
        gSaveContext.save.saveInfo.permanentSceneFlags[i].switch0 |= flags.switch0;
        gSaveContext.save.saveInfo.permanentSceneFlags[i].switch1 |= flags.switch1;
        gSaveContext.save.saveInfo.permanentSceneFlags[i].clearedRoom |= flags.clearedRoom;
        gSaveContext.save.saveInfo.permanentSceneFlags[i].collectible |= flags.collectible;
    }
    gPlayState->actorCtx.sceneFlags.chest |= sScene.chest;
    for (size_t i = 0; i < ARRAY_COUNT(sScene.switches); i++) {
        gPlayState->actorCtx.sceneFlags.switches[i] |= sScene.switches[i];
    }
    for (size_t i = 0; i < ARRAY_COUNT(sScene.collectible); i++) {
        gPlayState->actorCtx.sceneFlags.collectible[i] |= sScene.collectible[i];
    }
    gPlayState->actorCtx.sceneFlags.clearedRoom |= sScene.clearedRoom;
    for (const auto& [id, obtained] : sChecks) {
        RANDO_SAVE_CHECKS[id].cycleObtained |= obtained;
    }
    sCycle.reset();
    sChecks.clear();
}
