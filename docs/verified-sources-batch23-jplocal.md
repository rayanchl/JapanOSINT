# Batch 23 — beat `jplocal` (Source Agent J2): 100 rows

Japanese prefectural and municipal open-data catalogues off the beaten track,
plus a handful of prefectural police / prefectural CMS feeds and one legacy
prefectural GIS download index:

* **six CKAN portals** — Akita (`ckan.pref.akita.lg.jp`, 12 departments incl.
  the prefectural police HQ), Gifu (`gifu-opendata.pref.gifu.lg.jp`, 23
  publishing organisations: four prefectural departments and nineteen cities,
  towns and villages), Yamaguchi (`yamaguchi-opendata.jp/ckan`, the prefecture
  and all nineteen municipalities), the Kanazawa city-region joint catalogue
  (seven municipalities), Toyama City, Miyazaki (`ckan.pref.miyazaki.lg.jp`,
  4,937 datasets, mostly disaster-data releases) — one row per publishing
  organisation, a `{q}` full-text pivot per portal, and the organisation
  register per portal;
* **seven SHIRASAGI opendata portals** whose `api/package_search` is
  CKAN-shaped but not CKAN — Hokkaido HARP (1,021 datasets), Aomori, Shizuoka,
  Tottori (on a vendor domain, `odp-pref-tottori.tori-info.co.jp`), Tokushima,
  Kagawa, Ehime — a full listing and a `{q}` pivot each;
* **three ArcGIS Hub sites** (Shibuya City, Aizuwakamatsu City, the ESRI Japan
  government portal) via their DCAT-US feeds, and the Hub search API as a
  `{q}` pivot for the two municipal ones;
* the national **e-Gov data portal** CKAN (18,141 datasets; not registered
  anywhere in the tree) as a `{q}` pivot and a ministry register — two rows,
  kept deliberately small because it is national, not local;
* **four feeds**: Hyogo Prefectural Police what's-new RSS, the Kumamoto
  Prefectural Police site feed, the Okayama organised-crime division feed,
  Saitama's featured-information feed;
* **Okinawa Prefecture's legacy GIS** download index (112 zipped layers).

Manifest (the source of truth): `docs/candidate-sources-batch23.jplocal.txt`.
Generated table: `native/collectors/pivot/table/hp3b23_jplocal.c` (100 rows,
`gen_hp_batch.py --prefix hp3b23 --batch 23`, never hand-edited). Rejects as
data: `docs/rejected-sources-batch23-jplocal.tsv`.

Every row was fetched over the wire from this machine, parsed in its declared
mode, run through the **real binary**, read back as `emitted N of M`, and run
again against a **fresh per-row scratch database** to read `stored=` off the
run line (house rule 4b). Nothing was authored from documentation.

Sibling batch-23 manifests were read before authoring so as not to collide:
`jpmuni` (190 municipal `www.city.*` RSS feeds), `jpopendata` (geospatial.jp
organisations), `jpbulk` (Sapporo / Tokyo catalogue), `jpbosai` (JMA). None of
those hosts appears here. BODIK (`data.bodik.jp`), Yokohama, Minato and
geospatial.jp are already registered and were not touched.

## Attrition

| stage | in | out | dropped |
| --- | --- | --- | --- |
| pre-screen over the wire (own `urllib` pass; 521 host/path guesses across all 47 prefectures and 17 cities, plus the portals named by search.ckan.jp / the Qiita prefectural-catalogue survey / awesome-japan-opendata) | ~600 URLs | 101 rows authored | DNS failures (opendata.pref.toyama.jp, open-governmentdata.org, catalog.registries.digital.go.jp), 403 (BODIK, odp.jig.jp), no API (Saitama portal, Sabae, the four dataeye portals), timeouts (Otsu, Osaka city, opendatastack), the Fujitsu-CMS `rss/10/list1.xml` guess 404 on 22 prefectural hosts |
| `probe_hp_batch.py` | 101 | 100 | Gunma `list6.xml` (RSS 1.0, one item, unparseable) |
| `--check-filter` | 100 | 100 | 3 `FILTER_IGNORED` fixed rather than dropped (see below); 2 `NET_ERR` recovered serially |
| `batch_exclusions.py`, pagination, reachable | 100 | 100 | — |
| `audit_batch_emit.py` + rule 4b | 100 | 100 | 1 `TRANSPORT_FAIL` fixed (Shizuoka, see below); 6 serial re-runs recovered |

Registry: 14,115 → **14,215** seeds in the built binary (`lint-sources`
counts `REGISTER_SOURCE` only and stays at 10,438).

## Gate results (quoted)

