#!/usr/bin/env bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native || exit 1
for f in aed_map hazard_map_portal odpt_transport osm_transport_buses \
         resas_industry ski_resorts unified_airports npa_traffic_accidents \
         marine_traffic mlit_n02_stations jartic_traffic; do
  printf '%-26s ' "$f"
  if grep -q 'overpass' "collectors/sources/$f.c"; then printf 'OVERPASS '; fi
  grep -oE 'overpass_collect[a-z_]*\(ctx, sink,|overpass_collect[a-z_]*\(' "collectors/sources/$f.c" | head -1 | tr -d '\n'
  printf '  '
  grep -oE '[0-9]+, *[0-9]+, *(map|mapfn|[a-z_]+), *(NULL|&)' "collectors/sources/$f.c" | head -2 | tr '\n' ' '
  echo
done
