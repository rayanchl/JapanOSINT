#!/usr/bin/env bash
B="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
echo "########## bodies of the 200-but-no-items feeds"
for u in https://www.nzherald.co.nz/arc/outboundfeeds/rss/curated/78/ \
         https://www.nationthailand.com/rss \
         https://www.brusselstimes.com/feed \
         https://www.fijitimes.com.fj/feed/ ; do
  echo "--- $u"
  curl -sS -L -m 20 -A "japanosint-collector" -H "Accept: application/rss+xml,*/*" "$u" | head -c 350
  echo; echo
done
echo "########## tuoitre"
curl -sSv -m 20 -A "japanosint-collector" https://tuoitrenews.vn/rss/homepage.rss -o /tmp/a6t.xml 2>&1 | grep -E '^\*|HTTP/' | head -12
echo "########## candidate replacements"
for u in https://www.irishtimes.com/arc/outboundfeeds/feeds/section/news/?outputType=xml \
         https://www.irishtimes.com/rss/ \
         https://www.irishtimes.com/arc/outboundfeeds/rss/?outputType=xml \
         https://caucasuswatch.de/rss.xml \
         https://caucasuswatch.de/en/feed \
         https://caucasuswatch.de/en/news/rss.xml \
         https://www.caixinglobal.com/rss/all.xml \
         https://www.caixinglobal.com/rss/china.xml \
         https://mg.co.za/rss/ \
         https://mg.co.za/section/africa/feed/ \
         https://www.swissinfo.ch/service/rss \
         https://www.swissinfo.ch/eng/rss/ ; do
  r=$(curl -sS -o /tmp/a6z.xml -w '%{http_code} %{size_download}' -L -m 15 -H "Accept: application/rss+xml,*/*" -A "japanosint-collector" "$u" 2>&1 | tail -1)
  printf '%-64s %s items=%s entries=%s\n' "$u" "$r" "$(grep -c -i '<item[ >]' /tmp/a6z.xml)" "$(grep -c -i '<entry[ >]' /tmp/a6z.xml)"
done
