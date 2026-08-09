#!/usr/bin/env bash
Q='[out:json][timeout:60];area["ISO3166-1"="JP"][admin_level=2]->.jp;(node["emergency"="defibrillator"](area.jp););out center;'
for ep in https://overpass-api.de/api/interpreter \
          https://overpass.kumi.systems/api/interpreter \
          https://overpass.openstreetmap.ru/api/interpreter \
          https://overpass.private.coffee/api/interpreter ; do
  printf '%-52s ' "$ep"
  curl -sS -m 120 -o /tmp/ovp.json -w 'http=%{http_code} t=%{time_total} bytes=%{size_download}' \
    --data-urlencode "data=$Q" "$ep" 2>&1 | head -1
  echo "  elems=$(grep -o '"type"' /tmp/ovp.json 2>/dev/null | wc -l)"
done
