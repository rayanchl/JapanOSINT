#!/usr/bin/env python3
"""Run REGISTERED sources through the real binary and read back what they left.

WHY THIS EXISTS, and why `audit_batch_emit.py` was not enough.

House rule 4 ("fetching is not emitting") has been enforced since batch 18 — by
`tools/audit_batch_emit.py`, which reads rows out of a `docs/candidate-sources-
batch<N>.*.txt` manifest. That is the whole problem: the generated `vsrc*` fleet
in `collectors/feed/generated/` has NO manifest of that shape. It was verified
by fetch-probe alone, and rule 4 was never applied to it. When a sweep finally
ran the scheduled registry through the binary, **260 of 1,197 sources emitted
zero records** — every one of them registered, scheduled, appearing in
/api/status, and producing nothing on every run, forever.

This tool takes its source list from the BINARY (`--list-sources`), not from a
manifest, so nothing registered can hide from it.

It also measures the second number, house rule 4b's:

    emitted  the scheduler's own `records=N` — one per sink->emit() call
    stored   the DISTINCT rows those calls left behind

`records=N` alone cannot see a source whose records key onto each other in the
sink's uid: ECDC_RESPIRATORY called emit 12,648 times and left 31 rows, rc=0,
run reported successful. core/intel.c counts the distinct uids and the run line
now states both.

TWO INDEPENDENT READINGS, DELIBERATELY. `stored=` is what the process SAYS it
stored; `db_rows` is `SELECT COUNT(*) FROM intel_items WHERE source_id=?` on
that run's own database afterwards. They are produced by different code and
they must agree. When they do not, the row is flagged SINK_MISMATCH rather than
quietly trusted — a checker that believes a single self-report is exactly the
kind of check that let 260 silent sources through in the first place. (A
legitimate cause exists: a collector that makes its OWN sink under a different
source_id. That is worth seeing, which is why it is a flag and not a crash.)

EVERY RUN GETS A FRESH COPY OF A WARMED TEMPLATE DB. Two reasons, both learned:
  * `stored` and `db_rows` are meaningless against a shared database — you
    cannot tell this run's rows from the last one's, and the whole point is
    per-run accounting;
  * the host disk is not large. One source in an earlier sweep grew a 14 GB
    SQLite WAL in two minutes and wedged the machine. `--file-limit` caps each
    run's file size, so a runaway dies as one flagged row instead of taking
    the sweep and the host with it.

Usage:
  audit_registry_emit.py --bin ./bin/japanosint --scheduled --out sweep.tsv
  audit_registry_emit.py --bin ./bin/japanosint --match '^vsrc19' --jobs 8
  audit_registry_emit.py --bin ./bin/japanosint --ids-file ids.txt --resume

Exit status: 1 if any selected source came back EMITS_NOTHING or COLLISION,
else 0. SLOW and NO_RUN_LINE do not fail the run on their own — they are
"unmeasured", not "broken", and saying otherwise would be reporting a verdict
this tool did not establish.
"""
import argparse
import io
import os
import re
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import threading
import time
from concurrent.futures import ThreadPoolExecutor

# `[sched] <id> run rc=<rc> records=<n> <ms>ms stored=<s>` — `stored=` is
# appended after the duration (core/scheduler.c explains why it is not next to
# records=), and is absent on a binary built before house rule 4b, so it is
# matched separately rather than as part of this pattern.
SCHED = re.compile(r"\[sched\] (\S+) run rc=(-?\d+) records=(-?\d+) (\d+)ms")
STORED = re.compile(r"\bstored=(\?|>=\d+|\d+)")
LIST = re.compile(r"^(\S+)\s+collector=(\S+)\s+interval=(-?\d+)\s*$")
# hpengine's own line, kept because it separates "the upstream had nothing"
# from "we threw away what it gave us" — a distinction `records=0` cannot make.
HP_EMIT = re.compile(r"\[hp:[^\]]+\] emitted (\d+) of (\d+) available")
NEEDS_ENTITY = re.compile(r"scheduled run but the row needs an entity|"
                          r"entity yields no value")

_lock = threading.Lock()

COLUMNS = ["id", "verdict", "shell_rc", "sched_rc", "emitted", "stored",
           "db_rows", "available", "secs", "note"]


