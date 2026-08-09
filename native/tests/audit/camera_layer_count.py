#!/usr/bin/env python3
"""Why does the Layers tab say 8 cameras and the Sources tab say 18?

/api/layers is built by enumerating the CURATED table (source_registry.gen.c)
via src_meta_at(); /api/sources and /api/status resolve through src_meta_get(),
which falls through to the LIVE registry. Two different populations, two counts.
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
    v = []
    for m in FIELD.finditer(s):
        v.append(m.group(1) if m.group(1) is not None else (None if m.group(2) else m.group(3)))
    if len(v) >= 9:
        curated[v[0]] = {"category": v[3], "layer": v[8]}

tmp = tempfile.mkdtemp(prefix="camcount_")
db = os.path.join(tmp, "x.db")
env = dict(os.environ, JO_DB=db, JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
p = subprocess.run([os.path.join(NATIVE, "bin", "japanosint"), "--list-sources"],
                   capture_output=True, env=env, cwd=NATIVE, timeout=300)
live = {}
for line in p.stdout.decode("utf-8", "replace").splitlines():
    m = re.match(r"^(\S+)\s+collector=(\S+)\s+interval=(-?\d+)", line)
    if m:
        live[m.group(1)] = {"collector": m.group(2), "interval": int(m.group(3))}

cam_live = sorted(i for i, d in live.items() if d["collector"] == "camera-discovery")
cam_curated = sorted(i for i, d in curated.items() if d["layer"] == "cameras")

print("LIVE registry, collector=camera-discovery : %d" % len(cam_live))
for i in cam_live:
    mark = "" if i in curated and curated[i]["layer"] == "cameras" else "   <-- NOT in the cameras layer"
    print("   %-28s%s" % (i, mark))

print("\nCURATED rows with layer='cameras'        : %d" % len(cam_curated))
for i in cam_curated:
    mark = "" if i in live else "   <-- ORPHAN: no such collector"
    print("   %-28s%s" % (i, mark))

missing = [i for i in cam_live if not (i in curated and curated[i]["layer"] == "cameras")]
orphan = [i for i in cam_curated if i not in live]
print("\n--- the discrepancy ---")
print("registered but absent from the layer : %d  %s" % (len(missing), " ".join(missing)))
print("in the layer but not registered      : %d  %s" % (len(orphan), " ".join(orphan)))
