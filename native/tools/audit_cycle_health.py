#!/usr/bin/env python3
"""Which `page_resume` rows cannot actually advance their cycle.

THE SHAPE THIS FINDS, and it was found by accident. `ENA_PATHOGEN_SARSCOV2_RUNS`
read 1,000 records of 7.5 million on two consecutive runs and stored no resume
position, which reads exactly like "this row completes inside its ceiling" — the
measured no-op that makes `page_resume` safe to apply broadly. It was not that.
ebi.ac.uk answers HTTP 400 to the row's second page, so the walk is cut short,
the position is deliberately NOT stored (bookmarking a page that is refused
would retry that one page forever), and the cycle can never move past its first
slice. Every run reads the same 1,000 records, for ever, at rc=0.

A cycling row therefore has THREE outcomes and only one of them is healthy:

  CYCLING       a position was stored: the next run reads the next slice
  COMPLETE      the walk ended on its own inside the ceiling and cleared the
                position — the genuine no-op, strictly correct
  STUCK         a page FAILED, so no position was stored and the next run
                re-reads this same slice. The collection past this point is
                unreachable until the row's paging is re-pointed

STUCK is the finding. It is not a transient-failure verdict: the row is
re-measured here only once, so a 429 or a bad afternoon lands here too, which is
why the failing page's STATUS and URL are reported rather than a bare verdict.
A 4xx that names the parameter (ENA's "Unsupported param offset") is a property
of the endpoint; a 429 or a 5xx is a reading. Judge on the status.

This walks each row to the real ceiling rather than probing shallow, because the
failure can be at any page and a shallow probe would simply stop before it.
That is the cost: ~20 requests a row.

  python3 tools/audit_cycle_health.py --bin ./bin/japanosint \\
          --ids-file /tmp/cycling.txt --jobs 8 --out cycle.tsv

With no --ids-file every registered row that declares page_resume is swept, read
from `--describe-sources` so the C and the sweep cannot disagree.

Each run gets a fresh copy of a warm template database from a slot pool — the
same discipline, and for the same measured reason, as tools/audit_page_ceiling.py.
"""
import argparse
import json
import os
import queue
import re
import shutil
import sqlite3
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor

SCHED = re.compile(r"\[sched\] (\S+) run rc=(-?\d+) records=(-?\d+) (\d+)ms")
NOTICE = "collector-truncation-notice"
FAILURE = "collector-fetch-failure"
_lock = threading.Lock()

COLS = ["id", "verdict", "records", "pages_read", "resume_stored",
        "failed_page", "failed_page_status", "secs", "failed_page_url", "note"]


def checkpoint(db):
    try:
        con = sqlite3.connect(db)
        con.execute("PRAGMA wal_checkpoint(TRUNCATE)")
        con.close()
    except Exception:
        pass


def warm_template(binpath, path):
    env = dict(os.environ, JO_DB=path, JO_FTS_REBUILD="0")
    p = subprocess.run([binpath, "--list-sources"], capture_output=True,
                       env=env, stdin=subprocess.DEVNULL)
    if p.returncode != 0:
        raise SystemExit("could not build a warm template DB at %s" % path)
    checkpoint(path)


ROW = re.compile(r'\{\s*\.id\s*=\s*"([^"]+)"')


def cycling_ids(tree):
    """Every row that declares page_resume, read from the C.

    The C and not a manifest, for the reason the batch-tooling section of
    CLAUDE.md gives: after a beat ships the C is the maintained copy and 48 of
    79 beats already differ from what their manifest generates.

    Returns (id, host) pairs: the HOST is what run_one serialises on, so a
    sweep never puts two rows of the same upstream in flight at once."""
    out = []
    for dirpath, _dirs, files in os.walk(tree):
        for fn in files:
            if not fn.endswith(".c"):
                continue
            p = os.path.join(dirpath, fn)
            with open(p, encoding="utf-8", errors="replace") as f:
                t = f.read()
            if ".page_resume" not in t:
                continue
            for m in ROW.finditer(t):
                nxt = t.find("{ .id =", m.end())
                body = t[m.start():nxt if nxt > 0 else len(t)]
                if not re.search(r"\.page_resume\s*=\s*1", body):
                    continue
                mu = re.search(r'\.url\s*=\s*"https?://([A-Za-z0-9._-]+)', body)
                out.append((m.group(1), mu.group(1).lower() if mu else ""))
    return out


