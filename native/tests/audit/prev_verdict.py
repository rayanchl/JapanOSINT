#!/usr/bin/env python3
"""Verdict for given source ids in a previous sweep directory."""
import json, os, sys

d = sys.argv[1]
want = set(sys.argv[2:])
for name in ("results_scheduled.jsonl", "results_ondemand.jsonl"):
    p = os.path.join(d, name)
    if not os.path.exists(p):
        continue
    for line in open(p, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except json.JSONDecodeError:
            continue
        if r["id"] in want:
            print("%-24s %-10s rows=%-5s rc=%s  db_error=%s"
                  % (r["id"], r["verdict"], r["rows"], r["rc"],
                     (r.get("db_error") or "")[:60]))
