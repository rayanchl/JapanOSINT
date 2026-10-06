# Batch 36: 188 live-measured rows for France, the UK, the USA, Africa and Hong Kong

Authored 2026-10-05 in a session **with** outbound HTTPS, so this batch is
measured end to end rather than reasoned about. Every endpoint was fetched,
every record count in a description is what that body held, every paging claim
was tested by fetching page 2 and comparing it against page 1, and every
`id_keys` was chosen by counting distinct values over real records.

Registry **18,456 → 18,644**, +188.

| Beat | Rows | Region | What the rows are |
|---|---:|---|---|
| `hp3b36_africa.c` | 61 | Africa + global reference | World Bank per-country GDP series (54 African countries), World Bank reference collections, WHO GHO dimension and indicator vocabularies |
| `hp3b36_usa.c` | 60 | USA | the openFDA estate (drug, device, food, tobacco, animal-vet, substance registries), Federal Register, ClinicalTrials.gov, NWS, NOAA NCEI, NSF awards, SEC company tickers, 40 Socrata portal inventories |
| `hp3b36_france.c` | 31 | France | BODACC legal announcements, the service-public administration directory, HAL, data.gouv.fr collections, 25 OpenDataSoft territorial and ministry catalogues |
| `hp3b36_uk.c` | 28 | UK | the Parliament API estate (bills, committees, treaties, Commons/Lords divisions, Erskine May, written questions), Environment Agency flood/hydrology/ecology/asset registers, ONS, postcodes.io, police.uk, GOV.UK search, NHS ODS |
| `hp3b36_china.c` | 8 | Hong Kong | data.gov.hk CKAN actions, HK Observatory weather, KMB bus open data, MTR station list, C&SD statistics |
| **total** | **188** | | |

## The User-Agent is part of the measurement

The first probe pass used a probe-only agent. That is **not a test of what the
engine can read**, and this tree already knows why: `core/httpclient.c` keeps a
per-host override table because real hosts filter on the substrings `OSINT` and
`collector`, and the engine's own `JO_USER_AGENT` contains both. Every row here
was re-probed under the engine's exact conditions — `JO_USER_AGENT` plus the 17
per-host overrides. It changed the answer both ways:

* **HDX's 406 was a bot wall, not content negotiation.** `data.humdata.org`
  answers `{"error":"Blocked due to bot activity."}` to anything carrying
  `OSINT`. It already has an override entry and 76 country endpoints in the
  tree, so the 54-row HDX Africa beat this batch originally planned was a
  duplicate of work already done. Africa is World Bank series instead.
* **`array_path=1` passed the hand-rolled probe and failed the repo's.** 61
  World Bank rows resolve their record array by *position*, and `hp_path` has no
  positional root index. A probe more capable than the engine passes rows the
  engine cannot read.
* **12 early rejects were egress-proxy failures, not dead endpoints.** CLAUDE.md
  records that 196 live sources were once discarded by that confusion, so
  transport failures carry their own verdict and never reach a reject list.

## Rule 4b found nine defects, and every one of them was mine

The first draft chose `title_keys` and `id_keys` by walking a precedence list of
field *names*. That is the trap CLAUDE.md names directly — "`id_keys` is a
MANIFEST DECLARATION, not the upstream's identity" — and it produced nine
distinct failures, every one invisible in the record count. The fix in each case
was to fetch real records and count distinct values per field, trying composites
where identity is a tuple of dimensions.

