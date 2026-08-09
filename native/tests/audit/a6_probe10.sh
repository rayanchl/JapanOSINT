#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_cand3.txt
: > "$OUT"
B="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
probe() {  # url [ua]
  ua="${2:-japanosint-collector}"
  code=$(curl -sS -o /tmp/a6z.xml -w '%{http_code}' -L -m 15 -H "Accept: application/rss+xml,*/*" -A "$ua" "$1" 2>&1 | tail -1)
  n=$(grep -o -i '<item[ >]' /tmp/a6z.xml | wc -l)
  e=$(grep -o -i '<entry[ >]' /tmp/a6z.xml | wc -l)
  echo "--- $1  [$code] items=$n entries=$e" >> "$OUT"
  grep -o -i '<title>[^<]\{1,70\}' /tmp/a6z.xml | head -3 >> "$OUT"
}
probe https://gateway.caixin.com/api/data/global/feedlyRss.xml "$B"
probe https://api.brusselstimes.com/rss
probe "https://www.nzherald.co.nz/arc/outboundfeeds/rss/curated/78/?outputType=xml"
probe "https://www.irishtimes.com/arc/outboundfeeds/rss/?outputType=xml"
probe https://mg.co.za/rss/
probe https://kyivindependent.com/feed/rss/
probe https://maritime-executive.com/articles.rss
probe https://www.rferl.org/api/zbqiml-vomx-tpeqkmy
probe https://www.cfr.org/feed
echo "--- tuoitrenews www cert" >> "$OUT"
curl -sS -o /dev/null -w 'www %{http_code}\n' -m 15 https://www.tuoitrenews.vn/rss/homepage.rss >> "$OUT" 2>&1
cat "$OUT"
