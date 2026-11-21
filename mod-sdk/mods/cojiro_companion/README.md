# Cojiro Companion

`drop custom_item "nei.equip.cojiro"`

A passive companion equipment below the Magic Cape and Pendant. A on its pause
slot calls or dismisses him. Majora's Mask has no Grog or Cojiro trade, so he
is only obtainable as a randomizer item (or with the console command above).

Uses OoT's cucco skeleton, animation and procedural wing/neck poses from the
OoT Asset Loader's `oot.o2r` (`objects/oot_object_niw`), in a separate friendly
actor (`nei.cojiro_partner`). MM's own cucco object lacks the beak, comb and
claw textures. He follows, hops at obstructions, recalls to grounded Link when
too far away, and respawns across scenes. He can be carried and thrown. Nearby
idle movement alternates short runs, hops and resting/pecking. He flees
briefly when hit, clucks, cries when carried and uses wing-flap sounds. He
does not run the cucco revenge swarm AI.

The three white feather textures and the border-connected feather background
of the eye texture are tinted blue in local copies of the cucco display lists.
The eye's iris, pupil and inner highlights stay as in OoT, as do beak, comb and
claw textures. This prevents a white square around the eye. Other cuccos'
shared assets are never patched. The mod ships no OoT data: the OoT Asset
Loader builds these copies (`objects/object_nei_cojiro`) from the player's own
oot.o2r into `oot_shared_assets.o2r`, and `limb_models.inc.c` binds them to the
seven cucco limbs.

Any attack type can hit him, including Link's attacks. Three distinct hits
with a 16-update invulnerability interval dismiss him for 400 active gameplay
updates. Owned/toggle state is saved; temporary hit and recovery state reset
when loading a file.

Requires `command_melody`. Play A, A, C-Up, C-Right, C-Left, C-Down near him
after learning that song. Target him to choose him over another partner.
Stick moves, A jumps, holding A slows descent, B crows, C-Left/Right rotates
the camera, and C-Up returns to Link. Carrying him also gives Link cucco
falling settings: gravity -0.5, maximum fall speed -2, and continuous
fall-start reset. These settings apply only while carrying this companion
and descending; native movement restores its own settings on other actions.
