#!/usr/bin/env python3
"""Prove disable+backfill on a throwaway DB (no sqlite3 CLI on this host)."""
import os, sqlite3, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(os.path.dirname(HERE))

tmp = tempfile.mkdtemp(prefix="gnretire_")
db = os.path.join(tmp, "t.db")
env = dict(os.environ, JO_DB=db, JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
subprocess.run([os.path.join(NATIVE, "bin", "japanosint"), "--list-sources"],
               capture_output=True, env=env, cwd=NATIVE, timeout=300)

con = sqlite3.connect(db)
# fetched_at is NOT NULL — the earlier INSERT OR IGNORE swallowed that and the
# backfill then had nothing to annotate, which would have read as "step 2 works"
# when step 2 was never exercised.
con.execute("INSERT INTO intel_items (uid,source_id,fetched_at,title,properties)"
            " VALUES ('gnews-dk-world|x','gnews-dk-world',datetime('now'),"
            "'probe row','{\"guid\":\"x\"}')")
con.commit(); con.close()

subprocess.run([sys.executable, os.path.join(HERE, "gnews_retire.py"), "--db", db],
               cwd=NATIVE)

con = sqlite3.connect(db)
q = con.execute("SELECT COUNT(*) FROM sources WHERE quarantined_until>datetime('now')").fetchone()[0]
print("\nquarantined & active-blocking: %d" % q)
for r in con.execute("SELECT id,quarantined_until,substr(quarantine_reason,1,70)"
                     " FROM sources WHERE quarantined_until IS NOT NULL LIMIT 2"):
    print("  %-24s %s\n    %s" % r)
print("\nhistory annotation:")
for r in con.execute("SELECT source_id,properties FROM intel_items"
                     " WHERE source_id='gnews-dk-world'"):
    print("  %s  %s" % r)
# the keeper must NOT be quarantined
k = con.execute("SELECT COUNT(*) FROM sources WHERE id='gnews-no-world'"
                " AND quarantined_until IS NOT NULL").fetchone()[0]
print("\nkeeper gnews-no-world quarantined? %s (must be 0)" % k)
con.close()
