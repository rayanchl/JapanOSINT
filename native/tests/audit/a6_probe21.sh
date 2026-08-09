#!/usr/bin/env bash
for h in https://overpass-api.de/api/status \
         https://overpass.kumi.systems/api/status \
         https://overpass.openstreetmap.ru/api/status \
         https://overpass.osm.jp/api/status ; do
  echo "--- $h"
  curl -sS -o /tmp/a6ov.txt -w '  code=%{http_code} time=%{time_total}\n' -m 12 "$h" 2>&1 | tail -2
  head -c 120 /tmp/a6ov.txt; echo
done
