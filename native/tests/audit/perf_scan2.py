#!/usr/bin/env python3
"""Where does the 4.18 h serialized sweep actually go?"""
import json, os
HERE = os.path.dirname(os.path.abspath(__file__))
rs = []
for fn in ('results_scheduled.jsonl', 'results_ondemand.jsonl'):
    p = os.path.join(HERE, fn)
    if os.path.exists(p):
        for l in open(p, encoding='utf-8'):
            l = l.strip()
            if l:
                try:
                    rs.append(json.loads(l))
                except Exception:
                    pass
for r in rs:
    for k in ('records', 'duration_ms'):
        if not isinstance(r.get(k), (int, float)):
            r[k] = 0

tot = sum(r['duration_ms'] for r in rs)
print('runs=%d  total=%.0f s (%.2f h)\n' % (len(rs), tot / 1000, tot / 3.6e6))

buckets = [
    ('>=190s, 0 rows, rc!=0  (Overpass wall / dead)',
     lambda r: r['duration_ms'] >= 190000 and r['records'] == 0 and r.get('rc') != 0),
    ('>=190s, produced rows',
     lambda r: r['duration_ms'] >= 190000 and r['records'] > 0),
    ('60-190s',   lambda r: 60000 <= r['duration_ms'] < 190000),
    ('10-60s',    lambda r: 10000 <= r['duration_ms'] < 60000),
    ('1-10s',     lambda r: 1000 <= r['duration_ms'] < 10000),
    ('<1s',       lambda r: r['duration_ms'] < 1000),
]
seen = set()
for label, f in buckets:
    sel = [r for r in rs if f(r) and id(r) not in seen]
    for r in sel:
        seen.add(id(r))
    s = sum(r['duration_ms'] for r in sel)
    print('  %-46s %5d runs  %8.0f s  %5.1f%% of sweep   %8d rows'
          % (label, len(sel), s / 1000, 100.0 * s / tot,
             sum(r['records'] for r in sel)))

print('\n=== zero-yield time (runs that produced NO rows) ===')
z = [r for r in rs if r['records'] == 0]
print('  %d runs, %.0f s (%.2f h) = %.1f%% of the sweep produced nothing'
      % (len(z), sum(r['duration_ms'] for r in z) / 1000,
         sum(r['duration_ms'] for r in z) / 3.6e6,
         100.0 * sum(r['duration_ms'] for r in z) / tot))

print('\n=== estimated SQLite emit() cost inside the sweep ===')
rows = sum(r['records'] for r in rs)
print('  rows emitted this sweep       : %d' % rows)
for us, tag in ((725.4, 'measured CURRENT emit() @725 us/row'),
                (27.0, 'measured cached+batched @27 us/row')):
    print('  %-38s: %8.0f s  (%.1f%% of sweep)'
          % (tag, rows * us / 1e6, 100.0 * rows * us / 1e6 / (tot / 1000)))
print('  NOTE: bench ran with 9 indices on intel_items; production has 14,')
print('        so the CURRENT figure is a LOWER bound.')

print('\n=== the 29 timed-out sources, and what they cost ===')
to = [r for r in rs if r.get('timed_out')]
print('  %d timed out, %.0f s burned, %d rows kept'
      % (len(to), sum(r['duration_ms'] for r in to) / 1000,
         sum(r['records'] for r in to)))
