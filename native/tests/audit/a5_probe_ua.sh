#!/usr/bin/env bash
# same as a5_probe but with a browser UA, to isolate UA-gating
UA='Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36'
while IFS=$'\t' read -r id url; do
  [ -z "$id" ] && continue
  out=$(curl -sSL -o /tmp/a5probe2.body -w '%{http_code} %{size_download} %{content_type}' \
        -m 20 -H "Accept: application/rss+xml,application/xml,text/xml,*/*" -A "$UA" "$url" 2>&1)
  items=$(grep -oi '<item' /tmp/a5probe2.body 2>/dev/null | wc -l)
  entries=$(grep -oi '<entry' /tmp/a5probe2.body 2>/dev/null | wc -l)
  printf '%-24s %s items=%s entries=%s\n' "$id" "$out" "$items" "$entries"
done < "$1"
