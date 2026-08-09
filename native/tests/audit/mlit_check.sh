#!/usr/bin/env bash
# Are the MLIT KSJ GeoJSON endpoints alive? lib/mlit_ksj.h treats "no mirror
# responded" as success (return 0), so a dead family never shows up as broken
# in a row-count sweep — it just quietly reports zero forever.
set -u
cd "$(dirname "$0")/../.." || exit 1
echo "=== URLs the collectors actually request ==="
grep -rhoE 'https://[^"]*KsjTmplt[^"]*|https://[^"]*/ksj/[^"]*\.geojson' \
  collectors/sources/mlit_*.c lib/mlit_ksj.c 2>/dev/null | sort -u | head -12
echo
echo "=== live status of each ==="
UA='Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/126 Safari/537.36'
for u in $(grep -rhoE 'https://[^"]*\.geojson' collectors/sources/mlit_*.c lib/mlit_ksj.c 2>/dev/null | sort -u); do
  code=$(curl -s -o /dev/null -w '%{http_code} %{size_download}' -A "$UA" -L --max-time 25 "$u")
  printf '  %-6s %s\n' "$code" "$u"
done
