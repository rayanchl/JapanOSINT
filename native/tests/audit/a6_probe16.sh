#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_jaxa5.txt
: > "$OUT"
curl -sS -L -m 25 -A 'Mozilla/5.0' https://data.earth.jaxa.jp/en/datasets/script.js -o /tmp/a6s.js
echo "size=$(wc -c < /tmp/a6s.js)" >> "$OUT"
head -c 2000 /tmp/a6s.js >> "$OUT"
cat "$OUT"
