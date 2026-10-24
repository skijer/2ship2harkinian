#include "CustomActorTable.h"

#include <deque>

#include "2s2h/ShipUtils.h"

extern "C" {
#include "variables.h"
}

namespace {

constexpr s16 MaxActorId = 0x7FFF;

// Deques: the game keeps ActorOverlay and ActorProfile pointers, so registering more must never move them.
std::deque<ActorProfile> sProfiles;
std::deque<ActorOverlay> sOverlays;

} // namespace

extern "C" ActorOverlay* ActorOverlayTable_Get(s16 actorId) {
    if (actorId < 0) {
        return nullptr;
    }
    if (actorId < ACTOR_ID_MAX) {
        return gActorOverlayTable[actorId].profile != nullptr ? &gActorOverlayTable[actorId] : nullptr;
    }
    const size_t customIndex = actorId - ACTOR_ID_MAX;
    return customIndex < sOverlays.size() ? &sOverlays[customIndex] : nullptr;
}

extern "C" s16 ActorOverlayTable_GetCount(void) {
    return static_cast<s16>(ACTOR_ID_MAX + sOverlays.size());
}

s16 ActorOverlayTable_Add(const ActorProfile& profile, const std::string& debugName, const std::string& description) {
    if (ActorOverlayTable_GetCount() >= MaxActorId) {
        return -1;
    }
    const s16 actorId = ActorOverlayTable_GetCount();
    ActorProfile& stored = sProfiles.emplace_back(profile);
    stored.id = actorId;

    ActorOverlay overlay = {};
    overlay.profile = &stored;
    sOverlays.push_back(overlay);
    RegisterActorNames(actorId, debugName, description);
    return actorId;
}
