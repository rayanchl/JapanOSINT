#!/usr/bin/env python3
"""A SEVENTH discard shape: a record limit the row asks the UPSTREAM for.

WHY THIS EXISTS, and why it is not in `make audit-sources`.

tests/audit_source_exhaustiveness.py has six checks, and every one of them
looks for a bound written in C: a `#define …MAX` (`record-cap`), a `break`
(`loop-break`), a loop condition (`loop-cap`), a fixed `seen[]` ring
(`dedupe-ring`), a first-element read (`first-only`, `limit-one`) or a single
request where paging exists (`single-page`). A limit spelled into a URL —

    snprintf(url, sizeof url,
             "https://index.golang.org/index?since=%s&limit=200", since);

is none of those, so the tree read "0 findings" while `go-module-index` asked
for a six-hour window and stored 200 of the 22,248 versions that window
actually holds. 22,048 Go module publications dropped per hourly run, with
rc=0, records=200 and stored=200 — both of this repo's "the numbers look fine"
traps at once (CLAUDE.md rules 4 and 4b). Measured 2026-10-05; that row is now
retired in favour of GO_MODULE_INDEX, which pages with
`next_path=$last.Timestamp`.

So the shape is real and it is invisible to the gate. This file is separate
from that gate for a reason that matters more than tidiness:

**A URL limit is not automatically a discard, and this script cannot tell the
difference.** Rule 2 lets a consumer bound its own VIEW; what it forbids is
silent slicing of a collection that has an end. Those are different cases and
they look identical in C:

  * A relevance-ranked SEARCH pivot — "give me the top 15 matches for this
    entity" — has no complete answer to discard. `&rows=15` there is the
    question, not a truncation.
  * A SCHEDULED read of a collection that does have an end — a CKAN catalogue,
    a STAC item search, a module index — stores `limit` of however many exist,
    forever, and that is the violation.

Deciding which a row is needs the upstream, so running this script is the
START of the work and not the result of it. Wiring it into the gate would
instead add a hundred findings to a tree whose whole invariant is that a new
finding is a regression.

WHAT IT REPORTS, and what the number means.

Default scope is the narrow, dangerous one: hand-written collectors under
collectors/sources/ and collectors/pod/ that register at least one SCHEDULED
source, in a function that shows no paging construct and does not touch the
entity. On 2026-10-05 that is **103 sites across 78 files** — a CANDIDATE
list, not a defect list. None of the 103 has been measured; the Go index was
the 104th and is fixed, which is the only reason any of this is known.

`--all` drops both filters, so every `limit`-bearing URL in those directories
is listed (215 sites, 138 files) — that folds the search pivots back in, and
most of them are legitimate.

`--generated` widens the PATHS to the whole collector tree, keeping the same
filters (230 sites, 91 files). Treat even that as noise: the `vsrc*` fleet
pages through `jsonlist_emit_paged()` at the macro level and the `hp*` tables
page through declared opts, and neither is visible in the text around the URL,
so a generated row with `limit=60` is almost always being walked. Removing the
filters as well puts the number in the thousands and says nothing at all.

    python3 native/tools/audit_url_limits.py              # the narrow 100
    python3 native/tools/audit_url_limits.py --all        # + search pivots
    python3 native/tools/audit_url_limits.py --json       # for triage

To settle one row, ask the upstream how big the collection is and compare:

    ./bin/japanosint --run <ID>
    # then fetch the same URL with a much larger limit, or read the
    # upstream's own total, and see whether the row's number is the ceiling
"""
import argparse
import glob
import json
import os
import re
import sys

# Record-limit query parameters, as spelled by the APIs this tree actually
# talks to. `page`/`offset` are deliberately absent: those are paging, and a
# row that carries one is being walked, not truncated.
LIMIT_PARAM = re.compile(
    r'[?&](?:limit|per_page|page_size|pageSize|rows|maxRecords|maxCountItem'
    r'|count|size|\$top|retmax|resultRecordCount)=(\d+)')

