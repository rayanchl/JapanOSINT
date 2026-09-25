# Registry sweep, repair pass and batch 29 — 2026-09-14 → 15

One session: a full emit/store sweep of the registry, engine and per-row repairs
driven by it, and 342 new sources (batch 29). Every number below was measured on
the real binary — the run line's `records=`/`stored=` AND a `select count(*)`
from that run's own database — never taken from an agent's narrative.

## 1. The sweep, and how to read it

`tools/audit_registry_emit.py --all`, pre-fix binary, 16,367 sources, jobs 8,
timeout 220 s (2026-09-14 22:59 → 09-15 04:51):

| verdict | all | scheduled only |
| --- | --- | --- |
| OK | 13,178 | 13,150 (90.6 %) |
| EMITS_NOTHING | 2,675 | 857 |
| COLLISION | 323 | 323 |
| SLOW | 176 | 176 |
| NEEDS_ENTITY | 15 | 15 |

**Do not quote 2,675.** `--all` runs interval-0 pivot rows with no entity, and
most of them emit nothing by design without logging the needs-an-entity line —
1,818 of the 2,675 are pivots (2,047 in `collector=osint`). The scheduled column
is the like-for-like health number; earlier audits' "888 emit nothing" were
scheduled-only sweeps too.

The back half of the sweep *looked* like an outage (EMITS_NOTHING up to 45 % per
5 % slice). It was not: 2,171 of those 2,309 ids were already EMITS_NOTHING in
the previous registry sweep; the high slices are where pivot-heavy id families
sort. Network was verified healthy (SWPC 200 over IPv4 and IPv6).

**Re-measured on the fixed binary** (all 1,356 scheduled failures, 06:30–07:00):
322 now OK (EMITS_NOTHING→OK 216, COLLISION→OK 92, SLOW→OK 14); still
EMITS_NOTHING 635, COLLISION 233, SLOW 166. Scheduled health ≈ 13,472 / 14,521
(92.8 %) before the last fix agent's 108 repairs.

## 2. Engine fixes

Each is a class of silent loss: the run reported success while records were
dropped, never fetched, or miscounted.

