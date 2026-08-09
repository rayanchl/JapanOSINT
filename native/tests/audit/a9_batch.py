#!/usr/bin/env python3
"""Slot-a9 compact batch verifier: run N sources, one line of truth each.

  python3 tests/audit/a9_batch.py list.txt        # "ID" or "ID entity" per line
Prints: id | exit | schedrc | recs | rows | geo | geom | notitle | rt | err-tail
"""
import os, re, shutil, sqlite3, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(os.path.dirname(HERE))
BIN = os.path.join(NATIVE, "bin", "japanosint-a9")
SCHED_RE = re.compile(r"\[sched\] (\S+) run rc=(-?\d+) records=(-?\d+) (\d+)ms")


def one(sid, entity, timeout=180):
    tmp = tempfile.mkdtemp(prefix="a9v_")
    db = os.path.join(tmp, "v.db")
    env = dict(os.environ, JO_DB=db,
               JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
    argv = [BIN, "--run", sid] + ([entity] if entity else [])
    try:
        p = subprocess.run(argv, capture_output=True, timeout=timeout, env=env,
                           cwd=NATIVE)
        err = p.stderr.decode("utf-8", "replace")
        rc_txt = str(p.returncode)
    except subprocess.TimeoutExpired as e:
        err = (e.stderr or b"").decode("utf-8", "replace")
        rc_txt = "TMO"
    m = [x for x in SCHED_RE.finditer(err) if x.group(1) == sid]
    schedrc, recs = (m[-1].group(2), m[-1].group(3)) if m else ("?", "?")
    rows = geo = geom = notitle = 0
    rts = ""
    dberr = ""
    if os.path.exists(db):
        try:
            con = sqlite3.connect("file:%s?mode=ro" % db, uri=True)
            cur = con.cursor()
            rows = cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=?",
                               (sid,)).fetchone()[0]
            geo = cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=?"
                              " AND lat IS NOT NULL", (sid,)).fetchone()[0]
            geom = cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=?"
                               " AND geometry IS NOT NULL AND geometry<>''",
                               (sid,)).fetchone()[0]
            notitle = cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=?"
                                  " AND (title IS NULL OR title='')",
                                  (sid,)).fetchone()[0]
            rts = ",".join("%s=%d" % r for r in cur.execute(
                "SELECT COALESCE(record_type,'(null)'),COUNT(*) FROM intel_items"
                " WHERE source_id=? GROUP BY 1 LIMIT 4", (sid,)).fetchall())
            con.close()
        except Exception as e:
            dberr = "DB_ERROR:%s" % str(e)[:60]
    shutil.rmtree(tmp, ignore_errors=True)
    tail = ""
    for l in reversed(err.splitlines()):
        if "[sched]" in l or l.startswith("[db] "):
            continue
        if l.strip():
            tail = l.strip()[:150]
            break
    return "%-26s exit=%-4s rc=%-3s recs=%-5s rows=%-5d geo=%-5d geom=%-5d notitle=%-4d %s %s %s" % (
        sid, rc_txt, schedrc, recs, rows, geo, geom, notitle, rts, dberr, tail)


def main():
    with open(sys.argv[1]) as f:
        items = [l.split(None, 1) for l in (x.strip() for x in f)
                 if l and not l.startswith("#")]
    for it in items:
        sid = it[0]
        ent = it[1] if len(it) > 1 else None
        print(one(sid, ent), flush=True)


if __name__ == "__main__":
    main()
