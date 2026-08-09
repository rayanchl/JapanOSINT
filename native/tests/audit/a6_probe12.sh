#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_jaxa.txt
: > "$OUT"
B="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
curl -sS -L -m 25 -A "$B" https://data.earth.jaxa.jp/ -o /tmp/a6jx.html
echo "home size=$(wc -c < /tmp/a6jx.html)" >> "$OUT"
grep -o -E '(https://[a-z0-9./_-]*)?/(api|stac|catalog)[a-zA-Z0-9./_-]*' /tmp/a6jx.html | sort -u | head -30 >> "$OUT"
grep -o -E 'src="[^"]*\.js[^"]*"' /tmp/a6jx.html | head -10 >> "$OUT"
echo "--- app page" >> "$OUT"
curl -sS -L -m 25 -A "$B" https://data.earth.jaxa.jp/app/ -o /tmp/a6jx2.html
echo "app size=$(wc -c < /tmp/a6jx2.html)" >> "$OUT"
grep -o -E '/(api|stac)[a-zA-Z0-9./_-]*' /tmp/a6jx2.html | sort -u | head -30 >> "$OUT"
echo "--- guesses" >> "$OUT"
for u in https://data.earth.jaxa.jp/api/collection/v1/all \
         https://data.earth.jaxa.jp/api/v1/collection/all/ \
         https://data.earth.jaxa.jp/api/catalog/v1/all/ \
         https://data.earth.jaxa.jp/api/search/v1/all/ \
         https://data.earth.jaxa.jp/en/datasets/ \
         https://s3.ap-northeast-1.wasabisys.com/je-pds/cog/v1/index.json ; do
  c=$(curl -sS -o /tmp/a6jg.json -w '%{http_code}' -m 20 -L -A "$B" "$u")
  echo "$u [$c] $(wc -c < /tmp/a6jg.json)b $(head -c 90 /tmp/a6jg.json | tr -d '\n')" >> "$OUT"
done
cat "$OUT"