| where | defect | fix | measured |
| --- | --- | --- | --- |
| `lib/hpengine.c` page walk | offset params not spelled lowercase "offset" (`start`, `Start`, `resultOffset`, `$skip`, `startIndex`) had page_start coerced to 1 → offsets 0, step+1, 2·step+1: one record lost per page boundary; `resultOffset`/`Start` advanced as page numbers | start from the URL's own bound value; case-insensitive offset detection | hptest 9f-ter / 9f-quater; UK_GOVUK_ASYLUM_SUPPORT_DECISIONS 100 → 101 |
| `lib/hpengine.c` | a later page answering 429 / 5xx / transport failure broke the walk with no notice (Democracy Club kept 2–14 %) | `collector-truncation-notice` with `failed_page_status`; plain 4xx past the end stays end-of-data | hptest 9f-quinquies |
| `core/scheduler.c` `is_notice_record` | any record_type ending `-notice` counted as a run notice — 798 rows emit real `municipal-notice`, `procurement-notice`, `prefecture-notice` … records, so the run line, `fetch_log` and anomaly detection saw records=0 | require the `collector-` prefix | JPMUNI_ODA_UPDATE 0 → 897; JP25_JPBIZ_KKJ_PREF_01_HOKKAIDO 0 → 100 |
| `lib/jsonlist.c`, `lib/geojson.c` | a FULL last page with no link and no advanceable cursor filed no notice. Socrata `$limit` has no cursor sibling: 472 URLs read one page silently (sample of 38: 28 hold more than one page, up to 17.6 M rows) | disclosure only — "last page full, no cursor" notice; fetch behaviour unchanged (see §6) | nam-austin-311 500 + notice; Calgary 400 of 3,725,365 + notice; no false notice on complete walks (Paris 490/490, WA Hub 744/744, TCU 766, BC 3,357/3,357) |
| `lib/jsonlist.c`, `lib/pagewalk.c` | neither next-link reader followed the ARRAY form `links:[{rel:"next"}]` (ORDS/HAL/OGC) nor `meta.nextLink` | select by rel in both copies; `meta.nextLink` in both | br-tcu-inabilitados 25 → 766; ocha-fts-flow-2026 200 → 20,000 of 20,161 (page ceiling, disclosed) |
| `lib/jsonlist.c` `VJSON_KEYED` | walked page 1 only when the URL declared just a size (plain VJSON seeds a cursor) | seed the cursor for OFFSET families only (page numbers are 0-based on some APIs, 1-based on others), a repeat-page guard, and fall back to the URL as written if the upstream rejects the seeded cursor | af-dportal-act-et 100 → 22,298 (with page size 5,000); eur-cbs-datasets kept working via the fallback; eur-brreg-enheter reads page 0 again (§7) |
| `lib/arcgis_dir.{c,h}` (new) | ArcGIS REST directory rows pointed the JSON emitter at `folders` (bare strings) or at root `services` only | one shared folder walk, id = name/type, service_url composed, failed folders disclosed | 18 rows, e.g. Indiana 72 → 939, DC 21 → 450, SNIRH 0 → 720, NOAA coast 0 → 368 |
| `lib/rss_atom.c` | non-2xx fetch returned -1 silently | log transport rc + HTTP status | — |
| `lib/hpengine.c` HTML rows | `href_must` was tested against the raw href only, so a pattern written as the absolute URL never matched a page that links relatively — the row emitted nothing (7 batch-28 rows, fixed per row) | resolve the link first; accept a match on the raw href OR the resolved link | JO28_CAA_KIKEN 62/62; all gates pass |
| `core/hostgate.c` per-host gaps | `g_nover = 1` while the table held two entries, so `gap_for_host()` never saw the news.google.com gap (615 gnews-* rows kept firing together); NCBI E-utilities (3 req/s without a key) failed when a sweep ran its rows together | count corrected; `eutils.ncbi.nlm.nih.gov` gets a 400 ms gap | 3 NCBI rows run at once: 2,000/2,001, 2,000/2,001, 50/51; 3 gnews-jp rows at once: 70/70 each |
| `core/httpclient.c` `UA_OVERRIDE` | one-token User-Agent filters | 12 per-host entries, each bisected: "collector" (data.sanjoseca.gov, madamasr, telegram.hr, jornalnoticias, bsi.bund.de, container-news), "OSINT" (api.hpc.tools, reliefweb, unocha, hapi.humdata.org), repo URL (ftc.gov), product name (jamestown.org) | madamasr/telegram/jornalnoticias 0 → 10 each; San José datasets 0 → 170 |
| `lib/csv.c` tokenizer | a `"` opened a quoted field ANYWHERE in a cell, not only at a cell's start, so a single mid-field quote re-paired every quote after it and each following line was swallowed into its predecessor — the quote parity of the WHOLE FILE was load-bearing, while the file's own header claimed RFC 4180 | open a quoted field only at the START of a field; a quote elsewhere is an ordinary character (in trim mode blanks before it are still padding, so `a; "b"` reads as `b` as before) | THREATVIEW_C2 503 emitted / 496 stored → 1,173 / 1,165, matching the row's own live count (1,173 data rows, 1,165 distinct) — ~670 C2 detections recovered; TEAM_CYMRU_FULLBOGONS 3,056 and PISTAR_FCS_HOSTS 3,000 identical to baseline; new `tests/unit/test_csv.c` pins both readings |
| `lib/csv.c` + `lib/hpengine.c` | an upstream row leaving a quoted field unterminated ran to the next quote in the file, eating the rows between | close such a field at its own line end past 4 physical lines, count the repairs (`csv_quote_repairs()`) and disclose them as a `collector-shape-notice` (`csv-quote-repaired`) | fires 0 times on ThreatView once the parity defect above was fixed — it is a net there, not the fix. **And its first reported firings were false.** The scanner tracked quotes by toggling on any `"` while the tokenizer had moved to field-start-only, so the two disagreed about where a quoted region begins: it "found" 40 repairs on DataPlane, 15 on GCAT_SATCAT and 7 on GCAT_ORGS that do not exist, while missing a real 54-line runaway on the CVM register (§8). With the scanner mirroring the tokenizer's rule, all of those read 0 and the record counts are unchanged. Pinned by `test_csv` |
| `lib/hpengine.c` `hp_json_flat` | records of 140–280 KB exhausted the 2,048-property flatten budget BEFORE the declared `id_keys` was reached, so the engine fell back to the first scalar — a dimension label identical across records — and they collapsed at the sink | seed the declared id/title/date/link/detail keys straight from the record, independent of that budget | IE_CSO_COLLECTION 12 datasets in 3 rows → 13,039 emitted / 13,039 stored, 0 collisions |
| `lib/hpengine.c` `key_env` gate | a row whose credential was unset returned 0 with only a stderr line: `fetch_log` `ok`, records=0, green status — indistinguishable from "spent a request and found nothing". The same defect audit #29 fixed for twelve hand-written collectors, in the other copy of the code | emit the one `collector-status-notice` shape (`_credential_notice.inc`); still rc 0, still no request | OPENEI_UTILITY_RATES `records=0 stored=1 notices=1`, constant uid ending `\|collector-needs-credential`, `request_spent:false`, no observation fields; hptest case 6 rewritten to pin the new contract |
| `core/content_change.c` | the evidence uid cut the watched ref at `%.180s`, so two refs sharing a 180-character prefix — ordinary URLs differing in a late query parameter — produced ONE uid and the second capture overwrote the first (known-issues #28a) | hash a key past 180 characters into the uid; keys at or under the cut keep their exact old uid, so nothing already stored is re-keyed | builds at 0 warnings; no re-keying of existing rows |

## 3. Catalogue ordering, tree-wide

Offset paging over an unstable default order re-serves some records and never
serves others; the collision count only shows the re-served half.

| family | URLs | fix | evidence |
| --- | --- | --- | --- |
| OpenDataSoft catalogue | 552 in 25 files | `order_by=dataset_id` | Paris 473 distinct of 490 → 490; public.opendatasoft 415 → 418 |
| ArcGIS Hub search | 27 in 10 files | `sortBy=properties.created` (v1), `sort=name` (v3) | geo.wa.gov 563 distinct of 743 → 743; Cook County 313 → 355 |
| CKAN `package_search` | 1,415 in 88 files | `sort=id asc` | 40/40 sampled portals accept it with identical totals; Virginia (batch 29) 28,943 → 32,458 |
| d-portal | 35 in 24 files | page size 100 → 5,000 | af-dportal-act-et 22,309/22,307 (2 byte-identical) in 5 pages |

The CKAN edit renamed a duplicate endpoint the lint baseline already carried
(`eas-yokohama-od-recent` fetched the same catalogue walk as
`jpx-data-city-yokohama-ckan`); fixed by giving the "most recently updated" row
the order its name promises (`metadata_modified desc`), not by rewriting the
baseline. CKAN page size was NOT raised tree-wide: a portal whose `rows_max` is
lower answers a short page, which the walk reads as the end of data.

## 4. Per-row repairs

Main session (selected): parlch council 15/3 → 15/15; Stortinget sittings
155/99 → 155/155; pbdb strata 10,000 (truncated) → 29,617; UBA columnar tables
0 → 12/491/17/6; HK minibus routes 0 → 143/176/268; Sejm ELI full archive
0 → 52k+ acts each; Rada MPs 469/463 → 469/469; Elexon DISBSAD frozen window →
rolling day, 5/1 → 52/53; IFRC appeal documents 4,000 (truncated) → 7,621;
Caltrans CMS signs D12 67/1 → 67/67; Europarl adopted texts 1,000/706 →
5,461 (per-year walk; the API has no stable order); BC CKAN 2,000 → 3,357.

Fix agents (ledgers kept outside the repo, measured per row):

| agent | scope | fixed | already fixed | legit dedupe | gated | dead | empty | other |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | long tail from triage | 37 | — | 7 | 20 | 5 | — | 5 not reached |
| 2 | new sweep failures + 4 detail rows | 18 | 17 | 2 | 1 | 8 | 22 | 1 not reached |
| 3 (paused by the rate limit, resumed) | older live failures (413) | 54 | 62 | 94 | 142 | 41 | 15 | 5 still open |
| 5 | resumed 3/4 + still-failing batch | 108 | 91 | 65 | 224 | 128 | 40 | 21 not reached, 3 moved |

New shared helpers from the agents: `collectors/sources/_vjson_idkeys.inc`
(composite uid shim for the generated fleet), `_vjson_shapes.inc`,
`_ncbi_esummary.inc`, `od_oecd_qna.c`. The account session limit hit around
01:45: agent 4 died before writing anything, agent 3 paused and later resumed to
completion (it and agent 5 briefly worked overlapping rows; agent 3 stopped
editing those and only measured them). A clean rebuild after the limit passed
every gate, so no half-applied edit survived, and every agent edit predates the
final build's sync.

Agent 3 highlights: DataCite ×12 (the API's next link drops `sort`; a hook
restores it), HAL ×7 0 → 2,000 each (path `response.docs`), NCBI ×13 via an
esummary hop (`_ncbi_esummary.inc`; run serially — parallel runs trip NCBI's
rate limit), Japan Search ×4 50 → 1,000 (`from=` offsets sorted by
`common.id:asc`), Open-Meteo flood ×6 0 → 92 (columnar daily block split per
day), `gov-federalregister-docs` 1,834 → 2,000, `gov-es-datos-catalog` now 4,000
real datasets instead of JSON fragments.

Reading agent 3's verdicts: its 24 "upstream-dead" were grouped by run-log text,
not checked one by one (11 were 404s, 11 connection or 5xx failures that may be
transient) — re-run before retiring any. Two of them were later repaired by agent
5: `faa-class-airspace` (0 → 6,045; slow ArcGIS pages, not dead) and the Shiga
police CSVs (files moved under the same ids). Its 66 "not reached" are
unfinished, not verdicts: 43 never examined (mostly RSS rows emitting 0 and
hand-written Japanese scrapers), 4 bot-walled hosts that now answer the engine
agent with 200 but were not re-run, 2 that exceed the 220 s sweep timeout
(`sci-rcsb-holdings-current` ≈ 260,000 records, `eco-cbs-nl-cpi`), and 17 of its
own collision rows measured but not settled (FireHOL, ThreatFox and the
cybercrime tracker within 1–9 records of their distinct-line counts; the NB.no
catalogue loses about 5 % to reordering while paged, which a tiebreak sort only
partly fixes).

## 5. Batch 29 — 342 new sources

| beat | rows | evidence |
| --- | --- | --- |
| webcams | 147 | `docs/verified-sources-batch29.a.md` |
| forgotten portals | 51 | same |
| deepweb | 54 | `docs/verified-sources-batch29.b.md` |
| govosint | 90 | same |

All 342 passed probe + `--check-filter` (0 FILTER_IGNORED), exclusions against
the built registry, pagination and reachability audits, emit and store. Batch A
emitted 137,871 and stored 137,865 (Honolulu and Colombia CCTV repeat rows byte
for byte). Batch B's 113 scheduled rows each match their upstream's live total
(WA lobbyist reports 201,548; Hawaii candidate expenditures 165,116); pivots
were run by hand with real entities. Dropped with reasons in the rejects
files: IBI 511 (re-serves page 1 past the end), Taiwan TDX (key), SEC IAPD /
FINRA BrokerCheck / 360Giving / OpenParliament / EP declarations (overlapping
pages no sort fixes), Democracy Club (429).

## 6. Left for a decision

- **Socrata walking.** 417 distinct Socrata `$limit` URLs now disclose that they
  read one page. Walking them to the 20-page ceiling ≈ +2.9 M records per pass
  and ~20× their requests (exhaustion ≈ 490 M records). A data-volume decision,
  not a bug fix — not done.
- **Credentials.** 45 sources honestly emit nothing until a key is configured
  (37 keys; ESTAT_APP_ID and SHODAN_API_KEY unlock 4 each).
- **`ccs-projects` and house rule 1.** `collectors/sources/ccs_projects.c`
  fetches the JOGMEC CCS page, counts how many of nine Japanese project names
  appear, and when at least five do, emits all nine projects with coordinates,
  operator lists, region and storage type taken from a table compiled into the
  binary. The records already disclose it (`data_origin:
  "static_table_name_verified_upstream"`, `geo_precision:
  "approximate_project_area"`), but only the names are live — rule 1 forbids
  hardcoded content emitted as findings. Retire it, keep it disclosed, or give
  it a real parser: a decision, not changed here.
- **Rows that emit nothing by design** — `jcg-patrol`, `nowphas-wave`,
  `niconico-ranking` (and `drone-nofly`, found earlier): retire or rewrite.
- **`sas-bmkg-adm-11` (`cuaca.bmkg.go.id`) is User-Agent gated** — 403 to any
  non-browser client, from the binary and from a plain probe alike. No UA entry
  was added: the house rule forbids impersonation, and the 12 existing
  `UA_OVERRIDE` entries are honest product//contact strings, not browser
  disguises. Retire the row or leave it honestly failing — a decision.
- **The Socrata / ODS id-path family (2026-09-16).** `catalog/v1` puts the
  four-by-four id at `resource.id`, which needs `VJSON_IDKEYS` (dotted path);
  `VJSON_KEYED` takes top-level names only. Four rows were repaired this pass.
  **Counted here: 31 of 35 `us-socrata-search-*` rows are still on plain
  `VJSON`** — but not every one of them is a `catalog/v1` search, and a broad
  grep for ODS-shaped ids returns 219 rows, not the 39 a fix agent scoped to. The
  repair is one line per row; the blast radius is NOT established. Sweeping the
  family means checking each row's endpoint first — worth doing, not worth doing
  blind, and not done here.
- **Two rows that may be an engine defect rather than a dead upstream.**
  `lat-br-tse-ckan` fails inside the binary in 103 ms while a plain `urllib` GET
  returns 200 with a well-formed `result.results` — too fast for a network
  timeout, so it smells like the HTTP layer. `sci-cryo-geus-dataverse` (binary
  times out at 75 s, probe returns 200) may be the same class. Both are still
  recorded as upstream failures, which may be a lie; confirming means changing
  `core/`, so it is flagged rather than guessed at.
- **Hosts refusing every agent** (57 sources: Tel Aviv GIS 571, open2ch, loc.gov,
  IMF, CISA/FEMA/ICE fingerprinting libcurl) stay honestly failing — no spoofing.
- **Engine notes not changed:** fixed 20–25 s timeouts in VJSON/`jo_get` (raising
  them trades throughput on every dead host — a tuning decision, not a fix);
  zip-only distributions unreachable; RSS 1.0 item-as-sibling shape; probe-only
  ports emitting 0 by design (jcg-patrol, nowphas-wave, ccs-projects);
  `lib/geojson.c` NATIVE_ID_KEYS ranks `stop_id` before `id`; IOC sea-level
  duplicate station rows differ only in rolling stats (kept on `sensorid`);
  the hpengine walk has no repeated-page check (TAGINFO counts its final page
  twice, 14,526 emitted / 14,426 stored); the in-page collision guard cannot see
  id-less records whose titles recur on a LATER page; neither page walker knows
  `from=` as an offset; `rss_collect` caps at 500 items (disclosed, but about
  half of the PACER ctd/prd feeds go unread per run); the `csv_comment` sweep asked for here is DONE:
  372 rows declare none, 8 of their files open with a `#` line, 6 of those
  already declared it, and the two that did not (`TEAM_CYMRU_FULLBOGONS`,
  `PISTAR_FCS_HOSTS`) now do; hpengine's field loss on very large JSON items
  (140–280 KB) is FIXED — declared id/title/date/link keys are now resolved
  independently of the flatten budget, so `IE_CSO_COLLECTION` emits 13,039 and
  stores 13,039 where it collapsed 12 datasets into 3 rows; there is no batched
  emit for a very large single-document list (`sci-rcsb-holdings-current`, a
  260,000-id list, times out even at 1,500 s).

  **A root cause was asserted here on 2026-09-16 and it was wrong.** This
  document said the cost was `core/intel.c` `emit()` opening a `BEGIN`/`COMMIT`
  per record — "~260,000 transactions" — and proposed a run-level transaction.
  That came from reading the code, not from measuring it. Measured 2026-09-19
  with a harness that drives the real sink (5,000 records, scratch database):

  | path | per record |
  | --- | --- |
  | full `emit()` | 989.5 µs |
  | upsert + `BEGIN`/`COMMIT` each | 43.8 µs |
  | upsert inside ONE transaction | 42.9 µs |
  | **transaction cost** | **0.8 µs — 0.08% of the total** |
  | `fts_segment` ×9 (ASCII) | 0.4 µs |
  | `fts_write` | 383 µs, of which JSON flattening is 0.2 µs |

  The transaction is free because `core/db.c` already runs WAL with
  `synchronous=NORMAL`, so a commit never fsyncs. Batching it would have bought
  0.08%, in exchange for partial-run durability and two documented invariants
  (`alert_eval.h`, `simhash.h`: "AFTER the commit, never inside it"). **It was
  not implemented.** Cost per record is also FLAT in corpus size — 1,039 / 1,091
  / 967 / 1,094 / 1,050 µs at 4k / 8k / 12k / 16k / 20k rows — so there is no
  index-growth problem either. At ~1 ms/record, 259,747 records is ~260 s
  against a 1,500 s timeout, which means **the timeout is mostly not in the sink
  at all** and the collector side is where to look next. Still open.

## 7. Final re-measure

Clean rebuild of the merged tree (all sessions' agents finished; every edit
predates the sync at 08:39:54), `rm -rf obj bin`, 2026-09-15 08:40:

| gate | result |
| --- | --- |
| `make` | 0 warnings |
| registry | 16,709 sources (16,367 + 342 batch 29) |
| `make selftest` | PASS |
| `make unit` | exit 0, 0 failures |
| `make hptest` | all passed (incl. 9f-ter, 9f-quater, 9f-quinquies) |
| `make lint-sources` | OK |
| `make audit-sources` | strict set 0 findings; whole tree 0 findings |
| `make pagewalktest` | all passed |
| `make source-floor` | 16,709 ≥ 16,271 |

Re-measure of every source whose definition changed this session (git diff of
`native/collectors` mapped to its enclosing source id, plus every fix ledger and
both batch-29 id lists): 4,388 ids, 4,386 registered, 4,305 scheduled and
re-measured on that exact binary; 81 pivot rows skipped (verified by hand with
real entities by the agents that changed them).

Result (08:42 → 10:37), 4,305 scheduled sources on the final binary:

| verdict | sources |
| --- | --- |
| OK | 3,569 |
| EMITS_NOTHING | 481 |
| COLLISION | 207 |
| SLOW | 48 |

Emitted 4,571,621, stored 4,523,166 across the set. Against the pre-fix sweep:
EMITS_NOTHING → OK 221, COLLISION → OK 134, new batch-29 rows → OK 286
(of 308 scheduled; 18 SLOW, 3 COLLISION, 1 EMITS_NOTHING); unchanged failures
EMITS_NOTHING 463, COLLISION 166.

**All 37 "regressions" (OK before, failing now) were run down individually:**

- **16 d-portal rows → COLLISION**: page size 50/100 → 5,000 took them from one
  page to the whole upstream (e.g. Kenya 101 → 26,310 stored, Tanzania
  101 → 18,229, DR Congo 51 → 16,284). Each loses 1–12 records; Ethiopia,
  Tanzania and South Africa were fetched in full and every repeated activity id
  is byte-identical (Tanzania 18,238 rows / 18,226 byte-distinct; South Africa
  13,092 / 13,082). Correct dedupe, large gain.
- **6 → EMITS_NOTHING were transient**: re-run on the final binary,
  gov-federalregister-agencies 472/472, gov-federalregister-pi 121/121,
  sci-doaj-articles-latest 1,000, de-vbb-transport-rest-locations 3,
  LEMMY_COMMUNITIES_TTRPG_NETWORK 58/58 all emit normally.
- **3 → EMITS_NOTHING are upstream**: sci-cryo-geus-dataverse,
  us-epa-echo-detailed-facility-report and lat-cemaden-rss fail identically on
  the pre-fix binary.
- **2 were real regressions from this session's VJSON_KEYED cursor seeding, both
  fixed**:
  - `eur-cbs-datasets` 100 → "fetch failed": CBS's OData service answers
    `?$top=100` and returns HTTP 500 to ANY `$skip`, `$skip=0` included. A seeded
    walk whose first fetch fails now falls back to the URL as written.
  - `eur-brreg-enheter`: seeding wrote `page=1`, but Spring Data pages are
    0-based, so the newest 100 entities (page 0) were never read. Seeding is now
    restricted to offset families (a record offset starts at 0 everywhere; page
    numbers do not). The row states `page=0` and a tiebreak
    `sort=organisasjonsnummer,asc` — with the date-only sort 20 pages held 1,784
    distinct entities of 2,000, with the tiebreak 2,000 of 2,000.
- **`eco-ooni-measurements` 51 → 840 of 1,000**: now walks; the collisions are
  a live firehose shifting offsets between pages (newer measurements push rows
  down), which re-serves rows rather than skipping them.
- **Small (1–10 records)**: us-socrata-search-{air-monitoring,
  environmental-violations,calls-for-service}, gr-diavgeia-decision-search,
  us-courtlistener-search-opinions (now 260, was 180), cyb-grip-events,
  AP19_NZ_{AKL,WELLINGTON}_HUB, UK_GAZETTE_EDINBURGH — not individually proven;
  left recorded here. `eco-odre-eco2mix-regional` was checked: 2,000 records
  walked with and without an explicit `order_by` are 2,000 of 2,000 distinct
  both ways, so its 2 collisions were real-time data changing between pages,
  not an ordering defect.

Verification of the CBS / Brønnøysund / seeding fixes on a final clean rebuild
(10:44; 16,709 sources; selftest PASS, unit 0 failures, hptest, lint-sources OK,
audit strict 0 and whole tree 0, pagewalktest, source-floor all pass):

| source | result |
| --- | --- |
| eur-cbs-datasets | "seeded cursor rejected by the upstream; walking the URL as written" → 100 records + full-last-page notice (102 stored) |
| eur-brreg-enheter | 2,000 emitted / 2,001 stored (every entity distinct, page 0 included; + ceiling notice) |
| af-dportal-act-et | 22,298 emitted / 22,296 stored — seeding still walks offset families |
| gov-federalregister-agencies | 472 / 472 |
| eco-ooni-measurements | was 1,000 emitted / 643 stored (a live firehose shifted the offsets between pages). Now `until={{today}}` + `order_by=measurement_start_time&order=desc` + `limit=1000`: 20,000 emitted / 20,001 stored, 0 collisions, ceiling disclosed (measured: 5 pages live 969 distinct of 1,000, fixed window 1,000 of 1,000) |

Follow-up fixes from the review of this document, on a further clean rebuild
(11:22; 16,709 sources; selftest PASS, unit 0 failures, hptest, lint-sources OK,
audit strict 0 and whole tree 0, pagewalktest, source-floor all pass):

| source | result |
| --- | --- |
| sci-ncbi-bioproject-recent, sci-ncbi-biosample-recent, sci-ncbi-genome-arabidopsis (run simultaneously) | 2,000/2,001, 2,000/2,001, 50/51 — the 400 ms `eutils.ncbi.nlm.nih.gov` gap |
| gnews-jp-world, gnews-jp-nation, gnews-jp-business (run simultaneously) | 70/70 each — the news.google.com gap now actually applies |
| JO28_CAA_KIKEN | 62/62 after the `href_must` change (its pattern had already been fixed per row, so this proves no regression rather than the recovery) |

Agent 3's last fixes, on a final clean rebuild of the merged tree (18:46; 16,709
sources; selftest PASS, unit 0 failures, hptest, lint-sources OK, audit strict 0
and whole tree 0, pagewalktest, source-floor all pass):

