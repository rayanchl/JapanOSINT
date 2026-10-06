# Batch 39 — date tokens, JSONP, POST-body and path paging (2026-10-06)

Batch 38 listed what the engine could not express. Four mechanisms now cover
the main gaps. Each is mirrored in the probe, so a row is proven under the
conditions the engine runs it in. Each was then applied to every registered row
that needed it, and to the rejected candidates it unblocks.

## The mechanisms

| mechanism | syntax | where |
| --- | --- | --- |
| date tokens | `{date}`, `{date-7d}`, `{date+1m}`, `{date-1y:%Y}`, `{date-1d:%Y%m%d}`, `{date:@ms}`; `tz_minutes=540` (Japan), `480` (China) | `lib/datetok.c`, the ONE implementation: hp rows (url, post_body, headers, detail_url) and the VJSON/VGEO/VCSV/VRSS fleet through `vsrc_url_dates` (UTC; the old `{{today-N}}` still works) |
| walk back to the newest data | `date_back=N`, `date_step=d/w/m/y` | the first page answers 4xx or carries no records → every `{date…}` steps back one unit, up to N |
| JSONP | none (automatic) | `cb(…)`, `null(…)`, `(…)`, a callback behind an empty block comment, the `)]}'` guard; never valid JSON, so nothing else is touched |
| paging in a POST body or a URL path | `{page}` `{offset}` `{offset1}` `{offset_end}` `{size}` | re-expanded every page in the url AND the body; EPA's `rows/{offset}:{offset_end}` |
| stamp the file date into records | `date_field=<name>` | each record gets the date the url's first `{date…}` rendered to (after any step back) before it is keyed: a dated file whose rows carry no date otherwise overwrites yesterday's uids |
| pace a walk | `page_delay_ms=N` | a pause before every page after the first, for firewalls that answer a burst with 403 |

Two refinements came out of running these against real upstreams:
* **A failure right after a short page is the end of the data.** Envirofacts
  answers a range starting past the last row with HTTP 500. The GHG walk stored
  all 395,894 rows, then filed a truncation notice. It no longer does.
* **Later pages get three retries** (250 ms, 0.5 s, 1 s), not one. Before this
  change, the joined TRI-transfer walk died on a transient 500 at row 20,000.
  It now got 154,000 of 183,012 rows in the 50-minute audit budget, with no
  failure.

Tooling: `probe_hp_batch.py` expands date tokens and first-page tokens, steps
dates back the way `date_back` does (on a 4xx, an empty page or an unparseable
one), and unwraps JSONP, including `cb((…))`. It resolves `array_path` exactly
like `hp_run_json`: numeric segments into lists (`0.content` under
`null([…])`), `key=value` selectors, and the multi-path fallback that maps a
segment across arrays. A JSON null at the path is no records. Before this,
four valid BSE/NEEQ rows failed and three CNINFO rows passed hollow. `gen_hp_batch.py`
accepts `tz_minutes`, `date_back` and `date_step`. `audit_batch_pagination.py`
counts `{page}`/`{offset}` as a paging declaration. `batch_exclusions.py`
folds the new tokens when comparing endpoints.

Tests: `hptest` 30–34 cover date rendering against `gmtime_r`, `date_back` on
a 404 and on an empty page, `date_step=y`, `date_field` (the stepped-back file date reaching the uid), six JSONP forms, POST-body paging,
path-range paging, and both sides of the short-page rule. `lib/datetok.c` was
also cross-checked against Python's calendar on 270 cases: month-end clamping,
leap years, four timezones across midnight, epoch output, every step unit, and
malformed tokens. There were 0 mismatches.

## Applied to registered rows

### EPA Envirofacts: path-range paging

The three rows declared `page_param="rows"`, but Envirofacts pages in the
path, so every run stored the same 101 rows.

