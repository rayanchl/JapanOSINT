#!/usr/bin/env bash
# feed autodiscovery from homepages
B="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_disc.txt
: > "$OUT"
for h in https://caucasuswatch.de/ https://www.caixinglobal.com/ https://www.nzherald.co.nz/ \
         https://www.nationthailand.com/ https://www.brusselstimes.com/ \
         https://www.fijitimes.com.fj/ https://www.swissinfo.ch/eng/ https://tuoitrenews.vn/ ; do
  echo "--- $h" >> "$OUT"
  curl -sS -L -m 25 -A "$B" "$h" -o /tmp/a6h.html
  echo "  http=$? size=$(wc -c < /tmp/a6h.html)" >> "$OUT"
  grep -o -i '<link[^>]*type="application/\(rss\|atom\)+xml"[^>]*>' /tmp/a6h.html | head -8 >> "$OUT"
  grep -o -i 'href="[^"]*\(rss\|feed\|\.xml\)[^"]*"' /tmp/a6h.html | sort -u | head -12 >> "$OUT"
done
echo done >> "$OUT"
