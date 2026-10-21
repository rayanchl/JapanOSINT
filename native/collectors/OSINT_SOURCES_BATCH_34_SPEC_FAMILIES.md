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

## Measured 2026-10-20 — rules 4, 4b and 4d, live

The paragraph this replaces said "no row was fetched". Sixteen days later every
row was: `probe_hp_batch.py --check-filter` over all four manifests, then the
engine — `audit_registry_emit.py --match '^JO34_'` for the 115 scheduled rows,
`audit_batch_emit.py` with the manifest for the 24 Socrata pivots (a pivot
asked with no entity is `NEEDS_ENTITY`, not a verdict), and one re-run of each
`FETCH_FAILED`. Per-row readings: `docs/verified-sources-batch34.tsv`; the rows
removed: `docs/rejected-sources-batch34.tsv`.

* **Rule 4d held: zero `FILTER_IGNORED`** across the 24 Socrata pivots. The
  review's `domains=`/`search_context=` fix was the right one — an impossible
  entity returns an empty result set, a real one returns that portal's
  matching datasets and nothing from any other Socrata host.
* **Probe: 110 of 139 PASS.** The refusals were 19 HTTP errors (403 bot walls
  on data.gov.uk, data.go.th, opendata.az; 502s from the CKAN portals of six
  US cities and states, read at once), 4 transport failures, 4 Socrata
  catalogues empty for the probe entity, 1 non-JSON body (data.govt.nz), 1
  reset.
* **Engine, scheduled rows: 91 OK, 3 COLLISION, 21 FETCH_FAILED on the sweep;
  1 of the 21 recovered on the re-run.** Socrata pivots: 19 OK, 4 honest
  empties for the probe entity, 1 unmeasurable from here (the session's egress
  refused the tunnel to data.usaid.gov on every attempt — a reading of this
  environment, not of the source, so the row stays).
* **The three collisions were SKIPS, and they changed the beat's endpoint.**
  `current_package_list_with_resources` has no sort parameter and orders by
  `metadata_modified desc`, so a dataset edited while the walk runs shifts
  every later page: govdata lost 3 of 2,000, Zürich 16 of 942, and on an
  unstable order a collision at the sink means the same number of records
  never served at all (CLAUDE.md, "a collision that is really a skip"). All 95
  CKAN rows now read `package_search?sort=id+asc&rows=100` — the same
  datasets WITH their resources, on the stable key — with `page_resume=1` so
  a catalogue larger than the 20-page ceiling (govdata declares 169,254) is
  read over a cycle rather than its first 2,000 forever. Re-measured on the
  new endpoint the same day: Zürich 942 emitted / 942 stored, its own `count`
  exactly. The per-row readings of that second audit are the ones in the TSV.
* **Removed: the three FDSN event rows** whose node answers 400/404 to
  `event/1` on both runs — the shape the review below warned about (a node
  runs only some of the services the spec defines) — and the CKAN rows that
  failed twice on the NEW endpoint as well; the rejects TSV names each with
  both readings.

The engine cannot fabricate — a moved or reshaped endpoint yields an honest
empty, never an invented record — and now each of these rows has a measured
reading instead of a coverage gap.

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
