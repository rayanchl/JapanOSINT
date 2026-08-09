#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_jaxa3.txt
: > "$OUT"
for f in DatasetsCard.js Header.js; do
  echo "=== $f" >> "$OUT"
  curl -sS -L -m 25 -A 'Mozilla/5.0' "https://data.earth.jaxa.jp/assets/$f" -o /tmp/a6js.js
  echo "size=$(wc -c < /tmp/a6js.js)" >> "$OUT"
  grep -o -E "[\"'\`][^\"'\`]*(api|\.json)[^\"'\`]*[\"'\`]" /tmp/a6js.js | sort -u | head -20 >> "$OUT"
  grep -o -E 'fetch\([^)]{0,120}' /tmp/a6js.js | head -10 >> "$OUT"
done
cat "$OUT"