| gate | result |
| --- | --- |
| `gen_hp_batch.py ../docs/candidate-sources-batch23.jplocal.txt --outdir /tmp/gen23j2 --prefix hp3b23 --batch 23` | `100 manifest rows` → `/tmp/gen23j2/hp3b23_jplocal.c 100 rows` / `total 100 rows`; no duplicate opts, no swallowed `\;` |
| `probe_hp_batch.py --jobs 4` (first authoring, 101 rows) | `# 98/101 PASS` — `JPL_AKITA_CKAN_SEARCH_PIVOT` / `JPL_TOYAMACITY_CKAN_SEARCH_PIVOT` `EMPTY_RESULTSET` (probe entity `AED` has no hit on those two portals; probe changed to `csv` / `2024` → `# 2/2 PASS`), `JPL_GUNMA_PREF_RSS_LIST6` `UNPARSEABLE` (dropped) |
| `probe_hp_batch.py --check-filter --jobs 3` | `# 95/100 PASS`: `JPL_AKITA_CKAN_SEARCH_PIVOT FILTER_IGNORED filter ignored: an impossible entity returned 88 records vs 28 for the real one`, `JPL_GIFU_CKAN_SEARCH_PIVOT … 100 records vs 39`, `JPL_TOYAMACITY_CKAN_SEARCH_PIVOT … 100 records vs 8`; `JPL_GIFU_CKAN_ORG_30040` / `_30190` `NET_ERR Temporary failure in name resolution`. Those three portals' Solr ORs the tokens of an unquoted query (`zzqx9nonexistent7q` is analysed into `zzqx`/`9`/`nonexistent`/`7`/`q` and the digits match). Re-pointed at the quoted-phrase form `q=%22{q}%22` (impossible entity → `count=0` on all three, real entity unchanged: 28 / 39 / 8) and re-run serially with the two NET_ERR rows: `# 5/5 PASS`. **0 `FILTER_IGNORED` in the shipped manifest.** The other 12 `{q}` pivots (Yamaguchi, Kanazawa, e-Gov, the seven SHIRASAGI portals, two Hub sites) passed unquoted |
| `batch_exclusions.py --check … --bin ./bin/japanosint --skip-prefix hp3b23_jplocal` (against the 14,115-id registry, before the table was installed) | `id set: 14115 from the binary's registry` / `collisions: 0   near-misses needing a human read: 0`. By-name grep: every host in the manifest was grepped against the registry URL dump and `native/collectors`; the three hosts that do appear (`www.pref.kumamoto.jp`, `www.pref.okayama.jp`, `www.pref.saitama.lg.jp`, `www.police.pref.hyogo.lg.jp`) are registered for OTHER feeds (`rss/10/list1|3|6.xml`, `news/news.xml`, the crime CSV) — the rows here are different documents on those hosts |
| `audit_batch_pagination.py` | `0 of 100 rows declare no pagination but look paged  (13 declared pagination_ok)` |
| `audit_batch_reachable.py` | `0 of 100 rows can never run` |
| `audit_batch_emit.py --bin ./bin/japanosint --jobs 3 --timeout 400` | `OK=99  TRANSPORT_FAIL=1` / `records emitted across the run: 7,338`; 0 `DROPS_EVERYTHING`, 0 `SLOW`. The failure was `JPL_SHIZUOKA_ODP_ALL_DATASETS`: `opendata.pref.shizuoka.jp` needs **88 s** to serve a 100-row page (1.17 MB) and the engine's transport limit is 40 s; 50 rows take 33 s, 20 rows 5.5 s. Re-declared as `rows=25` with `page_size=25;page_max=30` (750 ≥ 669 datasets) for both Shizuoka rows → re-probed `PASS` (25 items each) and re-run: `OK=2 / records emitted across the run: 713` |
| **rule 4b** — `audit_registry_emit.py --match '^JPL_' --jobs 3 --timeout 400` (fresh copy of the warm template DB per row, `stored=` read off the run line AND counted back out of the database) | 100 rows measured: `79 OK`, `21 EMITS_NOTHING`. The 21 split: **15 are the `{q}` pivots**, which the registry sweep runs without an entity (`sched_rc=0, emitted=0`) — they were then run one by one with `--run <id> <entity>` against a fresh `JO_DB` each (table below); **6 were transport failures under three-way parallel load** (five Akita rows and the Shizuoka listing, `sched_rc=-1`), re-run serially: `OK=6 … records emitted: 788, rows stored: 788, records lost to uid collision: 0`. Over the 79 first-pass OK rows: `emitted=6019 stored=6019`. **0 `UID-COLLISION` on any of the 100 rows; stored == emitted everywhere** (static rows 6,807 = 6,807; pivots 1,202 = 1,202) |
| `make` | 0 warnings (`make 2>&1 \| grep -ci warning` → `0`) |
| `make audit-sources` | `files with findings: 0` / `findings           : 0` / `strict set (collectors/pivot/table/hp*_*.c): 0 finding(s)` |
| `make lint-sources` | `registered source_defs: 10438  (1114 direct + 9324 macro-expanded)` / `lint-sources: OK` |
| `make hptest` | `… ok    a Shift_JIS body from a .jp host is transcoded to UTF-8 before the parse … all passed` |
| `make unit` | `… ok: catalogue = 1739 of 1740 entity pivots (3460 scheduled rows excluded) … test_search_degradation: OK` |

