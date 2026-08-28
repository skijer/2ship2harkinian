"""Packs a mod directory into the .o2r a 2ship install can load.

Usage: python pack_mod.py enemy_rain --binary windows_x64=out/enemy_rain.dll [--out DIR]

A mod directory holds manifest.json and the sources. The game loads prebuilt libraries only, so
every archive carries at least one binary; pass --binary once per platform you built for.
"""

import argparse
import json
import sys
import zipfile
from pathlib import Path

SDK_ROOT = Path(__file__).resolve().parent


def ParseBinaryArgument(argument: str) -> tuple[str, Path]:
    platform, separator, path = argument.partition("=")
    if not separator:
        sys.exit(f'--binary wants platform=path, got "{argument}"')

    binary = Path(path)
    if not binary.exists():
        sys.exit(f"no such binary: {binary}")

    return platform, binary


def BuildArchive(modDir: Path, binaries: dict[str, Path], outDir: Path) -> Path:
    manifestPath = modDir / "manifest.json"
    manifest = json.loads(manifestPath.read_text(encoding="utf-8"))

    declared = manifest.get("binaries", {})
    for platform in binaries:
        if platform not in declared:
            sys.exit(f'manifest declares no "{platform}" binary')

    # A platform left in the manifest without its file makes the game look for something that is
    # not in the archive.
    manifest["binaries"] = {platform: declared[platform] for platform in binaries}

    outDir.mkdir(parents=True, exist_ok=True)
    archivePath = outDir / f"{manifest['name']}.o2r"

    with zipfile.ZipFile(archivePath, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("manifest.json", json.dumps(manifest, indent=4))
        for platform, binary in binaries.items():
            archive.write(binary, declared[platform])

    return archivePath


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mod", help="mod directory under mods_sdk/")
    parser.add_argument("--binary", action="append", required=True, metavar="PLATFORM=PATH",
                        help="prebuilt library to embed, e.g. windows_x64=out/mod.dll")
    parser.add_argument("--out", type=Path, default=SDK_ROOT / "out" / "mods",
                        help="destination mods/ folder (default: mods_sdk/out/mods)")
    args = parser.parse_args()

    modDir = SDK_ROOT / args.mod
    if not modDir.is_dir():
        sys.exit(f"no such mod directory: {modDir}")

    binaries = dict(ParseBinaryArgument(argument) for argument in args.binary)
    archive = BuildArchive(modDir, binaries, args.out)

    print(f"wrote {archive}")
    print("copy the mods/ folder next to 2ship.exe")


if __name__ == "__main__":
    main()
