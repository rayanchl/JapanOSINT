#!/usr/bin/env python3
"""Count rows and distinct id_keys over a fetched CSV/xlsx-as-CSV/JSON file,
the way the jp39 rows were checked (2026-10-06).
usage: measure_ids.py FILE --skip N [--enc cp932] [--noheader] KEY[+KEY...]
       measure_ids.py FILE.json --path a.b KEY[+KEY...]"""
import argparse, csv, io, json, collections
ap = argparse.ArgumentParser()
ap.add_argument("file"); ap.add_argument("keys")
ap.add_argument("--skip", type=int, default=0); ap.add_argument("--enc", default="utf-8-sig")
ap.add_argument("--noheader", action="store_true"); ap.add_argument("--path")
a = ap.parse_args()
parts = a.keys.split("+")
if a.file.endswith(".json"):
    node = json.load(open(a.file, encoding="utf-8"))
    for seg in (a.path or "").split("."):
        if seg: node = node[int(seg)] if isinstance(node, list) else node[seg]
    recs = node
else:
    text = open(a.file, "rb").read().decode(a.enc, "replace")
    text = "\n".join(text.split("\n")[a.skip:])
    rd = list(csv.reader(io.StringIO(text)))
    if a.noheader:
        recs = [{"col%d" % i: v for i, v in enumerate(r)} for r in rd if any(c.strip() for c in r)]
    else:
        h = rd[0]; recs = [dict(zip(h, r)) for r in rd[1:] if any(c.strip() for c in r)]
c = collections.Counter("|".join(str(r.get(k, "")) for k in parts) for r in recs)
print("rows", len(recs), "distinct", len(c), "top", c.most_common(2))
