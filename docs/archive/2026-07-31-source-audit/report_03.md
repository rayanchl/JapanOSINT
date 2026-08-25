# Slice a2 (auditor 3) — 207 sources

## Summary

| verdict | n | notes |
|---|---|---|
| DATA | 161 | 142 `gnews-top-*` + 19 others |
| LABELS_ONLY | 7 | rows that carry a name/link and no measurement |
| KEY_GATED | 13 | 10 gate on a local env var; 3 are newly gated *upstream* |
| EMPTY (honest) | 8 | fetch succeeded, upstream had nothing / is retired |
| DEAD_UPSTREAM (rc=-1) | 4 | domain gone or endpoint 401/404 |
| WAF_BLOCKED | 4 | 403 / 429 from the upstream edge |
| BLOCKED — unverifiable | 10 | every Overpass endpoint refuses this host right now |
| CRASH | 0 | |

### The five findings that matter most

1. **`lib/rss_atom.c` truncates `summary` mid-UTF-8-character — corrupts the DB
   for every RSS source in the product, not just my slice.**
   `rss_atom.c:172-173` does `strncpy(summ, desc, 240); summ[240] = 0;`. A
   240-*byte* cut lands inside a multi-byte sequence whenever the feed is
   non-Latin, so the `summary` column (and its FTS mirror) receives invalid
   UTF-8. That is the `DB_ERROR: OperationalError: Could not decode to UTF-8`
   the sweep saw on gnews-top-am/az/bd/by/il/jp/kg/kz/ru/tj/tm/uz — exactly the
   twelve Armenian/Azeri/Bengali/Belarusian/Hebrew/Japanese/Kyrgyz/Kazakh/
   Russian/Tajik/Turkmen/Uzbek editions, and it fires or not per run depending
   on where the cut lands. `lib/` is read-only for me. **Fix is one line:** walk
   back off any `0x80..0xBF` continuation byte before terminating. This affects
   every `rss_collect` caller across all ten slices.

2. **Physical things that could not be pinned — now fixed.** `bike-share-gbfs`
   had lat/lon for all 1000 docks in `properties` but never set
   `has_geo`/`lat`/`lon`, so not one dock was mappable. `landsat-jp` computed a
   scene centroid, stored it in `properties`, then hard-coded `it.has_geo = 0`
   and dropped the acquisition footprint polygon. Both fixed and re-run:
   1000/1000 and 50/50 rows now carry coordinates, and Landsat rows carry the
   real footprint `Polygon`.

3. **`geojson_emit_features` only reads `title|name|name_ja|label` out of
   `properties`, so five of my sources persisted rows with no title, no link
   and no timestamp** — invisible in every UI. `jma-weather`, `openmeteo-jma`,
   `osv-dev`, `wolfx-eew`, `mlit-river`. All five fixed by adding the missing
   keys, built from fetched values only. Note the collateral: for features with
   no NATIVE_ID key the uid is `sha1({g,p})`, so adding properties re-keys those
   rows once (`jma-weather`, `openmeteo-jma`, `osv-dev`); `wolfx-eew` and
   `mlit-river` have `event_id`/`station_id` and are unaffected.

4. **A "portal-status" family emits placeholder rows instead of data** — one row
   per source whose entire content is a title, a link and *"Set FOO_KEY to
   enable"*. Seven in my slice: `intelx-leaks`, `twitch-jp-streams`,
   `vrchat-active-jp`, `strava-segments-jp`, `jpo-trademarks`,
   `earthquake-network-citizen`, plus `pref-police-crime` (36 rows tagged
   `directory-only` with `"rows_available": 0` and no coordinates). These read
   as results in the feed but contain no measurement. The pattern comes from
   `lib/probe.h` and is used well beyond my slice, so I did **not** unilaterally
   change it — a key-gated source should log `gated (no FOO_KEY)` and emit
   nothing, like `docomo-insight` correctly does. This is an orchestrator-level
   decision.

