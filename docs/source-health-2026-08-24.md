# Registry health, 2026-08-24 — every registered source, measured

Companion artefact: `docs/source-health-2026-08-24.tsv` (one row per source:
id, collector, interval, rc, emitted, stored, bucket, note). Everything below
was measured on this machine; nothing is estimated. Where a number could not be
obtained, the row says so and is counted in a bucket that says so.

## Method, and what it does and does not cover

The source list came from the **binary** (`./bin/japanosint --list-sources`,
13,193 rows), not from a manifest, so nothing registered can hide. Each source
was run through the real binary — `--run <id> [entity]` — against **its own
fresh copy of a warmed template DB in `/dev/shm`**, deleted immediately after
its counts were read. Two numbers per run:

* **emitted** — `records=N` off the `[sched] <id> run rc=… records=N` line, i.e.
  the number of `emit()` CALLS;
* **stored** — `SELECT COUNT(*) FROM intel_items WHERE source_id=?` against that
  run's own database.

Rule 4b exists because those two are different numbers, and they are.

| pass | what it ran | jobs | timeout |
| --- | --- | --- | --- |
| sweep | all 13,193 | 64 | 180 s |
| entity-retry | 590 rows that returned nothing without an entity | 32 | 180 s |
| slow-400s | the 111 rows that timed out | 16 | 400 s |
| empty-recheck | the 539 rows that emitted nothing, re-run at low concurrency | 12 | 180 s |
| post-fix | the 188 rows whose manifest I repaired (see below) | 20 | 180 s |

The TSV names, per row, which pass its numbers came from.

**Cross-check against the reference tool.** `tools/audit_registry_emit.py`
(scheduled, `^[AB]`, first 80) gives `COLLISION=14 EMITS_NOTHING=5 OK=61`,
339,122 emitted / 331,498 stored, 7,624 lost. Comparing that slice row-by-row
against my own sweep, **75 of 80 agree exactly on both numbers**. The five that
differ are live-feed volume drift between the two runs (`AP19_SG_DATAGOVSG_*`,
`BLOCKLIST_DE_SSH`), one collision that appeared only in the later run
(`AP19_NZ_AKL_HUB` 171→157), and `ARIN_DELEGATED_STATS`, which my 180 s timeout
truncated and the tool's 220 s did not. The coordinator's own figure for that
slice was 7,667 lost; mine is 7,624 — 0.6 % apart. I treat the sweep as sound.

### Entities used for pivot rows

A row whose URL carries `{q}`/`{qd}`/… needs one. 1,044 rows do. The entity was
chosen by the row's declared `want` shape and by which tokens the URL needs, and
is recorded in the TSV per row:

`Toyota` (835 rows) · `0000320193` (110, a real 10-digit CIK) · `AS15169` (19) ·
`toyota.com` (16) · `8.8.8.8` (10) · `1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa` (7) ·
`0x00000000219ab540356cbb839cbe05303d7705fa` (6) · `info@toyota.com` (4) ·
`4b1806` (3). A second pass re-ran 590 further rows with `Toyota`.

### What I could NOT measure — stated plainly

* **15 of 13,193 sources are absent from the TSV.** Their run output contained a
  raw newline inside a record title, which broke the harness's line framing and
  the rows were dropped rather than guessed at: `AP19_TW_TWSE_AP09`,
  `CO_SECOP_CONTRACTS`, `MSRC_SUG_VULNERABILITY`, `PNCP_BR_ARMY_CONTRACTS`,
  `TPEX_PLEDGE_RATIO`, `cyb-ncsc-ie-atom`, `cyb-ubuntu-cves-json`,
  `eas-jma-overview-020000`, `gov-govinfo-bills`, `gov-govinfo-fr`,
  `misskey-timeline`, `ncsc-ie`, `sci-figshare-collections`,
  `sci-figshare-datasets`, `us-openfda-other-historicaldocument`.
  **Denominator for everything below: 13,178.**
* **62 rows never finished.** 26 of them had already stored rows (up to 75,666
  for `ua-police-stolen-phones`) when the clock ran out, so their `emitted` is
  recorded as 0 and is wrong-low; they are counted as `timeout`, not as healthy
  and not as broken. 56 rows of the 400 s re-run were lost to a scratch-directory
  collision between two concurrent harness processes and were not re-measured.
* **243 rows were not exercised at all** — on-demand pivots whose URL I could not
  recover from the C tree, so the harness had no entity to give them.
