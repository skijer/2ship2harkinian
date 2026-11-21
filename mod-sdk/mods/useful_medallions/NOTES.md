# useful_medallions: 2Ship port

The six medallions register standalone custom items and spell actors. Spell animations come from `oot_gameplay_keep` when oot_assets is installed. Fire, ice and light arrows use native MM arrow actions; dark, soul and wind effects attach custom SW97 actors to normal arrows.

Select the bow in Kaleido and press A to cycle owned elemental medallions. The selected element appears in button icons and below the bow cell.

- Water: enemies the spell touches freeze for 6 seconds, and water inside it turns into MM's Ice Arrow floes (BgIcefloe, run through a mod-owned profile so no scene needs OBJECT_ICEFLOE).
- Shadow: enemies and NPCs lose sight of Link while the timer runs; it is not a shield.
- Spirit: flight as a custom MM EnElf, stick to steer, A/R for height, B to leave.
- Casting cameras: Fire uses the Song of Soaring camera and the other spells the bottle camera, MM's closest matches to OoT's Din's Fire cutscene and turn-around camera. Fast Farore's casts without one.
- OoT sounds MM lacks are mapped in `sw97_compat.h` to the closest MM sound.

MM has no slingshot or OoT mud wall: elemental seeds and mud-wall melting are not implemented. Costs: Forest 12, Spirit 24, Shadow 24, Water 12, Light 24, Fire 12.
