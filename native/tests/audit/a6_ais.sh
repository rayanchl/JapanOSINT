#!/usr/bin/env bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native || exit 1
for s in marine-traffic vessel-finder maritime-ais; do
  echo "--- $s"
  python3 tests/audit/verify_one.py --slot a6 --stderr --timeout 50 "$s" 2>&1 |
    grep -E 'exit:|sched:|persisted|gated|status=' | head -6
done
