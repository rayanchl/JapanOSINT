#!/usr/bin/env python3
"""Phase 4: did the fabricated-geo removals actually land?

Several collectors replaced an invented coordinate with an explicit JSON null
geometry. lib/geojson.c used to serialise that to the 4-byte string "null",
which passes `geometry IS NOT NULL AND geometry<>''` — so the row still counted
as pinned and the fix silently regressed. After the guard, these sources must
show geo_rows == 0 (or only their legitimately-located subset).
"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
# source -> expected pins ("none", or a note when some rows legitimately pin)
EXPECT = {
    "peeringdb-jp":          "facilities only",
    "cisa-kev-jp":           "none",
    "poc-in-github":         "none",
    "classifieds":           "area-centroid, honest",
    "mastodon-jp-instances": "none",
    "trickest-cve":          "none",
    "osm-changesets-jp":     "bbox only",
    "flight-adsb":           "real fixes only",
    "spamhaus-drop":         "none",
    "feodo-tracker-jp":      "none",
    "npa-missing-persons":   "none",
    "wolfx-eqlist":          "all (real epicentres)",
    "nws-alerts-us":         "real polygons only",
    "jma-intensity":         "all (real epicentres)",
}

rows = {}
for name in ("results_scheduled.jsonl", "results_ondemand.jsonl"):
    p = os.path.join(HERE, name)
    if not os.path.exists(p):
        continue
    for line in open(p, encoding="utf-8"):
        line = line.strip()
        if line:
            try:
                r = json.loads(line)
                rows[r["id"]] = r
            except json.JSONDecodeError:
                pass

print("%-24s %8s %6s %6s %9s  %s" %
      ("source", "verdict", "rows", "geo", "geometry", "expected"))
for sid, note in EXPECT.items():
    r = rows.get(sid)
    if not r:
        print("%-24s %8s %6s %6s %9s  %s" % (sid, "(no run)", "-", "-", "-", note))
        continue
    print("%-24s %8s %6d %6d %9d  %s" %
          (sid, r["verdict"], r["rows"], r["geo_rows"], r["geometry_rows"], note))
