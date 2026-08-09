#!/usr/bin/env bash
B="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
echo "=== maritime-executive homepage feed links ==="
curl -sS -L -m 20 -A "$B" https://maritime-executive.com/ | grep -o -i '<link[^>]*application/rss[^>]*>' | head -10
curl -sS -L -m 20 -A "$B" https://maritime-executive.com/ | grep -o -i 'href="[^"]*rss[^"]*"' | head -10
echo "=== rferl homepage feed links ==="
curl -sS -L -m 20 -A "$B" https://www.rferl.org/ | grep -o -i 'href="[^"]*api/[a-z-]*"' | head -20
echo "=== isw blogspot ==="
for u in https://www.iswresearch.org/feeds/posts/default https://iswresearch.blogspot.com/feeds/posts/default https://www.understandingwar.org/rss.xml; do
  r=$(curl -sS -o /tmp/a6i.xml -w '%{http_code} %{size_download}' -L -m 15 -A "japanosint-collector" "$u" | tail -1)
  echo "$u -> $r items=$(grep -c -i '<item' /tmp/a6i.xml) entries=$(grep -c -i '<entry' /tmp/a6i.xml)"
done
echo "=== cfr feed titles (www.cfr.org/feed) ==="
curl -sS -L -m 15 -A "japanosint-collector" https://www.cfr.org/feed | grep -o -i '<title>[^<]*' | head -6
echo "=== cfr feed titles (feeds.cfr.org/publication/) ==="
curl -sS -L -m 15 -A "japanosint-collector" https://feeds.cfr.org/publication/ | grep -o -i '<title>[^<]*' | head -6
