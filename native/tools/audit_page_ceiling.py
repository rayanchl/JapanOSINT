#!/usr/bin/env python3
"""Which scheduled rows stop at the PAGE CEILING, and how many pages they need.

THE SHAPE THIS FINDS. A row that declares paging and no `page_max` walks to
jo_page_max() (20, lib/pagewalk.c) and stops. That stop is disclosed — a
collector-truncation-notice with `more_pages_pending: true` — but a notice is a
row in a database, not a build error, so a row nobody reads after its first run
keeps discarding the same tail every run, at rc=0, with records and stored
agreeing. `make audit-sources` cannot see it: all six of its checks look for a
bound written in C, and this bound is a constant in the engine.

WHY THIS PROBES SHALLOW AND THEN DOES ARITHMETIC. Walking 1,900 rows to their
real ceiling is tens of thousands of requests and hours of upstream time. It is
also unnecessary, because the engine already reports the only number that
settles the question: when the upstream declares its own total, the notice says
so in `records_available_basis` ("upstream declared this total"). So each row is
run ONCE with a deliberately low ceiling (--probe-pages, default 3) and the
answer is read out of the notice:

  COMPLETE          the walk ended on its own inside the probe ceiling; the row
                    is not ceiling-bound at any setting
  CEILING_DECLARED  more pages pending AND the upstream declared a total, so
                    pages_needed = ceil(total / page_size) is a measured number,
                    not an estimate. `page_max` follows from it
  CEILING_UNKNOWN   more pages pending and the upstream publishes no total. This
                    is NOT a finding on its own: the collection may be 4 pages
                    or 4 million, and only a full walk can say. Reported so the
                    full walk is a deliberate choice about a known list
  NO_PAGING         the row declares no paging; one request per run. Whether
                    that discards anything depends on the endpoint, which is
                    exactly what tools/audit_url_limits.py cannot decide either
  EMPTY / FETCH_FAILED / NEEDS_ENTITY / SLOW / UNREGISTERED

A CEILING verdict is a question, not a diagnosis — the same caution the detail
hop audit arrived at. A relevance-ranked pivot has no complete answer to
truncate, so its ceiling IS its question; a scheduled read of a collection that
has an end is the violation. They are identical here, so the ids are printed and
not acted on.

  python3 tools/audit_page_ceiling.py --bin ./bin/japanosint \\
          --ids-file /tmp/paged.txt --jobs 6 --out ceiling.tsv

Each run gets a fresh copy of a warm template database (the same discipline as
tools/audit_registry_emit.py: a tool that writes the developer's live DB is a
tool nobody runs twice).
"""
import argparse
import json
import math
import os
import queue
import re
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import threading
import time
from concurrent.futures import ThreadPoolExecutor

SCHED = re.compile(r"\[sched\] (\S+) run rc=(-?\d+) records=(-?\d+) (\d+)ms")
NOTICE = "collector-truncation-notice"
FAILURE = "collector-fetch-failure"
_lock = threading.Lock()


def checkpoint(db):
    try:
        con = sqlite3.connect(db)
        con.execute("PRAGMA wal_checkpoint(TRUNCATE)")
        con.close()
    except Exception:
        pass


def warm_template(binpath, path):
    """One boot pays for the schema, the migrations and the registry seed."""
    env = dict(os.environ, JO_DB=path, JO_FTS_REBUILD="0")
    p = subprocess.run([binpath, "--list-sources"], capture_output=True,
                       env=env, stdin=subprocess.DEVNULL)
    if p.returncode != 0:
        raise SystemExit("could not build a warm template DB at %s" % path)
    checkpoint(path)


def read_back(db, sid):
    """Records stored, and the truncation notice this run filed (if any)."""
    try:
        con = sqlite3.connect("file:%s?mode=ro" % db, uri=True)
        rows = con.execute(
            "SELECT count(*) FROM intel_items WHERE source_id=? "
            "AND record_type NOT LIKE 'collector-%'", (sid,)).fetchone()[0]
        note = con.execute(
            "SELECT properties FROM intel_items WHERE source_id=? "
            "AND record_type=? LIMIT 1", (sid, NOTICE)).fetchone()
        # A refused fetch is its own record since 2026-10-06 (rule 1). Without
        # reading it, a 429 reads as EMPTY -- "this source returns nothing",
        # which is the opposite of what a rate limit means, and the sweep would
        # retire live sources on a bad afternoon.
        fail = con.execute(
            "SELECT title FROM intel_items WHERE source_id=? "
            "AND record_type=? LIMIT 1", (sid, FAILURE)).fetchone()
        con.close()
        return (rows, (json.loads(note[0]) if note and note[0] else None),
                (fail[0] if fail and fail[0] else None))
    except Exception:
        return None, None, None


