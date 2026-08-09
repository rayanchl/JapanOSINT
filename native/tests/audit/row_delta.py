#!/usr/bin/env python3
"""Biggest per-source row/geo movements between two sweeps (common sources)."""
import json, os, sys

def load(d):
    out = {}
    for name in ("results_scheduled.jsonl", "results_ondemand.jsonl"):
        p = os.path.join(d, name)
        if not os.path.exists(p):
            continue
        for line in open(p, encoding="utf-8"):
            line = line.strip()
            if line:
                try:
                    r = json.loads(line)
                    out[r["id"]] = r
                except json.JSONDecodeError:
                    pass
    return out

old = load(sys.argv[1]); new = load(sys.argv[2])
both = set(old) & set(new)
d = sorted(both, key=lambda s: new[s]["rows"] - old[s]["rows"])
print("=== biggest row LOSSES ===")
for s in d[:14]:
    print("  %-30s rows %7d -> %7d (%+d)  geo %6d -> %6d  %s -> %s"
          % (s, old[s]["rows"], new[s]["rows"], new[s]["rows"] - old[s]["rows"],
             old[s]["geo_rows"], new[s]["geo_rows"],
             old[s]["verdict"], new[s]["verdict"]))
print("\n=== biggest row GAINS ===")
for s in d[-10:]:
    print("  %-30s rows %7d -> %7d (%+d)  geo %6d -> %6d  %s -> %s"
          % (s, old[s]["rows"], new[s]["rows"], new[s]["rows"] - old[s]["rows"],
             old[s]["geo_rows"], new[s]["geo_rows"],
             old[s]["verdict"], new[s]["verdict"]))
