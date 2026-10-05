# Batch 31: 371 government, public-record and surveillance high-penetrancy rows

Organised by *who does the recording*: the state. Three layers, ordered by how
far each sits from a company register.

1. **Government as an institution** — the legislature, the gazette, the
   rulemaking docket, the lobbying register, the procurement platform, the
   audit court. What the state decided, who asked it to, and what it bought.
2. **Public registers of things** — cadastre, vehicles, vessels, aircraft,
   spectrum, mineral tenements, concessions, plus the multilateral bodies'
   project, debarment and arbitration records.
3. **Standing surveillance** — state camera estates, sensor networks, transport
   telemetry and internet-wide measurement.

## Counts

| File | Rows | Coverage |
|---|---:|---|
| `hp3b31_usfed.c` | 25 | US federal: lobbying, FARA, campaign finance, rulemaking, research funding, banking, exclusions |
| `hp3b31_usstate.c` | 27 | 17 further Secretary of State registers + municipal record datasets (NYC, Chicago, SF, LA, Seattle) |
| `hp3b31_ukie.c` | 25 | UK procurement, Parliament APIs, regulators, land, FOI + Ireland (Oireachtas, lobbying, charities) |
| `hp3b31_eutrans.c` | 25 | EU expert groups, comitology, lobby meetings, document registers, Parliament open data, cohesion money |
| `hp3b31_eunat.c` | 25 | DE/AT/CH/FR/NL/BE/SE/NO/DK/FI/ES/PT/IT/PL/CZ parliaments, procurement, gazettes |
| `hp3b31_jpgov.c` | 26 | JP: corporate-number spine, gBizINFO five-hop, e-Gov law, kanpō, courts, licensing, procurement |
| `hp3b31_apacgov.c` | 26 | AU/NZ/KR/TW/HK/ID/TH/MY/PH/IN + Pacific islands |
| `hp3b31_afrgov.c` | 25 | ZA/NG/GH/SN/CI/KE/TZ/UG/RW/ET/BW/NA + AfricanLII, SAFLII, extractives |
| `hp3b31_latamgov.c` | 25 | BR legislature & municipal gazettes, CL lobbying act, CO/AR/PE/MX + Central America & Caribbean |
| `hp3b31_menacis.c` | 26 | Gulf procurement, IL/TR/MA/TN/EG/JO + RU/UA/BY/AM/GE/UZ |
| `hp3b31_intlbody.c` | 23 | UN, MDB projects & debarment, arbitration, FATF-style bodies, IAEA, OPCW |
| `hp3b31_assetreg.c` | 23 | Cadastre, vehicles, vessels, aircraft, spectrum, minerals, concessions |
| `hp3b31_survcam.c` | 19 | State-operated road camera inventories, NA/EU/APAC |
| `hp3b31_survsensor.c` | 15 | Air, radiation, seismic, water, fire, GNSS networks |
| `hp3b31_survtransport.c` | 17 | Rail telemetry, coastal AIS, ADS-B, transit operator registries |
| `hp3b31_survnet.c` | 19 | Host exposure, certificate transparency, censorship & outage measurement, wireless mapping |
| **total** | **371** | |

Registry: **17,469 → 17,840**, exactly +371, nothing displaced. 335 rows are
entity pivots; 36 are whole-collection feeds carrying a scheduler interval.

## It started as 400 — what happened to the other 29

This batch was authored against a tree with 2,803 sources and de-duplicated
against the ~1,200 ids a regex could see in it. Main has since reached 17,469
sources, and 29 rows turned out to duplicate something already wired:

* **4 duplicate ids** — `EMSC_SEISMIC_EVENTS` and `ES_DATOS_GOB_CATALOG`
  (already in `hp3_sigint.c` / `hp3_portals.c`), `GREYNOISE_COMMUNITY` and
  `SHODAN_INTERNETDB` (hand-written collectors in `netintel_world2.c` /
  `infra_world.c`). Dropped.
