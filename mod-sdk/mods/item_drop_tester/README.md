# Item Drop Tester (2Ship)

Standalone local test mod. Enable `item_drop_tester`, then open **Mods > Item Drop Tester**
with a save loaded.

- **Custom items and variants**: one row per parent item, a Drop button for the item
  and separate buttons for its registered pickup variants. A click queues a physical
  pickup 80 units above Link; close the menu and unpause. Ownership is granted only
  when collected, through the item's own acquisition message, model and receive callback.
- **Vanilla / built-in rando rewards**: 238 named rewards from the 2Ship rando catalog
  with their RandoItemId. Needs a randomizer save. The reward is lent to the carrier
  check `RC_CLOCK_TOWN_SOUTH_CHEST_UPPER` and paid out by the randomizer's own give
  queue, so progressives, conversions, messages and get-item cutscenes behave as in a
  seed. Once paid out, the carrier check gets its original contents back.
- The filter is case-insensitive and matches names, keys and variant names.

Select an entry without dropping it to inspect its identity and checklist. For custom
entries the panel also shows the parent/subtype, icon and acquisition/held model source.
The checklist covers pickup, icon, acquisition model, held model, use, save/load and
Anchor. Results persist locally. **Copy full checklist (Markdown)** exports pending and
tested entries for both catalogs.

Tests use the current inventory, mods and rando settings: duplicate/progressive rewards
keep their normal effects. The tester does not reset ownership or fake an unowned
inventory. While a built-in reward is paid out, Anchor receives the carrier check status.

Native display names and RI constants in `rando_catalog.inc.h` come from
`mm/2s2h/Rando/StaticData/Items.cpp`. The custom catalog is read live from loaded mods.
