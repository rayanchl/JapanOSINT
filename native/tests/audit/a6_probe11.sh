#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_probe11.txt
: > "$OUT"
echo "### note.com search API" >> "$OUT"
for u in "https://note.com/api/v2/searches?context=user&q=OSINT&size=20" \
         "https://note.com/api/v3/searches?context=user&q=OSINT&size=20" \
         "https://note.com/api/v2/searches?context=note&q=OSINT&size=5" ; do
  code=$(curl -sS -o /tmp/a6n.json -w '%{http_code}' -m 20 -H 'user-agent: JapanOSINT/1.0' -H 'accept: application/json' "$u")
  echo "--- $u [$code] $(wc -c < /tmp/a6n.json)b" >> "$OUT"
  head -c 400 /tmp/a6n.json >> "$OUT"; echo >> "$OUT"
done
echo "### jaxa endpoints" >> "$OUT"
for u in https://data.earth.jaxa.jp/api/collection/v1/all/ \
         https://data.earth.jaxa.jp/api/collection/ \
         https://data.earth.jaxa.jp/api/datasets/ \
         https://data.earth.jaxa.jp/api/ \
         https://data.earth.jaxa.jp/stac/catalog.json ; do
  code=$(curl -sS -o /tmp/a6j.json -w '%{http_code}' -m 20 -L "$u")
  echo "--- $u [$code] $(wc -c < /tmp/a6j.json)b" >> "$OUT"
  head -c 300 /tmp/a6j.json >> "$OUT"; echo >> "$OUT"
done
cat "$OUT"
