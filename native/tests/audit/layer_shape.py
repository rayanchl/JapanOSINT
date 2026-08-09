#!/usr/bin/env python3
"""Shape of the current layer taxonomy: themes vs one-source-per-layer vs phantoms."""
import collections, json, os

HERE = os.path.dirname(os.path.abspath(__file__))
d = json.load(open(os.path.join(HERE, "layer_gap.json"), encoding="utf-8"))
cur, inl, live, no_layer = d["curated"], d["inline"], d["live"], d["no_layer"]


def lay(s):
    return cur.get(s, {}).get("layer") or inl.get(s)


alllay = collections.Counter()
for s in live:
    l = lay(s)
    if l:
        alllay[l] += 1

phantom = collections.Counter()
for s, m in cur.items():
    if m.get("layer") and s not in live:
        phantom[m["layer"]] += 1
dead = [l for l in phantom if l not in alllay]

print("distinct layers with >=1 live source : %d" % len(alllay))
print("  ...with EXACTLY ONE live source    : %d" % sum(1 for n in alllay.values() if n == 1))
print("  ...with 2+                         : %d" % sum(1 for n in alllay.values() if n > 1))
print("layers with ZERO live sources        : %d  (phantoms)" % len(dead))
print("live sources carrying a layer        : %d of %d" % (sum(alllay.values()), len(live)))
print("live geo producers with NO layer     : %d" % len(no_layer))

print("\n=== multi-source layers (the genuine themes) ===")
for l, n in alllay.most_common():
    if n > 1:
        print("   %-26s %3d" % (l, n))

singles = sorted(l for l, n in alllay.items() if n == 1)
print("\n=== one-source layers: %d ===" % len(singles))
for i in range(0, len(singles), 4):
    print("   " + "  ".join("%-24s" % s for s in singles[i:i + 4]))

print("\n=== phantom layers (no live source at all): %d ===" % len(dead))
for i in range(0, len(dead), 4):
    print("   " + "  ".join("%-24s" % s for s in dead[i:i + 4]))
