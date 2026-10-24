#pragma once

#include <cstdint>
#include <vector>
#include "SceneCommand.h"
#include <libultraship/libultra/types.h>

namespace SOH {
class SetExitList : public SceneCommand<uint32_t> {
  public:
    using SceneCommand::SceneCommand;

    uint32_t* GetPointer();
    size_t GetPointerSize();

    uint32_t numExits;

    std::vector<uint32_t> exits;
};
}; // namespace SOH