def classify(sid, blob, rc, rows, note, fail, secs, timed_out, probe, page_size):
    out = {"id": sid, "records": rows if rows is not None else -1,
           "secs": secs, "pages_read": "", "available": "", "basis": "",
           "page_size": page_size or "", "pages_needed": "",
           "suggest_page_max": "", "note": ""}
    sched = None
    for m in SCHED.finditer(blob):
        if m.group(1) == sid:
            sched = m
    if timed_out:
        out["verdict"] = "SLOW"
        out["note"] = "hit --timeout at %ss with a %d-page probe" % (secs, probe)
        return out
    if "is not registered" in blob or (rc not in (0, None) and not sched):
        out["verdict"] = "UNREGISTERED"
        return out
    if sched is None:
        out["verdict"] = "NO_RUN_LINE"
        out["note"] = "no [sched] line — not an intel-sink source?"
        return out
    if fail:
        out["verdict"] = "FETCH_FAILED"
        out["note"] = fail[-90:]
        return out
    if note is None:
        out["verdict"] = "COMPLETE" if (rows or 0) > 0 else "EMPTY"
        if out["verdict"] == "COMPLETE":
            out["note"] = "walk ended inside the %d-page probe" % probe
        return out

    out["pages_read"] = note.get("pages_read", "")
    out["available"] = note.get("records_available", "")
    # A row need not declare page_size — the engine only requires it for
    # offset-style stride — so when it does not, the probe measured it: this
    # run read `records_used` over `pages_read` full pages. Marked derived,
    # because a last short page would understate it, and understating the page
    # size only ever asks for MORE pages than needed, never fewer.
    pr = int(out["pages_read"] or 0)
    if not page_size and pr > 0:
        got = int(note.get("records_used") or 0)
        if got >= pr:
            page_size = got // pr
            out["page_size"] = "%d~" % page_size
    basis = note.get("records_available_basis", "")
    out["basis"] = "declared" if basis.startswith("upstream declared") else "counted"
    used = int(note.get("records_used") or 0)

    # The authoritative signal is the UPSTREAM'S OWN total exceeding what was
    # stored — not `more_pages_pending`, which reports whether the walk still
    # held a next-page URL when it stopped and is false at the ceiling.
    # JPN_EGOV_DATA_ORG_ORG_0300 files records_available 325, records_used 300,
    # basis "upstream declared this total", reason "the page ceiling ... stopped
    # the walk" — and more_pages_pending false. Keyed on that flag this tool
    # called it COMPLETE while 25 declared records were being dropped every run.
    if out["basis"] == "declared" and out["available"] != "" \
            and int(out["available"]) > used:
        total = int(out["available"])
        out["verdict"] = "CEILING_DECLARED"
        if page_size:
            need = int(math.ceil(total / float(page_size)))
            out["pages_needed"] = need
            out["suggest_page_max"] = need if need > 20 else ""
            out["note"] = "%d of %d records, %d a page" % (used, total, page_size)
        else:
            out["note"] = ("%d of %d records; no page_size on the row, so the "
                           "page count cannot be computed" % (used, total))
        return out
    if note.get("more_pages_pending"):
        out["verdict"] = "CEILING_UNKNOWN"
        out["note"] = "upstream declares no total; only a full walk can size it"
        return out
    out["verdict"] = "COMPLETE"
    out["note"] = note.get("reason", "")[:80]
    return out


