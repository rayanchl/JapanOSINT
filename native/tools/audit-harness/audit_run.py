#!/usr/bin/env python3
"""Per-source live audit harness.

Runs `japanosint --run <ID> [entity]` against an isolated SQLite DB and reports
what the source ACTUALLY emitted: row count, geo coverage (map-pin capability),
record types, field fill rates and a sample row. One JSON object per source on
stdout (JSONL), so a swarm of auditors can slice it.

  ./audit_run.py --ids ids.txt --out results.jsonl --jobs 8 --timeout 90

`ids.txt` lines are "<source_id>" or "<source_id>\t<entity>".
"""
import argparse, json, os, re, shutil, sqlite3, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed

NATIVE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BIN = os.path.join(NATIVE, "bin", "japanosint")
SCHEMA = os.path.join(NATIVE, "core", "schema.sql")
SCHED_RE = re.compile(r"\[sched\] (\S+) run rc=(-?\d+) records=(-?\d+) (\d+)ms")

# Columns of intel_items we measure fill-rate on. `geometry` and lat/lon drive
# whether a source can ever produce a map pin.
FIELDS = ["title", "body", "summary", "link", "author", "language",
          "published_at", "tags", "properties", "record_type", "sub_source_id",
          "geometry"]


def inspect(dbpath, source_id):
    """Read back everything the sink persisted for this source."""
    out = {"rows": 0, "geo_rows": 0, "geometry_rows": 0, "record_types": [],
           "fill": {}, "sample": None, "props_keys": [], "distinct_titles": 0,
           "db_error": None}
    if not os.path.exists(dbpath):
        out["db_error"] = "no db file"
        return out
    try:
        con = sqlite3.connect("file:%s?mode=ro" % dbpath, uri=True, timeout=20)
        cur = con.cursor()
        cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=?", (source_id,))
        out["rows"] = cur.fetchone()[0]
        if out["rows"] == 0:
            con.close()
            return out
        cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=? AND lat IS NOT NULL"
                    " AND lon IS NOT NULL", (source_id,))
        out["geo_rows"] = cur.fetchone()[0]
        cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=? AND geometry IS NOT NULL"
                    " AND geometry<>''", (source_id,))
        out["geometry_rows"] = cur.fetchone()[0]
        cur.execute("SELECT COUNT(DISTINCT title) FROM intel_items WHERE source_id=?", (source_id,))
        out["distinct_titles"] = cur.fetchone()[0]
        cur.execute("SELECT DISTINCT COALESCE(record_type,'(null)') FROM intel_items"
                    " WHERE source_id=? LIMIT 25", (source_id,))
        out["record_types"] = [r[0] for r in cur.fetchall()]
        for f in FIELDS:
            cur.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=? AND %s IS NOT NULL"
                        " AND %s<>'' AND %s NOT IN ('{}','[]')" % (f, f, f), (source_id,))
            out["fill"][f] = cur.fetchone()[0]
        cur.execute("SELECT uid,title,body,summary,link,published_at,lat,lon,record_type,"
                    "sub_source_id,tags,properties FROM intel_items WHERE source_id=? LIMIT 1",
                    (source_id,))
        r = cur.fetchone()
        if r:
            cols = ["uid", "title", "body", "summary", "link", "published_at", "lat", "lon",
                    "record_type", "sub_source_id", "tags", "properties"]
            s = {}
            for k, v in zip(cols, r):
                if isinstance(v, str) and len(v) > 1200:
                    v = v[:1200] + "…<truncated>"
                s[k] = v
            out["sample"] = s
            try:
                p = json.loads(r[11] or "{}")
                if isinstance(p, dict):
                    out["props_keys"] = sorted(p.keys())[:60]
            except Exception:
                out["props_keys"] = ["<unparseable properties JSON>"]
        con.close()
    except Exception as e:  # a corrupt/locked DB is itself a finding
        out["db_error"] = "%s: %s" % (type(e).__name__, e)
    return out


