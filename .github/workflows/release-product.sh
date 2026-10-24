#!/usr/bin/env bash
# 2S2H [Unbound] The product a release tag names, as key=value lines for $GITHUB_OUTPUT: the product
# (which picks .github/release-notes/<product>.md), the branch that builds it, the release title,
# and whether the release is a draft. A tag ending in "-test" publishes a draft, for trying the pipeline.
set -euo pipefail

TAG="$1"
case "$TAG" in
  *unbound*) echo "product=unbound"; echo "branch=unbound"; echo "title=2Ship: Unbound ${TAG}" ;;
  *) echo "tag ${TAG} names no product" >&2; exit 1 ;;
esac
case "$TAG" in
  *-test) echo "draft=true" ;;
  *)      echo "draft=false" ;;
esac
