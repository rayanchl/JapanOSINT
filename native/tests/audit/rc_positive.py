#!/usr/bin/env python3
"""Sources whose run() returned a POSITIVE rc — i.e. the emitted row count.

`core/scheduler.c` does `status = rc == 0 ? "ok" : "error"`, so a collector that
ends `return n;` instead of `return n >= 0 ? 0 : -1;` is logged as a FAILED run on
every execution and feeds anomaly_detect() — the source gets quarantined precisely
because it worked. Static grep can't tell a helper's `return n` from a run()'s, but
the sweep can: rc is whatever run() actually returned.
"""
import json, os, collections

HERE = os.path.dirname(os.path.abspath(__file__))
rows = []
for name in ("results_scheduled.jsonl", "results_ondemand.jsonl"):
    p = os.path.join(HERE, name)
    if not os.path.exists(p):
        continue
    for line in open(p, encoding="utf-8"):
        line = line.strip()
        if line:
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError:
                pass

pos = [r for r in rows if isinstance(r.get("rc"), int) and r["rc"] > 0]
pos.sort(key=lambda r: -r["rc"])
print("sources returning positive rc: %d" % len(pos))
print("rows they emitted in total:    %d" % sum(r["rows"] for r in pos))
print("of those, rc == rows exactly:  %d\n" % sum(1 for r in pos if r["rc"] == r["rows"]))
print("%-34s %6s %6s %5s  %s" % ("id", "rc", "rows", "geo", "verdict"))
for r in pos:
    print("%-34s %6d %6d %5d  %s" % (r["id"], r["rc"], r["rows"], r["geo_rows"],
                                     r["verdict"]))
print("\nby verdict:", dict(collections.Counter(r["verdict"] for r in pos)))
