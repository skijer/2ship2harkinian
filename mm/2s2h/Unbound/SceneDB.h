#ifndef UNBOUND_SCENE_DB_H
#define UNBOUND_SCENE_DB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "z64scene.h"
#include "z64save.h"

// Scenes and entrances beyond the vanilla tables (unbound-docs/SPEC.md §7). Custom scene ids follow SCENE_MAX; custom
// entrances keep Majora's Mask's bit layout with a scene slot from ENTR_SCENE_MAX up, so `entrance >> 9`,
// `entrance + layer` and Entrance_Create keep working on them.
SceneTableEntry* SceneDB_GetSceneEntry(s16 sceneId);
const char* SceneDB_GetScenePath(s16 sceneId);
EntranceTableEntry* SceneDB_GetCustomEntrance(u32 entrance);

#ifdef __cplusplus
}

#include <optional>
#include <string>

void SceneDB_Init();
bool SceneDB_IsBaseMounted();
std::optional<u32> SceneDB_FindEntrance(const std::string& name);
std::string SceneDB_GetEntranceName(u32 entrance);
std::optional<s16> SceneDB_FindScene(const std::string& key);
std::string SceneDB_GetSceneKey(s16 sceneId);
#endif

#endif // UNBOUND_SCENE_DB_H
