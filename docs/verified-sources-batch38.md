# Batch 38 — 100 Japanese and Chinese sources picked for data yield

Authored, verified and selected 2026-10-02/03. **100 rows**: 50 Japanese and
50 Chinese (39 mainland, 11 Hong Kong and Macau). Every row was fetched live,
probed, compiled into the real binary and run against a fresh database to
prove it **emits and stores**. Together they expose **about 4.31 M records per
full pass** (Japan 1.67 M, China 2.63 M). Registry 18,170 → **18,270**.

Per-row numbers: `docs/verified-sources-batch38.tsv`. Manifests:
`docs/candidate-sources-batch38.<beat>.txt` (shipped rows only). Tables:
`native/collectors/pivot/table/hp3b38_<beat>.c`. Everything fetched and not
shipped, with the reason and the full manifest line:
`docs/reserve-sources-batch38.tsv` (92 rows). The agents' rejects during
authoring: `docs/rejected-sources-batch38.<beat>.tsv`. Authoring brief and
helper scripts: `docs/batch38/`.

## What shipped

| beat | rows | records / pass | what it is |
| --- | --- | --- | --- |
| `jpgeo3` | 29 | 1,253,389 | Japanese public-sector GIS layers and municipal tables: Omihachiman and Kosai lot-number layers (134k, 129k), Tokyo street trees (226k across two files), MLIT motor-carrier accident reports 2003–2023 (109k), NIES annual air-quality station data since 1981 (5 pollutants), Kanazawa wholesale-market origins since 2000, Tsukuba sewer manholes, MLIT CORINS contracts and My City Construction, food-permit registers (Sapporo, Fukuyama, Minato), Itami watch-camera locations |
| `jpnat3` | 21 | 419,767 | Japanese national bodies: NII/CODH Geoshape historical place names, Edo villages and historical municipalities (186k), MHLW occupational-injury databases 2019–2021 and the 2024 fatality database, MHLW care-provider registers (5 service types), NDL Imperial Diet minutes (both houses), MLIT 2021 road traffic census, MOFA overseas-safety notices, 2025 food-poisoning incidents, the e-Gov public-comment results feed |
| `cnmkt` | 23 | 2,199,975 | Chinese markets and disclosure: Eastmoney data centre (block trades with buying/selling branch 683k, institutional research visits 288k, Dragon-Tiger list 269k, earnings pre-announcements 191k, director dealings 172k, holder changes 147k, share pledges 127k) and its announcement stream; SSE and SZSE director dealings, commitments, regulatory measures, inquiry letters, disciplinary sanctions, suspensions; CSRC penalties, market bans and licensing results; CNINFO equity and bond registers |
| `cngov` | 16 | 336,957 | Mainland government and telemetry: SAMR national, industry and planned standards (277k), State Council policy library (gazette, State Council and ministry documents, interpretations), MIIT documents, intangible-heritage items and named inheritors, CMA live warnings (15 min) and 2,440-station forecasts, CNEMC hourly city AQI, CENC earthquakes, Zhejiang typhoon catalogue since 1949, MEE coastal seawater |
| `cnsar` | 11 | 96,924 | Hong Kong and Macau: HKEXnews announcements, Companies Registry TCSP licensees and money lenders, Customs money-service operators, HKMA fraudulent bank websites/apps and HIBOR, Buildings Department s.24 orders, FEHD restaurant licences, registered drugs, live metered-parking occupancy, Macau real-estate agent licences |

## How the 100 were chosen from 192 verified candidates

1. **Dead, refused or about to go stale (21).**
   * All 8 ANRE FIT/FIP certified-plant files (153k records). Each monthly
     edition is a new Salesforce attachment id, and the March 2026 id for
     Ibaraki now redirects to `SiteFileNotFound`, so every row would die at
     the next edition.
   * 5 NFRA rows. The regulator's WAF answered 403 on page 41 and page 6 of
     the penalty walks (800 of 32,938 and 100 of 41,797), then 403 on page 1
     for every row on the retry.
   * HKMA's CMU instrument list returned `"No result found"` the day after it
     served 2,986.
   * Tokyo's four wholesale-market files sit under `/shijyou/2026/` and Izu
     water temperature is `2026fixed_…`. These are fiscal-year paths that stop
     updating at year end.
   * Kobe's permit register is a May 2021 snapshot.
   * The Okinawa farmland ledger calls itself a staff app's verification
     layer.
2. **Thin (6).** The CFETS fixing, Shibor and LPR rows are a rolling one-month
   window (about one new record a day). The three CFFEX option books have a few
   hundred quotes each.
