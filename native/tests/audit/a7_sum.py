import re, sys
for path in sys.argv[1:]:
    txt = open(path, encoding="utf-8", errors="replace").read()
    for blk in txt.split("#################################################"):
        m = re.search(r"^=== (\S+)(?:  entity=(.*))?$", blk, re.M)
        if not m:
            continue
        sid = m.group(1)
        sc = re.search(r"sched: rc=(-?\d+) records=(-?\d+) (\d+)ms", blk)
        pe = re.search(r"persisted: (\d+) rows, (\d+) with lat/lon, (\d+) with geometry", blk)
        errs = re.findall(r"\[[a-z0-9_.-]+\] ([a-z ]*(?:gated|http status=\d+|no features|unavailable|no elements)[^\n]*)", blk)
        print("%-24s rc=%-4s rows=%-6s geo=%-6s gj=%-4s %s" % (
            sid,
            sc.group(1) if sc else "?",
            pe.group(1) if pe else "?",
            pe.group(2) if pe else "?",
            pe.group(3) if pe else "?",
            "; ".join(dict.fromkeys(errs))[:70]))