| row(s) | the wrong key | what it actually was | cost | corrected to |
|---|---|---|---:|---|
| `UK_EA_ECOLOGY` | `long` | the **longitude** | 449 of 2,500 | `site_id` (1:1 over 250) |
| World Bank ×54 | `value` | the **GDP figure** — null for several years in most countries, so every null year collapses onto one row | latent | `countryiso3code+indicator.id+date` |
| ODS catalogues ×28 | — | paging was **unstable**, not mis-keyed | 1–18 per portal | `order_by=dataset_id` |
| `FR_CARBON_RTE_ODRE` | `links` | an **array of hyperlinks** | 11 of 187 | `dataset.dataset_id` |
| `US_FDA_DEVICE_PMA` | `pma_number` | one approval carries many supplements | 127 of 1,000 | `pma_number+supplement_number` |
| `US_FDA_DEVICE_REGISTRATIONLISTING` | `k_number` | absent from 20 of 100 records | 160 of 1,000 | `registration.registration_number+establishment_type+proprietary_name` |
| `US_FDA_ANIMALANDVETERINARY_EVENT` | `treated_for_ae` | a **clinical** field | 65 of 1,000 | `unique_aer_id_number` |
| `UK_PARL_ERSKINE` | `sectionId` | repeats for every paragraph in a section | 4 of 61 | `searchResultText` |
| `UK_PARL_WRITTEN_Q` | `value` | an **object**, not a scalar — the row stored **nothing** while the endpoint answered 200 | 60 of 60 | `value.id` / `value.questionText` |

Two of those deserve naming on their own.

**A page size must be what the server SERVES, not what it is asked for.**
`UK_EA_ECOLOGY` was declared `_limit=200`. Measured: `_limit=200`, `250` and
`500` all return exactly **250** records, so the parameter is ignored. With
`skip` advancing 200 against pages of 250, every page overlapped by 50 — 449 of
2,500 records collapsed at the sink **while the record count still read 2,500**.
That is the FDSN `csv_delim` failure wearing different clothes: the count was
real, which is exactly why it read as healthy. `_limit` is now dropped from the
URL because it does nothing, and `page_size=250` is the measured truth.

**Unstable ordering loses rows, it does not merely duplicate them.** The 28
OpenDataSoft catalogue rows page by `offset`, and the unordered walk returns a
different page 1 on each request (first row `place_stg` on one call,
`actions_insertion_professionelle` on the next). Records that move between pages
are not only re-fetched, they can be **skipped entirely**. `order_by=dataset_id`
makes the walk deterministic; all eight colliding portals now store exactly what
they emit.

## Rule 4d dropped three rows, and that is the point

`probe_hp_batch.py --check-filter` asks every pivot row about an entity that
cannot exist. Three UK Parliament rows — `Interests`, `oralquestions/list` and
`StatutoryInstrument` — returned the **same 20 records** for an impossible
entity as for a real one. An entity pivot that ignores its filter answers "what
do we have on X" with records about everything else, attributed to X. They are
gone rather than fixed, because the upstream exposes no parameter that narrows
them.

Also dropped: `UK_POSTCODES_TERMINATED` and `US_FEDREG_SUGGESTED` (single
objects, not record sets), `FR_GEO_COMMUNES` (filter not honoured),
`FR_DATAGOUV_REUSES` (unreachable on re-verification — not written off as dead,
simply not shippable from here) and `US_FDA_OTHER_NSDE`: openFDA does not index
its name fields for free-text `search=`, so the pivot form returns zero for
every entity, and the bulk form duplicates an endpoint already in the tree.

## Measurements

`probe_hp_batch.py --check-filter`: **188/188 PASS.** One row
(`CN_HK_MTR_LINES`) returned a TLS `UNEXPECTED_EOF` on the batch run and passed
3/3 on retry, so it is counted as live rather than dropped on one blip.

`audit_batch_emit.py` through the real binary: **DROPS_EVERYTHING 0.**

All 35 entity pivots were additionally run against a real entity, because a
sweep with no entity reports `EMITS_NOTHING` for a pivot that works perfectly:

