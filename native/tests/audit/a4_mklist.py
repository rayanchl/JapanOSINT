#!/usr/bin/env python3
import re, sys
rows = []
cur_file = None
for line in open('/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/slice_05.md', encoding='utf-8'):
    m = re.match(r'^## `([^`]+)`', line)
    if m:
        cur_file = m.group(1); continue
    if not line.startswith('| '): continue
    c = [x.strip() for x in line.strip().strip('|').split('|')]
    if len(c) < 9 or c[0] in ('id', '---'): continue
    if set(c[0]) <= set('-'): continue
    sid, cat, iv, pivot, sweep, nrows, geo, ms = c[:8]
    note = c[8] if len(c) > 8 else ''
    ent = ''
    if pivot and pivot != '-':
        ent = pivot.split('=', 1)[1] if '=' in pivot else ''
    rows.append((sid, ent, sweep, cur_file, nrows, geo, note))
with open('/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a4_all.txt', 'w') as f:
    for sid, ent, sweep, cf, nr, geo, note in rows:
        f.write(("%s %s" % (sid, ent)).strip() + "\n")
print(len(rows), "sources")
