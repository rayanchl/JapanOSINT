#!/usr/bin/env python3
"""Identify PURE portal probes from the live sweep, not from the #include list.

A pure probe emits reachability and nothing else: its persisted properties carry
`reachable` and no measurement. Including lib/probe.h is not the test — several
collectors use the helper and still emit real rows (npa-missing-persons, the
power TSOs), and those must not be swept up in a removal.
"""
import json, os, collections

HERE = os.path.dirname(os.path.abspath(__file__))
rows = {}
for name in ("results_scheduled.jsonl", "results_ondemand.jsonl"):
    p = os.path.join(HERE, name)
    if not os.path.exists(p):
        continue
    for line in open(p, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
            rows[r["id"]] = r
        except json.JSONDecodeError:
            pass

# Keys that mean "this row carries a measurement", so the source is NOT a pure probe.
DATA_KEYS = {"lat", "lon", "value", "count", "records", "level", "magnitude",
             "mw", "load_mw", "price", "amount", "status_text", "title_ja",
             "published_at", "cases", "total", "rows_available"}

pure, mixed = [], []
for sid, r in sorted(rows.items()):
    keys = set(r.get("props_keys") or [])
    if "reachable" not in keys:
        continue
    payload = keys - {"reachable", "operator", "service", "portal", "url",
                      "source", "checked_at", "entity"}
    if not payload and r["rows"] <= 12:
        pure.append((sid, r))
    else:
        mixed.append((sid, r, sorted(payload)[:6]))

print("=== PURE PROBES (reachability only, no payload) : %d ===" % len(pure))
print("%-30s %6s %6s  %s" % ("id", "rows", "geo", "reachable?"))
for sid, r in pure:
    s = r.get("sample") or {}
    props = {}
    try:
        props = json.loads(s.get("properties") or "{}")
    except Exception:
        pass
    print("%-30s %6d %6d  %s" % (sid, r["rows"], r["geo_rows"],
                                 props.get("reachable")))

print("\n=== USES probe.h BUT CARRIES PAYLOAD — do NOT remove : %d ===" % len(mixed))
for sid, r, payload in mixed:
    print("%-30s %6d rows  payload=%s" % (sid, r["rows"], payload))
