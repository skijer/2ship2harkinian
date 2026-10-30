#pragma once

#include <set>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "2s2h/Rando/Logic/Logic.h"

// Cross-game entrance routing for a combined seed: the entrances 2Ship offers to the shared pools, and the
// routes Fleet installs back. Every call throws std::exception on a malformed request.
nlohmann::json FleetCrossDungeon_DumpCatalogue(const nlohmann::json& options);
nlohmann::json FleetCrossDungeon_DumpCatalogueForSeed(const nlohmann::json& spoiler);
void FleetCrossDungeon_SetSeed(const nlohmann::json& seed);
void FleetCrossDungeon_SetAccess(const nlohmann::json& access);
nlohmann::json FleetCrossDungeon_GetExits();
bool FleetCrossDungeon_CanEnterArrival(int32_t entrance);

// While alive, logic follows the installed routes. Roots() gives where a search starts, and Remember() keeps
// what it reached for GetExits.
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