def list_sources(binpath, db):
    """Ask the BINARY what is registered. -> [(id, collector, interval)]"""
    env = dict(os.environ, JO_DB=db, JO_FTS_REBUILD="0")
    p = subprocess.run([binpath, "--list-sources"], capture_output=True,
                       text=True, env=env, timeout=600)
    out = []
    for line in (p.stdout or "").splitlines():
        m = LIST.match(line.rstrip())
        if m:
            out.append((m.group(1), m.group(2), int(m.group(3))))
    if not out:
        raise SystemExit("--list-sources produced no parseable rows; refusing "
                         "to report on a source list this tool did not get.\n"
                         + (p.stderr or "")[-800:])
    return out


def checkpoint(path):
    """Fold the WAL back into the main file. Without this a template is copied
    without whatever its last writer left in the -wal, and every run starts
    from a database that is missing the migrations that were just applied."""
    con = sqlite3.connect(path)
    con.execute("PRAGMA wal_checkpoint(TRUNCATE)")
    con.close()


def warm_template(binpath, path):
    """A database with the schema applied, every migration run and the sources
    table seeded — the state a real deployment is in before a collector runs.

    Built by booting the binary once against an empty file. Doing it here, once,
    rather than per source is worth ~1.5 s x N and, more importantly, makes
    every run start from an IDENTICAL database, so a difference between two
    sources is a difference between the sources."""
    for suf in ("", "-wal", "-shm"):
        try:
            os.unlink(path + suf)
        except OSError:
            pass
    env = dict(os.environ, JO_DB=path, JO_FTS_REBUILD="0")
    p = subprocess.run([binpath, "--list-sources"], capture_output=True,
                       text=True, env=env, timeout=900)
    if not os.path.exists(path):
        raise SystemExit("could not build a warm template DB at %s\n%s"
                         % (path, (p.stderr or "")[-800:]))
    checkpoint(path)
    return path


def build_cmd(a, sid):
    """The per-source command, with its file-size ceiling.

    The ceiling is imposed by `ulimit` in a shell that then `exec`s the binary,
    NOT by subprocess's preexec_fn: preexec_fn is documented as unsafe in the
    presence of threads, and this tool is a thread pool. `exec` means the shell
    is REPLACED, so the pid subprocess waits on and kills at --timeout is the
    binary itself and nothing is left orphaned holding a 14 GB WAL."""
    if a.file_limit_kb and os.path.exists("/bin/sh"):
        return ["/bin/sh", "-c", 'ulimit -f %d; exec "$0" --run "$1"'
                % a.file_limit_kb, a.bin, sid]
    return [a.bin, "--run", sid]


def db_rows_for(db, sid):
    try:
        con = sqlite3.connect("file:%s?mode=ro" % db, uri=True)
        n = con.execute("SELECT COUNT(*) FROM intel_items WHERE source_id=?",
                        (sid,)).fetchone()[0]
        con.close()
        return n
    except Exception:
        return None            # unreadable is NOT zero — see verdict_for()


def run_one(a, tmpl, sid, slot):
    db = os.path.join(a.workdir, "r%d.db" % slot)
    for suf in ("", "-wal", "-shm"):
        try:
            os.unlink(db + suf)
        except OSError:
            pass
    shutil.copyfile(tmpl, db)
    # JO_SHAPE_NOTICES=0: a collector-shape-notice is one emitted record, and
    # this tool judges DROPS_EVERYTHING by that count — a row that stored only
    # its own notice must still read as 0.
    env = dict(os.environ, JO_DB=db, JO_FTS_REBUILD="0", JO_SHAPE_NOTICES="0")
    t0 = time.time()
    timed_out = False
    blob = ""
    rc = None
    try:
        p = subprocess.run(build_cmd(a, sid), capture_output=True,
                           text=True, env=env, timeout=a.timeout,
                           stdin=subprocess.DEVNULL)
        rc = p.returncode
        blob = (p.stdout or "") + (p.stderr or "")
    except subprocess.TimeoutExpired as e:
        timed_out = True
        # The partial output is the point: a source killed at the cap has
        # usually already printed its per-page progress, and that is the
        # difference between "slow" and "broken".
        blob = ((e.stdout or b"").decode("utf-8", "replace") if
                isinstance(e.stdout, bytes) else (e.stdout or "")) + \
               ((e.stderr or b"").decode("utf-8", "replace") if
                isinstance(e.stderr, bytes) else (e.stderr or ""))
    secs = round(time.time() - t0, 1)

    rows = db_rows_for(db, sid)
    for suf in ("", "-wal", "-shm"):
        try:
            os.unlink(db + suf)
        except OSError:
            pass

    if a.keep_logs:
        io.open(os.path.join(a.keep_logs, sid + ".log"), "w",
                encoding="utf-8", newline="\n").write(blob[-40000:])

    res = verdict_for(sid, blob, rc, rows, secs, timed_out)
    with _lock:
        emit_row(a, res)
    return res


