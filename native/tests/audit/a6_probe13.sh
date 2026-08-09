#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_jaxa2.txt
: > "$OUT"
B="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
curl -sS -L -m 25 -A "$B" https://data.earth.jaxa.jp/en/datasets/ -o /tmp/a6d.html
cat /tmp/a6d.html >> "$OUT"
echo "=== home ===" >> "$OUT"
curl -sS -L -m 25 -A "$B" https://data.earth.jaxa.jp/ >> "$OUT"
echo "=== more guesses ===" >> "$OUT"
for u in https://data.earth.jaxa.jp/stac/ \
         https://data.earth.jaxa.jp/api/collection/ \
         https://data.earth.jaxa.jp/en/api/ \
         https://data.earth.jaxa.jp/api/collection/v2/all/ \
         https://data.earth.jaxa.jp/api/collections/ \
         https://data.earth.jaxa.jp/dataset/ ; do
  c=$(curl -sS -o /tmp/a6g.json -w '%{http_code}' -m 20 -L -A "$B" "$u")
  echo "$u [$c] $(wc -c < /tmp/a6g.json)b" >> "$OUT"
done
cat "$OUT"
