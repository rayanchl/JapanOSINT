# Items 2 + 3: back Google News off to ~7200 s with a per-source stagger, and
# repair the 313 definitions that carry the copy-pasted es-419 label.
import os, re, sys
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), "sources"))

ORDER = ["gnews_world_top.c", "gnews_world_topics_1.c",
         "gnews_world_topics_2.c", "gnews_osint_monitors.c"]

PAT = re.compile(
    r'RSSX\(\s*(\w+)\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*,'
    r'\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*,'
    r'\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*(\d+)\s*,\s*"((?:[^"\\]|\\.)*)"\s*\)\s*;',
    re.S)

# gl -> (English country, Japanese country); gnews_world_top.c is the one file
# whose 142 country labels were never templated over, so it is the source of truth.
GL = {}
for m in PAT.finditer(open("gnews_world_top.c", encoding="utf-8").read()):
    gl = re.search(r'[?&]gl=([^&"]+)', m.group(7)).group(1)
    GL[gl] = (m.group(3).split(" — ", 1)[1], m.group(4).split(" — ", 1)[1])

TOPIC = {"WORLD": "World", "BUSINESS": "Business", "TECHNOLOGY": "Technology",
         "SCIENCE": "Science", "HEALTH": "Health", "NATION": "National"}

NOTE = """/* AUDIT NOTE (2026-08-09) — why the interval is ~7200 and not 3600/1800.
 *
 * Every source in gnews_world_top.c (142), gnews_world_topics_1.c (218),
 * gnews_world_topics_2.c (185) and gnews_osint_monitors.c (68) hits ONE host,
 * news.google.com. At the previous intervals (3600 s for the three country
 * files, 1800 s for the monitors) the scheduler wanted 681 requests per hour
 * from one IP — one every 5.3 s.
 *
 * reddit_world_geo.c is the written post-mortem of exactly this failure on a
 * different host: 142 feeds at 1800 s (one per 12.7 s) had 135 of them return
 * rc=-1 with zero rows, because rss_collect() maps any non-2xx — including a
 * transient 429 — to -1, which anomaly_detect turns into status_bad and
 * quarantines. Google News is served through the same RSSX body at ~2.4x that
 * request rate, and throttling is host-correlated, so the whole ~24% of the
 * fleet that lives on news.google.com trips together on one trigger.
 *
 * 7200 s puts the cohort at ~11.7 s average spacing (306 req/h, less than half
 * the old rate). The interval is additionally staggered by one second per
 * source (7200 + cohort index, so 7200..7812): identical intervals preserve
 * whatever burst the boot ramp created forever, whereas a 1 s spread de-phases
 * the 613 sources across a full window inside a day and keeps them spread.
 *
 * As in the reddit case this is a mitigation, not a cure. The cure is a shared
 * per-host rate limiter (core/hostgate.h exists but is not consulted here) and
 * a distinct "throttled" outcome in lib/rss_atom.c so a 429 stops counting as
 * a collector failure. Both live in shared code and are reported, not patched
 * here.
 */
"""

renamed = 0
retimed = 0
idx = 0
for f in ORDER:
    s = open(f, encoding="utf-8").read()
    ms = list(PAT.finditer(s))
    edits = []          # (start, end, text) applied back-to-front
    for m in ms:
        url = m.group(7)
        ival_new = 7200 + idx
        idx += 1
        if m.group(10) != str(ival_new):
            edits.append((m.start(10), m.end(10), str(ival_new)))
            retimed += 1

        # Only the es-419 template is wrong; the NATION rows were hand-written
        # and already agree with their URLs.
        if "Latin America (es-419)" not in m.group(3):
            continue
        gl = re.search(r'[?&]gl=([^&"]+)', url).group(1)
        hl = re.search(r'[?&]hl=([^&"]+)', url).group(1)
        t = re.search(r'topic/([A-Z]+)', url)
        if not t or gl not in GL:
            print("SKIP (cannot derive):", m.group(2)); continue
        topic = TOPIC[t.group(1)]
        en, ja = GL[gl]
        name = "Google News %s — %s" % (topic, en)
        nameja = "Google %s — %s" % (topic, ja)
        desc = "Google News %s section for %s" % (topic, en)
        if hl == "es-419":
            desc += " — Latin American Spanish (es-419) regional edition"
        edits.append((m.start(11), m.end(11), desc))
        edits.append((m.start(4), m.end(4), nameja))
        edits.append((m.start(3), m.end(3), name))
        renamed += 1

    for a, b, txt in sorted(edits, reverse=True):
        s = s[:a] + txt + s[b:]

    if "AUDIT NOTE (2026-08-09)" not in s:
        anchor = '#include "../../lib/rss_atom.h"\n'
        assert s.count(anchor) == 1, f
        s = s.replace(anchor, anchor + "\n" + NOTE, 1)
    open(f, "w", encoding="utf-8", newline="").write(s)

print("definitions relabelled:", renamed, " intervals restaggered:", retimed,
      " cohort size:", idx)