def run_one(a, tmpl, sid, page_size):
    # The scratch database is taken from a POOL, not indexed by the task
    # number. `i % jobs` looks safe because only `jobs` tasks run at once, but
    # the in-flight set is not `jobs` CONSECUTIVE indices: one slow row lets
    # later tasks finish and task i+jobs start while task i is still running,
    # on the same file — the copy truncates a live run's database and the row
    # reads as EMPTY. Caught on JSTAGE_CORPUS_BULK, which measured 300 records
    # alone and EMPTY inside a parallel sweep. A verdict that changes with
    # --jobs is not a measurement, so the pool makes the two agree by
    # construction: a slot is held for the whole run and handed back after.
    slot = a.slots.get()
    try:
        return _run_in_slot(a, tmpl, sid, slot, page_size)
    finally:
        a.slots.put(slot)


def _run_in_slot(a, tmpl, sid, slot, page_size):
    db = os.path.join(a.workdir, "c%d.db" % slot)
    for suf in ("", "-wal", "-shm"):
        try:
            os.unlink(db + suf)
        except OSError:
            pass
    shutil.copyfile(tmpl, db)
    env = dict(os.environ, JO_DB=db, JO_FTS_REBUILD="0",
               JO_SHAPE_NOTICES="0", JO_PAGE_MAX=str(a.probe_pages))
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
    rows, note, fail = read_back(db, sid)
    for suf in ("", "-wal", "-shm"):
        try:
            os.unlink(db + suf)
        except OSError:
            pass
    res = classify(sid, blob, rc, rows, note, fail, secs, timed_out,
                   a.probe_pages, page_size)
    with _lock:
        print("%-34s %-17s records=%-7s pages=%-4s avail=%-9s %s" % (
            res["id"], res["verdict"], res["records"], res["pages_read"],
            res["available"], res["note"]), flush=True)
        if a.out:
            a.out.write("\t".join(str(res.get(k, "")) for k in COLS) + "\n")
            a.out.flush()
    return res


COLS = ("id", "verdict", "records", "pages_read", "available", "basis",
        "page_size", "pages_needed", "suggest_page_max", "secs", "note")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", required=True)
    ap.add_argument("--ids-file", help="one id per line, or 'id<TAB>page_size'")
    ap.add_argument("--only", help="comma-separated ids")
    ap.add_argument("--probe-pages", type=int, default=3,
                    help="JO_PAGE_MAX for the probe run (3)")
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--timeout", type=int, default=120)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--out", help="TSV output path")
    a = ap.parse_args()

    todo = []
    if a.only:
        todo += [(s.strip(), None) for s in a.only.split(",") if s.strip()]
    if a.ids_file:
        for line in open(a.ids_file, encoding="utf-8"):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            todo.append((parts[0].strip(),
                         int(parts[1]) if len(parts) > 1 and
                         parts[1].strip().isdigit() else None))
    if not todo:
        raise SystemExit("nothing selected: pass --only or --ids-file")
    if a.limit:
        todo = todo[:a.limit]

    a.workdir = tempfile.mkdtemp(prefix="jo_ceiling_")
    a.slots = queue.Queue()
    for i in range(max(1, a.jobs)):
        a.slots.put(i)
    tmpl = os.path.join(a.workdir, "template.db")
    warm_template(a.bin, tmpl)
    out_f = open(a.out, "w", encoding="utf-8", newline="\n") if a.out else None
    a.out = out_f
    if out_f:
        out_f.write("\t".join(COLS) + "\n")

    print("%d source(s), %d-page probe, %d job(s)\n" % (
        len(todo), a.probe_pages, a.jobs), flush=True)
    results = []
    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        futs = [ex.submit(run_one, a, tmpl, sid, ps)
                for sid, ps in todo]
        for f in futs:
            results.append(f.result())

    if out_f:
        out_f.close()
    counts = {}
    for r in results:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1
    print("\n== verdicts")
    for k in sorted(counts, key=lambda k: -counts[k]):
        print("  %-18s %5d" % (k, counts[k]))
    need = [r for r in results if r["suggest_page_max"]]
    if need:
        print("\n== %d row(s) whose own upstream total needs more than 20 pages"
              % len(need))
        for r in sorted(need, key=lambda r: -int(r["pages_needed"]))[:40]:
            print("  %-34s %s records at %s a page -> page_max %s" % (
                r["id"], r["available"], r["page_size"], r["pages_needed"]))
    shutil.rmtree(a.workdir, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
