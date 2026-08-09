#!/usr/bin/env python3
"""READ-ONLY follow-up profile: index redundancy, pending-index selectivity,
FTS duplication, per-source storage. Writes nothing."""
import os, sqlite3, time

DB = os.environ.get("JO_DB", "/mnt/c/Users/rayan/sources/repos/OSINTsaas/data/japanmap.db")
con = sqlite3.connect("file:%s?mode=ro" % DB, uri=True)
con.execute("PRAGMA query_only=ON")
cur = con.cursor()

print("=== tenant cardinality (does tenant_id earn its two indices?) ===")
for r in cur.execute("SELECT tenant_id, COUNT(*) FROM intel_items GROUP BY tenant_id"):
    print("  tenant %-20s %10d" % r)

print("\n=== selectivity of the 'pending work' partial indices ===")
for label, pred in [
    ("translate_pending  (translated_at IS NULL AND translate_failed<3)",
     "translated_at IS NULL AND COALESCE(translate_failed,0) < 3"),
    ("media_pending      (media_scanned_at IS NULL)", "media_scanned_at IS NULL"),
    ("simhash_todo       (simhash IS NULL)", "simhash IS NULL"),
    ("geom_pending       (lat IS NULL AND geom_source IS NULL)",
     "lat IS NULL AND geom_source IS NULL"),
    ("capture_stills=1", "capture_stills=1"),
]:
    try:
        t0 = time.time()
        n = cur.execute("SELECT COUNT(*) FROM intel_items WHERE " + pred).fetchone()[0]
        print("  %-58s %9d rows  (%.1f%% of table)  [%.2fs]"
              % (label, n, 100.0 * n / 2743208, time.time() - t0))
    except Exception as e:
        print("  %-58s ! %s" % (label, e))

print("\n=== index write amplification: how many B-trees per intel_items write ===")
idx = cur.execute("SELECT name FROM sqlite_master WHERE type='index'"
                  " AND tbl_name='intel_items'").fetchall()
print("  %d indices on intel_items (incl. the uid autoindex)" % len(idx))
try:
    tot = cur.execute(
        "SELECT SUM(pgsize) FROM dbstat WHERE name IN"
        " (SELECT name FROM sqlite_master WHERE type='index' AND tbl_name='intel_items')"
    ).fetchone()[0]
    tab = cur.execute("SELECT SUM(pgsize) FROM dbstat WHERE name='intel_items'").fetchone()[0]
    print("  index bytes %.0f MB   vs table bytes %.0f MB   (ratio %.2f)"
          % (tot / 1e6, tab / 1e6, tot / float(tab)))
except Exception as e:
    print("  !", e)

print("\n=== FTS duplication: intel_items_fts stores a SECOND copy of the text ===")
try:
    for n in ("intel_items_fts_content", "intel_items_fts_data",
              "intel_items_fts_docsize", "intel_items_fts_idx",
              "intel_items_fts_uid_map", "idx_intel_items_fts_uid_map_rowid"):
        b = cur.execute("SELECT SUM(pgsize) FROM dbstat WHERE name=?", (n,)).fetchone()[0]
        print("  %-38s %8.1f MB" % (n, (b or 0) / 1e6))
except Exception as e:
    print("  !", e)
print("  (schema.sql:343 already uses content='breach_items' for breach_fts —")
print("   the external-content pattern is known here, just not applied to the big one)")

