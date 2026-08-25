# Batch 21 — 102 rows across eight under-covered beats

Every row below was fetched over the wire on this machine, parsed in its
declared mode, run through the **real binary**, read back as
`emitted N of M available`, and then read back a second time out of the
database to prove that what was emitted was also **stored**. Nothing here was
authored from documentation.

Manifests (the source of truth) — `docs/candidate-sources-batch21.{austrialaw,
delaw,envreg,foodhygiene,healthpay,medreg,providerreg,vehiclereg}.txt`.
Generated tables — `native/collectors/pivot/table/hp3b21_*.c`. Edit the
manifest. Rejects are kept as data in `docs/rejected-sources-batch21.tsv`
(118 rows).

## What shipped

| beat | rows | what it is | why it was under-covered |
| --- | --- | --- | --- |
| `envreg` | 23 | the whole SDWIS drinking-water model as a **PWSID pivot** — a public water system, its wells and treatment plants, its Safe Drinking Water Act violations, the enforcement actions answering them and its lead-and-copper sampling — plus the TRI child tables behind a reporting form, the PCS/NPDES permit and inspection history, and the FRS industry cross-references | `data.epa.gov/efservice` was in the tree only as FRS and TRI facility lookups and a handful of `rows/0:0` shape probes. Not one drinking-water table was reachable, and no EPA row anywhere in the tree took a PWSID |
| `vehiclereg` | 19 | the Dutch national vehicle register (RDW) keyed on a **licence plate** — the vehicle, its fuel and emission class, axles and axle loads, bodywork, European class and subcategory, status particulars, every inspection notification and observed defect, its test expiry, and its live recall status | the tree already fetched 24 RDW Socrata resources and **every one was a static `?$limit=1000` head-of-table read**. The main vehicle table (m9d7-ebf2) was absent in any form, and no row anywhere could answer a question about a specific vehicle. RDW is the only European national vehicle register that answers a plate anonymously, without a key |
| `healthpay` | 16 | CMS Open Payments as a **named-physician pivot** — general, research and ownership payments from drug and device manufacturers, the physician identity and taxonomy tables, the per-physician yearly totals with the disputed-transaction counts, and the reporting-entity and teaching-hospital profiles | `openpaymentsdata.cms.gov` was in the tree as the bare `/datastore/query/` base, the metastore item list and one 2021 dataset. Seven years of ownership data — which doctors hold an investment interest in which manufacturer — were unreachable |
| `austrialaw` | 15 | the Austrian Rechtsinformationssystem: the Supreme Administrative Court, the Federal Administrative Court, the nine provincial administrative courts, the Equal Treatment Commission, the federal disciplinary commissions, the staff-representation supervisory commission, the norm index, the authentic and historical Federal Law Gazettes back to 1780, government bills, consultation drafts, and consolidated and gazetted provincial law | the tree carried six RIS endpoints — bundesrecht and landesrecht by title, and the Judikatur applications Justiz, Vfgh and Dsk. Every other register the API serves was missing, and each `Applikation` is a separate register with its own schema, not a view of the same data |
| `providerreg` | 10 | the CMS Provider Data Catalog as an entity pivot — the Medicare clinician directory by surname, the clinician-to-facility affiliation edge table, and nursing-home **ownership**, penalties, health and fire-safety deficiencies, survey dates and survey summaries by CMS certification number | `data.cms.gov/provider-data` was two static `?limit=500` head-of-table reads out of a 236-dataset catalogue. Professional licensing and facility inspection records were not queryable by anything |
| `foodhygiene` | 9 | the UK Food Standards Agency Food Hygiene Rating Scheme — the register of the 363 inspecting authorities, ~600,000 inspected establishments by address, by inspecting authority and by business sector, a standing nationwide list of every zero-rated premises, and the score-descriptor and business-type key sets | `api.ratings.food.gov.uk` appeared in the tree exactly once, as a name search. Every enumeration path was unreachable — and the whole API 404s without an `x-api-version` header, so a discovery pass reads it as dead |
| `medreg` | 6 | Health Canada's non-drug registers — the Medical Devices Active Licence Listing by licence name, by holder and by device, the device-company register in full, and the approved label claim for every licensed natural health product | all fourteen `health-products.canada.ca` endpoints in the tree are the Drug Product Database. The device register — the non-FDA equivalent of the openFDA device endpoints the tree does carry — and the natural health product database were both unreachable |
| `delaw` | 4 | the German federal legal information portal (BMJ / DigitalService) — federal supreme court case law and federal legislation with ECLIs and ELIs, searched separately and across both | the tree carried `de.openlegaldata.io`, a volunteer scrape, and nothing from the German state's own service. This is the official replacement for gesetze-im-internet.de and rechtsprechung-im-internet.de |

