#!/usr/bin/env bash
set -euo pipefail

TAG="$1"
mkdir -p dist
cp rel/2ship-mac/2Ship.dmg "dist/2Ship-${TAG}-Mac.dmg"
cp rel/2ship-linux/2ship.appimage "dist/2Ship-${TAG}-Linux.appimage"
cp rel/2ship-windows/2ship.lib dist/2ship.lib
cp rel/2ship-unbound-unsafe-mac/2Ship-unbound-unsafe.dmg "dist/2Ship-${TAG}-Mac-unsafe-mods.dmg"
cp rel/2ship-unbound-unsafe-linux/2ship-unbound-unsafe.appimage "dist/2Ship-${TAG}-Linux-unsafe-mods.appimage"

for edition in regular unsafe-mods; do
    source_prefix=2ship
    suffix=""
    if [ "$edition" = unsafe-mods ]; then
        source_prefix=2ship-unbound-unsafe
        suffix=-unsafe-mods
    fi
    for platform in windows mac linux; do
        case "$platform" in
            windows) label=Win64 ;;
            mac) label=Mac ;;
            linux) label=Linux ;;
        esac
        (cd "rel/${source_prefix}-${platform}" && zip -qr "../../dist/2Ship-${TAG}-${label}${suffix}.zip" .)
    done
done
