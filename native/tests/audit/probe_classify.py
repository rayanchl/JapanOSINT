#!/usr/bin/env python3
"""Separate PURE PROBE STUBS from BROKEN SCRAPERS among the removal candidates.

Both emit a single reachability row today, but they are not the same thing:
a stub has no extraction code at all and never could return data, while a broken
scraper has a real parser that has stopped matching. Deleting the second kind
destroys working-in-principle code and hides a fixable bug, so only the first
kind is safe to remove.

Heuristic, then eyeballed: a stub is short and its only network/parse surface is
the probe helper — no cJSON, no manual tag/field extraction, no second fetch.
"""
import os, re, json

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(os.path.dirname(HERE)), "collectors", "sources")
plan = json.load(open(os.path.join(HERE, "probe_removal_plan.json")))

# Evidence of real extraction beyond "is the portal up?"
PARSE_HINTS = ("cJSON_Parse", "strstr(", "strcasestr", "sscanf", "strtok",
               "csv_", "geojson_", "rss_collect", "regcomp", "strtol",
               "html", "decode(", "strip_tags")

stubs, scrapers = [], []
for sid, fn in plan["delete_whole"]:
    p = os.path.join(SRC, fn)
    try:
        t = open(p, encoding="utf-8", errors="replace").read()
    except OSError:
        continue
    lines = t.count("\n") + 1
    hits = sorted({h for h in PARSE_HINTS if h in t})
    # probe_* calls don't count as extraction
    body = re.sub(r'probe_\w+\s*\([^;]*\);', '', t)
    real_hits = sorted({h for h in PARSE_HINTS if h in body})
    if lines <= 90 and not real_hits:
        stubs.append((sid, fn, lines))
    else:
        scrapers.append((sid, fn, lines, real_hits[:5]))

print("=== PURE STUBS — safe to remove (%d) ===" % len(stubs))
for sid, fn, n in stubs:
    print("  %-28s %-32s %3d lines" % (sid, fn, n))
print("\n=== HAS REAL EXTRACTION — do NOT delete, it's a broken scraper (%d) ==="
      % len(scrapers))
for sid, fn, n, hits in scrapers:
    print("  %-28s %-32s %3d lines  %s" % (sid, fn, n, ",".join(hits)))

json.dump({"stubs": [[s, f] for s, f, _ in stubs],
           "scrapers": [[s, f] for s, f, _, _ in scrapers]},
          open(os.path.join(HERE, "probe_classified.json"), "w"), indent=1)
