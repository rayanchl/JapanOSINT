#!/usr/bin/env python3
"""Run sources and report per-source data-quality stats (null title/link/date/geo)."""
import os, re, sqlite3, subprocess, sys, tempfile, shutil

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(os.path.dirname(HERE))
SCHED = re.compile(r"\[sched\] (\S+) run rc=(-?\d+) records=(-?\d+) (\d+)ms")
BIN = os.path.join(NATIVE, "bin", "japanosint-a4")

def one(sid, ent=None, tmo=200):
    tmp = tempfile.mkdtemp(prefix="a4q_")
    db = os.path.join(tmp, "v.db")
    env = dict(os.environ, JO_DB=db,
               JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
    argv = [BIN, "--run", sid] + ([ent] if ent else [])
    rc = "?"
    try:
        p = subprocess.run(argv, capture_output=True, timeout=tmo, env=env, cwd=NATIVE)
        err = p.stderr.decode("utf-8", "replace")
        m = [x for x in SCHED.finditer(err) if x.group(1) == sid]
        rc = m[-1].group(2) if m else "norc"
    except subprocess.TimeoutExpired:
        rc = "TMO"
    stats = {}
    if os.path.exists(db):
        con = sqlite3.connect("file:%s?mode=ro" % db, uri=True)
        c = con.cursor()
        q = lambda w: c.execute(
            "SELECT COUNT(*) FROM intel_items WHERE source_id=? AND " + w,
            (sid,)).fetchone()[0]
        try:
            stats["rows"] = q("1")
            stats["no_title"] = q("(title IS NULL OR title='')")
            stats["no_link"] = q("(link IS NULL OR link='')")
            stats["no_date"] = q("(published_at IS NULL OR published_at='')")
            stats["no_body"] = q("(body IS NULL OR body='') AND (summary IS NULL OR summary='')")
            stats["geo"] = q("lat IS NOT NULL")
            stats["props"] = q("properties IS NOT NULL AND properties<>'{}'")
            stats["uids"] = c.execute(
                "SELECT COUNT(DISTINCT uid) FROM intel_items WHERE source_id=?",
                (sid,)).fetchone()[0]
        except sqlite3.Error as e:
            stats["ERR"] = str(e)
        con.close()
    shutil.rmtree(tmp, ignore_errors=True)
    return rc, stats

if __name__ == "__main__":
    for line in open(sys.argv[1], encoding="utf-8"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 1)
        sid = parts[0]
        ent = parts[1] if len(parts) > 1 else None
        rc, s = one(sid, ent)
        print("%-42s rc=%-5s %s" % (sid, rc,
              " ".join("%s=%s" % (k, v) for k, v in s.items())), flush=True)
