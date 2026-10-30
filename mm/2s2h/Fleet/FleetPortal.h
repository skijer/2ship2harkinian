#ifndef FLEET_PORTAL_H
#define FLEET_PORTAL_H

#include <fleet/FleetGameApi.h>

#ifdef __cplusplus
extern "C" {
#endif

// The doorways between the games. 2ship's side of "market" is the Clock Tower door: entering the tower from South
// Clock Town leaves for the other game, and arriving through it lands in front of the tower.
void FleetPortal_Arrive(const FleetDestination* arrival);
// Once per frame: hands a pending departure to Fleet and walks into a pending arrival, opening the save first if
// 2ship is not in gameplay yet.
void FleetPortal_FrameEnd(void);

#ifdef __cplusplus
}
#endif

#endif
