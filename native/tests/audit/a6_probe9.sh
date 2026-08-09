#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_cand2.txt
: > "$OUT"
for u in https://gateway.caixin.com/api/data/global/feedlyRss.xml \
         https://www.fijitimes.com/feed/ \
         "https://www.nzherald.co.nz/arc/outboundfeeds/rss/curated/78/?outputType=xml" \
         "https://www.nzherald.co.nz/arc/outboundfeeds/rss/section/nz/?outputType=xml" \
         https://www.nationthailand.com/rss.xml \
         https://www.nationthailand.com/api/rss \
         https://www.nationthailand.com/feed \
         https://www.brusselstimes.com/rss \
         https://www.brusselstimes.com/feed/rss \
         https://api.brusselstimes.com/rss \
         https://www.swissinfo.ch/eng/latest-news/rss \
         https://www.swissinfo.ch/eng/feed/ \
         https://www.swissinfo.ch/rss \
         https://tuoitre.vn/rss/tin-moi-nhat.rss ; do
  r=$(curl -sS -o /tmp/a6z.xml -w '%{http_code} %{size_download}' -L -m 15 -H "Accept: application/rss+xml,*/*" -A "japanosint-collector" "$u" 2>&1 | tail -1)
  printf '%-70s %s items=%s entries=%s\n' "$u" "$r" "$(grep -c -i '<item[ >]' /tmp/a6z.xml)" "$(grep -c -i '<entry[ >]' /tmp/a6z.xml)" >> "$OUT"
done
cat "$OUT"
