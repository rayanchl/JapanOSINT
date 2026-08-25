#!/usr/bin/env python3
"""Find the DANGEROUS `o += snprintf(buf+o, sizeof buf - o, ...)` accumulator.

snprintf returns what it WOULD have written, so on truncation `o` jumps past the
buffer and the next write lands at buf+(huge) with a size_t-underflowed limit —
a stack write at an attacker-influenced offset.

Two shapes must NOT be confused:

  SAFE   fixed-width conversion inside a loop bounded by `o + N < cap`
         (the urlenc family: "%%%02X" always writes 3 bytes, and the loop
         condition re-checks before each iteration, so o cannot overshoot)
  SAFE   every subsequent write pre-gated on `o + N < sizeof buf`
         (academic_search.c) — after an overshoot the gate fails and the bad
         offset is never used

  UNSAFE a variable-length conversion (%s, or a %f/%d with no headroom check)
         followed by another write at buf+o with no bounds check in between

Only the last shape is reported.
"""
import os, re, sys

NATIVE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
NATIVE = os.path.join(NATIVE, "native") if not os.path.isdir(os.path.join(NATIVE, "core")) else NATIVE
ACC = re.compile(r"(\w+)\s*\+=\s*(?:\(size_t\)\s*)?snprintf\s*\(\s*(\w+)\s*\+")

rows = []
for sub in ("collectors/sources", "core", "lib"):
    d = os.path.join(NATIVE, sub)
    if not os.path.isdir(d):
        continue
    for fn in sorted(os.listdir(d)):
        if not fn.endswith((".c", ".inc", ".h")):
            continue
        p = os.path.join(d, fn)
        try:
            lines = open(p, encoding="utf-8", errors="replace").read().splitlines()
        except OSError:
            continue
        for i, ln in enumerate(lines):
            m = ACC.search(ln)
            if not m:
                continue
            acc, buf = m.group(1), m.group(2)
            stmt = " ".join(lines[i:i + 3])
            variable = "%s" in stmt
            # a bound on the accumulator anywhere in the enclosing ~8 lines
            ctx = "\n".join(lines[max(0, i - 6):i + 1])
            gated = bool(re.search(r"%s\s*\+\s*\d+\s*<\s*(sizeof|cap|\w*len)" % re.escape(acc), ctx))
            rows.append((sub + "/" + fn, i + 1, acc, variable, gated, ln.strip()[:86]))

danger = [r for r in rows if r[3] and not r[4]]
safe = [r for r in rows if not (r[3] and not r[4])]

print("accumulator sites: %d" % len(rows))
print("  fixed-width or pre-gated (safe): %d" % len(safe))
print("  VARIABLE-LENGTH + UNGATED      : %d\n" % len(danger))
print("=== needs the clamp ===")
cur = None
for f, ln, acc, _, _, src in danger:
    if f != cur:
        print("\n  %s" % f); cur = f
    print("    :%-5d %s" % (ln, src))
