#!/usr/bin/env python3
"""Prove each row EMITS, not merely that its endpoint fetches.

This is the check the rest of the batch tooling does not do, and the difference
is not academic. `tools/probe_hp_batch.py` proves an endpoint answers and that
its body parses into records. It says nothing about whether the *engine* turns
those records into intel_items — and the engine drops a record that has neither
a resolvable title nor a resolvable id:

    lib/hpengine.c:633
    /* A record with neither a title nor an id is shape noise, not a finding. */
    if (!title) { if (!rkey) return; ... }

`hp_pick` falls back to a conventional key list ("name", "title", "id", "uid",
…). An upstream that names its fields differently matches none of them, so
every record is dropped and the run reports:

    [hp:CELESTRAK_GP_STARLINK] emitted 0 of 10926 available across 1 page(s)

Ten thousand records fetched, zero stored, exit code 0. That is precisely the
"looks alive and emits nothing" failure the no-fabrication rule exists to catch,
and it is invisible to a fetch-only probe. The fix is per-row: declare
`title_keys` and `id_keys` naming the fields the upstream actually uses.

The row's own probe URL supplies the pivot entity, recovered by diffing the URL
template against it, so entity-gated rows are exercised too rather than skipped.

SLOW IS NOT BROKEN. The timeout used to be a fixed 180 s with no flag, and a
run that hit it came back `TIMEOUT` — reported in the same list, at the same
weight, as `DROPS_EVERYTHING`. They want opposite responses: one is a row whose
identity or keys are wrong, the other is a bulk file that is simply large.
Batch 19 spent an afternoon on rows that only needed a bigger number. `SLOW`
is now its own verdict, it carries whatever partial `emitted N of M` the engine
had printed before the kill, and `--timeout` moves the line. A SLOW row is
UNMEASURED — it does not fail the run, because this tool did not establish
anything about it.

A RUN THAT MEASURED NOTHING DOES NOT PASS. This tool used to exit 0 unless it
saw DROPS_EVERYTHING, so a manifest whose every row came back UNREGISTERED (the
binary was stale, or the ids were typed wrong), UNPARSEABLE or NO_RUN_LINE
printed a tally and passed — "no row drops everything" because no row ran. Now:

  exit 1  any DROPS_EVERYTHING, UNREGISTERED, UNPARSEABLE or NO_RUN_LINE row,
          any malformed manifest line, or a run in which NO row was measured
  exit 0  otherwise

HTTP_<code> and TRANSPORT_FAIL are the UPSTREAM's answer, not a verdict on the
row's keys, so they are listed in a section of their own rather than mixed into
the tally — a dead endpoint is probe_hp_batch.py's finding to make. SLOW,
NEEDS_KEY, NEEDS_ENTITY, ENTITY_SHAPE and NO_ENTITY_RECOVERED are unmeasured,
and say so.

EVERY RUN GETS ITS OWN SCRATCH DATABASE. Runs used to inherit JO_DB, and with
it unset the binary wrote every audited row into the developer's live
data/japanmap.db. Each run now gets a fresh copy of one warm template DB
(schema applied, migrations run, sources seeded) in a private temp dir that is
removed afterwards — the same arrangement as tools/audit_registry_emit.py.

Usage:
  audit_batch_emit.py MANIFEST... --bin PATH_TO_japanosint [--only ID,ID]
                                  [--jobs N] [--timeout S] [--out results.tsv]
                                  [--workdir DIR]
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
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from manifest import COLS, iter_lines                       # noqa: E402

# Every entity token hp_uses_entity() recognises starts "{q" EXCEPT the raw
# POST form "{Q}" -- see lib/hpengine.c. Without it, entity_of() cannot find
# the token on a row whose only entity reference is {Q}, so this tool invokes
# `--run <id>` with no entity at all and the row comes back NO_ENTITY_RECOVERED
# (unmeasured, not a false pass) instead of actually being exercised.
TOKEN = re.compile(r"\{q[a-zA-Z]*\}|\{Q\}")
EMITTED = re.compile(r"\[hp:([^\]]+)\] emitted (\d+) of (\d+) available")

# When the engine prints no `emitted` line it has still said WHY, and the first
# version of this tool threw that away: 84 rows of batch 19 came back as one
# opaque NO_RUN_LINE covering at least four unrelated situations — a 404, an
# entity pivot invoked with no entity, a body that would not parse, and a source
# that is simply empty right now (the CVE delta feed's `new` array legitimately
# holds 0 entries between publications). Only two of those are defects, and a
# verdict that cannot tell them apart sends you to read 84 rows by hand.
#
# Ordered: the first pattern that matches wins, most specific first.
REASONS = [
    (re.compile(r"\[hp:[^\]]+\] status=(\d+)"),                 "HTTP_%s"),
    (re.compile(r"\[hp:[^\]]+\] transport failure"),            "TRANSPORT_FAIL"),
    (re.compile(r"\[hp:[^\]]+\] non-JSON body"),                "UNPARSEABLE"),
    (re.compile(r"\[hp:[^\]]+\] non-XML body"),                 "UNPARSEABLE"),
    (re.compile(r"\[hp:[^\]]+\] entity shape mismatch"),        "ENTITY_SHAPE"),
    (re.compile(r"\[hp:[^\]]+\] scheduled run but the row needs an entity"),
                                                                "NEEDS_ENTITY"),
    (re.compile(r"\[hp:[^\]]+\] entity yields no value"),        "NEEDS_ENTITY"),
    (re.compile(r"\[hp:[^\]]+\] missing key"),                   "NEEDS_KEY"),
]
# `[sched] <id> run rc=0 records=0 0ms` with no [hp:] line at all: the engine
# returned before making a request. For an on-demand pivot that is rule 3 doing
# its job, and it means THIS TOOL failed to recover an entity, not that the row
# is broken.
SCHED_ZERO = re.compile(r"\[sched\] \S+ run rc=(-?\d+) records=(\d+) (\d+)ms")

# Verdicts that carry a real emitted/available reading.
MEASURED = ("OK", "PARTIAL", "DROPS_EVERYTHING", "NO_RECORDS", "EMPTY_UPSTREAM")
# Verdicts that mean the row never ran as a record source at all.
FATAL_UNRUN = {
    "UNREGISTERED": "the binary does not know this id (stale binary, or the "
                    "row was never generated)",
    "UNPARSEABLE":  "the engine could not parse the body in the row's mode",
    "NO_RUN_LINE":  "the process printed no run line — it died, or the row "
                    "never reached hp_run",
}


def rows(paths):
    """-> (rows, malformed). Malformed lines used to be skipped in silence,
    which made "no DROPS_EVERYTHING" cover rows this tool never ran."""
    out, bad = [], []
    for p in paths:
        for lno, raw, kind, r in iter_lines(p):
            if kind == "row":
                out.append(r)
            elif kind == "bad":
                bad.append(("%s:%d" % (os.path.basename(p), lno),
                            raw.count("|") + 1))
    return out, bad


def entity_of(r):
    """Recover the pivot entity by diffing the URL template against the probe.

    The template and the probe differ only where the token was substituted, so
    the common prefix and suffix bracket the entity exactly.
    """
    url, probe = r["url"], r["probe"]
    m = TOKEN.search(url)
    if not m:
        return None
    pre, suf = url[:m.start()], url[m.end():]
    # the suffix may itself contain further tokens; cut at the next one
    m2 = TOKEN.search(suf)
    if m2:
        suf = suf[:m2.start()]
    if not probe.startswith(pre):
        return None
    rest = probe[len(pre):]
    if suf:
        i = rest.find(suf)
        if i >= 0:
            rest = rest[:i]
    return rest or None


def partial_of(blob):
    """The last `emitted N of M` the engine managed to print. On a killed run
    this is the whole difference between "slow" and "drops everything": a row
    that had emitted 40,000 of 900,000 when the axe fell is working."""
    last = None
    for m in EMITTED.finditer(blob or ""):
        last = m
    return (int(last.group(2)), int(last.group(3))) if last else (0, 0)


def _unlink_db(path):
    for suf in ("", "-wal", "-shm"):
        try:
            os.unlink(path + suf)
        except OSError:
            pass


def warm_template(binpath, path):
    """Boot the binary once against an empty file so every run starts from the
    same migrated, seeded database instead of paying for that per row."""
    _unlink_db(path)
    env = dict(os.environ, JO_DB=path, JO_FTS_REBUILD="0")
    p = subprocess.run([binpath, "--list-sources"], capture_output=True,
                       text=True, env=env, timeout=900)
    if not os.path.exists(path):
        raise SystemExit("could not build a warm template DB at %s\n%s"
                         % (path, (p.stderr or "")[-800:]))
    con = sqlite3.connect(path)        # fold the WAL in before it is copied
    con.execute("PRAGMA wal_checkpoint(TRUNCATE)")
    con.close()
    return path


def run_one(args):
    binpath, r, timeout, tmpl, workdir, slot = args
    ent = entity_of(r)
    cmd = [binpath, "--run", r["id"]] + ([ent] if ent else [])
    # The slot is the row's index, never index % jobs: the pool does not hand
    # work out in queue order, and two runs sharing a file corrupt both.
    db = os.path.join(workdir, "r%d.db" % slot)
    _unlink_db(db)
    shutil.copyfile(tmpl, db)
    try:
        # JO_SHAPE_NOTICES=0: a collector-shape-notice is itself one emitted
        # record; a row that stored only its own notice must still read as 0.
        env = dict(os.environ, JO_SHAPE_NOTICES="0", JO_DB=db,
                   JO_FTS_REBUILD="0")
        p = subprocess.run(cmd, capture_output=True, text=True, env=env,
                           timeout=timeout, stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired as e:
        # Keep whatever it printed before the kill. TimeoutExpired carries the
        # output captured so far; it is bytes or str depending on how the pipe
        # was drained, so normalise both.
        def s(x):
            return x.decode("utf-8", "replace") if isinstance(x, bytes) else (x or "")
        em, av = partial_of(s(e.stdout) + s(e.stderr))
        return (r["id"], "SLOW", em, av, ent or "")
    finally:
        _unlink_db(db)
    blob = (p.stdout or "") + (p.stderr or "")
    if "unknown source" in blob:
        return (r["id"], "UNREGISTERED", 0, 0, ent or "")
    m = EMITTED.search(blob)
    if not m:
        for pat, label in REASONS:
            hit = pat.search(blob)
            if hit:
                return (r["id"],
                        label % hit.group(1) if "%s" in label else label,
                        0, 0, ent or "")
        s = SCHED_ZERO.search(blob)
        if s and s.group(2) == "0":
            # No [hp:] line at all. Either the engine never fetched (0ms — an
            # on-demand pivot with no entity, so the tool is at fault), or it
            # fetched and the upstream had nothing (the emitted line is
            # suppressed when both counts are zero), which is an honest empty.
            if s.group(3) == "0":
                return (r["id"], "NO_ENTITY_RECOVERED", 0, 0, ent or "")
            return (r["id"], "EMPTY_UPSTREAM", 0, 0, ent or "")
        return (r["id"], "NO_RUN_LINE", 0, 0, ent or "")
    emitted, avail = int(m.group(2)), int(m.group(3))
    if avail == 0:
        verdict = "NO_RECORDS"          # fetched nothing this run
    elif emitted == 0:
        verdict = "DROPS_EVERYTHING"    # the defect this tool exists for
    elif emitted < avail:
        verdict = "PARTIAL"
    else:
        verdict = "OK"
    return (r["id"], verdict, emitted, avail, ent or "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifests", nargs="+")
    ap.add_argument("--bin", required=True)
    ap.add_argument("--only")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--timeout", type=int, default=180,
                    help="seconds before a run is killed and called SLOW "
                         "(180). SLOW is unmeasured, not failed — raise this "
                         "and re-run the SLOW rows to measure them.")
    ap.add_argument("--out")
    ap.add_argument("--workdir",
                    help="parent directory for the per-run scratch databases "
                         "(default: the system temp dir)")
    a = ap.parse_args()

    rs, malformed = rows(a.manifests)
    for at, nf in malformed:
        print("%s MALFORMED %d fields, want %d — NOT CHECKED"
              % (at, nf, len(COLS)))
    missing = set()
    if a.only:
        want = set(x.strip() for x in a.only.split(",") if x.strip())
        rs = [r for r in rs if r["id"] in want]
        missing = want - set(r["id"] for r in rs)
        for m in sorted(missing):
            print("%s NOT IN ANY MANIFEST — NOT CHECKED" % m)
    sys.stderr.write("running %d rows through the engine, timeout=%ds\n"
                     % (len(rs), a.timeout))

    a.bin = os.path.abspath(a.bin)
    if not os.access(a.bin, os.X_OK):
        raise SystemExit("not executable: %s" % a.bin)
    workdir = tempfile.mkdtemp(prefix="jo_batch_emit.", dir=a.workdir)
    try:
        tmpl = warm_template(a.bin, os.path.join(workdir, "warm.db"))
        with ThreadPoolExecutor(max_workers=a.jobs) as ex:
            res = list(ex.map(run_one, [(a.bin, r, a.timeout, tmpl, workdir, i)
                                        for i, r in enumerate(rs)]))
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    if a.out:
        with io.open(a.out, "w", encoding="utf-8", newline="\n") as fh:
            fh.write("id\tverdict\temitted\tavailable\tentity\n")
            for x in res:
                fh.write("%s\t%s\t%d\t%d\t%s\n" % x)

    tally = {}
    for x in res:
        tally[x[1]] = tally.get(x[1], 0) + 1
    total_emitted = sum(x[2] for x in res)
    print("\n%s" % "  ".join("%s=%d" % kv for kv in sorted(tally.items())))
    print("records emitted across the run: %s" % f"{total_emitted:,}")
    bad = [x for x in res if x[1] == "DROPS_EVERYTHING"]
    if bad:
        print("\nfetched records but emitted none — needs title_keys/id_keys:")
        for x in sorted(bad, key=lambda y: -y[3])[:40]:
            print("  %-34s %7d available" % (x[0], x[3]))
    slow = [x for x in res if x[1] == "SLOW"]
    if slow:
        print("\nUNMEASURED — killed at --timeout %ds, NOT judged (re-run "
              "these with a larger --timeout):" % a.timeout)
        for x in sorted(slow, key=lambda y: -y[2]):
            print("  %-34s %s" % (x[0], "reached emitted %d of %d" % (x[2], x[3])
                                  if x[3] else "printed no progress at all"))
    upstream = [x for x in res if x[1].startswith("HTTP_")
                or x[1] == "TRANSPORT_FAIL"]
    if upstream:
        print("\nUPSTREAM ERROR — the endpoint did not answer, so this row's "
              "keys were NOT measured (probe_hp_batch.py's finding to make):")
        for x in sorted(upstream, key=lambda y: (y[1], y[0])):
            print("  %-34s %s" % (x[0], x[1]))
    # Rows this tool could not even run. Each is a defect in the batch (or in
    # the binary it was pointed at), never a pass.
    broken = [x for x in res if x[1] in FATAL_UNRUN]
    if broken:
        print("\nNOT RUN — fails the audit:")
        for x in sorted(broken, key=lambda y: (y[1], y[0])):
            print("  %-34s %s  %s" % (x[0], x[1], FATAL_UNRUN[x[1]]))
    measured = [x for x in res if x[1] in MEASURED]
    if malformed:
        print("\nNOT CHECKED: %d malformed manifest line(s)." % len(malformed))
    if not measured:
        print("\nNOTHING WAS MEASURED: %d row(s) ran and none produced an "
              "emitted/available reading. That is not a pass." % len(res))
    if missing:
        print("\nNOT CHECKED: %d --only id(s) named no manifest row." % len(missing))
    return 1 if (bad or broken or malformed or missing or not measured) else 0


if __name__ == "__main__":
    sys.exit(main())