| source | before | after |
| --- | --- | --- |
| FIREHOL_LEVEL2 | `#` banner lines stored as records, colliding | `csv_comment = "#"`: 18,402 emitted / 18,402 stored |
| THREATFOX_CSV_RECENT | same | 9,615 / 9,615 |
| EPA_AIRNOW_STYLE_AQS | filtered on a CDC category that does not exist: 0 | real category: 21 / 21 |
| WORDPRESS_PLUGIN_REGISTRY | collisions | keyed on `slug`: 1,000 / 1,001 (+ ceiling notice) |

Agent 3's 5 still open — 2026-09-16, three of the five are now closed:

| row | state |
| --- | --- |
| `IE_CSO_COLLECTION` | **FIXED.** The flatten budget was exhausted by one 140–280 KB field before the declared `id_keys` was read; declared keys are now seeded independently of it. 13,039 emitted / 13,039 stored, 0 collisions (was 12 datasets in 3 rows) |
| `THREATVIEW_C2` | **FIXED, and it was not the parse discard it looked like.** `lib/csv.c` opened a quoted field on ANY `"`, not only at a field's start, so the file's whole quote parity was load-bearing and one mid-field quote swallowed every record after it. 503 / 496 → 1,173 / 1,165, the row's own live count. The unterminated-quote recovery written first fires 0 times here — it is a net, not the fix |
| `sci-rcsb-holdings-current` | **STILL OPEN — and the root cause this table gave on 2026-09-16 was wrong.** It blamed per-record `BEGIN`/`COMMIT` in `core/intel.c`; measured on 2026-09-19 that costs 0.8 µs of 989.5 µs per record (0.08%), because WAL + `synchronous=NORMAL` means a commit never fsyncs. ~1 ms/record × 259,747 ≈ 260 s, well inside the 1,500 s timeout, so the sink is not the problem. See §6 for the measurements |
| `eco-cbs-nl-cpi` | still times out at 1,500 s; not investigated |
| `classifieds` | still 9 unverified collisions |

## 9. Third pass — 2026-09-19/20

**Three claims made during this pass were wrong, and are corrected here rather
than quietly dropped**, because each was believed long enough to act on:

1. *"All 20 Socrata hosts are down (Tyler outage)."* False. Fetched directly:
   `data.calgary.ca`, `data.cityofchicago.org`, `data.sfgov.org`,
   `chronicdata.cdc.gov` and `data.seattle.gov` all answer **HTTP 200 with real
   records**. The claim came from a sub-agent's parting message and would have
   excused a fleet-wide failure as somebody else's outage.
2. *"The after-sweep shows −152,181 records across 33 sources."* False, and it
   was my own harness: it parsed `records=` out of a line a `grep` filter had
   already discarded for rows whose output shape differs, so missing output
   became a zero. Re-run without the filter, the same rows read **2,000 each**
   (`socrata-calgary-311`: baseline 100 → 2,000 across 20 pages).
