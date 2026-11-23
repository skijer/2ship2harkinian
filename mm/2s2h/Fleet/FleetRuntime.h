#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <fleet/FleetGameApi.h>

bool FleetRuntime_HasSession();
const nlohmann::json& FleetRuntime_Seed();
int FleetRuntime_Slot();
void FleetRuntime_Initialize();
void FleetRuntime_FrameEnd();
void FleetRuntime_CatchUpParked();
nlohmann::json FleetRuntime_Status();
nlohmann::json FleetRuntime_ReadOwnership(const nlohmann::json& items);
bool FleetRuntime_CollectForSmoke(const std::string& check);
nlohmann::json FleetRuntime_CycleSmoke();
void FleetRuntime_Save();
// Blocks until queued saves reach disk; required before the host reads or copies the save folder.
void FleetRuntime_FinishSaving();
double FleetRuntime_SlowestSaveMilliseconds();
void FleetRuntime_InstallRoutes();
int32_t FleetRuntime_Route(int32_t entrance, bool beforeNative);
bool FleetRuntime_TakeDeparture(FleetDestination& destination, std::string& payload);
bool FleetRuntime_QueueArrival(int32_t entrance);
int32_t FleetRuntime_ConsumeArrival();
int32_t FleetRuntime_Materialize(int32_t entrance);
void FleetRuntime_SkipNextRoute();
void FleetRuntime_ObserveScene(int32_t scene);
