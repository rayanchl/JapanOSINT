#!/usr/bin/env bash
# a5_run.sh <listfile>   — each line: SOURCE_ID<TAB or space>entity (entity may contain spaces)
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native || exit 1
while read -r id ent; do
  [ -z "$id" ] && continue
  case "$id" in \#*) continue;; esac
  if [ -n "$ent" ]; then
    python3 tests/audit/verify_one.py --slot a5 --rows 0 --timeout "${TMO:-120}" "$id" "$ent" 2>&1 \
      | grep -E '^(===|exit:|sched:|--- persisted|record_type:)' | tr '\n' ' '
  else
    python3 tests/audit/verify_one.py --slot a5 --rows 0 --timeout "${TMO:-120}" "$id" 2>&1 \
      | grep -E '^(===|exit:|sched:|--- persisted|record_type:)' | tr '\n' ' '
  fi
  echo
done < "$1"
