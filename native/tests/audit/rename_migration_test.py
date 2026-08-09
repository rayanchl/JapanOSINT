#!/usr/bin/env python3
"""Prove the rename migration moves a row's source_id AND its uid prefix, and
that nothing is left pointing at the old id."""
import os, sqlite3, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(os.path.dirname(HERE))
OLD, NEW = "gnews-bo-world", "gnews-es419-world"

tmp = tempfile.mkdtemp(prefix="renmig_")
db = os.path.join(tmp, "t.db")
env = dict(os.environ, JO_DB=db, JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
subprocess.run([os.path.join(NATIVE, "bin", "japanosint"), "--list-sources"],
               capture_output=True, env=env, cwd=NATIVE, timeout=300)

con = sqlite3.connect(db)
# The rename already landed in code, so `sources` no longer seeds the old id;
# insert the historical rows a live deployment would still hold.
con.execute("INSERT OR IGNORE INTO sources (id,name,type,category) "
            "VALUES (?,?,'api','news')", (OLD, "old row"))
con.execute("INSERT INTO intel_items (uid,source_id,fetched_at,title,properties)"
            " VALUES (?,?,datetime('now'),'hist','{}')",
            (OLD + "|guid123", OLD))
con.execute("INSERT INTO fetch_log (source_id,status,records_fetched,duration_ms)"
            " VALUES (?,'ok',5,100)", (OLD,))
con.commit(); con.close()

subprocess.run([sys.executable, os.path.join(HERE, "gnews_mislabel_apply.py"),
                "--db", db], cwd=NATIVE)

con = sqlite3.connect(db)
print("\nintel_items after migration:")
for r in con.execute("SELECT uid, source_id FROM intel_items"):
    print("   uid=%s  source_id=%s" % r)
print("fetch_log after migration:")
for r in con.execute("SELECT source_id,status FROM fetch_log"):
    print("   %s %s" % r)
left = con.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=? OR uid LIKE ?",
                   (OLD, OLD + "|%")).fetchone()[0]
print("\nrows still pointing at %s: %d (must be 0)" % (OLD, left))
con.close()
