# Slice 09 — 206 sources (build slot a8)

## Summary

| verdict | n |
|---|---|
| DATA | 128 (5 of them "labels-only" portal probes) |
| KEY_GATED | 26 |
| ENV_BLOCKED (Overpass) | 17 |
| EMPTY (honest) | 16 |
| DEAD_UPSTREAM | 10 |
| WAF_BLOCKED | 9 |
| CRASH / DB_ERROR / TIMEOUT | 0 after fixes |

23 sources currently return `rc=-1` and are therefore quarantined by `core/scheduler.c`:
the 17 Overpass sources, `reliefweb`, `nato-news`, and 4 WAF-blocked outlets. All are
environment/upstream failures — `rc=-1` on a genuine fetch failure is the correct contract,
so none was papered over.

### Five most important findings

**1. `EXIF_EXTRACTOR` returned its record count on success (`return 1`)** — the `rc>0`
quarantine class. Every *successful* image geolocation marked the run `error`. The
fleet-wide `rc>0` sweep missed it because the source emits 0 rows unless the pivot is a real
image URL. **Fixed + verified**: extracts `CASIO EX-FH20 / 2024:01:08 / 35.68948797,
139.69170597` from a Commons JPEG, matching Commons' own metadata exactly, now with `rc=0`.

**2. `gdacs`: 305 emits collapsed onto 38 rows, only 15 events had a pin.**
`geteventlist/MAP` returns one feature *per geometry layer* (a cyclone ships a centre point,
a track line and several impact polygons). Each event's row was overwritten by whichever
layer came last, so `has_geo` survived only when that layer was a `Point`; 23 events had no
coordinates, and the scheduler was told `records=305` for 38 rows. **Fixed → 39 rows / 39
with lat/lon / 39 with geometry.**

**3. `nws-alerts-us` was storing the literal string `"null"` in `intel_items.geometry` for
243 of 256 rows.** Zone-referenced alerts carry `"geometry": null`, and `lib/geojson.c` only
checks the key *exists* before `cJSON_PrintUnformatted()`. **Fixed locally** (13 real
polygons remain); the missing `cJSON_IsNull` guard in `lib/geojson.c:236` is a read-only
finding affecting every GeoJSON-fed source fleet-wide.

**4. Cameras never get a `link`.** `core/camera_store.c:230-238` builds the `intel_item`
with only uid/title/geo/geometry/properties — no `link`, `published_at` or `tags`, even
though `properties.url` holds the camera page. Affects `cam-worldcams` + `cam-scs_com_ua`
here and every camera channel fleet-wide. Read-only, reported.

**5. No usable Overpass endpoint from this host, and each attempt costs 141 s + a
quarantine.** `overpass-api.de` refuses instantly; kumi/openstreetmap.ru/private.coffee
hang. `overpass.osm.ch` answers in 0.14 s **but is a Switzerland-only extract (0 elements
for a Tokyo query)** — so it is *not* a valid addition to `ENDPOINTS`. What
`lib/overpass.c` needs is an overall time budget, not another mirror. 17 slice sources are
down for this reason alone.

**Runner-up: ReliefWeb has blocklisted this application's User-Agent.** Any UA containing
`japanosint` gets `406 {"error":"Blocked due to bot activity"}` from both `reliefweb.int`
and `api.reliefweb.int`; the identical request with any other UA returns the feed with 20
items. That is `lib/rss_atom.c:119` (`User-Agent: japanosint-collector`). A browser UA was
**not** used to evade it — the remedy is a descriptive UA with a contact address plus
rate-limit discipline, and that is a shared-lib decision.

## Fixes applied

