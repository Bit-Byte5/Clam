#!/usr/bin/env bash
# Download the combined cross-platform .geode from a GitHub Release tag.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${1:-}"
REPO="${CLAM_GITHUB_REPO:-Bit-Byte5/Clam}"

if [[ -z "$VERSION" ]]; then
  VERSION="$(python3 -c "import json; print(json.load(open('$ROOT/mod.json'))['version'])")"
fi

TAG="v${VERSION#v}"
OUT_DIR="$ROOT/build/release"
OUT_FILE="$OUT_DIR/paxcirlot.clam.geode"

mkdir -p "$OUT_DIR"

echo "Downloading $REPO release $TAG ..."
gh release download "$TAG" --repo "$REPO" -p "paxcirlot.clam.geode" -D "$OUT_DIR" --clobber

if [[ ! -f "$OUT_FILE" ]]; then
  echo "Release asset not found at $OUT_FILE" >&2
  exit 1
fi

echo "Saved universal build: $OUT_FILE ($(du -h "$OUT_FILE" | cut -f1))"