5. **Six upstreams are dead or newly closed; six more silently looked like "no
   results".** Retired: `japanapi.curtisbarnard.com` (NXDOMAIN),
   `www.land.mlit.go.jp` (NXDOMAIN — MLIT moved trade data to
   reinfolib.mlit.go.jp behind a key), `jp.mercari.com/v1/web/suggest/trends`
   (404), both `nlftp.mlit.go.jp` C02 mirrors (404), 9 of 10 prefectural-police
   scam pages (404), `www.nicter.jp/atlas/` (404 — the slashless path is fine).
   Newly key-gated upstream with no local gate: OpenCorporates v0.4 (401
   "Invalid Api Token") → `COMPANY_LOOKUP`; abuse.ch MalwareBazaar (401) →
   `HASH_LOOKUP`; api.opensanctions.org (401 "No API key provided") →
   `SANCTIONS_CHECK`; RepeaterBook export (401 `auth_missing`) →
   `amateur-radio-repeaters`; GDELT (429) → `NEWS_ARCHIVE`. All five returned a
   bare empty that was indistinguishable from "the entity has no records"; each
   now logs the real reason.

### Environment caveat that limits this report

Every Overpass endpoint (`overpass-api.de`, `kumi.systems`, `openstreetmap.ru`,
`private.coffee`) refuses or times out from this host as of 2026-07-31 ~20:30 —
`overpass-api.de:443` gives an immediate connection refusal, consistent with an
IP-level quota ban triggered by the bulk sweep. `mlit-river` pulled 1193 rows
through the same helper during the sweep window (19:26–19:55), so the code path
works; the 10 Overpass-backed sources in my slice simply cannot be verified now
and are reported as BLOCKED rather than broken.

## Fixes applied

