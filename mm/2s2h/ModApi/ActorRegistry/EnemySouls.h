#ifndef S2H_MOD_API_ENEMY_SOULS_H
#define S2H_MOD_API_ENEMY_SOULS_H

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "ActorRegistry.h"
#include "2s2h/Rando/Types.h"
#include "z64save.h"

struct Actor;

// Fills the enemy's soul category and drop check, creating the soul item and drop check the definition asks for.
void EnemySouls_OnEnemyRegistered(const S2HActorDefinition* definition, S2HCustomEnemy& enemy);
void EnemySouls_AddToPool(const RandoSaveInfo& save, std::vector<RandoItemId>& itemPool);
bool EnemySouls_HasBosses();
bool EnemySouls_IsBossWithoutSoul(int16_t actorId);
// Clears or restores the boss's targetability; true while it must not update.
bool EnemySouls_HoldBossWithoutSoul(Actor* actor);
std::string EnemySouls_DescribeNativeSoul(const char* soulKey, const std::string& name);
std::string EnemySouls_DescribeCustomSoul(const char* soulKey, const std::string& fallbackName);
void EnemySouls_WriteSpoiler(nlohmann::json& spoiler);

#endif
