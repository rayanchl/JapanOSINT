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

A maintenance pod (collector name beginning with `_`, the convention source.h
documents) is reported POD and NOT run: it writes its own tables and nothing to
intel_items, so asking it whether it emits records is the confusion the
EMITS_NOTHING verdict exists to expose. The first registry-wide sweep reported
eight of them as dead sources.

Exit status: 1 if any selected source came back EMITS_NOTHING, COLLISION,
SINK_MISMATCH, NO_RUN_LINE, UNREADABLE_DB or UNREGISTERED, or if an --only id
is not registered; else 0. SLOW does not fail the run on its own — it is
"unmeasured", not "broken". The others used to be in that bucket too and are
not: SINK_MISMATCH is the two independent readings DISAGREEING (the one thing
this tool exists to refuse to paper over), NO_RUN_LINE is a process that died
before its run returned, UNREADABLE_DB means the verdict rests on the process's
own word, and an id the binary does not know was never run at all. A sweep made
entirely of those used to exit 0.
"""
import atexit
import argparse
import io
import os
import re
import shutil
import json
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
# core/scheduler.c trails `notices=N` after stored=. The run line's `stored` is
# a DISTINCT-UID count over everything the run wrote, notices included; the
# database reading below deliberately excludes them (see db_rows_for). Comparing
# the two without subtracting is comparing different things, and it reported
# SINK_MISMATCH for every healthy row that happened to file one notice.
RUN_NOTICES = re.compile(r"\bnotices=(\d+)")
LIST = re.compile(r"^(\S+)\s+collector=(\S+)\s+interval=(-?\d+)\s*$")
# hpengine's own line, kept because it separates "the upstream had nothing"
# from "we threw away what it gave us" — a distinction `records=0` cannot make.
HP_EMIT = re.compile(r"\[hp:[^\]]+\] emitted (\d+) of (\d+) available")
# The engine's own stderr when it was refused, so the FETCH_FAILED verdict is
# corroborated by the run output and not inferred from a row count alone.
#
# The `[sched]` alternative is the SCHEDULER's own disclosure (2026-10-22), and
# it exists because this pattern only ever matched hpengine. Until the scheduler
# filed a fetch-failure for collectors that file none themselves, a refused
# VJSON or bespoke row had no notice and no `[hp:…]` line, so it fell through to
# `EMITS_NOTHING` — a verdict about a source that FETCHED and stored none, which
# is a different defect wanting a different fix. Measured on this tool's own
# first registry-wide run: 288 of 3,612 rows were that, 18 of 18 sampled being
# vsrc rows, against 34 that disclosed and were all hp16_*. One more instance of
# the rule this repo keeps relearning — when you fix something, look for the
# other copies — and the tool was the third copy.
FETCH_FAILURE = re.compile(r'\[hp:[^\]]+\]\s+(?:status=\d+|transport failure|'
                           r'non-JSON body)'
                           r'|\[sched\]\s+\S+\s+fetch-failure:')
# The engine's three ways of saying "this row was never asked a question".
# The third was added 2026-10-11 after the second-largest misreading in this
# tool's history: an on-demand pivot with no entity returned silently, so 268
# of batch 31's 295 rows came back EMITS_NOTHING — a verdict about sources
# that had never been asked anything. Use audit_batch_emit.py with the
# MANIFEST for pivot rows; it supplies each row's probe entity.
NEEDS_ENTITY = re.compile(r"scheduled run but the row needs an entity|"
                          r"entity yields no value|"
                          r"on-demand pivot and no entity was given")

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


# Record types the ENGINE writes about itself. They are real rows and they are
# meant to be stored -- but they are not records FROM the upstream, and counting
# them as such is how a dead source reads as a healthy one. lib/hpengine.c's
# collector-fetch-failure notice made this urgent: a row whose upstream answers
# 429 now stores exactly one row and the run line says `records=1 stored=1`,
# which without this exclusion is indistinguishable from a source that fetched
# one real record.
ENGINE_NOTICES = (
    "collector-fetch-failure",
    "collector-truncation-notice",
    "collector-shape-notice",
)

# ...AND THAT LIST WAS A COPY OF A RULE, so it fell behind the rule. The engine
# decides what a notice is in ONE place, intel_item_is_notice() in core/intel.c:
# a record_type that starts `collector-` and ends `-notice`. The run line's
# `notices=` counter uses it, this tool's three-name tuple did not, and the two
# disagreed about `collector-status-notice` — the "needs credential" note rule 1
# prescribes for a gated source.
#
# The cost was a FALSE ACCUSATION, which is the worst thing a gate can produce.
# A key-gated row stores exactly one status notice, so the run line reads
# `records=0 stored=1 notices=1` while this tool counted that same row as one
# RECORD, the two numbers disagreed, and the verdict was SINK_MISMATCH: "a
# collector emitting under another id, or a bug". Measured 2026-10-23 on
# OPENEI_UTILITY_RATES and worldbank-debarred-firms, both behaving exactly as
# designed. They are the only two in 9,151 rows, which is precisely why nobody
# would have questioned the verdict.
#
# So the SQL mirrors the engine's rule instead of naming kinds: any
# `collector-%-notice`, plus collector-fetch-failure, which is deliberately NOT
# a `-notice` (a dead source reports records=1 so the run line cannot hide it)
# and still is not a record from the upstream. A notice kind added tomorrow is
# covered without touching this file — the tuple above stays only because the
# epilog and the POD note name those three by hand.
_NOTICE_SQL = ("(record_type LIKE 'collector-%-notice' "
               "OR record_type = 'collector-fetch-failure')")


def db_rows_for(db, sid):
    """-> (real_rows, notice_rows) or (None, None) when the DB is unreadable.

    Unreadable is NOT zero -- see verdict_for()."""
    try:
        con = sqlite3.connect("file:%s?mode=ro" % db, uri=True)
        real = con.execute(
            "SELECT COUNT(*) FROM intel_items WHERE source_id=? "
            "AND (record_type IS NULL OR NOT %s)" % _NOTICE_SQL,
            (sid,)).fetchone()[0]
        notices = con.execute(
            "SELECT COUNT(*) FROM intel_items WHERE source_id=? "
            "AND %s" % _NOTICE_SQL, (sid,)).fetchone()[0]
        con.close()
        return real, notices
    except Exception:
        return None, None


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
                           encoding="utf-8", errors="replace",
                           env=env, timeout=a.timeout,
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

    rows, notices = db_rows_for(db, sid)
    # Read the gated reason HERE, while the scratch database still exists: the
    # unlink below is three lines away, and passing the PATH into verdict_for
    # meant opening a file that had already been deleted — which failed
    # silently and left a gated source reading EMITS_NOTHING, the very verdict
    # this is meant to separate it from.
    gated = status_notice_reason(db, sid)
    failcause = fetch_failure_reason(db, sid)
    for suf in ("", "-wal", "-shm"):
        try:
            os.unlink(db + suf)
        except OSError:
            pass

    if a.keep_logs:
        io.open(os.path.join(a.keep_logs, sid + ".log"), "w",
                encoding="utf-8", newline="\n").write(blob[-40000:])

    res = verdict_for(sid, blob, rc, rows, secs, timed_out, notices, gated,
                      failcause)
    with _lock:
        emit_row(a, res)
    return res


def fetch_failure_reason(db, sid):
    """-> a one-line CAUSE for a refused fetch, read off the engine's own
    collector-fetch-failure record, else None.

    This existed as a fixed sentence for 4,761 of the 55,834 rows the first
    complete registry sweep measured (2026-10-25) -- the largest failure class
    in the tree, and every row of it read "the upstream refused the request or
    served a body that did not parse", which is the verdict restating itself.
    The engine has written the cause as DATA since 2026-10-06 (lib/hpengine.c)
    and 2026-10-22 (core/scheduler.c, every other collector), and since
    2026-10-23 that includes the nine TLS causes named apart -- a missing
    intermediate is fixable and an unrecognized_name is not. Not reading it was
    this tool failing rule 4c against the engine beside it: the disclosure is a
    row in the database, and the gate that reports on it was matching stderr.

    The two writers carry different fields on purpose (the scheduler has no
    http_status to report, because a bespoke collector never measured one), so
    this reads whichever are present rather than assuming a shape."""
    if not db:
        return None
    try:
        con = sqlite3.connect("file:%s?mode=ro" % db, uri=True)
        row = con.execute(
            "SELECT properties FROM intel_items WHERE source_id=? "
            "AND record_type='collector-fetch-failure' LIMIT 1", (sid,)).fetchone()
        con.close()
    except Exception:
        return None
    if not row or not row[0]:
        return None
    try:
        p = json.loads(row[0])
    except Exception:
        return None
    bits = []
    # A status of -1 (or 0) is "nobody measured one", which is a real and
    # deliberate state: core/scheduler.c files no http_status at all because a
    # bespoke collector never reported one, and naming a status nobody measured
    # would be this gate inventing the cause -- the mistake CLAUDE.md records
    # three times over (Archive-It, the Imperva wall, FDSN's nodata=404).
    st = p.get("http_status")
    if isinstance(st, (int, float)) and st > 0:
        bits.append("HTTP %d" % int(st))
    else:
        st = None
    if p.get("body_parsed") is False and st:
        bits.append("body did not parse")
    cause = p.get("tls_cause")
    if cause and cause != "none":
        bits.append("TLS %s" % cause)
        if p.get("tls_detail"):
            bits.append(str(p["tls_detail"])[:60])
    te = p.get("transport_error")
    if te and te not in ("none", "ok"):
        bits.append("transport %s" % te)
    hc, ha = p.get("hosts_contacted"), p.get("hosts_answered")
    if isinstance(hc, (int, float)) and isinstance(ha, (int, float)):
        bits.append("%d of %d hosts answered" % (int(ha), int(hc)))
    nb = p.get("hosts_no_answer")
    if nb:
        bits.append("silent: %s" % str(nb)[:80])
    by = p.get("bytes_fetched")
    if isinstance(by, (int, float)):
        bits.append("%d bytes" % int(by))
    if not bits:
        # No measured field to report, but the record still carries the
        # engine's own sentence about what happened. Prefer it to this tool's
        # generic one: it is the writer's account, and an hpengine row whose
        # transport failed before any status existed has nothing else to give.
        rsn = p.get("outcome") or p.get("reason")
        if rsn:
            bits.append("no status measured; engine: " + str(rsn)[:200])
        else:
            return None
    rem = p.get("remedy") or p.get("tls_remedy")
    out = "; ".join(bits)
    if rem:
        out += " | remedy: " + str(rem)[:200]
    return out


def status_notice_reason(db, sid):
    """-> a one-line reason when this source stored a collector-status-notice
    and nothing else, else None.

    The notice is the engine's "I did not invent a record" record: for a gated
    source it carries status=needs_credential and the env vars that would open
    it, which is the remedy a reader needs. Read from the DATABASE rather than
    matched on stderr, because a notice is data and the run line is not."""
    if not db:
        return None
    try:
        con = sqlite3.connect("file:%s?mode=ro" % db, uri=True)
        row = con.execute(
            "SELECT properties FROM intel_items WHERE source_id=? "
            "AND record_type='collector-status-notice' LIMIT 1", (sid,)).fetchone()
        con.close()
    except Exception:
        return None
    if not row or not row[0]:
        return None
    try:
        p = json.loads(row[0])
    except Exception:
        return "stored a collector-status-notice and no records"
    st = p.get("status") or "status notice"
    envs = p.get("env_vars_required") or []
    if st == "needs_credential":
        return ("waiting for a credential, and said so as data: needs %s "
                "(rule 1's gated note, not a defect)"
                % (", ".join(envs) if envs else "an unnamed credential"))
    return ("stored a collector-status-notice saying %r and no records" % st)


def verdict_for(sid, blob, rc, rows, secs, timed_out, notices=0, gated=None,
                failcause=None):
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
        # The database is the only witness when the process never printed its
        # run line. A row killed after landing 96,244 records is NOT the same
        # finding as one that died having stored nothing, and reporting both as
        # NO_RUN_LINE buried five working d-portal rows in the batch that
        # prompted this.
        if rows:
            return (sid, "SLOW", rc if rc is not None else -1, 0,
                    -1, -1, rows, available, secs,
                    "no [sched] line — the process was killed before the run "
                    "returned, but the database holds %d rows for it, so it "
                    "was working: unmeasured, not failed" % rows)
        n = "no [sched] line — the process died before the run returned"
        return (sid, "NO_RUN_LINE", rc if rc is not None else -1, 0,
                -1, -1, rows if rows is not None else -1, available, secs, n)
    if rows is None:
        return (sid, "UNREADABLE_DB", rc, sched_rc, emitted, stored, -1,
                available, secs,
                "could not read the run's database back; the numbers above are "
                "the process's own word and are NOT confirmed")

    # The run line's notices, so `stored` and `rows` describe the same set.
    run_notices = 0
    if sched:
        nm = RUN_NOTICES.search(blob[sched.end():sched.end() + 240])
        if nm:
            run_notices = int(nm.group(1))
    stored_records = stored - run_notices if stored >= 0 else stored

    # A refused fetch, judged BEFORE the mismatch test: its one stored row is a
    # notice, so stored_records is 0 and rows is 0 — they agree, and what
    # matters is saying WHY nothing came back rather than calling it empty.
    if rows == 0 and notices > 0 and FETCH_FAILURE.search(blob):
        return (sid, "FETCH_FAILED", rc, sched_rc, emitted, stored, rows,
                available, secs,
                failcause or
                "the upstream refused the request or served a body that did "
                "not parse; the engine stored a collector-fetch-failure "
                "notice that carried no cause fields (rule 1)")

    note = stored_note
    if stored_records >= 0 and rows != stored_records and not stored_note:
        return (sid, "SINK_MISMATCH", rc, sched_rc, emitted, stored, rows,
                available, secs,
                "run line says stored=%d (%d of them notices), the database "
                "holds %d records for this source_id — a collector emitting "
                "under another id, or a bug"
                % (stored, run_notices, rows))

    # `rows`, not `stored`, decides the verdict: it is the reading this tool
    # took itself -- and `rows` now EXCLUDES the engine's own notices, so a
    # source that stored nothing but a collector-fetch-failure still reads as
    # storing nothing. Without that split the notice would have made every
    # refused source look like a one-record success.
    if emitted <= 0 and rows == 0:
        if NEEDS_ENTITY.search(blob):
            return (sid, "NEEDS_ENTITY", rc, sched_rc, emitted, stored, rows,
                    available, secs,
                    "on-demand pivot: reachable only with an entity (rule 3)")
        # A row that is WAITING FOR A CREDENTIAL stored exactly what rule 1
        # asks of it: a collector-status-notice naming the env vars it needs,
        # and no invented records. Reporting that as EMITS_NOTHING puts a row
        # doing the right thing in the same column as a row whose keys are
        # wrong, and the remedy is not the same — one needs a key, the other
        # needs title_keys/id_keys. Same reasoning as NEEDS_ENTITY above and
        # POD below: a source behaving exactly as designed does not belong in
        # a defect bucket. (Before this it read SINK_MISMATCH, for the
        # notice-definition drift recorded at ENGINE_NOTICES.)
        if gated:
            return (sid, "GATED", rc, sched_rc, emitted, stored, rows,
                    available, secs, gated)
        # A DATE-WINDOW ROW READ AN HONESTLY EMPTY WINDOW AND ADVANCED, which
        # is this tree's own rule 1 (an upstream's "nothing matched" is not a
        # refusal) and not a defect. Reporting it EMITS_NOTHING is this tool
        # making the confusion that verdict exists to expose, exactly as the
        # POD and GATED corrections above were.
        #
        # It matters at scale because of HOW this gate runs: every source gets
        # a FRESH scratch database, so a window row has no stored position and
        # always reads window 1 of N, starting at its declared window_from.
        # The verdict is therefore about the walk's STARTING POINT, never about
        # the source. Measured 2026-10-25: all 47 KKJ prefecture rows read
        # EMITS_NOTHING for exactly that reason — kkj.go.jp answers their first
        # 45-day window with `<SearchHits>0</SearchHits>` in 144 bytes because
        # Japan's procurement corpus begins at fiscal year 2021 (Aomori: 0 in
        # each of 2021-01..03, then 108, 68, 117 from April), while a window
        # near today holds 218 records and 2024 holds 282. Forty-seven healthy
        # sources in a defect bucket, on the one reading a fresh database can
        # take.
        wm = re.search(r"\[hp:%s\] window (\S+)\.\.(\S+)" % re.escape(sid), blob)
        adv = ("the next run reads the window beginning" in blob)
        if wm and adv and rc == 0:
            return (sid, "WINDOW_EMPTY", rc, sched_rc, emitted, stored, rows,
                    available, secs,
                    "date-window row: the window %s..%s is honestly empty and "
                    "the walk advanced (rule 1). This gate gives every source "
                    "a fresh database, so a window row reads window 1 of N "
                    "from its declared window_from — this says where the walk "
                    "STARTS, not whether the source works. Re-read it with a "
                    "window near today before treating it as a finding"
                    % (wm.group(1), wm.group(2)))
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
                "uid (rule 4b). EITHER the declared id is a dimension, not a "
                "record key (real loss — fix id_keys) OR the upstream genuinely "
                "repeats identical records (correct dedupe — not a bug). This "
                "tool cannot tell which: fetch the url and compare distinct id "
                "values to record count before treating it as a defect"
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
        epilog="verdicts: OK | EMITS_NOTHING | COLLISION | FETCH_FAILED | "
               "SLOW | NEEDS_ENTITY | GATED | POD"
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
    ap.add_argument("--no-pin", action="store_true",
                    help="run the binary in place instead of a pinned copy "
                         "(only to measure the binary as it changes)")
    a = ap.parse_args()

    a.bin = os.path.abspath(a.bin)
    if not os.access(a.bin, os.X_OK):
        raise SystemExit("not executable: %s" % a.bin)
    # PIN THE BINARY. CLAUDE.md says "never rebuild the binary while an audit
    # runs", and that is a discipline nobody can hold for the two hours a 290-
    # row sweep takes: a `make` in another shell replaces this file underneath
    # the sweep and every remaining row comes back shell rc=126, which the
    # tool reports as NO_RUN_LINE — "the process died before the run returned".
    # Measured 2026-10-20: 229 of 290 batch-49 rows read that way, from a
    # rebuild made in the middle for an unrelated row fix. Those verdicts look
    # exactly like a batch of broken sources.
    #
    # So copy it once, run against the copy, and the footgun is gone rather
    # than remembered.
    if not a.no_pin:
        _pin = os.path.join(tempfile.gettempdir(),
                            "jo-audit-bin-%d" % os.getpid())
        shutil.copy2(a.bin, _pin)
        os.chmod(_pin, 0o755)
        atexit.register(lambda: os.path.exists(_pin) and os.unlink(_pin))
        if not a.quiet:
            print("pinned binary: %s (a rebuild cannot disturb this run)"
                  % _pin, flush=True)
        a.bin = _pin
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
        missing = set()
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
        # A MAINTENANCE POD IS NOT A RECORD SOURCE, and running one answers a
        # question it was never asked. `source.h`'s convention is a collector
        # name beginning with `_` (_maint, _enrich, _probe, _vec…), and the
        # embedding backfill, the camera stills lane, the translator and the
        # vector indexers all live there: they write their own tables and
        # nothing to intel_items, by design. The first registry-wide sweep
        # reported eight of them as EMITS_NOTHING, which is this tool making the
        # confusion the EMITS_NOTHING verdict exists to expose. They are
        # reported as POD and not run — listed, so nothing registered hides,
        # and never counted as a finding.
        pods = [s for s in srcs if s[1].startswith("_")]
        srcs = [s for s in srcs if not s[1].startswith("_")]
        if pods:
            sys.stderr.write("pods: %d maintenance lane(s) reported POD, not "
                             "run (collector name starts with '_')\n" % len(pods))
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
            exists = os.path.exists(a.out)
            if exists and not a.resume:
                sys.exit("refusing to overwrite existing --out %r without "
                          "--resume (it already has results); pass --resume "
                          "to append/skip, or choose a new --out path" % a.out)
            new = not exists
            a.out_fh = io.open(a.out, "a" if not new else "w",
                               encoding="utf-8", newline="\n")
            if new:
                a.out_fh.write("\t".join(COLUMNS) + "\n")

        sys.stderr.write("running %d sources, jobs=%d timeout=%ds\n"
                         % (len(ids), a.jobs, a.timeout))
        res = []
        done_ids = already_done(a.out) if (a.resume and a.out) else set()
        for pid, pcoll, pival in pods:
            if pid in done_ids:
                continue
            row = (pid, "POD", 0, 0, -1, -1, -1, -1, 0.0,
                   "maintenance pod (collector %s): writes its own tables, not "
                   "intel_items — not run" % pcoll)
            res.append(row)
            if a.out_fh:
                a.out_fh.write("\t".join(str(x) for x in row) + "\n")
                a.out_fh.flush()
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
    #
    # A FETCH_FAILED row is excluded from the pair as well, and that is not
    # tidiness: its `emitted` is 1 because the engine's own fetch-failure
    # record goes through emit() and is NOT a `collector-*-notice`, while
    # `db_rows_for` excludes that record_type from the row count by design. So
    # the pair reads 1 emitted against 0 stored and the difference tallies as a
    # record "lost to uid collision" — a disclosure counted as a discard. It
    # was invisible while only hpengine disclosed (34 rows of the first 3,612);
    # the scheduler-side disclosure of 2026-10-22 would have turned it into the
    # headline number, since every refused row in the fleet now files one.
    # Counted and named separately instead.
    failed = [r for r in res if r[1] == "FETCH_FAILED"]
    measured = [r for r in res
                if r[4] >= 0 and r[6] >= 0 and r[1] != "FETCH_FAILED"]
    tot_e = sum(r[4] for r in measured)
    tot_s = sum(r[6] for r in measured)
    # Per-source, and floored at zero: a collector that makes its own sink can
    # legitimately land more rows than the scheduler counted emits for, and
    # that surplus must not cancel out somebody else's loss.
    lost = sum(max(0, r[4] - r[6]) for r in measured)
    print("over the %d sources where both numbers are known — records emitted: "
          "%s, rows stored: %s, records lost to uid collision: %s"
          % (len(measured), f"{tot_e:,}", f"{tot_s:,}", f"{lost:,}"))
    if failed:
        print("(%d source(s) stored a fetch-failure disclosure and no records; "
              "that row is a disclosure, not a discard, so they are not in "
              "those totals)" % len(failed))
    skipped = len(res) - len(measured) - len(failed)
    if skipped > 0:
        print("(%d source(s) contributed no comparable pair — SLOW, "
              "NO_RUN_LINE or UNREADABLE_DB — and are excluded from those "
              "totals rather than counted as zero)" % skipped)

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
    broken = [r for r in res if r[1] in ("SINK_MISMATCH", "NO_RUN_LINE",
                                         "UNREADABLE_DB", "UNREGISTERED")]
    if broken:
        print("\nnot measured, and that is a failure — the run died, the two "
              "readings disagree, or the id is unknown: %d" % len(broken))
        for r in broken[:40]:
            print("  %-38s %-14s %s" % (r[0], r[1], r[9]))
    if missing:
        print("\n%d --only id(s) are not registered and were never run: %s"
              % (len(missing), ", ".join(sorted(missing))))
    return 1 if (coll or dead or broken or missing) else 0


if __name__ == "__main__":
    sys.exit(main())
