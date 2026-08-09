#!/usr/bin/env bash
# Field shapes of every curated camera catalog in the retired Node service,
# plus the URL each one derived, so the C port can reproduce them exactly.
set -u
cd "$(dirname "$0")/../../.." || exit 1
c=$(git log --diff-filter=D --format=%H -1 -- server/src/collectors/_cameraSources.js)
git show "$c^:server/src/collectors/_cameraSources.js" > /tmp/cs.js
git show "$c^:server/src/collectors/cameraDiscovery.js" > /tmp/cd.js
for k in EXPRESSWAY_CAMS BROADCAST_LIVECAMS TOURISM_CAMS EARTHCAM_JP MANUAL_IP_CAMS WEBCAMENDIRECT_SEED; do
  echo "=== $k — first 3 entries ==="
  awk "/export const $k = \[/,/^\];/" /tmp/cs.js | grep -E "^\s*\{" | head -3
  echo
done
echo "=== how cameraDiscovery.js builds each url ==="
grep -nE "url: *\`|const url = \`|portalUrl|gazo|\\\$\{c\.ip\}" /tmp/cd.js | head -20
