#!/usr/bin/env python3
"""Second-pass duplicate detection over the REMAINING Google News sources.

The first pass grouped by exact guid-set equality, which is timing-sensitive:
two fetches of a live feed seconds apart can differ by one item and split a
group. That under-counts — safe, but it left 6 duplicates sitting as "keepers"
(found later at jaccard 0.97-1.00). This pass uses Jaccard >= 0.8 and
single-linkage clustering so near-identical feeds group properly.

Keeper preference, in order:
  1. an id whose country code matches the edition Google actually served
  2. an id that is NOT country-coded (e.g. gnews-es419-*), since a
     language-edition id is honest about serving no single country
  3. alphabetically first, for determinism
"""
import concurrent.futures as cf, json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(os.path.dirname(HERE)), "collectors", "sources")
FILES = ["gnews_world_topics_1.c", "gnews_world_topics_2.c"]
RSSX = re.compile(
    r'RSSX\(\s*\w+\s*,\s*"([^"]+)"\s*,[^,]*,[^,]*,[^,]*,[^,]*,\s*"([^"]+)"', re.S)
GUID = re.compile(r"<guid[^>]*>(.*?)</guid>", re.S)
CEID = re.compile(r"ceid=([A-Za-z]{2}):([A-Za-z\-]+)")
UA = ("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/126 Safari/537.36")
THRESH = 0.8


def entries():
    out = []
    for fn in FILES:
        p = os.path.join(SRC, fn)
        if os.path.exists(p):
            t = open(p, encoding="utf-8", errors="replace").read()
            out += [(s, u) for s, u in RSSX.findall(t) if s.startswith("gnews-")]
    return out


def fetch(item):
    sid, url = item
    try:
        r = subprocess.run(["curl", "-s", "-A", UA, "-L", "--max-time", "25", url],
                           capture_output=True, timeout=40)
        b = r.stdout.decode("utf-8", "replace")
    except Exception:
        return sid, None, None
    g = {x.strip() for x in GUID.findall(b)}
    m = CEID.search(b)
    return sid, (g or None), (m.group(1).lower() if m else None)


def main():
    items = entries()
    print("remaining gnews sources: %d" % len(items), file=sys.stderr)
    data = {}
    served = {}
    with cf.ThreadPoolExecutor(10) as ex:
        for i, (sid, g, sv) in enumerate(ex.map(fetch, items), 1):
            if g:
                data[sid] = g
                served[sid] = sv
            if i % 100 == 0:
                print("  fetched %d/%d" % (i, len(items)), file=sys.stderr)

    ids = sorted(data)
    # group by topic first: different topics can never be the same feed, and it
    # cuts the pairwise comparison from ~90k to a few thousand.
    bytopic = {}
    for s in ids:
        bytopic.setdefault(s.rsplit("-", 1)[1], []).append(s)

    parent = {s: s for s in ids}

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[rb] = ra

    for topic, group in bytopic.items():
        for i in range(len(group)):
            for j in range(i + 1, len(group)):
                a, b = data[group[i]], data[group[j]]
                inter = len(a & b)
                if not inter:
                    continue
                if inter / float(len(a | b)) >= THRESH:
                    union(group[i], group[j])

    clusters = {}
    for s in ids:
        clusters.setdefault(find(s), []).append(s)

    dupes = {}
    for members in clusters.values():
        if len(members) < 2:
            continue
        keeper = None
        for s in sorted(members):
            cc = s.split("-")[1] if len(s.split("-")) > 2 else ""
            if served.get(s) and cc == served[s]:
                keeper = s
                break
        if not keeper:
            noncountry = [s for s in sorted(members)
                          if len(s.split("-")[1]) > 2]
            keeper = noncountry[0] if noncountry else sorted(members)[0]
        dupes[keeper] = sorted(s for s in members if s != keeper)

    tot = sum(len(v) for v in dupes.values())
    print("\nsecond-pass duplicate groups: %d   further redundant sources: %d"
          % (len(dupes), tot))
    for k in sorted(dupes):
        print("  keep %-26s  drop %s" % (k, " ".join(dupes[k])))
    json.dump(dupes, open(os.path.join(HERE, "gnews_dupes2.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