3. **Japan ranked by stored records with five registers protected.** MOFA
   overseas safety, the 2024 fatality database, 2025 food poisoning, Itami
   watch cameras and the e-Gov feed came in. NIES was capped at 5 of its 8
   pollutant layers, and raccoon captures, one drug-price edition and the
   threatened-plant mesh went out. The line fell at 4,684 records.
4. **China held to 50, with Hong Kong and Macau capped at 11.** Within the
   mainland, sanction and enforcement registers were kept ahead of yield
   (CSRC market bans, SZSE and SSE discipline). Nine state-media lists that
   add ≥10 items a day went to reserve in favour of registers and telemetry.
   The registry already has several Chinese news feeds.

## Gates (2026-10-03, on the final tree)

`make` 0 warnings · `selftest` PASS · `unit` all passed · `hptest` all passed
· `lint-sources` OK (18,270) · `audit-sources` 0 findings tree-wide, strict
set 0 / 287 · `pagewalktest` all passed · `source-floor` 18,270 ≥ 16,271.

Batch gates:
* **Final re-probe: 100/100 PASS.** Four mainland rows (CENC, both ihchina
  rows, SZSE regulatory measures) hit a timeout or connection reset in the
  parallel pass and passed on a serial retry.
* **`batch_exclusions`:** 0 endpoint collisions with the rest of the tree.
  Two near-misses against `geospatial_jp_ckan.c` were read: that collector
  stores `package_search` catalogue records and never fetches resource files.
  0 ids shared with the pre-batch registry, 0 duplicate ids within the batch.
* **`audit_batch_reachable`:** 0 of 100 can never run.
* **`audit_batch_pagination`:** 0 undeclared paging (44 `pagination_ok`).
* **`--check-filter`:** no row is an entity pivot, so there is nothing to test.

## Emit/store audit (`audit_registry_emit.py`, fresh DB per source)

| verdict | rows | meaning |
| --- | --- | --- |
| OK | 84 | stored == emitted |
| SLOW | 11 | still walking at the audit timeout (600 s), each having stored real, distinct records: 7 Eastmoney data-centre tables (73,000–116,000 by then), the announcement stream, two SAMR standards walks (70k, 76k), CSRC licensing results |
| COLLISION | 5 | all correct dedupe of byte-identical upstream rows (below) |

The 5 collisions:
* Tokyo 23-ward street trees: 1,954 byte-identical rows in the CSV.
* MLIT motor-carrier accidents: 1 duplicate line in 108,668, counted
  directly.
* 2025 food poisoning: 2 identical incidents.
* SZSE disciplinary sanctions: 1 pair identical in every column.
* HKMA scam list: 1 duplicate entry in 2,497.

**CSRC is intermittent from here.** The licensing-results walk failed to
connect in both audit runs (two 20 s connect timeouts, 40 s), and one run of
the IPO-committee row did the same. Run standalone, the licensing walk stored
12,360 distinct records in 11 minutes before it was stopped by hand, and the
IPO-committee row stored all 4,997. A failed run is reported as an error and
backed off, never stored as an empty result. That measurement is recorded in
`docs/batch38/emit38_zzmanual.tsv`.

## Fixes found by this batch

**1. `tools/probe_hp_batch.py` read xlsx cells into the wrong columns.**
`<c\b([^>]*)>(.*?)</c>` let a self-closing empty cell (`<c r="E1" s="3"/>`)
swallow the next cell, so every later value shifted one column left, and a
self-closing cell's style attributes were applied to the swallowed value. It
also ignored `r` references, so it dropped the blank line `lib/xlsx.c` writes
for a missing row. The probe and the engine then disagreed about which
physical line `csv_skip_lines` lands on. The reader now places cells and rows
as `lib/xlsx.c` does.

Re-judged on the same downloaded bytes, 78 of 87 xlsx rows across all
manifests got the same verdict. On the 9 that changed, the engine agrees with
the new reader every time. Eight are this batch's FIT rows (the engine stored
exactly the new count). The ninth is a shipped row that had been broken all
along:

* **`JO30_BANK_FSA_KINSA`** stored 35 records titled with banner fragments
  (所管, 預金等媒介業務, 関東財務局) and read 1,770 rows as empty. It passed the
  old probe because the probe miscounted the multi-line banner. The upstream
  has also added a banner line since. It is now `csv_skip_lines = 11` and
  stores 31 intermediaries plus the two-row header's second row (the C carries
  the measurement; the manifest is mirrored).

