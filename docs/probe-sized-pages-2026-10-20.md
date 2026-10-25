# A page size that came from the authoring probe — 37 rows

**An eighth discard shape, or rather the seventh wearing a disguise.** CLAUDE.md
already records "a record limit the row asks the UPSTREAM for" as the shape no
C-level check can see. This is that shape with a value nobody would defend if
they saw it: **`$top=2`, `limit=3`, `count=3`, `per_page=5`, `size=2`** —
page sizes of one to five records, carried verbatim out of each row's authoring
PROBE, where a tiny limit is correct because the probe only has to prove the
endpoint answers and parses.

Found while measuring open item C4 (eight cycling rows with a descending sort).
`us-legistar-seattle-matters` turned up with `$top=2`, and asking how many rows
share the shape found 42, of which 39 are real:

    $top=1   1 row     $top=2   17 rows    $top=3   1 row
    limit=3/5, count=3, per_page=3/5, pageSize=5, size=2, rows=3, PageSize=5

**Every one is an `hp16_*` detail-hop row** — the batch-16/17 rows that
`collectors/gen_detail_hop_upgrade.py` moved onto hpengine so the record behind
each list hit would be fetched. That upgrade did exactly what it promised and
preserved the probe's page size while doing it.

Three `ENA_PATHOGEN_*` rows matched the search and are NOT in it: ENA's
`limit=0` means "no limit", which is the opposite of a bound.

## Why the gates could not see it

* `audit_url_limits.py` finds the shape but is deliberately not gated, because
  a limit on a ranked search is the question being asked rather than a bound.
* the 2026-10-11 pass that measured those candidates covered only the 42 "whose
  URL is a complete literal (no runtime `%s` to fill)". None of these 39 is in
  `docs/measured-url-limits-2026-10-11.tsv` — checked by id, not assumed.
* every other check looks for a bound written in C. `$top=2` is not a
  `#define`, a `break`, a loop condition, a `seen[]` ring or a single request.
* and the run line looks healthy: `records=40 stored=41 rc=0`.

## What each upstream actually honours, measured

One request per value, 2026-10-20. `n` is records returned:

    gov.uk      count=100 -> 100      count=1000 -> 1000     count=1500 -> 1500
    peeringdb   limit=250 -> 250      limit=1000 -> 1000
    oyez        per_page=100 -> 100   per_page=1000 -> 1000
    fedreg      per_page=100 -> 100   per_page=1000 -> 1000
    Socrata     $limit=1000 -> 1000   $limit=10000 -> 10000
    OpenAIRE    pageSize=100 -> 100   pageSize=200 -> HTTP 400     <- cap 100
    usaspending limit=100 -> 100      limit=500 -> HTTP 422        <- cap 100
    prozorro    limit=100 -> 100      limit=1000 -> 1000
    Safecast    limit=100 -> 25       limit=1000 -> 25             <- cap 25
    CMS         size=1000 -> 1000     size=5000 -> 5000
    OSM         limit=100 -> ok       limit=300 -> HTTP 400        <- cap 100
    ESMA solr   rows=1000 -> 1000     rows=5000 -> 5000
    sk-rpo      limit=100 -> ok       limit=1000 -> timeout
    pl-bzp      PageSize=100 -> 10    PageSize=500 -> 10           <- cap 10
    legistar    $top=1000 -> 1000     $top=5000 -> HTTP 400        <- cap 1000
                $top=1000&$skip=1000 -> 1,000 more, ids 3157..4303, no overlap

## Raising the size was the easy half: the engine was advancing the wrong cursor

A page size only means something if the next page can be asked for, and these
rows all left that to inference — `page_walk = 1`, which pairs the size
parameter a URL declares with the cursor that FAMILY uses
(`PAGERS` in `lib/jsonlist.c`). So each row's page 2 was asked with a parameter
nobody had measured against that host. Measured one page-1-vs-page-2 body hash
per candidate, 2026-10-20:

    family        cursor measured to WORK        what inference would advance
    gov.uk        start                          nothing — `count` is not a
                                                 PAGERS size param at all
    peeringdb     skip                           offset — IGNORED, byte-identical
    Safecast      page                           offset — IGNORED, byte-identical
    CMS           offset                         page
    usaspending   page                           offset
    pl-bzp        PageNumber                      page — IGNORED, byte-identical
    prozorro      next_page.uri (published)      offset=100, but its offset is an
                                                 opaque cursor
                                                 (`1430393143.410437.1.060dc19…`)
    sk-rpo        none — offset is byte-identical
    OSM           none — offset is byte-identical; it slices by time/max_id

So **three of these families were being asked for page 2 with a parameter they
ignore**, which serves page 1 again: the `SAME_AS_PAGE1` finding
`tools/audit_page_two.py` exists for. That sweep could not have caught these,
because it builds page 2 from a parameter the row DECLARES and these rows
declared none.

Each row now names its own measured cursor, and `page_walk = 1` was removed
from the 22 that got one — the engine applies `page_walk` only when a row
declares no paging of its own (`hpengine.c`: `!paged && s->page_walk`), so
leaving both would read as two paging mechanisms on one row. Page-NUMBERED
cursors (`page`, `PageNumber`) take `page_start` and deliberately NO
`page_size`, which is CLAUDE.md's 83-row trap. The two that have no cursor at
all carry an `exhaustive-ok` marker naming the measurement, because for them one
request really is the whole of what can be asked.

## The size chosen is bounded by the DETAIL BUDGET, not by the API

This is the part that makes the fix a judgement rather than a maximisation.
Every one of these rows has a `detail_url`, and `hp_detail_admit()` spends a
per-run budget of 1,000 hops. Raise a page size to the API's ceiling and leave
`page_max` at 20 and a run stores 20,000 records of which 1,000 are deepened —
19,000 stamped `_detail_pending`, disclosed by a notice, and **never deepened
again**, because the budget is spent in page order so the same first thousand
get the hops every run.

So each row is set to **page size × `page_max` ≈ 1,000**: 100 a page over 10
pages for most, 25 over 40 for Safecast (its server caps at 25). A run reads up
to 1,000 records and every record it stores is complete. That is 16x more than
the 60 the old `count=3` configuration could reach, and every one of them whole.

`pl-bzp-board-search` only goes 5 → 10, because 10 is what its server returns
whatever you ask for.

## Two rows left alone, deliberately

* `global-nominatim-structured-search` asks Nominatim for ONE street address
  with `limit=5`. That is a ranked geocode: the limit IS the question, and
  there is no complete answer to truncate.
* `us-sf-311-open311-requests` keeps `page_size=5`. Its host is the one already
  paced at 1,200 ms in `core/hostgate.c` after rate-limiting both the page walk
  and the detail hop, and it 429'd at page 20 even under that pacing. Raising
  its page size without measuring that host's budget first is the guess
  CLAUDE.md forbids.

## The lesson, which is not about page sizes

A generator that carries a probe's parameters into a shipped row carries the
probe's ASSUMPTIONS with them, and a probe is written to be cheap. The same
mistake is on record twice in this tree in other clothes — a date frozen into a
URL at authoring time (43 rows re-fetching the same past week for ever) and
`id_keys` guessed by a generator from the record's first field (batch 36). When
a row is generated from a probe, the probe's limits are the first thing to
re-decide, not the last.
