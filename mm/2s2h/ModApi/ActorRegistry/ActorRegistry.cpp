#include "ActorRegistry.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include "2s2h/ActorTable/CustomActorTable.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ModApi/ModApi.h"
#include "2s2h/ModApi/ModMessages/ModMessages.h"
#include "2s2h/ShipUtils.h"

extern "C" {
#include "z64.h"
}

namespace {

std::unordered_map<std::string, int16_t> sIdsByKey;
std::unordered_map<std::string, std::string> sOwners;
std::deque<std::string> sNames;
std::vector<S2HCustomEnemy> sEnemies;

int16_t FindVanillaId(const std::string& debugName) {
    for (s16 actorId = 0; actorId < ACTOR_ID_MAX; actorId++) {
        if (ActorOverlayTable_Get(actorId) != nullptr && GetActorDebugName(actorId) == debugName) {
            return actorId;
        }
    }
    return -1;
}

bool IsValidDefinition(const S2HActorDefinition* definition) {
    return definition != nullptr && definition->key != nullptr && definition->key[0] != '\0' &&
           definition->structSize >= S2H_ACTOR_DEFINITION_MIN_SIZE && definition->init != nullptr;
}

bool IsEnemyDefinition(const S2HActorDefinition* definition) {
    return definition->structSize >= S2H_ACTOR_DEFINITION_ENEMY_SIZE && (definition->enemyFlags & S2H_ACTOR_ENEMY);
}

ActorProfile ToProfile(const S2HActorDefinition* definition) {
    ActorProfile profile = {};
    profile.type = definition->category;
    profile.flags = definition->actorFlags;
    profile.objectId = definition->objectId;
    profile.instanceSize = definition->instanceSize != 0 ? definition->instanceSize : sizeof(Actor);
    profile.init = (ActorFunc)definition->init;
    profile.destroy = (ActorFunc)definition->destroy;
    profile.update = (ActorFunc)definition->update;
    profile.draw = (ActorFunc)definition->draw;
    if (definition->structSize >= S2H_ACTOR_DEFINITION_RESET_SIZE) {
        profile.reset = definition->reset;
    }
    return profile;
}

const char* StoreName(const char* name) {
    return sNames.emplace_back(name).c_str();
}

void AddEnemy(const S2HActorDefinition* definition, int16_t actorId) {
    S2HCustomEnemy enemy = {};
    enemy.key = StoreName(definition->key);
    enemy.name = definition->description != nullptr ? StoreName(definition->description) : enemy.key;
    enemy.actorId = actorId;
    enemy.params = definition->enemyParams;
    enemy.flags = definition->enemyFlags;
    enemy.spawnHeight = definition->enemySpawnHeight;

    auto position = std::lower_bound(
        sEnemies.begin(), sEnemies.end(), enemy,
        [](const S2HCustomEnemy& left, const S2HCustomEnemy& right) { return strcmp(left.key, right.key) < 0; });
    sEnemies.insert(position, enemy);
}

// An actor that set its own hintId in init keeps it.
void AttachTatlHint(const S2HActorDefinition* definition, int16_t actorId) {
    if (definition->structSize < S2H_ACTOR_DEFINITION_HINT_SIZE || definition->naviHint == nullptr) {
        return;
    }
    const uint8_t hintId = ModMessages_RegisterTatlHint(definition->key, definition->naviHint);
    if (hintId == TATL_HINT_ID_NONE) {
        return;
    }
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorInit>(actorId, [hintId](Actor* actor) {
        if (actor->hintId == TATL_HINT_ID_NONE) {
            actor->hintId = hintId;
        }
    });
}

} // namespace

int16_t ActorRegistry_GetId(const char* key) {
    if (key == nullptr) {
        return -1;
    }
    const int16_t customId = ActorRegistry_FindCustomId(key);
    return customId >= 0 ? customId : FindVanillaId(key);
}

int16_t ActorRegistry_FindCustomId(const char* key) {
    if (key == nullptr) {
        return -1;
    }
    auto registered = sIdsByKey.find(key);
    return registered == sIdsByKey.end() ? -1 : registered->second;
}

int16_t ActorRegistry_Register(const S2HActorDefinition* definition) {
    if (!IsValidDefinition(definition)) {
        return -1;
    }
    const int16_t alreadyRegisteredId = ActorRegistry_FindCustomId(definition->key);
    if (alreadyRegisteredId >= 0) {
        return alreadyRegisteredId;
    }

    const std::string description = definition->description != nullptr ? definition->description : definition->key;
    const int16_t actorId = ActorOverlayTable_Add(ToProfile(definition), definition->key, description);
    if (actorId < 0) {
        SPDLOG_ERROR("[ActorRegistry] No actor ids left for '{}'", definition->key);
        return -1;
    }
    sIdsByKey.emplace(definition->key, actorId);
    sOwners.emplace(definition->key, ModApi_GetCurrentMod());
    if (IsEnemyDefinition(definition)) {
        AddEnemy(definition, actorId);
    }
    AttachTatlHint(definition, actorId);
    return actorId;
}

uint32_t ActorRegistry_GetEnemyCount(void) {
    return (uint32_t)sEnemies.size();
}

const S2HCustomEnemy* ActorRegistry_GetEnemyAt(uint32_t index) {
    return index < sEnemies.size() ? &sEnemies[index] : nullptr;
}

const S2HCustomEnemy* ActorRegistry_FindEnemyById(int16_t actorId) {
    auto enemy = std::find_if(sEnemies.begin(), sEnemies.end(),
                              [actorId](const S2HCustomEnemy& candidate) { return candidate.actorId == actorId; });
    return enemy == sEnemies.end() ? nullptr : &*enemy;
}

std::vector<S2HRegisteredActor> ActorRegistry_List() {
    std::vector<S2HRegisteredActor> actors;
    for (const auto& [key, actorId] : sIdsByKey) {
        actors.push_back({ key, sOwners[key], actorId });
    }
    std::sort(actors.begin(), actors.end(),
              [](const S2HRegisteredActor& left, const S2HRegisteredActor& right) { return left.key < right.key; });
    return actors;
}