`lint-sources` counts `REGISTER_SOURCE` only, so these 102 `HP_REGISTER_TABLE`
rows do not move the number it prints. The built binary's own seed count is the
real one.

## Gate results

| gate | result |
| --- | --- |
| `tools/probe_hp_batch.py` | **102 of 102 PASS.** Five EPA rows needed a serial retry — this host had intermittent `Network is unreachable` episodes throughout, unrelated to the endpoints |
| `tools/batch_exclusions.py` | **0 collisions, 0 near-misses**, checked against the whole tree with `--skip-prefix hp3b21_`. Two further duplicates it could **not** see were found by hand and removed — see below |
| `tools/audit_batch_pagination.py` | **0 of 102 flagged.** 46 rows declare `pagination_ok` with the measurement behind it |
| `tools/audit_batch_reachable.py` | **0 of 102 can never run.** Every non-pivot row declares `interval > 0` |
| `tools/audit_batch_emit.py` | **0 `DROPS_EVERYTHING`, 0 `PARTIAL`. 102 of 102 OK** (95 in a `--jobs 2` pass, the other 7 on a serial retry after the same network episode). 40,798 records emitted |
| **house rule 4b, by hand** | **102 of 102 stored exactly what they emitted.** 41,295 emitted, 41,295 stored |

### Rule 4b, per row

Each row was run against its **own** fresh scratch database
(`JO_DB=/tmp/r4b_*.db`, deleted immediately after the count) so a stored count
can only have come from that run:

```
./bin/japanosint --run <ID> <entity>
sqlite3 $JO_DB "select count(*) from intel_items where source_id='<ID>'
                and record_type <> 'collector-truncation-notice'"
```

Every one of the 102 rows came back `stored == emitted`. Twenty rows stored
one record **more** than they emitted; in every case that record is the
`collector-truncation-notice` the engine writes when the page ceiling bites,
which is house rule 2 reporting the shortfall in band and is not a finding
about the source. Verified by `record_type` breakdown, e.g.:

```
RIS_JUD_VWGH   emitted 1000 of 1000 available across 10 page(s) (TRUNCATED)
               court-decision                1000
               collector-truncation-notice      1
```

The largest single-row results: `HC_MDALL_COMPANY_ALL` 7,637 of 7,637,
`EPA_SDWIS_SYSTEM_BY_NAME` 4,894 of 4,894, `HC_LNHPD_PRODUCT_PURPOSE` 5,000 at
the ceiling, `RDW_DEFECT_CODES` 1,007 of 1,007, `DERIB_DOCUMENT_SEARCH` 965 of
965, `OPENPAY_GENERAL_2024` 852 of 852, `CMSPD_CLINICIAN_BY_NAME` 189 of 189.

## Rows written, run, measured — and then removed

107 rows were authored. Five were deleted after being measured; two more were
rewritten rather than dropped. **This is the part of the batch the gate tools
did not catch on their own.**

