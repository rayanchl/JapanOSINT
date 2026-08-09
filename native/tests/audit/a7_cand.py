#!/usr/bin/env python3
"""Probe candidate replacement feed URLs. stdin/argv: file with 'id<TAB>url' lines.
Prints status, size, and whether the body looks like a real RSS/Atom feed."""
import subprocess, sys, concurrent.futures, re

UA = "japanosint-collector"


def probe(t):
    sid, url = t
    try:
        p = subprocess.run(["curl", "-sSL", "-m", "25", "-A", UA,
                            "-H", "Accept: application/rss+xml,*/*",
                            "-w", "\n@@@%{http_code} %{url_effective}", url],
                           capture_output=True, timeout=35)
        out = p.stdout.decode("utf-8", "replace")
        tail = out.rsplit("@@@", 1)
        meta = tail[1].strip() if len(tail) > 1 else "?"
        body = tail[0]
        items = len(re.findall(r"<item[ >]", body, re.I)) or len(re.findall(r"<entry[ >]", body, re.I))
        kind = "rss" if re.search(r"<rss|<feed|<rdf", body, re.I) else (
            "json" if body.lstrip()[:1] in "[{" else "html/other")
        return "%-24s %-38s %-10s items=%-4d %s" % (sid, meta[:38], kind, items, url)
    except Exception as e:
        return "%-24s ERR %s  %s" % (sid, e, url)


rows = []
for line in open(sys.argv[1], encoding="utf-8"):
    line = line.strip()
    if not line or line.startswith("#"):
        continue
    a, b = line.split("\t", 1)
    rows.append((a, b))
with concurrent.futures.ThreadPoolExecutor(max_workers=12) as ex:
    for r in ex.map(probe, rows):
        print(r)