3. *"A staged `grammars/` deletion breaks `make unit`."* False.
   `grammars/osint_analysis.schema.json` — the file the test actually reads — is
   present in both the repo and the build tree; only the unrelated
   `osint_analysis.gbnf` is staged for deletion, and `unit` passes.

The pattern in 1 and 2 is the one this document keeps recording: **a checker that
cannot see its input reports absence as a result.** It has now produced a
phantom dead source (`URLHAUS_CSV_RECENT`), a retirement that looked applied
(`rsync` never propagates deletions), an empty classification read as "no
failures" (`/tmp` wiped between WSL invocations), and this sweep. In every case
the tell was arithmetic that did not add up, not an error message.



**Six rows retired, on the operator's instruction.** `ccs-projects` (house rule
1: nine projects emitted from a table compiled into the binary, only their names
live), `jcg-patrol`, `nowphas-wave`, `niconico-ranking`, `drone-nofly` (all
emit nothing by construction) and `sas-bmkg-adm-11` (403 to every non-browser
client; no UA will be added). Their `source_registry.gen.c` overlay rows went
with them — `lint_sources.py`'s orphan check fails on a metadata row with no
implementation. Measured after: **16,703 registered** (was 16,709), none of the
six still listed, all eight gates green at 0 warnings, floor 16,271 intact.

That count is the whole story of a trap worth recording: the first attempt
reported **16,708** — one fewer, not six — and every gate still passed. `rsync
-a` carries changes but never removals, so five deleted collectors were still
sitting in the WSL build tree, still compiling, still registering; only the
edited row had propagated. Nothing failed, nothing warned. The arithmetic was
the only tell.

**The engine can now read a ZIP body, and that revives the IRS row.**
`US_IRS_EXEMPT_ORGS` — the 5,000 → 278,014 recovery CLAUDE.md cites — had been
pointing at `…/dl/FullData/data-download-pub78.txt`, which now 404s, as do the
`.txt` and `.csv` spellings of the new path: the IRS publishes Publication 78
only as a ZIP today. `lib/hpengine.c` now inflates a body beginning `PK\x03\x04`
before anything reads it as text (reusing `lib/zipread.c`, written for
gdelt-events), which also lifts the "zip-only distributions unreachable" limit
§6 has carried since the start. `.xlsx` is excluded deliberately — it IS a zip,
and `xlsx_to_csv` wants the archive. An archive holding more than the one entry
we read is disclosed as a `zip-extra-entries` shape notice rather than passed
over.

Pub 78 is also PIPE-delimited, so the row needed `csv_delim = "pipe"` in the
same change — without it every organisation would have been one unqueryable
cell, the defect §8 found in 17 other rows. Measured through the pivot path
(the only path that fetches it — the row declares `filter_query` and no
interval, so a scheduled `--run` skips it before reaching any of this):

```
[hp:US_IRS_EXEMPT_ORGS] emitted 2 of 2 available across 1 page(s)
                        [3 empty, 0 duplicate, 1419987 filtered out]
col0=306186591  col1=Annie B Fritch Tr UW FBO Lehigh Chapter of the American
                     Red Cross  col2=Saint Louis  col3=MO  col4=United States
                     col5=PF
```

**1,419,992 organisations** in the file, six named columns per record. The first
attempt at verifying this reported `records=0` in `0ms` and looked like a failed
fix; it was the wrong execution path.

**Socrata rows now walk their result sets** (agent-measured; re-verification on
the merged tree was still running when this was written). The premise in §6 was
wrong in the agent's favour: there are **571** distinct `$limit=` URLs, not 417,
and **294 already declared `page_param = "$offset"`** and were walking fine. The
genuinely single-page population was **277** — 244 generated macro rows plus 33
hand-written ones. And the defect was the same one in THREE copies, exactly the
pattern rule 4b warns about:

| file | change |
| --- | --- |
| `lib/jsonlist.c` | `$limit`→`$offset` added to `PAGERS`; `$offset` added to the cursor list |
| `lib/pagewalk.c` | `$offset` added to `PW_OFF_PARAMS` — without it `VJSON_KEYED` rows read one page even with a seeded cursor |
| `lib/geojson.c` | the `$limit`→`$offset` advance its own comment had been describing since 2026-09-15 without doing |
| `tests/pagewalk_test.c` | pins both behaviours, including still refusing to invent a cursor for a bare `$limit` |

Measured over 18 rows, fresh database each, read back from SQLite: **9,098 →
110,621 records, +101,523**. Nine rows hit the 20-page ceiling and file a
truncation notice naming `records_used`, `pages_read` and `more_pages_pending`;
rows that FINISH inside the ceiling file none, so the disclosure now separates
truncated from complete. Four control rows whose dataset fits in one page were
unchanged — no extra request, no false notice. Extrapolated (and stated as an
extrapolation): ~1.4 M records per pass across the 244 generated rows.

One honest cost, measured rather than assumed: **323 of the 571 URLs carry no
`$order`**, and an unordered offset walk drifts. Calgary overlaps 131 of 1,000
ids between windows (0 with `$order=:id`), which is why 8,000 emitted became
5,250 stored — and the key was checked before being blamed (987 distinct ids per
1,000 records, so the key is right and the rows really were re-served). That
loss is NOT silent: it lands on the run line as `UID-COLLISION` and in
`fetch_log.stored`. Adding `$order=:id` to 323 live endpoints was deliberately
NOT done unilaterally — see §6.

**Nine Kawasaki rows repointed.** Every one had a 404: the five hygiene CSVs
moved `202607` → `202608`, `CLEANING` was also RENAMED (`03kuri-ninngu` →
`03cleaning`, so it was never a date bump), and the three pharmacy rows use a
Japanese era stamp that went `R8.6` → **`R8.8`** (`R8.7` never existed). All
nine new URLs verified 200 `text/csv`, all nine predecessors 404.

**Then the repoint was thrown away and replaced by an engine feature, because a
repoint has an expiry date.** Both portals update monthly and neither offers a
stable link, so nine hand-edited URLs would have died again within weeks. A date
token was the obvious answer and would have been WRONG: `03kuri-ninngu` →
`03cleaning` is a RENAME, and no date arithmetic would have followed it.

`lib/hpengine.c` now takes two opts — `index_url` and `index_href_must`. The row
fetches the index page first, scans its anchors (the one scanner,
`lib/htmlparse.c`), takes the first href containing the discriminator, resolves
it against the index URL (RFC 3986) and uses that as the data URL. Everything
downstream is unchanged; it costs one extra request. When no href matches, the
row fails honestly — it does NOT fall back to the declared `.url`, because
falling back means silently re-fetching a stamp that is already stale, which is
the defect the opt exists to end. Pinned by two rows in `tests/hpengine_test.c`
(`T_INDEX`, `T_INDEX_MISS`), including `rc == -1` with exactly ONE request made.

Measured on all nine, each resolving its own current file through the index:

| row | records |
| --- | --- |
| `…_RIYO` | 558 | 
| `…_BIYO` | 1,763 |
| `…_CLEANING` | 533 |
| `…_BATH` | 180 |
| `…_RYOKAN` | 114 |
| `…_KOGYO` | 39 |
| `…_PHARMACY` | 666 |
| `…_TENPO` | 250 |
| `…_OROSHI` | 57 |

**4,160 records from nine rows that were all returning 404**, Japanese text
intact, including the three Shift_JIS pharmacy registers.

**And the same run exposed a defect in every CSV source, which the engine had
been reporting all along.** Six of the nine filed a shape notice reading
`title_keys "施設名称" matched 0 of 558 record(s)` — while the column was
plainly there in the stored properties. The cause: a UTF-8 BOM was being parsed
as part of the first header cell, so the column was keyed `\xEF\xBB\xBF施設名称`
and `title_keys = "施設名称"` matched nothing. `lib/csv.c` now strips a leading
BOM before skip_lines, the banner strip and the quote repair, so every mode and
every caller sees the same text; `tests/unit/test_csv.c` pins it. After: those
notices are **0**, the first field is keyed `施設名称`, and the rows are titled
by facility name (`Ｒｅｔｒｅａｔ`, `Ｆ・Ｐ薬局`) instead of by licence number.
The engine's own disclosure had been naming this defect on every run; it took
reading the notice rather than the record count to see it.

## 8. Second pass — 2026-09-16

Engine fixes from this pass are in §2 (the five new rows: the CSV tokenizer, the
unterminated-quote disclosure, declared-key seeding, the `key_env` credential
notice, and the `content_change` evidence uid). What follows is what is NOT yet
proven, so nobody mistakes it for measured.

**A disk incident voided part of a measurement run.** C: reached 0 bytes
mid-session; the ext4 inside the WSL VHDX remounted read-only while still
answering reads, so runs kept exiting 0 and reporting zero records.
The tail of a CSV regression batch (GCAT_SATCAT, DATAPLANE_TELNET) never ran and
is being re-measured. Measurements taken BEFORE the remount stand — ThreatView
1,173/1,165, FIREHOL_LEVEL2 19,193, THREATFOX_CSV_RECENT 8,768,
MALWAREBAZAAR_CSV_RECENT 1,586, TEAM_CYMRU_FULLBOGONS 3,056, PISTAR_FCS_HOSTS
3,000 — and ThreatView re-measured identically (1,173/1,165) on the clean
rebuild, which passes all eight gates at 0 warnings. The failure mode is still
worth knowing: a full disk reads as a fleet of collector regressions.

**A correction, because it is exactly the mistake this document exists to catch.**
An apparent `URLHAUS_CSV_RECENT` result of 0 records was first blamed on the disk
and then on the new tokenizer. It was neither: **there is no row with that id**.
The binary said so — `unknown source URLHAUS_CSV_RECENT` — and the run-line
filter used to collect the batch (`grep -E "run rc="`) discarded that line, so
the checker saw an empty database and reported a zero. The real row is
`URLHAUS_RECENT`, measured on the final binary at **13,149 emitted / 13,149
stored** from a 13,158-line file (the 9-line `#` banner is the difference) — the
largest quoted CSV in the regression set and no regression at all.
(Re-measured after the repair-scanner fix at 13,160 / 13,160: URLhaus publishes a
rolling window of recent URLs, so the difference is the feed moving, not the
parser. Both numbers are real; neither supersedes the other.) Two lessons,
both already this file's theme: a harness that greps for the SUCCESS line cannot
see a failure that does not print it, and a zero must be traced to a cause before
it is attributed to one.

