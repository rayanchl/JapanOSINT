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

Usage:
  audit_batch_emit.py MANIFEST... --bin PATH_TO_japanosint [--only ID,ID]
                                  [--jobs N] [--out results.tsv]
"""
import argparse
import io
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

COLS = ["id", "mode", "want", "category", "record_type", "tags", "portal",
        "name", "name_ja", "url", "probe", "description", "opts"]

TOKEN = re.compile(r"\{q[a-zA-Z]*\}")
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


def rows(paths):
    out = []
    for p in paths:
        for line in io.open(p, encoding="utf-8"):
            s = line.rstrip("\n")
            if s.startswith("#") or s.count("|") != 12:
                continue
            out.append(dict(zip(COLS, s.split("|"))))
    return out


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


def run_one(args):
    binpath, r = args
    ent = entity_of(r)
    cmd = [binpath, "--run", r["id"]] + ([ent] if ent else [])
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    except subprocess.TimeoutExpired:
        return (r["id"], "TIMEOUT", 0, 0, ent or "")
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
    ap.add_argument("--out")
    a = ap.parse_args()

    rs = rows(a.manifests)
    if a.only:
        want = set(x.strip() for x in a.only.split(","))
        rs = [r for r in rs if r["id"] in want]
    sys.stderr.write("running %d rows through the engine\n" % len(rs))

    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        res = list(ex.map(run_one, [(a.bin, r) for r in rs]))

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
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
