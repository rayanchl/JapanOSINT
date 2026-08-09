#!/usr/bin/env python3
"""Independently verify the /api/intel/items sort cost on the live DB.

Read-only. The handler orders by COALESCE(published_at,fetched_at) DESC, an
expression no index covers, so SQLite sorts the whole table for 50 rows — on the
single mongoose event-loop thread.
"""
import os, sqlite3, sys, time

db = os.environ.get("JO_DB") or sys.argv[1] if len(sys.argv) > 1 else None
if not db:
    for c in ("native/.run/japanosint.db", "native/data/japanosint.db",
              ".run/japanosint.db", "data/japanosint.db"):
        if os.path.exists(c):
            db = c
            break
if not db or not os.path.exists(db):
    sys.exit("live DB not found; pass the path as argv[1]")

print("db: %s  (%.2f GB)" % (db, os.path.getsize(db) / 1e9))
con = sqlite3.connect("file:%s?mode=ro" % db, uri=True, timeout=30)
con.execute("PRAGMA query_only=ON")
cur = con.cursor()

n = cur.execute("SELECT COUNT(*) FROM intel_items").fetchone()[0]
print("intel_items rows: %d\n" % n)

for label, sql in (
    ("bare indexed column", "SELECT uid FROM intel_items ORDER BY fetched_at DESC LIMIT 50"),
    ("the handler's ORDER BY",
     "SELECT uid FROM intel_items ORDER BY COALESCE(published_at,fetched_at) DESC, uid ASC LIMIT 50"),
):
    plan = cur.execute("EXPLAIN QUERY PLAN " + sql).fetchall()
    t0 = time.time()
    cur.execute(sql).fetchall()
    dt = time.time() - t0
    print("%-24s %8.3f s" % (label, dt))
    for r in plan:
        print("      %s" % r[-1])
    print()

has = [r[0] for r in cur.execute(
    "SELECT name FROM sqlite_master WHERE type='index' AND tbl_name='intel_items'"
    " AND sql LIKE '%COALESCE%'")]
print("indices covering the COALESCE expression: %s" % (has or "NONE"))
stat = cur.execute("SELECT COUNT(*) FROM sqlite_master WHERE name='sqlite_stat1'").fetchone()[0]
print("sqlite_stat1 present (ANALYZE has run): %s" % ("yes" if stat else "NO"))
con.close()
