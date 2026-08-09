#!/usr/bin/env bash
# Show the delay-certificate table rows as served, plus the loader JS body.
set -u
UA='Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126 Safari/537.36'
OUT=/tmp/jre_dump
mkdir -p "$OUT"
curl -s -A "$UA" -L --max-time 25 \
  'https://traininfo.jreast.co.jp/delay_certificate/' -o "$OUT/page.html"
curl -s -A "$UA" -L --max-time 25 \
  'https://traininfo.jreast.co.jp/train_info/js/getinformation-jp-delaycertificate.js' \
  -o "$OUT/loader.js"
echo "=== loader.js (verbatim) ==="
cat "$OUT/loader.js"
echo
echo "=== first 6 route rows ==="
grep -oE '<td class="delaycertificate-table__route".{0,300}' "$OUT/page.html" | head -6
echo
echo "=== routename cells ==="
grep -oE 'delaycertificate-table__routename">[^<]*' "$OUT/page.html" | head -20