* **365 rows emitted nothing and I could not prove why**, because their endpoint
  is not recoverable from the C source (macro-built or computed URLs). They are
  bucketed `empty-unproven` and are NOT counted as honest empties.

## Bucket counts — denominator 13,178 measured of 13,193 registered

| bucket | rows | share | meaning |
| --- | ---: | ---: | --- |
| healthy | 9,843 | 74.7 % | stored ≥ emitted, both > 0 |
| fetch-failure | 1,270 | 9.6 % | HTTP refusal, transport failure, or a collector returning rc=-1 |
| **KEY_COLLISION** | **1,053** | **8.0 %** | stored < emitted — rule 4b |
| empty-unproven | 365 | 2.8 % | stored 0, endpoint not recoverable, empty unverified |
| not-exercised | 243 | 1.8 % | entity pivot the harness could not supply an entity for |
| emits-nothing-upstream-has-records | 183 | 1.4 % | stored 0 while an independent fetch of the same endpoint returned records |
| credential-gated | 98 | 0.7 % | degrades to an explicit note — house rule 1 honoured |
| timeout | 62 | 0.5 % | no answer inside the harness timeout |
| honest-empty-proven | 49 | 0.4 % | stored 0, and an independent fetch also returned 0 records |
| not-a-source | 12 | 0.1 % | `_maint` / `_enrich` / `_test` pods |

**DROPS_EVERYTHING (emitted > 0, stored 0): zero rows.** Not one source in the
registry called `emit()` and left nothing behind. That is the batch-18 failure
mode and it is closed. The loss has moved one layer down, into the sink's uid.

Across the 13,116 rows that ran to completion (i.e. excluding the 62 timeouts,
whose `emitted` was never printed), **11,450,709 records were emitted and
10,752,480 distinct rows stored** — 93.9 % kept, **698,229 records discarded per
pass**, of which 581,909 are uid collisions in rows that otherwise look perfectly
healthy. The 62 timed-out runs stored a further 833,992 rows before the clock ran
out; those are excluded from both sides rather than credited to either.

## The rule-4b losses, and where they come from

### 1. The collision guard has a blind spot the manifests walk straight into

`hp_collision_map()` flags records that collide on a page and disambiguates them
by content hash. It computes each record's key as:

```c
const char *kp = rkey ? rkey : title;
if (kp) { hp_fnv_hex(kp, k[m].key); k[m].idx = i; m++; }
```

`hp_emit_record()` does not stop there — when both are NULL it falls back to
`hp_first_scalar()` and keys on that. **The guard's pre-pass does not replicate
that fallback**, so a row that declares neither `id_keys` nor `title_keys` is
exactly the shape the guard cannot see. Every record whose first scalar is a
dimension constant then collapses onto one uid.

Measured, before any fix: `WHO_XMART_NCD_MORTALITY` emitted 10,001 and stored
**2**, both rows keyed
`WHO_XMART_NCD_MORTALITY|NCD_CANCER_MORT_PERCENT` — the indicator code, constant
across all 10,000 observations, with no content-hash suffix because the guard
never flagged them.

**1,818 of the 3,279 registered hp rows with a URL declare neither key.** That is
the population at risk; 202 of them were losing records on the day.

The second, documented limit — *"Scope is one array … a record on page 2 that
keys onto one from page 1 is not caught"* — is real but secondary: it costs a
handful of rows, whereas the NULL-key blind spot cost hundreds of thousands.
Both live in `lib/hpengine.c`, which I do not own; this is a report, not a patch.

### 2. Pagination the upstream ignores — 86 rows, 69,041 records per pass

A distinct class, easy to mistake for a key collision. `stored` is *exactly*
`emitted / N` for integer N ≥ 2, because the engine walked N pages and the server
returned page 1 every time. The records are genuinely identical, so the sink is
right to keep one — the defect is upstream of it.

| row | emitted | stored | ratio | page_param |
| --- | ---: | ---: | ---: | --- |
| `stats-pt-ine-indicator` | 19,608 | 344 | 57× | *(none)* |
| `PY_CGR_ONG_RENDICION` | 10,001 | 1,001 | 10× | `inicio` |
| `PY_CGR_FONACIDE` | 10,001 | 1,001 | 10× | `inicio` |
| `geo-nve-flood-warning` | 3,231 | 3 | 1077× | *(none)* |
| `eur-sejm-votings` | 6,424 | 3,212 | 2× | *(none)* |
| `IMF_PORTWATCH_TRADE_REGIONAL` | 4,001 | 1,005 | 4× | `resultOffset` |
| `UKPARL_LDA_CONSTITUENCIES` | 3,877 | 1,298 | 3× | `_page` |
| `unhcr-population` / `-asylum-decisions` / `-asylum-applications` | 2,000 | 1,000 | 2× | *(none)* |