| row | why |
| --- | --- |
| `EPA_TRI_FORM_BY_FACILITY` (as first written) | **The filter was silently ignored.** Envirofacts answers a filter on a column the table does not have with HTTP 200 and the *unfiltered* table. `tri_reporting_form` has no `facility_name` column, so the row returned 10,000 records — the window cap — whose first record is byte-identical to the first record of the unfiltered table. It probed PASS and emit-audited OK. Rewritten to key on `tri_facility_id`, which was then verified both ways: 66 records for a real id, `[]` for a nonexistent one. **Every other Envirofacts row's filter column was re-checked the same way.** |
| `EPA_ICIS_FACILITY_BY_NAME` | removed. 16.9 MB in 26 s, capped at the 10,000-row window with no way to page past it; the `BEGINNING` form takes 36 s, past the engine's 30 s HTTP timeout; and one of four measurement fetches returned malformed JSON |
| `HC_LNHPD_MEDICINAL_INGREDIENT` | removed on **rule 4b**. The endpoint has no unique column: over 1,000 consecutive records `lnhpd_id` has 839 distinct values and every other field fewer, so any `id_keys` is a dimension. Measured: engine emitted 5,000 and stored 4,857 while the upstream itself served 4,997 distinct records — a real 2.9% loss, not dedupe |
| `DERIB_CASELAW_RECENT` | removed. The unfiltered case-law stream cannot be walked without repeating records: ten pages of 100 yield **997** distinct documents unsorted and **990** sorted by date. The same `sort=date` fix *does* work on `/v1/document` (963 distinct → 965 of 965) and is applied there; on case-law the date has too many ties |
| `DERIB_DOCUMENT_SEARCH` | kept, after `id_keys` was moved from `item.legislationIdentifier,item.documentNumber` to `item.@id` and `sort=date` was added. Before: 965 emitted, 960 stored. After: 965 of 965 |
| `RDW_VEHICLE_TRACKS` | removed as a duplicate `batch_exclusions.py` could not see. The tracked-vehicle table holds 547 rows and `vsrc13_europe_data_6.c` already reads it in full at `$limit=1000`, so the plate pivot adds nothing. **Found by counting the table, not by comparing URLs.** The three other RDW tables that overlap an existing bulk feed (2ba7-embk, 7ug8-2dtt and the recall tables) hold 559,856 and 575,707 rows against a 1,000-row static read and were kept |
| `CMSPD_HOSPITAL_GENERAL` | removed as a duplicate `batch_exclusions.py` could not see. `hp3b19_health.c`'s `CMS_HOSPITAL_GENERAL_INFORMATION` already walks the same dataset weekly with offset paging. The URLs differ only by the `conditions[…]` query parameters, which the exclusion scanner does not normalise |
| `RIS_JUD_BVWG` | kept, after `DokumenteProSeite` was reduced from `OneHundred` to `Fifty`. The Federal Administrative Court register is large enough that a 100-record page takes 26 s, past the prober's 25 s timeout; at `Fifty` it is 13 s |
| `FSA_ESTABLISHMENT_BY_AUTHORITY` | kept, after `want` was changed from `numeric` to `any`. `HP_NUMERIC` requires four digits and a local authority id is one to three, so the shape gate refused the row's own probe entity |
| `OPENPAY_RESEARCH_2024` / `_2025` | kept, after the probe entity was changed. Both probed PASS on a surname with **zero** research payments — see prober defect #1 below |

## Engine and tooling defects found

### 1. `probe_hp_batch.py` ignores the row's declared `array_path` and can PASS an empty result set

`OPENPAY_RESEARCH_2024` was probed with a surname that has no 2024 research
payments. The response is a DKAN envelope: `{"results":[], "count":0,
"query":{"properties":{…252 column definitions…}}}`. The prober auto-detects
the densest array of objects, finds the response's own **schema block**, and
reports:

```
OPENPAY_RESEARCH_2024   PASS   json:query.properties   252   200   43337
```

252 "records", zero real ones. The row declares `array_path=results` in its
opts — the prober has that field and does not use it. Two rows in this batch
would have shipped as verified-live-and-permanently-empty, which is exactly the
`EMPTY_RESULTSET` failure CLAUDE.md says was closed. They were caught only
because `audit_batch_emit.py` reported `EMPTY_UPSTREAM`.

Suggested fix: when a row declares `array_path`, resolve it and count *that*;
a declared path that resolves to an empty array is a FAIL, and a declared path
that does not resolve is a shape mismatch rather than a licence to go hunting
for any array in the document.

### 2. Nothing in the tool chain checks that a query parameter is honoured

Three separate APIs in this batch accept a filter, ignore it, and answer HTTP
200 with the unfiltered collection:

* Envirofacts, on a column the table does not have (`tri_reporting_form/
  facility_name/...` → 10,000 unrelated records);
* the German legal portal, on `court`, `documentNumber` and `dateFrom`
  (→ the unfiltered collection, `totalItems` 10000);
* Health Canada MDALL, on `licence_id` and `original_licence_no`
  (→ the entire 21 MB licence table).

Every one of those probes PASSes, and the emit audit reports `OK` with a large
record count. The row is then a pivot that returns the same wrong answer to
every question — which is not fabrication, but is indistinguishable from it
downstream. The check is cheap and mechanical: **run the probe URL a second
time with the entity replaced by a value that cannot exist and require an empty
result.** That is how all three were found here, by hand.

### 3. `batch_exclusions.py` still cannot see two classes of existing endpoint

Batch 20 reported this and it cost this batch two more rows. Neither
`CMSPD_HOSPITAL_GENERAL` (same dataset, same query API, differing only in
`conditions[…]` parameters) nor `RDW_VEHICLE_TRACKS` (same Socrata resource,
`?kenteken=` versus `?$limit=1000`) is reported as a collision or as a
near-miss. Comparing **host + path with the query stripped** as a warning would
have caught both. The tool also prints `SUPPRESSED: files named hp3b21_* were
not scanned` and `WARNING: no --bin, so the id set is a REGEX APPROXIMATION` —
both are useful and both were heeded.

### 4. The engine's 30 s HTTP timeout is not configurable per row and is not distinguishable from a dead endpoint

