#ifndef FLEET_GAME_H
#define FLEET_GAME_H

#include <stdbool.h>

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

extern "C" {
#endif

// True while 2ship runs inside Fleet rather than as its own program.
bool FleetGame_IsHosted(void);
// Joins the Context Fleet created: 2ship's folder, settings and console variables become current.
void FleetGame_JoinHostContext(void);
// Releases what JoinHostContext and CreateResources registered. Main thread, at deinit.
void FleetGame_LeaveHostContext(void);
// Between frames: Fleet may park 2ship here and run the other game. No-op outside Fleet.
void FleetGame_FrameEnd(void);

#ifdef __cplusplus
}
#endif

#endif
