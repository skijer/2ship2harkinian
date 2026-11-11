#ifndef FLEET_GAME_H
#define FLEET_GAME_H

#include <stdbool.h>

#include <fleet/FleetGameApi.h>

#ifdef __cplusplus
#include <memory>
#include <string>

namespace Ship {
class ResourceManager;
}

// Gives 2ship its own resources inside the Context Fleet created.
std::shared_ptr<Ship::ResourceManager> FleetGame_CreateResources(const std::string& archivePath);
// What keeps 2ship from booting from its install folder, in words a player can act on; empty when nothing does.
std::string S2H_InstallProblem(const std::string& directory);
// The online room 2ship joins: inside Fleet each game keeps its own room, so packets never reach the other game.
std::string FleetGame_NetworkRoom(const std::string& room);

// Saves Fleet writes on its own (crossings, deliveries, a file created on arrival) never confirm the player's save.
class FleetAutomaticSave {
  public:
    FleetAutomaticSave();
    ~FleetAutomaticSave();
    FleetAutomaticSave(const FleetAutomaticSave&) = delete;
    FleetAutomaticSave& operator=(const FleetAutomaticSave&) = delete;
};

extern "C" {
#endif

// True while 2ship runs inside Fleet rather than as its own program.
bool FleetGame_IsHosted(void);
void* FleetGame_GetHostFunction(const char* name);
const char* FleetGame_GetSaveDirectory(void);
bool FleetGame_FileSelect(void);
bool FleetGame_SelectFile(int slot);
bool FleetGame_HasSaveSlot(int slot);
bool FleetGame_ManageSave(const char* operation, int from, int to);
const char* FleetGame_GetSaveFilePath(int slot, bool backup);
// Joins the Context Fleet created: 2ship's folder, settings and console variables become current.
void FleetGame_JoinHostContext(void);
// Releases what JoinHostContext and CreateResources registered. Main thread, at deinit.
void FleetGame_LeaveHostContext(void);
// Between frames: Fleet may park 2ship here and run the other game. No-op outside Fleet.
void FleetGame_FrameEnd(void);
// Asks Fleet to continue in the other game after this frame. No-op outside Fleet.
void FleetGame_RequestSwitch(const FleetDestination* destination);
void FleetGame_Log(FleetLogLevel level, const char* message);
// Hands a soft reset to Fleet, which resets the game being played; false outside Fleet.
bool FleetGame_ResetActiveGame(void);
// After the player saves: Fleet confirms both games' saves together. Any thread; no-op outside Fleet.
void FleetGame_CommitSave(void);
// 2ship's own soft reset, back to the boot logo and title.
void S2H_ResetToTitle(void);

#ifdef __cplusplus
}
#endif

#endif
