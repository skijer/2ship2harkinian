#ifndef S2H_Z64BOSS_H
#define S2H_Z64BOSS_H

#include <string.h>

#include "2s2h/ModApi/ModApi.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_Door_Warp1/z_door_warp1.h"
#include "overlays/actors/ovl_Item_B_Heart/z_item_b_heart.h"
#ifdef __cplusplus
}
#endif

static inline bool Z64Boss_IsFightHere(const S2HModApi* api, const char* key, PlayState* play) {
    if (!S2H_MOD_API_HAS(api, GetSceneBoss) || play == NULL || key == NULL) {
        return false;
    }
    const char* owner = api->GetSceneBoss(play->sceneId);
    return owner != NULL && strcmp(owner, key) == 0;
}

static inline int32_t Z64Boss_GetSceneSlot(const S2HModApi* api, PlayState* play) {
    for (int32_t slot = 0; slot < S2H_BOSS_SLOT_MAX; slot++) {
        if (api->GetBossSlotScene(slot) == play->sceneId) {
            return slot;
        }
    }
    return -1;
}

static inline void Z64Boss_RollCredits(PlayState* play) {
    play->nextEntrance = ENTRANCE(TERMINA_FIELD, 0);
    gSaveContext.nextCutsceneIndex = 0xFFF7;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_WHITE;
}

static inline void Z64Boss_SpawnRewards(PlayState* play, Actor* boss, Vec3f warpPos, Vec3f heartPos) {
    Actor_SpawnAsChild(&play->actorCtx, boss, play, ACTOR_DOOR_WARP1, warpPos.x, warpPos.y, warpPos.z, 0, 0, 0,
                       ENDOORWARP1_FF_1);
    Actor_Spawn(&play->actorCtx, play, ACTOR_ITEM_B_HEART, heartPos.x, heartPos.y, heartPos.z, 0, 0, 0,
                BHEART_PARAM_NORMAL);
}

static inline void Z64Boss_Finish(const S2HModApi* api, PlayState* play, Actor* boss, Vec3f warpPos,
                                  Vec3f heartPos) {
    if (!S2H_MOD_API_HAS(api, ReportBossDefeated) || play == NULL || boss == NULL) {
        return;
    }
    const int32_t slot = Z64Boss_GetSceneSlot(api, play);
    Flags_SetClear(play, play->roomCtx.curRoom.num);
    api->ReportBossDefeated(boss);
    if (slot < 0) {
        return;
    }
    if (slot == S2H_BOSS_SLOT_MAJORA) {
        Z64Boss_RollCredits(play);
        return;
    }
    Z64Boss_SpawnRewards(play, boss, warpPos, heartPos);
}

#endif
