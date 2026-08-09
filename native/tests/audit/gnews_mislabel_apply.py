#!/usr/bin/env python3
"""Apply the mislabelled-keeper resolution.

  --drop      code changes: retire the 6 duplicates, rename the 5 es-419 sources
  --db PATH   database migration for BOTH actions

RETIRE (6): gnews-am-* and gnews-hr-* serve editions that already have a
correctly-named source (jaccard 0.97-1.00). Same treatment as the other
duplicates: quarantine + annotate history + drop the definition.

RENAME (5): the gnews-bo-*/gnews-cr-science group serves ceid=US but in es-419
(Latin American Spanish) and shares nothing with the English us-* feeds
(jaccard 0.000). It is a real, distinct edition that simply has no country to
belong to — Bolivia was an arbitrary member of an 8-country group. Rename to
`gnews-es419-<topic>`, which is what it actually is. `gnews-es-*` already exists
and is Spain (hl=es-ES), so there is no collision.

The DB rename rewrites source_id in every table that carries one AND the uid
prefix in intel_items (uid = "<source_id>|<guid>"), plus the FTS uid maps —
missing any one of them orphans rows.
"""
import argparse, json, os, re, sqlite3

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(os.path.dirname(HERE)), "collectors", "sources")
FILES = ["gnews_world_topics_1.c", "gnews_world_topics_2.c"]
ACT = json.load(open(os.path.join(HERE, "gnews_mislabel_actions.json"),
                     encoding="utf-8"))
RETIRE = ACT["retire"]                       # sid -> {counterpart, jaccard, ...}
RENAME = {sid: "gnews-es419-" + sid.rsplit("-", 1)[1] for sid in ACT["rename"]}

REASON = ("duplicate feed: serves the %s edition, which %s already collects "
          "(jaccard %.2f)")


def code_changes():
    for fn in FILES:
        p = os.path.join(SRC, fn)
        if not os.path.exists(p):
            continue
        t = open(p, encoding="utf-8", errors="replace").read()
        orig = t

        # --- retire: replace the whole RSSX block with a note
        for sid, info in RETIRE.items():
            m = re.search(r'RSSX\(\s*\w+\s*,\s*"%s"' % re.escape(sid), t)
            if not m:
                continue
            end = t.find(");", m.start())
            if end == -1:
                continue
            t = (t[:m.start()]
                 + "/* %s retired: serves the %s edition, already collected by "
                   "%s (jaccard %.2f). See tests/audit/gnews_mislabel_actions.json. */"
                   % (sid, info["served"], info["counterpart"], info["jaccard"])
                 + t[end + 2:])

        # --- rename: id, symbol, display names, country tag, description
        for old, new in RENAME.items():
            if '"%s"' % old not in t:
                continue
            topic = old.rsplit("-", 1)[1]
            country = old.split("-")[1]
            m = re.search(r'RSSX\(\s*(\w+)\s*,\s*"%s"' % re.escape(old), t)
            if m:
                t = t.replace(m.group(1), "gn_es419_" + topic)
            t = t.replace('"%s"' % old, '"%s"' % new)
            # display names: "… — Bolivia" -> "… — Latin America (es-419)"
            t = re.sub(r'("Google News [A-Za-z]+) \xe2\x80\x94 [^"]*"'.encode()
                       .decode('unicode_escape'),
                       r'\1 — Latin America (es-419)"', t) if False else t
            t = re.sub(r'("Google News %s) — [^"]+"' % topic.capitalize(),
                       r'\1 — Latin America (es-419)"', t)
            t = re.sub(r'("Google %s) — [^"]+"' % topic.capitalize(),
                       r'\1 — ラテンアメリカ"', t)
            t = t.replace('\\"%s\\"' % country, '\\"latin-america\\"')
            t = re.sub(r'"Google News %s section for [^"]+"' % topic.capitalize(),
                       '"Google News %s, Latin American Spanish (es-419) edition '
                       '— Google serves one es-419 feed for the whole region"'
                       % topic.capitalize(), t)
        if t != orig:
            open(p, "w", encoding="utf-8").write(t)
            print("%s: updated" % fn)


def db_changes(dbpath):
    con = sqlite3.connect(dbpath)
    cur = con.cursor()

    # Tables that carry a source_id. fetchall() FIRST: running PRAGMA on the
    # same cursor mid-iteration destroys the outer result set, so the list came
    # back empty and every table except intel_items was silently skipped —
    # which is exactly how a rename orphans history.
    names = [r[0] for r in cur.execute(
        "SELECT name FROM sqlite_master WHERE type='table'").fetchall()]
    tabs = []
    for n in names:
        try:
            cols = [r[1] for r in cur.execute("PRAGMA table_info(%s)" % n).fetchall()]
        except sqlite3.Error:
            continue
        if "source_id" in cols:
            tabs.append(n)

    nq = nb = 0
    for sid, info in RETIRE.items():
        cur.execute(
            "UPDATE sources SET quarantined_at=COALESCE(quarantined_at,datetime('now')),"
            " quarantined_until='9999-12-31 00:00:00', quarantine_reason=?1 WHERE id=?2",
            (REASON % (info["served"], info["counterpart"], info["jaccard"]), sid))
        nq += cur.rowcount
        cur.execute(
            "UPDATE intel_items SET properties="
            " json_set(COALESCE(NULLIF(properties,''),'{}'),'$.duplicate_of',?1)"
            " WHERE source_id=?2", (info["counterpart"], sid))
        nb += cur.rowcount
    print("retire: quarantined %d, annotated %d rows" % (nq, nb))

    nr = 0
    for old, new in RENAME.items():
        # uid carries the source_id as its prefix — rewrite it or every row
        # is orphaned from its source.
        cur.execute("UPDATE intel_items SET uid = ?1 || substr(uid, ?2),"
                    " source_id = ?1 WHERE source_id = ?3",
                    (new, len(old) + 1, old))
        nr += cur.rowcount
        for t in tabs:
            if t == "intel_items":
                continue
            try:
                cur.execute("UPDATE %s SET source_id=?1 WHERE source_id=?2" % t,
                            (new, old))
            except sqlite3.Error as e:
                print("  warn %s: %s" % (t, e))
        for t in ("intel_items_fts_uid_map", "entities_fts_uid_map"):
            try:
                cur.execute("UPDATE %s SET uid = ?1 || substr(uid, ?2)"
                            " WHERE uid LIKE ?3" % t,
                            (new, len(old) + 1, old + "|%"))
            except sqlite3.Error:
                pass
    print("rename: rewrote %d intel_items rows across %d id-bearing tables"
          % (nr, len(tabs)))
    con.commit()
    con.close()


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--db")
    ap.add_argument("--drop", action="store_true")
    a = ap.parse_args()
    if a.db:
        db_changes(a.db)
    if a.drop:
        code_changes()
    if not a.db and not a.drop:
        ap.error("give --db PATH and/or --drop")
