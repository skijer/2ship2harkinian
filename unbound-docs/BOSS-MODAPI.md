# Boss ModApi for MM

`S2HModApi` appends `RegisterBoss`, `GetBossSlot`, `GetSceneBoss`, `GetBossSlotScene`, and
`ReportBossDefeated`. Check `S2H_MOD_API_HAS(api, ReportBossDefeated)` before using this extension.
Definitions and slot masks are in `mm/2s2h/ModApi/ModDungeons/BossRegistry.h`, included by `ModApi.h`.
Existing table member offsets are preserved. Native mods must be rebuilt after the expanded game structs.

A definition supplies a unique key, display name, optional SceneDB scene key, allowed slot mask,
preferred slot (-1 for any), optional logic expression, and optional enabling CVar. Registration copies strings.
Slots are Odolwa, Goht, Gyorg, Twinmold, and Majora. Dungeon and final masks are separate.
Register scenes before entering an arena. A missing scene or occupied compatible slots leaves the boss unused.

Normal files use preferred compatible slots. Randomizer files assign compatible slots from the final seed,
then save boss keys in the `unboundBosses` save section and spoiler. Reloading preserves this assignment;
missing keys or scenes fall back to vanilla. Scene resources come from the custom arena while the running
scene keeps the vanilla slot identity for rewards, exits, and existing boss hooks.

Call `ReportBossDefeated(actor)` from the custom boss when it dies. Reporting is deduplicated per actor until
destruction or scene initialization. The game emits the vanilla slot's boss-defeated hook. The mod remains
responsible for its actor behavior, arena exit/warp and final ending flow.

Logic accepts `and`/`or`/`not`, `&&`/`||`/`!`, parentheses, `true`/`false`, the existing ModApi capabilities,
`cap(name)`, `canUse(name)`, and `has(name)`. Item names accept vanilla display/spoiler names or registered
custom item keys. Quote names containing spaces, for example `canUse("Hero's Bow")`. Invalid expressions
log an error and evaluate false. Empty logic retains the slot's vanilla kill requirement.
Expressions are limited to 8192 bytes and nesting depth 128.

The language also accepts MM enum names (`RI_*`, `RO_*`, `RE_*`, `ACTOR_*`), `canKill(ACTOR_*)`,
`event(RE_*)`, `setting(RO_*, n)`, `trick(RO_*)`, and `tokens(n)`. Dungeon expressions additionally
use `keys(n)`, `bosskey`, and local `event(name)`. Names from OoT's enums are not MM item or actor ids.

Rules replace the four dungeon arenas' existing `CanKillEnemy` conditions in randomizer logic. MM's current
Majora region has no equivalent completion condition; the final slot supplies arena substitution and reporting,
but does not add a new randomizer completion rule. Dungeon registration, key contexts, custom checks,
logic events, and coupled dungeon entrance shuffle are described in [DUNGEONS.md](DUNGEONS.md).

The Windows Release target has compiled. In-game validation is still required for vanilla arenas,
seed save/reload, missing-mod fallback, and a custom boss death/reward flow.
