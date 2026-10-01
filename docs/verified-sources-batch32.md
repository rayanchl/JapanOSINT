# Batch 32 — 340 Japanese sources picked for data yield

Authored 2026-09-30, verified and selected 2026-10-02. **340 rows** across eight
beats, every one fetched live, probed, compiled into the real binary and run
against a fresh database to prove it **emits and stores**. Together they expose
**about 6.49 M records per full pass**. Registry 17,829 → **18,169**.

Per-row numbers: `docs/verified-sources-batch32.tsv`. Manifests:
`docs/candidate-sources-batch32.<beat>.txt` (shipped rows only). Tables:
`native/collectors/pivot/table/hp3b32_<beat>.c`. Everything fetched and not
shipped, with the reason: `docs/reserve-sources-batch32.tsv` (163 rows) and
`docs/rejected-sources-batch32.<beat>.tsv` (the agents' rejects during
authoring). Authoring brief: `docs/batch32/BRIEF.md`.

## What shipped

| beat | rows | records / pass | what it is |
| --- | --- | --- | --- |
| `jprepo2` | 75 | 687,472 | JAIRO Cloud university repositories not yet registered (full OAI-PMH walks; Hitotsubashi 62,484, Hiroshima 40,615, KEK 28,217 …) |
| `jpsearch2` | 66 | ~130,700 | Japan Search databases not yet registered: national/prefectural archives, NDL, National Archives of Japan, geo-tagged photo and survey collections, Hiroshima Peace Memorial and Minamata archives |
| `jparcgis` | 53 | ~3.21 M | public-sector ArcGIS layers: MLIT bridges/tunnels/boreholes/medical, prefectural police accident layers, Hyogo crime, bear sightings, Tokyo hydrants, Shibuya food permits and footfall |
| `jpmuni3` | 36 | ~2.20 M | municipal open data: ten land-parcel indexes, building confirmations, food permits, religious corporations, bear reports, route land prices |
| `jpfeeds` | 36 | ~2,900 per fetch (refreshed every 15–60 min) | feeds that add ≥10 new items a day (measured): livedoor, Yahoo News categories and regional providers, NHK NEWS WEB, Asahi, Hochi, ITmedia |
| `jpregister` | 29 | ~147,000 | licence and sanction registers: FSA warning lists and fund notifiers, Tokyo NPOs and consumer-law dispositions, construction, real-estate, industrial-waste licensees, Osaka minpaku |
| `jptelemetry` | 25 | ~52,000 per run, accumulating | JARTIC road-traffic counts (no other source in the tree), DOCOMO bike share, Toei positions, TEPCO/Chubu/Shikoku demand, Gifu road sensors |
| `jpmarket` | 20 | ~60,500 + trade streams | MOF JGB yield curve since 1974, BOJ daily rates, JSF loan balances, JPX listed issues, Japanese exchange trade feeds |

## How the 340 were chosen from 503 verified candidates

1. **Dead or about to die (10).** Two Aichi workbooks 404'd two days after
   they were probed (file replaced). Seven register files are republished
   weekly or monthly under a new file name, and the previous edition already
   404s (OTIT ×2 weekly; Saitama ×3, Kanagawa construction and CAA FOSHU
   monthly). The MOF asset-freeze CSV is a dated snapshot that MOF keeps
   online, so the row would silently freeze on the 2026-09-29 designations.
   A later recheck added Hokkaido intractable-disease providers (an `R8.9`
   monthly file name that stopped answering after the October edition).
2. **Thin streams (43).** The 17 JR West position feeds (1–17 trains per
   snapshot), JR Kyushu disruptions (empty when trains run normally), three
   3–17-record road and space-weather rows, and 22 feeds whose measured
   turnover is under 10 new items a day.
3. **The repository family capped at 75** of 100 for diversity; the 25
   smallest (2,574–~3,000 records) went to reserve.
4. **The rest ranked by measured records per run**, with the seven small but
   complete sanction and warning registers protected (FSA ×4, JTA
   dispositions, Tokyo consumer-law dispositions, MLIT recalls). The line fell
   at 1,727 records.

## Gates (2026-10-02, on the final tree)

