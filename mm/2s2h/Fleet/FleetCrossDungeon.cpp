#include "FleetCrossDungeon.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <fleet/rando/CrossDungeonState.h>
#include <fleet/rando/CrossEntranceTime.h>

#include "CrossEntranceMmCatalogue.h"

namespace {

using namespace ComboRando::CrossDungeon;

constexpr int kMm = 1;

HostState sState(kMm);
bool sSearching = false;
std::set<RandoRegionId> sLastReachable;
std::unordered_map<RandoRegionId, Rando::Logic::RegionTimeState> sLastTimeStates;

// Grotto portals use logical identities the native entrance table does not know.
RandoRegionId RegionForEntrance(int32_t entrance) {
    if (const RandoRegionId region = ComboCrossMmCatalogue::RegionForGrottoReturn(entrance); region != RR_MAX) {
        return region;
    }
    if (const RandoRegionId region = ComboCrossMmCatalogue::RegionForGrottoEntry(entrance); region != RR_MAX) {
        return region;
    }
    return Rando::Logic::GetRegionIdFromEntrance(entrance);
}

// A grotto's exit is keyed by its logical identity, whichever native entrance it physically uses.
const Route* RouteForExit(s32 exitId, RandoRegionId fromRegion) {
    const int32_t grottoExit = ComboCrossMmCatalogue::GrottoExitForRegion(fromRegion);
    return sState.Lookup(grottoExit >= 0 ? grottoExit : exitId, exitId);
}

// A route into the other game leaves 2Ship's graph.
std::optional<RandoRegionId> RerouteDuringSearch(s32 exitId, RandoRegionId fromRegion) {
    if (!sSearching) {
        return std::nullopt;
    }
    const Route* route = RouteForExit(exitId, fromRegion);
    if (route == nullptr) {
        return std::nullopt;
    }
    return route->to.game == kMm ? RegionForEntrance(route->to.index) : RR_MAX;
}

std::vector<Dungeon> Catalogue(const PoolOptions& options) {
    if (options.bosses) {
        throw std::invalid_argument("cross-game boss entrances are not served by 2Ship yet");
    }
    std::vector<Dungeon> dungeons;
    if (options.dungeons) {
        dungeons = { { "mm:woodfall",
                       { kMm, ENTRANCE(WOODFALL_TEMPLE, 0) },
                       { kMm, ENTRANCE(WOODFALL, 1) },
                       { kMm, ENTRANCE(WOODFALL, 1) } },
                     { "mm:snowhead",
                       { kMm, ENTRANCE(SNOWHEAD_TEMPLE, 0) },
                       { kMm, ENTRANCE(SNOWHEAD, 1) },
                       { kMm, ENTRANCE(SNOWHEAD, 1) } },
                     { "mm:greatbay",
                       { kMm, ENTRANCE(GREAT_BAY_TEMPLE, 0) },
                       { kMm, ENTRANCE(ZORA_CAPE, 7) },
                       { kMm, ENTRANCE(ZORA_CAPE, 7) } },
                     { "mm:stonetower",
                       { kMm, ENTRANCE(STONE_TOWER_TEMPLE, 0) },
                       { kMm, ENTRANCE(STONE_TOWER, 2) },
                       { kMm, ENTRANCE(STONE_TOWER, 2) } },
                     { "mm:inverted",
                       { kMm, ENTRANCE(STONE_TOWER_TEMPLE_INVERTED, 0) },
                       { kMm, ENTRANCE(STONE_TOWER_INVERTED, 1) },
                       { kMm, ENTRANCE(STONE_TOWER_INVERTED, 1) } } };
        // The boss warps that leave each temple without its front door.
        dungeons[0].escapeExits = { { kMm, ENTRANCE(WOODFALL, 3) } };
        dungeons[1].escapeExits = { { kMm, ENTRANCE(MOUNTAIN_VILLAGE_SPRING, 7) } };
        dungeons[2].escapeExits = { { kMm, ENTRANCE(ZORA_CAPE, 9) } };
        dungeons[4].escapeExits = { { kMm, ENTRANCE(IKANA_CANYON, 15) } };
    }
    ComboCrossMmCatalogue::Append(dungeons, options);
    Validate(dungeons);
    return dungeons;
}

nlohmann::json CatalogueJson(const PoolOptions& options) {
    nlohmann::json out = nlohmann::json::array();
    for (const Dungeon& dungeon : Catalogue(options)) {
        out.push_back(DungeonJson(dungeon));
    }
    return out;
}

} // namespace

nlohmann::json FleetCrossDungeon_DumpCatalogue(const nlohmann::json& options) {
    return CatalogueJson(ParseOptions(options));
}

