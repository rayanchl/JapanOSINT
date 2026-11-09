# Audit round two (2026-11-08/09)

Follows `docs/egress-tls-reread-2026-11-07.md` and the round-one pass
(commit `06fd20f4`). Round two asked four questions of the whole tree: is
every feature wired to the others, can shared code replace its copies, do
sources that work the same way read the same way, and do the changed sources
still work. Commits: `cce83cee`, `36a4402c`, `2f8cd994`, `4b2ab853` and the
one carrying this file.

## What was verified, and how

* **826 changed scheduled rows through the engine** (the real binary,
  `audit_registry_emit.py`, a fresh copy of a warm database per row): 732 OK.
  Of the 61 that read worse than the 2026-10-25 sweep, **40 were OK on a
  second run** — one probe is one reading. The rest were diagnosed one family
  at a time:
  * six FAA rows on `services6.arcgis.com` answered 208 bytes only when the
    sweep ran them **concurrently on one host**; alone each stores 4,000;
  * `geo-la-noaa-storm-reports` holds **0 features today** (`returnCountOnly`
    says 0, sorted or not) — a seasonal empty, not the sort;
  * data.gov.il flapped (17 rows, 919-byte answers in the sweep; `--run`
    alone stores 2,000);
  * two real defects, fixed below (EMODnet, Seattle).
* **47 VJSON rows** changed by the round-two agent: 42 OK; the rest are an
  out-of-session empty (Kokkai), SWOB's tie-ordered sort (already disclosed),
  one-record collisions and one slow row.
* `make unit` (92 tests), `hptest`, `pagewalktest`, `rsstest`, `authtest`,
  `lint-sources`, both floors, `audit-sources`, `collectors-def-check`,
  `tools-selftest`, client `vitest` (214) and `vite build`: all pass.

## Engine changes

