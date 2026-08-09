#!/usr/bin/env python3
"""Per-source verdict table for slice 05 from the run dumps (later dirs win)."""
import os, re, glob, sys

BASE = '/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit'
DIRS = ['a4_out', 'a4_out2', 'a4_fix', 'a4b_out1', 'a4b_out2', 'a4b_out3',
        'a4b_out4', 'a4b_out5']

SCHED = re.compile(r'^sched: rc=(-?\d+) records=(-?\d+) (\d+)ms')
PERS = re.compile(r'^--- persisted: (\d+) rows, (\d+) with lat/lon, (\d+) with geometry')

res = {}
for d in DIRS:
    for f in sorted(glob.glob(os.path.join(BASE, d, '*.txt'))):
        sid = os.path.basename(f)[:-4]
        txt = open(f, encoding='utf-8', errors='replace').read()
        rc = rows = geo = gj = ms = None
        notimeout = 'never returned' in txt
        for line in txt.splitlines():
            m = SCHED.match(line)
            if m:
                rc, ms = int(m.group(1)), int(m.group(3))
            m = PERS.match(line)
            if m:
                rows, geo, gj = int(m.group(1)), int(m.group(2)), int(m.group(3))
        rt = ''
        m = re.search(r'^record_type: (.*)$', txt, re.M)
        if m:
            rt = m.group(1)
        stderr = ''
        m = re.search(r'--- stderr ---\n(.*?)\n--- persisted', txt, re.S)
        if m:
            stderr = ' | '.join(l for l in m.group(1).splitlines()
                                if not l.startswith('[sched]'))[:200]
        res[sid] = dict(rc=rc, rows=rows or 0, geo=geo or 0, gj=gj or 0, ms=ms,
                        rt=rt, err=stderr, timeout=notimeout, dir=d)

order = [l.split()[0] for l in open(os.path.join(BASE, 'a4_all.txt')) if l.strip()]
out = open(os.path.join(BASE, 'a4b_table.tsv'), 'w', encoding='utf-8')
out.write('id\trc\trows\tgeo\tgeom\tms\tsrc\trecord_type\tstderr\n')
missing = []
for sid in order:
    r = res.get(sid)
    if not r:
        missing.append(sid); continue
    out.write('\t'.join(str(x) for x in
          [sid, 'TIMEOUT' if r['timeout'] else r['rc'], r['rows'], r['geo'],
           r['gj'], r['ms'], r['dir'], r['rt'], r['err']]) + '\n')
out.close()
if missing:
    print('MISSING: ' + ', '.join(missing))
print('wrote a4b_table.tsv')