**Fix agent 6's 58 rows — re-measured here, and 7 of its 8 fixes hold.** The
agent classified 58 rows (27 legit-dedupe, 11 already fixed by this session's
engine changes, 12 upstream-dead/gated/empty, 8 fixed); its ledger is
`/home/rayan/jo_fixagent6/ledger.tsv`. Every one of the 8 was re-run here on the
final binary against a fresh database, because a fix is believed when it is
measured twice, not when it is reported:

| row | agent reported | measured here | verdict |
| --- | --- | --- | --- |
| `eur-ods-paysdelaloire` | 213 / 213 | 213 / 213 | holds |
| `eur-ods-toulouse` | 844 / 844 | 844 / 844 | holds |
| `us-socrata-search-air-monitoring` | 751 / 751 | 751 / 750 on one pass, 751 / 751 on the next | holds — the collapse is an upstream repeat, see below |
| `us-socrata-search-broadband-access` | 557 / 557 | 557 / 557 | holds |
| `us-socrata-search-calls-for-service` | 1,749 / 1,749 | 1,749 / 1,749 | holds |
| `us-socrata-search-environmental-violations` | 245 / 245 | 245 / 245 | holds |
| `eco-odre-eco2mix-regional` | 2,000 / 2,002 | 2,000 records + 2 notices = 2,002 | holds |
| `gr-diavgeia-decision-search` | 100 / 101 | 100 records + 1 notice = 101 | holds |

`us-socrata-search-air-monitoring` needed a third and fourth measurement before
it could be called either way, and the sequence is worth recording because it is
how a moving upstream imitates a defect:

1. re-run here: 751 emitted / **750** stored, `UID-COLLISION: 1 of 751` — looks
   exactly like the rule 4b shape the agent had just claimed to fix;
2. a live sequential walk of the same endpoint, minutes later: 751 records, 751
   distinct `resource.id`, 751 distinct serialisations, **zero** repeats — which
   kills the "upstream repeats an asset" explanation that covers its siblings;
3. run again with a simultaneous walk: the engine stored **751 of 751**, every
   row carrying a distinct `resource.id` and nothing from the walk missing —
   while the WALK that time returned 751 records holding only 750 distinct ids,
   repeating `hz5s-4vin`.

So the duplicate is real but it lives in the upstream's paging window, not in the
row: `order=createdAt` has ties, and whichever pass straddles one sees that asset
twice — the engine on the first pass, the walk on the third. The collapse is
correct dedupe of an upstream repeat, the fix holds, and the id is right. The
general lesson is the one this document keeps relearning: a single measurement of
a live federated index cannot separate "we lost a record" from "they sent one
twice", and the difference is only visible by measuring the engine and the
endpoint AT THE SAME TIME.

**Fleet check of the tokenizer change.** The quote rule touches every CSV row in
the tree — 372 declare no `csv_comment` alone — so a sample beyond the rows that
motivated it was run on the final binary, chosen mechanically as the first
scheduled `HP_CSV` rows not already measured:

| row | records / stored |
| --- | --- |
| USGS_WATER_SITE_INFO | 1,955 / 1,955 |
| OPENFLIGHTS_AIRLINES | 6,162 / 6,162 |
| BINARYDEFENSE_BANLIST | 4,150 / 4,148 (see below) |
| DATAPLANE_SSH_CLIENT | 19,073 + 1 notice |
| DATAPLANE_DNSRD | 9,356 / 9,356 |
| DATAPLANE_VNC_RFB | 2,282 + 1 notice |
| DATAPLANE_SIPINVITE | 84 / 84 |
| AFRINIC_DELEGATED_STATS | 9,968 / 9,968 |
| AFRINIC_DELEGATED_EXTENDED | 19,714 / 19,714 |
| OFAC_SANCTIONED_CRYPTO_ETH | 120 / 120 |
| OFAC_SANCTIONED_CRYPTO_BTC | 532 / 532 |
| OSV_ECOSYSTEMS | 46 / 46 |
| GCAT_SATCAT | 70,000 + 1 notice |
| DATAPLANE_TELNET | 46,983 + 1 notice |

No row lost records to the parse, none returned zero, and none became an unknown
id. The rows storing one MORE than they emitted were disclosure notices — but
those particular notices later proved to be FALSE repairs, reported by a scanner
that had drifted out of step with the tokenizer (see the `BR_CVM_FUNDS` entry
below). Re-measured with that fixed, `DATAPLANE_SSH_CLIENT`,
`DATAPLANE_VNC_RFB`, `DATAPLANE_TELNET` and `GCAT_SATCAT` all carry **0
notices** with their record counts unchanged. The counts in this table stand as
measured; the notice column is the part that was wrong. With `URLHAUS_RECENT` at 13,149 / 13,149 from a
13,158-line file, the change is exercised across comma-, pipe-, tab- and
whitespace-delimited feeds from 46 to 70,000 records.

`BINARYDEFENSE_BANLIST` is the sample's only collision (2 of 4,150) and it is the
upstream's own: the published file holds 4,150 data lines and **4,148 distinct**,
with `137.184.105.1` listed three times. Correct dedupe, checked rather than
assumed — repeating the air-monitoring mistake here would have been easy. (The
first probe of that file returned 7 lines and looked like a dead feed; `curl`
without `-L` had stopped at a 301 that the engine follows.)

**The fleet check then found a defect of its own: GCAT.** `GCAT_SATCAT` (70,000
records) and `GCAT_ORGS` (4,110) declare `mode = HP_CSV` on a **`.tsv`** URL with
no `csv_delim` and no `csv_comment`. Both run green and have done so for weeks.
What they actually store, read back out of the database 2026-09-16:

```
title      : organisation EARTH	EARTH	EARTH	AP	C	-	-	Earth	Earth	…
properties : { "#Code\tUCode\tStateCode\tType\tClass\t…" : "EARTH\tEARTH\tEARTH\tAP\tC\t…" }
```

