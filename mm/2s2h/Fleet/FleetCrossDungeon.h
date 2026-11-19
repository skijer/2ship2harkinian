#pragma once

#include <set>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "2s2h/Rando/Logic/Logic.h"

nlohmann::json FleetCrossDungeon_DumpCatalogue(const nlohmann::json& options);
nlohmann::json FleetCrossDungeon_DumpCatalogueForSeed(const nlohmann::json& spoiler);
void FleetCrossDungeon_SetSeed(const nlohmann::json& seed);
void FleetCrossDungeon_SetAccess(const nlohmann::json& access);
void FleetCrossDungeon_Reset();
nlohmann::json FleetCrossDungeon_GetExits();
bool FleetCrossDungeon_CanEnterArrival(int32_t entrance);
uint16_t FleetCrossDungeon_GetBossDefeated();

class FleetCrossDungeonSearch {
  public:
    FleetCrossDungeonSearch();
    ~FleetCrossDungeonSearch();
    FleetCrossDungeonSearch(const FleetCrossDungeonSearch&) = delete;
    FleetCrossDungeonSearch& operator=(const FleetCrossDungeonSearch&) = delete;

    void Roots(std::set<RandoRegionId>& reachable,
               std::unordered_map<RandoRegionId, Rando::Logic::RegionTimeState>& timeStates) const;
    void Remember(const std::set<RandoRegionId>& reachable,
                  const std::unordered_map<RandoRegionId, Rando::Logic::RegionTimeState>& timeStates) const;

  private:
    bool mPreviousSearching;
};
