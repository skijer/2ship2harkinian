#ifndef ENEMY_RANDOMIZER_H
#define ENEMY_RANDOMIZER_H

#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "2s2h/ModApi/ActorRegistry/ActorRegistry.h"

#define ENEMY_POOL_EXCLUDED_CVAR "gRando.EnemyPoolExcluded"
#define SOUL_POOL_EXCLUDED_CVAR "gRando.SoulPoolExcluded"

namespace EnemyRandomizer {

struct LogicReplacement {
    int16_t actorId;
    const S2HCustomEnemy* custom;
    bool onlySubmerged;
};

struct PoolEntry {
    std::string key;
    std::string name;
};

void AssignForSeed(uint32_t seed);
void WriteSpoiler(nlohmann::json& spoiler);
void ReadSpoiler(const nlohmann::json& spoiler);

LogicReplacement FindLogicReplacement(int16_t sceneId, int16_t originalActorId);
bool IsSoulInPool(const char* soulKey);
bool IsCustomSoulItemExcluded(int32_t randoItem);

std::vector<PoolEntry> ListEnemyPool(bool minibosses);
std::vector<PoolEntry> ListSoulPool();
bool IsKeyExcluded(const char* cvar, const std::string& key);
void SetKeyExcluded(const char* cvar, const std::string& key, bool excluded);

} // namespace EnemyRandomizer

#endif // ENEMY_RANDOMIZER_H