## Rule 4b — the fifteen pivots, run with an entity against a fresh database

| row | entity | run line |
| --- | --- | --- |
| `JPL_AKITA_CKAN_SEARCH_PIVOT` | csv | `rc=0 records=28 3607ms stored=28` |
| `JPL_GIFU_CKAN_SEARCH_PIVOT` | AED | `rc=0 records=39 2405ms stored=39` |
| `JPL_YAMAGUCHI_CKAN_SEARCH_PIVOT` | AED | `rc=0 records=18 3121ms stored=18` |
| `JPL_KANAZAWA_CKAN_SEARCH_PIVOT` | AED | `rc=0 records=7 1453ms stored=7` |
| `JPL_TOYAMACITY_CKAN_SEARCH_PIVOT` | 2024 | `rc=0 records=8 1804ms stored=8` |
| `JPL_EGOV_DATA_SEARCH_PIVOT` | AED | `rc=0 records=1001 28469ms stored=1001` (ten-page ceiling + truncation notice) |
| `JPL_HOKKAIDO_HARP_SEARCH_PIVOT` | AED | `rc=0 records=33 4861ms stored=33` |
| `JPL_AOMORI_ODP_SEARCH_PIVOT` | AED | `rc=0 records=2 1331ms stored=2` |
| `JPL_SHIZUOKA_ODP_SEARCH_PIVOT` | AED | `rc=0 records=45 8040ms stored=45` |
| `JPL_TOTTORI_ODP_SEARCH_PIVOT` | AED | `rc=0 records=1 1762ms stored=1` |
| `JPL_TOKUSHIMA_ODP_SEARCH_PIVOT` | AED | `rc=0 records=4 2719ms stored=4` |
| `JPL_KAGAWA_ODP_SEARCH_PIVOT` | AED | `rc=0 records=4 3355ms stored=4` |
| `JPL_EHIME_ODP_SEARCH_PIVOT` | AED | `rc=0 records=10 2998ms stored=10` |
| `JPL_SHIBUYA_HUB_SEARCH_PIVOT` | AED | `rc=0 records=1 419ms stored=1` |
| `JPL_AIZUWAKAMATSU_HUB_SEARCH_PIVOT` | AED | `rc=0 records=1 519ms stored=1` |

## Largest results

`JPL_EGOV_DATA_SEARCH_PIVOT` 1,001 (ceiling), `JPL_HOKKAIDO_HARP_ALL_DATASETS`
1,000 of 1,021 (ceiling, truncation notice), `JPL_MIYAZAKI_CKAN_ALL_DATASETS`
1,000 of 4,937 (ceiling, truncation notice), `JPL_TOKUSHIMA_ODP_ALL_DATASETS`
877, `JPL_SHIZUOKA_ODP_ALL_DATASETS` 668, `JPL_AOMORI_ODP_ALL_DATASETS` 220,
`JPL_KANAZAWA_CKAN_ORG_KANAZAWA` 198, `JPL_TOTTORI_ODP_ALL_DATASETS` 187,
`JPL_ESRIJ_GOV_HUB_DCAT` 186, `JPL_GIFU_CKAN_ORG_30050` 174,
`JPL_TOYAMACITY_CKAN_ORG_TOYAMA` 133, `JPL_SHIBUYA_HUB_DCAT` 122,
`JPL_KAGAWA_ODP_ALL_DATASETS` 113, `JPL_OKINAWA_GIS_OPENDATA_INDEX` 110,
`JPL_KUMAMOTO_POLICE_RSS` 100.

Titles were read back out of a scratch database for a SHIRASAGI row, a CKAN
row, the Okinawa HTML index, the Hyogo RSS and the Shibuya DCAT feed — all
UTF-8 Japanese, no mojibake (e.g. `宮古島断層(shape)（ZIP：464KB）`, `薬局一覧`,
`子育て関連施設一覧&ひろば`).

## Things worth knowing that the tools did not say

