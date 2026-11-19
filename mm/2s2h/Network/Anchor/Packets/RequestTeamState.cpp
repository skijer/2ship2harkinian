#include "2s2h/Network/Anchor/Anchor.h"
#include <nlohmann/json.hpp>
#include <libultraship/libultraship.h>
#include "2s2h/BenPort.h"

void Anchor::SendPacket_RequestTeamState() {
    if (!roomState.syncItemsAndFlags) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = REQUEST_TEAM_STATE;
    payload["targetTeamId"] = CVarGetString("gNetwork.Anchor.TeamId", "default");

    QueueOutgoingPacket(payload);
}

void Anchor::HandlePacket_RequestTeamState(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    SendPacket_UpdateTeamState(CVarGetString("gNetwork.Anchor.TeamId", "default"));
}
