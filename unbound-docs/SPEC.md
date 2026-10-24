# 2Ship Unbound — archive format specification

**Format version 1.** This document is the contract: every way an `.o2r` read by 2Ship Unbound may differ from a
vanilla 2Ship archive. It says *what* is accepted and *what it means*; how the engine does it lives in the code
under `mm/2s2h/Unbound/`. A change to this text is a format change.

It follows SoH: Unbound's format (roborich/Shipwright, `unbound-docs/SPEC.md`) wherever Majora's Mask allows, so a
tool that writes one learns the other quickly. Where Majora's Mask differs (scene commands, the bit-packed entrance
encoding, packed surface words, owl and new-cycle saves), this document says so.

Normative words: **must**, **must not**, **accepted**, **rejected** (not loaded, error logged), **ignored**.

## 1. Container

1. An Unbound archive is a zip with the `.o2r` extension. Stored and deflated entries are accepted.
2. A **base** archive provides every vanilla scene in the §4 form and **must** carry `unbound.json` (§6) with
   `"scenes"` in `features`. The game converts `mm.o2r` into `mm-unbound.o2r` at boot (§6) and mounts it above it.
   The base holds scene/room, collision, path and text documents and copies the original ROM resources.
3. Archives mount lowest first: `2ship.o2r`, `mm.o2r`, `mm-unbound.o2r`, then mods in mod-menu order. The documents
   of §4–§7 merge across layers (§3); every other path replaces whole (last mounted wins).
4. Once a base is mounted, every vanilla scene is read from `scenes/<scene>/scene.json`.
   Converter revision 2 rebuilds older bases to include collision metadata, paths and text.

## 2. Values

| Kind | Written by the converter | Accepted on read |
|---|---|---|
| Integer | JSON number | number (fraction truncated toward zero); a string that is wholly an optionally signed decimal or `0x` hex integer; a boolean (`true` = 1) |
| Number | JSON number | number; a string that is wholly a decimal with optional fraction/exponent, or `0x` hex |
| Position | `[x, y, z]`, integers when whole | 3-element array of numbers; world units, ±1 048 576 (§8) |
| Integral position | `[x, y, z]` | 3-element array of numbers, a fraction is **rounded** |
| Vector | `[x, y, z]` | 3-element array of integers; extra elements ignored; fewer → `[0, 0, 0]` |
| Colour | `[r, g, b]` 0–255 | 3-element array |
| Path | archive-relative string, forward slashes | string |
| Positional list key | `"0"`, `"1"`, … | contiguous from `"0"` after merging (§3.2) |

- A missing key takes the zero/empty default of its type unless stated otherwise. A key of the wrong JSON type is
  treated as missing. Unknown keys are ignored. `$` keys are reserved (§3).
- Ranges are requirements on the writer: the reader stores an out-of-range value in the engine field's width.
- JSON comments are accepted. The first byte of a §4 document **must** be `{`.

## 3. Layering

For one path, the documents of every mounted archive merge lowest first:

1. Objects merge key-wise and recursively; a later value replaces an earlier one.
2. `null` deletes a key. In a positional list a deletion is legal only at the tail; a hole **rejects** the document.
   A layer that does not parse is skipped with an error.
3. Arrays replace whole.
4. `"$replace": true` inside an object discards every lower layer's value of that object.
5. `"$order": [keys…]` on a keyed list gives the order of the listed keys; the rest follow, integer keys ascending
   numerically first, then the others lexically. Illegal on a positional list.
6. `"$schema": "<type>/<version>"` names the document type. A §4 document is **rejected** unless some layer provides
   it; the highest layer that does wins.
7. Binary bulk files (display lists, textures, legacy collision headers, `collision.bin`, legacy paths,
   cutscenes, animated materials, audio) do not merge. JSON collision/path/text documents do merge.

## 4. Scenes

### 4.1 Paths

```
scenes/<scene>/scene.json             $schema 2ship-unbound/scene/1
scenes/<scene>/rooms/<room>.json      $schema 2ship-unbound/room/1
<any path>.json                       $schema 2ship-unbound/collision/1   (§4.5)
```

