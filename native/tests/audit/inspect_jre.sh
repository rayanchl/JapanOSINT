#!/usr/bin/env bash
# Dump the shape of a fetched page: class histogram + the region around a marker.
set -u
URL="$1"; MARK="${2:-}"
UA='Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126 Safari/537.36'
curl -s -A "$UA" -L --max-time 25 "$URL" -o /tmp/insp.html
echo "size=$(wc -c </tmp/insp.html)"
echo "--- class histogram ---"
grep -o 'class="[a-zA-Z0-9 _-]*"' /tmp/insp.html | sort | uniq -c | sort -rn | head -22
echo "--- script/json hints ---"
grep -o 'src="[^"]*\.js[^"]*"' /tmp/insp.html | head -10
grep -o '[a-zA-Z0-9_./-]*\.json' /tmp/insp.html | sort -u | head -10
if [ -n "$MARK" ]; then
  echo "--- around '$MARK' ---"
  grep -o ".\{0,200\}$MARK.\{0,400\}" /tmp/insp.html | head -4
fi