`make` 0 warnings · `selftest` PASS · `unit` all passed · `hptest` all passed
· `lint-sources` OK (18,169) · `audit-sources` 0 findings / 1,650 files,
strict set 0 / 264 · `pagewalktest` all passed · `source-floor` 18,169 ≥
16,271.

Batch gates: final re-probe 338/340 PASS. The 2 EMPTY results are Toei bus and
train positions at 02:35 JST, outside service hours; both stored real positions
in the daytime audit (495 and 92). `batch_exclusions`: 0 endpoint
collisions with the rest of the tree, 0 ids shared with the pre-batch registry,
0 duplicate ids within the batch. `audit_batch_reachable`: 0 of 340 can never
run. `--check-filter`: no row is an entity pivot, so there is nothing to test
(0 FILTER_IGNORED).

## Emit/store audit (`audit_registry_emit.py`, fresh DB per source)

| verdict | rows | meaning |
| --- | --- | --- |
| OK | 239 | stored == emitted |
| SLOW | 80 | still walking at the audit timeout (75 OAI repositories, 2 ArcGIS, 3 parcel indexes); each had stored real, distinct records by then. Repositories stored 700–2,900 at 480 s; the large ArcGIS and parcel layers 89,885–268,000 at 600 s |
| COLLISION | 20 | read one by one, all correct dedupe of identical content (below) |
| NO_RUN_LINE | 1 | `JO32_ARC_MLIT_BRIDGE` (740,449 features): killed by the audit's own 1 GB per-run file ceiling after storing 137,026 distinct rows. That is the harness's limit, not a source failure |

The 20 collisions:

* **16 Japan Search rows.** The upstream's `from` paging has no stable order
  (no sort parameter is documented; page 1 of `madb_game` called twice
  returned two disjoint sets). So one walk can return the same record on two
  pages, and the sink keeps it once. Measured on `dignl`: the 4 pages held
  1,845 distinct ids out of 2,000. Annotated on each row.
* **4 registers, each checked against the parsed file.**
  * Tokyo consumer-law dispositions: one disposition is listed twice,
    identical.
  * FSA unregistered-business warnings: three anonymised individuals ("Ａ")
    are identical in all five columns.
  * Aichi real-estate brokers: 199 identical filler rows with no licence
    number.
  * Hokkaido waste haulers: six rows hold only U+3000 spaces, and two of
    them are identical. All 5,567 licences are stored.

## Engine fix found by this batch: xlsx cells longer than 4 lines

`lib/csv.c` has a repair pass for malformed feeds: a quoted field spanning more
than `CSV_QUOTE_MAX_LINES` (4) physical lines is treated as an unterminated
quote. It is closed at its first line, and the lines it held are re-parsed as
records. That is right for hand-made CSV. It was wrong for the xlsx path,
because `lib/xlsx.c` writes the CSV itself, quotes every multi-line cell per
RFC 4180, and Excel users write long cells. Every cell of 5+ lines was
shredded into junk records.

The FSA fund-notifier register (`menkyoj/tokurei/011.xlsx`: 4,590 rows, 1,690
cells of 5 to 44 lines) read `emitted 19052 [36846 empty]`, `stored 12323`.

The fix adds `csv_parse_wellformed()` (the same parse without the repair), and
`hp_run_csv` uses it for `HP_XLSX`. Unit test:
`test_wellformed_long_cell_is_not_repaired`.

Measured effect, old parser → fixed parser:

| row | before (emitted/stored) | after |
| --- | --- | --- |
| JO32_REG_FSA_PROFUND_NOTIFIERS | 19,052 / 12,323 | 4,590 / 4,590 |
| JO32_REG_FSA_UNREGISTERED_WARNED_OVERSEAS | 772 / 656 | 436 / 436 |
| JO32_REG_FSA_PROFUND_ABOLITION_ORDERS | 630 / 626 | 611 / 611 |
| JO32_REG_TOKYO_NPO_CORPORATIONS | 8,797 / 8,797 (read OK, inflated) | 8,529 / 8,529 |
| JO32_REG_JTA_TRAVEL_AGENCY_DISPOSITIONS | 70 / 70 (read OK, inflated) | 56 / 56 |
| JO30_BANK_FSA_CHUUKAI (existing) | 749 / 710 | 679 / 679 |
| JO30_BANK_FSA_TRUST_AGENTS (existing) | 360 / 357 | 350 / 350 |
| JO30_BANK_FSA_BANK_AGENTS (existing) | 150 / 150 (inflated) | 141 / 141 |
| JO30_BANK_FSA_SHINKIN_EPSP (existing) | 52 / 52 (inflated) | 35 / 35 |