`<scene>` for a vanilla scene is its file name in `mm.o2r` (`Z2_TOWN`, `Z2_00KEIKOKU`); the converter names each
room document after the room's file name (`Z2_TOWN_room_00`). A custom scene uses any other directory name and is
reached only through the registry (§7). Room documents are found through the scene's `rooms` list, so their names
are free.

### 4.2 Documents

```text
{ "$schema": "2ship-unbound/scene/1", "setups": { "0": { … }, "1": { … } } }
```

`setups` is keyed by layer: `"0"` is the scene's main header and **must** exist; `"1"` and up are the alternate
headers `gSaveContext.sceneLayer` selects. Every setup is complete: nothing is inherited from `"0"`. A layer index
without a setup falls back to `"0"`, as vanilla's empty alternate header does. A room document has the same shape.

Every key of a setup is optional; an absent key emits no scene command. A setup emits its commands in the order of
its `order` array (setup keys; unknown, absent and repeated keys are ignored), then every other present key in the
order of the table below. The converter writes `order` so a vanilla header keeps its own command order: some
commands read what an earlier one set (the spawn list reads the entrance list).

| Key | Command | Value |
|---|---|---|
| `spawns` | 0x00 spawn list | positional list of `{ id, pos, rot, params }` (`pos` a position, `rot` a vector) |
| `actors` | 0x01 actor list | positional list of `{ id, pos, rot, params }`; `id` may also be an actor's debug name (`"En_Torch2"`) or a key registered through the ModApi |
| `actorCsCameras` | 0x02 actor cutscene cameras | positional list of `{ setting, points: [vector, …] }` |
| `collision` | 0x03 collision header | path of a vanilla collision header resource or of a §4.5 document |
| `rooms` | 0x04 room list | positional list of room document paths |
| `wind` | 0x05 | `{ west, vertical, south, speed }` |
| `entrances` | 0x06 entrance list | positional list of `{ spawn, room }` |
| `specialObjects` | 0x07 | `{ elfMessage, globalObject }` |
| `behavior` | 0x08 room behavior | `{ gameplayFlags, currRoomUnk2, currRoomUnk5, msgCtxUnk, enablePointLights, kankyoContextUnkE2 }` |
| `mesh` | 0x0A room shape | §4.3 |
| `objects` | 0x0B object list | array of object ids |
| `lights` | 0x0C light list | positional list of `{ type, pos, color, glow, radius }`; `pos` is a position for a point light (`type` 0 or 2) and the packed direction words for a directional one (`type` 1) |
| `paths` | 0x0D path list | array of path resource paths |
| `transitionActors` | 0x0E | positional list of `{ id, pos, rotY, params, front: { room, effects }, back: { room, effects } }` |
| `lighting` | 0x0F environment lights | positional list of `{ ambient, light1Dir, light1Color, light2Dir, light2Color, fogColor, fogNear, fogFar }`, plus the world fog keys below |
| `time` | 0x10 | `{ hour, minute, increment }` (defaults 255) |
| `skybox` | 0x11 | `{ unk, id, weather, indoors }` |
| `skyboxModifier` | 0x12 | `{ skyboxDisabled, sunMoonDisabled }` |
| `exits` | 0x13 exit list | array of exit values (§4.4) |
| `sound` | 0x15 | `{ seq, natureAmbience, reverb }` |
| `echo` | 0x16 | integer |
| `cutscenes` | 0x17 cutscene script list | positional list of `{ script: path, exit, entrance, flag }` |
| `regionVisited` | 0x19 set region visited | `true` emits the command |
| `animatedMaterials` | 0x1A | path of a texture animation resource |
| `actorCutscenes` | 0x1B actor cutscene list | positional list of `{ priority, length, csCamId, scriptIndex, additionalCsId, endSfx, customValue, hudVisibility, endCam, letterboxSize }` |
| `minimap` | 0x1C | `{ scale, entries: positional list of { mapId, unk2, unk4, unk6, unk8 } }` |
| `minimapChests` | 0x1E | positional list of `{ unk0, unk2, unk4, unk6, unk8 }` |