One column, named after the entire header line, holding the entire 17-column
record as a single unqueryable string — and 42 columns the same way for
`SATCAT`. The banner's own `# Updated 2026 Sep 15 1939:34` line is stored as a
record too, titled `organisation # Updated …`. Nothing is lost to the sink, so
every count-based gate passes: `records=`, `stored=`, the audit scanners and the
source floor are all blind to it, because the loss is INSIDE the cell. This is
the `csv_delim` trap CLAUDE.md names ("the whole line lands as one unqueryable
cell"), still live in two rows.

The repair is `csv_delim = "tab"` plus real `id_keys`/`title_keys` — and a
`csv_comment` of **`"# Updated"`, not `"#"`**, which is the part worth recording.
These files open with TWO hash lines: the real header (`#JCAT…`, 42 fields) and a
timestamp (1 field). `hp_run_csv` strips EVERY line matching the prefix and
promotes nothing, so `"#"` would delete the header as well, and the parser —
taking row 0 as column names — would then name all 42 columns after satellite
`S00001` and swallow that record. A plausible one-word opt would have quietly
cost a record and every column name. Measured: `#JCAT` is unique across all
70,000 rows, `#Code` across all 4,110.

Measured after, on a clean rebuild (0 warnings, `lint-sources` and
`audit-sources` OK):

| row | before | after |
| --- | --- | --- |
| `GCAT_ORGS` | 4,110 rows, one of them the `# Updated` banner line; every record a single cell holding 17 columns | 4,109 records, **19 named fields** (`#Code`, `UCode`, `StateCode`, `Type`, `Class`, `TStart`, …), titled `Earth`, **0** banner rows. (It first reported 1 notice disclosing 7 "repaired rows"; those were false positives from the drifted repair scanner described below, and re-measure as **0 notices / 4,109 records**.) |
| `GCAT_SATCAT` | 70,000 rows, one of them the banner; every record a single cell holding 42 columns | 69,999 records, **36 named fields** (`#JCAT` = `S00001`, `Satcat`, `Launch_Tag`, `Piece`, `Type`, `Name`), titled `8K71PS No. M1-10 Stage 2`, **0** banner rows; run time 387 s → 108 s. (Its 15 "repaired rows" were the same false positives; it re-measures as **0 notices / 69,999 records**.) |

The record count barely moves — 70,000 → 69,999 — and that is exactly the point.
Every count-based gate was already green; what changed is that 42 columns per
satellite are now addressable instead of sitting in one string, the banner line
is no longer stored as a satellite, and the row is 3.6× faster because it is no
longer building 70,000 single-cell records. A sampled record carries 36 fields
against a 42-column header (trailing empty cells do not become fields) — worth
confirming if a consumer ever needs a fixed schema from this row.

**And GCAT was not alone: the whole RIR delegated-stats family had it too.** A
scan of every `HP_CSV` row with no `csv_delim` and a static URL (258 rows,
fetched and inspected) found six more publishing pipe-separated files and
parsing them on commas: `AFRINIC_DELEGATED_STATS` (9,968 records),
`AFRINIC_DELEGATED_EXTENDED` (19,714), `RIPE_DELEGATED_STATS`,
`RIPENCC_DELEGATED_STATS`, `ARIN_DELEGATED_STATS`, `LACNIC_DELEGATED_STATS` —
together the public map of who holds which address space, every record stored as
one string like `afrinic|ZA|asn|1228|1|19910301|allocated`.

**The repair could not be `csv_delim` alone, and the reason is worth keeping.**
All six already declare `csv_no_header = 1`, so their records key on `col0` — and
while the whole line sat in `col0` that key was unique, because the LINE is
unique. Splitting on the pipe makes `col0` the registry name, identical on every
record: adding the delimiter by itself would have collapsed a 19,714-record file
onto ONE uid. So each row takes `csv_delim = "pipe"` together with
`id_keys = "col0+col1+col2+col3"` (registry+cc+type+start, unique per delegated
resource) and `title_keys = "col2+col3"`. A one-opt "obvious" fix would have
destroyed the source while every probe and lint still passed. Before-numbers were taken on the pre-fix binary first, because that baseline
cannot be recovered once the fixed declarations are compiled in:

| row | before | what a record actually held |
| --- | --- | --- |
| `AFRINIC_DELEGATED_STATS` | 9,968 / 9,968 | 1 data field |
| `AFRINIC_DELEGATED_EXTENDED` | 19,714 / 19,714 | 1 data field |
| `RIPE_DELEGATED_STATS` | 167,110 / 167,110 (157 s) | `col0` = `2\|ripencc\|1789509599\|167106\|19700101\|20260915\|+0200` |
| `RIPENCC_DELEGATED_STATS` | 260,605 / 260,605 (307 s) | same shape |
| `ARIN_DELEGATED_STATS` | 202,687 / 202,687 (260 s) | same shape |
| `LACNIC_DELEGATED_STATS` | 97,228 / 97,228 (129 s) | same shape |

**757,312 records across the family, every one of them a single string.** The
scale is the argument: this is the largest single body of data in the tree that
is stored but not usable, and nothing in the run line, the sweep verdicts, the
lint or the audit scanners could see it — `records=`, `stored=` and the uid were
all perfectly healthy precisely BECAUSE the whole line was the key. The first
record of each file is the format's version line, stored as a finding titled
`resource-delegation 2|ripencc|…`, which is the visible corner of the same
problem.

Measured after, on a clean rebuild that passes all eight gates at 0 warnings:

| row | before | after | fields |
| --- | --- | --- | --- |
| `AFRINIC_DELEGATED_STATS` | 9,968 / 9,968 | **9,968 / 9,968** | 1 → 9 |
| `AFRINIC_DELEGATED_EXTENDED` | 19,714 / 19,714 | **19,714 / 19,714** | 1 → 9 |
| `RIPE_DELEGATED_STATS` | 167,110 / 167,110 | **167,110 / 167,110** | 1 → 9 |
| `RIPENCC_DELEGATED_STATS` | 260,605 / 260,605 | **260,605 / 260,605** | 1 → 9 |
| `ARIN_DELEGATED_STATS` | 202,687 / 202,687 | **202,687 / 202,687** | 1 → 9 |
| `LACNIC_DELEGATED_STATS` | 97,228 / 97,228 | **97,228 / 97,228** | 1 → 9 |

**Six for six, not one record lost, and 757,312 records that were single strings
are now nine addressable fields each** — registry, country, resource type, start
value, count, date, status and opaque holder id. `stored` equals `records` on
every row, which is the specific thing that had to be proven: the composite key
`col0+col1+col2+col3` really is unique per delegated resource, so splitting the
line did not collapse the file the way `csv_delim` alone would have.

The before and after counts being IDENTICAL is the point worth keeping. No
count-based check — not `records=`, not `stored=`, not the sweep verdicts, not
`audit-sources` — could have detected this defect or can now confirm its repair.
The only evidence that anything changed is the shape of a stored record, which
is why this document quotes one.

The scan is a **lower bound**, not a clean bill of health: ~24 of the 258 rows
answered my bare `urllib` probe with an HTTP error (FIREHOL_LEVEL2 among them,
which the engine itself fetches fine at 19,193 records), so they are unverified
rather than clean. That re-probe is now done, with an honest
client string (`curl/8.5.0` — a real client name, not a browser disguise), and
it splits those rows cleanly:

* **Three more rows flatten.** `PISTAR_DCS_HOSTS`, `PISTAR_DEXTRA_HOSTS` and
  `PISTAR_DPLUS_HOSTS` are tab-separated with no `csv_delim` — 40 of 40 sampled
  lines carry tabs and none carry a comma. Their sibling `PISTAR_FCS_HOSTS` is
  genuinely single-column and needs nothing, which is why a blanket "add
  csv_delim to the family" would have been wrong.
* **Twelve answer 404 or 503 to a plain GET**: the nine `JP25_JPBIZ_KAWASAKI_*`
  rows, `US_IRS_EXEMPT_ORGS`, `MALTRAIL_COBALTSTRIKE` and
  `CA_SEMA_SANCTIONS_CSV` return 404; `IGRA2_STATION_LIST` returns 503. A 404
  under an honest client is not the same signal as the earlier refusals — those
  were the probe's fault, this looks like the endpoint. `US_IRS_EXEMPT_ORGS`
  matters most, because CLAUDE.md cites it as the 5,000 → 278,014 recovery. NOT
  yet confirmed against the engine's own HTTP client, which sends different
  headers and follows redirects, so they are flagged rather than declared dead.
* The remainder parse as comma-separated or single-column and are fine.

**Then the scan itself turned out to be the weak link.** Its separator list was
tab, pipe and multi-space — so it cleared `PISTAR_FCS_HOSTS` as "comma/1-col ok"
when that file is `FCS00100;Repeater;FCS001 - Repeater;;;`, six SEMICOLON fields
on all 3,001 lines. A scan is only as wide as its separator list, and mine had a
hole in it. Re-run across 250 rows with semicolon included, it found five more:

| row | shape as measured | what the fix had to be |
| --- | --- | --- |
| `BR_ANS_OPERADORAS` | header of 20 cells, quoted values, 1,110 rows | `csv_delim="semi"`; `REGISTRO_OPERADORA` distinct on all 1,110 |
| `HU_OMSZ_STATIONS` | header of 9 cells, blank-padded, 348 rows | `csv_delim="semi"` + **composite** key: `StationNumber` is distinct on only 342 of 348 (a station has one line per measurement PERIOD), so `StationNumber+StartDate` |
| `HU_KSH_TRANSPORT` | 7 cells, 25 lines | `semi` + `csv_skip_lines=1` (a TITLE line sits above the header) + `charset="ISO-8859-2"` |
| `HU_KSH_EDUCATION` | 18 cells, 68 lines | same three; `Tanév` unique per row |
| `BR_CVM_FUNDS` | header of 41 cells, **46,806** rows, 17.9 MB | `semi` + a five-part composite — see below |

Two of those are worth spelling out.

**The Hungarian rows had three defects stacked, each hiding the next.** Wrong
delimiter, a title line above the real header, and ISO-8859-2 text that was being
stored as mojibake (`Év` as `\xC9v`) because `hp_body_to_utf8` only transcodes a
row that DECLARES a charset or lives on a `.jp` host. Fixing the delimiter alone
would have produced neatly separated garbage.

**`BR_CVM_FUNDS` is the one where the obvious fix would have destroyed data.**
`TP_FUNDO` is its first column and holds 27 distinct values across 46,806 rows —
so adding `csv_delim` WITHOUT `id_keys` would have handed the engine's
first-scalar fallback a 27-way key and collapsed the whole register on the next
run. And no declared key is unique: `CNPJ_FUNDO` 41,103 of 46,806,
`CNPJ_FUNDO+DT_REG` 45,714, the best five-part composite 46,376 — while all
46,806 whole lines differ, which is why the flattened form was accidentally
lossless. A fund legitimately recurs with successive classes and situations. The
430 rows that still share the best key should be separated by the in-page
content-hash guard, since this row is a single page; that is a claim about the
engine, so the fix stands or falls on the run storing 46,806.

That makes **17 rows** in the delimiter family: 2 GCAT, 6 RIR, 4 Pi-Star, and
these 5. Every one of them was green on every gate the whole time.

Baselines for the nine newest were taken on the pre-fix binary before any
rebuild, for the same reason as the RIR family — the comparison is
unrecoverable afterwards:

| row | before, measured |
| --- | --- |
| `PISTAR_DEXTRA_HOSTS` | 910 records for 906 reflectors — the four `#` banner lines stored as findings, and the row's single column NAMED `# DExtra_Hosts.txt downloaded from…`, i.e. line 0 read as the column header |
| `PISTAR_DCS_HOSTS` | 1,800 records for 1,796 reflectors, same shape |
| `PISTAR_DPLUS_HOSTS` | 1,506 records for 1,502 reflectors, same shape |
| `PISTAR_FCS_HOSTS` | 3,000 records, each holding a whole `FCS00100;Repeater;FCS001 - Repeater;;;` line in one cell |
| `BR_ANS_OPERADORAS` | 1,110 records, one column named `REGISTRO_OPERADORA;CNPJ;RAZAO_SOCIAL;…` holding the entire operator line as its value |
| `BR_CVM_FUNDS` | 46,806 records + 1 notice, 167 s; one column named `TP_FUNDO;CNPJ_FUNDO;DENOM_SO…` holding each whole fund line |
| `HU_OMSZ_STATIONS` | 348 records, one column named `StationNumber;StartDate; End…` holding `        13704; 19950422;2004…` |
| `HU_KSH_TRANSPORT` | 24 records |
| `HU_KSH_EDUCATION` | 67 records |

`BR_CVM_FUNDS`'s 46,806 is the number the composite key has to reproduce; the
flattened form is accidentally lossless, so anything lower means the declared key
is discarding funds and the in-page guard did not cover them.

One anomaly to resolve on the after-run rather than guess at: both `HU_KSH` rows
report their record counts but yield NO sampled record when queried for a
non-notice row, with zero notices recorded — which points at a NULL stored
`record_type` despite the rows declaring `statistical-series`. That is either a
side effect of the mojibake or something separate, and it is not what the
delimiter fix is aimed at.

Measured after, on a clean rebuild passing all eight gates at 0 warnings:

| row | before | after | fields | first record now reads |
| --- | --- | --- | --- | --- |
| `PISTAR_DEXTRA_HOSTS` | 910 | **906** | 1 → 2 | `XRF000` / `201.62.48.60` |
| `PISTAR_DCS_HOSTS` | 1,800 | **1,796** | 1 → 2 | `DCS000` / `201.62.48.60` |
| `PISTAR_DPLUS_HOSTS` | 1,506 | **1,502** | 1 → 2 | `REF000` / `201.62.48.60` |
| `PISTAR_FCS_HOSTS` | 3,000 | **3,001** | 1 → 3 | `FCS00100` / `Repeater` / `FCS001 - Repeater` |
| `BR_ANS_OPERADORAS` | 1,110 | **1,110** | 1 → 17 | titled `18 DE JULHO ADMINISTRADORA DE BENEFÍCIOS LTDA`, keyed `REGISTRO_OPERADORA` |
| `HU_OMSZ_STATIONS` | 348 | **348** | 1 → 9 | titled `Sopron Kuruc-domb`, `StationNumber`+`StartDate` |
| `HU_KSH_TRANSPORT` | 24 | **23** | 1 → 7 | `Év` = `1990`, `Áruszállítás ezer tonna` = `230 112` |
| `HU_KSH_EDUCATION` | 67 | **66** | 1 → 18 | `Tanév` = `1960/1961`, `Óvodás gyermek` = `184` |

Every count move is explained and intended:

* **The three D-STAR rows drop by exactly 4** — the `#` banner lines that were
  being stored as reflectors. `titles containing '#'` is now **0** on all four
  Pi-Star rows, where it was the banner text before.
* **`PISTAR_FCS_HOSTS` GAINS one** (3,000 → 3,001). Its first room was being
  consumed as a header row; with `csv_no_header=1` it is a record again. The
  3,001 store distinctly, so the `col0+col1+col2` composite separated the
  duplicated designator that a bare `col0` would have dropped.
* **The two Hungarian rows drop by exactly 1** — the title line above the header,
  no longer a "statistical series". And the mojibake is gone: columns now read
  `Év`, `Áruszállítás árutonna-kilométer, millió`, `Tanév`, `Óvodás gyermek`.
  The NULL `record_type` anomaly noted above also resolved — both rows now store
  `statistical-series` on every record — so it was a symptom of the same
  mis-parse rather than a separate defect.
* **`BR_ANS_OPERADORAS` holds at 1,110** while going from one string to 17 named
  columns, titled by company name instead of by a semicolon-joined line.

### `BR_CVM_FUNDS`, and a defect this session introduced

The ninth row did not match its baseline: **46,752 records against a pre-fix
46,806**, with `stored == records`, no collision, one `csv-quote-repaired`
notice, and every gate green. 54 funds short, and nothing in the run line said
so.

Three explanations were tested and killed in order, which is worth recording
because two of them were mine and both were wrong:

1. *"The baseline was naive — a real parser joins quoted cells that span
   lines."* Python's `csv` on the same bytes: **46,806 rows, all exactly 41
   fields, zero embedded newlines.** Dead.
2. *"My tokenizer's trim rule opens a quote after leading blanks, where the RFC
   would not."* The file contains **no `; "` sequence at all**, and simulating
   both variants gave 46,807 lines either way. Dead.
3. *"A NUL byte truncates the parse"* — `csv_parse_x` takes a NUL-terminated
   string, which would silently drop the remainder of any CSV containing one.
   The file has **zero** NUL bytes. Dead (and worth knowing the engine is
   exposed to it if a feed ever ships one).

Running `lib/csv.c` in isolation on those bytes separated parse from emit and
gave the exact accounting:

```
record #16047: fields=59  newlines_inside=54
46,807 physical − 1 header − 54 absorbed = 46,752 = parsed_records
```

One record had swallowed the 54 lines after it. **The cause was a drift I
created earlier in this same session.** When `csv_rows` was changed so a quote
opens a field only at that field's START, `csv_repair_unterminated` was left
toggling on any `"`. The pre-pass and the parser it protects then paired the
file's 97 quotes differently: the scanner saw no over-long span, "repaired" one
harmless spot, and reported `repairs=1` — which reads as reassurance — while the
parser ran a field-start quote for 54 lines. Two readers of one rule, exactly the
failure CLAUDE.md §4c describes, introduced by the fix for a different bug in the
same file.

The repair scanner now mirrors the tokenizer's field-start rule (it takes the
delimiter and the ws/trim flags it previously was not given). After, on a clean
rebuild passing all eight gates at 0 warnings:

