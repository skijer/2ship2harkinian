#ifndef S2H_MOD_API_ACTOR_REGISTRY_H
#define S2H_MOD_API_ACTOR_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Actor;
struct PlayState;

typedef void (*S2HActorFunc)(struct Actor* actor, struct PlayState* play);
typedef void (*S2HActorResetFunc)(void);

#define S2H_ACTOR_ENEMY (1 << 0)
#define S2H_ACTOR_ENEMY_NOT_IN_CLEAR_ROOMS (1 << 1)
#define S2H_ACTOR_ENEMY_NOT_IN_TIMED_ROOMS (1 << 2)

// Ship's fields after reset, which 2Ship shipped first: same names, appended so older mods keep their layout.
typedef struct {
    uint32_t structSize;
    const char* key;
    const char* description;
    uint8_t category;
    uint32_t actorFlags;
    int16_t objectId;
    uint32_t instanceSize;
    S2HActorFunc init;
    S2HActorFunc destroy;
    S2HActorFunc update;
    S2HActorFunc draw;
    // Clears the mod's static state once the last instance is gone, like the port's vanilla actors.
    S2HActorResetFunc reset;
    uint32_t enemyFlags;
    int16_t enemyParams;
    int16_t enemySpawnHeight;
    // Tatl's line when C-up is pressed on the targeted actor; Ship's name kept for shared mod sources.
    const char* naviHint;
} S2HActorDefinition;

#define S2H_ACTOR_DEFINITION_MIN_SIZE (offsetof(S2HActorDefinition, draw) + sizeof(((S2HActorDefinition*)0)->draw))
#define S2H_ACTOR_DEFINITION_RESET_SIZE \
    (offsetof(S2HActorDefinition, reset) + sizeof(((S2HActorDefinition*)0)->reset))
#define S2H_ACTOR_DEFINITION_ENEMY_SIZE \
    (offsetof(S2HActorDefinition, enemySpawnHeight) + sizeof(((S2HActorDefinition*)0)->enemySpawnHeight))
#define S2H_ACTOR_DEFINITION_HINT_SIZE \
    (offsetof(S2HActorDefinition, naviHint) + sizeof(((S2HActorDefinition*)0)->naviHint))

typedef struct {
    const char* key;
    const char* name;
    int16_t actorId;
    int16_t params;
    uint32_t flags;
    int16_t spawnHeight;
} S2HCustomEnemy;

// A registered key returns its runtime id; vanilla actors answer to their debug name ("En_Torch2").
int16_t ActorRegistry_Register(const S2HActorDefinition* definition);
int16_t ActorRegistry_GetId(const char* key);
int16_t ActorRegistry_FindCustomId(const char* key);
uint32_t ActorRegistry_GetEnemyCount(void);
const S2HCustomEnemy* ActorRegistry_GetEnemyAt(uint32_t index);
const S2HCustomEnemy* ActorRegistry_FindEnemyById(int16_t actorId);

#ifdef __cplusplus
}

#include <string>
#include <vector>

// Every actor type a mod registered, with the mod that registered it.
struct S2HRegisteredActor {
    std::string key;
    std::string owner;
    int16_t actorId;
};

std::vector<S2HRegisteredActor> ActorRegistry_List();
#endif

#endif
