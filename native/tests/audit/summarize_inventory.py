#!/usr/bin/env python3
"""Shape of the registry: how many sources per collector, and how many need an
OSINT pivot entity to do anything."""
import json, os, collections
HERE = os.path.dirname(os.path.abspath(__file__))
rows = [json.loads(l) for l in open(os.path.join(HERE, "inventory.jsonl"), encoding="utf-8")]
print("total:", len(rows))
sched = sum(1 for r in rows if (r.get("interval") or 0) > 0)
print("scheduled (interval>0):", sched, " on-demand (interval<=0):", len(rows) - sched)
print("reads ctx->entity:", sum(1 for r in rows if r["reads_entity"]))
print("\nby collector:")
for k, v in collections.Counter(r.get("collector") or "?" for r in rows).most_common():
    print("  %-18s %4d" % (k, v))
print("\nby category (top 25):")
for k, v in collections.Counter(r.get("category") or "?" for r in rows).most_common(25):
    print("  %-24s %4d" % (k, v))
print("\nby type:")
for k, v in collections.Counter(r.get("type") or "?" for r in rows).most_common():
    print("  %-14s %4d" % (k, v))
print("\nentity-pivot sources with interval>0 (both scheduled and pivotable):",
      sum(1 for r in rows if r["reads_entity"] and (r.get("interval") or 0) > 0))
