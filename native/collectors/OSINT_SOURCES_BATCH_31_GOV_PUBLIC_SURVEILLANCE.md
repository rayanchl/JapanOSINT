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
python3 tools/audit_registry_emit.py --bin ./bin/japanosint \
        --match hp3b31 --jobs 6 --timeout 220 --out b31.tsv
python3 tools/probe_hp_batch.py --check-filter      # FILTER_IGNORED is fatal
```

Retire anything returning `EMITS_NOTHING`, `COLLISION` or `FILTER_IGNORED`. An
unmeasured row is a gap in coverage, not a source of false data — the engine
only forwards fields that came back over the wire, so a moved or reshaped
endpoint yields an honest empty.

## No manifest

These 16 tables were hand-authored, not scaffolded by `tools/gen_hp_batch.py`,
so there is no `docs/candidate-sources-batch31.*.txt`. Do not regenerate over
them. The manifest-driven auditors (`audit_batch_reachable.py`,
`audit_batch_pagination.py`, `audit_batch_emit.py`) cannot be pointed at this
batch; `audit_registry_emit.py --match hp3b31` reads `--list-sources` and needs
no manifest.

## Merge note

The branch was cut from a 2,803-source tree and merged forward across batches
21-30. Two files conflicted and both resolved to main's side:

* `native/lib/hpengine.c` — this branch raised the fixed `HP_MAX_SOURCES` array
  from 1024 to 4096. Main deleted the fixed array and grows the table with
  `realloc` instead, which is strictly better; the bump is gone.
* `docs/SOURCE_EXHAUSTIVENESS.md` — this branch bumped a stale file count.
  Main rewrote the whole paragraph with current figures.
