# NEI mods for 2Ship

The 53 mod folders have been compiled and linked in Release against 2ship-unbound-modapi on 2026-10-12. Their packages are installed in x64/Release/mods. Compilation does not establish in-game correctness; gameplay still needs testing.

Configure with cmake -S mod-sdk -B build/mod-sdk and build with cmake --build build/mod-sdk --config Release. Set S2H_GAME_DIR to the game folder to install packages automatically. Asset-only changes require running mod-sdk/tools/pack_mod.py again.

Save Editor > Mod Items & Equipment contains Give/Remove, bulk ownership controls and the Pause menu disposition grid. Select an item then click a cell, or drag its name into the grid. Screen controls are Items, Collectables and Masks. Each screen lists Vanilla, 1, 2, 3 and so on. Collectables page 1 contains the medallions and spiritual stones. Masks page 1 has a four-column equipment grid with a 3D preview of Link in the pause menu. Changes persist in configuration and can be reset per item. Equipment defaults to the masks overlay pages. In pause, L cycles extra pages; A wears/removes a piece. For shared equipment cells: A opens a wheel, left/right selects, A closes and toggles the piece, B cancels. Worn equipment icons are tinted green.

The ports use Majora's Mask actors, item actions and hooks. Differences from the SoH versions remain where the games have different systems; see each mod's notes. No runtime validation is claimed.

OoT Asset Loader (oot_assets) supplies shared icons, names, get-item models and spell animations without replacing MM assets. OoT Items in MM (oot_items) supplies the Slingshot, Boomerang, Hammer and Din/Farore/Nayru spells. See their READMEs for controls and the native MM adaptations.
