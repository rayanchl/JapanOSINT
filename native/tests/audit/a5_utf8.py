#!/usr/bin/env python3
"""Run a source and report which columns hold non-UTF-8 bytes."""
import os, subprocess, sqlite3, sys, tempfile, shutil
NATIVE = "/mnt/c/Users/rayan/sources/repos/OSINTsaas/native"
sid = sys.argv[1]
ent = sys.argv[2] if len(sys.argv) > 2 else None
tmp = tempfile.mkdtemp(prefix="a5utf_")
db = os.path.join(tmp, "v.db")
env = dict(os.environ, JO_DB=db, JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
argv = [os.path.join(NATIVE, "bin", "japanosint-a5"), "--run", sid] + ([ent] if ent else [])
subprocess.run(argv, capture_output=True, timeout=180, env=env, cwd=NATIVE)
con = sqlite3.connect("file:%s?mode=ro" % db, uri=True)
con.text_factory = bytes
cur = con.cursor()
cols = ["uid", "title", "body", "summary", "link", "author", "properties", "tags"]
bad = {}
rows = cur.execute("SELECT %s FROM intel_items WHERE source_id=?" % ",".join(cols), (sid,)).fetchall()
print("rows=%d" % len(rows))
for r in rows:
    for k, v in zip(cols, r):
        if isinstance(v, bytes):
            try:
                v.decode("utf-8")
            except UnicodeDecodeError as e:
                bad.setdefault(k, []).append((len(v), v[max(0, e.start-12):e.start+6]))
for k, lst in bad.items():
    print("BAD %-12s n=%d  e.g. len=%d tail=%r" % (k, len(lst), lst[0][0], lst[0][1]))
if not bad:
    print("all columns valid UTF-8")
con.close(); shutil.rmtree(tmp, ignore_errors=True)
