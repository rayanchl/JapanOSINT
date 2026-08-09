#!/usr/bin/env python3
"""Curated metadata rows that describe collectors which no longer exist.

core/source_registry.gen.c rows are POSITIONAL, not designated initializers:
  {id,name,name_ja,category,type,url,description,license,layer,free,interval}
so field 0 is the id and field 8 is the layer.

/api/layers is built from this table (miscapi.c), not from the live registry,
and db_seed_sources never deletes — so a removed collector leaves a layer that
answers 200 with an empty FeatureCollection forever.
"""
import os, re, subprocess, tempfile

NATIVE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
NATIVE = os.path.join(NATIVE, "native") if not os.path.isdir(os.path.join(NATIVE, "core")) else NATIVE
gen = os.path.join(NATIVE, "core", "source_registry.gen.c")

FIELD = re.compile(r'"((?:[^"\\]|\\.)*)"|(NULL)|(-?\d+)')

curated = {}
for line in open(gen, encoding="utf-8", errors="replace"):
    s = line.strip()
    if not s.startswith('{"'):
        continue
    vals = []
    for m in FIELD.finditer(s):
        vals.append(m.group(1) if m.group(1) is not None
                    else (None if m.group(2) else m.group(3)))
    if len(vals) >= 9:
        curated[vals[0]] = vals[8]          # id -> layer

tmp = tempfile.mkdtemp(prefix="phantom_")
db = os.path.join(tmp, "x.db")
env = dict(os.environ, JO_DB=db, JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
p = subprocess.run([os.path.join(NATIVE, "bin", "japanosint"), "--list-sources"],
                   capture_output=True, env=env, cwd=NATIVE, timeout=300)
live = set()
for line in p.stdout.decode("utf-8", "replace").splitlines():
    m = re.match(r"^(\S+)\s+collector=", line)
    if m:
        live.add(m.group(1))

orphan = sorted(set(curated) - live)
orphan_layers = sorted({curated[o] for o in orphan if curated[o]})
live_layers = sorted({curated[i] for i in curated if i in live and curated[i]})

print("curated rows          : %d" % len(curated))
print("live registry sources : %d" % len(live))
print("ORPHAN curated rows   : %d   (describe a collector that no longer exists)"
      % len(orphan))
print("phantom layer ids     : %d   (published, zero collectors behind them)"
      % len(set(orphan_layers) - set(live_layers)))
print("\nsample orphan ids : %s" % " ".join(orphan[:12]))
print("\nphantom layers    : %s" % " ".join(sorted(set(orphan_layers) - set(live_layers))[:30]))
