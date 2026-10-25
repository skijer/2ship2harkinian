# Port verification — 2026-10-12

53/53 mods compiled and linked in Release. Installed in x64/Release/mods; the existing actor_template package was preserved.

Verified ZIP integrity, installed package hashes and embedded DLL equality against build output for all 53. Verified ModInit, ModSetApi and ModGetRequirements exports and all imports from 2ship.exe. Complete literal resource paths resolve in base game archives or the individual package. Constructed resource paths and gameplay still require runtime testing.

Game build: build/x64/mod-layout-build.log (successful).
Mods build: build/mod-sdk/port-build-verified.log (successful; compiler warnings remain).

Save Editor > Mod Items & Equipment now contains ownership controls and a clickable/draggable disposition grid. Equipment defaults to additional pages over the masks menu (L cycles pages). Changes persist in configuration and can be reset individually.

Updated builds: build/x64/save-editor-equipment-final.log and build/mod-sdk/save-editor-equipment-mods.log.

- ball_and_chain
- beetle
- byrna
- cane_pacci
- champion_tunic
- climb_boots
- deku_leaf
- demise_destruction
- divine_shield
- dominion_rod
- elemental_rods
- elemental_wand
- four_sword
- garo
- gerudo
- gust_jar
- ikana_shield
- kafei
- keaton
- kite_shield
- lantern
- magic_cape
- magic_tunic
- mic_ocarina
- minish_cap
- mogma_mitts
- pegasus_boots
- pendant
- phantom_hourglass
- rito
- roc_boots
- rocs
- rod_of_seasons
- sages_tunic
- sheikah_slate
- shovel
- somaria
- spinner
- spiritual_stones
- switch_hook
- time_gate
- trident
- trutefel_miniblin
- trutefel_molmauk
- trutefel_sbeetle
- turbo
- ultrahand
- useful_medallions
- whip
- wolf_link
- zonai_permafrost

Speed/icon fixes: spiritual_stones now scales a fresh movement target rather than multiplying retained velocity each update; climbing animation boost is bounded. Save Editor icons load registered Texture resources directly instead of checking the vanilla ExtensionCache. Give/Remove uses responsive columns; disposition cells show up to four icons plus their count. Build logs: build/x64/mod-icons-speed-final.log and build/mod-sdk/spiritual-speed-final.log (successful). Runtime reproduction remains unverified.

Added oot_assets and oot_items. Pause menu disposition now lists Items/Collectables/Masks and Vanilla/1/2/3 pages. Stones and medallions default to Collectables 1. Masks 1 is equipment plus a 3D Link preview. Equipment and collectable legacy placements are migrated once; subsequent user edits remain editable. Final logs: build/x64/pause-disposition-oot-release.log and build/mod-sdk/pause-disposition-oot-release.log. No in-game validation is claimed.

Pause visual editor and wheels: editable cell geometry is shared with the game. A cycles shared custom cells; holding A uses the wheel; C-Up describes items. Equipment uses swords/shields/tunics/boots rows, C toggles custom/vanilla in gameplay, and held-wheel release wears the chosen equipment. Collectables 1 uses OoT's 25 positions; vanilla Collectables uses the four upgrade points for quiver, bomb bag, strength and scale. Native sword/shield baselines appear on the equipment page. Link preview remains available via Show Link model. OoT Asset Loader now requires the canonical mods/oot.o2r as well as its resource markers; the older alternative archive was backed up outside mods. Logs: build/x64/pause-visual-wheels.log and build/mod-sdk/pause-visual-wheels.log. Runtime/UI/ROM extraction remain unverified.

2026-10-13 extraction crash fixed: ZRom decoded OoT DMA text files as MM YAR archives because GAME_MM was a compile-time switch. The host build now chooses YAR by the detected MM ROM CRC without modifying submodules. XML declarations were removed for the legacy ZAPD parser. Extracted secondary archives discard version/portVersion metadata and reject metadata-only results. Full NTSC 1.0 extraction using the failed-attempt ROM passed: 2,745 resources, ZIP integrity and all eight loader markers. Prepared archive installed as mods/oot.o2r; updated oot_assets.o2r installed. Logs: build/x64/oot-extraction-final.log and build/oot-extraction-regression-20261013/extraction-v2.log. Native dialogs and in-game rendering were not exercised.

2026-10-13 disposition/ownership update: vanilla Items and Masks in the disposition editor use canonical slot icons independently of saved inventory. The Link proof-of-concept preview and its UI toggle were removed. Equipment and extra Collectables hide unowned entries in the actual pause menu; disposition still shows registered entries. Added oot_equipment (54 mods total): Master Sword, Biggoron Sword, Deku Shield, OoT Mirror Shield, Goron Tunic, Iron Boots and Hover Boots, using 14 validated icon/name paths from oot.o2r. Optional equipment presentationFlags adds ownership-only entries which cannot be worn, toggled or assigned to buttons; no equipment behavior is claimed. Old binary definitions remain supported through structSize. Build logs: build/x64/oot-equipment-disposition.log and build/mod-sdk/oot-equipment-disposition.log. In-game UI validation remains unverified.

2026-10-13 quest cursor/background and tunic tint: the extra Quest cursor now uses the edited cell center and the exact page camera/matrix instead of the old (0,0) fallback. Both quest background draw paths select the real 15-piece OoT background; OoT Asset Loader includes the three missing English NES-background textures and requires them before treating extraction as complete. Installed oot.o2r has 2,748 resources and all 11 loader markers. Restored VB_APPLY_TUNIC_COLOR and the three mod callbacks: Champion blue, Magic orange/black with rupees, Sages off-white. Native human tunic material patches apply while worn and restore cosmetic/default color when removed. All 54 mods repackaged and verified. Logs: build/x64/quest-cursor-tunic-final.log and build/mod-sdk/quest-cursor-tunic.log. The real background texture was composed and visually checked; in-game interaction/rendering remains unverified.

2026-10-13 additional Collectables cursor correction: the inherited cursor-circle translation still added local z=-50 after switching to the quest page matrix/camera. Extra Quest circles now use local z=0, matching icon depth; native cursor paths retain their original depth. Page entry selects the first visible owned claim, spatial navigation skips hidden/unowned claims, and empty extra pages focus the page arrow instead of an empty quest cell. Game rebuilt successfully: build/x64/quest-cursor-depth.log. No in-game confirmation is claimed.

2026-10-13 passive pause toggles: custom items have optional onPauseToggle/isPauseActive callbacks in an ABI-compatible tail. A toggles a single passive cell; shared cells keep tap-to-cycle and held-wheel release toggles the chosen passive. Stones independently persist walking/climbing/swimming enablement in speed-effects-disabled, preserving the existing warps record and C-button warp behavior. Magic Cape now gates every magic-cost effect on its equipment toggle; Pendant uses its existing saved toggle. Quest and equipment icons use full alpha when active and half alpha when inactive; the Items screen also honors passive alpha when dispositions move entries there. Logs: build/x64/pause-effect-toggles.log and build/mod-sdk/pause-effect-toggles.log. Game and 54 mods compiled; in-game visual/interaction validation remains unverified.
