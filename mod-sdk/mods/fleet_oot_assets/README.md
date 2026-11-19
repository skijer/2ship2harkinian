# Fleet OoT reward identities for MM

This development package registers 325 stable OoT reward identities through the existing MM CustomItemRegistry.
Every randomizer definition contributes zero copies, with no count option or native pool sources. The definitions
have no local gameplay action, acquisition or receipt callback. Fleet's delivery journal remains responsible for
granting the actual reward in its owning game.

Registration requires the approved `fleet.presentation.context.v1` adapter service with the correct receiving
game, compatible size/version and an active Combo mode. An environment flag cannot enable standalone use.
Standalone and solo modes register nothing. Do not enable this development package in a player installation: the catalog service
and manifest explicitly report presentation as incomplete. A catalog entry is not proof that its model works.

`catalog.cpp` is the compiled registrar. `catalog.inc.h` contains stable identities. The JSON records native source
provenance and audited static draw recipes; it contains no extracted game data. `native_draw.inc.h` and
`draw_bindings.inc.h` now compile as private drawing code connected to per-entry acquisition callbacks. They use
only the adapter's typed C-function bindings, without importing a game executable or DLL. Matrix and interpolation
state and resource absence are tested. The Triforce drawing port reads its counter from the owning OoT save
through the service and retries an unavailable save rather than using MM's inventory or guessing a tier.

`special_draws.inc.h` prevents native constructor placeholders from being drawn for items whose randomizer has a
custom override. 167 OoT entries currently execute a basic authoritative table recipe in the isolated C++ harness;
this is not an in-game visual acceptance result. Special/progressive recipes, icon/resource cache installation and
skeletal animation remain unfinished. The catalog's presentation-ready flag stays false.

The read-only `fleet.oot_assets.catalog.v1` service reports registered keys and names, never runtime IDs.
Foreign seed metadata uses FNV-1a-64 over canonical UTF-8 item names, prefixed with `fleet.oot.`.

Native draw provenance: the current `soh/src/code/z_draw.c` in soh-fleet. Its routines are adapted to the receiving
game's PlayState; foreign PlayState pointers are never passed across game modules. No code is sourced from legacy
mod tools folders. No ROM-derived meshes or textures are distributed in this source folder.
