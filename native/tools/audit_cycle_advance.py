#!/usr/bin/env python3
"""Which cycling rows can never advance?

A row with `page_resume = 1` reads its collection over a cycle: each run
starts where the last one stopped. That holds only if the page AFTER the
ceiling can be fetched. `ENA_PATHOGEN_SARSCOV2_RUNS` found the failure shape
(CLAUDE.md, 2026-10-18): ebi.ac.uk answers HTTP 400 to `offset=1000`, so page
2 is refused, the engine clears the position (bookmarking a refused page would
retry it forever), and the row re-reads the same first slice on every run —
with `rc=0`, a healthy record count and a truncation notice that, since that
day, says "the position was NOT advanced because a page failed". The engine
DISCLOSES it per run; nothing READS the disclosures across the registry. This
is that sweep.

Method: run each cycling row through the real binary under
`JO_PAGE_MAX=<cap>` (default 2 — since 2026-10-22 the operator's brake reaches a
row's own declared ceiling too, jo_page_max_for in lib/pagewalk.c), so a run is page 1 plus the attempt at page
2 rather than a hundred pages, against a fresh copy of a warm template DB, and
read the truncation notice back out of that database. Verdicts, from the
notice's own fields:

  STUCK        page 2 answered a 4xx that is not a wall or a rate limit (400,
               404, 416, 422 …) — the upstream refuses the request shape, so no
               run can ever read past page 1. Re-point the paging (a narrower
               filter, a cursor, a different parameter) — the row's remedy
               field says the same.
  CUT          page 2 failed for a reason that can be transient (transport,
               5xx, 429, 401/403/407/408) — the position was not advanced this
               run either, but a re-run or a hostgate gap may fix it. Re-run
               once before calling it stuck.
  ADVANCES     the notice names the URL the next run resumes at.
  COMPLETES    the walk ended inside <cap> pages: the collection is that small,
               so cycling is a no-op for the row (hptest 40b-ii).
  FETCH_FAILED page 1 itself was refused (collector-fetch-failure stored).
  NEEDS_ENTITY an entity pivot; the scheduler never resumes those anyway.
  SLOW         killed at --timeout; unmeasured, not failed.
  NO_RUN_LINE  the binary printed no run line; read the blob column.

Ids come from --ids-file (one per line) or --from-tables <dir>, which scans the
hp tables for `.page_resume = 1` blocks — the C is the maintained copy, so the
tables are the honest list. Writes a TSV; exits non-zero when nothing was
measured.
"""
import argparse, glob, json, os, re, shutil, sqlite3, subprocess, sys, threading, time
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from audit_registry_emit import warm_template, checkpoint, NEEDS_ENTITY  # noqa: E402

NOTICE = "collector-truncation-notice"
FAILURE = "collector-fetch-failure"
RUN_LINE = re.compile(r"\[sched\] (\S+) run rc=(-?\d+) records=(-?\d+) (\d+)ms stored=(-?\d+)")
# Statuses a later page can answer and still be a transient reading rather than
# a property of the endpoint: lib/hpengine.c's remedy text draws the same line.
TRANSIENT = {0, 401, 403, 407, 408, 429}
_opener = re.compile(r'\{\s*\.id\s*=\s*"([^"]+)"')


def ids_from_tables(d):
    out = []
    for f in sorted(glob.glob(os.path.join(d, "**", "*.c"), recursive=True)):
        t = open(f, encoding="utf-8", errors="replace").read()
        if ".page_resume" not in t:
            continue
        starts = list(_opener.finditer(t))
        for i, m in enumerate(starts):
            end = starts[i + 1].start() if i + 1 < len(starts) else len(t)
            if re.search(r"\.page_resume\s*=\s*1", t[m.end():end]):
                out.append(m.group(1))
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
        con.close()
        return (rows, (json.loads(note[0]) if note and note[0] else None),
                (fail[0] if fail and fail[0] else None))
    except Exception:
        return None, None, None


def verdict(rows, note, fail, blob, timed_out):
    if timed_out:
        return "SLOW", ""
    if NEEDS_ENTITY.search(blob):
        return "NEEDS_ENTITY", ""
    if not RUN_LINE.search(blob):
        return "NO_RUN_LINE", ""
    if fail and not rows:
        return "FETCH_FAILED", fail
    if not note:
        return "COMPLETES", "no truncation notice: the walk ended inside the cap"
    nxt = str(note.get("next_run_resumes_at", ""))
    if "failed_page" in note:
        st = int(note.get("failed_page_status") or 0)
        why = "page %s answered %s: %s" % (note.get("failed_page"), st,
                                           note.get("failed_page_url", ""))
        return ("CUT" if st in TRANSIENT or st >= 500 else "STUCK"), why
    if nxt.startswith("http"):
        return "ADVANCES", nxt
    return "COMPLETES", nxt


