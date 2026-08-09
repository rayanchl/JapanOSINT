#!/usr/bin/env python3
"""Per-source verdict diff between two sweeps.

Totals alone are misleading once sources have been removed: DATA can fall simply
because duplicate and stub sources that counted as DATA are gone. This compares
only sources present in BOTH runs, so a real regression can't hide behind the
removals.

  sweep_diff.py <old_dir> <new_dir>
"""
import json, os, sys, collections

def load(d):
    out = {}
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
                out[r["id"]] = r
            except json.JSONDecodeError:
                pass
    return out


old_d = sys.argv[1] if len(sys.argv) > 1 else "tests/audit/prev_2300_postfix"
new_d = sys.argv[2] if len(sys.argv) > 2 else "tests/audit"
old, new = load(old_d), load(new_d)

gone = sorted(set(old) - set(new))
added = sorted(set(new) - set(old))
both = sorted(set(old) & set(new))

print("old=%d  new=%d  common=%d  removed=%d  added=%d"
      % (len(old), len(new), len(both), len(gone), len(added)))

print("\n=== verdict mix, COMMON sources only ===")
co = collections.Counter(old[s]["verdict"] for s in both)
cn = collections.Counter(new[s]["verdict"] for s in both)
for v in sorted(set(co) | set(cn)):
    d = cn[v] - co[v]
    print("  %-10s %5d -> %5d   %+d" % (v, co[v], cn[v], d))

print("\n=== rows, COMMON sources only ===")
ro = sum(old[s]["rows"] for s in both)
rn = sum(new[s]["rows"] for s in both)
go = sum(old[s]["geo_rows"] for s in both)
gn = sum(new[s]["geo_rows"] for s in both)
print("  rows %d -> %d (%+d)   geo %d -> %d (%+d)" % (ro, rn, rn - ro, go, gn, gn - go))

trans = collections.Counter()
for s in both:
    a, b = old[s]["verdict"], new[s]["verdict"]
    if a != b:
        trans[(a, b)] += 1
print("\n=== transitions (common sources) ===")
for (a, b), n in trans.most_common(15):
    print("  %-10s -> %-10s %4d" % (a, b, n))

print("\n=== REGRESSIONS: was DATA, now not ===")
regs = [s for s in both if old[s]["verdict"] == "DATA" and new[s]["verdict"] != "DATA"]
for s in sorted(regs)[:40]:
    print("  %-34s %-9s rows %d -> %d" %
          (s, new[s]["verdict"], old[s]["rows"], new[s]["rows"]))
print("  total: %d" % len(regs))

print("\n=== RECOVERIES: was not DATA, now DATA ===")
recs = [s for s in both if old[s]["verdict"] != "DATA" and new[s]["verdict"] == "DATA"]
for s in sorted(recs)[:30]:
    print("  %-34s %-9s -> DATA  rows %d -> %d" %
          (s, old[s]["verdict"], old[s]["rows"], new[s]["rows"]))
print("  total: %d" % len(recs))
