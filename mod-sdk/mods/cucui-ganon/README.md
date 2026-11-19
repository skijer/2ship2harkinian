# Cucui Ganon for 2Ship

A custom Ganondorf actor that takes Majora's slot in `SCENE_LAST_BS`. Enable **Cucui Ganon battle** in **Mods → Gameplay** and enter the final battle. It has its own drawing with `gGanondorfSkel`, its own intro and camera, and keeps the Ship version's fight and lyrics. Needs the OoT Asset Loader. After the finisher, MM's victory helper rolls the credits.

## Fight

- Ganondorf has 80 internal points and summons four Phantoms at 40. Back hits and back arrows take 1 point.
- Three back arrows or reflected orbs knock him down, vulnerable for 120 updates; an orb that completes the count takes 5 points.
- He throws fire, ice, electric and explosive orbs from his hands, raises rocks, sends shockwaves and dashes. Orbs can be reflected with the sword.
- The Phantoms use `object_gnd` at 80% scale. Each dies to a back hit, a Bomb Arrow, or a reflected orb aimed at it. While any remain, Ganondorf is shielded; once all four fall, one reflected orb stuns him.
- At 1 point, a back slash during a stun triggers the finisher.

The boss title card reads **papi chulo** over **Ganondorf**. Tatl's C-Up descriptions are in English. Spanish lyrics scroll on their own translucent layer from the start of the dance; they hide while a real game message is open. `ganondorf_lyrics.inc` holds the 169 verses, synced as 610 beats at 125 BPM.

## Music

The mod interrupts `NA_BGM_OPENING_GANON`, plays the RC1066 intro and entrance, one loop pass without the three vocal tracks, then the full loop, using fonts 22, 27 and 32 from `oot.o2r`. Animations and meshes are referenced by OTR path for resource-mod compatibility.
