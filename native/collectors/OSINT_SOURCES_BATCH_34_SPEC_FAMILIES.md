# Batch 34: 139 rows on specification-fixed endpoint families

> **Renumbered at merge, 2026-10-05.** Authored as "batch 33" (ids `JO33_*`, `hp3b33_*`); another session's 43-row supply-chain batch merged to main first under that number, so this one became batch 34 (`JO34_*`, `hp3b34_*`) and the WEKO3 batch became 35. Row content is unchanged by the renumber. Registry figures below are as of authoring; after the merge the tree is 18,452.

Authored 2026-10-04 in a session with **no outbound HTTPS** (the environment's
network policy allowed Anthropic APIs, package registries and GitHub only).
That constraint shaped the whole batch, so it is the first thing stated here.

Registry **18,170 → 18,309**, +139 (shipped as 150, then 11 FDSN rows were removed on review — see *Review* below).

## The problem this batch is built around

An unprobed row is a guess. Batch 31 was 361 hand-written rows against endpoints
nobody could fetch, and when someone with egress finally probed the 27 scheduled
ones, seven were dead and three duplicated an existing collector. That is a ~37%
failure rate on the part that could be measured.

So batch 34 does not guess. Every row satisfies two conditions that can be
checked **without** a network:

1. **The host is already proven reachable by a different row in this tree**, on
   the same host and the same API base path. If `data.gov.ie/api/3/action/package_search`
   answers today, then `data.gov.ie/api/3/action/` resolves and serves CKAN.
2. **The endpoint path, its parameters, its paging and its response envelope are
   fixed by a published specification**, not inferred from a page. Three specs
   carry the whole batch.

What remains uncertain is narrow and stated: whether *that particular action* is
enabled on *that particular deployment*. A CKAN that disabled a deprecated
action, or an FDSN node that serves only `dataselect`, yields an honest empty.

## Counts

| Beat | Rows | Specification | What each row returns |
|---|---:|---|---|
| `hp3b34_ckanbulk.c` | 95 | CKAN Action API | `current_package_list_with_resources` — whole package dicts **with resources inline**: every dataset plus the URL, format and size of every downloadable file under it |
| `hp3b34_fdsnstation.c` | 15 | FDSN web services | `station/1?level=station` — one row per seismic station: network, station, lat/lon, elevation, operating institution, operating epoch |
| `hp3b34_fdsnevent.c` | 5 | FDSN web services | `event/1` — located events with origin time, hypocentre, depth, contributing agency, magnitude and type |
| `hp3b34_socratacat.c` | 24 | Socrata Discovery API | `/api/catalog/v1?q={q}&domains=<host>&search_context=<host>` — every dataset on that portal whose metadata mentions an entity |
| **total** | **139** | | |

### Why these four, and not 150 new portals

6,520 distinct hosts are already wired. Guessing new portal hostnames without a
network produces dead rows, so the batch instead mines **services already-proven
hosts expose but the tree never wired**:

- 138 CKAN hosts had `package_search` (1,101 rows), but only 31 had
  `current_package_list_with_resources`. `package_search` answers a query with
  dataset summaries; the bulk action answers *"what does this body publish, and
  where are the actual files"*. 105 hosts were missing it; 95 are here and 10
  NGO/aggregator portals were deferred to keep the batch at the requested 150
  (named in the manifest header, not silently dropped).
- 19 hosts had `/fdsnws/` wired, almost all for `availability/1/extent`. Only
  **one** had the station inventory and six had event catalogues.
- 63 Socrata domains had dataset rows; 44 had the catalogue pivot. 25 were
  missing it — 24 are here, and `api.data.gov.in` was **rejected**: it serves
  `/resource/<uuid>` under its own API and is not Socrata, so it has no
  `/api/catalog/v1` path at all.

## Two engine subtleties this batch had to get right

**`csv_delim` defaults to comma and the engine does not sniff.** FDSN
`format=text` is pipe-delimited with a `#` header. All 31 FDSN rows declare
`csv_delim=pipe;csv_comment=#;csv_no_header=1`, so columns are positional
`col0..col7` after the header line is stripped.

This was found by reading `lib/hpengine.c` (`char delim[64] = ","`) and it
exposed a live defect in **batch 31's own `FDSN_STATION_INVENTORY`**, which
declared none of them. CLAUDE.md cites that row as storing 151,303 records —
true, and misleading: with a comma delimiter each whole line became a single
cell, `col1`/`col2`/`col3`/`col5` resolved to nothing, the title fell back to the
raw line and no station carried coordinates. **The count was real; the fields
were never parsed.** Fixed in this change, along with its `id_keys`.

