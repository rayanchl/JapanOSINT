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
| 19 DHS Program rows (9 hp, 10 VJSON) | 100 of RecordCount each (1,900 a pass), keyed on the indicator label | `perpage=5000`, paged, keyed on `DataId`: **333,097 a pass, stored = emitted on all 19** (Senegal 38,088, Rwanda 24,848, Uganda 20,391 …) |
| `eu-emodnet-ha-desalination` | 11 of 3,911 merged at page seams (`properties.id` is the plant) | one request, **3,911/3,911** |
| `us-socrata-discovery-seattle` | `order=createdAt` ties: 2 datasets served twice, 2 never | `order=dataset_id`, **696/696** |
| `us-usaspending-agency-program-activity` | 100 of 456 | **456/456** |
| `geo-inat-birds`, `geo-inaturalist-obs-threatened` | VJSON offset walks over a live head | hp keyset rows: **2,000/2,000**, **4,000/4,000** |
| `UK_GOVUK_SEARCH` (pivot) | `count=50` stepped by 100: half of every block never asked | `count=100`, keyed on `link`: **305 of 305** for a test query |
| 8 GOV.UK search rows | no `id_keys` (39 siblings key on `link`) | `id_keys = "link"`: all 8 OK, stored = emitted + notices (MAIB 1,106, RAIB 605, the rest 300) |
| 6 d-portal transaction rows | title fallback | the 8-part composite their siblings carry. **Unmeasured**: all 11 d-portal rows, siblings included, run past the sweep's kill line (SLOW on 2026-10-25 at 90 s and today at 400 s) |
| `CFTC_SWAPS_REPORT` | the oldest 200 of 291,909, every 15 minutes | its siblings' `:id` keyset over a cycle, 6-hourly: **10,000/10,000** a run |
| `co-dnp-proyectos-sgr`, `lat-co-siri` | fallback key lost 1 and 3 a run | `:id` (35,010 and 10,000 distinct) |
| 56 Tokyo incident rows | 16 of 1,377 identical-byte incidents merged (2023 parts theft) | `occurrence_rows = 1`: **1,377/1,377** |
| `JO41_WP_CORP_4012` | "non-JSON body" every run (two BOMs) | **142/142** |
| WhatsMyName | one site at a time, up to 15 minutes a pivot | a 12-worker pool, same budget and disclosure |
| 36 TPEx/TWSE rows (`hp3b19_finance.c`) | 14 keyed on a field equal on every record (`出表日期`/`Date`), 17 on `CompanyName`, `TWSE_TRADING_HALTS` on its list POSITION (each new halt #1 overwrote the last) | a key distinct over every record and equal to the distinct-whole-record count, the reporting period included where the record carries one; **all 36 OK** through the engine |
| 16 frozen-date rows (USAspending ×9, NASA POWER, Open-Meteo, NASA NEO, Comtrade, SICONFI ×2, Oyez) | a fixed year/window re-read for ever | `{date…}` tokens tracking the latest published period, with a step back where a period can be unpublished; a year stamped into records that carry none. **15 OK**; `us-usaspending-agency-overview` then needed its own `fiscal_year` (the default is the empty current year): **16/16** |

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

## Decided by the owner and done (2026-11-11)

* **Twins removed** (12 rows, `docs/removed-sources-2026-11-11-twins.tsv`):
  six `gov-ukparl-*` rows that read the same endpoint as a `cyb-ukparl-*`
  row (interests, committees, committee publications, Commons divisions,
  Lords divisions, treaties; the `members` pair differs by a current-member
  filter and both stay); five `chronicdata.cdc.gov` inventory rows (the host
  is an alias: catalogue 1,480 = 1,480, identical ids and metadata); and
  `JP25_JPNATGOV_JISHIN_HQ_POSTS`, whose survivor gains its body field.
* **OpenAlex scheduled rows gated** on `OPENALEX_API_KEY` (26 VJSON rows via a
  new `VJSON_KEYQ` macro, 8 hp rows via `key_env`); the on-demand pivots stay
  keyless. Without the key a row files one needs-credential notice and makes
  no request; with it the key is sent as `api_key=` and never stored (central
  redaction, checked with a fake key: 0 stored rows carry it).
* **law.go.kr gated** on `LAW_GO_KR_OC` (13 rows sent `OC=test`): 11 hp rows
  `OC={key}`, 2 VJSON rows `VJSON_KEYQ` with `OC`.
* **TWSE fixed.** The "FOR SECURITY REASONS" page is a CDN miss: the
  exchange's origin refuses this container and the CDN answers 307 with no
  Location, while cached copies answer 200. `core/httpclient.c` now retries a
  3xx that names no target (as it does a 5xx or 429); test
  `test_http_dangling_redirect.c` (checked to fail without it). Measured
  after: `TWSE_BROKER_LICENCE` 22, `TWSE_ESG_INFOSEC` 832,
  `TWSE_ESG_RISKPOLICY` 822 stored. `TWSE_PUBCO_BALANCE` is empty upstream
  until the quarter is published (one placeholder row with only its table
  date filled): a new `require_keys` row field (hptest 37g) treats such a row
  as a placeholder, so the run is an honest empty instead of storing it.
* **Dead rows**: `JO41_WP_CITY_324485` re-pointed to the town's new site feed
  (1,125 items); five removed with their evidence in
  `docs/removed-sources-2026-11-11-dead.tsv` — `CERT_PH_WP_POSTS` and
  `cyb-ncert-ph` (Cloudflare challenge / login), the JBSTORECSS route (gone),
  and the two old Misato WordPress routes.
* **Old rows**: `tools/cleanup_stale_uids.py` gained a `rekeyed` rule for the
  re-keyed sources (DHS, d-portal, GOV.UK search, the UK interests register,
  the moved iNaturalist rows, every keyed TPEx/TWSE row): rows are grouped by
  the source's new key fields, read from the source's own C definition, and
  an older row is stale only when a newer row with a different uid holds
  every data field it stored. CFTC, the two Colombia `:id` rows and the
  year-stamped rows are not covered: their old rows never stored the new key.
  Report first: `python3 native/tools/cleanup_stale_uids.py "$JO_DB"`, then
  `--delete` when you choose.

Registered sources: **61,510** (61,527 − 12 twins − 5 dead), both floors
re-recorded from that build.

## Round three: the owner's second set of decisions (2026-11-12/14)

Each change was run through the real binary after it was made (paced engine
runs, a fresh copy of a warm database per row); the readings below are the
engine's, not the probe's.

**Security and core**

* **Private addresses are refused by default** (`JO_HTTP_BLOCK_PRIVATE=0`
  opts out). The operator's own service URLs (LLM, embeddings, vision, JWKS,
  the source summary model) go through `http_request_service()`, an explicit
  per-call exemption, at seven call sites. **The first strict build refused
  every fetch in this container**: the egress proxy listens on `127.0.0.1`,
  and behind a proxy the peer libcurl connects to is the proxy. A configured
  proxy endpoint (address AND port, from the variables libcurl reads) is now
  allowed; `test_http_egress_proxy.c` fails without that and passes with it.
  Every unit test had passed before the fix, because none runs behind a proxy.
