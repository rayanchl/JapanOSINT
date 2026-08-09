#!/usr/bin/env python3
"""Run ONE source against a throwaway DB and print everything it emitted.

This is the auditor's microscope: unlike audit_run.py (which summarises 2000
sources) this dumps the actual rows so you can judge whether the data is real,
complete, and correctly shaped.

  ./verify_one.py --slot a1 JMA_EARTHQUAKE
  ./verify_one.py --slot a1 --rows 5 ASN_LOOKUP 8.8.8.8
  ./verify_one.py --slot a1 --stderr SOME_SOURCE      # show collector logging
"""
import argparse, json, os, re, shutil, sqlite3, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(os.path.dirname(HERE))
SCHED_RE = re.compile(r"\[sched\] (\S+) run rc=(-?\d+) records=(-?\d+) (\d+)ms")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source_id")
    ap.add_argument("entity", nargs="?", default=None)
    ap.add_argument("--slot", default="", help="build slot -> bin/japanosint-<slot>")
    ap.add_argument("--rows", type=int, default=3, help="rows to dump in full")
    ap.add_argument("--timeout", type=int, default=180)
    ap.add_argument("--stderr", action="store_true", help="print collector stderr")
    a = ap.parse_args()

    binary = os.path.join(NATIVE, "bin",
                          "japanosint-" + a.slot if a.slot else "japanosint")
    if not os.path.exists(binary):
        sys.exit("no such binary: %s  (run tests/audit/agent_build.sh %s)" % (binary, a.slot))

    tmp = tempfile.mkdtemp(prefix="joverify_")
    db = os.path.join(tmp, "v.db")
    env = dict(os.environ, JO_DB=db, JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
    argv = [binary, "--run", a.source_id] + ([a.entity] if a.entity else [])
    try:
        p = subprocess.run(argv, capture_output=True, timeout=a.timeout, env=env, cwd=NATIVE)
        err = p.stderr.decode("utf-8", "replace")
        rc_txt = str(p.returncode)
    except subprocess.TimeoutExpired as e:
        err = (e.stderr or b"").decode("utf-8", "replace")
        rc_txt = "TIMEOUT after %ss" % a.timeout

    print("=== %s%s" % (a.source_id, "  entity=%s" % a.entity if a.entity else ""))
    print("exit: %s" % rc_txt)
    m = [x for x in SCHED_RE.finditer(err) if x.group(1) == a.source_id]
    if m:
        print("sched: rc=%s records=%s %sms" % (m[-1].group(2), m[-1].group(3), m[-1].group(4)))
    else:
        print("sched: (no [sched] line — the collector never returned)")

    if a.stderr:
        noise = re.compile(r'^\[db\] (migrated|seeded|opened)')
        lines = [l for l in err.splitlines() if not noise.match(l)]
        print("--- stderr ---")
        print("\n".join(lines[-60:]))

    if os.path.exists(db):
        con = sqlite3.connect("file:%s?mode=ro" % db, uri=True)
        cur = con.cursor()
        n = cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=?",
                        (a.source_id,)).fetchone()[0]
        geo = cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=? AND lat IS NOT NULL",
                          (a.source_id,)).fetchone()[0]
        gj = cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=?"
                         " AND geometry IS NOT NULL AND geometry<>''",
                         (a.source_id,)).fetchone()[0]
        print("--- persisted: %d rows, %d with lat/lon, %d with geometry ---" % (n, geo, gj))
        if n:
            rts = cur.execute("SELECT COALESCE(record_type,'(null)'), COUNT(*) FROM intel_items"
                              " WHERE source_id=? GROUP BY 1", (a.source_id,)).fetchall()
            print("record_type: " + ", ".join("%s=%d" % r for r in rts))
            subs = cur.execute("SELECT COALESCE(sub_source_id,'(null)'), COUNT(*) FROM intel_items"
                               " WHERE source_id=? GROUP BY 1 LIMIT 15", (a.source_id,)).fetchall()
            print("sub_source_id: " + ", ".join("%s=%d" % r for r in subs))
            cols = ["uid", "title", "body", "summary", "link", "author", "language",
                    "published_at", "lat", "lon", "geometry", "record_type",
                    "sub_source_id", "tags", "properties"]
            rows = cur.execute("SELECT %s FROM intel_items WHERE source_id=? LIMIT ?"
                               % ",".join(cols), (a.source_id, a.rows)).fetchall()
            for i, r in enumerate(rows, 1):
                print("\n--- row %d ---" % i)
                for k, v in zip(cols, r):
                    if v in (None, "", "{}", "[]"):
                        continue
                    s = v if isinstance(v, str) else repr(v)
                    if len(s) > 900:
                        s = s[:900] + " …<%d more chars>" % (len(s) - 900)
                    print("  %-14s %s" % (k, s))
        con.close()
    shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
