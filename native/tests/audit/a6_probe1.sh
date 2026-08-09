#!/usr/bin/env bash
# probe the RSS feeds that returned rc=-1, exactly as the collector does
# (UA japanosint-collector, 8s) and again with a browser UA.
U="japanosint-collector"
B="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
while read -r id url; do
  [ -z "$id" ] && continue
  a=$(curl -sS -o /tmp/a6f.xml -w '%{http_code} %{size_download} %{url_effective}' \
        -m 12 -H "Accept: application/rss+xml,*/*" -A "$U" "$url" 2>&1 | tail -1)
  n=$(grep -c -i '<item' /tmp/a6f.xml 2>/dev/null)
  e=$(grep -c -i '<entry' /tmp/a6f.xml 2>/dev/null)
  b=$(curl -sS -o /tmp/a6g.xml -w '%{http_code} %{size_download}' -L \
        -m 12 -A "$B" "$url" 2>&1 | tail -1)
  n2=$(grep -c -i '<item' /tmp/a6g.xml 2>/dev/null)
  e2=$(grep -c -i '<entry' /tmp/a6g.xml 2>/dev/null)
  printf '%-22s plain[%s items=%s entries=%s]  browserUA+L[%s items=%s entries=%s]\n' \
    "$id" "$a" "$n" "$e" "$b" "$n2" "$e2"
done < "$1"