* **SHIRASAGI's `api/package_search` is CKAN-shaped but not CKAN.** The path
  is `/api/package_search` (not `/api/3/action/…`), records carry `name` /
  `uuid` / `updated` / `url` rather than `title` / `id` /
  `metadata_modified`, and **`start` is required and must be ≥ 1** (`rows=2`
  alone → `Validation Error: start Must be a natural number`). The base URL
  therefore carries `start=1`; the engine appends `&start=101` on the next
  page and the server takes the last value (verified: `start=1&start=101`
  returns the same ids as `start=101` and not those of `start=1`). Declared as
  `page_param=start;page_size=100;page_start=1`.
* **Three CKANs OR the tokens of an unquoted query** (Akita, Gifu, Toyama
  City — older CKAN/Solr with a Japanese analyser that splits on digit
  boundaries). Unquoted, the prober's impossible entity matched 88–100
  datasets; quoted as a phrase it matches 0. Yamaguchi, Kanazawa and e-Gov
  return 0 unquoted. The three rows send `q=%22{q}%22`.
* **Miyazaki's CKAN matches an impossible keyword to 544 of 4,937 datasets**
  even quoted — no `{q}` pivot was authored for it (the full listing and the
  `bosai-data` organisation are shipped instead). Rejected as
  `FILTER_IGNORED` before it reached the manifest.
* **`opendata.pref.shizuoka.jp` serves ~12 KB per dataset record and takes
  88 s for 100 of them**, past the engine's 40 s transport limit, with no
  error other than `transport failure`. `rows=25` is 6 s. `page_max=30`
  covers the 669-dataset catalogue.
* `data.bodik.jp` (the Kyushu/Kansai shared CKAN behind `odcs.bodik.jp/<pref
  code>/`, 18,390 datasets) answers **403 from nginx** to every request from
  this network, IPv4 or IPv6, any User-Agent. It is also already registered.
  Ten prefectural catalogues (Tochigi, Osaka, Kyoto, Hyogo, Shiga, Wakayama,
  Saga, Nagasaki, Oita, Kumamoto, Kagoshima, Okinawa) sit behind it and are
  unreachable from here for that reason alone.
* Hosts that resolve only to IPv6 from WSL and fail with `Errno 101` in
  `urllib` (Gifu, Yamaguchi, BODIK, Saitama, ArcGIS Hub) work over IPv4; the
  Python tools were run with a `sitecustomize.py` that prefers `AF_INET`
  results from `getaddrinfo`. libcurl in the engine falls back on its own and
  needed nothing.
* Under `--jobs 3` the Akita CKAN dropped five of fourteen requests
  (`sched_rc=-1`); serially it answered every one. Its rows are daily jobs so
  the scheduler will not hit it three-wide.
* `pref.ehime.jp` began answering **403** to the catalogue API from the WSL
  build host after roughly a dozen requests in ten minutes (probe,
  check-filter, emit, 4b, a manual check) and was still 403 there 75 s and
  then ten minutes later — while `curl` from the Windows side of the same
  machine (same public address) got **200** for the identical URL at the same
  moment, with either User-Agent. So it is a per-client throttle keyed on
  something other than the IP (the TLS client hello, most likely), not a host
  block; the engine's earlier runs from the same WSL client all succeeded.
  See "Not verified".

## Not verified

* `JPL_EHIME_ODP_ALL_DATASETS` emits **1** record: the unfiltered
  `package_search` on Ehime's portal reports `count=2`, while `q=AED` reports
  10 — the portal's empty-query listing is evidently not the whole catalogue,
  and a follow-up check of `q=*` / `q=県` could not be completed because the
  host had started returning 403 (rate limit; the earlier runs all
  succeeded). Both Ehime rows pass every gate and emit what the server
  returns; the listing row is kept as an honest (small) window and the pivot
  is the useful one.
* `JPL_OKAYAMA_ORGCRIME_RSS` (1 item), `JPL_TOTTORI_ODP_SEARCH_PIVOT`,
  `JPL_SHIBUYA_HUB_SEARCH_PIVOT`, `JPL_AIZUWAKAMATSU_HUB_SEARCH_PIVOT` (1 hit
  for `AED`) emitted a single record on the probe entity; a one-record run
  cannot show a collision defect.
* The Hub search pivots declare `next_path=links.rel=next.href`; neither
  municipal Hub returned more than 100 hits for any entity tried, so the
  next-link walk was not exercised on the wire.
* `batch_exclusions.py` cannot be re-run meaningfully after the table is
  installed (every row then "collides" with itself); the 0-collision reading
  above is the pre-install run against the 14,115-id registry.
