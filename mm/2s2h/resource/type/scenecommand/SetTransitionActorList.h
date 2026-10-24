#pragma once

#include <cstdint>
#include <vector>
#include "SceneCommand.h"
#include "z64math.h"

namespace SOH {
typedef struct {
    struct {
        s16 room;           // Room to switch to
        s8 effects;        // How the camera reacts during the transition
    } /* 0x00 */ sides[2]; // 0 = front, 1 = back
    /* 0x04 */ s16 id;
    /* 0x08 */ Vec3f pos; // 2S2H [Unbound] f32 (world extent), mirrors z64scene.h
    /* 0x14 */ s16 rotY;
    /* 0x16 */ s16 params;
} TransitionActorEntry; // size = 0x18

class SetTransitionActorList : public SceneCommand<TransitionActorEntry> {
  public:
    using SceneCommand::SceneCommand;

    TransitionActorEntry* GetPointer();
    size_t GetPointerSize();

    uint32_t numTransitionActors;
    std::vector<TransitionActorEntry> transitionActorList;
};
}; // namespace SOH
