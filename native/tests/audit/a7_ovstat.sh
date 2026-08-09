#!/usr/bin/env bash
for h in overpass-api.de overpass.kumi.systems overpass.openstreetmap.ru overpass.private.coffee; do
  printf '%-32s ' "$h"
  curl -sS -m 20 -o /dev/null -w "http=%{http_code} t=%{time_total}\n" "https://$h/api/status" 2>&1 | tail -1
done
echo "--- dns ---"
for h in overpass-api.de overpass.kumi.systems; do
  printf '%-32s ' "$h"; getent hosts "$h" || echo NXDOMAIN
done
