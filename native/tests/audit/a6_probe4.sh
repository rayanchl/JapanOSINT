#!/usr/bin/env bash
B="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
echo "=== maritime articles.rss ==="
for u in https://maritime-executive.com/articles.rss https://www.maritime-executive.com/articles.rss; do
  r=$(curl -sS -o /tmp/a6m.xml -w '%{http_code} %{size_download}' -m 15 -H "Accept: application/rss+xml,*/*" -A "japanosint-collector" "$u" | tail -1)
  echo "$u -> $r items=$(grep -c -i '<item' /tmp/a6m.xml)"
done
grep -o -i '<title>[^<]*' /tmp/a6m.xml | head -4
echo "=== isw blogspot no-redirect, browser UA ==="
curl -sS -o /tmp/a6b.xml -w 'iswresearch %{http_code} %{size_download} %{redirect_url}\n' -m 15 -A "$B" https://www.iswresearch.org/feeds/posts/default | tail -2
head -c 300 /tmp/a6b.xml
echo
echo "=== rferl rssfeeds page ==="
curl -sS -L -m 20 -A "$B" https://www.rferl.org/rssfeeds | grep -o -i 'href="[^"]*"' | grep -i -E 'api/|rss' | head -30