def verdict_for(sid, blob, rc, rows, secs, timed_out):
    sched = None
    for m in SCHED.finditer(blob):
        if m.group(1) == sid:
            sched = m
    hp = None
    for m in HP_EMIT.finditer(blob):
        hp = m
    available = int(hp.group(2)) if hp else -1

    emitted = int(sched.group(3)) if sched else -1
    sched_rc = int(sched.group(2)) if sched else 0
    stored, stored_note = -1, ""
    if sched:
        sm = STORED.search(blob[sched.end():sched.end() + 240])
        if sm:
            v = sm.group(1)
            if v == "?":
                stored_note = "binary reported stored=? (not an intel sink)"
            elif v.startswith(">="):
                stored, stored_note = int(v[2:]), "stored is a FLOOR (counter capped)"
            else:
                stored = int(v)
        else:
            stored_note = ("no stored= in the run line — binary predates house "
                           "rule 4b; rebuild before trusting this column")

    if timed_out:
        # A distinct verdict, carrying whatever it got to. A fixed cap reported
        # as a defect is how batch 19 spent an afternoon on sources that were
        # merely large.
        note = "hit --timeout %ss" % secs
        if emitted >= 0:
            note += "; partial emitted=%d" % emitted
        elif available >= 0:
            note += "; reached %d available" % available
        # The rows it managed to land before the kill are the useful partial:
        # the engine prints `emitted N of M` only when a run COMPLETES, so on a
        # killed hpengine row the database is the only witness that it was
        # working.
        if rows:
            note += "; %d rows already stored" % rows
        return (sid, "SLOW", rc if rc is not None else -1, sched_rc,
                emitted, stored, rows if rows is not None else -1,
                available, secs, note)

    if "unknown source" in blob:
        return (sid, "UNREGISTERED", rc, 0, -1, -1, -1, -1, secs,
                "the binary does not know this id")
    if sched is None:
        n = "no [sched] line — the process died before the run returned"
        return (sid, "NO_RUN_LINE", rc if rc is not None else -1, 0,
                -1, -1, rows if rows is not None else -1, available, secs, n)
    if rows is None:
        return (sid, "UNREADABLE_DB", rc, sched_rc, emitted, stored, -1,
                available, secs,
                "could not read the run's database back; the numbers above are "
                "the process's own word and are NOT confirmed")

    note = stored_note
    if stored >= 0 and rows != stored and not stored_note:
        return (sid, "SINK_MISMATCH", rc, sched_rc, emitted, stored, rows,
                available, secs,
                "run line says stored=%d, the database holds %d rows for this "
                "source_id — a collector emitting under another id, or a bug"
                % (stored, rows))

    # `rows`, not `stored`, decides the verdict: it is the reading this tool
    # took itself.
    if emitted <= 0 and rows == 0:
        if NEEDS_ENTITY.search(blob):
            return (sid, "NEEDS_ENTITY", rc, sched_rc, emitted, stored, rows,
                    available, secs,
                    "on-demand pivot: reachable only with an entity (rule 3)")
        if available > 0:
            return (sid, "EMITS_NOTHING", rc, sched_rc, emitted, stored, rows,
                    available, secs,
                    "fetched %d records and stored none — needs title_keys/"
                    "id_keys, or is not a record source" % available)
        return (sid, "EMITS_NOTHING", rc, sched_rc, emitted, stored, rows,
                available, secs, note or "no records reached intel_items")
    if emitted > 0 and rows < emitted:
        return (sid, "COLLISION", rc, sched_rc, emitted, stored, rows,
                available, secs,
                "%d of %d emitted records collapsed onto an already-written "
                "uid (rule 4b): the row's identity is wrong"
                % (emitted - rows, emitted))
    return (sid, "OK", rc, sched_rc, emitted, stored, rows, available, secs,
            note)