def run_one(a, tmpl, sid, slot):
    db = os.path.join(a.workdir, "c%d.db" % slot)
    for suf in ("", "-wal", "-shm"):
        try:
            os.unlink(db + suf)
        except OSError:
            pass
    shutil.copyfile(tmpl, db)
    env = dict(os.environ, JO_DB=db, JO_FTS_REBUILD="0", JO_SHAPE_NOTICES="0",
               JO_PAGE_MAX=str(a.cap))
    t0 = time.time(); timed_out = False; blob = ""
    try:
        p = subprocess.run([a.bin, "--run", sid], capture_output=True,
                           encoding="utf-8", errors="replace", env=env,
                           timeout=a.timeout, stdin=subprocess.DEVNULL)
        blob = (p.stdout or "") + (p.stderr or "")
    except subprocess.TimeoutExpired as e:
        timed_out = True
        blob = "".join((x.decode("utf-8", "replace") if isinstance(x, bytes) else (x or ""))
                       for x in (e.stdout, e.stderr))
    rows, note, fail = read_back(db, sid)
    v, why = verdict(rows, note, fail, blob, timed_out)
    m = RUN_LINE.search(blob)
    emitted = m.group(3) if m else ""
    stored = m.group(5) if m else ""
    avail = note.get("records_available", "") if note else ""
    return [sid, v, str(emitted), str(stored), str(rows if rows is not None else ""),
            str(avail), "%.1f" % (time.time() - t0), why.replace("\t", " ").replace("\n", " ")]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", default="./bin/japanosint")
    ap.add_argument("--ids-file")
    ap.add_argument("--from-tables", help="scan this collectors dir for page_resume rows")
    ap.add_argument("--cap", type=int, default=2, help="JO_PAGE_MAX for each run (default 2)")
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--timeout", type=int, default=220)
    ap.add_argument("--workdir", default=".run/cycle-advance")
    ap.add_argument("--template")
    ap.add_argument("--out", default="cycle-advance.tsv")
    ap.add_argument("--resume", action="store_true", help="skip ids already in --out")
    a = ap.parse_args()
    ids = []
    if a.ids_file:
        ids += [l.strip() for l in open(a.ids_file) if l.strip() and not l.startswith("#")]
    if a.from_tables:
        ids += ids_from_tables(a.from_tables)
    ids = list(dict.fromkeys(ids))
    if not ids:
        raise SystemExit("no ids: pass --ids-file or --from-tables")
    os.makedirs(a.workdir, exist_ok=True)
    done = set()
    if a.resume and os.path.exists(a.out):
        for l in open(a.out, encoding="utf-8"):
            if l and not l.startswith("id\t"):
                done.add(l.split("\t", 1)[0])
    todo = [i for i in ids if i not in done]
    tmpl = a.template or warm_template(a.bin, os.path.join(a.workdir, "template.db"))
    lock = threading.Lock()
    header = "id\tverdict\temitted\tstored\tdb_rows\tavailable\tsecs\tdetail\n"
    mode = "a" if (a.resume and done) else "w"
    f = open(a.out, mode, encoding="utf-8")
    if mode == "w":
        f.write(header)
    counts = {}
    slots = list(range(a.jobs)); slot_lock = threading.Lock()

    def job(sid):
        with slot_lock:
            s = slots.pop()
        try:
            r = run_one(a, tmpl, sid, s)
        finally:
            with slot_lock:
                slots.append(s)
        with lock:
            f.write("\t".join(r) + "\n"); f.flush()
            counts[r[1]] = counts.get(r[1], 0) + 1
            n = sum(counts.values())
            if n % 25 == 0 or r[1] in ("STUCK",):
                print("[%d/%d] %s %s %s" % (n, len(todo), r[0], r[1], r[7][:100]), flush=True)
    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        list(ex.map(job, todo))
    f.close()
    print("measured %d: %s" % (sum(counts.values()),
          ", ".join("%s %d" % kv for kv in sorted(counts.items(), key=lambda x: -x[1]))))
    if not counts:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
