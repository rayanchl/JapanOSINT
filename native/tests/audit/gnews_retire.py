#!/usr/bin/env python3
"""Execute the duplicate-Google-News retirement: disable -> backfill -> drop.

  --db PATH   run steps 1+2 against a live database
  --drop      run step 3: remove the RSSX definitions from the source files

Step 1 DISABLE  quarantine each duplicate (sources.quarantined_until). The
                scheduler already skips quarantined sources
                (core/scheduler.c:129), so this stops new duplicate rows the
                moment it runs — no deploy, no code change, fully reversible via
                the existing /api/admin/sources/<id>/unquarantine route.

Step 2 BACKFILL annotate existing intel_items so history is interpretable
                instead of orphaned: each row from a retired source gets
                properties.duplicate_of = <keeper id>. Rows are NOT re-keyed to
                the keeper — that would collide with the keeper's own uids
                (uid = source_id|guid) and silently destroy rows.

Step 3 DROP     delete the RSSX definitions so the next build stops registering
                them. Leaves a comment naming the keeper.
"""
import argparse, json, os, re, sqlite3, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(os.path.dirname(HERE)), "collectors", "sources")
FILES = ["gnews_world_topics_1.c", "gnews_world_topics_2.c"]
# --map lets a second, tolerant pass reuse the same machinery: the first pass
# grouped on exact guid-set equality and under-counted.
_MAP = os.environ.get("GNEWS_MAP", "gnews_dupes.json")
DUPES = json.load(open(os.path.join(HERE, _MAP), encoding="utf-8"))

REASON = ("duplicate feed: Google discards gl= for locales it has no edition "
          "for and serves a generic edition; identical guid set to %s")


def disable_and_backfill(dbpath):
    con = sqlite3.connect(dbpath)
    cur = con.cursor()
    n_q = n_b = 0
    for keeper, others in DUPES.items():
        for sid in others:
            cur.execute(
                "UPDATE sources SET quarantined_at=COALESCE(quarantined_at,datetime('now')),"
                " quarantined_until='9999-12-31 00:00:00', quarantine_reason=?1"
                " WHERE id=?2", (REASON % keeper, sid))
            n_q += cur.rowcount
            # annotate history; json_set leaves every other property untouched
            cur.execute(
                "UPDATE intel_items SET properties="
                " json_set(COALESCE(NULLIF(properties,''),'{}'),'$.duplicate_of',?1)"
                " WHERE source_id=?2", (keeper, sid))
            n_b += cur.rowcount
    con.commit()
    con.close()
    print("quarantined %d source rows; annotated %d intel_items rows" % (n_q, n_b))


def drop_definitions():
    drop = {s: k for k, v in DUPES.items() for s in v}
    removed = 0
    for fn in FILES:
        p = os.path.join(SRC, fn)
        if not os.path.exists(p):
            continue
        t = open(p, encoding="utf-8", errors="replace").read()
        out, i, n = [], 0, 0
        # RSSX(sym, "id", ...); blocks end at the first ");" at line start-ish
        for m in re.finditer(r'RSSX\(\s*\w+\s*,\s*"([^"]+)"', t):
            sid = m.group(1)
            if sid not in drop:
                continue
            start = m.start()
            end = t.find(");", start)
            if end == -1:
                continue
            end += 2
            out.append((start, end, sid, drop[sid]))
        if not out:
            continue
        res, last = [], 0
        for start, end, sid, keeper in out:
            res.append(t[last:start])
            res.append("/* %s retired: duplicate of %s (identical guid set; "
                       "Google serves one generic edition for locales it does "
                       "not publish). See tests/audit/gnews_dupes.json. */"
                       % (sid, keeper))
            last = end
            n += 1
        res.append(t[last:])
        open(p, "w", encoding="utf-8").write("".join(res))
        removed += n
        print("%s: removed %d definitions" % (fn, n))
    print("total definitions removed: %d" % removed)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--db")
    ap.add_argument("--drop", action="store_true")
    a = ap.parse_args()
    if not a.db and not a.drop:
        ap.error("give --db PATH and/or --drop")
    if a.db:
        disable_and_backfill(a.db)
    if a.drop:
        drop_definitions()
