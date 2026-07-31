# Batch: 400 high-penetrancy government, public-record and surveillance sources

Third high-penetrancy batch. The first
(`OSINT_SOURCES_BATCH_300_HIGH_PENETRANCY.md`) was organised by *depth* — the
detail endpoint behind a search hit. The second
(`OSINT_SOURCES_BATCH_300_CRITICAL_VALUES.md`) was organised by *consequence* —
the record that changes what you may lawfully do with a counterparty. This one
is organised by *who is doing the recording*: the state.

Three layers, in order of how far they sit from a company register:

1. **Government as an institution** — the legislature, the gazette, the
   rulemaking docket, the lobbying register, the procurement platform, the
   audit court. What the state decided, who asked it to, and what it bought.
2. **Public registers of things** — cadastre, vehicles, vessels, aircraft,
   spectrum, mineral tenements, concessions, and the two archives that publish
   the signed text of extractive and land deals.
3. **Standing surveillance** — the camera estates, sensor networks, transport
   telemetry and internet-wide measurement that public bodies and civic
   projects operate and publish.

## Counts

| File | Rows | Coverage |
|---|---:|---|
| `sources/hp3_gov_us_federal.c` | 28 | US federal: lobbying, FARA, campaign finance, rulemaking, research funding, banking, exclusions |
| `sources/hp3_gov_us_states.c` | 27 | 17 more Secretary of State registers + municipal record datasets (NYC, Chicago, SF, LA, Seattle) |
| `sources/hp3_gov_uk_ie_public.c` | 26 | UK procurement, Parliament APIs, regulators, land, FOI + Ireland (Oireachtas, lobbying, charities) |
| `sources/hp3_gov_eu_transparency.c` | 25 | EU expert groups, comitology, lobby meetings, document registers, Parliament open data, cohesion money |
| `sources/hp3_gov_europe_national.c` | 27 | DE/AT/CH/FR/NL/BE/SE/NO/DK/FI/ES/PT/IT/PL/CZ parliaments, procurement, gazettes |
| `sources/hp3_gov_japan_public.c` | 26 | JP: corporate-number spine, gBizINFO five-hop, e-Gov law, kanpō, courts, licensing, procurement |
| `sources/hp3_gov_asiapac_public.c` | 26 | AU/NZ/KR/TW/HK/ID/TH/MY/PH/IN + Pacific islands |
| `sources/hp3_gov_africa_public.c` | 26 | ZA/NG/GH/SN/CI/KE/TZ/UG/RW/ET/ZM/BW/NA + AfricanLII, SAFLII, extractives |
| `sources/hp3_gov_latam_public.c` | 25 | BR legislature & municipal gazettes, CL lobbying act, CO/AR/PE/MX + Central America & Caribbean |
| `sources/hp3_gov_mena_cis_public.c` | 26 | Gulf procurement, IL/TR/MA/TN/EG/JO + RU/UA/BY/AM/GE/UZ |
| `sources/hp3_pub_intl_bodies.c` | 23 | UN, MDB projects & debarment, arbitration, FATF-style bodies, IAEA, OPCW |
| `sources/hp3_pub_asset_registers.c` | 23 | Cadastre, vehicles, vessels, aircraft, spectrum, minerals, concessions |
| `sources/hp3_surv_cameras.c` | 23 | State-operated road camera inventories, NA/EU/APAC |
| `sources/hp3_surv_sensors.c` | 23 | Air, radiation, seismic, water, weather, space, fire, GNSS networks |
| `sources/hp3_surv_transport.c` | 23 | Rail telemetry, coastal AIS, ADS-B, transit operator registries |
| `sources/hp3_surv_netscan.c` | 23 | Host exposure, certificate transparency, censorship & outage measurement, wireless mapping |
| **total** | **400** | |

Registry before: 2803 sources. After: **3203**. Verified by diffing
`--list-sources` on a fresh database before and after: exactly +400, and no id
in the 2803 was displaced.

`lib/hpengine.c`'s `HP_MAX_SOURCES` was raised from 1024 to 4096. The engine
table now holds 1001 rows; the old bound would have left 23 spare and an
overflow drops a whole collector, so the allocation bound was moved well clear
of the real count. It is an allocation limit, not an editorial one — overflow
was already loud (a stderr line naming the dropped id).

