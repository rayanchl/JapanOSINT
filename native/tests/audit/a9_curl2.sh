#!/usr/bin/env bash
# a9_curl2.sh <urlsfile>  -- lines: id<TAB>url ; tests with a browser UA
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0 Safari/537.36'
while IFS=$'\t' read -r id url; do
  [ -z "${id:-}" ] && continue
  case "$id" in \#*) continue;; esac
  code=$(curl -sS -o /tmp/a9b -w '%{http_code}' --max-time 25 -L \
        -H 'Accept: application/rss+xml,application/xml,text/xml,*/*' -H "User-Agent: $UA" \
        "$url" 2>/dev/null)
  n=$(grep -c -i '<item' /tmp/a9b 2>/dev/null)
  ne=$(grep -c -i '<entry' /tmp/a9b 2>/dev/null)
  sz=$(stat -c %s /tmp/a9b 2>/dev/null)
  h=$(head -c 90 /tmp/a9b | tr '\n' ' ')
  echo "$id | $code sz=$sz item=$n entry=$ne | $h"
done < "$1"
