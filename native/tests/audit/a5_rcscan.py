#!/usr/bin/env python3
"""Find run() functions in slice files whose return path can yield a positive
non-zero value (the scheduler treats rc!=0 as an error and quarantines)."""
import os, re, sys
BASE = "/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/collectors/sources"
files = [l.strip() for l in open(sys.argv[1]) if l.strip()]
# names bound as .run = X in a source_def
for fn in files:
    p = os.path.join(BASE, fn)
    if not os.path.exists(p):
        print("MISSING", fn); continue
    src = open(p, encoding="utf-8", errors="replace").read()
    runs = set(re.findall(r"\.run\s*=\s*(\w+)", src))
    runs |= set(re.findall(r"run\s*=\s*run_##SYM", src)) and set()
    for name in sorted(runs):
        m = re.search(r"^static\s+int\s+" + re.escape(name) + r"\s*\([^)]*\)\s*\{",
                      src, re.M)
        if not m:
            continue
        # crude brace match
        i = src.index("{", m.start()); depth = 0
        for j in range(i, len(src)):
            if src[j] == "{": depth += 1
            elif src[j] == "}":
                depth -= 1
                if depth == 0: break
        body = src[i:j]
        rets = re.findall(r"return\s+([^;]+);", body)
        bad = []
        for r in rets:
            r = r.strip()
            if r in ("0", "-1"): continue
            if re.match(r"^.*\?\s*0\s*:\s*-1$", r): continue
            if re.match(r"^\(?\s*-1\s*\)?$", r): continue
            bad.append(r)
        if bad:
            print("%-28s %-22s %s" % (fn, name, " | ".join(bad[:4])))
