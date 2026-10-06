#!/usr/bin/env python3
"""Read an HTML file exactly as the probe's table mode does and report header
names, row count, sample rows and id_keys uniqueness.
usage: tbl.py FILE CHARSET TABLE_MUST [INDEX] [SKIP] [ID_KEYS] [NSAMPLE]"""
import sys, collections, re
sys.path.insert(0, "/Users/rayan/OSINTsaas/native/tools")
import probe_hp_batch as P

f, cs, must = sys.argv[1], sys.argv[2], sys.argv[3]
idx = int(sys.argv[4]) if len(sys.argv) > 4 else 0
skip = int(sys.argv[5]) if len(sys.argv) > 5 else 0
idk = sys.argv[6] if len(sys.argv) > 6 else ""
ns = int(sys.argv[7]) if len(sys.argv) > 7 else 3
text = open(f, "rb").read().decode(cs, errors="replace")
grid = P.html_table_grid(text, must, idx)
if grid is None:
    print("NO TABLE"); sys.exit(1)
raw, names = [], []
for k, (h, _) in enumerate(grid[skip]):
    nb = h[:250] if h else "col%d" % k
    dup = 1 + raw.count(nb)
    raw.append(nb)
    names.append(nb if dup == 1 else "%s_%d" % (nb, dup))
header = [c[0] for c in grid[skip]]
data = [row for row in grid[skip + 1:] if any(c[0] for c in row) and [c[0] for c in row] != header]
print("names:", names)
print("rows:", len(data), "grid rows:", len(grid))
recs = []
for row in data:
    d = {names[k]: row[k][0] for k in range(len(names))}
    hrefs = [c[1] for c in row if c[1]]
    if hrefs: d["link"] = hrefs[0]
    recs.append(d)
for d in recs[:ns]: print(d)
if ns: print("...last:", recs[-1] if recs else None)
if idk:
    def key(d):
        for alt in idk.split(","):
            parts = [d.get(p, "") for p in alt.split("+")]
            if any(parts): return "+".join(parts)
        return ""
    c = collections.Counter(key(d) for d in recs)
    dup = [(k, v) for k, v in c.items() if v > 1]
    print("id_keys %s: %d records, %d distinct, %d duplicated keys" % (idk, len(recs), len(c), len(dup)))
    for k, v in dup[:5]: print("  dup:", repr(k), v)
