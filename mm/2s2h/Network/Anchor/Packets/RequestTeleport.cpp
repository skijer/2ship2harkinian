#include "2s2h/Network/Anchor/Anchor.h"
#include <nlohmann/json.hpp>
#include <libultraship/libultraship.h>
#include "2s2h/GameInteractor/GameInteractor.h"

void Anchor::SendPacket_RequestTeleport(uint32_t clientId) {
    if (!IsSaveLoaded()) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = REQUEST_TELEPORT;
    payload["targetClientId"] = clientId;

    QueueOutgoingPacket(payload);
}

void Anchor::HandlePacket_RequestTeleport(nlohmann::json payload) {
    if (!IsSaveLoaded()) {
        return;
    }

    uint32_t clientId = payload["clientId"].get<uint32_t>();
    SendPacket_TeleportTo(clientId);
}

bool Anchor::CanTeleportTo(uint32_t clientId) {
    if (roomState.teleportMode == 0) {
        return false;
    }

    if (!IsSaveLoaded()) {
        return false;
    }

    if (clients.find(clientId) == clients.end()) {
        return false;
    }

    AnchorClient& client = clients[clientId];

    if (client.self) {
        return false;
    }

    if (!client.online || !client.isSaveLoaded) {
        return false;
    }

    std::string ownTeamId = CVarGetString("gNetwork.Anchor.TeamId", "default");
    if (roomState.teleportMode == 1 && client.teamId != ownTeamId) {
        return false;
    }

    if (client.sceneId == SCENE_MAX || client.sceneId == SCENE_KAKUSIANA) {
        return false;
    }

    return true;
}
