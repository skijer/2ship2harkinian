# Unbound mod protocol and parity status

## Target

Ship and 2Ship should expose equivalent mod facilities while preserving OoT and MM native behavior.
The intended Combo direction is an MM engine module hosted by Ship, not two standalone executables.
This change does not implement that engine module or a Combo run dependency gate.

## Packages

New packages stamp `format: 2`, `game: "oot" | "mm"`, and a positive integer `modapi`.
Both loaders reject unknown formats, mismatched games and mismatched ABI majors before extracting/loading native code.
Format 2 requires explicit game and major. Historical untagged packages retain their original host-local major-1 interpretation;
there is no reliable way to infer their game from the old manifest alone.
The platform `binaries` map and existing signature domains are unchanged.
Table additions are appended; these two game tables are not binary interchangeable.

The authoritative OoT NEI checkout remains `Shipwright-unbound-mod-nei/mod-sdk/mods`.
Its packager and SDK now stamp the same contract. No mod source was replaced.

## Store

Catalog format 2 retains numeric OoT build keys for migration and adds `oot:<major>` / `mm:<major>` keys.
Explicitly scoped keys take precedence; MM never selects a numeric OoT fallback.
Per-game `variants` hold name, author, version, description, repository, permissions and dependencies.
Publishing one game preserves the other game's build and metadata. Root metadata remains the OoT presentation when available.
Dependencies remain local to the selected game. A future Combo run must require each game's package explicitly;
a matching shared mod ID alone is insufficient.
Release manifests carry `game`; historical release manifests without it are OoT-only.
Existing OoT upload filenames are preserved; MM assets use `<id>-mm-api<major>.o2r`.

2Ship now has the store worker, catalog parser, HTTPS transport, menu, install/disable/remove staging,
signed-document verification and Windows release updater. The current generated Windows project includes the new sources.
The menu lives under Network > NEI-Store. Update staging runs before OTR initialization; queued package operations run before mod scanning.
The 2Ship origin repository supplies its catalog/release URLs. Remote publication workflows and deployment were not run.
2Ship's existing zero mod signing key is preserved, and its new release key is also unset: signed store content
will not verify until the appropriate public keys and signed release assets are configured.
HTTPS uses WinHTTP on Windows; non-Windows curl support needs build integration when that target is enabled.

## Behavior discovery

MM provides HasVanillaBehavior and RegisterVBByName for its native behavior discovery and adapts toward the existing OoT API. OoT's API and requirements layout remain the reference: the additions previously made there were removed. Behavior names do not guarantee identical callback argument layouts. Run MM's sync_behavior_names.py --check after MM behavior enum edits.
2Ship also appends archive read/list visitors and player-selected archive mounting.

## Implemented MM services

All 159 Ship service names now exist in the MM table; MM exposes 168 services including nine MM-side entries.
Every entry in both tables has an assignment. This is source API coverage, not binary interchangeability or proof
of identical gameplay semantics across different engines.

MM now implements progressive item chains with save/logic banks, native item delivery and rollback; scoped logic
grants; custom worn items and remote rendering; native labels, icons and time splits; and dynamic audio sequences
and soundfonts. Mod initialization runs after native audio tables are ready.

MM accepts an optional appended ModRequirements requiredBehaviors list and reject an unavailable named
behavior before ModInit. Older requirements structures remain valid through their original prefix size.

## Anchor provenance and extensions

The native MM Anchor base is imported from upstream PR #1349:
https://github.com/2ship2harkinian/2ship2harkinian/pull/1349
Pinned commit: 3820c56b44e8b94e99deb3a3648a8e2286d26348.
The unrelated Sail and action refactors were not imported.

Anchor is wired into the native menu and lifecycle with SDL2_net. Incoming state changes apply on the game thread.
The port adds the 19 mod networking table services, peer/room state, channels, teleport/summon/countdown, custom
form and held/worn item rendering, owned equipment/items, custom flags, progressive state and stable custom check
item keys. Native flag packet handlers are implemented rather than retaining upstream placeholders. Incoming
application suppresses outgoing echoes. Save-bank item progress is synchronized; MM time synchronization is not added.
Remote peers require the corresponding form/item mods for their definitions; missing team-state item definitions
produce a notification instead of treating another installation's runtime numeric IDs as local IDs.

## Validation and remaining execution checks

The protocol suites cover package identity, game/major rejection, independent per-game catalog ownership/listing,
publication preservation and signature domains. Source permission scanning recognizes MM network and archive mounting.
MSVC syntax-only checks passed for the new Anchor/Mod API/store code and modified native integration points.
Release x64 native compilation and executable linking passed on 2026-10-23. The local dependency cache needed its empty stb_image.h restored from the existing sibling installation; new object output paths were shortened in the generated project. No relay session or in-game validation was run. Signed store publication still
requires configured public keys and signed assets as described above.

The MM-as-a-mod engine lifecycle and Combo run dependency gate remain explicitly deferred by the user's latest scope.
The checked-in modapi-parity-status.json records table coverage and separates syntax verification from runtime validation.

## Content audit correction (2026-10-23)

Table coverage is not full functional parity. The authoritative OoT checkout has 71 mod folders and MM has 55.
See mod-content-parity-status.json for missing folders and shared mods lacking Ship networking integration.
MM oot_equipment explicitly implements ownership/icons only, without equipment behavior.
The store menu now matches Ship at Network > NEI-Store; signing keys and publication workflows remain incomplete.

The MM store uses the 2Ship repository for its own catalog and releases, with independent publication and signing configuration. Shared package protocol and future Combo identities do not require a shared catalog.
