# Batch: 301 regional "critical values" high-penetrancy sources

Second high-penetrancy batch. Where the first batch
(`OSINT_SOURCES_BATCH_300_HIGH_PENETRANCY.md`) was organised by *depth* — the
detail endpoint behind a search hit — this one is organised by *region and
consequence*: the records that actually decide a due-diligence outcome in Asia,
South America, North America and Europe.

A "critical value" here means a record that changes what you may lawfully or
prudently do with a counterparty:

* **blacklists and debarment** — Mexico's SAT 69-B simulated-operations list,
  Kazakhstan's unreliable-suppliers register, Brazil's CNEP/CEIS and slave-labour
  list, Peru's OSCE debarments, the EU's EDES exclusions, the multilateral
  development bank cross-debarment lists, Japan's METI foreign end user list;
* **licence and authorisation status** — EBA/EIOPA/ESMA, BaFin, FINMA, CySEC,
  CSSF, AFM, DNB, CNMV, CNBV, OJK, MAS, SFC, KNF, CNB, the US state professional
  and contractor boards;
* **enforcement** — NY DFS and CA DFPI consent orders, FinCEN penalties,
  DG COMP cartel decisions, CNMC and CADE antitrust, TCEQ and IBAMA
  environmental orders, CONSOB and AMF market abuse;
* **tax standing** — Poland's VAT whitelist, Romania's inactive-taxpayer flag,
  India's GST status, Vietnam's tax-code lookup, Texas franchise forfeiture;
* **public money** — Slovenia's Erar payment-level disclosure, Spain's BDNS
  subsidy database, ANAC, CompraNet, Prozorro, goszakup, the EU Financial
  Transparency System;
* **courts and insolvency** — Czech ISIR, Austrian Ediktsdatei, German
  Insolvenzbekanntmachungen, BODACC, NYSCEF, Taiwan and China judgment
  databases;
* **critical infrastructure** — the EU ETS installation register, French ICPE
  sites, German PRTR facilities, Global Energy Monitor asset trackers, WRI's
  power-plant data, Open Supply Hub facilities.

## Counts

| File | Rows | Coverage |
|---|---:|---|
| `sources/hp2_asia_east.c` | 22 | KR, TW, CN, HK, JP, MN |
| `sources/hp2_asia_south_sea.c` | 23 | IN, PK, BD, LK, NP, SG, MY, ID, TH, VN, PH, KH/LA/MM |
| `sources/hp2_asia_central.c` | 16 | KZ, UZ, KG, TJ, AZ, AM, GE, EAEU, BY, MD |
| `sources/hp2_southam_brazil.c` | 22 | BR (federal registers, sanctions, courts, regulators) |
| `sources/hp2_southam_andes.c` | 25 | AR, CL, CO, PE, EC, BO, PY, UY, VE + regional |
| `sources/hp2_northam_us_reg.c` | 25 | US federal regulators & enforcement |
| `sources/hp2_northam_states.c` | 24 | US state registers, licences, courts, lobbying |
| `sources/hp2_northam_ca_mx.c` | 23 | CA (12) + MX (11) |
| `sources/hp2_eu_institutions.c` | 22 | EU-level agencies and registers |
| `sources/hp2_eu_dach.c` | 21 | DE, AT, CH, LI |
| `sources/hp2_eu_west.c` | 20 | FR, BE, NL, LU, IE |
| `sources/hp2_eu_south.c` | 20 | IT, ES, PT, GR, MT, CY |
| `sources/hp2_eu_central.c` | 21 | PL, CZ, SK, HU, SI, HR, RO, BG, RS, UA |
| `sources/hp2_world_critical.c` | 17 | MDB debarment, AML regimes, export control, infra |
| **total** | **301** | |

Registry before: 2502 sources. After: **2803**. No id collides with an existing
source and none of the 2502 was displaced — verified by diffing
`--list-sources` on a fresh database.

## What the rows are held to

Every row is a declarative `hp_source` on the shared engine
(`lib/hpengine.{c,h}`), so all of them inherit the guarantees in
`docs/SOURCE_EXHAUSTIVENESS.md`:

* **no implicit cap** — not one row sets `max_items`; the engine emits every
  record the upstream returned;
* **pagination declared where the upstream supports it** — `page_param` /
  `page_size` / `page_start` on the Socrata, CKAN, opendatasoft, goszakup,
  Sirene, RIS, SHAB and CIPO rows, so page two onwards is not silently
  discarded; when the page ceiling bites it is stamped on every record as a
  `collector-truncation-notice`, never as a log line;
* **full flatten** — every scalar field of the upstream record is kept in
  `properties`, not a hand-picked three;
* **honest empty** — a blocked fetch, a shape change or a missing credential
  emits nothing and says so. Confirmed by smoke-running rows in this batch:
  `MX_SAT_EFOS_69B` → `status=403 … records=0`,
  `OPEN_SUPPLY_HUB_FACILITIES` → `OPEN_SUPPLY_HUB_TOKEN unset — honest empty`.

`make audit-sources` now holds `collectors/sources/hp*_*.c` strictly clean (the
glob was widened from `hp_*.c` so this batch is covered): **0 findings across
699 files**.

## Credentials

Six new variables, all appended to `.env.example` with the row they serve:
`KR_DATA_GO_KR_KEY`, `KR_ODCLOUD_KEY`, `FMCSA_WEBKEY`, `INPI_TOKEN`,
`INSEE_SIRENE_TOKEN`, `OPEN_SUPPLY_HUB_TOKEN`. `SAM_API_KEY` and
`BR_TRANSPARENCIA_KEY` are reused from the previous batch. Every other row in
this batch is keyless. An unset variable produces an honest empty, never a
substituted record.

## Verification actually performed

Done here:

* `make` — clean at `-Wall -Wextra`, zero warnings, zero errors;
* `make audit-sources` — 0 findings, strict set includes `hp2_*`;
* `make hptest` — engine guarantees (pagination both styles, no implicit cap,
  truncation disclosure, shape gates) still pass offline;
* fresh-DB `--list-sources` — 2803 registered, 301 added, 0 lost, 0 duplicate
  ids;
* `--selftest` — PASS;
* `--run` smoke tests on rows in four different files.

**Not** done here, and worth stating plainly: outbound HTTPS is policy-blocked
in this environment (403 on CONNECT to every host), so **no endpoint in this
batch has been exercised against the live upstream**. The URLs are written from
knowledge of each portal, not from a successful response. Where a portal is a
single-page application or sits behind an anti-bot layer, the HTML rows will
return honest empty rather than anchors — which is the designed behaviour, but
it means per-row live reachability remains unverified until this runs somewhere
with egress. The mode/shape choices most likely to need adjustment after a live
pass are the SPA-backed registries (Washington CCFS, Ohio SOS, SHAB, EUIPO
TMview) and the two POST-JSON California rows.