The five `FAA_ARCGIS_*` rows sit here too (5,001 → 511, ~10×, `resultOffset`), as
do `IMF_PORTWATCH_PORTS` / `_AIRPORTS` / `_CHOKEPOINT_DAILY` / `_PORT_SPILLOVERS`
(10,001 → ~1,010). Every one of these spends N requests to fetch one page.

### Worst offenders by records lost per pass (post-fix state)

| row | emitted | stored | lost |
| --- | ---: | ---: | ---: |
| `sci-exoarchive-stellarhosts` | 47,857 | 5,519 | 42,338 |
| `ECDC_ERVISS_ILI_ARI_RATES` | 34,681 | 28 | 34,653 |
| `sci-exoarchive-ps` | 40,106 | 6,370 | 33,736 |
| `ECDC_ERVISS_VARIANTS` | 33,191 | 21 | 33,170 |
| `IANA_LANGUAGE_SUBTAGS` | 49,312 | 19,448 | 29,864 |
| `ofcom-wtr` | 64,208 | 39,287 | 24,921 |
| `gr-diavgeia-positions` | 25,144 | 232 | 24,912 |
| `stats-pt-ine-indicator` | 19,608 | 344 | 19,264 |
| `UK_OFSI_CONSOLIDATED_XML` | 19,761 | 5,135 | 14,626 |
| `WA_STATEWIDE_CONTRACT_SALES` | 10,001 | 20 | 9,981 |
| `IESO_GEN_OUTPUT_CAPABILITY` | 9,847 | 26 | 9,821 |
| `PY_CGR_ONG_RENDICION` | 10,001 | 1,001 | 9,000 |
| `PY_CGR_FONACIDE` | 10,001 | 1,001 | 9,000 |
| `IMF_PORTWATCH_PORT_SPILLOVERS` | 10,001 | 1,011 | 8,990 |
| `IMF_PORTWATCH_PORTS` | 10,001 | 1,011 | 8,990 |

The top six are **not** in a batch manifest — they are hand-written collectors
under `collectors/sources/` and `collectors/feed/`, which another agent owns.
They are reported, not touched.

## What I fixed

### The three corrupted rows (`\;` ate the opts separator)

| row | before | after |
| --- | --- | --- |
| `UA_NBU_EXCHANGE_SITE` | `.date_keys = "exchangedate;pagination_ok=start/end are a DATE range…"` — 106 characters of prose used as a date field name | `.date_keys = "exchangedate"`; run with entity `20240101`: `records=1 stored=1`, title `USD` |
| `UA_DREAM_PROJECTS` | `.detail_key = "id;pagination_ok=from= is not an offset…"` — a field that does not exist, so the second hop never resolved | `.detail_key = "id"`; `records=1000 stored=1000`, 25 details merged (max 96,496 B of properties), 975 stamped `_detail_pending`, **0 detail errors** |
| `WIKIMEDIA_SEARCH_PAGE` | `User-Agent: JapanOSINT-research/1.0 (+https://github.com/);pagination_ok=the Wikimedia core search API caps limit at 100 and exposes no offset (measured: limit=500 returns 0 results)` — 130 characters of prose sent to Wikimedia on every request | clean UA; `records=100 stored=100` |

A **second defect in `UA_DREAM_PROJECTS`** surfaced the moment the first was
fixed: its `detail_url` used `{q}` (the pivot entity, constant for the whole run)
where the engine substitutes the per-record value into `{v}`. Repairing
`detail_key` activated the hop and it fired 25 identical **404s**. Changing the
token to `{v}` was verified against the live endpoint
(`…/ideas/29x2er4k-8693-4r06-axn3-colyq2r4jjx5` → HTTP 200, 106,207 bytes) and
the hop now returns real detail.

That is a class, not a one-off: **37 rows across the batch manifests declare a
`detail_url` with no `{v}` in it** — 32 use `{q}`, 4 use a literal `{}` (an empty
token the engine leaves verbatim), one uses `{qU}`. Every one of them spends a
second request per record on a URL that cannot vary per record. Files:
`batch19.{eurasia,energyenv,latam,…}`. I fixed only the row I had activated;
the rest are reported.

