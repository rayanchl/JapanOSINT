#!/usr/bin/env python3
"""Camera coverage audit: run every camera source into ONE database and measure
what actually reaches the map.

Answers the three questions directly:
  1. which sources return nothing (broken)
  2. how many distinct POSITIONS exist vs rows (stacking)
  3. how many rows carry no coordinate at all (ungeocoded)
"""
import collections, json, os, shutil, sqlite3, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(os.path.dirname(HERE))
BIN = os.path.join(NATIVE, "bin", "japanosint")
SCHEMA = os.path.join(NATIVE, "core", "schema.sql")

SOURCES = sys.argv[1:] or [
    "cam-camscape", "cam-camstreamer", "cam-geocam", "cam-insecam-scrape",
    "cam-scs_com_ua", "cam-skylinewebcams", "cam-tabi_cam",
    "cam-webcamendirect_list", "cam-webcamera24", "cam-webcamtaxi",
    "cam-windy_api", "cam-worldcam_eu", "cam-worldcams", "cam-youtube_live",
    "public-cameras", "shodan-cameras-jp",
]

tmp = tempfile.mkdtemp(prefix="camaudit_")
db = os.path.join(tmp, "cams.db")
env = dict(os.environ, JO_DB=db, JO_SCHEMA=SCHEMA)

print("running %d camera sources into one db ...\n" % len(SOURCES))
per = {}
for sid in SOURCES:
    try:
        p = subprocess.run([BIN, "--run", sid], capture_output=True,
                           env=env, cwd=NATIVE, timeout=340)
        err = p.stderr.decode("utf-8", "replace")
    except subprocess.TimeoutExpired:
        err = "(timed out)"
    tail = [l for l in err.strip().splitlines() if "emitted" in l or "gated" in l
            or "status" in l]
    per[sid] = tail[-1][:110] if tail else "(no log line)"
    print("  %-26s %s" % (sid, per[sid]))

con = sqlite3.connect(db)
cur = con.cursor()

print("\n=== per-source rows / positions ===")
print("%-26s %6s %6s %8s %8s  %s" %
      ("source", "rows", "geo", "distinct", "stacked", "worst point"))
tot_rows = tot_geo = 0
for sid in SOURCES:
    rows = cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=?",
                       (sid,)).fetchone()[0]
    geo = cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=?"
                      " AND lat IS NOT NULL AND lon IS NOT NULL",
                      (sid,)).fetchone()[0]
    dis = cur.execute("SELECT COUNT(*) FROM (SELECT DISTINCT lat,lon FROM"
                      " intel_items WHERE source_id=? AND lat IS NOT NULL)",
                      (sid,)).fetchone()[0]
    worst = cur.execute("SELECT lat,lon,COUNT(*) c FROM intel_items WHERE"
                        " source_id=? AND lat IS NOT NULL GROUP BY lat,lon"
                        " ORDER BY c DESC LIMIT 1", (sid,)).fetchone()
    tot_rows += rows; tot_geo += geo
    w = "%s,%s x%d" % (worst[0], worst[1], worst[2]) if worst else "-"
    print("%-26s %6d %6d %8d %8d  %s"
          % (sid, rows, geo, dis, (geo - dis) if geo else 0, w))

print("\n=== FLEET TOTALS ===")
allrows = cur.execute("SELECT COUNT(*) FROM intel_items WHERE record_type='camera'"
                      " OR source_id LIKE 'cam-%' OR source_id IN"
                      " ('public-cameras','shodan-cameras-jp')").fetchone()[0]
allgeo = cur.execute("SELECT COUNT(*) FROM intel_items WHERE (record_type='camera'"
                     " OR source_id LIKE 'cam-%') AND lat IS NOT NULL").fetchone()[0]
alldis = cur.execute("SELECT COUNT(*) FROM (SELECT DISTINCT lat,lon FROM intel_items"
                     " WHERE (record_type='camera' OR source_id LIKE 'cam-%')"
                     " AND lat IS NOT NULL)").fetchone()[0]
print("  camera rows        : %d" % allrows)
print("  with coordinates   : %d" % allgeo)
print("  DISTINCT positions : %d   <-- what the map can actually show" % alldis)
if allgeo:
    print("  stacked rows       : %d (%.0f%% share a point with another camera)"
          % (allgeo - alldis, 100.0 * (allgeo - alldis) / allgeo))

print("\n=== worst stacking points (fleet-wide) ===")
for lat, lon, c in cur.execute(
        "SELECT lat,lon,COUNT(*) c FROM intel_items WHERE lat IS NOT NULL"
        " AND (record_type='camera' OR source_id LIKE 'cam-%')"
        " GROUP BY lat,lon ORDER BY c DESC LIMIT 12"):
    print("  %9.4f, %9.4f  x%d" % (lat, lon, c))

print("\n=== precision flags in properties ===")
flags = collections.Counter()
for (pj,) in cur.execute("SELECT properties FROM intel_items WHERE"
                         " record_type='camera' OR source_id LIKE 'cam-%'"):
    try:
        p = json.loads(pj or "{}")
    except Exception:
        continue
    flags[(p.get("location_precision"), bool(p.get("location_approximate")))] += 1
for k, v in flags.most_common():
    print("  precision=%-12s approximate=%-5s  %d" % (k[0], k[1], v))

con.close()
shutil.rmtree(tmp, ignore_errors=True)