def run_one(spec, timeout, keep_db_dir):
    sid, entity = (spec + ["", ""])[:2] if isinstance(spec, list) else (spec, "")
    tmp = tempfile.mkdtemp(prefix="joaudit_")
    dbpath = os.path.join(tmp, "t.db")
    env = dict(os.environ)
    env["JO_DB"] = dbpath
    env["JO_SCHEMA"] = SCHEMA
    argv = [BIN, "--run", sid] + ([entity] if entity else [])
    rec = {"id": sid, "entity": entity or None, "rc": None, "records": None,
           "duration_ms": None, "timed_out": False, "crashed": False,
           "stderr_tail": "", "argv": " ".join(argv[1:])}
    try:
        p = subprocess.run(argv, capture_output=True, timeout=timeout, env=env, cwd=NATIVE)
        err = p.stderr.decode("utf-8", "replace")
        rec["exit_code"] = p.returncode
        # negative exit code == killed by signal (SIGSEGV/SIGABRT) -> a real crash
        rec["crashed"] = p.returncode < 0 or p.returncode > 128
        m = None
        for m2 in SCHED_RE.finditer(err):
            if m2.group(1) == sid:
                m = m2
        if m:
            rec["rc"] = int(m.group(2))
            rec["records"] = int(m.group(3))
            rec["duration_ms"] = int(m.group(4))
        rec["stderr_tail"] = err[-2500:]
    except subprocess.TimeoutExpired as e:
        rec["timed_out"] = True
        rec["exit_code"] = None
        rec["stderr_tail"] = (e.stderr or b"").decode("utf-8", "replace")[-2500:]
    rec.update(inspect(dbpath, sid))
    if keep_db_dir:
        try:
            shutil.move(dbpath, os.path.join(keep_db_dir, sid.replace("/", "_") + ".db"))
        except Exception:
            pass
    shutil.rmtree(tmp, ignore_errors=True)
    return rec


def verdict(r):
    """Coarse bucket so a 2000-row report is triageable at a glance."""
    if r["crashed"]:
        return "CRASH"
    if r["timed_out"]:
        return "TIMEOUT"
    if r.get("db_error"):
        return "DB_ERROR"
    if r["rc"] is None:
        return "NO_RUN"          # binary never reached the collector
    if r["rc"] != 0:
        return "RC_ERROR"
    if r["rows"] == 0:
        return "EMPTY"           # ran clean, persisted nothing
    if r["rows"] > 0 and r["fill"].get("title", 0) == 0:
        return "NO_TITLE"
    return "DATA"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ids", required=True, help="file: <id>[\\t<entity>] per line")
    ap.add_argument("--out", required=True)
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--timeout", type=int, default=90)
    ap.add_argument("--keep-dbs", default="")
    ap.add_argument("--resume", action="store_true",
                    help="append, skipping ids already present in --out")
    a = ap.parse_args()
    if a.keep_dbs:
        os.makedirs(a.keep_dbs, exist_ok=True)
    specs = []
    with open(a.ids, encoding="utf-8") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            parts = line.split("\t")
            specs.append([parts[0].strip(), parts[1].strip() if len(parts) > 1 else ""])

    # Resume: a 2000-run sweep is ~20 minutes of live network, so losing it to a
    # relink of the binary underneath (Errno 26) must not mean starting over.
    seen = set()
    if a.resume and os.path.exists(a.out):
        for line in open(a.out, encoding="utf-8"):
            line = line.strip()
            if line:
                try:
                    seen.add(json.loads(line)["id"])
                except (json.JSONDecodeError, KeyError):
                    pass
        specs = [s for s in specs if s[0] not in seen]
        print("resuming: %d already done, %d to go" % (len(seen), len(specs)), file=sys.stderr)
    done = 0
    # as_completed, not map: map yields in submission order, so one 120s source
    # at the head of the list stalls all reporting for a 2000-run sweep.
    with open(a.out, "a" if a.resume else "w", encoding="utf-8") as fh, \
            ThreadPoolExecutor(a.jobs) as ex:
        futs = [ex.submit(run_one, s, a.timeout, a.keep_dbs) for s in specs]
        for fut in as_completed(futs):
            rec = fut.result()
            rec["verdict"] = verdict(rec)
            fh.write(json.dumps(rec, ensure_ascii=False) + "\n")
            fh.flush()
            done += 1
            print("[%d/%d] %-28s %-8s rows=%-5s geo=%-5s rc=%s" %
                  (done, len(specs), rec["id"], rec["verdict"], rec["rows"],
                   rec["geo_rows"], rec["rc"]), file=sys.stderr)


if __name__ == "__main__":
    main()
