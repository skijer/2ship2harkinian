#ifndef FLEET_RANDO_H
#define FLEET_RANDO_H

#include <fleet/FleetGameApi.h>
#include <nlohmann/json.hpp>

char* FleetRando_DumpStaticData(void);
void FleetRando_Reset(void);
void FleetRando_SetOwnedItems(const char* itemsJson);
char* FleetRando_GetReachableChecks(void);
bool FleetRando_PlaceItem(const char* check, const char* item);
char* FleetRando_Request(const char* op, const char* json);
void FleetRando_FreeString(char* text);
void FleetRando_ApplyCombinedSave(const nlohmann::json& seed);
bool FleetRando_IsOptionDrawnByFleet(const char* cvar);
#define FLEET_HIDDEN_SEED_GENERATION "Fleet.SeedGeneration"
#define FLEET_HIDDEN_SEED_HEALTH "Fleet.SeedHealth"
#define FLEET_HIDDEN_COMING_SOON "Fleet.ComingSoon"
#define FLEET_HIDDEN_STARTING_ITEMS "Fleet.StartingItems"

#endif