* **25 duplicate endpoints under different ids** — Amtrak, airplanes.live `/mil`,
  Singapore LTA traffic images, NSF awards, Sensor.Community, FARA registrants,
  four Digitraffic feeds, WebTRIS, data.gouv.fr, NDBC, api.weather.gov,
  CelesTrak, Caltrans D3, Earth Search STAC, SatNOGS, HHS OIG LEIE, SWPC
  alerts, SSLBL, URLhaus, Find a Tender, PACRA. Dropped.
* **1 renamed, not dropped** — `USGS_WATER_SITES` collided by id with
  `hp3_energy.c`, but the endpoints genuinely differ: that row reads `/nwis/iv/`
  (instantaneous readings), this one reads `/nwis/site/` (the site inventory).
  Renamed `USGS_NWIS_SITE_INVENTORY`.

Thirteen of those were invisible to a plain grep and were caught only by
`make lint-sources`, which resolves string macros and follows `*.inc` includes.
That is the gate to trust for de-duplication, not a hand-rolled comparison.

No replacement rows were invented to restore the round number. 371 real
non-duplicate rows is the honest count; padding it with four more unverifiable
endpoints would have made the batch worse.

## House rule 3: 46 rows would have registered and never run

46 rows are whole-collection feeds — camera inventories, sensor station lists,
blocklists, debarment lists, parliamentary document feeds — with a static URL
carrying no entity token. `hp_source.interval` defaults to 0, and 0 means
"on-demand pivot", so every one of them was registered, visible in
`/api/status`, and unreachable: the exact state CLAUDE.md records 763 rows of
batches 18-19 being in.

Each now carries an interval chosen for how fast its data moves — 600s for live
AIS, 900s for traffic cameras and outage alerts, 1800-3600s for threat feeds,
21600s for legislative and tender feeds, 86400s for stable station and register
inventories. `filter_query` is gated on the entity being present, so these rows
emit the whole collection on a scheduled run and filter client-side on a pivot.
After the duplicate drops, 36 of the 46 remain in the batch.

## Two silent-discard classes found by re-reading the house rules

Both were shipped in the first version of this batch and are fixed here. Both
fail with every record-counting gate green, which is why they are worth naming.

### `page_size` on a page-NUMBER parameter turns pages into offsets

`hpengine.c` decides the paging arithmetic as
`offset_style = (page_size > 0) || offset_named`, and then computes
`page_start + (page + 1) * step`. So declaring `page_size` on a parameter that
is a page *number* makes the engine multiply the page number by the page size:
`US_FEC_CANDIDATE_SEARCH` walked `page=1`, then `page=101`, `page=201`;
`JP_GBIZINFO_*` walked `page=1`, then `page=5001`. Pages 2-100 were never
requested, the run was green, and `records=` looked healthy.

27 rows were in that state. `page_size` is documented as "records per page, for
offset-style paging" and has no other use in the engine, so it is now set only
on offset-style rows:

* **23 page-number rows** — `page_size` removed, so the walk is
  `page_start + page + 1` (2, 3, 4 …).
* **`US_SAM_EXCLUSIONS`, `US_COLLEGE_SCORECARD`** — `page_size` removed *and*
  `page_zero_based = 1`, because both data.gov-family APIs number the first page
  0. Without it the engine coerces the start to 1, so the walk would be "server
  default (page 0), then 2, 3" and page 1 would be skipped. Declaring
  zero-based is also the safe direction if a guess is wrong: the worst case is
  one duplicate page, which the sink dedupes, never a hole.
* **`WORLDBANK_PROJECTS_API`** — `os` really is an offset, but the engine's
  `offset_named` test looks for "offset"/"start"/"skip", so `os` was coerced to
  1 and walked offsets 101, 201 — one record lost at every page boundary, the
  exact ArcGIS defect `hpengine.c` documents at line 2993. `page_zero_based = 1`
  keeps the start at 0, so it walks 0, 100, 200.
* **`CAM_OHGO_OHIO`** — `page-all` is OHGO's *return-everything flag*, not a
  cursor. Declared as `page_param` it made the engine send `page-all=501`,
  `page-all=1001`. It is now bound as `page-all=true` in the URL with no walk
  declared, so one response carries the whole inventory.

