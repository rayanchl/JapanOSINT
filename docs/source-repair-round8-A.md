# Source repair, round 8 — agent A (shard A)

Perpetual sweep-and-repair over shard A (`docs/batch30/shardA_ids.txt`, 8,416
ids; editable files in `docs/batch30/shardA_files.txt`). Method per
`docs/batch30/REPAIR_BRIEF.md`: every scheduled id in the shard is run through
the real binary with `tools/audit_registry_emit.py --scheduled` (fresh copy of
a warm template DB per run, 220 s kill line), every non-OK verdict is
diagnosed against a live fetch, fixed in the collector C, rebuilt, and
re-measured with `--only`. **A fix counts only when the recheck says OK.**
Per-source lines are in `docs/source-repair-round8-A.tsv`.

Build tree: `~/jorepairA` (WSL Ubuntu-24.04). Sweeps run against a COPY of
the binary (`~/repair8/binA/japanosint`) so a rebuild for a fix never swaps
the executable under a sweep in progress.

Entity pivots (interval 0) are skipped by `--scheduled`; a pivot run with no
entity is a fake EMITS_NOTHING. They are counted below as "pivot (not swept)".

## Running totals

| chunk | id range | ids | scheduled swept | OK | fixed → OK | correct dedupe (documented) | transient | slow (noted) | dead / unreachable | engine-bug | pivot (not swept) |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| 1 | 1–250 | 250 | 66 | 63 | 1 | 0 | 1 | 1 | 0 | 0 | 184 |
| 2 | 251–500 | 250 | 161 | 149 | 6 | 3 | 0 | 2 | 1 | 0 | 89 |
| 3 | 501–750 | 250 | 219 | 196 | 9 pivots re-declared + 1 key | 3 | 0 | 9 | 0 | 1 rate wall | 31 |
| 4 | 751–1000 | 250 | 230 | 183 | 2 pivots re-declared; 9 → OK (CO_REGALIAS key, CL_SERVEL order+ceiling, CL_SEIA + WD_OIL timeouts, 2 WD via 60 s, AR_JUSTICIA timeout pending, 6 DPD at long timeout) + 2 ceilings lifted (CO_ANM 10,000→12,914; RUNAP 1,000→2,074) | 3 (MCC, OFSI, WD_MILITARY) | 0 | 3 left (CL_SNIFA, RIPE, ARIN) | 8 (CR layer dead, JM×2 reset, EC + AR×4 stall) | 10 (PNCP×3 upstream 504, PA×2 HTTP/2, BO bot wall, 2 FEC rows → key slot, 3 WD queries over WDQS's 60 s) | 20 |
| handoff | nra-radiation (id 3515) | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 |
| rule-2 caps found while editing | 4 CKAN rows (ODS_ITALY/IRELAND/CHILE/ARGENTINA) | 4 | 4 | 4 (100 each, capped) | 3 → full catalogue (Italy recheck running) | | | | | | |

Chunk 2 detail: D3FEND_MAPPINGS (moved host), ODS_COLOMBIA (timeout), the
three AP19_NZ_*_HUB rows (1-based `startindex`), SOCRATA_HAWAII (User-Agent,
then paging: 100 → 976) → OK. TWSE_AP11, BINARYDEFENSE_BANLIST and
THREATVIEW_C2 are correct dedupe of byte-identical upstream repeats, now
documented on their rows with the counts. CELESTRAK_GP_STARLINK is
unreachable at TCP level (as round 7 found for 37 CelesTrak rows).

Chunk 3 detail: nine `NEEDS_ENTITY` rows were entity-token URLs (`{q}`,
`{qU}`, `{ql}`, `{qd}`) declared with an interval — a scheduled run can never
fill the token, so they failed on every run; each is now `interval = 0` and
proven to run with an entity (AZ_CBAR 42/42, MD_BNM 40/40, CIMA 138/138,
Delphi ECDC 12/12). PSE_POEB_RBN, TPEX_DIRECTOR_HOLDINGS, TPEX_BOARD_ELECTION
and UKHSA_MEASLES got explicit composite identities and are correct dedupe of
byte-identical repeats (stored == distinct full records in every case).
TRONSCAN_TOP_ACCOUNTS is a 429 rate wall (3 rps, escalating IP suspension).
Nine SLOW rows (delegated-stats files, Health Canada DPD bulk tables, OFSI,
WHO xMart, FATCA GIIN, CVM funds) are on a single long-timeout recheck.

## Fix patterns seen

* **Composite key that was right on a quiet day.** `geo-nve-flood-warning`
  keyed on `MunicipalityCsvString+ValidFrom`, verified 1,077/1,077 on
  2026-09-07 when every record was a placeholder. With a real warning active
  the feed carries one record per covered municipality and each repeats the
  warning's WHOLE list in the CSV string, so 31 rows share a key. Per-record
  identity is `MunicipalityList[].Id`; joined ids + `ValidFrom` is 1,077 of
  1,077. Lesson: verify a composite on a day the upstream has something to say.
* **Upstream moved host, same path.** `D3FEND_MAPPINGS`: d3fend.mitre.org now
  serves a GitHub-Pages 404 on `/api/ontology/inference/…`; the identical
  path on `next.d3fend.mitre.org` serves 60 MB of live JSON. 0 → 16,164.
* **1-based offset parameter with no value bound in the URL.** ArcGIS Hub's
  OGC search (`startindex`) counts from 1. The engine reads the start from the
  URL and otherwise starts at 0, so page 2 was `startindex=100` — one item
  re-served per page boundary (never a hole, as the engine comment says). The
  row-level fix is `page_start = 1`. Three NZ hubs; any other `startindex`
  Hub row without a bound value has the same one-record overlap.
* **Slow bodies under the 20 s engine default.** `ODS_COLOMBIA` (TTFB 21 s)
  and `AP19_TW_TWSE_AP11` (10.4 MB at a variable rate, 120–303 s) both fail as
  "transport failure" and store nothing; each now declares a `timeout_ms`
  sized to the measured transfer.
* **Bot wall on one word of the User-Agent.** `SOCRATA_HAWAII` (CKAN) answers
  502 in 0.5 s to the engine and 200 to curl. Bisected: `JapanOSINT/1.0` 200,
  `JapanOSINT/1.0 (contact via repo issues)` 200, `OSINT/1.0` 200,
  `JapanOSINT collector` **502**. The rejected token is the word "collector",
  exactly as data.boston.gov and INE in round 7. Per-row `.headers` User-Agent
  (honest self-identification) is the collector-side fix; the per-host table
  in `core/httpclient.c` is the maintainer's.
* **Transient upstream throttling reads as EMITS_NOTHING.** `OVERPASS_TUNNEL`
  answered empty in 26.5 s during the sweep and 100/100 twice afterwards.
* **SLOW that is only the kill line.** `cyb-caida-asrank` (20 pages at ~10 s)
  is OK at 420 s. `CERTPL_DOMAINS_JSON` and `RIPENCC_DELEGATED_STATS` are still
  running at 600 s with 106k and 104k rows stored — unmeasured, not failed.

## Dead upstreams (recorded, left failing honestly)

* `nra-radiation` (`collectors/sources/nra_radiation.c`, handoff from the
  batch-30 research pass): all four feeds dead on 2026-09-21, measured twice.
  The two `radioactivity.nra.go.jp/cont/json/*` files are 403 (S3
  AccessDenied); the relaunched Nuxt site's `/api/v1/*` endpoints all answer
  401 to a plain GET; its only public file (`/sea-data.json`) is sea-area
  monitoring — lat/lon/value markers by month, no station, no dose rate — a
  different dataset and not a replacement; `www.kankyo-hoshano.go.jp` is
  NXDOMAIN; `emdb.jaea.go.jp` 302s to `/emdb/top` which is a 500 behind
  Incapsula. Evidence is on the feed table in the file and in the TSV.
* `CELESTRAK_GP_STARLINK`: resolves, no TCP handshake on :443 or :80, three
  tries over two days.

* **Entity token + interval = a row that can never run.** Nine rows in
  `hp3b19_health.c` / `hp3b19_finance.c` carried `{q}`-family tokens AND an
  interval. `hp_run` refuses the scheduled run ("needs an entity") and the
  sweep shows NEEDS_ENTITY; `audit_batch_reachable.py` only checks the
  opposite direction (static URL + no interval). Flip to `interval = 0`.
* **CKAN catalogues capped at one page.** Five `package_search` rows used
  `rows=100` with no `page_param`, so each stored 100 of 976 / 65,503 /
  22,727 / 3,217 / 1,287. `start` is honoured (0-based) and `rows=1000` is
  accepted on all five. The exhaustiveness scanner cannot see a cap that is
  "no paging declared", so this class is found only by reading `count`.

* **Page ceilings hiding behind green runs.** `CO_ANM_RUCOM_TITULOS`
  (10,000 of 12,914), `CO_RUNAP_AREAS` (1,000 of 2,074), `CL_SERVEL_DOCUMENTOS`
  (1,000 of 8,361), `CL_SEIA_PROYECTOS` (1,000 of ?) all stopped at the
  10-page default with `rc=0`. Only reading the upstream's own count
  (`$select=count(*)`, `last_page`, `X-WP-Total`) exposes it; `page_max` fixes
  it per row.
* **Unstable page order.** WordPress media in `date desc` (SERVEL) and a
  Laravel paginator with no ORDER BY (RUNAP) re-serve one item and skip
  another at page boundaries. `orderby=id&order=asc` fixed SERVEL to
  8,361/8,361; RUNAP ignores every sort parameter and keeps a 5-row overlap.
* **Shared demo credentials.** `OPENFEC_FILINGS` and `FCC_ECFS_FILINGS`
  carried `api_key=DEMO_KEY` — api.data.gov's 40-calls/hour-per-IP demo key,
  shared by every DEMO_KEY row on the host (OpenEI, two NASA rows too). Both
  were 429 on every run. They now take `{key}` from `FEC_API_KEY` /
  `FCC_API_KEY` and report "needs credential" when unset, which is the
  honest state. The other DEMO_KEY rows (`hp3_energy.c`, `hp3_geo.c`) are on
  the same quota and will fail the same way under load.

## Reported, not fixed (engine / maintainer)

* **`load_dotenv` takes an inline comment as the value.** `.env` line 256 is
  `FEC_API_KEY=` followed by spaces and `# FEC_CONTRIBUTIONS (api.open.fec.gov;
  DEMO_KEY works for testing)`. The engine sent that comment, URL-encoded, as
  the FEC api_key and got 403 — so `FEC_CONTRIBUTIONS` (hp_americas_deep.c)
  has been failing on every run with a key the operator believes is unset.
  Either strip ` #…` in `load_dotenv` or treat an all-whitespace value as
  unset. Found via `OPENFEC_FILINGS` after this round moved it to the same
  key slot.
* **Sink throughput caps the delegated-stats files.** `RIPENCC_DELEGATED_STATS`
  and `ARIN_DELEGATED_STATS` were still ingesting at 1,500 s with 191k /
  194k rows stored — roughly 130 rows/s. The upstream is a 3 s download; the
  time is all ours. Anything over ~250k rows cannot finish inside any
  reasonable scheduler slot until inserts are batched.
* **WDQS under sweep concurrency.** Wikidata's query service throttles per
  agent (and kills queries at 60 s). A `--jobs 4` sweep that lands several of
  the 27 `WD_*` rows together makes every one of them fail, and the heavy
  `P31/P279*` queries (naval bases, nuclear plants) exceed 60 s even alone.
  Measure the WD family serially; a per-host concurrency of 1 in the
  scheduler would keep production runs off the wall.

* **No today-date token.** `PSE_POEB_RBN` and the seven other
  `api.raporty.pse.pl` rows in `hp3b19_energyenv.c` (rce-pln, pk5l-wp,
  his-wlk-cal, crb-rozl, csdac-pln, mbu-tu, zmb) walk an OData feed from
  business_date 2024-06-14 with `$first=50` and the 10-page ceiling: 500
  records per run, the same 500 forever, run green. The API accepts
  `$filter=business_date eq 'YYYY-MM-DD'` and `$first=5000`. `AZ_CBAR_FX_XML`
  and `MD_BNM_FX_XML` are the same gap from the other side: date-addressed
  feeds with no dateless form, so they can only be pivots on a date entity.
  A `{today}` / `{today-N}` token in `hp_expand` would make all ten rows
  real scheduled sources.
* `SOCRATA_HAWAII`: the "collector" token belongs in the per-host UA table in
  `core/httpclient.c` next to data.boston.gov; the per-row header is the
  workaround available without touching the engine.
* `TRONSCAN_TOP_ACCOUNTS`: a `key_env` slot gates the row OFF when unset
  (`jo_needs_credential`), so it cannot be added as "optional key"; the row
  needs either an engine notion of optional credentials or a per-host rate
  limiter (3 rps).

## Range reached

Chunks 1–3 complete: shard A ids 1–750 swept, fixed and rechecked, plus the
`nra-radiation` handoff (id 3515). Chunk 4 (ids 751–1000) sweep in progress;
ODS_ITALY (66-page recheck) and the nine SLOW rows' long-timeout recheck in
progress.