nlohmann::json FleetCrossDungeon_DumpCatalogueForSeed(const nlohmann::json& spoiler) {
    return CatalogueJson(OptionsFromSeed(spoiler.value("crossDungeonEntrances", nlohmann::json{})));
}

void FleetCrossDungeon_SetSeed(const nlohmann::json& seed) {
    for (const Route& route : Decode(seed)) {
        // The vanilla one-way Goht exit has no region of its own; it stays valid as a source.
        const bool gohtEscape = route.phase == Phase::BeforeNative &&
                                route.from == Endpoint{ kMm, ENTRANCE(MOUNTAIN_VILLAGE_SPRING, 7) };
        if (route.from.game == kMm && !gohtEscape && RegionForEntrance(route.from.index) == RR_MAX) {
            throw std::invalid_argument("cross-dungeon seed names unknown MM source " +
                                        std::to_string(route.from.index));
        }
        if (route.to.game == kMm && RegionForEntrance(route.to.index) == RR_MAX) {
            throw std::invalid_argument("cross-dungeon seed names unknown MM destination " +
                                        std::to_string(route.to.index));
        }
    }
    sState.Install(seed);
    Rando::Logic::SetExitRedirect(RerouteDuringSearch);
}

void FleetCrossDungeon_SetAccess(const nlohmann::json& access) {
    sState.SetAccess(access);
}

// Which routes into the other game the last search can take, judged on their native exits.
nlohmann::json FleetCrossDungeon_GetExits() {
    const bool previousSearching = sSearching;
    sSearching = true;
    nlohmann::json exits = nlohmann::json::array();
    for (RandoRegionId region : sLastReachable) {
        Rando::Logic::SetCurrentRegionTime(sLastTimeStates, region);
        for (auto& [exitId, regionExit] : Rando::Logic::Regions.at(region).exits) {
            const Route* route = RouteForExit(exitId, region);
            if (route != nullptr && route->to.game != kMm && regionExit.condition()) {
                exits.push_back(EndpointJson(route->to));
            }
        }
    }
    sSearching = previousSearching;
    return exits;
}

// Raw owned half-days on purpose: time logic's synthetic day-one fallback does not count.
bool FleetCrossDungeon_CanEnterArrival(int32_t entrance) {
    if (entrance < 0 || entrance > 0xFFFF || Rando::Logic::GetRegionIdFromEntrance(entrance) == RR_MAX) {
        return false;
    }
    uint8_t ownedHalfDays = 0;
    for (int halfDay = 0; halfDay < 6; halfDay++) {
        if (Rando::Logic::OwnsClockHalfDay(halfDay)) {
            ownedHalfDays |= 1u << halfDay;
        }
    }
    return MmForeignArrivalAllowed(CAN_PLAY_SONG(TIME), Rando::Logic::SettingClocks(), ownedHalfDays);
}

FleetCrossDungeonSearch::FleetCrossDungeonSearch() : mPreviousSearching(sSearching) {
    sSearching = true;
}

FleetCrossDungeonSearch::~FleetCrossDungeonSearch() {
    sSearching = mPreviousSearching;
}

// Without routes, or with the overworld open, the search starts from the game's own start. A cross arrival
// keeps its native region's waiting restrictions, unlike that synthetic start.
void FleetCrossDungeonSearch::Roots(std::set<RandoRegionId>& reachable,
                                    std::unordered_map<RandoRegionId, Rando::Logic::RegionTimeState>& timeStates) const {
    if (sState.routes.empty() || sState.overworld) {
        reachable.insert(RR_MAX);
        timeStates = Rando::Logic::InitializeRegionTimeStates(RR_MAX);
    }
    for (int32_t entrance : sState.arrivals) {
        const RandoRegionId region = RegionForEntrance(entrance);
        reachable.insert(region);
        for (auto& [arrivalRegion, incoming] : Rando::Logic::InitializeRegionTimeStates(region)) {
            incoming.canStayOverTime = Rando::Logic::Regions.at(arrivalRegion).canStayOverTime;
            const auto [existing, inserted] = timeStates.emplace(arrivalRegion, incoming);
            if (!inserted) {
                existing->second = Rando::Logic::MergeTimeStates(existing->second, incoming);
            }
        }
    }
}

void FleetCrossDungeonSearch::Remember(
    const std::set<RandoRegionId>& reachable,
    const std::unordered_map<RandoRegionId, Rando::Logic::RegionTimeState>& timeStates) const {
    sLastReachable = reachable;
    sLastTimeStates = timeStates;
}