One row, `NETLAS_HOST_RESPONSES`, is still reported by
`tools/audit_page_param.py` because its URL binds its own `start=0`. That shape
is handled: `start` is offset-named so the start is read from the URL, and
`hp_url_set_param` replaces rather than appends. 408 rows tree-wide share it.

### `id_keys` naming a dimension instead of a record

No row in this batch used a comma where it meant a composite — all 133
`id_keys` were single-field — but six named a *group* rather than a record,
which collapses the set at the sink just as silently:

| row | was | now |
|---|---|---|
| `UN_COMTRADE_TRADE_FLOWS` | `period` | `period+reporterCode+partnerCode+cmdCode+flowCode` |
| `JP_MLIT_LAND_TRADE_PRICES` | `Municipality` | `Period+MunicipalityCode+DistrictName+TradePrice+Area+BuildingYear` |
| `OONI_COUNTRY_AGGREGATION` | `probe_cc` | `measurement_start_day+probe_cc+test_name` |
| `IODA_OUTAGE_ALERTS` | `entity.code` | `entity.code+datasource+time` |
| `US_NYC_PAYROLL` | `payroll_number` | `fiscal_year+agency_name+last_name+first_name+mid_init+title_description` |
| `US_CHICAGO_SALARIES` | `name` | `name+job_titles+department` |
| `US_SEATTLE_BUSINESS_LICENSES` | `customer_number` | `customer_number+trade_name+ownership_type` |

`payroll_number` is the agency's number, not the employee's, so every employee
of an agency shared one uid. Composing is safe even where a named field turns
out not to exist: the engine joins a missing part as `""` and requires only one
part present, so a composite can add distinctness but never remove it.

These repairs are reasoned from the engine's arithmetic and each upstream's
documented parameter semantics, **not** measured — see the unverified section
below. The registry sweep is what would confirm them.

## Where the penetrancy is

* **gBizINFO, five second hops off one corporate number** — subsidies,
  government contracts, certifications, patents and filed financials all hang
  off the 13-digit Japanese corporate number, including for unlisted companies
  that publish nothing on EDINET.
* **Brazil's Chamber of Deputies → deputy expense claims** — the detail hop
  returns every supplier paid out of a deputy's quota, with its CNPJ.
* **UK Commons divisions → the full aye and no lists.**
* **FEC candidates and committees → cycle totals.**
* **CQC providers → inspection ratings and every location operated.**
* **Chile's Ley del Lobby** — every minister, mayor and regulator must publish
  each meeting with an outside party: who attended, for which company, what was
  sought.
* **Querido Diário** — normalised daily gazettes from thousands of Brazilian
  municipalities.
* **Alaska's corporations database** — one of the only US registers publishing
  officials *and their percentage ownership*.
* **Czech and Spanish cadastres** — parcel ownership readable by name, free.

## Verified

| Gate | Result |
|---|---|
| `make` (`-Wall -Wextra`) | clean, no new warnings |
| `make selftest` | PASS (17,840 sources seeded) |
| `make unit` | all passed |
| `make hptest` | all passed |
| `make lint-sources` | **OK** — dup-id 0, dup-endpoint at baseline |
| `make audit-sources` | **0 findings across 1,601 files**; strict set clean, and these rows are now *inside* the gated glob |
| `make pagewalktest` | all passed |
| `make source-floor` | ok, 17,840 ≥ 16,271 |
| house rule 3 | 0 unreachable rows |
| registry delta | +371, nothing displaced |

## NOT verified — rules 4, 4b and 4d are unmeasured

The environment this batch was finished in has no outbound HTTPS, so **no row
here has been fetched over the wire.** Nothing below has been measured:

* **rule 4** — fetching is not emitting. Batch 18 was fully probe-verified and
  59 of 223 rows still stored nothing.
* **rule 4b** — emitting is not storing. A row whose `id_keys` names a group
  rather than a record reports a healthy `records=N` and stores one row.
* **rule 4d** — answering is not answering the question. A filter silently
  ignored returns the whole collection with HTTP 200 and every counting gate
  green, which is worse than returning nothing.

