#!/bin/bash
Q='[out:json][timeout:60];area["ISO3166-1"="JP"][admin_level=2]->.jp;(node["amenity"="townhall"](area.jp););out center 5;'
for ep in https://overpass-api.de/api/interpreter https://overpass.kumi.systems/api/interpreter https://z.overpass-api.de/api/interpreter; do
  echo "-- $ep"
  code=$(curl -s --max-time 90 -o /tmp/ov.json -w '%{http_code}' -d "data=$Q" "$ep")
  echo "   http=$code size=$(stat -c%s /tmp/ov.json 2>/dev/null)"
  head -c 250 /tmp/ov.json | tr '\n' ' '; echo
done
echo "=== overpass endpoints in lib"
grep -n 'overpass' /mnt/c/Users/rayan/sources/repos/OSINTsaas/native/lib/overpass.c | head -20
