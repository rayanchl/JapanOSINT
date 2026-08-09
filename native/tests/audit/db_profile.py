#!/usr/bin/env python3
"""READ-ONLY profile of the live japanmap.db. Opens with mode=ro; writes nothing."""
import os, sqlite3, sys, time

DB = os.environ.get("JO_DB", "/mnt/c/Users/rayan/sources/repos/OSINTsaas/data/japanmap.db")
con = sqlite3.connect("file:%s?mode=ro" % DB, uri=True)
con.execute("PRAGMA query_only=ON")
cur = con.cursor()

print("db bytes: %d (%.2f GB)" % (os.path.getsize(DB), os.path.getsize(DB) / 1e9))
for p in ("page_size", "page_count", "journal_mode", "cache_size",
          "mmap_size", "wal_autocheckpoint", "synchronous", "auto_vacuum"):
    try:
        print("  PRAGMA %-18s = %s" % (p, cur.execute("PRAGMA %s" % p).fetchone()[0]))
    except Exception as e:
        print("  PRAGMA %-18s ! %s" % (p, e))

print("\n=== has ANALYZE ever run? ===")
r = cur.execute("SELECT count(*) FROM sqlite_master WHERE name='sqlite_stat1'").fetchone()[0]
print("sqlite_stat1 present:", bool(r))
if r:
    print("stat1 rows:", cur.execute("SELECT count(*) FROM sqlite_stat1").fetchone()[0])

print("\n=== dbstat: largest objects by bytes ===")
try:
    rows = cur.execute(
        "SELECT name, SUM(pgsize) b, COUNT(*) pages FROM dbstat"
        " GROUP BY name ORDER BY b DESC LIMIT 25").fetchall()
    tot = sum(x[1] for x in rows)
    for n, b, p in rows:
        print("  %-42s %12d  %7.2f MB  %8d pages" % (n, b, b / 1e6, p))
except Exception as e:
    print("  dbstat unavailable:", e)

print("\n=== row counts ===")
for t in ("intel_items", "intel_items_fts_uid_map", "entity_mentions", "entities",
          "breach_items", "breach_meta", "simhash_bands", "fetch_log",
          "collector_anomaly", "evidence", "media_assets", "sources",
          "audit_events", "content_snapshots", "station_line_dots",
          "gtfs_stop_times", "collector_cache"):
    try:
        t0 = time.time()
        n = cur.execute("SELECT COUNT(*) FROM %s" % t).fetchone()[0]
        print("  %-26s %10d   (COUNT took %.2f s)" % (t, n, time.time() - t0))
    except Exception as e:
        print("  %-26s  -- %s" % (t, e))

print("\n=== indices on intel_items ===")
for r in cur.execute("SELECT name, sql FROM sqlite_master WHERE type='index'"
                     " AND tbl_name='intel_items' ORDER BY name"):
    print("  %-40s %s" % (r[0], (r[1] or "(auto)")[:130]))

print("\n=== intel_items column NULL/fill stats ===")
try:
    row = cur.execute(
        "SELECT COUNT(*), SUM(lat IS NOT NULL), SUM(geometry IS NOT NULL),"
        " SUM(simhash IS NULL), SUM(cluster_id IS NOT NULL),"
        " SUM(LENGTH(COALESCE(properties,''))), SUM(LENGTH(COALESCE(geometry,''))),"
        " SUM(LENGTH(COALESCE(body,'')))"
        " FROM intel_items").fetchone()
    print("  rows=%d  with_lat=%s  with_geometry=%s  simhash_NULL=%s  clustered=%s"
          % row[:5])
    print("  bytes: properties=%.1f MB  geometry=%.1f MB  body=%.1f MB"
          % (row[5] / 1e6, row[6] / 1e6, row[7] / 1e6))
except Exception as e:
    print("  !", e)

print("\n=== top sources by row count ===")
try:
    for sid, n in cur.execute("SELECT source_id, COUNT(*) c FROM intel_items"
                              " GROUP BY source_id ORDER BY c DESC LIMIT 20"):
        print("  %-38s %8d" % (sid, n))
except Exception as e:
    print("  !", e)

print("\n=== EXPLAIN QUERY PLAN on representative hot queries ===")
QUERIES = [
    ("uid probe (per ingested row)",
     "SELECT 1 FROM intel_items WHERE uid=?", ("x",)),
    ("map bbox scan",
     "SELECT uid,lat,lon FROM intel_items WHERE lat IS NOT NULL"
     " AND lat BETWEEN ? AND ? AND lon BETWEEN ? AND ? LIMIT 5000",
     (35.0, 36.0, 139.0, 140.0)),
    ("recent feed",
     "SELECT uid FROM intel_items ORDER BY fetched_at DESC LIMIT 50", ()),
    ("tenant recent feed",
     "SELECT uid FROM intel_items WHERE tenant_id=? ORDER BY fetched_at DESC LIMIT 50",
     ("legacy",)),
    ("per-source recent",
     "SELECT uid FROM intel_items WHERE source_id=? ORDER BY published_at DESC LIMIT 50",
     ("npa-traffic-accidents",)),
    ("COUNT(*) whole table",
     "SELECT COUNT(*) FROM intel_items", ()),
    ("geom pending backfill",
     "SELECT uid FROM intel_items WHERE lat IS NULL AND geom_source IS NULL LIMIT 50", ()),
    ("simhash backfill page",
     "SELECT uid,tenant_id FROM intel_items WHERE simhash IS NULL"
     " ORDER BY COALESCE(published_at,fetched_at) ASC, uid ASC LIMIT 500", ()),
    ("record_type filter",
     "SELECT uid FROM intel_items WHERE record_type=? LIMIT 50", ("camera",)),
    ("cluster lookup",
     "SELECT COUNT(*) FROM intel_items WHERE cluster_id=? AND tenant_id=?",
     ("x", "legacy")),
]
for label, q, args in QUERIES:
    try:
        plan = cur.execute("EXPLAIN QUERY PLAN " + q, args).fetchall()
        print("  --- %s" % label)
        for p in plan:
            print("        %s" % p[-1])
    except Exception as e:
        print("  --- %s  ! %s" % (label, e))

print("\n=== timed hot queries (cold-ish, real DB) ===")
TIMED = [
    ("COUNT(*) intel_items", "SELECT COUNT(*) FROM intel_items", ()),
    ("recent 50 by fetched_at",
     "SELECT uid,title FROM intel_items ORDER BY fetched_at DESC LIMIT 50", ()),
    ("bbox Tokyo 5000",
     "SELECT uid,lat,lon FROM intel_items WHERE lat IS NOT NULL"
     " AND lat BETWEEN 35.4 AND 35.9 AND lon BETWEEN 139.4 AND 139.9 LIMIT 5000", ()),
    ("group by source_id",
     "SELECT source_id,COUNT(*) FROM intel_items GROUP BY source_id", ()),
    ("fts match",
     "SELECT uid FROM intel_items_fts WHERE intel_items_fts MATCH 'tokyo' LIMIT 50", ()),
]
for label, q, args in TIMED:
    try:
        t0 = time.time()
        n = len(cur.execute(q, args).fetchall())
        print("  %-30s %8.3f s  (%d rows)" % (label, time.time() - t0, n))
    except Exception as e:
        print("  %-30s  ! %s" % (label, e))

con.close()