Treat every row as a documented **candidate** until measured. With network:

```sh
cd native
python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch31.*.txt --check-filter   # FILTER_IGNORED is fatal
python3 tools/audit_batch_emit.py ../docs/candidate-sources-batch31.*.txt \
        --bin ./bin/japanosint --jobs 6 --timeout 220
```

The command this section used to give, `audit_registry_emit.py --match hp3b31`,
measured NOTHING: `--match` is a regex on the SOURCE ID, and no batch-31 id
contains `hp3b31` (that is the file name). It selected zero rows and would have
reported a clean sweep. The other line, `probe_hp_batch.py --check-filter`, had
no manifest to read. Both are why the manifests below exist.

Retire anything returning `EMITS_NOTHING`, `COLLISION` or `FILTER_IGNORED`. An
unmeasured row is a gap in coverage, not a source of false data — the engine
only forwards fields that came back over the wire, so a moved or reshaped
endpoint yields an honest empty.

## Manifests (reconstructed 2026-10-05)

These 16 tables were hand-authored, not scaffolded by `tools/gen_hp_batch.py`,
so until 2026-10-05 there was no manifest and no manifest-driven gate
(`probe_hp_batch.py`, `audit_batch_reachable.py`, `audit_batch_pagination.py`,
`audit_batch_emit.py`) could be pointed at this batch.

`docs/candidate-sources-batch31.<beat>.txt` now exists for all 16 beats, 360
rows, **reconstructed from the C**. It is a record of the tables, not their
source — the C stays the maintained copy, and nothing should be regenerated
over it. Fidelity was proven both ways: `gen_hp_batch.py` regenerated from the
manifests yields the same 360 rows with 0 field mismatches against the
committed tables, and an independent grep confirmed every field set in the C is
present in the manifest. The `probe` column holds one concrete entity per row,
picked for what the row asks for (an IP row gets `8.8.8.8`, a domain row
`toyota.co.jp`, a Japanese-text row `トヨタ`, the NTA invoice row a real
registration number), so `--check-filter` has a real entity to compare against.

Two kinds of row the probe cannot settle on its own: **`key_env` rows** (the
probe sends `{key}` literally, so they need `audit_batch_emit.py` with the key
set) and **`post_body` rows** (the probe only GETs). Nothing in these manifests
has been probed — the session that wrote them could reach no source host.

The number 31 was also used by a docs-only staging set
(`docs/candidate-sources-batch31.{cam,eu,fr,jp,row,us}.txt`, 144 candidates,
unrelated to these tables). A `batch31.*.txt` glob would have mixed the two, so
that set moved to **batch 36** (`docs/batch36/STAGING_README.md`).

## Merge note

The branch was cut from a 2,803-source tree and merged forward across batches
21-30. Two files conflicted and both resolved to main's side:

* `native/lib/hpengine.c` — this branch raised the fixed `HP_MAX_SOURCES` array
  from 1024 to 4096. Main deleted the fixed array and grows the table with
  `realloc` instead, which is strictly better; the bump is gone.
* `docs/SOURCE_EXHAUSTIVENESS.md` — this branch bumped a stale file count.
  Main rewrote the whole paragraph with current figures.

## After the branch merge (2026-10-02): 371 → 361 rows

origin/main and the local `merge/open-prs` branch each merged PR #23; reconciling
them kept this integration and removed ten rows from it.

**Seven dead endpoints**, re-run through the binary on 2026-10-02 and refused
every time, storing nothing (house rule 1):

| id | answer |
| --- | --- |
| `ZA_ETENDERS_OCDS` | 400 |
| `US_FARA_FOREIGN_PRINCIPALS` | 404 |
| `WORLDBANK_DEBARRED_FIRMS` | 401 |
| `JP_GTFS_DATA_REPOSITORY` | 404 |
| `CAM_511ON_ONTARIO` | 400 |
| `CAM_511AB_ALBERTA` | 400 |
| `IODA_OUTAGE_ALERTS` | 400 |