**`id_keys` must be unique per record, not per group** (rule 4b). A station
*code* is not unique across networks — `CI.PAS` and `GR.PAS` are different
stations — and an epoch can re-open under the same code, so station identity is
`col0+col1+col6` (network + station + start time), composed with `+`. The event
rows key on `col0`, which is the FDSN event id and genuinely unique.

**FDSN's `offset` counts from 1**, so the event rows set `page_start=1`. Left at
the unset 0 the walk would re-read the first event of every page.

## Verified (offline gates, all green)

| Gate | Result |
|---|---|
| `make` (`-Wall -Wextra`) | clean, no new warnings |
| `make selftest` | PASS |
| `make unit` | PASS |
| `make hptest` | PASS |
| `make lint-sources` | PASS — no duplicate id or endpoint |
| `make audit-sources` | PASS — 0 findings, strict set clean |
| `make pagewalktest` | PASS |
| `make source-floor` | PASS |
| `batch_exclusions.py --check --bin --strict` | **0 collisions** against the registry's 18,170 ids (the binary's set, not a regex approximation) |
| `audit_batch_reachable.py` | **0 of 150** rows can never run (rule 3) |
| `audit_batch_pagination.py` | **0 of 150** look paged without declaring it (18 declare `pagination_ok`) |
| `audit_page_param.py` | no batch-33 row binds its own `page_param` |
| registry delta | +139 after review, all 139 `JO34_*` ids registered |

## NOT verified — rules 4, 4b and 4d

No row was fetched. Unmeasured: whether the engine turns the response into
`intel_items` (rule 4), whether emitted records survive the sink's uid upsert
(4b), and whether a pivot's filter is honoured rather than silently ignored
(4d — a filter being ignored returns the whole collection with HTTP 200 and
every counting gate green, which is worse than returning nothing).

**Unlike batch 31, the manifests exist**, so both tools can be pointed straight
at this batch:

```sh
cd native
python3 tools/audit_registry_emit.py --bin ./bin/japanosint \
        --match JO34_ --jobs 6 --timeout 220 --out b33.tsv
python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch34.*.txt \
        --check-filter
```

Retire whatever returns `EMITS_NOTHING`, `COLLISION` or `FILTER_IGNORED`. The
24 Socrata rows are the ones `--check-filter` matters most for, since they are
the only entity pivots here; the other 126 are scheduled collection feeds.

The engine cannot fabricate — a moved or reshaped endpoint yields an honest
empty, never an invented record — so an unmeasured row is a coverage gap, not a
source of false data.

## Review (same day, before any probe)

**Socrata pivots were not scoped to their own domain.** A Socrata domain's
`/api/catalog/v1` searches every Socrata portal unless `domains=` (and
`search_context=`) is passed — the tree already had 64 rows doing so, which is
how this was noticed. Unscoped, a pivot on "X" returns datasets from every
portal and attributes them to this one: rule 4d's confident wrong answer, which
no record-counting gate can see. All 24 rows now pass
`domains=<host>&search_context=<host>`, in both the C and the manifest.

**11 FDSN rows asked a host for a service it does not run.** The FDSN spec
fixes the services, but each node implements a subset, and the tree's own
evidence shows which: hosts wired only for `availability/1` are EIDA waveform
nodes, and EIDA's node service set is dataselect/station/availability — not
event. Removed, and kept as rejects in the manifest headers with the reason:

* station on `api.franceseisme.fr`, `earthquake.usgs.gov`,
  `www.seismicportal.eu` — event catalogues with no station service;
* event on `service.iris.edu` (IRIS retired fdsnws-event in favour of USGS),
  and on AusPass, ORFEUS, BGR, BGS, UiB, LMU and EPOS-France — waveform
  archives that locate no earthquakes and so have no catalogue to serve.

Event rows were kept for the five that run monitoring networks and publish a
catalogue: SCEDC, ICGC, ETH/SED, NOA and KOERI. **This was reasoned from what
each operator does, not measured.** A probe may still find one of the kept five
unimplemented, or one of the dropped eleven live.

The removal itself broke the build once — where the dropped row was the last in
its table, the deleted chunk also carried `};` and `HP_REGISTER_TABLE(...)`. An
id-set diff against HEAD confirmed exactly the 11 intended ids were gone after
the footers were restored.

Counts after review: ckanbulk 95, fdsnstation 15, fdsnevent 5, socratacat 24.

## MEASURED, 2026-10-20 — and the reasoning above was 3-for-5 wrong

This batch shipped unprobed (the authoring session's network policy reached
Anthropic APIs, package registries and GitHub, nothing else). Both batches were
run through the real binary on 2026-10-20 — 244 rows of 34 and 35 together, each
against a fresh copy of a warm template DB, then every failure re-run once:
**`docs/emit-sources-batch34-35.tsv`**.

