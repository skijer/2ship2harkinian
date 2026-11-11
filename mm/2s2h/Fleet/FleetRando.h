#ifndef FLEET_RANDO_H
#define FLEET_RANDO_H

#include <fleet/FleetGameApi.h>
#include <nlohmann/json.hpp>

// 2Ship's half of Fleet's combined randomizer: the reachability oracle and the services behind RandoRequest.
char* FleetRando_DumpStaticData(void);
void FleetRando_Reset(void);
void FleetRando_SetOwnedItems(const char* itemsJson);
char* FleetRando_GetReachableChecks(void);
bool FleetRando_PlaceItem(const char* check, const char* item);
char* FleetRando_Request(const char* op, const char* json);
void FleetRando_FreeString(char* text);
void FleetRando_ApplyCombinedSave(const nlohmann::json& seed);
// True for an option Fleet draws once for both games through OoT's control.
bool FleetRando_IsOptionDrawnByFleet(const char* cvar);
// Parts of the native rando menu with no meaning in a combined seed, named in the same hidden set.
#define FLEET_HIDDEN_SEED_GENERATION "Fleet.SeedGeneration"
#define FLEET_HIDDEN_SEED_HEALTH "Fleet.SeedHealth"
#define FLEET_HIDDEN_COMING_SOON "Fleet.ComingSoon"

#endif