**2. A 403 on a later page ended the walk in silence.** `hp_run` already
treated a mid-walk 429 or 5xx as a walk cut short, emitting a
`collector-truncation-notice`. A 403 was treated as the end of the collection.
NFRA's WAF answers bursts with 403, and the penalty walks reported
`emitted 800 of 800 available`, rc=0. A 403 after page 1 is now a truncation
too. A 404 or 400 past the end is still a silent, legitimate end. `hptest`
pins both cases, and the 403 test fails on the old condition.

**3. `core/httpclient.c` discarded curl's error.** Callers log only
"transport failure", which is why the CSRC connect timeouts took a standalone
run to diagnose. The client now prints `curl_easy_strerror()`.

## Follow-up, 2026-10-05: the `page_size` trap, fixed tree-wide

`hp_run` treats `page_param` as an OFFSET whenever a row declares `page_size`
(`lib/hpengine.c`, `offset_style`). A page-NUMBER API configured that way is
asked for pages 1, 1+N, 1+2N … and N−1 of every N pages are never fetched,
while the run reports success. `AP19_CN_SZSE_LISTED` (`PAGENO`, 146 pages)
stored ~160 of 2,904 listings that way.

**How each row was judged.** All 110 rows that pair `page_size` with a
parameter not named like an offset were fetched twice: the URL as written,
and the same URL with the parameter +1. A page number answers with a disjoint
page; an offset answers with the same records shifted by one. Then every
changed row's default page was compared with page 0 and page 1 (by record id,
because WordPress objects carry per-request fields), so no walk now starts on
page 0 and jumps to page 2.

| outcome | rows |
| --- | --- |
| `page_size` removed: measured page numbers (74), plus CIMA ×2, the French company directory, Doorkeeper, Kobe, SZSE, Georisques, Setouchi and OpenAerialMap, each measured separately or by its API's contract. Czech CRO and WHO IRIS also got `page_zero_based=1`: their pages are 0-based, so the walk had gone 0 → 2 | 83 |
| true offsets, already right: NDL `idx` ×3, CRD `results_get_position` ×2, Narou `st` ×2, GeoNetwork `from` ×2, `inicio` ×2, Hatena `of` ×2, DBLP `f` ×2, WorldBank `os`, MapRoulette `page` (a record offset despite its name; now marked `page-offset-ok`) | 17 |
| `page_zero_based=1` added: Metacpan `from` (0-based; coerced to 1 it lost one release per page), CFPB `frm` (the URL binds frm=0; not measured, the host blocks this machine) | 2 |
| wrong parameter: PRH Finland used v1's `resultsFrom`, ignored by v3 → `page` | 1 |
| parameter ignored by the upstream: Yanoshin `recent.json` ignores every paging variant. TDnet → no paging, `limit=300` (~3 days; 100 was 3.5 h, less than the 15:00 disclosure peak); EDINET removed (below) | 2 |
| SHAB: filter ignored and 401 (below) | 1 |
| path-range paging the engine cannot express: EPA Envirofacts `rows/0:100` ×3 (they keep disclosing the cut-off via the repeated-page notice) | 3 |
| unmeasurable: INSEE SIRENE `debut` (needs a key); documented 0-based, left unchanged | 1 |

**Measured effect.** On the 68 scheduled rows, run old binary vs new against
the same upstreams with a 240 s budget: 56 store more, 11 the same, 1 fewer
(the CIMA change register hit the audit timeout mid-walk). Stored records went
from 30,987 to 54,495; many rows now stop at their own `page_max` with a
truncation notice instead of sampling every Nth page. Examples: JTSB aircraft
accidents 60 → 1,740, Teraren banks 50 → 1,146, MUFG branches 50 → 717,
WordPress archives 100 → 1,001. The 19 entity pivots via `--dispatch`: CIMA
VMPP 200 → 2,001, CIMA supply problems 200 → 874, Qiita 100 → 801, WHO IRIS
400 → 1,001, PRH 101 → 154 (all of them).

**What consecutive pages exposed.** Walking contiguous pages surfaced three
defects that the sparse walk had hidden:

* **`id_keys` that were not unique (rule 4b).** DailyMed `rxcui` (756
  distinct in 1,000 records: one concept, several names) → `rxcui+rxtty+rxstring`;
  `unii_code` → `unii_code+active_moiety`; `code` → `code+type+codingSystem`
  (one code sits under two coding systems).
* **Unstable server ordering.** VNDIRECT repeated and skipped stocks (985
  distinct in 1,000) → `&sort=code` gives 1,000. OpenAerialMap (992) →
  `&order_by=_id&sort=asc` gives 1,000.
