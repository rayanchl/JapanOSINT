import csv, sys, io, collections
f, enc, keys = sys.argv[1], sys.argv[2], sys.argv[3]
skip = int(sys.argv[4]) if len(sys.argv) > 4 else 0
txt = open(f, 'rb').read().decode(enc, errors='replace').lstrip('﻿')
lines = txt.splitlines()[skip:]
rows = list(csv.DictReader(lines))
ks = keys.split('+')
c = collections.Counter(tuple(r.get(k) for k in ks) for r in rows)
full = collections.Counter(tuple(r.values()) for r in rows)
print('rows', len(rows), 'distinct key', len(c), 'distinct full-row', len(full), 'dups', [x for x in c.most_common(3) if x[1] > 1])
print('header', list(rows[0].keys()))
print('last', list(rows[-1].values())[:14])
