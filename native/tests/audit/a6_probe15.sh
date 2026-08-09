#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_jaxa4.txt
: > "$OUT"
curl -sS -L -m 25 -A 'Mozilla/5.0' https://data.earth.jaxa.jp/en/datasets/ -o /tmp/a6d.html
echo "=== tail of datasets page ===" >> "$OUT"
tail -c 1600 /tmp/a6d.html >> "$OUT"
echo >> "$OUT"
echo "=== url-ish strings ===" >> "$OUT"
grep -o -E '[a-zA-Z0-9./_-]*\.(json|js)' /tmp/a6d.html | sort -u >> "$OUT"
for u in https://data.earth.jaxa.jp/assets/datasets.json \
         https://data.earth.jaxa.jp/en/datasets/datasets.json \
         https://data.earth.jaxa.jp/api/collection/v1/all/index.json ; do
  c=$(curl -sS -o /tmp/a6g.json -w '%{http_code}' -m 20 -L -A 'Mozilla/5.0' "$u")
  echo "$u [$c] $(wc -c < /tmp/a6g.json)b $(head -c 80 /tmp/a6g.json | tr -d '\n')" >> "$OUT"
done
cat "$OUT"