| file | source(s) | bug | fix | re-test result |
|---|---|---|---|---|
| `collectors/sources/bike_share_gbfs.c` | `bike-share-gbfs` | lat/lon fetched from GBFS `station_information` but only written into `properties`; `intel_item.has_geo` never set → 1000 unpinnable docks. Also `return n > 0 ? 0 : -1` | promote lat/lon (range-checked, 0/0 rejected) onto the row; add `record_type="bike-share-station"`; honest `return 0` | `rc=0 records=1000` — **1000 rows, 1000 with lat/lon** (was 0). e.g. `新御徒町ステーション` 35.707252/139.777587, "0 bikes / 8 docks" |
| `collectors/sources/landsat_jp.c` | `landsat-jp` | `it.has_geo = 0` hard-coded although the STAC feature carries the acquisition footprint and a centroid is already computed; footprint dropped. `return n > 0 ? 0 : -1` | set `has_geo`+lat/lon from the existing centroid, carry the STAC `geometry` into `geometry_geojson`; honest `return 0` | `rc=0 records=50` — **50 rows, 50 lat/lon, 50 geometry**; row 1 `LC09_L2SP_112041_…` lat 27.634 lon 129.917 with the real 5-point Polygon |
| `collectors/sources/jma_weather.c` | `jma-weather` | row had no title/summary/link/published_at (`geojson_emit_features` finds none of its keys in `properties`) | add `title` from `targetArea`, `summary` from the forecast `text`, `link`, `published_at` from `reportDatetime` | `title 東京都 の天気概況`, full forecast text as summary, `published_at 2026-07-31T16:37:00+09:00` |
| `collectors/sources/openmeteo_jma.c` | `openmeteo-jma` | same — 9 title-less rows | `title`/`summary` formatted from the fetched `current` block, per-city `link` | `Sapporo 18.2°C` / `Sapporo: 18.2°C, wind 7.9 m/s, precip 0.0 mm` ×9 |
| `collectors/sources/osv_dev.c` | `osv-dev` | 51 advisories with no title and no way back to osv.dev | `title` = `<OSV id> — <package>`, `link` = `https://osv.dev/vulnerability/<id>`, `published_at` = `modified` | `GHSA-496g-mmpw-j9x3 — npm:misskey-js` + link + date, ×51 |
| `collectors/sources/wolfx_eew.c` | `wolfx-eew` | EEW row had no title and no timeline placement | `title` from hypocentre/magnitude/max-intensity; `link`; `OriginTime` (JST `YYYY/MM/DD hh:mm:ss`) normalised to ISO+09:00 | `緊急地震速報 奄美大島北西沖 M3.9 最大震度2`, `published_at 2026-07-31T21:58:43+09:00` |
| `collectors/sources/mlit_river.c` | `mlit-river` | gauge name stored as `station_name`, which is not in the title key list → 1193 title-less rows | mirror the same OSM value into `name`; add OSM node `link`. `station_id` is a NATIVE_ID key so uids are unchanged | **not re-verified** — Overpass is IP-blocked from this host (see caveat). Compiles clean |
| `collectors/sources/nicter_stats.c` | `nicter-stats` | URL `…/atlas/` 404s → `rc=-1` on every run (permanent quarantine). And on success it would emit one Feature with all three metrics `null` | URL → `https://www.nicter.jp/atlas` (200); if no counter parses, emit nothing and `return 0` with a diagnostic | `rc=0 records=0`, logs `no counters in 12066 bytes … live figures are served at https://www.nicter.jp/top10` |
| `collectors/sources/phone_scam_hotspots.c` | `phone-scam-hotspots` | NPA gate URL 404s → `rc=-1` before the prefectural pages were ever tried; per-page failures silent | gate → `https://www.npa.go.jp/bureau/safetylife/sos47/` (200); log each unreachable prefectural page | `rc=0 records=0` and 9 named 404 URLs in the log (was an unexplained `rc=-1`) |
| `collectors/sources/homes_co.c` | `homes-co` | `return n > 0 ? 0 : -1` — an upstream with no listing count quarantined the source | count failed fetches; `-1` only when *all 47* pages fail, else honest `0` | `rc=0 records=0 — 0/47 prefecture pages unreachable` (pages load, they just no longer render a `件` count server-side) |
| `collectors/sources/tochi_info.c` | `tochi-info` | same `n > 0 ? 0 : -1`; 47 silent failures | count failed fetches; `-1` only on a total failure, with an explicit "host does not resolve" line | `rc=-1` (correct — genuinely dead) but now says `www.land.mlit.go.jp no longer resolves; MLIT moved the trade data to reinfolib.mlit.go.jp` |
| `collectors/sources/theharvester.c` | `EMAIL_HARVESTER` | POSIX ERE has no look-behind, so percent-encoded SERP links produced junk subdomains (`2Fwww.toyota.co.jp` from `%2Fwww.toyota.co.jp`). Also the pivot domain went into the regex with unescaped `.` | reject a match whose preceding byte can still be part of a host token; escape the domain before building the pattern | before: `2Fwww.toyota.co.jp`; after: `www.toyota.co.jp`, `rent.toyota.co.jp`, `idm.kitora.toyota.co.jp` |
| `collectors/sources/company_lookup.c` | `COMPANY_LOOKUP` | silent empty on OpenCorporates 401 | log the status and the reason | `OpenCorporates http status=401 (v0.4 now requires an api_token) — 0 rows` |
| `collectors/sources/hash_lookup.c` | `HASH_LOOKUP` | silent empty on MalwareBazaar 401 | log the status and the reason | `MalwareBazaar http status=401 (abuse.ch now requires an Auth-Key header)` |
| `collectors/sources/sanctions_check.c` | `SANCTIONS_CHECK` | silent empty read as "no sanctions match" | log the OpenSanctions gate | `OpenSanctions search returned nothing (api.opensanctions.org now requires an API key)` |
| `collectors/sources/news_archive.c` | `NEWS_ARCHIVE` | GDELT throttling indistinguishable from "no coverage" | log rc + HTTP status | `GDELT rc=0 http status=429 — 0 articles` |
| `collectors/sources/patent_search.c` | `PATENT_SEARCH` | key gate returned silently → classified EMPTY, not KEY_GATED | log `gated (no PATENTSVIEW_API_KEY)` | `[patent-search] gated (no PATENTSVIEW_API_KEY)` |
| `collectors/sources/amateur_radio_repeaters.c` | `amateur-radio-repeaters` | unexplained `rc=-1` | log the failed URL and detect the RepeaterBook `auth_missing` body | `no CSV from https://www.repeaterbook.com/api/export.php?country=Japan` (upstream is a hard 401) |
| `collectors/sources/japan_api_prefectures.c` | `japan-api-prefectures` | unexplained `rc=-1` | log the retired host | `no JSON from https://japanapi.curtisbarnard.com/… (host does not resolve — upstream retired)` |
| `collectors/sources/mercari_trending.c` | `mercari-trending` | unexplained `rc=-1` | log the 404 endpoint | `no JSON from https://jp.mercari.com/v1/web/suggest/trends (endpoint returns 404 + HTML shell)` |

## Findings not fixed (with reason)

