#!/usr/bin/env bash
# Restore Node's camera cadence.
#
# Node ran the whole camera sweep on `cron.schedule('15 * * * *')` — hourly —
# and its registry `updateInterval` was explicitly "informational only". The C
# port made the per-source interval the real cadence and set every cam_* to
# 21600 (6 h), so the fleet refreshes 6x less often than it did, and
# public-cameras (the only surviving OSM channel) 24x less often.
set -u
cd "$(dirname "$0")/../../collectors/sources" || exit 1
echo "=== before ==="
grep -h "update_interval_sec" cam_*.c public_cameras.c 2>/dev/null \
  | grep -oE "update_interval_sec = [0-9]+" | sort | uniq -c

for f in cam_*.c; do
  sed -i 's/\.update_interval_sec = 21600/.update_interval_sec = 3600/' "$f"
done
sed -i 's/\.update_interval_sec = 86400/.update_interval_sec = 3600/' public_cameras.c

echo "=== after ==="
grep -h "update_interval_sec" cam_*.c public_cameras.c 2>/dev/null \
  | grep -oE "update_interval_sec = [0-9]+" | sort | uniq -c
