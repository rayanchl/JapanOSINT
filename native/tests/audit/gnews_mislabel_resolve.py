#!/usr/bin/env python3
"""Decide what to do with each mislabelled surviving keeper.

A keeper like `gnews-am-business` serves the RUSSIAN edition. If a correctly
named `gnews-ru-business` already exists and returns the same feed, the honest
action is to RETIRE the mislabelled one as a duplicate — not to invent a new id
for it. Only where no correctly-named counterpart exists is a rename right.

Exact guid-set equality (what gnews_dupes.py used) is timing-sensitive: two
fetches of a live feed seconds apart can differ by one item and split a group.
That makes the earlier grouping UNDER-count duplicates — safe, but incomplete.
Here we compare with Jaccard so near-identical feeds are recognised, and fetch
both sides concurrently to minimise drift.
"""
import concurrent.futures as cf, json, os, re, subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(os.path.dirname(HERE)), "collectors", "sources")
FILES = ["gnews_world_topics_1.c", "gnews_world_topics_2.c"]
RSSX = re.compile(
    r'RSSX\(\s*\w+\s*,\s*"([^"]+)"\s*,[^,]*,[^,]*,[^,]*,[^,]*,\s*"([^"]+)"', re.S)
GUID = re.compile(r"<guid[^>]*>(.*?)</guid>", re.S)
UA = ("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/126 Safari/537.36")

urls = {}
for fn in FILES:
    p = os.path.join(SRC, fn)
    if os.path.exists(p):
        t = open(p, encoding="utf-8", errors="replace").read()
        for sid, url in RSSX.findall(t):
            urls[sid] = url

mis = json.load(open(os.path.join(HERE, "gnews_mislabelled.json"), encoding="utf-8"))
SERVED_TO_PREFIX = {"ru": "ru", "us": "us"}


def guids(sid):
    u = urls.get(sid)
    if not u:
        return None
    try:
        r = subprocess.run(["curl", "-s", "-A", UA, "-L", "--max-time", "25", u],
                           capture_output=True, timeout=40)
        g = {x.strip() for x in GUID.findall(r.stdout.decode("utf-8", "replace"))}
        return g or None
    except Exception:
        return None


def jac(a, b):
    if not a or not b:
        return 0.0
    return len(a & b) / float(len(a | b))


retire, rename = {}, {}
for sid, info in sorted(mis.items()):
    topic = sid.rsplit("-", 1)[1]
    served = info["served"]
    cand = None
    pref = SERVED_TO_PREFIX.get(served)
    if pref:
        c = "gnews-%s-%s" % (pref, topic)
        if c in urls and c != sid:
            cand = c
    if not cand:
        rename[sid] = info
        print("%-26s served=%-4s no correctly-named counterpart -> RENAME"
              % (sid, served))
        continue
    with cf.ThreadPoolExecutor(2) as ex:
        fa, fb = ex.submit(guids, sid), ex.submit(guids, cand)
        a, b = fa.result(), fb.result()
    j = jac(a, b)
    verdict = "RETIRE as duplicate" if j >= 0.8 else "RENAME (feeds differ)"
    (retire if j >= 0.8 else rename)[sid] = dict(info, counterpart=cand,
                                                 jaccard=round(j, 3))
    print("%-26s served=%-4s vs %-24s jaccard=%.3f -> %s"
          % (sid, served, cand, j, verdict))

json.dump({"retire": retire, "rename": rename},
          open(os.path.join(HERE, "gnews_mislabel_actions.json"), "w"), indent=1)
print("\nretire-as-duplicate: %d   rename: %d" % (len(retire), len(rename)))