# Any sign that the enclosing function walks pages. Deliberately generous —
# a false NEGATIVE here (a paged row not reported) costs nothing, while a
# false positive adds noise to a list a human has to read.
PAGING = re.compile(
    r'pw_walk|jsonlist_emit_paged|jsonlist_next_page|pagewalk|'
    r'next_path|page_param|offset|&page=|\?page=|'
    r'page\s*\+\+|\+\+\s*page|for\s*\(\s*int\s+page|while\s*\(\s*page')

# The entity reaching the URL means this is a pivot: a bounded view of a
# ranked answer, which rule 2 permits.
ENTITY = re.compile(r'ctx->entity|->entity\b|vars->raw')

SCHEDULED = re.compile(r'\.update_interval_sec\s*=\s*[1-9]')

FUNC_START = re.compile(r'^static\s+\w[\w \*]*\w\s*\([^;]*\)\s*\{', re.M)


def functions(text):
    """-> [(first_line_no, body)] for each top-level static function.

    Crude on purpose: the only thing the scope is used for is deciding whether
    paging and the entity appear NEAR the URL rather than elsewhere in a file
    that may hold a dozen unrelated collectors. soc_packages_feeds.c is exactly
    that shape — four collectors in one file, one of which paged nothing — and
    a whole-file test would have cleared it on another collector's evidence.
    """
    starts = [m.start() for m in FUNC_START.finditer(text)]
    if not starts:
        return []
    starts.append(len(text))
    return [(text[:starts[i]].count("\n") + 1, text[starts[i]:starts[i + 1]])
            for i in range(len(starts) - 1)]


def scan(paths, require_scheduled=True, skip_pivots=True):
    hits = []
    for path in paths:
        try:
            text = open(path, encoding="utf8", errors="replace").read()
        except OSError:
            continue
        if require_scheduled and not SCHEDULED.search(text):
            continue
        for line0, body in functions(text):
            if PAGING.search(body):
                continue
            if skip_pivots and ENTITY.search(body):
                continue
            for m in LIMIT_PARAM.finditer(body):
                if int(m.group(1)) <= 1:
                    continue      # limit=1 is its own check (`limit-one`)
                hits.append({
                    "file": path,
                    "line": line0 + body[:m.start()].count("\n"),
                    "param": m.group(0).lstrip("?&"),
                    "value": int(m.group(1)),
                })
    return hits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default="collectors",
                    help="collector tree root (default: collectors)")
    ap.add_argument("--all", action="store_true",
                    help="include entity pivots, whose limit is a bounded "
                         "VIEW and usually legitimate")
    ap.add_argument("--generated", action="store_true",
                    help="widen the PATHS to the whole collector tree, same "
                         "filters. The vsrc*/hp* fleets page in the engine, "
                         "not in the text, so these extra hits are almost all "
                         "walked rows -- the flag exists so nobody has to "
                         "rediscover that")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()

    if a.generated:
        paths = sorted(glob.glob(os.path.join(a.root, "**", "*.c"),
                                 recursive=True) +
                       glob.glob(os.path.join(a.root, "**", "*.inc"),
                                 recursive=True))
    else:
        paths = sorted(glob.glob(os.path.join(a.root, "sources", "*.c")) +
                       glob.glob(os.path.join(a.root, "pod", "*.c")))

    hits = scan(paths,
                require_scheduled=not a.all,
                skip_pivots=not a.all)

    if a.json:
        json.dump(hits, sys.stdout, indent=1)
        print()
        return 0

    for h in hits:
        print("%s:%d\t%s\t%d" % (h["file"], h["line"], h["param"], h["value"]))
    by_file = len({h["file"] for h in hits})
    print("\n%d site(s) across %d file(s) — CANDIDATES, not findings. A limit "
          "on a ranked search pivot is the question being asked; a limit on a "
          "scheduled read of a collection that has an end is a discard. Only "
          "the upstream can say which, so this is the start of the work."
          % (len(hits), by_file), file=sys.stderr)
    # Always 0: this is a lint to read, not a gate to pass. Returning non-zero
    # would make it unusable in CI and tempt someone to wire it in before the
    # hundred candidates have been triaged one upstream at a time.
    return 0


if __name__ == "__main__":
    sys.exit(main())