* **Ceilings below the data.** The DailyMed lists had no `page_max`, so the
  10-page default kept 0.2–82.6% of each. They now reach the end:
  RXCUIS 100 → 78,631 stored (one run), UNIIS 8,037, drug classes 1,210.
  The SPL index (1,594 pages) moved from daily to weekly; its daily run had
  been an arbitrary 1,000 of an unordered list.

**Dead rows found on the way.**
* **SHAB:** its `query` parameter is IGNORED. An impossible entity returned
  all 2,862,192 publications (rule 4d), and without `publicationStates` the
  API answers 401. It is now `keyword={q}&publicationStates=PUBLISHED`
  (impossible → 0, Novartis → 841).
* **Czech CRO:** moved to `cro.gov.cz`. The old host's HTTP/1.0 redirect made
  libcurl fail every run.
* **Removed:** `YANOSHIN_EDINET_RECENT` (its "recent" filings end on
  2018-06-04 and the dated lists are empty for 2026) and
  `SG_SGX_ANNOUNCEMENTS` (`Missing Authentication Token`).
* **Also removed, found by re-probing all 440 batch-32/33 rows** (435 PASS; the
  others were Toei buses outside service hours and three mainland timeouts
  that passed on retry): `JO32_MUNI_HINO_BUILDING_CONFIRMATIONS`. Hino unshared
  its dated layer (ArcGIS 499 `Token Required`), and the org has no public
  successor.
* **Registry:** 18,270 → **18,267**.
* **`JO32_REG_NIIGATA_CONSTRUCTION`** (batch 32) went 404 three days after it
  shipped: the monthly edition moved to a new attachment id. It now resolves
  the file from its landing page every run (`index_url`; 9,034 stored).

**A lint so it stays fixed.** `make lint-sources` has a new check,
`page-size-on-page-number`: an hp row whose `page_param` is named like a page
number and that declares `page_size` fails CI unless it carries a
`page-offset-ok: <measurement>` marker. Run against the pre-fix versions of
the 19 edited files it reports 84 rows (the 83 above plus MapRoulette). On
the tree it reports 0. The C carries a one-line measurement comment on every
changed row, and the 86 rows with manifests had their manifests mirrored. Every
batch manifest in the tree now passes the generator's strict load.

## What the engine or the tooling could not express — status

| gap | status (2026-10-05) |
| --- | --- |
| probe only sends GET | **fixed**: `probe_hp_batch.py` POSTs `post_body` with the row's `content_type`, as `hp_run` does (AMAC managers: PASS) |
| probe cannot read ZIP | **fixed**: inflates the first entry, as `lib/zipread.c` does (HK signboards: 24,631 features, PASS) |
| probe cannot read `x:`-prefixed SpreadsheetML | **fixed**: prefix-tolerant reader (HKEX ListOfSecurities: 17,006 securities, PASS) |
| a file re-published under a new name each edition | **fixed in the tooling**: the engine already had `index_url` + `index_href_must`; the generator and probe now accept them. Not usable for FIT, whose links differ only in anchor text |
| long composite titles hashed past 63 bytes | **fixed**: the title and body picks use a buffer as wide as the composite join; only keys are hashed. `hptest` 18a-ter |
| Eastmoney's `{"result":null}` past the end filed an array-path-missing notice every run | **fixed**: a null or absent path on a later page, after records, is the end of data. Page 1 is still drift. `hptest` 18a-bis |
| no rolling date token | open: SHFE/CZCE/CFFEX daily files, SSE suspensions, SZSE margin, HKIA flights, CR weekly lists, JMA windows |
| paging carried in a POST body | open: flk.npc.gov.cn laws, the CAA accident databank |
| JSONP-only APIs | open: BSE and NEEQ, NMC typhoon tracks |
| "latest resource of a package" (CKAN) | open: needs a `package_show` hop |
| TRS `index.html`/`index_1.html` paging | open: SAMR vehicle recalls |
| path-range paging (`rows/{start}:{end}`) | open: EPA Envirofacts ×3 |

## Operational notes

* **`api.hkma.gov.hk`** (2 shipped rows) is behind a WAF. After about 40
  requests in a minute it answered 405 and then held responses for about 60 s.
  Both rows declare `timeout_ms=120000`.
* **The Eastmoney rows walk 250–1,400 pages per run** (block trades about
  1,366). At the daily interval that is about 4,300 requests a day to
  Eastmoney. Move the historical tables to weekly if the host objects.
