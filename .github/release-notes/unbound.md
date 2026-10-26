# 2Ship: Unbound

Play Majora's Mask with native code mods and the in-game NEI-Store.

## Download

**Choose your system and download a ZIP. The regular edition is recommended for players.**

| System | Regular — signed mods | Unsafe — also allows unsigned mods |
| --- | --- | --- |
| Windows (64-bit) | [Download ZIP](https://github.com/skijer/2ship2harkinian/releases/download/{{TAG}}/2Ship-{{TAG}}-Win64.zip) | [Download unsafe ZIP](https://github.com/skijer/2ship2harkinian/releases/download/{{TAG}}/2Ship-{{TAG}}-Win64-unsafe-mods.zip) |
| macOS (Intel / Apple Silicon) | [Download ZIP](https://github.com/skijer/2ship2harkinian/releases/download/{{TAG}}/2Ship-{{TAG}}-Mac.zip) | [Download unsafe ZIP](https://github.com/skijer/2ship2harkinian/releases/download/{{TAG}}/2Ship-{{TAG}}-Mac-unsafe-mods.zip) |
| Linux (64-bit) | [Download ZIP](https://github.com/skijer/2ship2harkinian/releases/download/{{TAG}}/2Ship-{{TAG}}-Linux.zip) | [Download unsafe ZIP](https://github.com/skijer/2ship2harkinian/releases/download/{{TAG}}/2Ship-{{TAG}}-Linux-unsafe-mods.zip) |

## Setup

1. Download and extract the ZIP for your system.
2. On Windows, launch `2ship.exe`. On macOS, open the included DMG and install the app. On Linux, make the AppImage executable and launch it.
3. Follow the first-run setup with your own supported Majora's Mask ROM.
4. Open **Network > NEI-Store** to install signed mods, or place `.o2r` mod packages in `mods/`.

The **unsafe** edition is for mod developers and people testing unsigned mods. It asks for permission before loading each unsigned mod. Only allow mods you trust.

The regular edition loads signed mods from the [NEI-Store](https://github.com/skijer/2ship2harkinian/releases/tag/mods). Both editions include the mod API.

The DMG and AppImage downloads under **Assets** are also available directly. `2ship.lib` is for mod developers; `unbound-release.json` is used by the in-game updater. GitHub's **Source code** ZIP is not a playable build.

[Mod SDK and documentation](https://github.com/skijer/2ship2harkinian/tree/unbound-modapi/mod-sdk)
