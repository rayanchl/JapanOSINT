#!/usr/bin/env python3
"""Flag manifest rows whose endpoint pages but which declare no pagination.

House rule 2 (docs/SOURCE_EXHAUSTIVENESS.md): a source that is called must be
used exhaustively. A paged endpoint read once has silently discarded everything
after page 1, and that is invisible in the output -- the response looks complete
because it is a complete *page*.

`make audit-sources` catches this in C, after generation. This catches it in the
manifest, before, where it is one field to fix rather than a regenerated table.

Three signals, in decreasing confidence:

  1. the URL carries a paging parameter (page=, offset=, start=, from=, skip=)
     and the row declares neither `page_param` nor `next_path`;
  2. the URL carries a page-size parameter (limit=, per_page=, rows=, size=,
     count=, maxrecords=) at a value that is plainly a page rather than the
     whole set, and no pagination is declared;
  3. the row's probe returned exactly the page size it asked for -- the classic
     signature of a truncated read, since a result set that happens to be
     exactly `limit` long is far rarer than one that was cut off there.

Signal 3 needs the probe results, so pass --results to enable it.

Usage:
  audit_batch_pagination.py MANIFEST... [--results verified.tsv] [--quiet]
"""
import argparse
import csv
import io
import os
import re
import sys
import urllib.parse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
# THE manifest parser. This file used to read the opts field with a bare
# `"page_param=" in o` SUBSTRING test -- which finds the text inside a
# post_body value and calls the row paginated -- and split `pagination_ok` out
# with a naive `.split(";")` that ignores the `\;` escape the format defines
# for header values. Two more ways for two tools to disagree about one line.
from manifest import COLS, parse_opts                       # noqa: E402

PAGE_PARAM = re.compile(
    r"[?&](page|pageNum|pageNumber|offset|start|startIndex|startAt|skip|from|"
    r"resultOffset|\$offset|page%5Bnumber%5D)=([^&]*)", re.I)
# `from=` and `start=` are record offsets on some APIs and *timestamps* on
# others. IODA's /signals/raw takes a mandatory from/until window and ignores
# page and limit entirely; declaring page_param there would make the engine
# refetch one document and emit duplicates. So a from/start whose value parses
# as a date or an epoch is a time window, not an offset, and is not a finding.
TIMESTAMPY = re.compile(
    r"^(\d{9,13}|\d{4}-\d{2}-\d{2}([T ].*)?|-?\d+[dhwmy]|now(-.*)?)$", re.I)
SIZE_PARAM = re.compile(
    r"[?&](limit|per_page|per-page|pageSize|page_size|rows|size|count|"
    r"maxrecords|maxCountItem|resultsPerPage|retmax|\$limit|"
    r"page%5Bsize%5D)=(\d+)", re.I)

def rows(paths):
    """-> (rows, malformed) — the lines that LOOK like rows and are not come
    back rather than being `continue`d over. Skipping them silently meant this
    tool reported "0 findings" about a file it had only partly read, which is
    the same lie as a passing gate."""
    out, bad = [], []
    for p in paths:
        for lno, line in enumerate(io.open(p, encoding="utf-8"), 1):
            s = line.rstrip("\n")
            if not s.strip() or s.lstrip().startswith("#"):
                continue
            at = "%s:%d" % (p.rsplit("/", 1)[-1], lno)
            if s.count("|") != len(COLS) - 1:
                bad.append((at, s.count("|") + 1))
                continue
            r = dict(zip(COLS, s.split("|")))
            r["_at"] = at
            out.append(r)
    return out, bad


def declares_paging(o):
    """`o` is the PARSED opts. The old test was `"page_param=" in r["opts"]`,
    which is true whenever those nine characters occur anywhere in the field —
    inside a post_body, inside a header, inside a pagination_ok reason — and a
    row that merely mentions paging was thereby excused from declaring it."""
    return bool(o.get("page_param") or o.get("next_path"))


def declares_exception(o):
    """`pagination_ok=<reason>` — the C tree's `exhaustive-ok` marker, in a
    manifest. Some endpoints genuinely cannot be paged: the Wikimedia core
    search API caps `limit` at 100 and has no offset parameter at all, and the
    OSM changesets API refuses limit>100. Those rows would stay flagged forever
    otherwise, and a warning that can never be cleared is a warning nobody
    reads — which is how the real ones get missed. The reason is mandatory."""
    return o.get("pagination_ok") or None
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifests", nargs="+")
    ap.add_argument("--results", help="verified TSV, enables the items==size check")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()

    items = {}
    if a.results:
        for rec in csv.DictReader(io.open(a.results, encoding="utf-8"), delimiter="\t"):
            if rec.get("items", "").isdigit():
                items[rec["id"]] = int(rec["items"])

    n = flagged = excepted = ambiguous = 0
    rs, malformed = rows(a.manifests)
    for at, nf in malformed:
        print("%-38s %-26s %d fields, want %d — NOT CHECKED"
              % (at, "", nf, len(COLS)))
    for r in rs:
        n += 1
        o, dups, _junk = parse_opts(r["opts"])
        if dups:
            ambiguous += 1
            print("%-38s %-26s duplicate opt %r — UNVERIFIABLE"
                  % (r["_at"], r["id"], dups[0]))
            continue
        if declares_paging(o):
            continue
        if declares_exception(o):
            excepted += 1
            continue
        url = urllib.parse.unquote(r["url"])
        why = None
        m = PAGE_PARAM.search(url)
        if m and not (m.group(1).lower() in ("from", "start")
                      and TIMESTAMPY.match(m.group(2).strip())):
            why = "URL carries a page/offset parameter (%s=)" % m.group(1)
        else:
            m = SIZE_PARAM.search(url)
            if m:
                size = int(m.group(2))
                got = items.get(r["id"])
                if got is not None and got >= size:
                    why = ("probe returned %d for a page size of %d — the read was "
                           "cut at the page boundary" % (got, size))
                elif got is None and size >= 50:
                    why = "URL carries a page-size parameter of %d" % size
        if why:
            flagged += 1
            if not a.quiet:
                print("%-38s %-26s %s" % (r["_at"], r["id"], why))

    print("\n%d of %d rows declare no pagination but look paged"
          "  (%d declared pagination_ok)" % (flagged, n, excepted))
    if ambiguous or malformed:
        print("    NOT CHECKED: %d row(s) with an ambiguous opt, %d malformed "
              "line(s) — unexamined, not clean." % (ambiguous, len(malformed)))
    return 1 if (flagged or ambiguous or malformed) else 0


if __name__ == "__main__":
    sys.exit(main())