The other 37 existing xlsx rows (all in `hp3b30_jpbank2.c`) are unchanged.
Note the two rows that read OK while inflated: a shredded cell becomes
*distinct* junk records, so no count-based check sees it. Only comparing
against the file's own row count does.

## Tool fix: the probe honours a row's `timeout_ms`

`probe_hp_batch.py` waited a fixed 25 s whatever the row declared. JAIRO Cloud
pages take about 50 s, so 94 of 100 repositories read TIMEOUT: a live source
reported dead because the probe waited under different conditions than the
collector does. This is the same class of mismatch `row_headers()` exists to
prevent. The probe now uses `max(25 s, timeout_ms)`.

## Known limits, stated rather than hidden

* **Japan Search returns at most 2,000 records per query** (`from+size ≤
  2000`, `size ≤ 500`; `from=1949&size=500` answers hit=0). Large databases are
  a stated bounded view: `bibnl` 6.8 M, `najda` 4.3 M, `dignl` 5.0 M.
* **Many register files carry an edition in their name.** The weekly and
  monthly ones were excluded. The annual and quarterly ones that shipped
  (Osaka, Hokkaido, Kagawa, Ishikawa, Nagano, Tokushima, Osaka City lodging)
  will 404 at their next edition and must be repointed then. The engine
  cannot follow a landing page to its current file.
* **Storage.** The live DB holds 3.8 M items in 9.2 GB (about 2.4 KB each).
  A full pass of this batch is about 6.5 M records, so expect the database to
  roughly double after the first full cycle. Most of it is `MLIT_BRIDGE`
  (740 k), `SHIBUYA_PEOPLECOUNT` (561 k), the ten parcel indexes (about 2 M
  together) and `MLIT_BOREHOLE` (262 k). Weekly re-runs mostly update rows
  rather than add them.
* **Host concentration.** 16 of the 36 feeds are on `news.yahoo.co.jp`.
* **Upstream quirks noted in the row descriptions:**
  * BOJ and CPI files open with metadata rows that arrive as records.
  * Japannext rows carry no date, so each issue is overwritten every run.
  * Gifu freeze and snow sensors read null outside winter.

## Problems found in the existing tree (not changed here)

* `www3.nhk.or.jp/rss/news/*` has had nothing newer than 2026-08-08; NHK moved
  to `news.web.nhk`. Rows on www3 are emitting stale items without error.
* `www.jartic.or.jp/d/traffic_info/road_traffic.json` (registered) returns 404.
* `data.bodik.jp` returned 403 to this machine for about two days after a
  discovery scan (2026-09-30); it answered 200 again on 2026-10-02. The 43
  BODIK endpoints would have failed any audit run in that window.
* CKAN datastore copies can be silently truncated relative to their source
  file: Gifu cultural properties 1,750 of 5,059, Ishikawa hydrants exactly
  10,000 of 15,133, Kanagawa parks 2,250 of 8,121. They pass every gate. Rows
  here use the source files.

## Engine gaps the beats hit (recorded in their rejects files)

* **Missing URL and file handling:**
  * a date token in URLs (JSDA daily bond quotes, JPX margin files, OCCTO
    reserve margin, utilities' dated CSVs);
  * landing page → current file;
  * paging by a cursor taken from the last record's id (bitFlyer `before=`,
    Coincheck `starting_after`).
* **Missing document shapes:**
  * a wildcard over object keys (NEXCO Central `traffic.json`, the JARTIC jam
    forecast);
  * two sibling record arrays in one document;
  * multi-section CSVs;
  * multi-sheet workbooks.
* **Unsupported formats:**
  * legacy `.xls`;
  * ZIP bodies in the probe (the engine can inflate them, but the probe can't
    read them);
  * GTFS-RT protobuf;
  * JSONP.
