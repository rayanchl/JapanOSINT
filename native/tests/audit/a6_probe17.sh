#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_jaxa6.txt
: > "$OUT"
for f in createList.js createDatails.js; do
  echo "=== $f" >> "$OUT"
  curl -sS -L -m 25 -A 'Mozilla/5.0' "https://data.earth.jaxa.jp/en/datasets/$f" -o /tmp/a6c.js
  echo "size=$(wc -c < /tmp/a6c.js)" >> "$OUT"
  head -c 1400 /tmp/a6c.js >> "$OUT"
  echo >> "$OUT"
done
cat "$OUT"
