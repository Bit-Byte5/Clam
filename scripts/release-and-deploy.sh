#!/usr/bin/env bash
# Tag push -> GitHub universal build -> Mocha upload (all platforms).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VERSION="$(python3 -c "import json; print(json.load(open('mod.json'))['version'])")"
TAG="v${VERSION#v}"
REPO="${CLAM_GITHUB_REPO:-Bit-Byte5/Clam}"

echo "Clam release + Mocha deploy for $TAG"

if ! git diff --quiet || ! git diff --cached --quiet || [[ -n "$(git ls-files --others --exclude-standard)" ]]; then
  echo "Committing pending changes ..."
  git add -A
  git commit -m "Release $TAG: sync improvements, UI fixes, custom menu icon."
fi

echo "Pushing main ..."
git push origin HEAD

if git rev-parse "$TAG" >/dev/null 2>&1; then
  echo "Tag $TAG already exists locally."
else
  git tag "$TAG"
fi

echo "Pushing tag $TAG ..."
git push origin "$TAG"

echo "Waiting for GitHub Release workflow ..."
gh run watch "$(gh run list --repo "$REPO" --workflow "Release Geode Mod" --limit 1 --json databaseId --jq '.[0].databaseId')" --repo "$REPO" --exit-status

echo "Downloading universal .geode ..."
"$ROOT/scripts/fetch-release-geode.sh" "$VERSION"

echo "Uploading to Mocha (universal / all platforms) ..."
PLATFORM=universal "$ROOT/scripts/deploy-mocha.sh" "$ROOT/build/release/paxcirlot.clam.geode"

echo "Done. $TAG is on GitHub Releases and Mocha (universal)."
