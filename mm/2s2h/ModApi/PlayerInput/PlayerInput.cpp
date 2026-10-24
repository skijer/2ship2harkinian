#include "PlayerInput.h"

#include <map>
#include <string>

#include "2s2h/GameInteractor/GameInteractor.h"

extern "C" {
#include "macros.h"
#include "z64.h"
}

namespace {

struct InputBlock {
    uint32_t buttons;
    bool stick;
};

std::map<std::string, InputBlock> sBlocks;
uint32_t sBlockedButtons = 0;
bool sStickBlocked = false;

void RecomputeBlocks() {
    sBlockedButtons = 0;
    sStickBlocked = false;
    for (const auto& [owner, block] : sBlocks) {
        sBlockedButtons |= block.buttons;
        sStickBlocked = sStickBlocked || block.stick;
    }
}

void ClearPad(OSContPad& pad) {
    pad.button &= ~sBlockedButtons;
    if (sStickBlocked) {
        pad.stick_x = 0;
        pad.stick_y = 0;
    }
}

void FilterPlayerInput(Input* input) {
    if (sBlockedButtons == 0 && !sStickBlocked) {
        return;
    }
    ClearPad(input->cur);
    ClearPad(input->prev);
    ClearPad(input->press);
    ClearPad(input->rel);
}

} // namespace

void PlayerInput_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPassPlayerInputs>(FilterPlayerInput);
}

bool PlayerInput_Block(const char* owner, uint32_t buttons, bool blockStick) {
    if (owner == nullptr || owner[0] == '\0') {
        return false;
    }
    sBlocks[owner] = { buttons, blockStick };
    RecomputeBlocks();
    return true;
}

void PlayerInput_Release(const char* owner) {
    if (owner == nullptr || sBlocks.erase(owner) == 0) {
        return;
    }
    RecomputeBlocks();
}

uint32_t PlayerInput_GetBlockedButtons(void) {
    return sBlockedButtons;
}

bool PlayerInput_IsStickBlocked(void) {
    return sStickBlocked;
}