I checked every other `\;` in the batch manifests before touching anything:
**22 occurrences, 19 of them legitimate** (an escaped semicolon inside a header
value, e.g. `Mozilla/5.0 (compatible\;JapanOSINT-research/1.0)` or
`application/json\;charset=utf-8`). Only the three named rows escaped a
SEPARATOR. `gen_hp_batch.py` now rejects that shape by name and the regenerations
passed it.

### 176 rows given a measured identity — +360,213 stored rows per pass

For the 202 collision rows that declared neither key and live in a batch
manifest, I fetched one live page of each endpoint and measured every field's
distinct-count and coverage. Where a **single field was unique across the whole
page and its name reads like an identifier**, it became `id_keys`; otherwise the
best label-shaped field became `title_keys` (which is enough on its own, because
it makes the record visible to the collision guard). Nothing was guessed: every
value written came out of a fetch on this machine.

Rejected on purpose: `DELPHI_EPIDATA_FLUVIEW` (`num_patients`) and
`NOAA_FOSS_AFSC_GROUNDFISH` (`species_code`) — unique on one page by coincidence,
a measure and a dimension, not identities. Declaring either would be the ECDC
mistake again. Five geometry-derived titles (`geometry.rings.0.2.1`,
`attributes.SHAPE_Length`, `geometry.y`, two Latvian coordinate columns) were
applied, measured, judged worse than the fallback label, and reverted.

Re-measured after regeneration and rebuild, 187 rows:

| row | stored before → after | emitted |
| --- | --- | ---: |
| `OONI_DOMAIN_INDEX` | 31 → **29,605** | 29,605 |
| `CO_XM_SIMEM_DESPACHO` | 48 → **24,192** | 24,192 |
| `LLAMA_YIELD_POOLS` | 104 → **16,931** | 16,937 |
| `D3FEND_MAPPINGS` | 1 → **16,164** | 16,164 |
| `WHO_XMART_NCD_MORTALITY` | 2 → **10,001** | 10,001 |
| `CO_RETHUS_TALENTO_SALUD` | 3 → **10,001** | 10,001 |
| `CO_ANM_REGALIAS_VOLUMEN` | 87 → **10,001** | 10,001 |
| `WHO_XMART_POPULATION_COUNT` | 195 → **10,001** | 10,001 |
| `BTS_T100_SEGMENT_CARRIER` | 120 → **9,975** | 10,001 |
| `CFS_HAZMAT_FLOWS` | 2 → **9,602** | 10,001 |
| `UKHSA_INFLUENZA_POSITIVITY_ENGLAND` | 2 → **3,651** | 3,651 |
| `CO_INVIMA_SANCIONATORIOS` | 30 → **3,789** | 3,789 |

**Net: +360,213 stored rows per pass, −897 across nine rows.** Every one of the
nine is upstream drift or a transient, not the patch: `OPENFEC_FILINGS` hit an
HTTP 429 on the second run, `COURTLISTENER_DOCKETS` a transport failure,
`BR_PNCP_*` and `DOT_TOC_INCIDENT_REPORTS` returned slightly different data.

`BR_CVM_ISSUERS` was a bonus find. Its proposed title came back as the entire
47-column header line, because the CVM publishes **semicolon-delimited** CSV and
the row declared no `csv_delim` — the whole line was landing in one unqueryable
cell. `gen_hp_batch.py` refused the generated opts, which is how it surfaced. Now
`csv_delim=semi;title_keys=DENOM_SOCIAL`, measured 2,677 records / 2,566 distinct
`CD_CVM`.

Gates after all of this: `make` 0 warnings, `make audit-sources` **0 findings
across 1,527 files**, `make lint-sources` OK, `make hptest` all passed,
`--list-sources` still 13,193.

## Fetch failures — 1,270 rows

