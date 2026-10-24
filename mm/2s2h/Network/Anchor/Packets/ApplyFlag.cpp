#include "2s2h/Network/Anchor/Anchor.h"
extern "C" {
#include "functions.h"
}
namespace {
template <typename T> void ChangeBit(T& bits, uint32_t bit, bool state) {
    const T mask = static_cast<T>(T(1) << bit);
    if (state) bits |= mask; else bits &= ~mask;
}
}
void Anchor::ApplyFlag(s16 scene, FlagType type, uint32_t flag, bool state) {
    switch (type) {
        case FLAG_WEEK_EVENT_REG:
            if ((flag >> 8) < sizeof(gSaveContext.save.saveInfo.weekEventReg)) {
                if (state) Flags_SetWeekEventReg(flag); else Flags_ClearWeekEventReg(flag);
            }
            return;
        case FLAG_WEEK_EVENT_REG_HORSE_RACE:
            Flags_SetWeekEventRegHorseRace(state ? flag : 0);
            return;
        case FLAG_EVENT_INF:
            if ((flag >> 4) < sizeof(gSaveContext.eventInf) && (flag & 15) < 8) {
                if (state) Flags_SetEventInf(flag); else Flags_ClearEventInf(flag);
            }
            return;
        case FLAG_RANDO_INF:
            if ((flag >> 4) < ARRAY_COUNT(gSaveContext.save.shipSaveInfo.rando.randoInf)) {
                if (state) Flags_SetRandoInf(flag); else Flags_ClearRandoInf(flag);
            }
            return;
        case FLAG_SCENES_VISIBLE:
            if (flag < 32 * ARRAY_COUNT(gSaveContext.save.saveInfo.scenesVisible)) ChangeBit(gSaveContext.save.saveInfo.scenesVisible[flag / 32], flag % 32, state);
            return;
        case FLAG_OWL_ACTIVATION:
            if (flag < 16) ChangeBit(gSaveContext.save.saveInfo.playerData.owlActivationFlags, flag, state);
            return;
        default: break;
    }
    if (scene < 0) return;
    auto* permanent = SceneFlags_GetPermanent(scene);
    auto* cycle = SceneFlags_GetCycle(scene);
    if (!permanent || !cycle) return;
    u32* stored = nullptr;
    switch (type) {
        case FLAG_PERM_SCENE_CHEST: stored = &permanent->chest; break;
        case FLAG_PERM_SCENE_SWITCH: if (flag < 64) stored = flag < 32 ? &permanent->switch0 : &permanent->switch1; break;
        case FLAG_PERM_SCENE_CLEARED_ROOM: if (flag < 32) stored = &permanent->clearedRoom; else SceneFlags_SetRoomClear(scene, flag, 0, state); break;
        case FLAG_PERM_SCENE_COLLECTIBLE: stored = &permanent->collectible; break;
        case FLAG_PERM_SCENE_UNK_14: stored = &permanent->unk_14; break;
        case FLAG_PERM_SCENE_ROOMS: if (state) SceneFlags_SetRoomVisited(scene, flag); else SceneFlags_ClearRoomVisited(scene, flag); break;
        case FLAG_CYCL_SCENE_CHEST: stored = &cycle->chest; break;
        case FLAG_CYCL_SCENE_SWITCH: if (flag < 64) stored = flag < 32 ? &cycle->switch0 : &cycle->switch1; break;
        case FLAG_CYCL_SCENE_CLEARED_ROOM: if (flag < 32) stored = &cycle->clearedRoom; else SceneFlags_SetRoomClear(scene, flag, 0, state); break;
        case FLAG_CYCL_SCENE_COLLECTIBLE: stored = &cycle->collectible; break;
        default: return;
    }
    const bool switchType = type == FLAG_PERM_SCENE_SWITCH || type == FLAG_CYCL_SCENE_SWITCH;
    if (stored && (flag < 32 || (switchType && flag < 64))) ChangeBit(*stored, flag % 32, state);
    if (!gPlayState || gPlayState->sceneId != scene) return;
    auto& live = gPlayState->actorCtx.sceneFlags;
    if (switchType && flag < 128) ChangeBit(live.switches[flag / 32], flag % 32, state);
    if ((type == FLAG_CYCL_SCENE_CHEST || type == FLAG_PERM_SCENE_CHEST) && flag < 32) ChangeBit(live.chest, flag, state);
    if ((type == FLAG_CYCL_SCENE_COLLECTIBLE || type == FLAG_PERM_SCENE_COLLECTIBLE) && flag < 128) ChangeBit(live.collectible[flag / 32], flag % 32, state);
    if ((type == FLAG_CYCL_SCENE_CLEARED_ROOM || type == FLAG_PERM_SCENE_CLEARED_ROOM) && flag < 32) ChangeBit(live.clearedRoom, flag, state);
    if (state) GameInteractor_ExecuteOnSceneFlagSet(scene, type, flag); else GameInteractor_ExecuteOnSceneFlagUnset(scene, type, flag);
}
