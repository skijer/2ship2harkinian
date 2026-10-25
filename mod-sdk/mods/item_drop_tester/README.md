# Item Drop Tester (2Ship)

Independent local test mod. Open **Mods > Item Drop Tester** with a save loaded.
Filter by acquisition key; click **Drop** to queue a real pickup 80 units above Link.
Close the menu and unpause: it spawns on the game thread, then falls into pickup range.
The standard cutscene, model, text and receive callbacks run when collected.

The list reads the live custom item registry, including registered pickup subtypes,
equipment holder items and progressive rewards. Both unowned and already owned items
can be dropped. Repeated pickups retain each mod's own duplicate/progression behavior.
It does not reset ownership, progression or the save. Vanilla items and arbitrary
actor parameters are not part of this catalog.

The selected entry shows its parent/subtype, icon and acquisition/held model source.
Mark the manual checklist for pickup, icon, acquisition model, held model, use, save/load
and Anchor. Results persist in local configuration; **Copy full checklist (Markdown)**
copies every registered key, including pending entries, for a fresh audit.

Local Release build compiled and installed on 2026-10-24. Package/import verification: 71 packages, 0 failures. In-game tests remain manual. Enable item_drop_tester in the mod manager, then restart if requested.
