#include "2s2h/Network/Anchor/Anchor.h"
#include <nlohmann/json.hpp>
#include <libultraship/libultraship.h>
#include "2s2h/GameInteractor/GameInteractor.h"

void Anchor::HandlePacket_DisableAnchor(nlohmann::json payload) {
    Disable();
}
