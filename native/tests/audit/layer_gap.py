#!/usr/bin/env python3
"""What needs curating: sources that PRODUCE geometry but have no map layer.

Joins three things:
  - the live registry (id, collector, interval)
  - the curated table (id -> layer, category)
  - `.layer = "..."` declared inline on a source_def
  - tonight's sweep results (does it actually emit lat/lon or geometry?)

Output drives the recuration: real geo producers with no layer are the work.
"""
import json, os, re, subprocess, tempfile, collections

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(NATIVE, "collectors", "sources")

# ---- curated table (positional initializers) -------------------------------
FIELD = re.compile(r'"((?:[^"\\]|\\.)*)"|(NULL)|(-?\d+)')
curated = {}
for line in open(os.path.join(NATIVE, "core", "source_registry.gen.c"),
                 encoding="utf-8", errors="replace"):
    s = line.strip()
    if not s.startswith('{"'):
        continue
    v = []
    for m in FIELD.finditer(s):
        v.append(m.group(1) if m.group(1) is not None else (None if m.group(2) else m.group(3)))
    if len(v) >= 9:
        curated[v[0]] = {"category": v[3], "layer": v[8]}

# ---- inline .layer on source_defs ------------------------------------------
inline_layer = {}
DEF = re.compile(r'\.id\s*=\s*"([^"]+)"')
LAY = re.compile(r'\.layer\s*=\s*"([^"]+)"')
for fn in sorted(os.listdir(SRC)):
    if not fn.endswith((".c", ".inc")):
        continue
    t = open(os.path.join(SRC, fn), encoding="utf-8", errors="replace").read()
    for blk in re.split(r"source_def\s+\w+\s*=\s*\{", t)[1:]:
        blk = blk[:2000]
        i, l = DEF.search(blk), LAY.search(blk)
        if i and l:
            inline_layer[i.group(1)] = l.group(1)

# ---- live registry ----------------------------------------------------------
tmp = tempfile.mkdtemp(prefix="layergap_")
db = os.path.join(tmp, "x.db")
env = dict(os.environ, JO_DB=db, JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
p = subprocess.run([os.path.join(NATIVE, "bin", "japanosint"), "--list-sources"],
                   capture_output=True, env=env, cwd=NATIVE, timeout=300)
live = {}
for line in p.stdout.decode("utf-8", "replace").splitlines():
    m = re.match(r"^(\S+)\s+collector=(\S+)\s+interval=(-?\d+)", line)
    if m:
        live[m.group(1)] = {"collector": m.group(2), "interval": int(m.group(3))}

# ---- sweep results: who actually emits geometry ----------------------------
geo = {}
for name in ("results_scheduled.jsonl", "results_ondemand.jsonl"):
    fp = os.path.join(HERE, name)
    if not os.path.exists(fp):
        continue
    for line in open(fp, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except json.JSONDecodeError:
            continue
        geo[r["id"]] = max(r.get("geo_rows", 0), r.get("geometry_rows", 0))

def layer_of(sid):
    c = curated.get(sid, {}).get("layer")
    return c or inline_layer.get(sid)

producers = {s: g for s, g in geo.items() if g > 0 and s in live}
no_layer = {s: g for s, g in producers.items() if not layer_of(s)}
has_layer = {s: g for s, g in producers.items() if layer_of(s)}

print("live sources                 : %d" % len(live))
print("emitted geometry in the sweep: %d" % len(producers))
print("  ...already on a layer      : %d" % len(has_layer))
print("  ...NO LAYER (to curate)    : %d   (%d pins currently unmappable)"
      % (len(no_layer), sum(no_layer.values())))
print("inline .layer declared       : %d  (invisible to /api/layers today)"
      % len(inline_layer))

print("\n=== TOP UNMAPPED GEO PRODUCERS (by pins) ===")
for s, g in sorted(no_layer.items(), key=lambda kv: -kv[1])[:45]:
    print("  %-34s %8d  collector=%-18s iv=%s"
          % (s, g, live[s]["collector"], live[s]["interval"]))

print("\n=== existing layers in use ===")
used = collections.Counter(layer_of(s) for s in has_layer)
for l, n in used.most_common():
    print("  %-28s %3d sources" % (l, n))

json.dump({"no_layer": no_layer, "curated": curated, "inline": inline_layer,
           "live": live},
          open(os.path.join(HERE, "layer_gap.json"), "w"), indent=1)
print("\n-> layer_gap.json")