| shape | rows |
| --- | ---: |
| collector returned rc=-1 with no HTTP status | 639 |
| HTTP 404 | 197 |
| HTTP 400 | 137 |
| transport failure (DNS/TLS/connect) | 96 |
| HTTP 403 | 79 |
| HTTP 500 / 429 / 204 | 20 / 18 / 17 |
| 2xx body that would not parse in its declared mode | 12 |
| HTTP 571 (one host's own refusal code) | 12 |
| HTTP 406 / 401 / 502 | 6 / 5 / 4 |

**25 of these answered 2xx on re-probe minutes later.** My sweep ran 64 requests
in parallel and several hosts throttled under it. Those rows carry
`host answered 2xx on re-probe; transient` in the TSV note and should not be read
as dead sources — the failure was partly mine.

### User-Agent blocks — 6 confirmed, 2 fixable inside the existing policy

I probed all 112 refusing rows three ways: the agent the engine actually sends,
no agent at all, and a browser agent.

| row | engine | none | browser | verdict |
| --- | ---: | ---: | ---: | --- |
| `OASIS_PUBLISHED_STANDARDS` | 520 | **200** | 200 | **fixable** — a no-UA or renamed-UA override works |
| `FAA_NNUMBER_REGISTRY` | 403 | **200** | 200 | **fixable** — same |
| `MT_MBR_COMPANIES` | 403 | 403 | 200 | browser-only |
| `SE_ALLABOLAG_SEARCH` | 403 | 403 | 200 | browser-only |
| `MX_PJF_SENTENCIAS` | 403 | 403 | 200 | browser-only |
| `PT_BASE_CONTRACTS` | 999 | 999 | 200 | browser-only |

The first two belong in `core/httpclient.c`'s `UA_OVERRIDE` table alongside
`data.humdata.org` — hosts `www.oasis-open.org` and `registry.faa.gov`. That file
is another agent's, so this is a handover, not a patch. The other four answer
**only** a browser string, which `httpclient.h` rules out and which I did not do.

The remaining 55 return 403 to all three agents (a real refusal, IP or geo), 12
return 571 to all three, 5 return 401.

## HTTP-200 error documents stored as findings

Three rows, all ArcGIS Feature Services, stored exactly **one** row during the
sweep whose title is a bare status code:

* `PORTWATCH_DAILY_REGIONAL` — `regional-trade-series 429`
* `PORTWATCH_MARITIME_LINKS` — `shipping-connection 429`
* `FAA_AMD_PENDING_BUILDING` — `airport-mapping-record 429`

ArcGIS answers an over-quota query with **HTTP 200** carrying
`{"error":{"code":429,…}}`, and the engine files the error code as a record. It
is a rule-1 violation: an error is being displayed as a finding. I could not
re-trigger it on demand (a 40-way burst against the same endpoint returned 40×200
with real data), so this is an intermittent that shows up under load — which is
precisely when nobody is looking. The other four rows my title scan flagged
(`ECMA-434`, `trade-statistic 466`, `cadastral-unit 505`, `well-location 421`)
are false positives: real record identifiers that happen to look like codes.

## Emits nothing while the upstream has records — 183 rows

Every one of the 539 sources that emitted nothing was **re-run at concurrency 12**
to rule out self-inflicted throttling: 518 emitted zero a second time. Their
endpoints were then fetched independently. 183 came back with records.

| row | records at the endpoint today |
| --- | ---: |
| `sci-rcsb-holdings-current` | 258,616 |
| `sci-rcsb-holdings-unreleased` | 35,157 |
| `ie-data-gov-ie-tags` | 22,700 |
| `sci-gfz-hpo-index` | 11,321 |
| `de-opendata-schleswig-holstein-de-tags` | 5,905 |
| `de-datenregister-berlin-de-tags` | 4,224 |
| `geo-opentopography-catalog` | 3,890 |
| `it-dati-toscana-it-tags` | 2,506 |
| `lv-data-gov-lv-tags` | 2,020 |
| `sci-gfz-kp-index` | 1,887 |
| `us-caltrans-lcs-d04` | 1,228 |

Spot-checked against the binary one at a time: `ie-data-gov-ie-tags` 0 records,
`us-caltrans-lcs-d04` 0 records in 10.6 s, `sci-gfz-kp-index` 0 records — all
three confirmed. Two rows that the sweep called empty (`aws-ip-ranges`,
`gcp-ip-ranges`) emitted 16,824 and 1,092 when re-run alone, so they were
transient and are counted healthy. The CKAN `*-tags` family — **19 rows** in this
bucket — is one shared defect, not nineteen: `/api/3/action/tag_list` returns a
flat array of bare strings and the collector keeps none of them.

**49 rows are proven honest empties** — stored 0, and an independent fetch of the
same endpoint also returned 0 records today. That is the only claim of
"legitimately empty" this report makes, and it covers 49 rows, not 539.

## Credential-gated — 98 rows, all compliant

Every gated source degrades to an explicit note naming the missing variable
(`SHODAN_API_KEY`, `HIBP_API_KEY`, `COURTLISTENER_API_KEY`, `SENTINELHUB_CLIENT_ID/SECRET`,
`SPACETRACK_USER/SPACETRACK_PASS`, …) rather than to a silent empty. House rule 1
is being honoured here without exception.

## Rule 3 — reachability

**513 registered rows have `interval = 0` and no entity token in their URL or POST
body**: never scheduled, never dispatchable, guaranteed to emit nothing forever.
8 of them are `internal://` registry pseudo-sources, which is by design. The rest
include bulk files that plainly want an interval — `US_IRS_EXEMPT_ORGS`,
`MX_SAT_EFOS_69B`, `CA_SEMA_SANCTIONS_CSV`, `JP_METI_END_USER_LIST`,
`FR_HATVP_DECLARATIONS`, `EU_EUROPARL_MEPS`, `HIBP_BREACH_CATALOG`,
`OURAIRPORTS_LOOKUP`, `RANSOMWHERE_PAYMENTS`. Reported, not changed: several are
in files I do not own, and an interval is a scheduling decision, not a repair.

## Per-family patterns

| collector | rows | healthy | collision | fetch-fail | emits-nothing |
| --- | ---: | ---: | ---: | ---: | ---: |
| `osint` | 4,172 | 58 % | 289 | 893 | 10 |
| `government` | 534 | 84 % | 26 | 39 | 9 |
| `academic` | 371 | 92 % | 21 | 0 | 0 |
| `europe_data` | 346 | 82 % | 58 | 2 | 1 |
| `science` | 339 | 75 % | 62 | 4 | 14 |
| `fr_opendata` | 287 | 98 % | 7 | 0 | 0 |
| `disaster` | 247 | 82 % | 19 | 6 | 5 |
| `environment` | 229 | 76 % | 20 | 13 | 10 |
| `ch_opendata` | 208 | 100 % | 1 | 0 | 0 |
| `cyber` | 202 | 70 % | 10 | 15 | 4 |
| `us_state` | 192 | 57 % | 73 | 0 | 5 |
| `transport` | 192 | 70 % | 10 | 15 | 0 |
| `jp_prefectural` | 178 | 100 % | 0 | 0 | 0 |
| `legal` | 146 | 60 % | 49 | 3 | 7 |
| `us_opendata` | 132 | 55 % | 9 | 50 | 0 |

Three things read straight off that table.

* **The generated national-portal families are the healthy ones.** `ch_opendata`
  100 %, `jp_prefectural` 100 %, `jp_local` 99 %, `fr_opendata` 98 %, `news` 97 %.
  One shape, one parser, verified once, correct everywhere.
* **`us_state` and `legal` are collision families, not failure families.** 73 of
  192 and 49 of 146 rows fetch fine and store a fraction. Both are dominated by
  Socrata/CKAN endpoints where the manifest declared no identity.
* **`osint` carries almost the whole failure load** — 893 of the 1,270 fetch
  failures. It is also the family holding every hand-written pivot, so a large
  part of that is my harness having no entity to give, not a dead endpoint.

## Handover — what is left, and who owns it

1. **`lib/hpengine.c`**: make `hp_collision_map()` use the same
   `hp_first_scalar()` fallback `hp_emit_record()` uses, so a row declaring
   neither key is visible to the guard. 1,818 registered rows are in that shape.
2. **`lib/hpengine.c`**: an HTTP-200 body that is an ArcGIS/JSON error envelope
   must not become a record.
3. **`core/httpclient.c`**: add `www.oasis-open.org` and `registry.faa.gov` to
   `UA_OVERRIDE`, measured above.
4. **CSV header BOM**: `SODIR_FIELD_RESERVES` and `IE_SEAI_WIND_FARMS` have a
   UTF-8 BOM glued to their first column name, so `title_keys=fldName` cannot
   match `﻿fldName`. Verified: declaring the clean name changed nothing.
5. **86 rows walk pages that return page 1 every time** (list above), and **37
   rows declare a `detail_url` with no `{v}`**. Both waste requests on data
   already held.
6. The six biggest single losses — `sci-exoarchive-*`, `ECDC_ERVISS_*`,
   `IANA_LANGUAGE_SUBTAGS`, `ofcom-wtr`, `gr-diavgeia-positions`,
   `stats-pt-ine-indicator` — are hand-written collectors, not manifest rows.
