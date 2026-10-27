#!/usr/bin/env bash
set -euo pipefail

TAG="$1"
mkdir -p dist
for platform in windows mac linux; do
    case "$platform" in
        windows) label=Win64 ;;
        mac) label=Mac ;;
        linux) label=Linux ;;
    esac
    (cd "rel/2ship-${platform}" && zip -qr "../../dist/2Ship-${TAG}-${label}.zip" .)
done