**World fog.** Vanilla `fogNear` is a point on the N64 fog scale, so fog cannot start past about 2 500 units, and
`fogFar` caps the far plane at 12 800. A lighting entry that carries any of these numbers instead uses world units
from the camera; an absent one takes the entry's vanilla equivalent:

| Key | Meaning | Default |
|---|---|---|
| `fogStart` | distance where fog begins | the start vanilla `fogNear` gives |
| `fogEnd` | distance where fog is opaque | `drawDistance` |
| `drawDistance` | far plane; room-shape entries past it are culled, and actor cull zones grow with it past 12 800 | `fogFar` |
| `nearPlane` | near plane; raise it for draw distances past ~100 000 to keep depth precision | the view's own (10) |

Entries blend through time of day and room transitions as vanilla entries do; a world-fog entry blending with a
vanilla one uses the vanilla one's equivalent. Effects that pull the fog in (Nayru's Love, fairies) still apply.

### 4.3 Room shape (`mesh`)

`{ "type": 0 | 1 | 2, … }`:

- type 0 (normal) and type 2 (culled): `entries`, a positional list of `{ opa, xlu }` display-list paths; a type 2
  entry also has `pos` (a position, the cull-sphere centre) and `radius`. At most 256 type 2 entries are drawn.
- type 1 (pre-rendered background): `format` (1 = one image, 2 = several), `opa`, `xlu`, and `images`, a positional
  list of `{ unk00, id, source, unk0C, tlut, width, height, fmt, siz, mode0, tlutCount }`.

Room meshes draw under the identity matrix, so their vertices are world coordinates; vertices are `s32` in the
engine (§8), so one mesh may span the whole world.

### 4.4 Exit values

An exit is the entrance the player is sent to. It is either a vanilla entrance as a JSON integer (the packed
`scene << 9 | spawn << 4 | layer` word, below `0x6E << 9`), or a string naming an entrance: `"<scene>/<entrance>"`
from the registry (§7). A number at or above `0x6E << 9` is **rejected**: that range holds custom entrances, whose
number depends on the player's mod stack, so a custom entrance is addressed by name only.

### 4.5 Collision document

```text
{
  "$schema": "2ship-unbound/collision/1",
  "bounds": { "min": [x, y, z], "max": [x, y, z] },
  "bulk": { "file": "scenes/my_cave/collision.bin", "vertices": 1832, "polys": 2410 },
  "surfaceTypes": { "0": { "floorType": 0, "material": 1, … } },
  "cameras": { "0": { "setting": 1, "count": 3, "positionIndex": 0 } },
  "cameraPositions": { "0": [x, y, z], "1": [x, y, z], "2": [fov, flags, unk] },
  "waterBoxes": { "0": { "xMin": 0, "ySurface": -40, "zMin": 0, "xLength": 800, "zLength": 800 } }
}
```

| Key | Meaning |
|---|---|
| `bounds.min`, `bounds.max` | integral positions; absent → computed from the vertices |
| `bulk.file` | the bulk file (§4.5.1); highest layer wins, never merged |
| `bulk.vertices`, `bulk.polys` | counts in the bulk file: at most 536 870 912 vertices and 2 147 483 647 polygons, subject to available memory. A file shorter than the counts imply **rejects** the document |
| `surfaceTypes` | positional list, referenced by polygon `type` |
| `cameras` | positional list of `{ setting, count, positionIndex }`: the camera reads `count` consecutive `cameraPositions` entries from `positionIndex` (position, rotation and fov/flags triples, as vanilla). A negative or missing `positionIndex`, or one past the list, reads `[0, 0, 0]` |
| `cameraPositions` | positional list of vectors, −32 768…32 767 (a remaining vanilla limit, §9) |
| `waterBoxes` | positional list, at most 2 147 483 647, subject to available memory; `room` is -1 (all rooms) or 0–32 767 |

**Surface type** — integers, packed by the reader into Majora's Mask's two words:

