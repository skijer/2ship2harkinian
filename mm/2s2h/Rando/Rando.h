#ifndef RANDO_H
#define RANDO_H

#include <optional>
#include <vector>

#include "StaticData/StaticData.h"
#include "Types.h"
#include "variables.h"

#define IS_RANDO (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO)
#include "2s2h/ModApi/ModDungeons/DungeonRegistry.h"
#define RANDO_SAVE_CHECKS RandoSaveChecksView{}
#define RANDO_SAVE_OPTIONS gSaveContext.save.shipSaveInfo.rando.randoSaveOptions
#define RANDO_EVENTS RandoEventsView{}

namespace Rando {

void Init();
void DrawItem(RandoItemId randoItemId, RandoCheckId randoCheckId = RC_UNKNOWN, Actor* actor = nullptr);
void GiveItem(RandoItemId randoItemId);
void RemoveItem(RandoItemId randoItemId);
RandoItemId CurrentJunkItem(RandoCheckId randoCheckId = RC_UNKNOWN);
const std::vector<RandoItemId>& JunkRotation();
u32 GetItemHintWeight(RandoItemId randoItemId);
RandoItemId CurrentTrapItem(RandoCheckId randoCheckId = RC_UNKNOWN);
bool IsItemObtainable(RandoItemId randoItemId, RandoCheckId randoCheckId = RC_UNKNOWN);
RandoItemId ConvertItem(RandoItemId randoItemId, RandoCheckId randoCheckId = RC_UNKNOWN);
RandoCheckId FindItemPlacement(RandoItemId randoItemId);
void RegisterMenu();

std::vector<RandoItemId> GetComputedStartingItems(RandoSaveInfo& randoSaveInfo);
void GrantStartingItems();
std::vector<RandoItemId> GetStartingItemsFromSpoiler(nlohmann::json& spoiler);
void SetStartingItemsInSpoiler(nlohmann::json& spoiler, std::vector<RandoItemId>& startingItems);
std::vector<RandoItemId> GetStartingItemsFromSave(RandoSaveInfo& randoSaveInfo);
void SetStartingItemsInSave(RandoSaveInfo& randoSaveInfo, std::vector<RandoItemId>& startingItems);
std::vector<RandoItemId> GetStartingItemsFromConfig();
void SetStartingItemsInConfig(std::vector<RandoItemId>& startingItems);
// While set, the config getters return a seed's own list instead of the player's; it is never saved.
void OverrideStartingItems(std::optional<std::vector<RandoItemId>> startingItems);

std::vector<RandoItemId> GetDefaultSariaPriorityItems();
std::vector<RandoItemId> GetSariaPriorityItemsFromSpoiler(nlohmann::json& spoiler);
void SetSariaPriorityItemsInSpoiler(nlohmann::json& spoiler, std::vector<RandoItemId>& priorityItems);
std::vector<RandoItemId> GetSariaPriorityItemsFromSave(RandoSaveInfo& randoSaveInfo);
void SetSariaPriorityItemsInSave(RandoSaveInfo& randoSaveInfo, std::vector<RandoItemId>& priorityItems);
std::vector<RandoItemId> GetSariaPriorityItemsFromConfig();
void SetSariaPriorityItemsInConfig(std::vector<RandoItemId>& priorityItems);
std::vector<RandoItemId> GetSariaPriorityItemCandidates();

std::vector<RandoCheckId> GetExcludedChecksFromConfig();
void SetExcludedChecksInConfig(std::vector<RandoCheckId>& excludedChecks);
void OverrideExcludedChecks(std::optional<std::vector<RandoCheckId>> excludedChecks);

std::vector<RandoCheckId> FindMultiItemPlacement(RandoItemId randoItemId);

} // namespace Rando

#endif
