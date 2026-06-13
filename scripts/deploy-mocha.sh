#!/usr/bin/env bash
# Upload a .geode build to Mocha on saltmine. Defaults to universal (all platforms).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GEODE_FILE="${1:-$ROOT/build/release/paxcirlot.clam.geode}"
PLATFORM="${PLATFORM:-universal}"
REMOTE="${MOCHA_SSH_HOST:-saltmine}"
REMOTE_NAME="$(basename "$GEODE_FILE")"
REMOTE_PATH="/tmp/$REMOTE_NAME"

if [[ ! -f "$GEODE_FILE" ]]; then
  echo "Missing .geode file: $GEODE_FILE" >&2
  echo "Run scripts/fetch-release-geode.sh after a GitHub Release, or pass a path." >&2
  exit 1
fi

echo "Uploading $GEODE_FILE to Mocha (platform=$PLATFORM) ..."
scp "$GEODE_FILE" "$REMOTE:$REMOTE_PATH"

ssh "$REMOTE" "TOKEN=\$(cd /var/www/saltmine/backend && node scripts/admin-jwt.mjs) && curl -sS -X POST -H \"Authorization: Bearer \$TOKEN\" -F \"geode=@$REMOTE_PATH\" -F \"platform=$PLATFORM\" http://127.0.0.1:5000/api/mocha/admin/builds"

echo ""
echo "Mocha upload complete."
