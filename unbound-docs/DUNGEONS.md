# MM dungeon registration

The layer-merged `unbound/dungeons.json` registry declares dungeons without a native binary. Scene and entrance
names must already exist in SceneDB. Each dungeon entry has a namespaced key, `name`, `scenes`, `entrance`,
`exit`, `overworldRegion` (an MM `RR_*` name), `entryRegion`, optional `entranceLogic`, dungeon item counts,
and a `regions` object. Invalid entries are rejected without keeping a partial graph.

```json
{
  "example.temple": {
    "name": "Example Temple",
    "scenes": ["example.temple"],
    "entrance": "example.temple/front",
    "exit": "example.field/temple_door",
    "overworldRegion": "RR_TERMINA_FIELD",
    "entryRegion": "Lobby",
    "entranceLogic": "RI_HOOKSHOT",
    "smallKeys": 1,
    "bossKey": true,
    "map": true,
    "compass": true,
    "regions": {
      "Lobby": {
        "events": { "Torches": "HasFireSource" },
        "checks": {
          "Key Chest": { "type": "chest", "flag": 1, "vanilla": "@smallkey", "logic": "true" },
          "Map Chest": { "type": "chest", "flag": 2, "vanilla": "@map", "logic": "true" },
          "Compass Chest": { "type": "chest", "flag": 3, "vanilla": "@compass", "logic": "true" }
        },
        "exits": { "Hall": "keys(1) and event(Torches)" }
      },
      "Hall": {
        "checks": { "Boss Key Chest": { "type": "chest", "flag": 4, "vanilla": "@bosskey" } },
        "exits": { "Lobby": "true" }
      }
    }
  }
}
```

This example assumes the named scenes and entrances are registered separately; it is not a complete scene pack.

Regions may override `scene`, declare logic `events`, `checks`, and `exits` to another local region or
`@outside`. Checks accept `chest` (flags 0–31) or `collectible` (flags 0–127), optional scene override,
`vanilla` item and `logic`. Vanilla items use MM `RI_*` names, item display/spoiler names, registered mod keys,
or `@smallkey`, `@bosskey`, `@map`, `@compass`. Checks match the native scene and flag; their actor params remain
part of the scene placement. Check names must remain stable between mod versions to preserve saved placements.

Conditions are described in [BOSS-MODAPI.md](BOSS-MODAPI.md). Events are declared before conditions compile,
so forward references within a dungeon work. Checks and regions enter MM's existing logic graph, generation,
item placement and spoiler paths. Graph entries and overworld links are restored before seed generation,
including after applying presets that recreate native regions.

Dungeon keys and items use a separate MM inventory context at index 9, restored when leaving the dungeon.
This index extends the nine native key slots; it does not borrow an active vanilla dungeon slot. Doors and
the HUD read the active context through the normal MM key/item operations. The save section `unboundDungeons`
stores keys, total received keys, dungeon items, check placements and collection state by name. Original
nine-slot JSON and binary saves retain their layout and receive a default for the new context.
Normal files reset dungeon keys/items with the cycle. Randomizer files restore received keys and keep
dungeon items, matching MM's native randomizer cycle behavior. Temporary logic events do not become save flags.

`smallKeys` accepts 0–127; `bossKey`, `map`, and `compass` enable one item each. Their declared counts supply
the item pool. Optional `smallKeyPlacement`, `bossKeyPlacement`, `mapPlacement`, `compassPlacement` select:

| Value | Placement |
|---|---|
| -2 | Vanilla locations |
| -1 | Native key setting (small/boss keys) |
| 0 | Anywhere |
| 1 | Own dungeon |
| 2 | Start with |
| 3 | Any dungeon |
| 4 | Overworld |

Map and compass default to own dungeon because MM has no native map/compass placement option. Ensure there
are enough checks for the declared items and placement mode; the generator rejects impossible placements.
Native small/boss-key settings remain the defaults. Maps/compasses and keys use native MM icons and models.

The `Dungeon entrance shuffle` ModApi randomizer option mixes the four normal MM dungeon doors and registered
mod doors as coupled pairs. Their assignments are generated from the final seed and saved by dungeon key,
not transient entrance number. Missing/incompatible entrance assignments fall back to original routes with a
warning. Scene layer bits are preserved when a route is replaced. Stone Tower's inversion connection remains
native. Boss arena substitution is independent of this option.

Dynamic check ids must fit MM's signed 16-bit pickup parameter; scene ids use the widened SceneDB range.
The native minimap still has 32 rooms and is disabled for larger scenes. Hints may use `Unknown` for scenes
without a native name. This port targets native MM; OoT item, event and actor enum names are not aliases.

Windows Release compilation and save-codec compatibility have been checked. In-game scene transitions,
key consumption/refill, generation with real scene packs, custom boss rewards and long text still need play tests.