| source | issue | why not fixed |
|---|---|---|
| all 142 `gnews-top-*` | `summary` cut at 240 bytes mid-UTF-8 → `Could not decode to UTF-8` in `intel_items.summary` and FTS | bug is in `lib/rss_atom.c:173`, which is READ-ONLY for me and shared by every RSS source in the product |
| all 142 `gnews-top-*` | `summary` and `body` are a raw `<ol><li><a href…>` Google related-articles blob, not prose; `published_at` is RFC-822 (`Fri, 31 Jul 2026 16:45:00 GMT`) not ISO — the file's own comment says "RFC822→ISO norm: P5 refine" | same file, `lib/rss_atom.c` |
| `osv-dev`, `ghsa-advisories`, `yahoo-realtime`, `nicter-stats` | every row pinned to a hard-coded Tokyo point (35.6895/139.6917 or NICT HQ). A CVE, a GHSA advisory and a search term are not places — 151 rows of invented coordinates land on the map | removing the pin changes map behaviour for a whole cross-slice family; it is a product decision, and the brief forbids me inventing a better coordinate |
| 7 portal-status sources (finding 4) | placeholder rows with no measurement | driven by `lib/probe.h`, used far beyond my slice — needs one consistent decision, not seven local ones |
| `jcg-patrol` | fetches the JCG index page, **discards the response**, and always emits 0. It can never produce data by construction | there is no upstream to point it at; it should be de-registered or re-implemented against a real JCG dataset |
| `jma-weather` | fetches office `130000` only — 1 of 47 JMA forecast offices. 46 prefectures are simply not collected | expanding needs a vetted table of 47 office coordinates; JMA's `area.json` has no coordinates and I will not type them from memory |
| `osv-dev` | `slice(0,6)` per package — OSV returns every known vuln, only 6 per package are emitted (51 rows from 15 packages) | deliberate JS-parity cap; raising it is a product call, not a bug fix |
| `google-my-maps` | is not Google My Maps data at all. With no `GOOGLE_MYMAPS_IDS` it runs the *identical* Overpass tourism/historic query as `famous-places`, with the identical feature shape → two source IDs producing the same rows | duplication is by design in the port; deduplicating is an orchestrator decision |
| `lib/overpass.c` | `collect_fetch` tries 4 endpoints at the **full** timeout each and logs only `no elements` — it never records the HTTP status, so a 429 ban looks the same as an empty query. `overpass_tiled_collect` is worse: 12 tiles × 4 endpoints × 90 s = ~72 min worst case, which is what made `famous-places`/`google-my-maps` blow the 180 s sweep budget. `tiled_fetch` also `calloc`s 1.6 MB for the dedupe set on every call and scans it linearly (O(n²) up to 200 k keys) | `lib/` is READ-ONLY |
| `EMAIL_HARVESTER` | 10 Google SERP fetches per run return nothing (Google blocks scraping) and cost ~30 s of the 37 s runtime; only DuckDuckGo yields hits and crt.sh returned none | changing the source mix is a design change, not a defect fix; flagging the wasted latency |
| `pref-police-crime` | 47 sequential prefecture fetches → never returns inside 180 s; emits 36 `directory-only` rows with `rows_available: 0` and no coordinates | needs a real crime-statistics endpoint per prefecture, which does not exist as a uniform API |
| `jma-ocean-wave` | works and is exhaustive (JMA `pointinfo.json` really does list only 6 stations), but `observed_at` on every row is `2026-03-25` — 128 days stale upstream | upstream data staleness, not a collector bug |
| `CA_CORPORATIONS` | emits 0 for `Shopify` and logs only `emitted 0` with no reason | ran out of audit budget to trace which of its portals failed; recommend the same status-logging treatment applied to the other five |
| `PL_KRS` | honestly refuses a company *name* — it needs a 10-digit KRS number (`entity not a 10-digit KRS number — empty`). The sweep's `company=Toyota` pivot can never work | correct behaviour; the source registry should declare the pivot type |
| 10 Overpass sources | unverifiable | all four Overpass endpoints refuse this host (see caveat) |

## Per-source table

