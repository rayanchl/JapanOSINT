#!/usr/bin/env python3
"""Which 'orphan' curated rows actually have data in the live DB?

A row whose id is not in the registry is not automatically safe to delete:
some ids are SYNTHETIC sink ids that carry rows without being a source_def —
`osint-search` (pipeline.c:174) and `camera-discovery` (CAMERA_SOURCE_ID, the
uid prefix for every camera row) are both in that category. Dropping their
metadata would orphan real data.
"""
import os, re, sqlite3, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(os.path.dirname(HERE))
GEN = os.path.join(NATIVE, "core", "source_registry.gen.c")
LIVE_DB = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(NATIVE), "data", "japanmap.db")

tmp = tempfile.mkdtemp(prefix="orphchk_")
db = os.path.join(tmp, "x.db")
env = dict(os.environ, JO_DB=db, JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
p = subprocess.run([os.path.join(NATIVE, "bin", "japanosint"), "--list-sources"],
                   capture_output=True, env=env, cwd=NATIVE, timeout=300)
live = set()
for line in p.stdout.decode("utf-8", "replace").splitlines():
    m = re.match(r"^(\S+)\s+collector=", line)
    if m:
        live.add(m.group(1))

ID = re.compile(r'^\{"((?:[^"\\]|\\.)*)"')
orphans = []
for line in open(GEN, encoding="utf-8", errors="replace"):
    m = ID.match(line.strip())
    if m and m.group(1) not in live:
        orphans.append(m.group(1))

if not os.path.exists(LIVE_DB):
    sys.exit("live DB not found at %s" % LIVE_DB)
con = sqlite3.connect("file:%s?mode=ro" % LIVE_DB, uri=True, timeout=30)
con.execute("PRAGMA query_only=ON")
cur = con.cursor()

has_rows, empty = [], []
for o in orphans:
    n = cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=?", (o,)).fetchone()[0]
    (has_rows if n else empty).append((o, n))
con.close()

print("orphan curated rows           : %d" % len(orphans))
print("  ...WITH rows in the live DB : %d   <-- DO NOT DELETE" % len(has_rows))
print("  ...with no rows             : %d   <-- safe to delete" % len(empty))
print("\n=== keep (synthetic//historic ids that carry data) ===")
for o, n in sorted(has_rows, key=lambda kv: -kv[1]):
    print("   %-28s %8d rows" % (o, n))
print("\n=== safe to drop (first 20) ===")
print("   " + "  ".join(o for o, _ in empty[:20]))