| row | before | after |
| --- | --- | --- |
| `EPA_GHG_SUBPART_EMISSIONS` | 101 of 395,894 | **395,894 of 395,894** (one weekly walk, 41 pages of 10,000) |
| `EPA_TRI_RELEASE_QUANTITIES` | 101 of 32,001,427 (whole history, unjoined) | the newest loaded reporting year, joined to its form (facility, chemical, certifying official): RY2024 = 756,188 rows. `{date-1y:%Y}` + `date_back=2` + `date_step=y` found RY2025 empty and fell back to 2024 by itself |
| `EPA_TRI_TRANSFER_QUANTITIES` | 101 of 9,255,866 | the newest reporting year, joined: RY2024 = 183,012 rows (154,000 stored in the 50-minute audit budget, still walking) |

All three have composite `id_keys`, each measured unique per record. Before,
they had none; the TRI rows titled on `doc_ctrl_num`, which a form shares
across media and transfers. Positional order was measured stable on the GHG
table and on the joins, but not on the bare release table, which is a second
reason for the join.

### Frozen date windows → rolling (43 rows)

Every one of these was a scheduled row re-fetching a window fixed on the day
it was written.

| rows | was | now |
| --- | --- | --- |
| Elexon REMIT | Aug 10–16 2026 | last 7 days |
| PL MF whitelist ×2 (+ their detail hops) | as of 2026-08-14 | as of today (`tz_minutes=60`) |
| CPSC recalls (hp) | Jan–Jun 2026 | last 6 months |
| Federal Register public inspection | 2026-08-14 | today, `date_back=4` over weekends |
| eCFR full text ×3, eCFR structure, eCFR ancestry | as of 2026-01-01 | newest issue date (`date_back=10`; today 404s until published), detail hop and VJSON row 7 days back |
| PNCP armed forces ×3 | June 2025 | last 30 days |
| PNCP contracts / procurements / price registries | 2–3 days of Jan 2025 | yesterday and today, every 6 h |
| CENACE ×2 | Aug 10–12 | last 3 days |
| XM bolsa / dispatch | Aug 1–2, Aug 15–16 | last 7 days / days 10–4 ago (measured publication lag 3 / 7–10 days), with composite keys (the title `Valor` had 170 / 1,314 distinct values) |
| ohsome contributions | ended 2026-01-01 | through last month, `date_back=6`, `date_step=m` (its data ends 2026-07-27 today) |
| EIDA waveform catalogue ×11 (pivots) | July 1–5 | the station's last 5 days |
| VJSON: GDACS, INGV ×2, AFAD ×2, GFZ Kp, AusTender ×2, met.no, Rechtspraak, Elering | fixed days or weeks in 2025–2026 | rolling `{date…}` windows |

Run through the engine: all 19 scheduled hp rows store what they emit. 11 of
12 VJSON rows do. The two INGV rows had been failing every run, before the
change too, because INGV's front end drops any User-Agent containing the word
"collector". `webservices.ingv.it` now gets the engine's agent minus that
word, as INE Spain and Boston already do (`core/httpclient.c`); they store 171
and 200 events. `geo-gfz-kp-nowcast` still stores nothing, also a pre-existing
failure: the host redirected to `kp.gfz.de`, and the response is parallel
arrays, not records. Another row reads the same index from the new host.

### POST bodies with a fixed page (4 pivots)