`EPA_ICIS_FACILITY_BY_NAME` fetches in 26–36 s depending on the operator.
Under `CURLOPT_TIMEOUT_MS` defaulting to 30000 in `core/httpclient.c` the row
alternates between working and reporting `transport failure`, which
`audit_batch_emit.py` renders as `TRANSPORT_FAIL` — the same verdict a
nonexistent host gets. Envirofacts is not the only government service in this
range. A per-row `timeout_ms` in `hp_source`, or simply reporting the elapsed
time alongside `transport failure`, would separate "slow" from "gone" the way
`--timeout` and the `SLOW` verdict already do one level up.

### 5. `audit_batch_emit.py --timeout` and the `SLOW` verdict are a real improvement

Recorded as a positive: batch 20 asked for exactly this and it now exists. It
is what let the four Open Payments rows that take 71–75 s per page be measured
rather than guessed at.

## Discovery: what was probed and rejected

118 rejects are recorded in `docs/rejected-sources-batch21.tsv`. The largest
populations:

* **21 `TABLE_NOT_AVAILABLE`** — documented Envirofacts tables that this REST
  service does not serve: `rmp_facility` (Risk Management Plan facilities),
  `sems_active_sites` (Superfund), `rcra_handler`, `icis_case` and
  `icis_enforcement_action` (federal enforcement casework), `tsca_chem`,
  `tri_submission` and eleven more. There is no machine-readable table
  catalogue either — `/efservice/metadata/JSON` is itself "not available" — so
  the only way to know is to ask for each one.
* **29 RIS 404s and HTTP-200 refusals** — the Austrian API has exactly three
  paths and selects the register with a query parameter. An invalid
  `Applikation` answers **HTTP 200** carrying a SOAP schema-validation error
  document, so twelve candidate registers written from the RIS user interface
  would have registered as live and stored a validation message as a finding.
* **11 `NO_PAGINATION_TOO_LARGE` / `HTTP_404`** across Health Canada's LNHPD —
  four of its endpoints serve a single unpaginated array of 10 MB, 45 MB, 50 MB
  and 147 MB.
* **10 `HTTP_401`** — the UK Trade Tariff API, which was open and is now behind
  a subscription. Every one of its ten endpoints answers the same 401.
* **Whole directions abandoned after probing.** Port state control (Paris MoU,
  USCG PSIX), spectrum licensing (ACMA, ISED, Anatel), mining cadastres (USGS
  MRDS serves WFS only), transport safety investigation (NTSB CAROL answers 405
  to GET and 500 to the documented POST body), New Zealand and Australian
  charity registers, and the World Bank / ADB debarment lists were all probed
  and none produced a usable open record endpoint. Those directions are thin in
  the tree because the data is *published* and very sparsely *served*.

## Build gates, quoted

From a tree rsynced fresh from the working copy and built from zero:

```
$ python3 tools/gen_hp_batch.py ../docs/candidate-sources-batch21.*.txt \
      --outdir /tmp/gen21 --prefix hp3b21 --batch 21
102 manifest rows
/tmp/gen21/hp3b21_austrialaw.c                   15 rows
/tmp/gen21/hp3b21_delaw.c                         4 rows
/tmp/gen21/hp3b21_envreg.c                       23 rows
/tmp/gen21/hp3b21_foodhygiene.c                   9 rows
/tmp/gen21/hp3b21_healthpay.c                    16 rows
/tmp/gen21/hp3b21_medreg.c                        6 rows
/tmp/gen21/hp3b21_providerreg.c                  10 rows
/tmp/gen21/hp3b21_vehiclereg.c                   19 rows
total 102 rows
(regenerated output is byte-identical to the shipped tables)

$ make
make exit=0
warnings/errors: 0

$ make audit-sources
files scanned      : 1535
files with findings: 0
findings           : 0
strict set (collectors/pivot/table/hp*_*.c): 0 finding(s)

$ make lint-sources
registered source_defs: 10564  (1114 direct + 9450 macro-expanded)
lint-sources: OK

$ make hptest
  ok    a real SDMX codelist yields one record per <str:Code>
  ok    each SDMX code is keyed on its own id= attribute
  ok    the SDMX label still comes from <com:Name>
  ok    the urn attribute is kept, not discarded
all passed

$ make unit
  ok: catalogue = 1627 of 1627 entity pivots (2652 scheduled rows excluded)
  ok: schema enum == 1627 listed services
  ok: analysis prompt within the ceiling
test_search_degradation: OK

$ ./bin/japanosint --list-sources | wc -l
13295
```

`lint-sources` prints 10,564 because it counts `REGISTER_SOURCE` only; the
built binary's own seed count moved from 13,193 to **13,295**, which is exactly
the 102 rows this batch registers through `HP_REGISTER_TABLE`.
