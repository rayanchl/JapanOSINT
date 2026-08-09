#!/usr/bin/env bash
# a6_batch.sh <listfile> <outfile> [rows]
# listfile lines: "<SOURCE_ID>[ <entity>]"
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native || exit 1
LIST="${1:?usage: a6_batch.sh listfile out [rows]}"
OUT="${2:?out}"
ROWS="${3:-2}"
: > "$OUT"
while read -r line; do
  [ -z "$line" ] && continue
  case "$line" in \#*) continue;; esac
  # shellcheck disable=SC2086
  python3 tests/audit/verify_one.py --slot a6 --stderr --rows "$ROWS" --timeout 150 $line \
    >> "$OUT" 2>&1
  echo "#################################################" >> "$OUT"
done < "$LIST"
echo "DONE $LIST" >> "$OUT"
