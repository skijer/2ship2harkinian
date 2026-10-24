#pragma once

#include <cstdint>
#include <vector>
#include <ship/resource/Resource.h>
#include "SceneCommand.h"
#include "z64math.h"

namespace SOH {
// 2S2H [Unbound] mirrors z64scene.h: position is f32 (world extent)
typedef struct {
    /* 0x00 */ s16 id;
    /* 0x04 */ Vec3f pos;
    /* 0x10 */ Vec3s rot;
    /* 0x16 */ s16 params;
} ActorEntry; // size = 0x18

class SetActorList : public SceneCommand<ActorEntry> {
  public:
    using SceneCommand::SceneCommand;

    ActorEntry* GetPointer();
    size_t GetPointerSize();

    uint32_t numActors;
    std::vector<ActorEntry> actorList;
};
}; // namespace SOH
