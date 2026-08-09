#!/usr/bin/env python3
"""Live sweep status: how far along, and the verdict mix so far."""
import json, os, collections
HERE = os.path.dirname(os.path.abspath(__file__))
PLANS = {"results_scheduled.jsonl": "plan_scheduled.tsv",
         "results_ondemand.jsonl": "plan_ondemand.tsv"}
tot = collections.Counter()
for res, plan in PLANS.items():
    rp, pp = os.path.join(HERE, res), os.path.join(HERE, plan)
    n_plan = sum(1 for _ in open(pp, encoding="utf-8")) if os.path.exists(pp) else 0
    c = collections.Counter()
    n = 0
    if os.path.exists(rp):
        for line in open(rp, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            try:
                c[json.loads(line)["verdict"]] += 1
                n += 1
            except json.JSONDecodeError:
                pass
    tot.update(c)
    print("%-28s %5d / %-5d  %s" % (res, n, n_plan, dict(c.most_common())))
print("%-28s %5d          %s" % ("TOTAL", sum(tot.values()), dict(tot.most_common())))
