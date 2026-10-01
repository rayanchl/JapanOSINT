# Slice 04 — 206 sources across 65 files (build slot a3)

## Summary

| verdict | count |
|---|---|
| DATA | 26 (incl. 4 taken from 0 rows → real rows this pass) |
| KEY_GATED | 10 (all log `gated (no …)` + return 0 — correct) |
| ENV_BLOCKED (Overpass) | 18 |
| RATE_LIMITED / WAF_BLOCKED | 4 (reddit ×142 counted once, FR24, JFTC, listafirme.ro) |
| EMPTY (honest / by design) | 3 |
| LABELS-ONLY ("portal probe") | 9 |
| **RC_ERROR** | **0** (was 137) |
| **CRASH** | **0** (was 1) |
| **NO_TITLE** | **0** (was 3) |

### Five most important findings

**1. `UK_REGISTRY` segfaulted mid-run.** The registry search-URL templates are not printf
formats (Land Registry's carries `et%5B%5D=lrcommon%3Afreehold`) and were fed to
`snprintf()`. `%5B` is an undefined conversion reading a garbage vararg; the process died at
`REGS[10]` after 15 rows. **Fixed** → `rc=0 records=40` across all 11 registries.

**2. `cisa-kev-jp` invented a coordinate for every row.** A CVE is not a place and CISA
publishes none — the pin came from a static `VENDOR_GEO` table of vendor head offices
compiled into the binary, falling back to **Tokyo Station** for unlisted vendors. 46
"exploited vulnerability" markers on ~10 Tokyo office buildings. **Fixed** (geometry
removed; `vendor` keeps the real non-spatial attribution).

**3. `HISTORICAL_WHOIS` had silently stopped returning anything.** Its only key-free path
scraped ViewDNS matching the literal `"<tr>"`/`"<td>"`; ViewDNS moved to Tailwind markup
(`<td class="px-6 py-4 …">`), so nothing matched and every lookup looked like an honest
empty. **Fixed** → **124 real domains** for github.com, now also carrying registration date
and registrar (two columns discarded even when it worked).

**4. `PDF_ANALYZER` returned its status inverted** — `return rc >= 0 ? 1 : 0` returned **1
on success**, so every successful analysis opened a `collector_anomaly`
(`verdict=status_bad`) while a failed emit looked healthy. `credential_leak.c` had the plain
`return emitted;` form. Both **fixed**. Neither appeared in the fleet `rc > 0` sweep because
both return 0 when they find nothing, and both were swept with a pivot that found nothing.

**5. `wolfx-eqlist` read field names that do not exist.** It asked for
`Hypocenter`/`MaxIntensity`; the feed publishes `location`/`shindo`. `cJSON_GetObjectItem`
is case-insensitive, which is why `Latitude`/`Magnitude`/`Depth` worked and masked it. All
50 rows shipped with no place name and no JMA intensity, title degrading to a bare `"M2.3"`.
**Fixed**, plus `time_full`/`info` (tsunami advisory) which were dropped entirely, plus
ISO-8601 `published_at`.

## Fixes applied

| file | source(s) | bug | fix | re-test result |
|---|---|---|---|---|
| `world_reg_uk.c` | UK_REGISTRY | percent-encoded octets in URL templates passed to `snprintf` → **segfault** | `uk_build_url()` copies literally, expands only `%s` | CRASH → `rc=0 records=40` |
| `cisa_kev_jp.c` | cisa-kev-jp | fabricated geo (vendor-HQ table + Tokyo-Station fallback); uid = sha1 over {geometry, properties} led by positional `idx` | geometry dropped; `uid` = cve_id | `records=46`, **0 lat/lon**, uid `cisa-kev-jp\|CVE-2023-4346` |
| `cisa_kev_jp.c` | cisa-kev-jp | description hard-cut at 400 bytes; no `published_at` | full description; `published_at` = dateAdded | `published_at 2026-07-15`, +666 chars |
| `reverse_whois.c` | HISTORICAL_WHOIS | ViewDNS scraper matched `"<td>"`; registrar + reg-date columns discarded | attribute-tolerant cell parser, all 3 columns | **0 → 124 rows** |
| `pdf_analyzer.c` | PDF_ANALYZER | `return rc >= 0 ? 1 : 0` → quarantined on success | `? 0 : -1` | `rc=1`+anomaly → `rc=0 records=1`, no anomaly |
| `credential_leak.c` | CREDENTIAL_LEAK_SEARCH | `return emitted;`; silent gate | `return 0` + logs | `rc=0`, gate visible |
| `wolfx_eqlist.c` | wolfx-eqlist | wrong keys → place & intensity null on all 50 rows; fields dropped; non-ISO date | read `location`/`shindo`; +`time_full`,`tsunami_info`,`report_type`; ISO+09:00 | `M2.3 — 熊本県熊本地方 (震度1)`, `2026-08-01T01:48:00+09:00` |
| `internet_exchanges.c` | internet-exchanges | IX loop `continue`d on missing lat/lon — PeeringDB `/api/ix` **never** carries coordinates, so every fetched IX was discarded; source depended 100% on `/api/fac` (429 today) | emit IX rows with no geometry (no coordinate invented); `uid` = PeeringDB id | **0 → 27 rows** (JPIX TOKYO…) with fac still 429 |
| `data_extractor.c` | DATA_EXTRACTOR | domain regex matched one label+TLD → `ir@toyota.co.jp` yielded IOC `toyota.co`, a *different real domain* | allow interior labels | `ioc:domain:toyota.co.jp` + `global.toyota` |
| `weather_service.c` | WEATHER_SERVICE | titled with the *requested* place while reading+pin came from wttr.in's resolved station — "Tokyo" → Shikinejima, ~150 km away | title names the resolved station | `Weather — Tokyo [nearest station: Shikinejima] (current)` |
| `spamhaus_drop.c`, `feodo_tracker_jp.c`, `npa_missing_persons.c`, `wolfx_eqlist.c` | 4 | predecessor replaced fake pins with `cJSON_CreateNull()` geometry — `lib/geojson.c:235` serialises it, so literal `"null"` was persisted into the geometry column (2,091 rows read as "has geometry") | omit the `"geometry"` key entirely | spamhaus `2085 rows, 0 with geometry` |
| `npa_missing_persons.c` | npa-missing-persons | no `published_at` → 5 years stacked on ingest time | period close `<year>-12-31T00:00:00Z` | `MISSING_2020 → 2020-12-31` |
| `bird_flu_outbreaks.c` | bird-flu-outbreaks | `outbreak_id` ∉ `NATIVE_ID_KEYS` → uid was a content hash | add `uid` | uid `\|HPAI_02_24` |
| `flightradar_jp.c` | flightradar-jp | failed fetch silent at a 60 s interval | log the block; still honest empty | block now visible |

## Findings not fixed (with reason)

| source | issue | why not fixed |
|---|---|---|
| 142 × `reddit-*` | reddit 429s per-IP; `rss_collect` maps any non-2xx to −1 → `scheduler.c:70` quarantines (`anomaly opened: reddit-osint verdict=status_bad`). Measured: first request in a window → 25 real rows; anything <30 s later → 429 | Real fix is a per-host rate limiter **and** a distinct "throttled" outcome in `lib/rss_atom.c` — both read-only. Predecessor's 1800→7200 mitigation kept (verified complete, 142/142) |
| 18 × Overpass | All four endpoints unusable: `overpass-api.de` refuses connection (http=000, 2.9 s); kumi / openstreetmap.ru / private.coffee connect but never answer (45 s, http=000) | ENV_BLOCKED. Predecessor's HTTP-budget fix (60 s client vs `[timeout:180]` server → 200 s) is sound in principle but **unverifiable** — flagged as unproven |
| 9 × portal-probe sources | Emit ONE signpost row with `reachable: true/false` and zero payload from the named dataset | By design (`lib/probe.h`: "~45 of the 62 OTHER ids follow this shape"). Real collectors would be a feature, not an audit fix |
| `pm25-japan` | Emits nothing by design | Correct — probed `soramame.env.go.jp`, it's an SPA shell returning identical 2,593 bytes for every path. No endpoint invented |
| `twitter-geo` | Not a collector: re-`SELECT`s existing `record_type='post'` rows and re-emits. No fetch, can never add data | Structural; out of scope |
| `SUBDOMAIN_FINDER` | Works (190 real subdomains from crt.sh) but takes **279 s** → `duration_outlier` anomaly | Needs a time budget / concurrency — design change. This is the sweep's TIMEOUT |
| `PROCUREMENT_OCDS` | Pivot "Tokyo" returns "Housing Tree Work, London Borough of Haringey" — keyword doesn't appear to constrain | Needs multi-portal API investigation at ~72 s/run |
| `TECH_STACK_DETECTION` | Reports "React" for github.com (Rails + Primer) — body-substring false positive; the 16 header signatures can never fire because `http_response` carries only `{status, body, body_len}` | Needs a headers field in `core/` — read-only |
| `news_world` | 5 dead feeds documented by predecessor (verified, 621 rows). **Additionally the AFP feed's top item is dated 2016-06-13** — stale feed still polled | Reuters/AP/Kyodo have no key-free replacement; left rather than swapped for an invented endpoint |
| `world_reg_uk` titles | ~40 leading spaces; `language` hardcoded `"ja"` on UK rows | Both from `jo_emit_anchors()` in `_jp_osint.inc` (134 collectors, not in slice) |
| `ccs-projects` | Coordinates/operators/region from a **static compiled table**; only the project *name* is verified upstream | Predecessor labels it honestly in-record (`data_origin`, `geo_precision`) — verified. Page is image-heavy, that's the honest ceiling |

## Shared-code findings — reported, not patched

1. **`lib/geojson.c:235`** `gj = geom ? cJSON_PrintUnformatted(geom) : NULL;` — an explicit
   JSON `null` geometry is serialised, so the 4-byte string `"null"` lands in the `geometry`
   column and every consumer (`geometry IS NOT NULL AND geometry<>''`) reads it as "has
   geometry". Hit 4 sources / 2,091 rows in this slice alone. Should skip `cJSON_IsNull(geom)`.
2. **`lib/probe.c:13`** `probe_head()` — 8 s HEAD with a custom UA, reports
   `reachable:false` on **any** transport error. `jpx.co.jp` and `mowlas.bosai.go.jp` both
   answer HEAD **200** to curl well under 8 s, yet both probes timed out at ~8.6 s and
   published `reachable:false`. The single fact this ~45-source family exists to publish is
   unreliable. Two also point at URLs that are now **404**
   (`mlit.go.jp/road/road_e/index.html`, `kansai-td.co.jp/teiden/area_jishin/`).
3. **`lib/rss_atom.c:157`** stores the **raw inner XML** of the Atom `<author>` element →
   every Atom row gets `author = "<name>/u/AutoModerator</name><uri>…</uri>"`. Same function
   leaves body/summary as raw HTML and unresolved entities (`&#xd;`).
4. **`collectors/sources/_jp_osint.inc:150,180`** `jo_emit_anchors()` right-trims anchor
   text but never left-trims, and hardcodes `it.lang = "ja"`. 134 collectors.
5. Confirmed present in this slice: the known `lib/rss_atom.c:180` raw RFC-822
   `published_at` (`sans-isc`, `WORLD_NEWS_RSS`, all 142 reddit rows).

## Per-source table (condensed)

**Fixed/verified DATA** — UK_REGISTRY 40/0 · HISTORICAL_WHOIS 124/0 · internet-exchanges
27/0 · cisa-kev-jp 46/0 · wolfx-eqlist 50/50 · bird-flu-outbreaks 24/24 · spamhaus-drop
2085/0 · feodo-tracker-jp 1/0 (only 1 JP C2 — honest) · npa-missing-persons 6/0 ·
PDF_ANALYZER 1/0 · DATA_EXTRACTOR 5/0 · WEATHER_SERVICE 8/8 · cam-webcamendirect_list 21/21
(prefecture-level, honestly flagged `location_approximate`) · ccs-projects 9/9 (static
table) · hatena-bookmark 40/0 · sans-isc 11/0 · WORLD_NEWS_RSS 621/0 · PROCUREMENT_OCDS 94/0
· SUBDOMAIN_FINDER 190/0 · SHODAN_SEARCH 3/0 · TOR_EXIT_CHECK 1/0 · TECH_STACK_DETECTION 3/0
(low confidence) · CZ_ARES 12/0 · OPENALEX 15/0.

**KEY_GATED (correct)** — edinet-filings, estat-employment, github-leaks-jp, leakix-jp,
sentinel-japan, wifi-networks-mls, GBIZINFO, IE_CRO, RO_COMPANIES (also 403 on its
listafirme.ro fallback), CREDENTIAL_LEAK_SEARCH.

**Blocked/empty** — flightradar-jp (FR24 302s to every UA — dead upstream) · pm25-japan (by
design) · twitter-geo (derived view).

**LABELS-ONLY (1 signpost row, no payload)** — jftc-mergers (probe genuinely 403) ·
jma-pollen · jnto-arrivals · jpx-quotes (**reports unreachable but curl HEADs 200**) ·
mext-schools · mlit-road-traffic (**probe URL 404**) · nied-mowlas (**reports unreachable but
curl HEADs 200**) · docomo-mobaku · regional-grid-outages 9 rows (**KEPCO URL 404**).

**Overpass ENV_BLOCKED (18)** — anime-pilgrimage, bus-routes, convenience-stores,
government-buildings, hospital-map, japan-post-offices, k-net, manhole-covers, mlit-dam,
national-parks, odpt-station, openstreetmap-jp, overpass-rail-tracks, red-light-zones,
themed-cafes, unified-subways, vending-machines, wagyu-ranches.

**Reddit RATE_LIMITED (142)** — uniform behaviour; verified 25 real `record_type=article`
rows on the first request in a window (`reddit-japan`), 429 → rc=−1 → quarantine on the next
two (`reddit-osint`, `reddit-worldnews`). No geo, correctly.

## Verdict on each inherited (predecessor) hunk

Correct and verified: Overpass HTTP-budget bump ×18 *(sound but unverifiable)*;
`convenience_stores` `24/7`→null, `vending_machines` vending/payment→null, `wagyu_ranches`
livestock→null (all invented-fact removals); `k_net`/`national_parks` positional id → OSM
id; `mlit_dam` 600 s→weekly; `github_leaks_jp` −1→0; `world_reg_uk` `uk_build_url()` (the
crash fix); `bird_flu_outbreaks` rewrite (24 real cases); `reddit_world_geo` 1800→7200 ×142
(complete); `news_world` dead-feed audit; `tech_stack`/`ccs_projects` comments (accurate).

Incomplete, **finished by this pass**: `feodo`/`spamhaus`/`npa` null-geometry → literal
`"null"` in the DB; `wolfx_eqlist` title builder used field names that don't exist; `npa`
missing `published_at`; `bird_flu` missing natural uid.

Nothing was reverted. Build clean at slot `a3`; all edits confined to slice files.
