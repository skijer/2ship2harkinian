#include "EnemyDropLogic.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Logic.h"
#include "2s2h/ModApi/RandoLogic/RandoLogic.h"
#include "2s2h/Rando/ActorBehavior/ActorBehavior.h"

namespace Rando {

namespace Logic {

DropStandIn gDropStandIn = { -1, -1 };

namespace {

using CheckLogic = std::pair<std::function<bool()>, std::string>;

// Woodfall's never-replaced Gekko brings its Snapper at runtime, which the spawn table does not list.
const std::set<RandoCheckId> sDropsWithOwnLogic = { RC_ENEMY_DROP_SNAPPER };

std::vector<std::pair<RandoRegionId, RandoCheckId>> sRelocatedDrops;
std::set<RandoCheckId> sEarnableDrops;

// Looked up without operator[]: regions also hold mod checks that have no static entry.
bool IsDropCheck(RandoCheckId check) {
    const auto staticCheck = Rando::StaticData::Checks.find(check);
    return staticCheck != Rando::StaticData::Checks.end() && staticCheck->second.randoCheckType == RCTYPE_ENEMY_DROP;
}

void RemoveRelocatedDrops() {
    for (const auto& [regionId, check] : sRelocatedDrops) {
        Regions[regionId].checks.erase(check);
    }
    sRelocatedDrops.clear();
}

// Keeps the copied drop's time of day and reach requirements; only its kill becomes the spawned actor's.
CheckLogic StandInLogic(const CheckLogic& original, int16_t originalActorId, int16_t spawnedActorId) {
    std::function<bool()> standInCondition = [originalCondition = original.first, originalActorId, spawnedActorId] {
        const DropStandIn previous = gDropStandIn;
        gDropStandIn = { originalActorId, spawnedActorId };
        const bool isMet = originalCondition();
        gDropStandIn = previous;
        return isMet;
    };
    return { standInCondition, original.second + " [as " + std::to_string(spawnedActorId) + "]" };
}

void RelocateDropsInRegion(RandoRegionId regionId, RandoRegion& region) {
    std::vector<std::pair<RandoCheckId, CheckLogic>> relocated;
    for (const EnemyRandomizer::SceneReplacement& entry : EnemyRandomizer::ListSceneReplacements(region.sceneId)) {
        const RandoCheckId originalDrop = ActorBehavior::GetEnemyDropCheck(entry.originalActorId);
        const RandoCheckId spawnedDrop = ActorBehavior::GetEnemyDropCheck(entry.replacement.actorId);
        if (originalDrop == RC_UNKNOWN || spawnedDrop == RC_UNKNOWN || spawnedDrop == originalDrop) {
            continue;
        }
        const auto original = region.checks.find(originalDrop);
        if (original == region.checks.end() || region.checks.contains(spawnedDrop)) {
            continue;
        }
        relocated.push_back(
            { spawnedDrop, StandInLogic(original->second, entry.originalActorId, entry.replacement.actorId) });
    }
    for (auto& [check, logic] : relocated) {
        if (region.checks.emplace(check, std::move(logic)).second) {
            sRelocatedDrops.push_back({ regionId, check });
        }
    }
}

bool CanSpawnInScene(RandoCheckId check, SceneId sceneId) {
    for (int16_t actorId : ActorBehavior::GetEnemyDropActors(check)) {
        if (!EnemyRandomizer::FindSpawnsOfActor(sceneId, actorId).empty()) {
            return true;
        }
    }
    return false;
}

bool IsRelocatedTo(RandoRegionId regionId, RandoCheckId check) {
    return std::find(sRelocatedDrops.begin(), sRelocatedDrops.end(), std::make_pair(regionId, check)) !=
           sRelocatedDrops.end();
}

void CollectEarnableDrops() {
    for (const auto& [regionId, region] : Regions) {
        for (const auto& [check, logic] : region.checks) {
            if (IsDropCheck(check) && (IsRelocatedTo(regionId, check) || CanSpawnInScene(check, region.sceneId))) {
                sEarnableDrops.insert(check);
            }
        }
    }
}

void RelocateDropsForPlacement() {
    RemoveRelocatedDrops();
    sEarnableDrops.clear();
    if (!EnemyRandomizer::IsPlacementActive()) {
        return;
    }
    for (auto& [regionId, region] : Regions) {
        RelocateDropsInRegion(regionId, region);
    }
    CollectEarnableDrops();
}

} // namespace

void RefreshEnemyDropLogic() {
    RandoLogic_EditWorld(RelocateDropsForPlacement);
}

// Drops without an actor profile (cutscene deaths: Captain Keeta, Gomess, Igos) keep their own logic.
bool IsEnemyDropEarnable(RandoCheckId check) {
    if (!EnemyRandomizer::IsPlacementActive() || ActorBehavior::GetEnemyDropActors(check).empty() ||
        sDropsWithOwnLogic.contains(check)) {
        return true;
    }
    return sEarnableDrops.contains(check);
}

} // namespace Logic

} // namespace Rando
