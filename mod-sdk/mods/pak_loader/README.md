# PAK Loader for 2Ship Unbound

ModLoader64 packs in 2Ship: bodies, equipment pieces, voices and emotes. Open **Mods > PAK Loader** after
restarting.

The same mod exists for Ship (`Shipwright-unbound-mod-nei/mod-sdk/mods/pak_loader`). Both share `archive.h`,
`model.h`, `names.h`, `library.h`, `equipment.h`, `sounds.h`, `emotes.h`, `session.h`, `catalog.h`,
`keep_textures.h`, `vorbis.c` and `third_party/`; keep the copies identical. `gen_catalog.py.txt` and
`gen_keep_textures.py.txt` live in the Ship copy.

## Page

- **Models and voices** (column 1): one model and one voice for Link (human form), Adult Link (the `adult_link`
  form, OoT's adult skeleton through `oot_assets`) and Fierce Deity. Deku, Goron, Zora and other mod forms keep
  their own models. Abilities stay with the current form. **Restore Defaults** clears models, voices, equipment and
  root adjustments.
- **Equipment and Items** (column 2): Kokiri/Razor/Gilded Sword, Hero's/Mirror Shield, Hero's Bow, Hookshot and
  Ocarina of Time. Each lists every piece of its kind from every imported pack, bodies included. OoT pieces fill the
  matching MM slot: Master Sword pieces fit the Razor Sword slot, Biggoron pieces the Gilded Sword slot and Hylian
  Shield pieces the Hero's Shield slot.
- **Emotes** (column 2): choose one, then hold L and press D-Pad Down, or press **Play Emote**. Moving or acting
  stops it.
- **Pack Library** (column 2): import PAK/ZOBJ, install O2R/OTR, root height per appearance, status.

`.pak` and `.zobj` files anywhere under the mods folder, subfolders included, are read at start (host
`ReadModsFolderFiles`). **Import PAK / ZOBJ** copies the chosen file there under a plain name; hosts without that
row get it wrapped as `pakloader_import_<hash>.o2r` instead. Restart to update the lists.

## What each pack provides

- **Bodies**: OoT adult, OoT child and MM human ZOBJs. Kind from the ZOBJ header.
- **Equipment**: Z64Online equipment ZOBJs (`EQUIPMANIFEST`) and the pieces inside every body (universal aliases
  or the zzplayas lookup tables). A held item is rebuilt as hand + pieces; the hand is the selected body's own fist,
  else the game's. The sheath limb is rebuilt from sheath, sheathed hilt (back matrix) and back shield; parts not
  replaced keep the game's own lists (MM's sheaths and back shields for Link, OoT's for Adult Link). MM draws the
  back shield on its own after the sheath, so a rebuilt sheath is drawn from `OnPlayerPostLimbDraw` with the game's
  sheath list cleared.
- **Voices**: `sounds/<sfx id>/*.ogg`, one clip picked at random per effect. Player effects are silenced through
  `OnActorPlaySfx` and the clip is mixed through `AudioMix` (SFX group, "Voice Pack Volume", 0-1 like 2Ship's other
  volumes). Voice offsets come from `voiceSfxIdOffset`.
- **Emotes**: `package.json` `emote` entry; raw link_animetion frames (134 bytes each) posed over Link at draw
  time, optional Ogg sound, optional loop.

## Formats

- zzplayas: skeleton at `0x500C`, or the fixed `0x5380` / `0x53A8` / `0x5420` (adult / child / MM) when exporters
  blank it. Body kind at `0x500B`: 0 adult, 1 child, 4 or 0x68 MM human. Back matrices at `0x5010`.
- Z64O universal: skeleton section at `0x501C`, kind at `0x501B`, 256 aliases from `0x5020`, back matrices at
  `0x5890` (swords) and `0x5950` (shields).
- Segment `0x0C` (cull list) passes through. Segment 4 textures are OoT `gameplay_keep` textures by offset, read
  from `objects/oot_gameplay_keep` (`oot_assets`); a texture missing there is drawn white.
- Matrix references (`0x0D`) are renumbered: exporters count the model's own meshed limbs, SkelAnime the base
  skeleton's. Meshes on joint-only limbs (root, lower, upper) are dropped.
- Root height follows the drawn skeleton: the model's leg length over Human Link's, OoT Adult Link's or Fierce
  Deity's.

## Boundaries

Items drawn by other actors (flying boomerang, hookshot chain, arrows, deku stick, bottles) keep the game's models.
Other forms (Deku/Goron/Zora ZOBJs) and PlayAs work files are skipped. O2R/OTR archives replace resources as a
whole. The mod changes the local player only. Hands accept MM and OoT display-list names; the Adult Link form may
resolve a limb before this mod.
