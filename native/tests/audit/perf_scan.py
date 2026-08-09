#!/usr/bin/env python3
import json, os
HERE = os.path.dirname(os.path.abspath(__file__))
rs = []
for fn in ('results_scheduled.jsonl', 'results_ondemand.jsonl'):
    p = os.path.join(HERE, fn)
    if not os.path.exists(p):
        continue
    for l in open(p, encoding='utf-8'):
        l = l.strip()
        if not l:
            continue
        try:
            rs.append(json.loads(l))
        except Exception:
            pass
print('total run records parsed:', len(rs))
def g(r, k):
    v = r.get(k)
    return v if isinstance(v, (int, float)) else 0


for r in rs:
    r['records'] = g(r, 'records')
    r['duration_ms'] = g(r, 'duration_ms')
ok = [r for r in rs if r['records'] > 0 and r['duration_ms'] > 0]
ok.sort(key=lambda r: -r['records'])
print()
print('%-36s %8s %10s %10s' % ('source', 'rows', 'ms', 'rows/s'))
for r in ok[:30]:
    print('%-36s %8d %10d %10.1f' % (r['id'], r['records'], r['duration_ms'],
                                     r['records'] * 1000.0 / r['duration_ms']))
tr = sum(r['records'] for r in ok)
tm = sum(r['duration_ms'] for r in ok)
print()
print('emitting sources: %d  rows %d  ms %d  agg %.1f rows/s'
      % (len(ok), tr, tm, tr * 1000.0 / tm))
big = [r for r in ok if r['records'] >= 1000]
if big:
    br = sum(r['records'] for r in big)
    bm = sum(r['duration_ms'] for r in big)
    print('>=1000-row sources: %d  rows %d  ms %d  %.1f rows/s'
          % (len(big), br, bm, br * 1000.0 / bm))

# wall-clock totals across the whole sweep (including zero-row sources)
allms = sum(r.get('duration_ms', 0) or 0 for r in rs)
print()
print('sweep wall-clock if fully serialized: %.1f s  (%.2f h) over %d runs'
      % (allms / 1000.0, allms / 3600000.0, len(rs)))
slow = sorted(rs, key=lambda r: -(r.get('duration_ms', 0) or 0))[:20]
print()
print('%-36s %10s %8s %6s' % ('slowest source', 'ms', 'rows', 'rc'))
for r in slow:
    print('%-36s %10d %8s %6s' % (r['id'], r.get('duration_ms', 0) or 0,
                                  r.get('records'), r.get('rc')))
to = [r for r in rs if r.get('timed_out')]
cr = [r for r in rs if r.get('crashed')]
print()
print('timed_out: %d   crashed: %d' % (len(to), len(cr)))
if to:
    print('timed out ids:', ', '.join(r['id'] for r in to[:40]))
if cr:
    print('crashed ids:', ', '.join(r['id'] for r in cr[:40]))
