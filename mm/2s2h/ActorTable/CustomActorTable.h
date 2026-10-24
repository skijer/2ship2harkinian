#ifndef CUSTOM_ACTOR_TABLE_H
#define CUSTOM_ACTOR_TABLE_H

#include "z64actor.h"

#ifdef __cplusplus
extern "C" {
#endif

// Ids from ACTOR_ID_MAX up belong to actors registered at runtime; NULL for an id with no actor.
ActorOverlay* ActorOverlayTable_Get(s16 actorId);
s16 ActorOverlayTable_GetCount(void);

#ifdef __cplusplus
}

#include <string>

s16 ActorOverlayTable_Add(const ActorProfile& profile, const std::string& debugName, const std::string& description);
#endif

#endif // CUSTOM_ACTOR_TABLE_H