| Field | Range | Field | Range |
|---|---|---|---|
| `camera` | 0–255, index into `cameras` | `material` | 0–15 |
| `exit` | 0–31, **1-based** index into the setup's `exits`; 0 = none | `floorEffect` | 0–3 |
| `floorType` | 0–31 | `lightSetting` | 0–31, index into `lighting` |
| `wallFlags` | 0–7 | `echo` | 0–63 |
| `wallType` | 0–31 | `canHookshot` | 0/1 |
| `floorProperty` | 0–15 | `conveyorSpeed` | 0–7 |
| `isSoft` | 0/1 | `conveyorDirection` | 0–63 |
| `isHorseBlocked` | 0/1 | `isWallDamage` | 0/1 |

**Water box** — `xMin`, `ySurface`, `zMin`, `xLength`, `zLength`: integral world units, unbounded. `camera` 0–255,
`lightSetting` 0–31, `room` 0–62 or `-1` = every room (default `-1`), `notSwimmable` 0/1 (the box is skipped by the
swim-surface query and found only by the ripple query).

#### 4.5.1 Bulk file

Little-endian, no header, two arrays back to back. It is SoH: Unbound's `collision.bin` v3 layout, so either game's
tools write it:

| Vertex (12 bytes) | Polygon (28 bytes) |
|---|---|
| `s32 x, y, z` | `u16 type; u16 pad; u32 vA; u32 vB; u32 vC; s16 nx, ny, nz; s16 pad; s32 dist` |

`vA`, `vB`, `vC`: bits 0–28 the vertex index (**must** be below `bulk.vertices`, which **rejects** the document
otherwise), bits 29–31 flags (`vA`: exclusion flags — camera, entity, projectile; `vB`: bit 29 = conveyor;
`vC`: reserved, write 0). `nx, ny, nz`: the unit normal scaled by 32 767. `dist`: the plane distance from the world
origin, rounded. Bytes past the declared counts are ignored.

## 6. Manifest

`unbound.json` at the root of a base:

```text
{ "format": "2ship-unbound", "formatVersion": 1, "game": "mm", "features": ["scenes"],
  "source": { "converter": "<build>", "romHashes": [ … ] } }
```

The game rebuilds `mm-unbound.o2r` when it is missing or its `source` no longer matches the running converter and
the mounted `mm.o2r`.

## 7. Scene registry

`unbound/scenes.json`, layer-merged, keyed by scene id (any string without `/`):

```json
{
  "my_cave": {
    "name": "My Cave",
    "scene": "scenes/my_cave/scene.json",
    "drawConfig": 0,
    "titleTextId": 0,
    "persistentFlags": { "chest": 1, "switch0": 0, "switch1": 0, "collectible": 0 },
    "entrances": {
      "front": { "spawn": 0, "titleCard": true, "startTransition": 2, "endTransition": 2, "continueBgm": false }
    }
  }
}
```

- A registry entry whose key is a vanilla scene file name adds entrances to that scene instead of a new scene.
- `entrances`: at most 32 per entry, in key order; more are dropped with an error. `spawn` indexes the scene's
  `spawns`; `titleCard` (default `true`) shows the area name; `startTransition`/`endTransition` are transition
  types 0–127 (default 2); `continueBgm` keeps the music playing.
- Every custom entrance gets a number at load from the range Majora's Mask never uses (scene slots from `0x6E`).
  The number is not stable across mod stacks: documents and saves refer to `"<scene>/<entrance>"`.
- Custom scene flags (chests, switches, collectibles, cleared rooms) are saved by scene id, next to the Save struct,
  with the save being written (new cycle or owl), so they follow Majora's Mask cycle rules. The Song of Time clears
  them except the bits set in `persistentFlags` (cleared rooms always reset), as vanilla keeps some flags per scene.

## 8. World extent

Positions are not capped at ±32 767. The engine renders with float matrices and `s32` vertices, keeps collision
integral in `s32`, and stores scene positions as `f32`. The world is ±1 048 576 units (2²⁰), where `f32` still
resolves about 0.06 units. Vanilla resources load unchanged: their `s16` data widens on load.

## 9. Limits