def read_back(db, sid):
    try:
        con = sqlite3.connect("file:%s?mode=ro" % db, uri=True)
        rows = con.execute(
            "SELECT count(*) FROM intel_items WHERE source_id=? "
            "AND record_type NOT LIKE 'collector-%'", (sid,)).fetchone()[0]
        note = con.execute(
            "SELECT properties FROM intel_items WHERE source_id=? "
            "AND record_type=? LIMIT 1", (sid, NOTICE)).fetchone()
        fail = con.execute(
            "SELECT title FROM intel_items WHERE source_id=? "
            "AND record_type=? LIMIT 1", (sid, FAILURE)).fetchone()
        # Where the run said the next one should start. This is the whole
        # question, and it is read out of the DATABASE rather than off the run
        # line: core/scheduler.c persists it, so this is the same value the
        # next run would actually load.
        try:
            tok = con.execute(
                "SELECT resume_token FROM source_sched_state WHERE source_id=?",
                (sid,)).fetchone()
        except sqlite3.Error:
            tok = None
        con.close()
        return (rows, (json.loads(note[0]) if note and note[0] else None),
                (fail[0] if fail and fail[0] else None),
                (tok[0] if tok and tok[0] else None))
    except Exception:
        return None, None, None, None


def classify(sid, blob, rc, rows, note, fail, token, secs, timed_out):
    out = dict.fromkeys(COLS, "")
    out["id"] = sid
    out["records"] = rows if rows is not None else -1
    out["secs"] = secs
    out["resume_stored"] = "yes" if token else "no"
    sched = None
    for m in SCHED.finditer(blob):
        if m.group(1) == sid:
            sched = m
    if timed_out:
        out["verdict"] = "SLOW"
        out["note"] = "hit --timeout at %ss; unmeasured, not failed" % secs
        return out
    if "is not registered" in blob or (rc not in (0, None) and not sched):
        out["verdict"] = "UNREGISTERED"
        return out
    if sched is None:
        out["verdict"] = "NO_RUN_LINE"
        return out
    if note:
        out["pages_read"] = note.get("pages_read", "")
        out["failed_page"] = note.get("failed_page", "")
        out["failed_page_status"] = note.get("failed_page_status", "")
        out["failed_page_url"] = (note.get("failed_page_url", "") or "")[:200]
    if out["failed_page"] != "":
        # The walk was cut short. No position is stored for a failed page, so
        # whatever slice this run read is the slice every future run reads.
        out["verdict"] = "STUCK"
        out["note"] = ("page %s answered %s; no position stored, so the next "
                       "run re-reads this slice"
                       % (out["failed_page"], out["failed_page_status"]))
        return out
    if fail:
        # The FIRST page failed, so there is no walk at all to cut short.
        out["verdict"] = "FETCH_FAILED"
        out["note"] = fail[-110:]
        return out
    if token:
        out["verdict"] = "CYCLING"
        out["note"] = "stored a position; next run reads the next slice"
        return out
    out["verdict"] = "COMPLETE" if (rows or 0) > 0 else "EMPTY"
    if out["verdict"] == "COMPLETE":
        out["note"] = "walk ended on its own and cleared the position"
    return out


def run_one(a, tmpl, sid):
    """One row at a time per HOST, whatever --jobs says.

    THIS TOOL MANUFACTURED ITS OWN FINDINGS without it. The first full sweep
    (2026-10-19, 530 rows, 12 jobs) returned 12 STUCK, and only ONE of them was
    a property of its endpoint. Five `au-asic` rows share data.gov.au and all
    five came back 403; re-tested by hand afterwards that host served 8 rapid
    serial requests and then 12 CONCURRENT ones, all 200. Two FAA rows share
    services6.arcgis.com and failed at pages 18 and 20 while a separate 6-job
    emit audit was hitting the same host. `us-sf-311-open311-requests` is
    already paced at 1,200 ms in core/hostgate.c and still 429'd at page 20.

    A verdict that changes with --jobs is not a measurement — the same
    reasoning that put the scratch-database SLOT POOL in
    tools/audit_page_ceiling.py, arrived at again one layer up. hostgate paces
    requests inside ONE process; it cannot pace twelve processes that each
    think they are alone, and a cycling row makes ~20 requests to one host, so
    two rows of the same host overlap for their whole run.

    Note what this cannot create: load makes a page FAIL, so it can only
    manufacture a false STUCK or FETCH_FAILED. It cannot manufacture a false
    CYCLING or COMPLETE, because those mean a position was or was not stored
    after a walk that was served. So an old sweep's healthy verdicts stand;
    only its findings need re-reading."""
    host = a.host_of.get(sid) or sid
    lock = a.hostlocks.setdefault(host, threading.Lock())
    with lock:
        slot = a.slots.get()
        try:
            return _run_in_slot(a, tmpl, sid, slot)
        finally:
            a.slots.put(slot)