print("\n=== storage by source: properties/geometry are the bulk ===")
try:
    t0 = time.time()
    rows = cur.execute(
        "SELECT source_id, COUNT(*) c,"
        " SUM(LENGTH(COALESCE(properties,''))) p,"
        " SUM(LENGTH(COALESCE(geometry,''))) g"
        " FROM intel_items GROUP BY source_id ORDER BY (p+g) DESC LIMIT 15").fetchall()
    print("  (%.1fs)  %-34s %8s %10s %10s %9s"
          % (time.time() - t0, "source", "rows", "props MB", "geom MB", "B/row"))
    for sid, c, p, g in rows:
        print("        %-34s %8d %10.1f %10.1f %9d"
              % (sid, c, (p or 0) / 1e6, (g or 0) / 1e6, ((p or 0) + (g or 0)) // max(c, 1)))
except Exception as e:
    print("  !", e)

print("\n=== simhash: what did 2 MeCab passes per emitted row actually buy? ===")
try:
    n_fp = cur.execute("SELECT COUNT(*) FROM intel_items WHERE simhash IS NOT NULL").fetchone()[0]
    n_z = cur.execute("SELECT COUNT(*) FROM intel_items WHERE simhash=0").fetchone()[0]
    n_cl = cur.execute("SELECT COUNT(*) FROM intel_items WHERE cluster_id IS NOT NULL").fetchone()[0]
    n_multi = cur.execute(
        "SELECT COUNT(*) FROM (SELECT cluster_id FROM intel_items"
        " WHERE cluster_id IS NOT NULL GROUP BY cluster_id HAVING COUNT(*)>1)").fetchone()[0]
    print("  fingerprinted        %9d" % n_fp)
    print("  fingerprint == 0     %9d   (unclusterable: below the token floor)" % n_z)
    print("  cluster_id set       %9d" % n_cl)
    print("  clusters with >1 row %9d   <-- the entire yield" % n_multi)
    print("  simhash_bands rows   %9d"
          % cur.execute("SELECT COUNT(*) FROM simhash_bands").fetchone()[0])
except Exception as e:
    print("  !", e)

print("\n=== fetch_log / anomaly growth (no retention anywhere) ===")
for t, col in (("fetch_log", "timestamp"), ("collector_anomaly", "created_at"),
               ("content_snapshots", "captured_at")):
    try:
        r = cur.execute("SELECT COUNT(*), MIN(%s), MAX(%s) FROM %s" % (col, col, t)).fetchone()
        print("  %-20s %8d rows   %s .. %s" % (t, r[0], r[1], r[2]))
    except Exception as e:
        print("  %-20s ! %s" % (t, e))

print("\n=== TIMED: the actual hot endpoint queries (real 2.74M-row DB) ===")
HOT = [
    ("intelapi.c:302 feed, no filter",
     "SELECT uid,source_id,title,lat,lon FROM intel_items"
     " ORDER BY COALESCE(published_at,fetched_at) DESC, uid ASC LIMIT 50", ()),
    ("intelapi.c:302 feed, page 2 (keyset)",
     "SELECT uid FROM intel_items"
     " WHERE (COALESCE(published_at,fetched_at) < ?)"
     " ORDER BY COALESCE(published_at,fetched_at) DESC, uid ASC LIMIT 50",
     ("2026-07-01",)),
    ("intelapi.c:542 /api/intel/sources GROUP BY",
     "SELECT source_id,COUNT(*),SUM(CASE WHEN lat IS NOT NULL THEN 1 ELSE 0 END),"
     " MAX(fetched_at),MAX(COALESCE(published_at,fetched_at))"
     " FROM intel_items GROUP BY source_id", ()),
    ("httpd.c:217 COUNT(*) WHERE source_id (x2 per run)",
     "SELECT COUNT(*) FROM intel_items WHERE source_id=?",
     ("parking-facilities",)),
    ("camera feed json_extract ORDER BY",
     "SELECT uid,COALESCE(json_extract(properties,'$.last_seen_at'),fetched_at) ts"
     " FROM intel_items WHERE record_type IN ('camera','camera-discovery')"
     " AND lat IS NOT NULL ORDER BY ts DESC, uid DESC LIMIT 200", ()),
    ("station_clusterer leading-wildcard LIKE",
     "SELECT uid FROM intel_items WHERE source_id IN"
     " ('unified-trains','unified-subways') AND uid LIKE ?", ("%|node/123456",)),
]
for label, q, args in HOT:
    try:
        t0 = time.time()
        n = len(cur.execute(q, args).fetchall())
        dt = time.time() - t0
        plan = " | ".join(p[-1] for p in
                          cur.execute("EXPLAIN QUERY PLAN " + q, args).fetchall())
        print("  %-42s %8.3f s (%4d rows)" % (label, dt, n))
        print("        plan: %s" % plan[:190])
    except Exception as e:
        print("  %-42s ! %s" % (label, e))

print("\n=== is idx_simhash_bands_lookup redundant with the WITHOUT ROWID PK? ===")
for r in cur.execute("SELECT name,sql FROM sqlite_master WHERE tbl_name='simhash_bands'"):
    print("  %-34s %s" % (r[0], (r[1] or "(auto)")[:150]))

con.close()