| check | result |
| --- | --- |
| parser in isolation | **46,806 records, 0 repairs, 0 lines absorbed** |
| `BR_CVM_FUNDS` | **46,806 emitted / 46,806 stored**, 0 notices — the target exactly, so the five-part composite plus the in-page content-hash guard really do separate all 46,806 |
| `THREATVIEW_C2` | 1,173 / 1,165 — unchanged |
| `GCAT_SATCAT` | 69,999, **0 notices** (its 15 "repairs" were false) |
| `GCAT_ORGS` | 4,109, **0 notices** (its 7 were false) |
| `DATAPLANE_TELNET` | 46,954, **0 notices** (its 40 were false). The upstream file at that moment held exactly **46,954** non-comment lines, all distinct — the row now matches its source line for line. (It read 46,983 forty minutes earlier; that feed churns hourly, and the difference is the feed, not the parser.) |

Two lessons, both uncomfortable. First, the repair counter was reporting
confidence in the exact runs where it was blind — a disclosure that lies is worse
than no disclosure. Second, the only reason this surfaced is that
`BR_CVM_FUNDS` had a **pre-fix baseline to contradict**: had the row been fixed
without measuring it first, 46,752 would have looked like a perfectly healthy
number forever.

The Pi-Star numbers are the tidiest illustration in this document of why a
record COUNT proves nothing: 910 is larger than the 906 reflectors that exist,
and the four extra "findings" are the file's own download banner — a row that
over-reports and under-delivers at the same time, on every gate, for weeks.

**Two family-wide patterns it names — counted here, and the counts disagree.**
Measured on the tree 2026-09-16: of 35 `us-socrata-search-*` rows, **31 are on
plain `VJSON` and 4 on `VJSON_IDKEYS`** — the agent said "20 more still carry the
defect". The gap matters because not every Socrata row is a `catalog/v1` search
(only those put the four-by-four id at `resource.id`, which needs a dotted path
`VJSON_IDKEYS` rather than top-level `VJSON_KEYED`). A broad grep for ODS-shaped
ids returns 219 rows, not the 39 the agent scoped to. **So: the pattern is real
and the repair is one line per row, but the blast radius is unestablished — each
row needs its endpoint checked before anyone edits 31 or 219 of them.**

**Two engine suspicions, not investigated.** `lat-br-tse-ckan` fails inside the
binary in 103 ms while a plain `urllib` GET returns 200 with a well-formed
`result.results` — too fast to be a network timeout, so it looks like an
HTTP-layer defect rather than a dead source. `sci-cryo-geus-dataverse` (binary
times out at 75 s, probe returns 200) may be the same class. Both are still
recorded as upstream failures, which may be wrong.

**One row is genuinely User-Agent gated:** `sas-bmkg-adm-11` (`cuaca.bmkg.go.id`)
answers 403 to any non-browser client. No UA was added — the house rule forbids
impersonation, and this is a decision, not a repair. The other failures in that
set are not UA-fixable (502, 503, 429 and connect timeouts), and a UA entry would
only mask them.

## 10. Fourth pass — 2026-09-21

### The proof-of-life gate could not read a large part of what it judged

Two independent defects in the same tool, both of the family §9 named: *a checker
that cannot see its input reports absence as a result*. Neither produced a wrong
PASS/FAIL often enough to be noticed; both corrupted the numbers underneath.

**`probe_hp_batch.py` decoded every body as UTF-8.** `raw.decode("utf-8",
"replace")` was the only decode in the file, so a row's declared `charset` was
never consulted and neither was the `.jp` host gate `lib/feedlib.c` applies. Any
Shift_JIS or EUC-JP endpoint was therefore judged against U+FFFD soup: a
declared `title_keys=市区町丁` could not match a cp932 header, and the resulting
`KEY_MISSING` meant "the prober cannot read this file", not "the row is wrong".
That silently covered the legacy-encoded half of the JP registry. There is now a
`decode_body()` that mirrors the C rather than inventing a second policy —
declared charset wins (the encoding is a property of the endpoint, not its TLD);
otherwise `.jp` + not-valid-UTF-8 is read as Shift_JIS and **fails closed**, so a
`.jp` host serving something else is returned verbatim instead of becoming kanji;
and a leading BOM is stripped as `lib/csv.c` does. Verified on all five
behaviours, including `Zürich` on a `.de` host surviving unconverted and
`user.jp@evil.com` resolving to evil.com.

**`verify_feeds.py:143` counted only the first 200,000 characters.** `count_csv`
did `head = text[:200000]`, parsed that, and returned its length as the record
count. Every CSV larger than that was under-reported in proportion to how far
past the cut it ran. It was found by arithmetic, not by an error: the probe
reported 2,471 records for a file measured at 19,358 lines, 4,013 for one at
5,490, and 2,558 for one at 28,831 — and 2,471 × 130 B/line ≈ 2,558 × 125 ≈
4,013 × 80 ≈ **320 KB every time**, which is what 200,000 characters of mixed
ASCII and Japanese weighs. `R8.1.csv` settles it exactly: 295,542 bytes, 2,356
counted, 3,156 real. The dialect is still sniffed from a 4 KB head — that is what
sniffing is for — but the count now runs over the whole body, which `fetch()`
already bounds at `MAXBYTES`.

Both tools are shared by every beat, so counts recorded before this date are
floors rather than measurements wherever a CSV ran past 200 KB.

### Five live sources retired over a missing `.lg`

`docs/rejected-sources-batch28.jppolice.tsv` recorded five Tokyo Metropolitan
Police URLs as "Unreachable — DNS resolution failure (getaddrinfo ENOTFOUND)".
The observation was true and the conclusion was not: the host written in that
file, `www.keishicho.metro.tokyo.jp`, really is NXDOMAIN, but the site is served
from `www.keishicho.metro.tokyo.lg.jp`. One missing token retired the publisher
of ~344,000 rows of chōme-level crime data. All five re-probed at HTTP 200 on
2026-09-21 and the five reason cells now say so. The lesson generalises past this
row: a DNS failure is evidence about a *hostname*, and "the host does not
resolve" is only a statement about the source once the hostname has been checked.

### A defect this pass introduced

Adding `decode_body()` to `probe_hp_batch.py`, the follow-up edit replaced
`    text = raw.decode("utf-8", "replace")` with `replace_all` — and that pattern
also matched the fallback line *inside* the new helper, so `decode_body` called
itself. The smoke test caught it on the first run; it is recorded because the
lesson is that `replace_all` after inserting a helper containing the same line is
a shape that will recur.

### Batch 30 `tokyocrime` — 82 rows, 361,030 records, 0 unexplained losses

The 警視庁 statistics section, registered as `hp30tokyo_tokyocrime.c`: 17
chōme-level offence tables (83,195 records), 56 **incident-level** crime files
(255,718) and 9 precursor-incident files (22,117). Full account in
`docs/verified-sources-batch30.tokyocrime.md`, per-row results in
`docs/verified-sources-batch30.tokyocrime.tsv`.

