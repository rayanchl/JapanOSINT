#!/usr/bin/env bash
# Unify the three camera provenance vocabularies into one.
#
# Before:
#   insecam      coord_source + location_uncertain          (Node's original)
#   10 scrapers  location_precision + location_approximate  (C port invention)
#   curated      geo_provenance + geo_precision             (added this session)
# After, everywhere:
#   geo_provenance  where the coordinate came from
#                   ("curated-catalog" | "insecam_detail" | "area-centroid"
#                    | "geocoded:<provider>")
#   geo_precision   "exact" | "city" | "prefecture"
#   geo_uncertain   0 | 1
#
# This is what lets one query find every camera that needs geocoding, which is
# the whole point — camera_geocode_pod selects on geo_precision.
set -u
cd "$(dirname "$0")/../../collectors/sources" || exit 1
FILES=$(grep -l 'location_precision\|location_approximate\|coord_source\|location_uncertain' \
          cam_*.c public_cameras.c 2>/dev/null)
echo "files to rewrite:"; for f in $FILES; do echo "   $f"; done; echo

for f in $FILES; do
  sed -i \
    -e 's/"location_precision"/"geo_precision"/g' \
    -e 's/"location_approximate"/"geo_uncertain"/g' \
    -e 's/"coord_source"/"geo_provenance"/g' \
    -e 's/"location_uncertain"/"geo_uncertain"/g' \
    "$f"
done

echo "=== remaining old keys (should be none) ==="
grep -n 'location_precision\|location_approximate\|"coord_source"\|location_uncertain' \
     cam_*.c public_cameras.c 2>/dev/null | head
echo "=== new keys in place ==="
grep -c 'geo_precision\|geo_uncertain\|geo_provenance' cam_*.c public_cameras.c 2>/dev/null \
  | grep -v ':0$'
