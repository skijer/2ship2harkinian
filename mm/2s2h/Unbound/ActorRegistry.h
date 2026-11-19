#pragma once
#include <libultraship/libultra.h>

#ifdef __cplusplus
#include <cstdint>

#include "DeclaredActorType.h"

namespace S2H::Unbound {

inline constexpr int32_t kCustomActorIdBase = 0x1000;

void LoadCustomActors();

const DeclaredActorType* GetDeclaredActorType(int32_t actorId);

} // namespace S2H::Unbound

#endif