Measured through the real binary: **emitted 361,030, stored 360,559, 471 lost,
zero unexplained.** Every loss was matched against a duplicate measured in the
file itself. The beat is a clean demonstration of both halves of the collision
guard: `R7.csv` and `R5.csv` repeat a village with all 37 columns equal and
store it once (real dedupe), while `R8.csv` repeats `西多摩郡檜原村` with
**totals 8 and 10** and stores 5,156 of 5,156 with no `UID-COLLISION` line at
all — same key, different records, both kept.

That last row corrected a prediction of mine, and the reason is worth keeping:
I forecast `stored` from *distinct key* for the aggregate files and from
*distinct whole record* for the incident files. Only the second is what the
guard keys on, so the aggregate forecasts were lower bounds — the 65 incident
and precursor predictions held exactly, and the 17 aggregate ones came in four
high in total. Predicting `stored` means predicting distinct RECORDS, not
distinct keys.

Two shapes needed engine features that already existed and had not been
exercised together: `R8.3.csv` is published in a spreadsheet layout (banner,
ward line and a three-row merged header on lines 1–7), which needs
`csv_skip_lines=7` **with** `csv_no_header=1` — skipping alone would have eaten
the first real record as the column names, since the skip happens before the
header is read. And `R8.5.csv` is the one UTF-8-with-BOM file among seventeen
Shift_JIS ones, which is what exercised the `lib/csv.c` BOM strip end to end.

### `fra-grade-crossing-inventory` — a sort clause that never reached the wire

Found while re-measuring FRA with a timeout that fits it: the run emitted
400,000 and stored **366,341**, `UID-COLLISION: 33659 of 400000`.

The identity was not the problem. `crossingid` is unique 1,000 of 1,000 within
a page. The problem is that `FRA_URL` — the registered `.url`, the one a reader
inspects — carries `&$order=:id`, while `run()` composed its **own** paging URL
with `snprintf` and omitted it. The clause documented the source without ever
being sent. A Socrata `$offset` walk without a stable sort lets the server order
differently between requests, so consecutive windows overlap: measured
2026-09-21, pages 0 and 1 of this resource share **80 crossingids without
`$order` and 0 with it**.

The second defect was hiding behind the first. `$select=count(1)` returns
**438,835** rows, where the file's own header comment guessed "well over
200,000" — so `FRA_MAX_PAGES 400` × 1,000 could not reach the end of the table
even with a correct walk, and the guard bit at exactly 400,000 and fired its
truncation notice. Both are now fixed (`$order=:id` in the loop, guard raised to
600 with the measured count recorded), and the notice firing is what made the
ceiling legible rather than silent.

The generalisation: **a constant that documents a URL is not the URL that gets
requested.** Anywhere a collector has both a `#define` for its endpoint and a
`snprintf` that rebuilds one, the two can drift, and the paged one is the one
that matters.

### The same repair does NOT generalise to the rest of the tree — measured

FRA made an obvious next move look compelling: find every Socrata row without
`$order=:id` and fix it the same way. That move is wrong, and it is recorded
here as a negative result so nobody spends an afternoon re-deriving it.

Counting first, because two of my own intermediate numbers were wrong and both
are the kind that survive into a commit message. The tree holds **588** Socrata
`/resource/*.json` URLs:

| | rows | |
|---|---|---|
| `$order=:id` pinned | 473 | already repaired |
| `$order=<another column>` | 21 | ordered by `created_date DESC` and friends — a weaker guarantee (ties break arbitrarily), not an absent one |
| no `$order` at all | 94 | the only candidate class |

An earlier pass of mine reported "117 lacking `$order`" by grepping for the
literal `order=:id` and counting every row sorted by a different column as
defective. That number is wrong; ignore it if it appears anywhere.

Of the 94, drift can only bite a row that actually **pages** — a single
`$limit=200` fetch has no second window to disagree with. Checked per ROW rather
than per file (file-level paging counts answer a different question:
`hp3b21_vehiclereg.c` has 18 no-`$order` URLs and one paging marker in the whole
file), that leaves **17 paged rows** — 10 in `hp3b19_latam.c`, 6 in
`hp3b19_health.c`, 1 in `hp3b19_defence.c` — and 20 single-fetch rows that
cannot drift by construction.

**All 17 were then measured on their own endpoints, and none drifts.** Two
adjacent pages, with and without the clause:

```
CDC_NWSS_WASTEWATER_CONCENTRATION  no-order overlap=0   with-order overlap=0
HHS_HOSPITAL_CAPACITY_FACILITY     no-order overlap=0   with-order overlap=0
CO_REGALIAS_EJECUCION              no-order overlap=0   with-order overlap=0
…17 of 17 identical
```

against FRA's 80-of-1000 overlap on the same test. **Drift is a property of the
dataset, not of the missing clause** — so `$order=:id` is worth adding where a
walk is measured to drift, and is a no-op edit everywhere else.

The second finding is the useful one. **10 of those 17 declare no `id_keys` at
all** and fall back to the engine's `ID_FALLBACK` chain, which is precisely what
the collision backlog's own note says about most of its rows: *"the row's
identity is wrong"*. FRA was the opposite shape — identity correct
(`crossingid` unique 1,000 of 1,000), walk broken. Assuming one shape transfers
to the other is what produced the wrong hypothesis here.

### The collision backlog is mostly pre-repair, and must be re-measured first

`docs/source-audit-2026-09-04-collisions.tsv` records **882 rows losing 205,617
records per pass**, which reads as a live backlog and is not one. Splitting it:

* **144 rows (12,742 records/pass)** carry the exact integer page-repeat
  signature of issue 54 (`emitted == stored × N`);
* **738 rows (192,875 records/pass)** have non-integer ratios — ordinary wrong
  identity, which is 94% of the loss and the unglamorous half.

Of the 331 rows that could be mapped to a source file (the rest are generated
`VJSON` rows that do not use the `.id = "` form), **78,128 of 134,347
records/pass sit in files that now carry `$order=:id`** — repaired since the
artifact was written, and therefore counted here as loss that no longer happens.
Two named examples: `NY_LOBBYIST_REGISTRATIONS` (recorded loss 3,285) has
carried `$limit=1000&$select=*,:id&$order=:id` for some time, with the measured
before/after written into the file above it; `stats-pt-ine-indicator`, named in
issue 54 as fetching the same page 57 times, already reads `OK 19,608/19,608` in
the 08-25 health sweep.

So the artifact is a starting list, not a work queue, and any figure quoted from
it — including the headline 205,617 — is an upper bound on work already partly
done.

### The re-measure, and what is actually left

The worst 60 rows by recorded loss (156,577 records/pass on paper) were re-run
through the real binary. **OK=47, COLLISION=12, EMITS_NOTHING=1.**

Most of the backlog had already been repaired and nobody had re-measured it:

```
OR_STATE_SALARIES               10000/2443  ->  10000/10001
PORTWATCH_DAILY_REGIONAL        10000/1995  ->  10000/10001
NY_LOBBYIST_REGISTRATIONS       10000/6715  ->  10000/10001
HHS_HOSPITAL_CAPACITY_FACILITY  10000/6173  ->  10000/10001
FINRA_OTC_BLOCKS                 2000/476   ->   2000/2001
geo-nve-flood-warning            1077/1     ->   1077/1077
geo-tidesandcurrents-currents    4430/2785  ->   4430/4430
…plus ten WHO_XMART_* rows, all 10000/9xxx -> 10000/10001
```

(The `stored = emitted + 1` shape is normal — the extra row is the run's own
disclosure record. It is also a good way to break a naive classifier: mine
computed `lost = emitted - stored`, got −1, and filed 36 healthy rows as
"unmeasured" until the tool's own tally contradicted it.)

**Of the 33,225 records/pass still being lost across those 12 rows, most is not
loss at all.** `gr-diavgeia-positions` accounts for 24,985 of it — 25,218
records in, 233 stored — and it is correct. Its endpoint publishes exactly two
fields, and `uid` has **233 distinct values** across all 25,218 records while
`label` has 231. No `id_keys` could store more, because no field distinguishes
more: it is a position lookup table the publisher repeats, and the VJSON block's
own description already said "233 distinct official post titles". Likewise
`AP19_TW_TWSE_AP11`'s 671 is the case CLAUDE.md documents as genuine duplicate
triples. Netting those out leaves roughly **7,500 records/pass** of plausibly
real loss, concentrated in `sci-exoarchive-stellarhosts`, the three IOC
sea-level registrations (one upstream registered three times — an issue-52 case
as much as a 4b one), and the two `awc-*` rows.

`audit_registry_emit`'s note is the reason this pass produced so few edits, and
it deserves to be quoted:

> EITHER the declared id is a dimension, not a record key (real loss — fix
> id_keys) OR the upstream genuinely repeats identical records (correct dedupe —
> not a bug). This tool cannot tell which: fetch the url and compare distinct id
> values to record count before treating it as a defect.

Every time that was actually done this pass, the answer came back "not a
defect": the 17 no-`$order` rows, `gr-diavgeia-positions`, and four of the five
TWSE rows keyed on a bare `公司代號` (`AP06CI` 1048/1048, `AP08` 12/12, `AP12`
2/2, `AP14` 1083/1083 — all genuinely unique on it).

**Two real findings did survive.** `OR_OLCC_LIQUOR_LICENSES` now emits nothing
because its upstream is empty — `srxe-qkm2` answers `$select=count(1)` with 0,
and the same-named catalogue entries (`ibd5-n4kt`, `4kai-6t56`) return 403
"no row or column access to non-tabular tables", so there is no live tabular
resource to repoint to. The engine reports it honestly as `0 of 0 available
[1 empty]`; it is an upstream retirement, not a defect, and inventing a repoint
would be fabricating a source.

And `AP19_TW_TWSE_AP09` declared `id_keys = "公司代號"` for a dataset that **does
not publish that field at all** (its columns are 公司名稱, 出表日期, 百分比), so
every record resolved to the same empty key and 9 stored as 1. Repaired to
`公司名稱+出表日期`, measured 9 of 9 distinct. It never appeared in the backlog
TSV — at 8 records per pass it ranks nowhere — which is worth noting: the
ranked list finds big losses, not wrong rows.
