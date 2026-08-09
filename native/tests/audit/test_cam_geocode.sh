#!/usr/bin/env bash
# End-to-end: run a centroid-producing camera collector, then the geocoding pod
# against the SAME database, and show what moved.
set -u
cd "$(dirname "$0")/../.." || exit 1
T=$(mktemp -d); DB="$T/t.db"
export JO_DB="$DB" JO_SCHEMA=core/schema.sql
SRC="${1:-cam-webcamera24}"

echo "=== 1. collect $SRC ==="
./bin/japanosint --run "$SRC" 2>&1 | grep -E "emitted|sched" | tail -2

python3 - "$DB" <<'PY'
import json, sqlite3, sys
con = sqlite3.connect(sys.argv[1]); cur = con.cursor()
n = cur.execute("SELECT COUNT(*) FROM intel_items WHERE record_type='camera'").fetchone()[0]
d = cur.execute("SELECT COUNT(*) FROM (SELECT DISTINCT lat,lon FROM intel_items"
                " WHERE record_type='camera' AND lat IS NOT NULL)").fetchone()[0]
c = cur.execute("SELECT COUNT(*) FROM intel_items WHERE record_type='camera'"
                " AND json_extract(properties,'$.geo_precision') IN ('prefecture','city')").fetchone()[0]
print("   rows=%d  distinct positions=%d  centroid-flagged=%d" % (n, d, c))
con.close()
PY

echo
echo "=== 2. run the geocoding pod ==="
JO_CAM_GEOCODE_BUDGET=12 ./bin/japanosint --run camera-geocode 2>&1 | grep -E "cam-geocode|sched" | tail -2

echo
echo "=== 3. after ==="
python3 - "$DB" <<'PY'
import json, sqlite3, sys
con = sqlite3.connect(sys.argv[1]); cur = con.cursor()
d = cur.execute("SELECT COUNT(*) FROM (SELECT DISTINCT lat,lon FROM intel_items"
                " WHERE record_type='camera' AND lat IS NOT NULL)").fetchone()[0]
ex = cur.execute("SELECT COUNT(*) FROM intel_items WHERE record_type='camera'"
                 " AND json_extract(properties,'$.geo_precision')='exact'").fetchone()[0]
at = cur.execute("SELECT COUNT(*) FROM intel_items WHERE record_type='camera'"
                 " AND json_extract(properties,'$.geo_geocode_attempted_at') IS NOT NULL").fetchone()[0]
print("   distinct positions=%d  now-exact=%d  attempted=%d" % (d, ex, at))
print("   resolved samples:")
for t, la, lo, pv in cur.execute(
        "SELECT title, lat, lon, json_extract(properties,'$.geo_provenance')"
        " FROM intel_items WHERE record_type='camera'"
        "   AND json_extract(properties,'$.geo_precision')='exact'"
        "   AND json_extract(properties,'$.geo_provenance') LIKE 'geocoded%' LIMIT 6"):
    print("     %-40s %9.4f %9.4f  %s" % ((t or "")[:40], la, lo, pv))
con.close()
PY
rm -rf "$T"
