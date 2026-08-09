#!/usr/bin/env bash
echo "--- A records (IPv4) ---"
for h in overpass-api.de overpass.kumi.systems overpass.private.coffee overpass.openstreetmap.ru; do
  printf '%-30s A=' "$h"; getent ahostsv4 "$h" | head -1 || echo none
done
echo "--- curl -4 ---"
for h in overpass-api.de overpass.kumi.systems; do
  printf '%-30s ' "$h"
  curl -4 -sS -m 20 -o /dev/null -w "ipv4 http=%{http_code} t=%{time_total}\n" "https://$h/api/status" 2>&1 | tail -1
done
echo "--- host ipv6 route ---"
ip -6 route show default 2>/dev/null | head -2 || echo "no ipv6 default route"
echo "--- control: ipv4-only site ---"
curl -4 -sS -m 15 -o /dev/null -w "osm.org ipv4 http=%{http_code}\n" https://www.openstreetmap.org/ 2>&1 | tail -1
