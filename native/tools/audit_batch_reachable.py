#!/usr/bin/env python3
"""Flag manifest rows that can never run.

`lib/hpengine.c` (hp_run) decides how a row is reached:

    if (!entity) {                       /* a SCHEDULED run carries no entity */
        if (s->interval <= 0) return 0;  /* on-demand pivot, no entity */
        if (hp_uses_entity(s->url) || hp_uses_entity(s->post_body)) { ... }
    }

So a row is reachable in exactly two ways:

  * it references an entity token (`{q}`, `{qd}`, `{qh}`, …) in its URL or POST
    body, and is therefore dispatchable as an entity pivot; or
  * it declares `interval` > 0, and is therefore picked up by the scheduler.

A row with **neither** — a static URL and no interval — is registered, appears
in `/api/status`, and never executes. Nothing fetches it, so it emits nothing,
forever. That is the same silent-nothing failure as the EMPTY_RESULTSET trap:
the source looks present and is structurally incapable of producing a record.

This is easy to introduce by accident, because `hp_source.interval` defaults to
0 and the hpengine convention is that 0 means "on-demand pivot" — which is
correct for a row with a `{q}` template and wrong for a bulk file or a national
register that has no pivot at all.

Usage:
  audit_batch_reachable.py MANIFEST... [--fix SECONDS] [--quiet]
"""
import argparse
import io
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
# THE manifest parser, shared with gen_hp_batch.py and the other auditors.
# This file is the reason it exists: it used to carry its own copy of
# split_opts() and a FIRST-wins opt() lookup, while the generator resolved the
# same duplicated key last-wins — and so reported OSM_API_CHANGESETS_BLACKSEA,
# which runs daily, as a row that can never run. See tools/manifest.py.
from manifest import COLS, parse_opts                      # noqa: E402

# mirrors hp_uses_entity(): every entity token starts "{q"
ENTITY_TOKEN = re.compile(r"\{q")


def rows(path):
    """(lno, raw, row|None, malformed) — a line that LOOKS like a row and is
    not comes back flagged rather than silently indistinguishable from a
    comment, because "skipped" and "checked and clean" are not the same
    answer and this file used to give the second one for both."""
    for lno, line in enumerate(io.open(path, encoding="utf-8"), 1):
        s = line.rstrip("\n")
        if not s.strip() or s.lstrip().startswith("#"):
            yield lno, s, None, False
            continue
        if s.count("|") != len(COLS) - 1:
            yield lno, s, None, True
            continue
        yield lno, s, dict(zip(COLS, s.split("|"))), False


# How often a static source is worth re-fetching. A blanket number would be
# wrong in both directions -- polling a treaty register every 15 minutes is
# waste, and polling a live aircraft feed daily makes it useless -- so the
# cadence is chosen from what the row says it is. First match wins.
CADENCE = [
    (900,   ("live", "realtime", "real-time", "position", "adsb", "ais",
             "mempool", "outage", "shutdown", "nowcast")),
    (3600,  ("threat", "abuse", "blocklist", "phishing", "malware", "c2",
             "botnet", "advisory", "alert", "cve", "vulnerability", "outbreak",
             "surveillance", "seismic", "earthquake", "volcano", "cyclone",
             "flood", "fire", "hazard", "disaster", "weather", "space-weather",
             "grid", "electricity", "market", "price", "recall", "sanction")),
    (21600, ("procurement", "tender", "contract", "filing", "court", "case",
             "news", "media", "foi", "launch", "orbital", "vessel", "flight",
             "trial", "surveillance-report")),
    (86400, ("registry", "register", "identifier", "reference", "corporate",
             "company", "patent", "trademark", "standard", "statistics",
             "statistic", "indicator", "dataset-catalogue", "opendata",
             "allocation", "delegation", "taxonomy", "catalogue", "inventory",
             "facility", "infrastructure", "geo", "osm", "wikidata")),
]
DEFAULT_CADENCE = 21600


def cadence_for(r):
    hay = " ".join((r["tags"], r["record_type"], r["category"])).lower()
    for secs, words in CADENCE:
        if any(w in hay for w in words):
            return secs
    return DEFAULT_CADENCE


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifests", nargs="+")
    ap.add_argument("--fix", action="store_true",
                    help="set a cadence-matched interval on unreachable rows, in place")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()

    total = flagged = fixed = ambiguous = malformed = 0
    chosen = {}
    for p in a.manifests:
        out, changed = [], False
        for lno, raw, r, bad in rows(p):
            if bad:
                malformed += 1
                print("%-34s %-30s %d fields, want %d — NOT CHECKED"
                      % ("%s:%d" % (p.rsplit("/", 1)[-1], lno), "",
                         raw.count("|") + 1, len(COLS)))
                out.append(raw)
                continue
            if r is None:
                out.append(raw)
                continue
            total += 1
            o, dups, _junk = parse_opts(r["opts"])
            if dups:
                # A duplicated key has no value this tool is willing to state,
                # so it does not state one. Reporting "never runs" off a guess
                # is exactly what this file used to do.
                ambiguous += 1
                print("%-34s %-30s duplicate opt %r — UNVERIFIABLE, fix the "
                      "manifest (gen_hp_batch.py rejects it too)"
                      % ("%s:%d" % (p.rsplit("/", 1)[-1], lno), r["id"],
                         dups[0]))
                out.append(raw)
                continue
            pivotable = bool(ENTITY_TOKEN.search(r["url"]) or
                             ENTITY_TOKEN.search(o.get("post_body", "")))
            interval = o.get("interval")
            scheduled = interval is not None and interval.isdigit() and int(interval) > 0
            if pivotable or scheduled:
                out.append(raw)
                continue
            flagged += 1
            if not a.quiet:
                print("%-34s %-30s static URL and no interval — never runs"
                      % ("%s:%d" % (p.rsplit("/", 1)[-1], lno), r["id"]))
            if a.fix:
                secs = cadence_for(r)
                r["opts"] = (r["opts"] + ";" if r["opts"] else "") + "interval=%d" % secs
                chosen[secs] = chosen.get(secs, 0) + 1
                raw = "|".join(r[c] for c in COLS)
                changed = True
                fixed += 1
            out.append(raw)
        if changed:
            io.open(p, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")

    print("\n%d of %d rows can never run%s"
          % (flagged, total, (" — %d given an interval" % fixed) if a.fix else ""))
    if chosen:
        print("    cadence: " + ", ".join("%ds x%d" % (k, v) for k, v in sorted(chosen.items())))
    if ambiguous or malformed:
        print("    NOT CHECKED: %d row(s) with an ambiguous opt, %d malformed "
              "line(s). These are not 'clean' — they are unexamined."
              % (ambiguous, malformed))
    # An unexamined row fails the gate. The alternative is a tool that exits 0
    # over rows it could not read, which is the failure mode this whole batch
    # of fixes is about.
    return 1 if ((flagged and not a.fix) or ambiguous or malformed) else 0


if __name__ == "__main__":
    sys.exit(main())