## Where the penetrancy is

The batch is not a list of search boxes. The rows that earn the name:

* **gBizINFO, five second hops off one corporate number** — subsidies,
  government contracts, certifications, patents and filed financials all hang
  off the 13-digit Japanese corporate number, so one pivot on a Japanese
  company name returns its public-money history rather than confirming it
  exists. Financials are available here for unlisted companies that publish
  nothing on EDINET.
* **Brazil's Chamber of Deputies → deputy expense claims** — the detail hop
  returns every supplier paid out of a deputy's parliamentary quota, with the
  supplier's CNPJ and the document number.
* **UK Commons divisions → the aye and no lists** — the detail hop names every
  MP on each side of a vote.
* **FEC candidate/committee → cycle totals**, **Companies House-style detail
  hops on Regulations.gov documents and comments**, **CQC provider →
  inspection ratings and every location operated**, **UK Parliament bill →
  sponsors and stage history**.
* **Chile's Ley del Lobby** — every minister, mayor and regulator must publish
  each meeting with an outside party: who attended, for which company, what was
  sought. There is no comparable dataset in the hemisphere.
* **Querido Diário** — normalised daily gazettes from thousands of Brazilian
  municipalities, where local awards and appointments actually appear.
* **Alaska's corporations database** — one of the only US registers that
  publishes officials *and their percentage ownership*.
* **Czech and Spanish cadastres** — parcel ownership readable by name, without
  registration.
* **The Dutch national vehicle register** — an entire country's vehicle
  register as an open Socrata dataset.

## What the rows are held to

Every row is a declarative `hp_source` on the shared engine
(`lib/hpengine.{c,h}`), so all 400 inherit the guarantees in
`docs/SOURCE_EXHAUSTIVENESS.md`:

* **no implicit cap** — not one row sets `max_items`; the engine emits every
  record the upstream returned;
* **pagination declared wherever the upstream supports it** — `next_path` on
  the Senate LDA, Find a Tender, Federal Register, data.gouv.fr, Transitland,
  RIPE Atlas, OONI and NWS rows; `page_param` / `page_size` / `page_start` on
  every Socrata, CKAN, opendatasoft, FEC, Regulations.gov, FDIC, SAM and
  Riksdag row. Where the page ceiling bites it is stamped on every record as a
  `collector-truncation-notice`, never as a log line;
* **explicit field lists where an API defaults to a summary** — the NSF awards,
  GOV.UK search, USGS site file, PurpleAir sensor index, World Bank projects
  and SAM entity rows request the full field set rather than accepting the
  four-field default, because accepting the default *is* a discard;
* **full flatten** — every scalar of the upstream record lands in `properties`;
* **honest empty** — a blocked fetch, a shape change or a missing credential
  emits nothing and says so.

Every HTML-mode row sets `base` and `filter_query`; every credentialed row
declares `key_env` so a missing key is reported rather than silently returning
zero records.

## Verification performed

| Check | Result |
|---|---|
| `make` (`-Wall -Wextra`) | clean, no new warnings |
| `make audit-sources` | **0 findings across 715 files**; strict set `hp*_*.c` clean |
| `make hptest` | all engine guarantees pass |
| `--list-sources` diff on a fresh DB | 2803 → 3203, exactly +400, nothing displaced |
| id uniqueness | 400 unique ids, none colliding with the 2803 existing |
| endpoint uniqueness | no row reuses an endpoint already wired by an existing `hp_*` row (13 first-draft rows that did were replaced) |
| dispatch smoke run | rows resolve, expand their URL templates correctly and degrade to honest empty |

**What could not be verified here:** this environment's network policy blocks
outbound HTTPS to third-party hosts, so no row was exercised against a live
upstream. The smoke runs confirm URL-template expansion, credential gating and
the honest-empty path (`GBIZINFO_API_TOKEN unset — honest empty`,
`transport failure https://internetdb.shodan.io/1.1.1.1`) — they do **not**
confirm any upstream's current response shape. Where an upstream has changed
its shape or path since these endpoints were documented, the affected row
degrades to fewer resolved fields or to an honest empty; it cannot fabricate,
because the engine only ever forwards fields that came back over the wire.
Re-running these dispatches from an unrestricted network is the natural
follow-up.