* **Workspace keys for pivots**: a search run's pivots are dispatched for the
  run's owner workspace (`core/pipeline.c` → `osint_dispatch_tenant()`), so a
  pivot's `key_env` resolves from that workspace's saved key first and the key
  is scrubbed from every record. Scheduled runs keep the environment.
* **Platform audit chain**: `GET /api/audit[/verify]?scope=platform`,
  operator-only (403 otherwise, 400 for an unknown scope).
* **HTTP/2 framing fault retried once over HTTP/1.1** (`core/httpclient.c`):
  the French commune feed whose server intermittently sends a forbidden
  `upgrade:` header now stores 10 of 10.
* **Three tables that were read and never written now have writers**:
  `collector_ttls` (`/api/admin/collector-ttls`, operator, audited, bounded to
  the cache's own clamp), `tenant_report_templates` (`/api/reports/template`,
  workspace owner/admin, audited, bounded to the renderer's buffers) and
  `llm_station_merges` (written by the station clusterer's judge as PENDING;
  only an operator-APPROVED merge is applied; review at
  `/api/admin/station-merges`). Client forms for all three.
* **GTFS Realtime**: `lib/gtfsrt.c` (protobuf decoder, no external library,
  `make gtfsrttest` 269 checks, in CI) and `gtfs-rt-jp`, which reads the
  realtime URLs the gtfs-data.jp catalogue declares and replaces each
  operator's rows in `gtfs_rt_trip_updates`, `gtfs_rt_positions` and
  `gtfs_rt_alerts`. One engine run at 00:40 JST stored 222 records (the
  agent's daytime harness run: 1,148). The departures board now falls back to
  the other half of a stop update's delay, scopes alerts to their own operator
  (two operators' route "3" used to share alerts), and uses only delays
  received in the last 30 minutes (`test_transit_rt.c`). **Under the default
  6 h cadence the board will show the timetable, not delays**: set
  `gtfs-rt-jp`'s interval from its Settings page for live delays.
* Tidy-ups: four dead library functions removed; one base64 implementation
  (`core/jo_codec`) for keysapi, camera_store and alert_deliver, checked
  byte-identical against the removed copies; `--ingest` reports over-long
  lines it skipped.

**Rows** (engine readings, 2026-11-14)

| row(s) | after |
| --- | --- |
| `cyb-sejm-pl-interpellations` | the whole term in one request: **21,033 of 21,033** (was a recent slice) |
| `eur-stortinget-saker` (fused with `gov-no-stortinget-saker`) | current session plus 3 past sessions a run, cycling: 1,942 then 2,297, **3,889 distinct over two runs**, resume `s=2022-2023` → `s=2019-2020` |
| `undp-transparency-projects` | year-stamped, keyed project+year: **5,636 of 5,636** |
| `us-cfpb-hmda-filers` | latest published year, keyed `lei+period`: **1,122 of 1,122** (a name key merged six lenders) |
| 4 `JO40_CNGOV_*` libraries | `n=100` (the API now refuses >150): **6,229 / 12,823 / 15,074 / 8,645**, each its declared total |
| `JO29A_CAM_US_VA_VDOT_CAMS` | the site's own `/services/511` path: **1,713 of 1,713** |
| `JO32_REG_GIFU_INDUSTRIAL_WASTE_TRANSPORT` | `index_url` on the portal page: **5,689 of 5,689** |
| `us-ny-dec-remediation-*` (1 re-pointed, 2 new) | the register split into three layers: **2,138 + 2,074 + 1,717** |
| `JO53_AGS_GISSERVICES_MUNICIPAL_STORM`, `_BOAT_LAUNCH` | re-pointed: **1,502** and **803**, complete |
| `classifieds` | prefecture labels in short form are now read: 3,490 emitted / 3,312 stored (was 285 / 278); the repeats are promoted ads served on several pages under one article id |
| `real-estate` | `{ar, ta}` pairs: 1,800 / 1,798 (was SUUMO's error page for every slug) |
| Europe PMC / OpenAIRE (16 rows) | first page bound or `nextPageUrl` followed: all OK, e.g. `EUROPE_PMC_SEARCH` 2,000 / 2,000, `OPENAIRE_SOFTWARE` 159 / 159 |
| 6 frozen-date VJSON rows | date tokens; all OK (NSF 8, Kokkai 30, NASA 100, NOAA 100, HMDA, UNDP above) |
| PMG ×3 | policy documents re-read at a weekday-varying page size (1,261 distinct of 1,270 this run; seven days cover all 1,270); daily schedule and committee meetings cycle 20 pages a run (resume `page=20` → `page=40`, 1,987 distinct over two runs) |
| `DK_CVR` + 3 cvrapi rows, `nlni-landuse` | gated with a needs-credential notice (`CVRAPI_TOKEN`; `MLIT_L03B_GEOJSON_URL`): no official keyless CVR lookup answered (Virk's index wants credentials, its gateway a challenge) |
| d-portal transaction rows | measured at last, with a 6 GB file limit: Ethiopia 100,000 / 100,000, South Africa 99,983 / 100,000 |

Removed: `mercari-trending` (endpoint 404 three readings, site alive;
`docs/removed-sources-2026-11-11-dead.tsv`) and `gov-no-stortinget-saker`
(fused). Registered sources: **61,511**, both floors re-recorded.

## Still open

* **Owner decisions** surfaced by this round, not acted on:
  * `PL_SEJM_API`, `gov-pl-sejm-interpellations` and
    `eur-sejm-term10-committees-sits` read strict subsets of what
    `cyb-sejm-pl-interpellations` now reads whole;
  * `bc-mineral-tenure` and `BC_MINERAL_TENURE` read the same layer;
  * `EUROPEPMC_SEARCH` and `EUROPE_PMC_SEARCH` are two pivots on one endpoint;
  * records a pivot fetches with a workspace's key land in the shared corpus;
  * `dataset.gov.md` is down host-wide (not removed: the government still
    links it).
* Hand-written collectors that read keys through `jo_env()` do not see
  workspace keys yet; `twitter-geo` caps its own database read at 5,000.
* Rate limits keyed on client IP behind a proxy.
