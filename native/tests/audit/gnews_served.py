#!/usr/bin/env python3
"""For each surviving Google News keeper, report the edition Google ACTUALLY
serves, so mislabelled survivors can be renamed to what they really are.

Reads the channel's own <link>…ceid=CC:lang and <language> element — Google
states the edition it fell back to, the collector just never looked.
"""
import concurrent.futures as cf, json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(os.path.dirname(HERE)), "collectors", "sources")
FILES = ["gnews_world_topics_1.c", "gnews_world_topics_2.c"]
RSSX = re.compile(
    r'RSSX\(\s*\w+\s*,\s*"([^"]+)"\s*,[^,]*,[^,]*,[^,]*,[^,]*,\s*"([^"]+)"', re.S)
CEID = re.compile(r"ceid=([A-Za-z]{2}):([A-Za-z\-]+)")
LANG = re.compile(r"<language>([^<]+)</language>")
TITLE = re.compile(r"<title>([^<]{0,80})</title>")
UA = ("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/126 Safari/537.36")

keepers = sorted(json.load(open(os.path.join(HERE, "gnews_dupes.json"),
                                encoding="utf-8")).keys())
urls = {}
for fn in FILES:
    p = os.path.join(SRC, fn)
    if os.path.exists(p):
        t = open(p, encoding="utf-8", errors="replace").read()
        for sid, url in RSSX.findall(t):
            urls[sid] = url


def probe(sid):
    url = urls.get(sid)
    if not url:
        return sid, None, None, None
    try:
        r = subprocess.run(["curl", "-s", "-A", UA, "-L", "--max-time", "25", url],
                           capture_output=True, timeout=40)
        b = r.stdout.decode("utf-8", "replace")
    except Exception:
        return sid, None, None, None
    m = CEID.search(b); l = LANG.search(b); t = TITLE.search(b)
    return (sid, m.group(1).lower() if m else None,
            l.group(1) if l else None, t.group(1).strip() if t else None)


rows = []
with cf.ThreadPoolExecutor(8) as ex:
    rows = list(ex.map(probe, keepers))

print("%-26s %-4s %-8s %-8s %s" % ("keeper", "own", "served", "lang", "channel title"))
mis = {}
for sid, served, lang, title in sorted(rows):
    own = sid.split("-")[1] if len(sid.split("-")) > 2 else "?"
    flag = ""
    if served and own != served:
        flag = "  <-- MISLABELLED"
        mis[sid] = {"own": own, "served": served, "lang": lang, "title": title}
    print("%-26s %-4s %-8s %-8s %s%s" % (sid, own, served or "?", lang or "?",
                                         (title or "")[:44], flag))
json.dump(mis, open(os.path.join(HERE, "gnews_mislabelled.json"), "w"), indent=1)
print("\nmislabelled keepers: %d -> gnews_mislabelled.json" % len(mis))
