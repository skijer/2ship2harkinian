#ifndef OOT_COMPAT_SOH_ACTOR_DB_H
#define OOT_COMPAT_SOH_ACTOR_DB_H

// Ship's actor database on MM's overlay table. An entry is the actor's live profile, so writes such as objectId
// reach the engine as they do in Ship; Ship-only fields (name, desc, category) do not exist here.
#include "z64actor.h"
#include "2s2h/ActorTable/CustomActorTable.h"
#include "2s2h/ModApi/ActorRegistry/ActorRegistry.h"

typedef ActorProfile ActorDBEntry;

static inline ActorDBEntry* ActorDB_Retrieve(const int id) {
    ActorOverlay* overlay = ActorOverlayTable_Get((s16)id);
    return overlay != NULL ? overlay->profile : NULL;
}

static inline int ActorDB_RetrieveId(const char* name) {
    return ActorRegistry_GetId(name);
}

#endif