def _run_in_slot(a, tmpl, sid, slot):
    db = os.path.join(a.workdir, "y%d.db" % slot)
    for suf in ("", "-wal", "-shm"):
        try:
            os.unlink(db + suf)
        except OSError:
            pass
    shutil.copyfile(tmpl, db)
    env = dict(os.environ, JO_DB=db, JO_FTS_REBUILD="0", JO_SHAPE_NOTICES="0")
    t0, timed_out, blob, rc = time.time(), False, "", None
    try:
        p = subprocess.run([a.bin, "--run", sid], capture_output=True,
                           encoding="utf-8", errors="replace", env=env,
                           timeout=a.timeout, stdin=subprocess.DEVNULL)
        rc, blob = p.returncode, (p.stdout or "") + (p.stderr or "")
    except subprocess.TimeoutExpired as e:
        timed_out = True
        for part in (e.stdout, e.stderr):
            if part:
                blob += part.decode("utf-8", "replace") if isinstance(
                    part, bytes) else part
    secs = round(time.time() - t0, 1)
    rows, note, fail, token = read_back(db, sid)
    for suf in ("", "-wal", "-shm"):
        try:
            os.unlink(db + suf)
        except OSError:
            pass
    r = classify(sid, blob, rc, rows, note, fail, token, secs, timed_out)
    with _lock:
        sys.stderr.write("%-11s %-46s %s\n" % (r["verdict"], sid, r["note"][:70]))
        sys.stderr.flush()
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default="./bin/japanosint")
    ap.add_argument("--ids-file")
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--out")
    ap.add_argument("--tree", default="collectors",
                    help="collector tree to read page_resume rows out of")
    a = ap.parse_args()

    # The host map comes from the C either way, so an --ids-file sweep is
    # paced per host exactly as a full one is. A row whose host cannot be read
    # (a runtime-composed URL) serialises on its own id, which is the safe
    # direction: it paces more than needed, never less.
    pairs = cycling_ids(a.tree)
    a.host_of = {sid: host for sid, host in pairs if host}
    a.hostlocks = {}

    if a.ids_file:
        ids = [x.strip() for x in open(a.ids_file) if x.strip()
               and not x.startswith("#")]
    else:
        ids = [sid for sid, _h in pairs]
    if not ids:
        raise SystemExit("no cycling rows to sweep")
    sys.stderr.write("sweeping %d cycling rows, %d jobs\n" % (len(ids), a.jobs))

    a.workdir = os.path.join(os.environ.get("TMPDIR", "/tmp"),
                             "jo-cycle-%d" % os.getpid())
    os.makedirs(a.workdir, exist_ok=True)
    tmpl = os.path.join(a.workdir, "template.db")
    warm_template(a.bin, tmpl)
    a.slots = queue.Queue()
    for i in range(a.jobs):
        a.slots.put(i)

    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        res = list(ex.map(lambda s: run_one(a, tmpl, s), ids))

    if a.out:
        with open(a.out, "w") as f:
            f.write("\t".join(COLS) + "\n")
            for r in sorted(res, key=lambda r: (r["verdict"], r["id"])):
                f.write("\t".join(str(r.get(c, "")) for c in COLS) + "\n")
    tally = {}
    for r in res:
        tally[r["verdict"]] = tally.get(r["verdict"], 0) + 1
    for k in sorted(tally, key=lambda k: -tally[k]):
        sys.stderr.write("  %-14s %d\n" % (k, tally[k]))
    shutil.rmtree(a.workdir, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
