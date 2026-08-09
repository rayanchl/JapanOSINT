#!/usr/bin/env bash
# a9_batch.sh <listfile> <outdir> [timeout]
# listfile lines: SOURCE_ID[<TAB>entity]
set -u
LIST="$1"; OUT="$2"; TO="${3:-120}"
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native || exit 1
mkdir -p "$OUT"
while IFS=$'\t' read -r id ent; do
  [ -z "${id:-}" ] && continue
  case "$id" in \#*) continue;; esac
  f="$OUT/$id.txt"
  if [ -n "${ent:-}" ]; then
    python3 tests/audit/verify_one.py --slot a9 --stderr --rows 3 --timeout "$TO" "$id" "$ent" > "$f" 2>&1
  else
    python3 tests/audit/verify_one.py --slot a9 --stderr --rows 3 --timeout "$TO" "$id" > "$f" 2>&1
  fi
  sched=$(grep -m1 '^sched:' "$f")
  pers=$(grep -m1 '^--- persisted:' "$f")
  echo "$id | $sched | $pers"
done < "$LIST"
