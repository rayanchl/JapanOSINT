#!/usr/bin/env python3
"""Run a source and count rows pinned at the Tokyo-station default coordinate."""
import os, sqlite3, subprocess, sys, tempfile, shutil

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(os.path.dirname(HERE))
BIN = os.path.join(NATIVE, "bin", "japanosint-a4")

sid = sys.argv[1]
tmp = tempfile.mkdtemp(prefix="a4t_")
db = os.path.join(tmp, "v.db")
env = dict(os.environ, JO_DB=db, JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
subprocess.run([BIN, "--run", sid], capture_output=True, timeout=300, env=env, cwd=NATIVE)
con = sqlite3.connect("file:%s?mode=ro" % db, uri=True)
c = con.cursor()
print("rows", c.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=?", (sid,)).fetchone()[0])
print("by record_type / at-Tokyo:")
for rt, n, tk in c.execute(
        "SELECT COALESCE(record_type,'(null)'), COUNT(*),"
        " SUM(CASE WHEN ABS(lat-35.6895)<1e-6 AND ABS(lon-139.6917)<1e-6 THEN 1 ELSE 0 END)"
        " FROM intel_items WHERE source_id=? GROUP BY 1", (sid,)):
    print("  %-12s n=%-6d at_tokyo=%s" % (rt, n, tk))
con.close()
shutil.rmtree(tmp, ignore_errors=True)
