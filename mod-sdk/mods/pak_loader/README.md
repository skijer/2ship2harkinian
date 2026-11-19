# PAK Loader for 2Ship Unbound

ModLoader64 packs in 2Ship: bodies, equipment pieces, voices and emotes. Open **Mods > PAK Loader** after
restarting.

Selections per appearance: Link (human form), Adult Link (the `adult_link` form, OoT's adult skeleton through
`oot_assets`) and Fierce Deity. Deku, Goron, Zora and other mod forms keep their own models. Abilities stay with the
current form. Ocarina of Time adult and child packs work here as well as Majora's Mask ones.

The shared library, catalog and formats are the Ship mod's (`Shipwright-unbound-mod-nei/mod-sdk/mods/pak_loader`);
keep the shared files identical in both. Its README describes formats, naming and validation.

Differences from Ship:

- Equipment slots are Majora's Mask's: Kokiri/Razor/Gilded Sword, Hero's/Mirror Shield, Hero's Bow, Hookshot and
  Ocarina of Time. Only held items are rebuilt; the sheath keeps MM's own sword and shield.
- Root height follows the drawn skeleton: the model's leg length over Human Link's, OoT Adult Link's or Fierce
  Deity's.
- Hands accept MM and OoT display-list names; the Adult Link form may resolve a limb before this mod.
- Voice offsets come from `voiceSfxIdOffset`; "Voice Pack Volume" is a 0-1 float like 2Ship's other volumes.
- OoT `gameplay_keep` textures come from `objects/oot_gameplay_keep` (`oot_assets`); without it they are drawn white.
