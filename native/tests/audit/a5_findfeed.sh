#!/usr/bin/env bash
UA='Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36'
for u in "$@"; do
  echo "=== $u"
  curl -sSL -m 25 -A "$UA" "$u" 2>&1 | grep -oiE '(href|url)="[^"]*(rss|feed|atom|\.xml)[^"]*"' | sort -u | head -20
done
