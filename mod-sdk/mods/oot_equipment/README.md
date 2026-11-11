# OoT Equipment

Master Sword, Biggoron Sword, Deku Shield, OoT Mirror Shield, Goron Tunic, Zora Tunic, Iron Boots and Hover Boots, registered as equipment for MM. Weapon, shield, tunic and boot behavior is handled by the equipment registry and z64equipment roles.

This mod declares its equipment-page layout and background. Positions follow OoT: swords, shields, tunics and boots on four rows, with three equipment columns to the right of the passive column. The Giant's Knife from oot_item_upgrades shares the Biggoron Sword cell. The English background title is packaged locally; the other background sections come from OoT Asset Loader.

L cycles the Masks subpages. A/C equips or toggles a single piece; C-Up describes it. Shared cells keep the item wheel, with C equipping the selected piece. Saved player layout edits override the mod defaults.

Pickup drawing follows the original OoT recipes: complete OPA/XLU meshes, tunic color lists, boot rivets and animated sword materials. Imported GI assets have private paths and rewritten hashed dependencies; they do not replace MM resources. Master Sword uses Shipwright's Randomizer_DrawMasterSword transform, also reused by its progressive stages.

The layout supplies the four original OoT cursor corners. The host anchors them to the actual cell geometry and page projection.

To rebuild the private pickup assets from a local, player-owned OoT archive:

```powershell
python mod-sdk/mods/oot_equipment/tools/import_get_item_assets.py <path-to-oot.o2r>
```

After building, check the packaged resource graphs and cursor formats:

```powershell
python <mods-checkout>/mod-sdk/mods/oot_equipment/tools/verify_get_item_assets.py --packages x64/Release/mods --game .
```
