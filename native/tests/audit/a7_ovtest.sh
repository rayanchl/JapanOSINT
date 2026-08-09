#!/usr/bin/env bash
# Probe each Overpass endpoint with the airport-infra query, timing each.
Q='[out:json][timeout:180];area["ISO3166-1"="JP"][admin_level=2]->.jp;(node["aeroway"="aerodrome"](area.jp);way["aeroway"="aerodrome"](area.jp););out center;'
for EP in https://overpass-api.de/api/interpreter \
          https://overpass.kumi.systems/api/interpreter \
          https://overpass.openstreetmap.ru/api/interpreter \
          https://overpass.private.coffee/api/interpreter ; do
  echo "=== $EP"
  curl -sS -m 90 -o /tmp/a7ov.json -w 'http=%{http_code} time=%{time_total} size=%{size_download}\n' \
    -A 'JapanOSINT/1.0 (+https://github.com/rayanchl/JapanOSINT)' \
    -H 'Content-Type: application/x-www-form-urlencoded' \
    --data-urlencode "data=$Q" "$EP" 2>&1 | tail -2
  head -c 300 /tmp/a7ov.json; echo
  python3 -c "import json,sys;d=json.load(open('/tmp/a7ov.json'));print('elements=',len(d.get('elements',[])))" 2>/dev/null || echo "(not json)"
done
