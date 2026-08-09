#!/usr/bin/env python3
"""Pre-rename survey: which gnews ids already exist, and every table that
carries a source_id or uid (so a rename migration can't miss one)."""
import os, sqlite3, subprocess, tempfile

NATIVE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
NATIVE = os.path.join(NATIVE, "native") if not os.path.isdir(os.path.join(NATIVE, "core")) else NATIVE

tmp = tempfile.mkdtemp(prefix="rensurvey_")
db = os.path.join(tmp, "x.db")
env = dict(os.environ, JO_DB=db, JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
subprocess.run([os.path.join(NATIVE, "bin", "japanosint"), "--list-sources"],
               capture_output=True, env=env, cwd=NATIVE, timeout=300)

con = sqlite3.connect(db)
print("=== existing gnews ids for candidate targets ===")
for pref in ("gnews-us-", "gnews-ru-", "gnews-es", "gnews-lang-"):
    ids = [r[0] for r in con.execute(
        "SELECT id FROM sources WHERE id LIKE ?1 ORDER BY id", (pref + "%",))]
    print("  %-12s %s" % (pref, " ".join(ids) if ids else "(none)"))

print("\n=== tables carrying source_id / uid ===")
for (n,) in con.execute("SELECT name FROM sqlite_master WHERE type='table'"):
    try:
        cols = [r[1] for r in con.execute("PRAGMA table_info(%s)" % n)]
    except sqlite3.Error:
        continue
    hit = [c for c in ("source_id", "uid", "sub_source_id") if c in cols]
    if hit:
        print("  %-24s %s" % (n, hit))
con.close()
