#!/usr/bin/env bash
# Re-run named sources and print just the verdict lines.
# A file, not an inline loop: `wsl.exe -- bash -c "for s in ...; do ... $s"` from
# PowerShell loses the loop variable and runs every iteration with an empty id.
set -u
cd "$(dirname "$0")/../.." || exit 1
for s in "$@"; do
  echo "=== $s"
  timeout 340 python3 tests/audit/verify_one.py --rows 0 "$s" 2>&1 \
    | grep -E 'sched:|persisted:|overpass\]|rss\]|no \[sched\]' | head -4
done