```sh
python3 native/tools/audit_registry_emit.py --bin ./bin/japanosint \
        --match '^JO3[45]_' --jobs 6 --timeout 220
```

**179 OK, 266,390 records emitted.** 42 `EMPTY_UPSTREAM` — the endpoint
answered and held nothing for that particular entity, which is rule 1's honest
empty and not a defect. 23 rows failed the first pass; one (`PACKAGIST_ALL_PACKAGE_NAMES`)
was SLOW and completed on the retry at 468,791, so **22 failed twice**.

The section above closes with "A probe may still find one of the kept five
unimplemented, or one of the dropped eleven live." It found three:

| kept row | reading |
| --- | --- |
| `JO34_FDSNEVT_EIDA_GEIN_NOA_GR` | HTTP 400 — no `event/1` |
| `JO34_FDSNEVT_EIDA_KOERI_BOUN_EDU_TR` | HTTP 404 — no `event/1` |
| `JO34_FDSNEVT_WS_ICGC_CAT` | HTTP 404 — no `event/1` |

So the review's own lesson — "the FDSN spec defines services; a node runs only
some of them" — was applied correctly as a rule and got the membership wrong for
three of the five it kept. Reasoning from what an operator *does* narrowed 15
rows to 5 and could not get past that; one request each settled it.

**The headline finding is a CKAN one, and it is the reason nothing was removed
on this reading.** `current_package_list_with_resources` is an expensive bulk
dump that a large portal disables or never exposes, while `package_search` on
the same host is served normally. Three rows that failed twice are alive:

* `data.gov.uk` — 403 on the bulk action, **59,370 datasets** via package_search
* `admin.opendata.az` — 403 on the bulk action, **1,970**
* `data.gov.il` — 404 on the bulk action, **1,203**

A literal reading of CLAUDE.md's "a row that fails twice leaves" would have
deleted three national catalogues and 62,543 datasets for using the wrong verb.
The remaining 19: six are one `ogopendata.com` 502 outage inside the same
minutes (Houston, Milwaukee, Oklahoma, San Antonio, Virginia, Hawaii — the host
is alive and its action untested), three are bot walls that refuse the engine
too, three are the FDSN rows above, `catalog.data.gov` has moved,
`catalogue.data.govt.nz` answers 200 in a shape that is not the declared one,
four are transport failures from this container alone, and `asahikawa-med`'s
WEKO answers 500. Each row's diagnosis is the TSV's last column.

Still open on this batch, and not done here:

* **rule 4b is unmeasured** for the 179 OK rows. This audit reads EMITTED; a
  collision between records keying onto each other is invisible to it. Batch 35
  inherited its keys from batch 25 and is the first to distrust.
* ~~21 rows sit at exactly 2,000 or 20,000~~ — **done 2026-10-21**, see below.
* ~~the three alive CKAN rows want re-pointing~~ — **done 2026-10-21**, see
  below.

## The page ceilings, measured and fixed (2026-10-21)

`docs/measured-page-ceilings-batch34-2026-10-21.tsv`. Twenty-one rows read
exactly `page_max x page_size` records — 19 CKAN portals at 2,000 and the two
live FDSN event catalogues at 20,000 — which is never an upstream's own total.

**The count had to come from a different endpoint.** These rows use
`current_package_list_with_resources`, which returns a BARE LIST: no
`result.count`, no envelope, nothing for a gate to compare a run against. That
is why 19 rows sat at 2,000 looking healthy through the emit audit. The same
hosts' `package_search?rows=0` publishes `result.count`, and that is where every
total below comes from. 792,984 datasets across the 19.

Offset safety was measured on all 19 before any ceiling was raised, because
CLAUDE.md's worst shape is a walk whose order drifts: the same offset asked
twice returns the same ids (STABLE), page 0 and page 1 share none (DISJOINT),
and the offset is honoured. All 19 passed all three.

| fix | rows | examples |
| --- | --- | --- |
| complete read (`page_max` = pages needed + 5) | 14 | NHSBSA 2,000 → **2,166**; Berlin → **2,627**; Aragón → **2,425**; data.ca.gov → **4,675** — each its declared total exactly, `stored == emitted` |
| cycle (`page_max = 200`, `page_resume = 1`) | 5 | Hamburg 246,666 declared (2,467 pages); geoscience QLD 192,703; govdata.de 169,142; Schleswig-Holstein 33,062; data.gov.ie 22,881 |

`data.gov.ie` is the cycle proof: two runs against one database stored 20,000
and then 2,881 — 22,881, its declared count exactly — and run 2 filed no notice
because the cycle completed and cleared.

