#pragma once
#include <stdint.h>
#ifdef __cplusplus
#include <map>
#include <string>
#include <nlohmann/json.hpp>
#include "z64save.h"
#include "variables.h"
#include <vector>

void DungeonRegistry_Init();
void DungeonRegistry_Load();
void DungeonRegistry_BeginSeed();
void DungeonRegistry_EndSeed();
void DungeonRegistry_AssignEntrances(uint32_t seed);
int32_t DungeonRegistry_FindScene(int32_t scene);
int32_t DungeonRegistry_FindItem(int32_t item);
int32_t DungeonRegistry_ConfinedItem(int32_t item);
bool DungeonRegistry_ItemAllowed(int32_t item, bool dungeonCheck);
void DungeonRegistry_AdjustPool(std::vector<RandoItemId>& items);
bool DungeonRegistry_HasItem(int32_t item);
bool DungeonRegistry_AddCheckToPool(RandoSaveInfo& save, RandoCheckId check,
                                   std::vector<RandoCheckId>& checks, std::vector<RandoItemId>& items);
bool DungeonRegistry_GrantItem(int32_t item, bool remove = false);
bool DungeonRegistry_HasKeys(int32_t dungeon, int32_t count);
bool DungeonRegistry_HasBossKey(int32_t dungeon);
bool DungeonRegistry_GetEvent(int32_t dungeon, const std::string& name);
int32_t DungeonRegistry_FindRegion(const std::string& name);
int32_t DungeonRegistry_FindSymbol(const std::string& name);
void DungeonRegistry_WriteSpoiler(nlohmann::json& spoiler);
void DungeonRegistry_ReadSpoiler(const nlohmann::json& spoiler);
RandoSaveCheck& DungeonRegistry_SaveCheck(RandoSaveInfo& save, int32_t check);
uint8_t& DungeonRegistry_LogicEvent(int32_t event);
struct RandoSaveChecksView {
    RandoSaveCheck& operator[](int32_t check) const {
        return DungeonRegistry_SaveCheck(gSaveContext.save.shipSaveInfo.rando, check);
    }
};
struct RandoEventsView {
    uint8_t& operator[](int32_t event) const { return DungeonRegistry_LogicEvent(event); }
};
#endif

#ifdef __cplusplus
extern "C" {
#endif
int32_t DungeonRegistry_KeyIndex(int32_t scene);
uint32_t DungeonRegistry_ResolveEntrance(uint32_t entrance);
#ifdef __cplusplus
}
#endif
