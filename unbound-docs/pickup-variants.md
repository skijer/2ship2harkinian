# Pickup variants

`S2HCustomPickupVariant` describes a seed reward that unlocks one function of an existing custom item. Register the inventory item first, then call `S2HModApi.RegisterPickupVariant` for each acquisition identity.

The inventory item keeps its normal key, slot, button and wheel. A pickup has a separate stable acquisition key, its parent `itemKey`, and a `variantKey`. Acquisition identities are hidden from inventory layouts and the save editor and cannot be equipped.

```c
static void ReceiveFunction(const char* itemKey, const char* variantKey) {
    // Persist the matching function in this item's storage/wheel.
}

S2HCustomPickupVariant pickup = Z64Items_DefinePickup(
    "example.staff", "fire", "example.staff.pickup.fire");
pickup.iconPath = "__OTR__textures/example/fire_icon";
pickup.getItemModelPath = "__OTR__objects/example/fire_staff_dl";
pickup.getItemText = "You unlocked the %rFire%w function!";
pickup.onReceive = ReceiveFunction;
pickup.randomizer = &fireRewardLogic;
Z64Items_RegisterPickup(api, &pickup);
```

Registration copies the acquisition data, including temporary text strings and randomizer metadata. Null presentation paths inherit the parent's presentation. An absent unique 3D model can reuse the existing base model; the API does not generate new assets.

Receiving a pickup records its acquisition identity, grants ownership of the parent, then calls `onReceive(itemKey, variantKey)`. It does not run the parent's `onReceive`, so a function pickup does not grant every function or choose another function. Callbacks should be idempotent.

## Seed and save identity

Each acquisition key has its own randomizer reward ID and fixed position in the pool. Spoilers store `pickup: { key, itemKey, variantKey }` alongside the stable reward name. Imports validate that identity. Save sections preserve each pickup location by key, so loading resolves the reward again if numeric runtime assignments change. Each actor binds its own acquisition key; one spawned reward cannot change another reward's presentation.

The actual draw/get-item path uses the acquisition definition's icon, model, text and scale. Inventory/button rendering continues using the parent definition. Wheel unlocks are persisted by the mod's normal storage.

Set the optional `drawSoulFlame` tail to true and supply `flameRed`, `flameGreen`, and `flameBlue` to draw the native animated enemy-soul flame in place of the pickup model. This affects world drops and get-item presentation; textbox icons and inventory icons remain unchanged. Older structures without this tail keep their model rendering.

Slate acquisitions use turquoise (Bomb), yellow (Stasis), pale blue-gray (Cryonis), and orange (Master Cycle). Seasons acquisitions match their icons: green (Spring), red (Summer), yellow (Autumn), and blue (Winter).

`S2HCustomItemRandomizer.poolModeOption`, `poolModeValue` and `poolModeInvert` optionally control pool membership from the captured seed option. With `poolModeInvert=false`, include this reward only when the seed's option equals the value. With `true`, include it for the other values.

## Console testing

```text
drop custom_item "nei.elemental_wand" sand
drop custom_item "nei.sheikah_slate" stasis
drop custom_item "nei.rod_of_seasons" winter
give custom_item "nei.elemental_wand" meteor
```

Explicit acquisition keys also work, for example `drop custom_item "nei.wand.sand"`.

Registered function names:

| Parent | Variants |
|---|---|
| `nei.elemental_wand` | `sand`, `tornado`, `water`, `meteor`, `storm`, `shadow` |
| `nei.sheikah_slate` | `bomb`, `stasis`, `cryonis`, `master_cycle` |
| `nei.rod_of_seasons` | `spring`, `summer`, `autumn`, `winter` |

Wand pools contain one parent for the complete/medallion modes and six specific acquisitions for Function pickups. Slate and Seasons pools contain their four specific acquisitions. Existing seed rewards pointing at the generic parent are still generic parent rewards; regenerate the seed to obtain fixed per-function placements.
