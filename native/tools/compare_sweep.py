#!/usr/bin/env python3
"""Before/after of the scheduled registry: 2026-08-24 health sweep vs today's.
Prints a markdown fragment; only sources present in BOTH are compared, and
sources new since the baseline are counted separately, so the delta is a
measurement and not a change of population."""
import csv, sys, collections
base_p, now_p = sys.argv[1], sys.argv[2]
base = {r['id']: r for r in csv.DictReader(open(base_p, encoding='utf-8'), delimiter='\t')}
now = {r['id']: r for r in csv.DictReader(open(now_p, encoding='utf-8'), delimiter='\t')}

def bucket_base(r):
    b = r['bucket']
    if b == 'healthy': return 'OK'
    if b == 'KEY_COLLISION': return 'COLLISION'
    if b in ('fetch-failure', 'emits-nothing-upstream-has-records', 'empty-unproven', 'timeout'): return 'EMITS_NOTHING'
    return b

both = [i for i in now if i in base]
new = [i for i in now if i not in base]
nv = collections.Counter(now[i]['verdict'] for i in now)
print(f"sources swept today: {len(now)} ({len(both)} also in the 08-24 baseline, {len(new)} registered since)")
print("today's verdicts:", dict(nv))
print()
trans = collections.Counter((bucket_base(base[i]), now[i]['verdict']) for i in both)
print("| 08-24 bucket → today | sources |")
print("| --- | --- |")
for (a, b), n in sorted(trans.items(), key=lambda kv: -kv[1]):
    print(f"| {a} → {b} | {n} |")
print()
fixed = [i for i in both if bucket_base(base[i]) != 'OK' and now[i]['verdict'] == 'OK']
broke = [i for i in both if bucket_base(base[i]) == 'OK' and now[i]['verdict'] != 'OK']
def emitted(r):
    try: return int(r.get('emitted') or 0)
    except ValueError: return 0
def stored(r):
    try: return int(r.get('stored') or 0)
    except ValueError: return 0
print(f"repaired (not OK → OK): {len(fixed)} sources, {sum(emitted(now[i]) for i in fixed):,} records/pass")
print(f"regressed (OK → not OK): {len(broke)} sources")
print(f"stored today over the common set: {sum(stored(now[i]) for i in both):,}  vs baseline {sum(stored(base[i]) for i in both):,}")
print(f"new sources (since 08-24): {len(new)} — OK {sum(1 for i in new if now[i]['verdict']=='OK')}, records {sum(emitted(now[i]) for i in new):,}")
print()
print("regressions, worst first (baseline emitted):")
for i in sorted(broke, key=lambda i: -emitted(base[i]))[:25]:
    print(f"  {i:45s} {now[i]['verdict']:14s} was {emitted(base[i]):>7} → {emitted(now[i]):>7}  {now[i]['note'][:70]}")
