# Native Unbound / ModApi port status

Compared against the local `Shipwright-unbound` checkout on `unbound-modapi`. This comparison covers native
host changes and the public ModApi; it excludes mod implementations and does not synchronize libultraship
versions. MM uses its own forms, items, actor ids, three-day cycle and entrance encoding.

| Facility | 2Ship implementation |
|---|---|
| Public ModApi function table | Equivalent functions, with MM form eligibility in place of OoT age eligibility; additional icon-path setter |
| Boss registry | Five MM slots, compatible/preferred assignments, scene substitution, logic rules, defeat reporting, save/spoiler integration |
| Mod dungeon registry | Layer-merged JSON regions/events/checks, dungeon items, logic predicates, placement modes, save/spoiler integration |
| Dungeon entrance shuffle | Coupled assignments for the four normal MM doors and registered mod doors; seeded and saved by name |
| Scenes/entrances/flags | Named SceneDB entries, extended room ids and saved room flags, MM entrance layout preserved |
| Coordinates/fog/paths | Expanded coordinates and float matrices, world-unit fog, float path points and layer-merged JSON paths |
| Collision | 29-bit vertex indices and 32-bit counts; static/dynamic node storage and dynamic actors/geometry grow as needed |
| Actors/objects/room meshes | 8192 loaded actors, 1024 object slots including persistent objects, dynamically allocated culled mesh lists |
| Declared actors | JSON static/animated models, collision, dialogue, tracking, limb axes/pivot, segment textures and hidden limbs |
| Text | Layer-merged English/staff message tables, add/replace/delete ids, indexed lookup, enlarged buffers and bounds |
| Exporter | Scene/room, collision/bulk, path and text conversion; original ROM resources copied into the output archive |
| Build | Windows x64 Release configured and built with CMake; large-object support, bounded parallel compilation, no forced duplicate-symbol link |

Validation performed:

- `cmake -S . -B build\x64`
- `cmake --build build\x64 --config Release --target 2ship`
- Actual JSON save converters tested with nine-slot legacy key arrays, ten-slot arrays and extra-slot input.
- Public boss, dungeon entrance/key, actor and message lookup exports verified in the generated x64 executable.
- Public ModApi function names compared with SoH's table; JSON documentation examples checked.
- Working-tree whitespace checks passed. No changes were staged or committed.

The executable is `x64/Release/2ship.exe`, with the native mod import library beside it. Mods that include
the changed game structs need rebuilding against this checkout. Native compiler warnings remain outside
the port; the final build has no compiler/linker errors and no forced-link/duplicate-video-symbol warnings.

Compilation is not a play test. Real scene packs still need generation/save/reload, key consumption and
cycle refill, shuffled transitions, custom boss rewards, declared actor rendering and long-text checks.
The native MM minimap remains limited to 32 rooms and is disabled for larger scenes. Some camera/saved
position formats retain native 16-bit fields. MM's randomizer currently has no Majora completion condition
equivalent to OoT's Ganon check; the final boss slot provides scene substitution and defeat reporting.

SoH's browser/WebAssembly target, browser host bridge and its libultraship platform changes were not ported
or tested here. This is native Windows feature parity, not a claim that every SoH platform/game mode or
runtime behavior has been reproduced or certified.