| id | verdict | rows | geo | data quality | notes |
|---|---|---|---|---|---|
| amateur-radio-repeaters | DEAD_UPSTREAM | 0 | 0 | dead upstream | RepeaterBook export API now 401 `auth_missing`; diagnostic added |
| bike-share-gbfs | DATA | 1000 | 1000 | real+complete | **fixed**: was 0 geo. HelloCycling + DOCOMO Cycle, bikes/docks live |
| bridge-tunnel-infra | BLOCKED | — | — | unverifiable | Overpass IP-ban |
| cam-tabi_cam | DATA | 27 | 27 | real+complete | `record_type=camera`, uid shared with camera-discovery |
| castles | BLOCKED | — | — | unverifiable | Overpass IP-ban |
| chiriin-place | DATA | 71761 | 71761 | real+thin (`dataType` always null) | GSI place names; 72519 emitted / 71761 distinct uids → ~758 collisions or genuine dupes worth a look |
| COMPANY_LOOKUP | KEY_GATED (upstream) | 0 | 0 | gated | OpenCorporates v0.4 → 401; reason now logged |
| covid19-japan | DATA | 30 | 0 | real+complete | national time series, correctly non-spatial |
| data-centers | BLOCKED | — | — | unverifiable | Overpass IP-ban |
| docomo-insight | KEY_GATED | 0 | 0 | gated | `no DOCOMO_INSIGHT_API_KEY` |
| earthquake-network-citizen | LABELS_ONLY | 1 | 0 | labels-only | `|portal` placeholder row, `reachable=false` |
| estat-education | KEY_GATED | 0 | 0 | gated | `ESTAT_APP_ID` |
| famous-places | BLOCKED | — | — | unverifiable | Overpass tiled; same query as google-my-maps |
| FLIGHT_TRACKER | DATA | 1 | 1 | real+complete | needs an ICAO24 pivot; `4b1817` → `Flight SWR122C` with live position. Sweep's `keyword=Tokyo` was the wrong pivot |
| GAZETTE_WORLD | WAF_BLOCKED | 0 | 0 | dead/blocked portals | uk-gazette 500, Legifrance 403, BODACC 404, BORME 404, Bundesanzeiger 0, BGBl 404 |
| ghsa-advisories | DATA | 100 | 100 | real+complete, **fake geo** | full advisory metadata (CVE, severity, CVSS, ecosystems); all 100 pinned to Tokyo |
| MEDIACLOUD | KEY_GATED | 0 | 0 | gated | `no MEDIACLOUD_API_KEY` |
| gnews-top-ae … gnews-top-zw (142) | DATA | 26–38 each | 0 | real+thin | correct for news. `summary`/`body` are HTML blobs; `published_at` is RFC-822; 12 non-Latin editions intermittently corrupt `summary` (finding 1) |
| google-my-maps | BLOCKED | — | — | unverifiable + duplicate | identical Overpass query to `famous-places` |
| HASH_LOOKUP | KEY_GATED (upstream) | 0 | 0 | gated | MalwareBazaar 401; VT enrichment already gated on `VIRUSTOTAL_API_KEY` |
| homes-co | EMPTY | 0 | 0 | dead scrape | **fixed** rc −1→0. Pages load but no longer render a server-side `件` count; browser UA gets CloudFront 403 |
| intelx-leaks | LABELS_ONLY | 1 | 0 | labels-only | `|portal` placeholder |
| japan-api-prefectures | DEAD_UPSTREAM | 0 | 0 | dead upstream | `japanapi.curtisbarnard.com` NXDOMAIN |
| jcg-patrol | EMPTY (by construction) | 0 | 0 | dead by design | fetches then discards; can never emit |
| jma-ocean-wave | DATA | 6 | 6 | real+complete but stale | 6/6 upstream stations; wave height + period; `observed_at` 128 days old |
| jma-weather | DATA | 1 | 1 | real+thin (1 of 47 offices) | **fixed**: title/summary/link/published_at |
| jpo-trademarks | LABELS_ONLY | 1 | 0 | labels-only | `|portal` placeholder, `reachable=0` |
| jstat-map | KEY_GATED | 0 | 0 | gated | `ESTAT_APP_ID` |
| landsat-jp | DATA | 50 | 50 | real+complete | **fixed**: was 0 geo, footprint dropped. Now centroid + Polygon + preview URL |
| manga-net-cafes | BLOCKED | — | — | unverifiable | Overpass IP-ban |
| mercari-trending | DEAD_UPSTREAM | 0 | 0 | dead upstream | trends endpoint 404s with the SPA shell |
| mlit-c02-ports | EMPTY | 0 | 0 | dead upstream | both `nlftp.mlit.go.jp` C02 mirrors 404; already returns an honest 0 |
| mlit-river | DATA | 1193 | 1193 | **labels-only for its own name** | titled "MLIT River Water Levels / 河川水位情報" but emits OSM monitoring-station *locations* with no water level at all. Title fix applied, unverified (Overpass ban) |
| nasa-firms-jp | KEY_GATED | 0 | 0 | gated | `NASA_FIRMS_MAP_KEY` |
| NEWS_ARCHIVE | WAF_BLOCKED | 0 | 0 | rate-limited | GDELT 429; 24 s per attempt. Emits real per-article rows when GDELT answers (sweep got 25) |
| nicter-stats | EMPTY | 0 | 0 | dead scrape | **fixed** rc −1→0 + URL 404→200. Real counters now live at `/top10` and need a re-implementation |
| npa-important-wanted | DATA | 10 | 10 | real+complete | 10 NPA 重要指名手配; geo = handling prefecture HQ (documented proxy) |
| odpt-flight | KEY_GATED | 0 | 0 | gated | `no ODPT token` |
| openmeteo-jma | DATA | 9 | 9 | real+complete | **fixed**: title/summary/link. temp + wind + precip for 9 cities |
| osv-dev | DATA | 51 | 51 | real+thin, **fake geo** | **fixed**: title/link/published_at. Capped at 6 vulns/package; all pinned to Tokyo |
| PATENT_SEARCH | KEY_GATED | 0 | 0 | gated | **fixed**: now logs `gated (no PATENTSVIEW_API_KEY)` |
| phone-scam-hotspots | EMPTY | 0 | 0 | dead scrape | **fixed** rc −1→0 + gate URL. 9/10 prefectural pages are 404, the 10th redirects to a homepage |
| pref-police-crime | LABELS_ONLY | 36 | 0 | labels-only | `directory-only` rows, `rows_available: 0`, no coordinates; 47 sequential fetches never finish in 180 s |
| real-estate | EMPTY | 0 | 0 | dead scrape | honest 0 already; 11 s runtime trips the duration detector |
| CA_CORPORATIONS | EMPTY | 0 | 0 | unexplained | logs `emitted 0` with no status; needs the same diagnostic treatment |
| GULF_REGISTRY | DATA | 6 | 0 | real+thin | 2 of 4 portals answer; others 404/429/0. Already logs per-portal status |
| PL_KRS | EMPTY (pivot mismatch) | 0 | 0 | correct refusal | requires a 10-digit KRS number, not a company name |
| ZA_CIPC | WAF_BLOCKED | 0 | 0 | blocked | CIPC 404, SAFLII 403 |
| REVERSE_GEOCODING | DATA | 1 | 1 | real+complete | Nominatim; `和泉二丁目, 杉並区, 東京都, 168-0063` |
| SANCTIONS_CHECK | KEY_GATED (upstream) | 0 | 0 | gated | OpenSanctions 401; **fixed**: reason now logged |
| semiconductor-fabs | BLOCKED | — | — | unverifiable | Overpass IP-ban |
| shodan-japan | KEY_GATED | 0 | 0 | gated | `SHODAN_API_KEY` |
| soramame | BLOCKED | — | — | unverifiable | Overpass IP-ban |
| strava-segments-jp | LABELS_ONLY | 1 | 0 | labels-only | `|portal` placeholder |
| tea-zones | BLOCKED | — | — | unverifiable | Overpass IP-ban |
| EMAIL_HARVESTER | DATA | 3 | 0 | real+thin | **fixed**: percent-encoding junk removed, domain regex escaped. Needs a domain pivot, not an email. 10 Google SERP fetches are wasted latency |
| tochi-info | DEAD_UPSTREAM | 0 | 0 | dead upstream | `www.land.mlit.go.jp` NXDOMAIN; rc semantics fixed + diagnostic |
| twitch-jp-streams | LABELS_ONLY | 1 | 0 | labels-only | `|portal` placeholder |
| unified-port-infra | BLOCKED | — | — | unverifiable | Overpass IP-ban |
| VEHICLE_LOOKUP | DATA | 25 | 0 | real+complete | needs a VIN pivot; `1HGCM82633A004352` → `2003 HONDA Accord` + 24 more rows. Sweep's `keyword=Tokyo` was the wrong pivot |
| vrchat-active-jp | LABELS_ONLY | 1 | 0 | labels-only | `|portal` placeholder |
| wdcgg-co2 | KEY_GATED | 0 | 0 | gated | `WDCGG_REGISTRY_URL` |
| wifi-networks | KEY_GATED | 0 | 0 | gated | all three sub-collectors gated (WIGLE / SHODAN / MLS) |
| wolfx-eew | DATA | 1 | 1 | real+complete | **fixed**: title + ISO `published_at` |
| MENA_REGISTRY | DATA | 15 | 0 | real+thin | some portals 404/429; already logs per-portal status. 43–64 s runtime |
| yahoo-realtime | WAF_BLOCKED | 0 | 0 | blocked | `search.yahoo.co.jp/realtime/buzz` → 403; handled honestly (rc=0). Would pin every trending term to Tokyo if it worked |