def emit_row(a, res):
    line = "\t".join(str(x) for x in res)
    if a.out_fh:
        a.out_fh.write(line + "\n")
        a.out_fh.flush()       # a 40-minute sweep must survive being killed
    if not a.quiet:
        sys.stderr.write("%-38s %-14s emitted=%-8s stored=%-8s db=%-8s %s\n"
                         % (res[0], res[1], res[4], res[5], res[6], res[9]))


def already_done(path):
    done = set()
    if path and os.path.exists(path):
        for line in io.open(path, encoding="utf-8"):
            f = line.split("\t")
            if len(f) >= 2 and f[0] != "id":
                done.add(f[0])
    return done


def main():
    ap = argparse.ArgumentParser(
        description="Prove registered sources EMIT and STORE, not merely fetch.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="verdicts: OK | EMITS_NOTHING | COLLISION | SLOW | NEEDS_ENTITY"
               " | SINK_MISMATCH | UNREADABLE_DB | NO_RUN_LINE | UNREGISTERED")
    ap.add_argument("--bin", required=True, help="path to the japanosint binary")
    sel = ap.add_argument_group("source selection (combined with AND)")
    sel.add_argument("--all", action="store_true",
                     help="every registered source (default if no selector)")
    sel.add_argument("--scheduled", action="store_true",
                     help="only sources with update_interval_sec > 0")
    sel.add_argument("--only", help="comma-separated ids")
    sel.add_argument("--ids-file", help="file of ids, one per line")
    sel.add_argument("--match", help="regex the source id must match")
    sel.add_argument("--collector", help="regex the collector name must match")
    sel.add_argument("--limit", type=int, default=0,
                     help="stop after N selected sources (0 = no limit)")
    ap.add_argument("--jobs", type=int, default=6, help="parallel runs (6)")
    ap.add_argument("--timeout", type=int, default=220,
                    help="seconds before a run is killed and called SLOW (220)")
    ap.add_argument("--file-limit", type=int, default=1024,
                    help="MB ceiling on any file a run writes; a runaway WAL "
                         "dies here instead of on the host volume (1024)")
    ap.add_argument("--workdir",
                    help="where per-run databases live (default /dev/shm, else "
                         "a temp dir). Must have room for --jobs of them.")
    ap.add_argument("--template",
                    help="reuse this warm template DB instead of building one")
    ap.add_argument("--keep-logs", help="directory to write each run's output to")
    ap.add_argument("--out", help="TSV results file (appended, header written once)")
    ap.add_argument("--resume", action="store_true",
                    help="skip ids already present in --out")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()

    a.bin = os.path.abspath(a.bin)
    if not os.access(a.bin, os.X_OK):
        raise SystemExit("not executable: %s" % a.bin)
    a.file_limit_kb = a.file_limit * 1024      # `ulimit -f` counts 1K blocks

    own_tmp = None
    if not a.workdir:
        a.workdir = "/dev/shm" if os.path.isdir("/dev/shm") else tempfile.gettempdir()
    # Unique per invocation. A shared name once let a short test sweep's
    # cleanup delete the directory a long sweep was still allocating databases
    # in — three sources came back unmeasurable for a reason that had nothing
    # to do with the sources.
    a.workdir = os.path.join(a.workdir, "jo_emit_sweep.%d" % os.getpid())
    os.makedirs(a.workdir, exist_ok=True)
    own_tmp = a.workdir
    if a.keep_logs:
        os.makedirs(a.keep_logs, exist_ok=True)

    try:
        tmpl = a.template or warm_template(a.bin,
                                           os.path.join(a.workdir, "warm.db"))
        # List against a COPY: --list-sources opens the database and re-seeds
        # the sources table, which would leave a -wal beside the template that
        # copyfile() does not carry.
        listdb = os.path.join(a.workdir, "list.db")
        shutil.copyfile(tmpl, listdb)
        srcs = list_sources(a.bin, listdb)
        for suf in ("", "-wal", "-shm"):
            try:
                os.unlink(listdb + suf)
            except OSError:
                pass
        sys.stderr.write("registry: %d sources\n" % len(srcs))

        if a.scheduled:
            srcs = [s for s in srcs if s[2] > 0]
        if a.match:
            rx = re.compile(a.match)
            srcs = [s for s in srcs if rx.search(s[0])]
        if a.collector:
            rx = re.compile(a.collector)
            srcs = [s for s in srcs if rx.search(s[1])]
        if a.only:
            want = set(x.strip() for x in a.only.split(",") if x.strip())
            srcs = [s for s in srcs if s[0] in want]
            missing = want - set(s[0] for s in srcs)
            for m in sorted(missing):
                sys.stderr.write("NOT REGISTERED: %s\n" % m)
        if a.ids_file:
            want = set(x.strip() for x in io.open(a.ids_file, encoding="utf-8")
                       if x.strip() and not x.startswith("#"))
            srcs = [s for s in srcs if s[0] in want]
        ids = [s[0] for s in srcs]
        if a.resume and a.out:
            done = already_done(a.out)
            skipped = len([i for i in ids if i in done])
            ids = [i for i in ids if i not in done]
            sys.stderr.write("resume: %d already in %s\n" % (skipped, a.out))
        if a.limit:
            ids = ids[:a.limit]

        a.out_fh = None
        if a.out:
            new = not (a.resume and os.path.exists(a.out))
            a.out_fh = io.open(a.out, "a" if not new else "w",
                               encoding="utf-8", newline="\n")
            if new:
                a.out_fh.write("\t".join(COLUMNS) + "\n")

        sys.stderr.write("running %d sources, jobs=%d timeout=%ds\n"
                         % (len(ids), a.jobs, a.timeout))
        res = []
        with ThreadPoolExecutor(max_workers=a.jobs) as ex:
            # The slot is the source's INDEX, not index % jobs: two tasks
            # sharing a slot can overlap (the pool does not hand work out in
            # the order it was queued), and two runs writing one database file
            # makes both `stored` readings garbage.
            futs = [ex.submit(run_one, a, tmpl, sid, i)
                    for i, sid in enumerate(ids)]
            for f in futs:
                res.append(f.result())
        if a.out_fh:
            a.out_fh.close()
    finally:
        if own_tmp:
            shutil.rmtree(own_tmp, ignore_errors=True)

    tally = {}
    for r in res:
        tally[r[1]] = tally.get(r[1], 0) + 1
    print("\n%s" % "  ".join("%s=%d" % kv for kv in sorted(tally.items())))
    # Totals over the sources where BOTH numbers are known. Summing `emitted`
    # over one set of rows and `stored` over another produced a nonsense
    # difference the first time this ran: a SLOW row contributes 33,249 stored
    # rows and no emit count, which made the fleet look like it stored 25,582
    # records more than it emitted.
    measured = [r for r in res if r[4] >= 0 and r[6] >= 0]
    tot_e = sum(r[4] for r in measured)
    tot_s = sum(r[6] for r in measured)
    # Per-source, and floored at zero: a collector that makes its own sink can
    # legitimately land more rows than the scheduler counted emits for, and
    # that surplus must not cancel out somebody else's loss.
    lost = sum(max(0, r[4] - r[6]) for r in measured)
    print("over the %d sources where both numbers are known — records emitted: "
          "%s, rows stored: %s, records lost to uid collision: %s"
          % (len(measured), f"{tot_e:,}", f"{tot_s:,}", f"{lost:,}"))
    if len(measured) != len(res):
        print("(%d source(s) contributed no comparable pair — SLOW, "
              "NO_RUN_LINE or UNREADABLE_DB — and are excluded from those "
              "totals rather than counted as zero)" % (len(res) - len(measured)))

    coll = sorted([r for r in res if r[1] == "COLLISION"],
                  key=lambda r: r[6] - r[4])
    if coll:
        print("\nrecords discarded inside our own sink (rule 4b):")
        for r in coll[:40]:
            print("  %-38s emitted %-8d stored %-8d  lost %d"
                  % (r[0], r[4], r[6], r[4] - r[6]))
    dead = [r for r in res if r[1] == "EMITS_NOTHING"]
    if dead:
        print("\nstored nothing (rule 4), worst first by records fetched:")
        for r in sorted(dead, key=lambda x: -x[7])[:40]:
            print("  %-38s %s" % (r[0], r[9]))
    slow = [r for r in res if r[1] == "SLOW"]
    if slow:
        print("\nunmeasured — hit --timeout, NOT judged: %d "
              "(re-run these with a larger --timeout)" % len(slow))
        for r in slow[:20]:
            print("  %-38s %s" % (r[0], r[9]))
    return 1 if (coll or dead) else 0


if __name__ == "__main__":
    sys.exit(main())
