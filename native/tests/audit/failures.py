#!/usr/bin/env python3
"""Show representative failures per verdict, with the stderr that explains them."""
import json, os, sys, collections, re

HERE = os.path.dirname(os.path.abspath(__file__))
WANT = sys.argv[1] if len(sys.argv) > 1 else None
N = int(sys.argv[2]) if len(sys.argv) > 2 else 6
NOISE = re.compile(r'^\[db\] (migrated|seeded|opened)|^\s*$')

rows = []
for f in ("results_scheduled.jsonl", "results_ondemand.jsonl"):
    p = os.path.join(HERE, f)
    if os.path.exists(p):
        for line in open(p, encoding="utf-8"):
            line = line.strip()
            if line:
                try:
                    rows.append(json.loads(line))
                except json.JSONDecodeError:
                    pass

by = collections.defaultdict(list)
for r in rows:
    by[r["verdict"]].append(r)

for v in sorted(by, key=lambda k: -len(by[k])):
    if WANT and v != WANT:
        continue
    print("\n######## %s — %d sources" % (v, len(by[v])))
    for r in by[v][:N]:
        print("\n=== %s  entity=%s rc=%s rows=%s %sms" %
              (r["id"], r["entity"], r["rc"], r["rows"], r["duration_ms"]))
        tail = [l for l in (r.get("stderr_tail") or "").splitlines() if not NOISE.match(l)]
        for l in tail[-8:]:
            print("   | " + l[:200])