| change | why (measured) |
| --- | --- |
| per-run **detail cache** in `lib/hpengine.c` | many list records name one detail record; DHS Uganda's 20,391 values share ~3,000 indicator definitions. The hop re-fetched each one per record and spent the run's budget on duplicates. One run: 5,097 records merged from 797 hops, no request spent. `hptest` 37d |
| **identity before the hop** | the uid was derived after the detail merge, so a fallback could resolve to `detail.id`/`detail.Label`: one record, two uids depending on whether this run's budget reached it, and the collision guard (keyed on the list record) never saw the emitter's key. `hptest` 37f, checked to fail under the old line. Rows whose key came from a `detail.*` field re-key once |
| every leading **UTF-8 BOM** stripped | cJSON skips one; a WordPress host serving two read "non-JSON" every run. `hptest` 37e |
| page-1 wall test on a **small body's head** only | a real HTML page with a reCAPTCHA footer is not a bot wall; a 100 MB body no longer pays six scans |
| `lib/pagewalk.c`: no "more records than collected" on a **resumed cycle** that completes | `total` is then this run's slice and the upstream's total the whole collection |
| declared totals gain `resultSetSize`, `PagingInfo.Total`, `page_metadata.*`, `numberOfRecords`, `RecordCount`; next link gains `page_metadata.next` | each was a row disclosing "no total" with one in its body |
| `lib/jsonlist.c`: `limit` + a bound `page` advances the **page**, not an invented `offset` | OpenAerialMap and USAspending ignore `offset=`; `lib/pagewalk.c` already advanced the page for the same OpenAerialMap URL. `pagewalk_test` [13] with a control |
| `http_client_absorb()` | worker-pool collectors (WhatsMyName, the Sherlock port) now report every request on the run line |
| `tools/lint_sources.py` inlines an `#include` with a trailing comment | six `VJSON_IDKEYS` rows read as unregistered (count 61,521 against the binary's 61,527) |

## Rows repaired (each measured through the engine after)

| row(s) | before | after |
| --- | --- | --- |
| `UK_LORDS_REGISTER_INTERESTS` | 20 a run (API caps a page at 20; `Skip` stepped 100; keys named a `value.` wrapper the records lack) | **4,169 of 4,169**, 210 requests, daily |
| 19 DHS Program rows (9 hp, 10 VJSON) | 100 of RecordCount, keyed on the indicator label | `perpage=5000`, paged, keyed on `DataId`: Uganda **20,391/20,391**, Côte d'Ivoire **12,711/12,711** |
| `eu-emodnet-ha-desalination` | 11 of 3,911 merged at page seams (`properties.id` is the plant) | one request, **3,911/3,911** |
| `us-socrata-discovery-seattle` | `order=createdAt` ties: 2 datasets served twice, 2 never | `order=dataset_id`, **696/696** |
| `us-usaspending-agency-program-activity` | 100 of 456 | **456/456** |
| `geo-inat-birds`, `geo-inaturalist-obs-threatened` | VJSON offset walks over a live head | hp keyset rows: **2,000/2,000**, **4,000/4,000** |
| `UK_GOVUK_SEARCH` (pivot) | `count=50` stepped by 100: half of every block never asked | `count=100`, keyed on `link`: **305 of 305** for a test query |
| 8 GOV.UK search rows | no `id_keys` (39 siblings key on `link`) | `id_keys = "link"` |
| 6 d-portal transaction rows | title fallback | the 8-part composite their siblings carry |
| `CFTC_SWAPS_REPORT` | the oldest 200 of 291,909, every 15 minutes | its siblings' `:id` keyset over a cycle, 6-hourly |
| `co-dnp-proyectos-sgr`, `lat-co-siri` | fallback key lost 1 and 3 a run | `:id` (35,010 and 10,000 distinct) |
| 56 Tokyo incident rows | 16 of 1,377 identical-byte incidents merged (2023 parts theft) | `occurrence_rows = 1` |
| WhatsMyName | one site at a time, up to 15 minutes a pivot | a 12-worker pool, same budget and disclosure |

Plus the round-two agents' changes (in `2f8cd994`): UK Parliament take/skip
paging on 18 rows, Environment Canada sorts and windows on 6, UNHCR,
USAspending, OCHA, GBIF and others' frozen dates rolled, 15,554 Japanese
rows tagged `lang = "ja"`, 1,843 feeds on their measured cadence, FPDS and
KSH keys, three `host_agents.def` lines (`www.cbc.ca`, `www.rt.com`,
`datos.gob.mx`) for hosts that refuse the engine agent once a row's own
User-Agent was removed.

## Not changed, and why

* **Feeds that now serve a homepage** (Brookings, two Carnegie, Banco
  Central do Brasil, Caribbean National Weekly, CERT-In): `--discover-feeds`
  finds no other declared feed (Brookings declares only the dead one). They
  keep disclosing the failure.
* **GTFS-RT tables** (`gtfs_rt_trip_updates`, `gtfs_rt_alerts`) are read by
  the departures board and written by nothing; the board shows scheduled
  times with no delay. Writing them needs a protobuf decoder this tree does
  not carry.
* **SWOB**: no tie-free newest-first sort exists (any second key reads as a
  filter); the seam is disclosed.

## Owner's decisions

* **Twin rows**: `gov-ukparl-interests` and `cyb-ukparl-interests` read the
  same URL (recorded in the lint baseline, not hidden); the
  `cyb-ukparl-*`/`gov-ukparl-*` families overlap on six endpoints;
  `chronicdata.cdc.gov` is an alias of `data.cdc.gov`; `www.jishin.go.jp`
  posts are registered twice.
* **OpenAlex keyless rows**: keyless requests from this container draw the
  shared-IP 429. Some siblings are already gated on `OPENALEX_API_KEY`; ~20
  keyless rows and ten pivots are not. Gating them stops them for an install
  without a key.
* **law.go.kr** (19 rows) sends the placeholder `OC=test` and is refused;
  it needs a registered key.
* **`cvrapi.dk`** answers `QUOTA_EXCEEDED` per agent; switching agents to get
  a fresh quota would be evading it, so nothing was added.
* Possible removals the diagnosis found: `CERT_PH_WP_POSTS` (REST needs a
  login), `JO49_..._JBSTORECSS` (route gone), `JO41_WP_CITY_324485`
  (site re-platformed).
* `JO_HTTP_BLOCK_PRIVATE` default, and rate limits keyed on client IP behind
  a proxy.