| Thing | Vanilla | Unbound |
|---|---|---|
| Scenes | 113 | 113 + custom ids up to 32 767 |
| Entrances | 110 scene tables | + 32 per registry entry, slots unbounded |
| Scene flags | 120 scenes | vanilla + one set per custom scene |
| World extent | ±32 767 | ±1 048 576 (§8) |
| Fog start / draw distance | ~2 500 / 12 800 | world units, unbounded (§4.2) |
| Object slots, including persistent objects | 35 | 1 024 |
| Loaded actors | 255 | 8 192 |
| Culled room-shape entries | 128 | 32-bit count, allocated as needed |
| Transition actors (doors, loading planes) | 48 (minimap) / 64 (params index) | 1 024 |
| Dyna (moving-collision) actors | 50 | allocated as needed, with separate growing polygon, vertex and water-box storage |
| Collision vertices / polygons per header | 8 192 / 65 535 | 29-bit vertex indices / signed 32-bit polygon indices |
| Rooms per scene | 32 | 32 768; room ids 0–32 767, with extended saved room flags |

The native minimap still supports 32 rooms. Scenes with more rooms disable its map data. Extended visited flags
persist; extended clear flags reset at the end of the cycle and temporary clear flags reset on scene initialization.

Remaining vanilla limits: scene camera data and cutscene camera points (`s16`), Epona's
saved position (`s16`), point-light positions sent to the renderer (`s16`).

## 10. JSON paths

A path resource can use JSON with resource type `2ship-unbound/path`, version 1, and schema
`2ship-unbound/path/1`. Like scene documents it merges across archive layers. `paths` is a positional object
list. Each entry has `points` (an array of positions), `additionalPathIndex` (default 255: no link), and
`customValue` (signed 16-bit, default zero). Points are stored as float coordinates; legacy binary paths
retain their original on-disk layout and convert to float on load. MM still uses 8-bit path counts:
at most 255 paths and 255 points per path. Linked indices must reference an existing path or be 255.

```json
{
  "$schema": "2ship-unbound/path/1",
  "paths": {
    "0": { "points": [[40000.5, 0, 0], [40500.25, 10, 100]], "additionalPathIndex": 255 }
  }
}
```

## 11. Text

`text/eng/messages.json` and `text/staff/messages.json` merge across layers. The converter writes the complete
base tables; a mod only writes changed message ids. Documents use `$schema: "2ship-unbound/text/1"` and a
`messages` object keyed by decimal or hexadecimal ids. Id 65535 is reserved. Setting an entry to null deletes
it; `$replace` follows the normal merge rules. An empty merged table stays empty and uses a safe fallback.

Each entry contains `box`, `ypos`, `text`, and the optional MM header fields `icon`, `nextMessage`,
`firstItemCost`, and `secondItemCost`. UTF-8 code points U+0000–U+00FF represent message bytes; other code points
become `?`. MM messages end with byte 0xBF, staff messages with 0x02. The loader adds a missing terminator.
Normal message ids must be below 0x4E20; MM routes ids from that value upward through its staff-text renderer.

The raw message limit is 8192 bytes including the MM header. The decoded buffer is 1024 bytes with a margin
for expanding control codes. MM's three-line box layout remains; use box-break control codes for long text.
Binary bases remain supported, and legacy `override/text/...` resources may add or replace ids after JSON.

## 12. Declared actors

Layer-merged `unbound/actors/<namespace>/<name>.json` files declare actor types placed by name in scene actor
entries. The driver supports normal/flex skeletons with a normal animation, a held frame or looping speed,
static display lists, translucent drawing, segment textures 8–12, hidden limbs, scale/shadow/culling settings,
solid collision cylinders, dialogue, and head tracking with configurable limb axes and pivot. Missing or
incompatible assets prevent that actor type from spawning and are logged.

Actor ids are allocated at runtime. Numeric scene ids are accepted only for vanilla actors; use names for
registered types. Non-zero placement params override a talking type's default message id. The format mirrors
SoH's actor declarations, while registration, drawing, tracking and message functions use native MM APIs.
