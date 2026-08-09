#!/usr/bin/env bash
# a7_batch.sh <listfile> [outfile]
# listfile lines: "<SOURCE_ID>[ <entity>]"   -> runs verify_one and appends a
# compact block per source.
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native || exit 1
LIST="${1:?usage: a7_batch.sh listfile [out]}"
OUT="${2:-/dev/stdout}"
: > "$OUT"
while read -r line; do
  [ -z "$line" ] && continue
  case "$line" in \#*) continue;; esac
  # shellcheck disable=SC2086
  python3 tests/audit/verify_one.py --slot a7 --stderr --rows 2 --timeout 150 $line \
    >> "$OUT" 2>&1
  echo "#################################################" >> "$OUT"
done < "$LIST"
echo "DONE $LIST" >> "$OUT"
