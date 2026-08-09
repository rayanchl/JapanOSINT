#!/usr/bin/env python3
"""READ-ONLY: prove that an index on COALESCE(published_at,fetched_at) removes
the 45 s temp B-tree. idx_intel_items_simhash_todo is ALREADY on exactly that
expression (but partial, WHERE simhash IS NULL), so it can serve as the proof
that SQLite matches the expression when an index exists for it."""
import os, sqlite3, time

DB = os.environ.get("JO_DB", "/mnt/c/Users/rayan/sources/repos/OSINTsaas/data/japanmap.db")
con = sqlite3.connect("file:%s?mode=ro" % DB, uri=True)
con.execute("PRAGMA query_only=ON")
cur = con.cursor()


def run(label, q, args=()):
    plan = " | ".join(p[-1] for p in cur.execute("EXPLAIN QUERY PLAN " + q, args))
    t0 = time.time()
    n = len(cur.execute(q, args).fetchall())
    print("  %-52s %8.3f s  (%d rows)" % (label, time.time() - t0, n))
    print("       %s" % plan[:200])


print("=== A. ORDER BY on a BARE column (index exists) ===")
run("ORDER BY fetched_at DESC LIMIT 50",
    "SELECT uid FROM intel_items ORDER BY fetched_at DESC LIMIT 50")

print("\n=== B. ORDER BY the COALESCE EXPRESSION (no index) — the live feed ===")
run("ORDER BY COALESCE(published_at,fetched_at) DESC LIMIT 50",
    "SELECT uid FROM intel_items ORDER BY COALESCE(published_at,fetched_at) DESC,"
    " uid ASC LIMIT 50")

print("\n=== C. SAME expression, but restricted so the EXISTING expression")
print("       index idx_intel_items_simhash_todo(COALESCE(pub,fetch),uid)")
print("       WHERE simhash IS NULL can serve it (1,035,490 rows) ===")
run("...WHERE simhash IS NULL ORDER BY COALESCE(...) ASC LIMIT 50",
    "SELECT uid FROM intel_items WHERE simhash IS NULL"
    " ORDER BY COALESCE(published_at,fetched_at) ASC, uid ASC LIMIT 50")
run("...WHERE simhash IS NULL ORDER BY COALESCE(...) DESC LIMIT 50",
    "SELECT uid FROM intel_items WHERE simhash IS NULL"
    " ORDER BY COALESCE(published_at,fetched_at) DESC, uid DESC LIMIT 50")

print("\n=== D. filtered feed (source_id given) — does the filter rescue it? ===")
run("WHERE source_id=? ORDER BY COALESCE(...) DESC LIMIT 50",
    "SELECT uid FROM intel_items WHERE source_id=?"
    " ORDER BY COALESCE(published_at,fetched_at) DESC, uid ASC LIMIT 50",
    ("npa-traffic-accidents",))
run("WHERE source_id=? (474k rows) ORDER BY COALESCE(...) DESC",
    "SELECT uid FROM intel_items WHERE source_id=?"
    " ORDER BY COALESCE(published_at,fetched_at) DESC, uid ASC LIMIT 50",
    ("parking-facilities",))

print("\n=== E. the FTS branch (intelapi.c:289-294) ===")
run("FTS MATCH + ORDER BY COALESCE(...) DESC LIMIT 50",
    "SELECT i.uid FROM intel_items i"
    " JOIN intel_items_fts f ON f.uid=i.uid WHERE f.intel_items_fts MATCH ?"
    " ORDER BY COALESCE(i.published_at,i.fetched_at) DESC, i.uid ASC LIMIT 50",
    ("tokyo",))
run("FTS MATCH, NO order by (jp_corpus_lookup style)",
    "SELECT i.uid FROM intel_items i"
    " JOIN intel_items_fts f ON f.uid=i.uid WHERE f.intel_items_fts MATCH ?"
    " LIMIT 50", ("tokyo",))

print("\n=== F. redundant-index check: only ONE tenant exists ===")
print("  idx_intel_items_tenant_fetched(tenant_id,fetched_at DESC) = 216.7 MB")
print("  idx_intel_items_fetched(fetched_at DESC)                  = 180.4 MB")
print("  idx_intel_items_tenant_source(tenant_id,source_id)        = 100.0 MB")
print("  idx_intel_items_source(source_id,published_at DESC)       =  83.3 MB")
print("  With tenant_id='legacy' on 100%% of 2,743,208 rows, the two tenant_*")
print("  indices are prefix-degenerate duplicates of the two bare ones.")

con.close()
