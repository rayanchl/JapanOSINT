#!/usr/bin/env python3
"""Derive the Google News duplicate map EMPIRICALLY, not from the audit prose.

Google discards `gl=` when the locale has no edition and silently serves a
generic feed, so the URL does not predict which sources collide — `hl=is` and
`hl=fa` both land on en-US. The only reliable test is to fetch each feed and
compare what actually comes back.

Groups feeds by the sorted set of item guids. Within a group the KEEPER is the
source whose `gl=` matches the country the served edition actually belongs to
(read from the channel's own <link>…ceid=XX:yy), falling back to the
alphabetically first id so the choice is deterministic and reviewable.

Writes gnews_dupes.json: {keeper: [duplicate ids...]}.
"""
import concurrent.futures as cf
import hashlib, json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(os.path.dirname(HERE)), "collectors", "sources")
FILES = ["gnews_world_topics_1.c", "gnews_world_topics_2.c"]

RSSX = re.compile(
    r'RSSX\(\s*\w+\s*,\s*"([^"]+)"\s*,[^,]*,[^,]*,[^,]*,[^,]*,\s*"([^"]+)"', re.S)
GUID = re.compile(r"<guid[^>]*>(.*?)</guid>", re.S)
CEID = re.compile(r"ceid=([A-Za-z]{2}):([A-Za-z\-]+)")

UA = ("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/126 Safari/537.36")


def entries():
    out = []
    for fn in FILES:
        p = os.path.join(SRC, fn)
        if not os.path.exists(p):
            continue
        t = open(p, encoding="utf-8", errors="replace").read()
        for sid, url in RSSX.findall(t):
            if sid.startswith("gnews-"):
                out.append((sid, url, fn))
    return out


def fetch(item):
    sid, url, fn = item
    try:
        r = subprocess.run(["curl", "-s", "-A", UA, "-L", "--max-time", "25", url],
                           capture_output=True, timeout=40)
        body = r.stdout.decode("utf-8", "replace")
    except Exception:
        return sid, None, None, fn
    guids = [g.strip() for g in GUID.findall(body)]
    if not guids:
        return sid, None, None, fn
    h = hashlib.sha1("\n".join(sorted(guids)).encode()).hexdigest()
    m = CEID.search(body)                      # the edition Google ACTUALLY served
    served = m.group(1).lower() if m else None
    return sid, h, served, fn


def main():
    items = entries()
    print("gnews sources found: %d" % len(items), file=sys.stderr)
    res = []
    with cf.ThreadPoolExecutor(10) as ex:
        for i, r in enumerate(ex.map(fetch, items), 1):
            res.append(r)
            if i % 50 == 0:
                print("  fetched %d/%d" % (i, len(items)), file=sys.stderr)

    groups = {}
    for sid, h, served, fn in res:
        if not h:
            continue
        groups.setdefault(h, []).append((sid, served))

    dupes, kept = {}, 0
    for h, members in groups.items():
        if len(members) < 2:
            continue
        # keeper: the id whose country code matches the served edition
        keeper = None
        for sid, served in sorted(members):
            cc = sid.split("-")[1] if len(sid.split("-")) > 2 else ""
            if served and cc == served:
                keeper = sid
                break
        if not keeper:
            keeper = sorted(m[0] for m in members)[0]
        others = sorted(m[0] for m in members if m[0] != keeper)
        dupes[keeper] = others
        kept += 1

    total = sum(len(v) for v in dupes.values())
    print("\nduplicate groups: %d   redundant sources: %d" % (kept, total))
    for k in sorted(dupes):
        print("  keep %-28s  drop %s" % (k, " ".join(dupes[k])))
    json.dump(dupes, open(os.path.join(HERE, "gnews_dupes.json"), "w"), indent=1)
    print("\n-> gnews_dupes.json")


if __name__ == "__main__":
    main()