| row | entity | before | after |
| --- | --- | --- | --- |
| `USASPENDING_AWARDS` | Lockheed Martin | 40 | 2,001 (20-page ceiling, disclosed) |
| `USASPENDING_SUBAWARDS` | Lockheed Martin | 40 | 2,001 |
| `EU_TED_NOTICES` | Siemens | 40 | 2,001 |
| `NIH_REPORTER_PI` | Smith | 40 | 15,001 (RePORTER's own offset limit) |

## New sources unlocked (batch 39)

### `jp39`: 28 rows, Japan (27 OK, 1 SLOW)

| rows | mechanism | stored (audit) |
| --- | --- | --- |
| `JO39_JP_CAA_ACCIDENT_DATABANK` | POST JSON body paged with `{offset}`/`{size}` through the CSV export | 70,772–81,797 in 15 min (register: 445,190), weekly |
| `JO39_JP_JSDA_OTC_BOND_REFERENCE` | daily file, tomorrow's first, `date_back=8` | 12,410 |
| 9 × `*_AREA_SUPPLY_DEMAND` + `TOHOKU_…_DAILY` | month token `{date-1d:%Y%m}` with `date_step=m` / day token | 251–1,488 each (half-hourly, by generation source) / 48 |
| 10 × `*_UNIT_GENERATION` (every grid operator) | `bghatsu_NN_{date-1d:%Y%m%d}.csv`, `date_back=3`, read every 6 h before the next day replaces it | 13–148 units a day, 48 half-hours each |
| `JO39_JP_OCCTO_RESERVE_MARGIN` / `_WEEKLY` | mandatory `fromDate` | 480 / 280 |
| `JO39_JP_OCCTO_INTERTIE_DAILY` / `_WEEKLY` | `inputDate={date:%Y/%m/%d}` + **`date_field`** (the daily file dates only its envelope) | 11 / 11 interconnectors × 48 slots / 14 days |
| `JO39_JP_JPX_MARGIN_DAILY` | date-named xlsx, `date_back=10`, **`date_field`**: two archived consecutive files shared 200 of ~210 ISINs, so keyed on ISIN alone ~96% of each day would overwrite the last | 418 |
| `JO39_JP_JMA_SHINDO_DB_RECENT` | rolling 3-day window under the database's 1,000-event cap | 27 (+ station-intensity detail hop) |

Fifteen candidates were rejected with the measured reason in
`docs/rejected-sources-batch39.jp39.tsv`. What still blocks some of them: HTML
table rows as records (the FSA money-lender register, 1,421, and isms.jp, 8,466,
are each one POST away but answer as HTML tables); detail hops into HTML pages;
ZIP entries after the first; and cookie/terms-of-use session gates (OCCTO's
national unit-generation system).

### `cn39`: 31 rows, China and Hong Kong (29 OK, 1 correct-dedupe collision; NPC fixed after the audit)

| rows | mechanism | stored (audit) |
| --- | --- | --- |
| `JO39_CN_PALACE_MUSEUM_COLLECTION` | POST `{}` with `{page}` in the url | 6,200 then 403 (see below); collection 1,860,076 |
| `JO39_CN_NPC_LOCAL_REGULATIONS` / `_LAWS_OTHER` | POST JSON body; split by category under the server's 30,000-offset cap | 23,475 + 7,421 = all 30,896 |
| `JO39_CN_SSE_SUSPENSIONS` | `{page}` in the url, full history since 2008 | 28,994 |
| `JO39_CN_AMAC_PRIVATE_FUND_MANAGERS` | POST `{}`, `{page}` in the url | 18,396 |
| `JO39_CN_NEEQ_SECURITIES`, `BSE_IPO_REVIEW`, `BSE_SECURITIES`, `BSE_ANNOUNCEMENTS` | POST form + **JSONP** `null([…])` + `{page}` in the body (`array_path=0.content`) | 5,789 / 874 / 348 / 132 |
| `JO39_CN_CNINFO_ANN_SZ` / `_SH` / `_BJ` | POST form, `{page}` and a date window in the body, `date_back` | 100 / 622 / 135 (two-day windows; a 3,000-record server cap is disclosed on reporting-season peaks) |
| `JO39_CN_SHFE_*` × 7, `CFFEX_DAILY`, `CZCE_DAILY` | `{date:%Y%m%d}` (`tz_minutes=480`) + `date_back` over weekends and the National Day closure; the quote files add **`date_field`** | 301–6,244 per file per day |
| `JO39_CN_SZSE_MARGIN_BALANCES` | dated xlsx + `date_field` | 2,107 |
| `JO39_CN_SSE_ANNOUNCEMENTS` | date window + `{page}` in the url | 475 |
| `JO39_CN_HKIA_*` × 4 | mandatory `date={date-2d}` + `date_field`, array mapped through the day objects | 90–456 flights |
| `JO39_CN_HK_CR_NEW_LOCAL_COMPANIES` / `_NONHK_COMPANIES` | weekly Monday-dated CSV, `date_back=21` | 4,148 / 45 |
| `JO39_CN_CNEMC_STATION_AQI` / `_SURFACE_WATER` | POST body | 1,601 stations hourly / 1,556 sections |

* **NPC local regulations** first stored 23,473 of 23,475. At 10,000 per
  page, the default order changed between requests, so 2 laws repeated across
  page boundaries and 2 others were skipped. No sort field is stable (`gbrq`
  ties repeat 9 in 600). One request at `pageSize=25000` returns all 23,475,
  distinct, in 56 s, so there is no boundary left. Re-run: 23,475 stored.
* **CZCE**: 4 of the 27 unlabelled subtotal lines are byte-identical
  zero-activity lines; 266 of 270 is correct dedupe.
* **Palace Museum**: the firewall answered 403 after 31 unpaced pages. The
  walk is disclosed as cut short, and the address was still blocked over an
  hour later. The row now pauses 3 s between pages (`page_delay_ms`): a
  monthly walk of about 8 h. The pace that clears the firewall is **not yet
  measured**. Re-run this row when the block has lifted.
* **Replaced**: the frozen VCSV `hk-companies-registry-new-nonhk-companies`
  (one week of 2025, since batch 17) is removed. `JO39_CN_HK_CR_NEW_NONHK_COMPANIES`
  reads the newest week.
* **Still rejected (6)**: DCE (JS challenge), SAMR food sampling (captcha),
  GGZY (dead host, 1,000-row cap), NMC typhoons (unwraps now, but all 32 storms
  duplicate the shipped Zhejiang catalogue), CNINFO latest (same announcements
  as the archive rows), and the AMAC fund register (HTTP 500).
* **Unread side arrays**: the SHFE quote files also carry product aggregates
  and implied-volatility tables that one row cannot read alongside its main
  array. Each description says so.

### `tbl39`: 13 rows, HTML tables as records (13 OK)

The fifth mechanism, `table_must` (mode=html): each row of a `<table>` is a
record keyed by its header cells.

| row | stored |
| --- | --- |
| `JO39_TBL_ISMS_CERTIFIED_ORGS` (form POST + Referer, one request) | 8,466 |
| `JO39_TBL_CAA_TOKUSHOHO_ACTIONS` (`{page}` in the path, 27 pages) | 531 |
| `JO39_TBL_JPX_DELISTED` + 6 yearly archives + `JPX_MARKET_TRANSFERS` | 137 + 500 + 48 |
| `JO39_TBL_GSI_GEONET_OUTAGES` (two-row header, `table_skip_rows=1`) | 42 |
| `JO39_TBL_RIKUDEN_OUTAGES_7D` (+ `index_url`: the page moves between dated folders) | 9 |
| `JO39_TBL_FNET_MT_CATALOG` (the monthly text catalogue, whitespace CSV + `{date:%Y%m}`) | 4 (96–141 a month) |

14 rejects with the measured reason in `rejected-sources-batch39.tbl39.tsv`.
What table mode still cannot express: a record spanning two rows under a
two-row header (FSA money lenders, JPX new listings), a date that appears only
on some rows (JWA dam and Kanagawa level tables), identity held in attributes
(F-net's `<tr id>`, MEE permits), tables drawn by JavaScript from a JSON map,
headerless or iframe-only tables, detail hops into HTML pages, and paging
cursors carried between responses (MEE's `tempReportKey`).

### Full-length runs (no audit time budget)

* `EPA_TRI_RELEASE_QUANTITIES`: 440,000 of RY2024's 756,188 rows in 1 h 58 m.
  It then met a 500 at offset 440,000 that outlasted three retries, and the
  same range answered 200 later. Later pages now get five retries (0.25–4 s
  backoff). The walk was disclosed as cut short, not reported complete.
* `JO39_JP_CAA_ACCIDENT_DATABANK`: all 46 pages in 1 h 23 m; 474,653 distinct
  reports stored. The export only sorts by `REGIST_DATE` (the other field
  names tried answer 400), and many reports share a date. 1,605 records
  (0.34%) repeated across page boundaries, so as many may be skipped in any
  one walk.
* `JO39_CN_PALACE_MUSEUM_COLLECTION`: the museum still answered 403 on
  2026-10-06 afternoon, about 3 h after the first burst. The 3 s pace remains
  unmeasured.

## Registry

18,267 → **18,325**: +28 jp39, +31 cn39, −1 replaced VCSV row.