| file | source(s) | bug | fix | re-test result |
|---|---|---|---|---|
| `exif_extractor.c` | EXIF_EXTRACTOR | `run()` returned **1** on success → quarantine; also silent on every path | return 0/-1 only; log fetch-fail / no-EXIF / success | `rc=0 records=1`, lat 35.68948797 lon 139.69170597, make/model/datetime present |
| `intel_gov_disaster.c` | gdacs | 305→38 rows, last-layer-wins geometry, 23 events unpinnable, inflated record count | group by (eventtype,eventid,episodeid); lat/lon from the event's Point layer else mean of its polygon vertices; keep impact polygon as geometry; drop layer-scoped `polygonlabel`/`Class`; add `geometry_layers`, `has_impact_polygon`, `coord_source` | `records=39`, 39 rows / 39 geo / 39 geometry |
| `intel_gov_disaster.c` | nws-alerts-us | literal `"null"` in the geometry column, 243/256 rows | delete JSON-null `geometry` before `geojson_emit_doc` | 256 rows, 13 with geometry (the real polygons), rest NULL |
| `unified_flights.c` | unified-flights | `label_feature()` returned early once a `title` existed, so `published_at` and `link` were never derived — all 89 rows NULL on both | fill each field independently | 89 rows, all with `link=globe.adsbexchange.com/?icao=…` and ISO `published_at` |
| `cam_worldcams.c` | cam-worldcams | title came from anchor text = the **city**; 31 cameras all titled "tokyo"; per-camera name in the URL slug discarded | parse+title-case the slug (`shibuya-crossing` → `Shibuya Crossing`); keep the anchor label as an extra centroid hint | titles `Shibuya Crossing`, `Kabukicho`, …; 30 rows / 30 geo |
| `tenki_jp.c`, `gas_outages.c`, `jcg_msi.c`, `luup_private.c`, `tiktok_jp_discover.c` | 5 portal sources | `record_type` NULL — nothing could distinguish a reachability probe from a real observation | `record_type="service-portal"` (and `"weather-observation"` for tenki.jp's scraped telop rows) | all five now `rt=service-portal=N` |

### Predecessor's uncommitted work — reviewed, re-run, all of it holds

- **`jma_intensity.c` is the headline catch.** The old `split_plus()` read ISO-6709 `cod` as
  parts[1]=lon/parts[0]=lat, so **every epicentre published at lat 36.0, lon ≈30–45 — the
  eastern Mediterranean.** Verified fixed: `熊本県熊本地方 M2.3 震度1` at 32.7/130.7, with
  magnitude, depth, JMA event id, English name and a verifiable per-bulletin link. 88 rows /
  88 geo.
- **`denki_yoho.h` + 10 `*_power.c`**: all ten TSO CSVs had moved to date-stamped files, and
  `strtok_r(",")` collapsed empty fields so `load_mw` was *the minute of a timestamp*.
  Verified: TEPCO 27,440 MW, Kansai 14,380, Chubu 12,930, Okinawa 1,030 at 2026-08-01
  03:35–03:40 JST, all titled and pinned.
- **`FAA_REGISTRY` (the interrupted rewrite) is complete and correct.** It used to emit the
  FAA site's nav anchors as 27 "registrations" for any entity including "Toyota". Verified:
  `N628TS` → 1 row, 33 parsed registry fields; `Toyota` → honest empty.
- **`trickest_cve.c`**: 16 fake Tokyo pins removed, titles added. **`intel_gov_disaster.c`**:
  USGS×3 RSS→GeoJSON (3/29/134 rows, 100 % geo), `who-outbreak-news` off a 404 RSS onto
  who.int JSON (50 rows), `cdc-newsroom` off the retired 2019-nCoV feed (1,835 rows),
  `ofac-recent-actions` off a 403 file (10 rows). **`regional_outlets_extra.c`**: 6
  repointed feeds all live (10/272/100/11/15/90 rows).
- All the honest-empty `return 0` conversions verified `rc=0`. Nothing reverted.

## Findings not fixed (with reason)

| source | issue | why not fixed |
|---|---|---|
| 17 Overpass sources | every mirror refuses/hangs; 141 s then `rc=-1` + duration outlier | environment; `lib/overpass.c` read-only. osm.ch answers but is a Swiss-only extract, so it is not a fix |
| all camera sources | `core/camera_store.c:230-238` never sets link/published_at/tags | `core/` read-only |
| every GeoJSON source | `lib/geojson.c:236` serialises JSON-null geometry as `"null"` | `lib/` read-only (worked around in nws-alerts-us) |
| reliefweb | UA blocklist → 406 | UA is in `lib/rss_atom.c:119`; evading a publisher's block is not an auditor's call |
| nato-news | nato.int has **no RSS at all** now (RSS.htm 404, no `<link rel=alternate>`, no `rss` token in the news page; AEM `general_search.search.json` 400s unauthenticated) | nothing machine-readable to point at; recommend retiring or writing an AEM scraper |
| mlit-p02-airports | both P02 GeoJSON mirrors 404; MLIT ships only per-prefecture GML zips (47 files) | `lib/mlit_ksj` reads GeoJSON; a zip+GML reader is a lib change |
| nws-alerts-us (geo) | only 13/256 alerts carry a polygon; the rest reference `affectedZones` | one fetch per unique zone on a 15-min poll; follow-up: bulk-load `api.weather.gov/zones` once and join |
| gas-outages, jcg-msi, tenki-jp, tiktok-jp-discover, luup-private | **labels-only** — a link to the operator's portal plus a `reachable` boolean, never outage/MSI/trend data (gas-outages reports `reachable:false` for all four utilities) | the "names, not data" class; real data needs per-utility scraping of pages unreachable from here, or a LUUP bearer token |
| DNS_RECORDS | single A record only, no AAAA/MX/NS/TXT, no link | `DOH_RESOLVE` already returns 32 rows for the same pivot; recommend deprecating DNS_RECORDS |
| trickest-cve | no published_at | upstream listing carries no date; deriving one would be invented |
| OPENDATA_CKAN | 67 s for 4 rows across 40 portals | serial portal walk; budget/parallelism change out of scope |
| chan-5ch, RADIO_STATIONS, TENSHOKU_KAIGI (451), ICIJ_OFFSHORE (202), amwaj-media (429), frontier-myanmar, the-new-arab, the-print-india | upstream WAF | re-tested twice |
| benar-news, caribbean-news, iss-africa, the-wire-india | feeds removed (connection refused / `/feed`→`/` 301 / 404-page-with-200 / SPA shell) | checked homepage `<link rel=alternate>` + 4 candidate paths each; no replacement exists |
| dam-water-level, drone-nofly, niconico-ranking | emit 0 rows **and log nothing** — indistinguishable from a crash. niconico returns in 0 ms, before any fetch | each needs its own parser rewrite |

## Per-source table

**Power TSOs (10, all DATA, 1 row each, 1 geo each, real+complete)** — tepco 27,440 MW ·
kepco 14,380 (was the NO_TITLE row) · chubu 12,930 · chugoku 5,720 · hokkaido 2,680 ·
hokuriku 2,640 · kyushu 8,760 · okinawa 1,030 · shikoku 2,580 · tohoku 7,290.

**arXiv feeds (44, all DATA, 0 geo, real+complete)** — cs-lg 290 · cs-ai 282 · cs-cv 201 ·
quant-ph 145 · cs-cl 139 · cond-mat-mtrl-sci 61 · cs-ro 58 · cs-cr 54 · math-oc 51 · cs-se
44 · eess-sy 40 · stat-ml 40 · cs-ir 38 · cs-hc 35 · astro-ph-im 33 · cs-ma 28 ·
physics-optics 26 · eess-sp 23 · cs-cy 22 · cs-ds 21 · cs-gt 21 · econ-gn 20 · astro-ph-ep
19 · cs-si 11 · cs-ar 10 · physics-soc-ph 10 · cs-ni 13 · nucl-ex 13 · cs-dc 17 · cs-et 9 ·
physics-ao-ph 8 · physics-geo-ph 7 · physics-med-ph 7 · q-bio-pe 6 · cs-pl 5 ·
physics-space-ph 5 · cs-db 3 · q-fin-gn 3 · cs-ne 2. **arxiv-cs-os EMPTY** (upstream RSS is
892 B / 0 items today — honest).

**Fixed this session** — EXIF_EXTRACTOR 1/1 real+complete (rc bug; verified vs Commons
metadata) · gdacs 39/39 real+complete (was 38 rows / 15 geo) · nws-alerts-us 256/13
real+complete (geometry `"null"` fixed; 243 zone-referenced upstream) · unified-flights
89/89 real+complete (link + published_at) · cam-worldcams 30/30 real+thin (city-centroid,
labelled approximate) · tenki-jp / gas-outages / jcg-msi / luup-private / tiktok-jp-discover
(record_type added; all labels-only).

**Predecessor's fixes verified** — jma-intensity 88/88 real+complete (**pins back in Japan**)
· FAA_REGISTRY 1/0 real+complete (33 fields for N628TS, honest empty for non-N-numbers) ·
trickest-cve 16/0 real+thin (no published_at upstream) · usgs-quake-m45-week 134/134 ·
usgs-quake-m25-day 30/30 · usgs-quake-significant 3/3 · who-outbreak-news 50/0 ·
cdc-newsroom 1835/0 · ofac-recent-actions 10/0 · balkan-insight-2 90/0 · semafor-africa-2
272/0 · scroll-in 100/0 · insight-crime-2 11/0 · kyiv-indep-3 15/0 · the-continent 10/0.

**Other DATA `real+complete`** — ACADEMIC_SEARCH 12 · GITHUB_CODE_SEARCH 15 · DOH_RESOLVE 32
· RDAP_IP 1 · CERTSPOTTER 65 (sweep's WAF verdict was a wrong pivot) · HACKERTARGET 51 ·
URLSCAN_SEARCH 25 · geothermal-projects 8/8 · NIH_REPORTER 20 · NSF_AWARDS 25 · gsi-geocode
1/1 · ecdc-threats 10 · fbi-press 300 · nasa-firms-blog 10 · uk-fcdo-travel 20 · un-news 30 ·
us-state-travel 224 · ipa-alerts 29 · jpcert-alerts-rss 26 · misskey-timeline 30 ·
PASSWORD_CHECKER 1 (HIBP k-anonymity, keyless) · FR_ENTREPRISES 15 · NO_BRREG 15 · allafrica
30 · bne-intellinews 13 · dialogo-americas 10 · emerging-europe 10 · hong-kong-fp 30 ·
islands-business 12 · meduza-2 30 · oc-media 16 · rnz-pacific 17 · CISA_KEV_GLOBAL 2 ·
EPSS_SCORES 1 · NVD_CVE 1 · wayback-jp 194 (47 s — duration outlier) · AVIATION_METAR 1/1
(sweep pivot was wrong; `RJTT` returns a METAR with coords) · NWS_US 14/14 ·
OPENMETEO_GLOBAL 7/7 · NHTSA_VIN 1 · novaya-europe 100 (UTF-8 corruption risk from
`lib/rss_atom.c:172`).

**DATA `real+thin`** — BGP_LOOKUP 1 (no link; predecessor's `audit-09` rc fix correct) ·
cam-scs_com_ua 238/238 (no link column — camera_store drops `properties.url`) · EN_LIGHTHOUSE
2 · DNS_RECORDS 1 (A only) · WIKIDATA 10 (no coords) · UKRI_GRANTS 25 (no published_at) ·
europol-news 10 (no published_at) · gvp-volcano-activity 22 (GVP RSS carries no coords) ·
jma-volcano 37 (no coords) · SERVERFAULT_SEARCH 20 · SUPERUSER_SEARCH 18 · OPENDATA_CKAN 4
(4 rows / 40 portals / 67 s) · SEC_EDGAR_SEARCH 44 (38/44 rows no link).

**KEY_GATED (26)** — alienvault-otx-jp · ARBISCAN/BASE/BSCSCAN/ETHERSCAN/GNOSIS/OPTIMISM/
POLYGONSCAN_BALANCE (7) · CIRCL_PDNS · estat-census · flickr-geo · HUNTER_IO · ZOOMEYE ·
instagram-geo · HYBRIDANALYSIS_HASH · VIRUSTOTAL_LOOKUP · DATALASTIC_VESSEL · netlas-jp ·
shodan-cameras-jp · YOUTUBE_SEARCH · virustotal-jp · windy-japan · yahoo-map-api ·
REALPROPERTY_REGISTRY (paid) · ZENRIN_JUTAKU (paid) · UK_COMPANIES.

**ENV_BLOCKED, Overpass (17)** — 5g-coverage · bosai-shelter · courts-prisons (1,139 rows in
the 19:26 sweep) · highway-traffic · jsdf-bases · koban-map · museums (3,812 rows in the
sweep) · nuclear-facilities · osm-transport-subways · pharmacy-map · port-infra · racetracks
(240 rows in the sweep) · resas-tourism · sake-breweries · steel-mills · tabelog-restaurants
· usfj-bases.

**DEAD_UPSTREAM (10)** — OPENWORK (404) · CADASTRAL_PARCELS (404) · HAZMAT_FACILITIES (404) ·
TOKYO_MOU_PSC (404) · mlit-p02-airports (P02 GeoJSON 404; GML zips only) · nato-news (no RSS
exists) · benar-news · caribbean-news · iss-africa · the-wire-india.

**WAF_BLOCKED (9)** — chan-5ch (403) · TENSHOKU_KAIGI (451 geo-block) · ICIJ_OFFSHORE (202
challenge) · RADIO_STATIONS (403) · reliefweb (406, UA blocklist) · amwaj-media (429) ·
frontier-myanmar (403) · the-new-arab (403) · the-print-india (JS challenge).

**EMPTY (16)** — honest: arxiv-cs-os · ROSENKA · DRONE_REGISTRY · SECURITY_SE_SEARCH ·
BE_KBO · PARIS_MOU_PSC · USCG_PSIX · EXPLOITDB · EUROPE_REGISTRY (0 rows across 25
registries). Connection failure: CHIKAMAP (status 0) · GSI_HISTORICAL (status 0). Silent
failure — logs nothing, indistinguishable from a crash: dam-water-level (river.go.jp CGI
returns 173 B) · drone-nofly (page 200/27 KB, parser matches nothing) · niconico-ranking (0
ms, exits before any fetch) · nowphas-wave (page 200/21 KB) · wifi-hotspots-jcfw (page
200/11 KB).

## Build state

`bash tests/audit/agent_build.sh a8` → **OK** (21:25:11), no warnings from any touched file.
Files edited (all in-slice): `intel_gov_disaster.c`, `unified_flights.c`, `cam_worldcams.c`,
`exif_extractor.c`, `tenki_jp.c`, `gas_outages.c`, `jcg_msi.c`, `luup_private.c`,
`tiktok_jp_discover.c`. No commits, checkouts or stashes.
