#pragma once
// SOH [Unbound] The driver every declared actor type runs (unbound-docs/actors.md). One actor implementation; each
// type is its own ActorDB entry pointing at it, and the driver finds its settings by actor id.
#include "DeclaredActorType.h"
#include "2s2h/ModApi/ActorRegistry/ActorRegistry.h"

// The ActorDB registration for a type: its name, flags, category and the shared driver functions.
S2HActorDefinition DeclaredActor_DBInit(const DeclaredActorType& type);
