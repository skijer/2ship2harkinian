# Signing

A mod's binary runs inside the game with the same rights as the game. A build with the `signed` policy only
loads a binary that the **sign-mod** workflow of this repository built and signed; anything else is refused and
listed when the game starts. Asset-only packages (no `binaries` in the manifest) are not affected.

## Three kinds of build

| `S2H_MOD_POLICY` | What loads |
| --- | --- |
| `open` (current default) | Every mod; the Mod Menu still shows which are signed |
| `ask` | Signed mods, plus unsigned ones after the player says yes (the `unsafe-mods` downloads) |
| `signed` | Only mods signed by the sign-mod workflow |

Set it with `cmake ... -DS2H_MOD_POLICY=signed`. The policy is fixed when the game is compiled: no setting,
config file or mod can switch it.

With `ask`, an unsigned mod opens a prompt the first time: its name, why it is not signed, anything its binary
imports from the system beyond what mods need, and its content hash. "Always load this version" is remembered by
that hash in `mods/.2ship-mod-approvals.json`; changing a single byte of the package asks again.

## Getting a mod signed

1. Push your mods to a **public** repository you own: one folder per mod with `manifest.json` (`"game": "mm"`),
   its sources and `assets/`. Add `UNBOUND_REF` to pin the 2Ship Unbound release (or tag, branch or commit) to
   build against; without it the newest Unbound release is used, or the current commit while there is none.
2. Open a **Sign a mod** issue in this repository with the repository, the full commit hash and the mods folder.
   Keep it open: it is your mods' page in the in-game **NEI-Store** (Network > NEI-Store).
3. The workflow answers on the issue: the signed mods, now in the store with their download links, or the step
   that failed.

Requests are signed one at a time, oldest first. Your first request waits until a maintainer approves it
(label `approved`); once you are on the trusted list, later ones go straight to the queue.

### Updates

After the first approval, a new version needs no new issue: publish a GitHub release in your repository (the
workflow checks every two hours), or comment `/sign <tag, branch or commit>` on your issue. Only mods whose
sources changed are signed again. A new version that asks for a permission its mod did not have before waits
for a maintainer to add `approved` again, and so does changing the repository or folder in the issue.

`/unlist <mod>` takes one of your mods out of the store and `/relist <mod>` brings it back. A mod's name is its
folder name, and the first issue that publishes a name keeps it.

### Packs and dependencies

A `packs.json` next to your mod folders groups them; players install a whole pack or only the parts they tick:

```json
{ "packs": [ { "id": "my-items", "name": "My Items", "mods": ["whip", "lantern"] } ] }
```

A mod that needs another one names it in its manifest, and the store installs both:

```json
"dependencies": ["oot_assets"]
```

### Unbound versions

`S2H_MOD_API_MAJOR` in `mm/2s2h/ModApi/ModApi.h` goes up only when a change breaks mods built before it; adding
entries to `S2HModApi` does not. The SDK stamps it into every package, and the game refuses packages built for
another major. When a release bumps it, `publish-release.yml` rebuilds every listed mod at its last signed commit
before the release goes public, and reports each result on its issue.

The signature names your GitHub account, the repository and the commit, and links to the workflow run with every
scan and build log. The game shows it in the Mod Menu next to the file name.

## Permissions

A mod that needs the microphone or the player's files declares it in its manifest and gets them only through the
game's services:

```json
"permissions": ["microphone"]
```

| Permission | What the player sees |
| --- | --- |
| `microphone` | Asked once per mod (kept in `mods/.2ship-mod-permissions.json`); an icon with the mod's name shows while it records |
| `files` | Asked every time: a file is only read once the player picks it, and writes only land in `mods/` |

The game identifies the calling mod by the binary the call comes from, so one mod cannot use another's
permission. The scan refuses a mod whose code calls a service its manifest does not declare.

## What the workflow checks

Nothing of your repository runs while it is checked or built: no CMake, scripts or hooks of yours. The build uses
this repository's `mod-sdk/ci/signing/CMakeLists.txt`, which compiles each mod folder with the SDK defaults.

| Step | Refuses |
| --- | --- |
| Layout | Build scripts, executables, symbolic links, and files other than sources, docs, `manifest.json` and a top-level `packs.json` outside `assets/` |
| Manifest | Unknown permissions; `dependencies` that are not mod folder names; `build` include folders outside the mod |
| Directives | Includes that leave the mod's folder or reach the operating system, and any `#pragma` but `once`, `pack`, `warning`, `region` and diagnostics |
| Preprocessed code | Inline assembly and calls that open files, start processes or threads, load libraries or use the network |
| [Cppcheck](https://cppcheck.sourceforge.io/) | Any error |
| [Semgrep](https://semgrep.dev/) `p/c` | Any finding of severity ERROR |
| Binary imports | Anything a binary imports outside the game, the C/C++ runtime and the CRT start-up |

A mod that needs any of those must ask the game for it through the ModApi. A signature does not prove a mod is
harmless inside the game; it makes its author identifiable by account, repository and exact commit.

## Maintainers

The keys are set up: `TrustedModKey.h` and `TrustedReleaseKey.h` hold the public halves, and the private ones are
the secrets `S2H_MOD_SIGNING_KEY` and `S2H_RELEASE_SIGNING_KEY` of the **mod-signing** environment (default branch
only). Replacing a key means `python mod-sdk/tools/mod_signature.py generate-key [--kind release]
--private-key-output <file>`, storing the file as the secret, deleting it and committing the header.

- **Build:** run **generate-builds** by hand, or push a `*unbound*` tag to build and publish a plain release.
- **Release for the updater:** run **publish-release** with the id of a successful generate-builds run less than
  a day old. It drafts the release with a signed `unbound-release.json`, rebuilds the mods if the mod API major
  changed, and publishes it as latest. Bump `PROJECT_FORK_VERSION` in `CMakeLists.txt` first.
- **Approve a new author:** add `approved` to their issue; to skip approval from then on, add their login to
  `.github/mod-signing/trusted-authors.txt`.
- **Block an author:** add their lowercase login to `kRevokedModAuthors` in
  `mm/2s2h/ModApi/ModTrust/RevokedMods.h`; one package: its `content_hash` to `kRevokedModPackages`.
- **Store links:** `.github/mod-signing/store-links.json`, published with the next signature.
- **Mod previews:** add the `generate-mods` label to a pull request to build its `mod-sdk/mods` as a prerelease.
