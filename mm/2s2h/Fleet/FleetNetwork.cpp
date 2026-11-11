#include "FleetGame.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/ShipInit.hpp"

namespace {

// Fleet copies the Anchor switch between both games; the connection follows the copied setting.
void FollowAnchorSwitch() {
    if (!FleetGame_IsHosted() || Anchor::Instance == nullptr) {
        return;
    }
    const bool wanted = CVarGetInteger("gNetwork.Anchor.Enabled", 0) != 0;
    if (wanted && !Anchor::Instance->isEnabled) {
        Anchor::Instance->Enable();
    } else if (!wanted && Anchor::Instance->isEnabled) {
        Anchor::Instance->Disable();
    }
}

static RegisterShipInitFunc sAnchorSwitch(FollowAnchorSwitch, { "gNetwork.Anchor.Enabled" });

} // namespace
