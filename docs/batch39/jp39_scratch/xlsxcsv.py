#!/usr/bin/env python3
"""Render an xlsx sheet the way probe_hp_batch / the engine does (CSV text),
print the first N physical lines with their index. usage: xlsxcsv.py FILE [sheet_index] [N]"""
import sys, os
sys.path.insert(0, '/Users/rayan/OSINTsaas/native/tools')
import probe_hp_batch as P
raw = open(sys.argv[1], 'rb').read()
sidx = int(sys.argv[2]) if len(sys.argv) > 2 else 0
n = int(sys.argv[3]) if len(sys.argv) > 3 else 20
tab, rows = P._xlsx_rows(raw, '', sidx)
out = []
for cells in rows:
    line = []
    for c in cells:
        if any(ch in c for ch in ',"\r\n'):
            c = '"' + c.replace('"', '""') + '"'
        line.append(c)
    out.append(",".join(line))
text = "\n".join(out) + "\n"
print("sheet:", tab, "rows:", len(rows), "physical lines:", text.count("\n"))
for i, l in enumerate(text.split("\n")[:n]):
    print(i, l[:300])