**Three duplicates** of collectors the tree already had. Their URLs sit behind
`#define`s, which is why the endpoint check above missed them:

* `FEODO_C2_TRACKER` fetched `feodotracker.abuse.ch/downloads/ipblocklist.json`,
  which `feodo_tracker_jp.c` and `sslbl_jp.c` already read.
* `TOR_EXIT_NODE_LIST` fetched `check.torproject.org/torbulkexitlist`, which
  `threatfeeds_world.c` (`TOR_EXITS_GLOBAL`) and `tor_exit_check.c` already read.
* `THREATFOX_IOC_SEARCH` POSTed the same `search_ioc` query as `IOC_LOOKUP`
  (`ioc_lookup.c`), but **without** the Auth-Key abuse.ch has required since
  2026-08-01. Every search would have come back empty, and for an IOC check
  "no hit" reads as "clean".

`USGS_NWIS_SITE_INVENTORY` stays. It reads every California site, while
`hp3_geo.c` reads active sites only.

**The 27 scheduled rows, measured 2026-10-02** (`audit_registry_emit.py`, a
fresh database per run):

* 13 store real records, among them FDSN 151,303, Safecast 30,000, Sejm 15,000,
  Oireachtas legislation 4,300 and members 1,928, and USGS NWIS 53,789 stored
  before the 300 s timeout.
* 14 are key-gated and store one explicit "gated (KEY)" notice.
* `EU_EP_CORPORATE_BODIES` loses 1 of 3,717 to a uid collision; that has not
  been read yet.

The 334 entity pivots are still unmeasured. The section above stands for them.

## Review fixes (2026-10-05): 361 → 360 rows

Found while reconstructing the manifests, before any probe:

* **`EU_VIES_VAT_VALIDATION` removed — broken by construction, and a
  duplicate.** Its body sent `{Q}` (the whole entity, upper-cased) as
  `countryCode` and `{qd}` (the digits) as `vatNumber`, so `DK28866984` became
  `countryCode="DK28866984"`, which VIES rejects, and no entity could form a
  valid request. The same lookup is already done correctly by
  `sources/corp_identifiers.c` and `vsrc16_eu_corporate_1.c`
  (`/rest-api/ms/{cc}/vat/{num}`).
* **`JP_NTA_INVOICE_ISSUER` stripped the `T`.** `{qd}` keeps digits only, and
  the NTA API wants the registration number WITH its `T` prefix, so every
  request was malformed. The url now puts it back (`number=T{qd}`), which works
  whether the analyst typed the `T` or not.
* **Three Socrata catalogue pivots were not scoped to their domain** (rule 4d):
  `cohesiondata.ec.europa.eu`, `finances.worldbank.org`, `www.datos.gov.co`.
  A domain's `/api/catalog/v1` searches every Socrata portal unless `domains=`
  and `search_context=` are passed; all three now pass both.
* **Four rows silently stopped at page 1** (rule 2): `CLIMATE_TRACE_ASSETS` and
  `TOR_ONIONOO_RELAYS` now walk `offset` (1000 × 20); `UK_CASELAW_ARCHIVE` walks
  `page` (20) WITHOUT `page_size`, because `page_size` on a page-NUMBER
  parameter switches the engine to offset arithmetic and page 2 is requested as
  `page=51`; `JP_ESTAT_STATSLIST` follows e-Stat's
  `RESULT_INF.NEXT_KEY` cursor through `next_path`/`next_tmpl`.
  `CLIMATE_TRACE_ASSETS` is also the row to check first when probing: it POSTs
  to an API whose public documentation describes GET.
* `JP_MLIT_LAND_TRADE_PRICES`, `MA_MARCHES_PUBLICS` and
  `CLOUDFLARE_RADAR_TRAFFIC` gained comments saying why they do NOT page (one
  response holds the range, or the endpoint has no offset).

The batch is now **360 rows: 27 scheduled, 333 entity pivots**. The 27
scheduled rows' 2026-10-02 measurements above predate these edits;
`TOR_ONIONOO_RELAYS` and `CLIMATE_TRACE_ASSETS` are pivots, so none of the
measured rows changed.
