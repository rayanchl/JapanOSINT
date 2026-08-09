#!/usr/bin/env python3
"""Remove curated rows describing collectors that no longer exist.

source_registry.gen.c is a frozen snapshot of the retired Node service's
registry, hand-maintained since. Rows whose id is not in the live registry
describe nothing — but /api/layers still counts them, which is how a deleted
collector keeps a phantom layer (and how `bosai-volcano-cam` survived its own
removal).

  --apply   rewrite the file; otherwise dry-run
"""
import os, re, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(os.path.dirname(HERE))
GEN = os.path.join(NATIVE, "core", "source_registry.gen.c")

tmp = tempfile.mkdtemp(prefix="prunecur_")
db = os.path.join(tmp, "x.db")
env = dict(os.environ, JO_DB=db, JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
p = subprocess.run([os.path.join(NATIVE, "bin", "japanosint"), "--list-sources"],
                   capture_output=True, env=env, cwd=NATIVE, timeout=300)
live = set()
for line in p.stdout.decode("utf-8", "replace").splitlines():
    m = re.match(r"^(\S+)\s+collector=", line)
    if m:
        live.add(m.group(1))
if len(live) < 100:
    sys.exit("registry looks empty (%d) — refusing to prune" % len(live))

# A row is only safe to drop if the id is BOTH unregistered AND carries no rows
# in the live DB. 67 of the 92 orphans still have data — mostly the last output
# of collectors retired tonight, but `osint-search` is a synthetic sink id that
# pipeline.c:174 still writes to. Deleting metadata for a row-bearing id makes
# those rows unattributable in the UI, which is worse than a stale row.
import sqlite3
LIVE_DB = os.path.join(os.path.dirname(NATIVE), "data", "japanmap.db")
has_rows = set()
if os.path.exists(LIVE_DB):
    con = sqlite3.connect("file:%s?mode=ro" % LIVE_DB, uri=True, timeout=30)
    con.execute("PRAGMA query_only=ON")
    for (sid,) in con.execute("SELECT DISTINCT source_id FROM intel_items"):
        has_rows.add(sid)
    con.close()
else:
    print("WARNING: live DB not found — refusing to prune without it")
    sys.exit(1)

ID = re.compile(r'^\{"((?:[^"\\]|\\.)*)"')
kept, dropped, retained = [], [], []
for line in open(GEN, encoding="utf-8", errors="replace"):
    m = ID.match(line.strip())
    if m and m.group(1) not in live:
        if m.group(1) in has_rows:
            retained.append(m.group(1))       # keep: still has data
        else:
            dropped.append(m.group(1))
            continue
    kept.append(line)

print("orphans retained (still have rows): %d" % len(retained))

print("live registry : %d" % len(live))
print("rows dropped  : %d" % len(dropped))
for i in range(0, len(dropped), 4):
    print("   " + "  ".join("%-26s" % d for d in dropped[i:i + 4]))

if "--apply" in sys.argv:
    open(GEN, "w", encoding="utf-8").write("".join(kept))
    print("\nrewrote %s" % GEN)
else:
    print("\n(dry run — pass --apply to rewrite)")