| row | entity | emitted | stored |
|---|---|---:|---:|
| `US_FDA_DRUG_LABEL` | aspirin | 1000 | 1003 |
| `US_FDA_DEVICE_510K` | medtronic | 1000 | 997 |
| `US_FDA_DEVICE_PMA` | medtronic | 1000 | 1001 |
| `US_FDA_ANIMALANDVETERINARY_EVENT` | dog | 1000 | 1001 |
| `UK_POLICE_NEIGHBOURHOODS` | metropolitan | 679 | 679 |
| `UK_PARL_WRITTEN_Q` | cyber | 60 | 61 |
| `UK_PARL_ERSKINE` | quorum | 61 | 61 |
| `US_FDA_DEVICE_COVID19SEROLOGY` | abbott | 220 | 220 |
| `UK_PARL_COMMITTEES` | science | 5 | 5 |

Where `stored` exceeds `emitted` the difference is disclosure notices, which are
themselves stored rows.

`US_FDA_DEVICE_REGISTRATIONLISTING` is the one row that still collapses records:
44 of 1,000, down from 160, with the (registration, establishment type, product)
triple that measures 100/100 unique over a page. The residue is duplicate
triples in openFDA's own data across pages, and it is **disclosed in the run
line** rather than silent — which is what rule 2 asks for when something cannot
be taken whole.

### The whole-batch sweep

`audit_registry_emit.py` over all 188 ids, reading `stored` off the run line and
the row count back out of a fresh database — two independent readings:

**152 OK, 34 EMITS_NOTHING, 2 COLLISION.**

The 34 are the entity pivots, swept with no entity. That verdict is an artefact
of the sweep, not a property of the rows: every one of the 34 was run against a
real entity (table above) and emits and stores. A pivot cannot be judged by a
run that never gave it anything to pivot on.

Both COLLISION rows lose exactly one record, and both were checked rather than
waved through:

* `UK_ONS_CODELISTS` — 84 emitted, 83 stored. There is **no unique field and no
  unique composite up to three fields**: `links.self.id` is 83/84 because ONS
  publishes the same code-list id twice. The collapse is real dedupe of a
  genuine duplicate upstream.
* `UK_FLOOD_STATIONS` — 1,421 emitted, 1,420 stored. The full station set
  fetched in one request is **1,419 stations with 0 duplicate `@id`**, so the
  emitted figure is 1,419 stations plus one paging-boundary duplicate plus one
  notice, and the stored figure is 1,419 plus the notice. **Every station is
  stored**; the collision is the boundary duplicate being deduped correctly.

## Ids carry no batch prefix

Like batch 33 (`CRATESIO_CRATE_VERSIONS`) and unlike batches 34 and 35
(`JO34_*`), the ids here are descriptive. Both conventions exist in the tree.
The consequence is that `audit_registry_emit.py --match` cannot select the
batch, so sweep it by id list:

```sh
grep -h '|' docs/candidate-sources-batch36.*.txt | grep -v '^#' | cut -d'|' -f1 > /tmp/b36.txt
python3 native/tools/audit_registry_emit.py --bin ./bin/japanosint \
        --ids-file /tmp/b36.txt --jobs 6 --timeout 300 --out sweep36.tsv
```

Note also that `batch_exclusions.py --skip-prefix` does **not** apply to the id
dump taken from the binary, so once the tables are generated the tool reports
every row of the batch as a `DUP-ID` against itself. The duplicate check for
this batch was therefore run against id and endpoint dumps taken **before**
generation: 0 id collisions and exactly 1 endpoint collision, which is why
`US_FDA_OTHER_NSDE` was dropped.

## What this batch does not claim

188 rows, not the 300 asked for. The shortfall is saturation, not effort: the
tree already carries 1,577 CKAN `package_search` endpoints, 868 OpenDataSoft
catalogues, 105 Socrata domains and 76 HDX country feeds, and 31 candidates were
dropped by `batch_exclusions.py` as duplicates of rows already shipped. At this
density, roughly one candidate dies for every one that ships.

Hong Kong carries the `china` beat at 8 rows. Mainland PRC endpoints are
overwhelmingly key-gated or unreachable; `data.gov.tw` rejects GET on its
documented search path, `data.gov.mo` serves HTML where it documents JSON, and
`geodata.gov.hk` was unreachable from here. Rather than pad the beat, it is 8
rows that work.
