#!/usr/bin/env python3
"""List sources holding a given verdict, with their db_error / stderr tail."""
import json, os, sys

want = sys.argv[1] if len(sys.argv) > 1 else "DB_ERROR"
HERE = os.path.dirname(os.path.abspath(__file__))
for name in ("results_scheduled.jsonl", "results_ondemand.jsonl"):
    p = os.path.join(HERE, name)
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
        if r.get("verdict") != want:
            continue
        print("%-28s rows=%-6s rc=%s" % (r["id"], r["rows"], r["rc"]))
        if r.get("db_error"):
            print("   db_error: %s" % r["db_error"])
        tail = (r.get("stderr_tail") or "").strip().splitlines()[-3:]
        for t in tail:
            print("   | %s" % t[:150])
