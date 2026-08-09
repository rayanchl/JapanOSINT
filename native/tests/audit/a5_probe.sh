#!/usr/bin/env bash
# probe a list of URLs the way rss_collect does: plain GET, UA japanosint-collector
UA='japanosint-collector'
while IFS=$'\t' read -r id url; do
  [ -z "$id" ] && continue
  out=$(curl -sSL -o /tmp/a5probe.body -w '%{http_code} %{size_download} %{content_type}' \
        -m 20 -H "Accept: application/rss+xml,*/*" -A "$UA" "$url" 2>&1)
  items=$(grep -oi '<item' /tmp/a5probe.body 2>/dev/null | wc -l)
  entries=$(grep -oi '<entry' /tmp/a5probe.body 2>/dev/null | wc -l)
  enc=$(head -c 200 /tmp/a5probe.body | tr -d '\0' | grep -oi 'encoding="[^"]*"' | head -1)
  printf '%-24s %s items=%s entries=%s %s\n' "$id" "$out" "$items" "$entries" "$enc"
done < "$1"