### The headroom is not padding

Set `page_max` to exactly the pages the total needs and the row files a
**false truncation notice on a complete read**. Measured here: at `page_max = 47`
data.ca.gov stored all 4,675 records *and* filed `"the page ceiling or a cancel
stopped the walk"` with `more_pages_pending: true`. Page 47 was short — 75 of
100, the upstream's own end of data — but our ceiling bit in the same breath and
the engine cannot distinguish them. At 52: the same 4,675, **no notice**. Five
pages also absorb ~500 datasets of growth. A disclosure that fires when nothing
was lost is worse than no disclosure, because it teaches the reader to skip them.

### The two FDSN rows

FDSN publishes no total, so the catalogues were sized by **bisecting the
offset** — about twenty one-row requests each.

* **ETHZ** ~46,620 events, oldest ECOS-09's year-0250 entry. `page_max = 52`,
  and the walk then emitted **exactly 46,620** — the probe's arithmetic and the
  engine's walk agreeing to the record. 20,000 → 46,620.
* **SCEDC** answers at offset 200,000 with a 1987 event and times out at
  1,000,000, so the depth below that is recorded as **unmeasured, not absent**.
  `page_max = 200` + `page_resume = 1`.

Both carried `orderby=time`, which is FDSN's **newest-first**, so every new
event lands at the head of the collection the walk is reading — one arrival
mid-walk shifts the boundaries, re-reads one record and skips another. Both are
`orderby=time-asc` now, measured accepted on each host.

## The three alive catalogues, re-pointed (2026-10-21)

`data.gov.uk`, `admin.opendata.az` and `data.gov.il` each failed two readings on
`current_package_list_with_resources` — 403, 403, 404 — and each is alive on
`package_search`. They are re-pointed rather than removed. 62,590 datasets.

**What was measured before changing anything**, because a re-point is only
honest if the replacement answers the same question:

| check | why it matters | result |
| --- | --- | --- |
| `result.count` | the total the row must reach | 59,370 / 2,017 / 1,203 |
| **resources inline** | the entire purpose of these rows is the dataset PLUS the url/format/size of every file under it; a re-point that dropped them would quietly change what the source holds | present on all three (363 / 169 / 343 resource rows on page 0) |
| `rows` honoured | else the server's default size silently governs | 100 returned for `rows=100` |
| `sort=id asc` honoured | an offset walk with no stable order skips records invisibly | yes |
| stable / disjoint | same `start` twice gives the same ids; consecutive pages share none | yes |

Ceilings follow the measured counts: data.gov.uk 594 pages, so `page_max = 200`
with `page_resume = 1` (the catalogue over ~3 runs); Azerbaijan 21 pages and
Israel 13, so complete reads at 26 and 18. Azerbaijan's count had moved from
1,970 to 2,017 in a day, which is what the five pages of headroom are for.

Two things this cost that reasoning would not have caught:

* **The rows' own description was wrong about the alternative.** Each said
  package_search "answers a query with dataset summaries", which is why the
  expensive bulk action was chosen. package_search returns whole package dicts
  with resources inline. An assumption written into a description reads like a
  finding a year later.
* **A whole-catalogue row can duplicate rows already in the tree, and here it
  does not.** data.gov.uk already had 18 per-organization package_search rows
  and data.gov.il 19. But `organization_list` says data.gov.uk publishes
  **1,044** organizations — those rows reach under 2% of its publishers — and 42
  of Israel's 61 have no row at all. Ask `organization_list` before assuming
  either redundancy or coverage.

### Verified after the re-point (2026-10-21)

    data.gov.il          0 ->  1,203   = its declared count, 14 requests
    admin.opendata.az    0 ->  2,017   = its declared count, 22 requests
    data.gov.uk          0 -> 40,000 over two runs, of 59,370

All three stored NOTHING before — a `collector-fetch-failure` notice and no
records. `stored == emitted` on every run; 1,197 of Israel's 1,203 records and
15,845 of the UK's first 20,000 carry `resources.0.url`, so the inline files are
parsed and flattened rather than merely fetched (the rest are datasets the
portals publish with no files, which page 0's 72/100 already predicted).

**The data.gov.uk cycle is additive, which is the hard thing to show.** Two runs
against one database stored 20,000 and 20,000 for **40,000 DISTINCT** records,
and the stored position reads
`...package_search?rows=100&start=40000&sort=id%20asc` — exactly where run 3
resumes. Nothing was re-read at the seam and nothing skipped: a cycle that
re-read records would show fewer than 40,000 distinct, and the skip CLAUDE.md
warns about (the invisible half of a collision) would too. The truncation notice
on each run is TRUE — 594 pages are needed and 200 were read — and says where it
stopped.
