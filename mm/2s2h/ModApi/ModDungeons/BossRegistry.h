#ifndef S2H_BOSS_REGISTRY_H
#define S2H_BOSS_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    S2H_BOSS_SLOT_ODOLWA,
    S2H_BOSS_SLOT_GOHT,
    S2H_BOSS_SLOT_GYORG,
    S2H_BOSS_SLOT_TWINMOLD,
    S2H_BOSS_SLOT_MAJORA,
    S2H_BOSS_SLOT_MAX,
} S2HBossSlot;

#define S2H_BOSS_SLOT_BIT(slot) (1u << (slot))
#define S2H_BOSS_SLOTS_DUNGEON ((1u << S2H_BOSS_SLOT_MAJORA) - 1)
#define S2H_BOSS_SLOTS_FINAL S2H_BOSS_SLOT_BIT(S2H_BOSS_SLOT_MAJORA)

typedef struct {
    uint32_t structSize;
    const char* key;
    const char* displayName;
    const char* scene;
    uint32_t allowedSlots;
    int32_t preferredSlot;
    const char* logic;
    const char* enabledCvar;
} S2HBossDefinition;

#ifdef __cplusplus
extern "C" {
#endif
typedef struct Actor Actor;
bool BossRegistry_Register(const S2HBossDefinition* definition);
int32_t BossRegistry_GetSlot(const char* key);
const char* BossRegistry_GetSceneBoss(int16_t sceneId);
int16_t BossRegistry_GetSlotScene(int32_t slot);
void BossRegistry_ReportDefeated(Actor* boss);
int32_t BossRegistry_ResolveSceneSource(int32_t sceneId);
#ifdef __cplusplus
}
#ifndef S2H_MOD
#include <string>
#include <nlohmann/json.hpp>
void BossRegistry_Init();
void BossRegistry_SetCurrentMod(const std::string& name);
void BossRegistry_AssignForSeed(uint32_t seed);
void BossRegistry_WriteSpoiler(nlohmann::json& spoiler);
void BossRegistry_ParseSpoiler(const nlohmann::json& spoiler);
#endif
#endif
#endif
