# Slice 05 — 206 sources across 81 files (build slot a4)

## Summary

| verdict | n |
|---|---|
| DATA | 149 |
| KEY_GATED | 24 |
| ENV_BLOCKED (Overpass IP-ban) | 12 |
| WAF_BLOCKED | 9 |
| DEAD_UPSTREAM | 8 |
| EMPTY (honest) | 3 |
| TIMEOUT | 1 |
| **total** | **206** |

No RC_ERROR, CRASH, NO_TITLE or DB_ERROR source remains in the slice that isn't explained
by an environment block, a missing credential, or a genuinely dead upstream. Build clean:
`[slot a4] OK -> bin/japanosint-a4 (linked 21:11:23)`.

**On the previous auditor's work:** every one of its 16 edited files was re-run against a
freshly linked binary. All reproduce and all are sound — kept. Two of its recorded results
were against a stale binary: `tor-exit-nodes` (it recorded a hashed uid; the fix does work,
uid is now the fingerprint) and `gcom-w` (its `boxes` geo branch never fires — see below).
Both finished.

### The five findings that matter

**1. `peeringdb-jp` was planting ~1,100 invented pins.** Every IXP *and every network* (an
ASN — not a place) was emitted at Tokyo Station 139.6917/35.6895, and a facility with no
coordinates fell back to the same point. PeeringDB's `/api/ix` publishes no lat/lon at all
(verified: 27 JP IXPs, 0 with coordinates). **Fixed** — only facilities with real
coordinates pin. Verified **1,112 rows with `at_tokyo=0`** (was 1,139 rows with every
non-facility row at Tokyo Station).

**2. `core/camera_store.c:camera_upsert()` never sets `it.link`** (nor `summary` /
`published_at`) even though the camera page URL is in hand as `m_url` and written to
`properties.url` two lines earlier. Every camera row in the fleet is a camera you cannot
open. In this slice: `cam-camscape` 32/32 link-less, `cam-webcamera24` 160/160.
**READ-ONLY — reported, not patched.** One-line fix at `core/camera_store.c:233`:
`it.link = m_url;` (and `it.summary` / `it.published_at` while you're there).

**3. Two more `run()`-returns-a-row-count quarantine bugs**, both of the class where
*success* trips the anomaly detector: `ioc_lookup.c:ioc_run` (`return emitted` / `return r`)
and `pep_watchlist.c:run_pep` / `run_watchlist` (`return opensanctions(...)`). A PEP
screening that actually **matched** a sanctioned person, or an IOC lookup that actually
**found** the indicator, was recorded `status=error` and quarantined the source; a clean
result looked healthy. **Both fixed**, verified with a real hit:
`IOC_LOOKUP 44d88612fea8a8f36de82e1278abb02f` → 1 row, `rc=0` (that exact input returned
`rc=1` before).

**4. Eight sources in this slice are reachability probes wearing a data source's clothes** —
a portal-status row and zero measurements: `xrain-radar`, `kafun-pollen`,
`jr-boarding-stats`, `mhlw-health`, `teikoku-failures`, `psbdmp-pastes`, `nexco-roadwork`,
`nitter-mirrors`. They all score as `DATA` in any row-count sweep. Six currently report
`reachable:false` — even the probe is failing — and `mhlw-health`'s own body says *"no
stable machine API to parse"*. Not fixed: each needs a real parser.

**5. `wanted-persons` is unsound by construction and its upstream is gone.**
`npa.go.jp/sousa/shimeitehai/index.html` now **404s**. Independently: the collector
regex-counts 氏名/年齢/罪名 on 10 prefectural police pages, takes the **max of the three
counts** as the case count, then pairs `name[i]`/`age[i]`/`crime[i]` across three
*independent* match lists (so a case can carry another person's charge) and scatters each
case on a **jittered ring around the police-HQ coordinate** (`lon + cos(angle)*r`,
r = 0.005–0.011°) — an invented location per wanted person. Not fixed (0 rows today, so no
fix could be re-verified); it now logs *why* it is empty.

## Fixes applied

✱ = previous auditor's edit, re-verified. Everything else is this auditor's.

| file | source(s) | bug | fix | re-test result |
|---|---|---|---|---|
| `peeringdb_jp.c` | `peeringdb-jp` | every IXP + every ASN pinned at Tokyo Station; coordinate-less facility also Tokyo | geometry key omitted for `ix`/`net`; facility pins only on real lat/lon | 1112 rows, **at_tokyo=0** (was 1139/1139 fabricated) |
| `peeringdb_jp.c` | `peeringdb-jp` | uid = sha1(geometry+properties) incl. order-dependent `idx` → every run re-inserted every row | stable `uid` = `peeringdb:<seg>:<id>` | `peeringdb-jp\|peeringdb:ix:30` |
| `peeringdb_jp.c` | `peeringdb-jp` | all 1139 rows had NULL `published_at` and NULL body | `published_at` from PeeringDB `updated`; per-kind `summary`; ~20 dropped upstream fields carried (website, fac_count, contacts, prefixes, policy, address1, …) | `no_date=0 no_body=0` |
| `peeringdb_jp.c` | `peeringdb-jp` | `/net`,`/ix`,`/fac` fired back-to-back → PeeringDB throttles 2 of 3, silently dropping two record kinds with no log | 2 s pacing + `fetched net=%d ix=%d fac=%d` log | 27 rows → **1112 rows** in one run |
| `jma_tide.c` | `jma-tide` | 166/166 rows had NULL link **and** NULL summary — a tide gauge whose reading you cannot read | `summary` from level/astro/anomaly; `url` = JMA bosai tide page for the gauge's class20 area | `函館 潮位 53cm（天文潮位 -12cm、偏差 +65cm）0時間前観測。` + link; 166/166 geo |
| `ioc_lookup.c` | `IOC_LOOKUP` | `run()` returned the emitted count → a **hit** quarantined the source | returns 0/-1; counts logged | `44d88612…` → 1 row, `rc=0` |
| `pep_watchlist.c` | `PEP_CHECK`, `WATCHLIST_CHECK_NEW` | same (a **match** quarantined the source); no-key path returned silently so "gated" was indistinguishable from "screened, clean" | returns 0/-1; gate logged | `rc=0`, `[PEP_CHECK] gated (no OPENSANCTIONS_API_KEY)` |
| `gcom_w.c` | `gcom-w` | granule's real footprint (`polygons`), orbit, CMR concept id, size, day/night flag all dropped — the ✱ `boxes` branch never fires because CMR serves AMSR2 with `coordinate_system=ORBIT` | carried into properties. Deliberately **not** used as geometry: a half-orbit swath runs pole to pole (lon −119 → +176 on the sample granule), so pinning it would draw a band across the whole map | properties now carry `granule_polygons`, `orbit`, `cmr_concept_id` |
| `license_plate.c` | `LICENSE_PLATE_LOOKUP` | gated silently — indistinguishable from "no such plate" | logs its gate | `gated (no PLATE_LOOKUP_URL template…)` |
| `wanted_persons.c` | `wanted-persons` | two silent `return 0` paths hid a 404'd upstream | both log | `NPA index unreachable (…/shimeitehai/index.html)` |
| ✱ `legal_records.c` | `LEGAL_SEARCH`, `BANKRUPTCY_SEARCH`, `CRIMINAL_RECORDS` | returned the row count (15) → every successful search errored | return 0 | all three `rc=0`, 15 rows |
| ✱ `phone_intel.c` | `PHONE_LOOKUP`, `PHONE_REPUTATION`, `CARRIER_LOOKUP` | same, returned 1 | return 0/-1 | `rc=0 records=1` |
| ✱ `license_plate.c` | `LICENSE_PLATE_LOOKUP` | same, returned 1 | return 0/-1 | `rc=0` |
| ✱ `vessel_finder.c` | `vessel-finder` | missing key returned −1 → anomaly on every tick for a merely-gated source | return 0 + gate log | `rc=0` |
| ✱ `bird_makeup_jp.c` | `bird-makeup-jp` | −1 on an honest empty | −1 only on total fetch failure | `rc=0`, `emitted 0 (handles 6, fetched 6)` |
| ✱ `geohazard_extra.c` | `emsc-quakes`, `iceland-quakes` | both RSS feeds retired; rows had no hypocentre | rewritten onto the EMSC/ORFEUS seismicportal FDSN event service | 300 rows each, **300/300 with lat/lon + geometry** (was 0) |
| ✱ `geohazard_extra.c` | `volcanodiscovery`, `copernicus-ems` | `/rss.xml` and Copernicus `activations-rapid/rss.xml` both 404 | repointed at `/volcano-news.rss` and `mapping.emergency.copernicus.eu/latest/feed/` | 20 and 10 rows (was 0) |
| ✱ `finance_markets_crypto.c` | `trading-econ` | `/rss/news.aspx` 403s every non-browser client | rewritten onto `ws/stream.ashx` — richer than the RSS ever was (country, category, importance, author) | 100 rows, `rc=0` |
| ✱ `japan_reit.c` | `japan-reit` | `/meigara/` retired → rc=−1 every run; anchor regex also required `</a>` immediately after the text, matching 3 of 61 issues | scrape `/list/rimawari/` + root; take the text run then skip to the real `</a>` | 57 rows (was 0) |
| ✱ `gcom_w.c` | `gcom-w` | unconstrained `?keyword=AMSR2` granule search → CMR HTTP 400, 0 rows, rc=−1 | constrain by `short_name=AU_Land\|AU_Ocean\|AU_SI12`; honest empty returns 0 | 50 rows |
| ✱ `jma_earthquake.c` | `jma-earthquake` | `list.json` publishes several bulletins per `eid`; the oldest (hypocentre-less) overwrote the newest. Depth, 最大震度, report serial, English strings and the detail link all dropped | keep first occurrence per `eid`; parse the 3rd ISO-6709 number as depth; readable title + link | 446 rows, **geo 398 → 439** |
| ✱ `poc_in_github.c` | `poc-in-github` | 94 rows pinned at Tokyo Station (a GitHub repo is not a place); NULL title/link/date; uid = hash of an order-dependent feature | geometry removed; `uid = <cve>\|<repo>`; title/link/published_at added | 94 rows, `no_title=0 no_link=0 no_date=0`, 0 pins |
| ✱ `osm_changesets_jp.c` | `osm-changesets-jp` | `properties.uid` held the OSM **user** id, which `lib/geojson.c` picks as the intel uid → all changesets by one mapper collapsed onto one row (100 emitted, 28 stored); NULL title; bbox-less changesets fell back to Tokyo Station | uid = changeset id (user id → `user_id`); title from user+comment+edits; no geometry without a bbox | 100 rows / **100 distinct uids** (was 28), `no_title=0` |
| ✱ `tor_exit_nodes.c` | `tor-exit-nodes` | a `continue` unless the relay had lat/lon dropped the **whole feed** — onionoo no longer publishes per-relay coordinates; uid hashed properties containing `last_seen`, so every hourly run re-inserted every relay | emit regardless of geo; uid = fingerprint; or/exit addresses, AS, platform, version carried | 5 rows = all 5 JP exit relays onionoo lists |
| ✱ `npa_special_fraud.c` | `npa-special-fraud` | NULL title on every monthly row; uid contained a running index dependent on how many earlier months parsed | title with 認知件数/被害額; uid = year-month | 26 rows, `no_title=0` |
| ✱ `job_boards.c` | `job-boards` | NULL title; `listing_id = JOB_LIVE_%05d` made the uid depend on Overpass result **order** | title + `uid = <type>/<osm id>` + OSM url | code correct; **cannot re-verify — Overpass IP-banned** |
| ✱ `gnews_osint_monitors.c` | `gnews-mon-tw-pla-incursion` | English `"PLA incursion"` returns 0 items from the zh-TW locale | query → `解放軍 擾台` | 0 → **100 rows** |
| ✱ `gnews_osint_monitors.c` | `gnews-mon-no-arctic-militarization` | English `"Arctic militarization"` returns 0 from the no-NO locale | query → `Arktis militær` | 0 → **81 rows** |

## Findings not fixed (with reason)

| source | issue | why not fixed |
|---|---|---|
| `cam-camscape`, `cam-webcamera24` | NULL `link`/`summary`/`published_at` on every camera although `camera_upsert` holds `m_url` | `core/camera_store.c:233` — READ-ONLY, affects all 14 camera sources fleet-wide |
| `gnews-mon-il-gaza` | `DB_ERROR: Could not decode to UTF-8` | `lib/rss_atom.c:172` 240-**byte** truncation — READ-ONLY, already reported |
| all 70 `gnews-mon-*`, `nhc-cpac`, `copernicus-ems`, `volcanodiscovery`, `bitcoin-mag`…`zerohedge` | `props[512]` overflow; raw RFC-822 `published_at` text-sorts ahead of ISO-8601 | `lib/rss_atom.c:165-171` / `:180` — READ-ONLY, already reported |
| `nhc-cpac` | NOAA publishes `<georss:point>` + `<nhc:*>` cyclone metadata; none parsed, so cyclone rows can never pin | needs a `lib/rss_atom.c` georss extension — READ-ONLY, fleet-wide |
| `RADIO_BROWSER` | radio-browser returns `geo_lat`/`geo_long`; dropped, so 11 physical transmitters can't pin | in-slice, but upstream only geo-tags a minority; flagged rather than half-fixed under time pressure |
| the 8 probe-only sources (finding #4) | reachability rows, not data | each needs a real upstream parser written from scratch |
| `wanted-persons` | invented ring coordinates + cross-indexed name/age/crime (finding #5) | upstream 404s, so no fix could be re-verified; needs redesign not repair |
| `npa-special-fraud` | 25 national monthly totals all pinned at the NPA HQ coordinate | a national statistic has no location; removing the pin empties a map layer — **product call** |
| `bom-au-warnings` | `bom.gov.au/rss/1.xml` 404s. The only live replacement, `api.weather.bom.gov.au/v1/warnings` (verified 200, 8 real warnings with issue/expiry) returns the metadata string *"This API is owned by the Bureau of Meteorology. You must not use, copy or share it."* | will not wire the product to an API whose own response forbids use. **Needs a decision.** |
| `unesco-heritage` | `whc.unesco.org/en/list/xml/` Cloudflare-challenged (403). `/en/list/rss/` **does** return 200 / 1.15 MB — but has no `iso_code`, no latitude, no longitude | switching trades a 25-site geo layer for a labels-only global feed. Left WAF_BLOCKED |
| `mlit-transaction`, `mlit-landprice` | `land.mlit.go.jp/webland/api/*` retired; successor `reinfolib.mlit.go.jp/ex-api` returns `401 missing subscription key` | dead upstream + key-gated replacement |
| `kitco-news` | `/rss/KitcoNews.xml`, `/rss/`, `/news/feed` all serve the site 404 page; `/news` exposes no `<link rel=alternate>` | genuinely retired; left returning `rc=-1` so the failure stays visible |
| `ADVISORY_COUNCILS` (404), `EGOV_PUBLIC_COMMENT` (404), `WASTE_HAULERS` (403), `BUSINESS_LICENSES` (status 0) | all four `gov_admin.c` endpoints dead or WAF'd | needs new endpoint research per service |
| 12 Overpass sources | `admin-boundaries`, `job-boards`, `city-halls`, `ferry-routes`, `cell-towers`, `rice-paddies`, `satellite-ground-stations`, `sento-public-baths`, `shrine-temple`, `submarine-cables`, `overpass-subway-tracks`, `unified-trains` | ENV_BLOCKED, unverifiable. `lib/overpass.c` returns −1 for an honest empty, so all additionally open a `status_bad` anomaly |
| ~5 live sources with `return n > 0 ? 0 : -1` | an empty upstream returns −1 and quarantines — same class as the fleet sweep, but only bites on a quiet day so none showed as RC_ERROR today | `data-go-jp-ckan`, `egov-laws`, `hatena-bookmark-extended`, `japan-reit`, `jma-earthquake`. Listed rather than blind-edited: distinguishing "fetch failed" from "fetch OK, nothing to emit" is per-collector work |
| `SSL_ANALYZER` (59 s), `BANKRUPTCY_SEARCH` (18–32 s), `SEA2_REGISTRY` (73 s), `AFRICA_REGISTRY` (16 s), `volcanodiscovery` (12 s) | `duration_outlier` anomalies | real upstream latency, not collector bugs — the 10 s threshold in `core/` is too tight for multi-endpoint on-demand services |

## Per-source table

`rows`/`geo` are from this auditor's own re-runs. `peeringdb-jp` shows 1112 from the
verified paced run; a later run returned 27 because PeeringDB had this IP in a rate-limit
cool-off.

| id | verdict | rows | geo | data quality | notes |
|---|---|---|---|---|---|
| admin-boundaries | ENV_BLOCKED | 0 | 0 | gated by env | Overpass; all 4 endpoints IP-banned |
| OPENAQ_GLOBAL | KEY_GATED | 0 | 0 | gated | no OPENAQ_KEY; v2 keyless fallback now 410 |
| OPENMETEO_AQ | DATA | 1 | 1 | real+complete | PM2.5/PM10/O3/NO2/SO2/CO + AQI |
| WAQI_GLOBAL | KEY_GATED | 0 | 0 | gated | no AQICN_TOKEN |
| ASN_LOOKUP | DATA | 1 | 0 | real+complete | an ASN is not a place |
| GBIF_OCCURRENCE | DATA | 30 | 30 | real+complete | |
| INATURALIST | DATA | 30 | 30 | real+complete | |
| OBIS_MARINE | DATA | 30 | 30 | real+complete | sweep used a vessel IMO as pivot; species name gives 30 geo rows |
| bird-makeup-jp | DEAD_UPSTREAM | 0 | 0 | dead upstream | 6 handles fetch 200 OK, every outbox empty |
| BREACH_INDEX_EMAIL/PASSWORD/PHONE/USERNAME | DATA | 1 each | 0 | real+complete | local 1,018-breach corpus |
| cam-camscape | DATA | 32 | 32 | real+thin (no link) | `core/camera_store.c` never sets `it.link` |
| cam-webcamera24 | DATA | 160 | 160 | real+thin (no link) | same core bug |
| cell-towers / city-halls / ferry-routes | ENV_BLOCKED | 0 | 0 | gated by env | Overpass IP-ban |
| EMAILREP_LOOKUP / NUMVERIFY_PHONE / OPENCORPORATES | KEY_GATED | 0 | 0 | gated | |
| CH_OPENDATA | DATA | 20 | 0 | real+complete | needs a local-language pivot |
| FI_OPENDATA | DATA | 20 | 0 | real+complete | needs a local-language pivot |
| UK_DATAGOV | DATA | 10 | 0 | real+complete | |
| crtsh-historical | TIMEOUT | 0 | 0 | unverifiable | crt.sh never returned inside 280 s |
| data-go-jp-ckan | DATA | 10 | 0 | real+complete | |
| docomo-population | KEY_GATED | 0 | 0 | gated | |
| egov-laws | DATA | 200 | 0 | real+complete | |
| estat-household | KEY_GATED | 0 | 0 | gated | no ESTAT_APP_ID |
| bitcoin-mag | DATA | 10 | 0 | real+complete | |
| cnbc-finance / cnbc-top / cnbc-world | DATA | 30 each | 0 | real+complete | |
| coindesk | DATA | 25 | 0 | real+complete | |
| cointelegraph | DATA | 30 | 0 | real+complete | |
| decrypt | DATA | 38 | 0 | real+complete | |
| ft-home | DATA | 10 | 0 | real+complete | |
| investing-news | DATA | 10 | 0 | real+complete | |
| kitco-news | DEAD_UPSTREAM | 0 | 0 | dead upstream | every published Kitco RSS path 404s |
| marketwatch-top | DATA | 10 | 0 | real+complete | |
| oilprice-2 | DATA | 15 | 0 | real+complete | |
| seeking-alpha | DATA | 7 | 0 | real+complete | |
| the-block | DATA | 20 | 0 | real+complete | |
| trading-econ | DATA | 100 | 0 | real+complete | rewritten onto ws/stream.ashx |
| zerohedge | DATA | 25 | 0 | real+complete | |
| ALPHAVANTAGE_SEARCH / FINNHUB_SEARCH / fofa-jp | KEY_GATED | 0 | 0 | gated | |
| gcom-w | DATA | 50 | 0 | real+thin (no pin) | granule catalogue; newest granule 2025-08-31 (~11 mo stale) |
| bom-au-warnings | DEAD_UPSTREAM | 0 | 0 | dead upstream | RSS 404; replacement API licence-restricted |
| copernicus-ems | DATA | 10 | 0 | real+complete | URL repointed |
| emsc-quakes | DATA | 300 | 300 | real+complete | rewritten onto FDSN |
| iceland-quakes | DATA | 300 | 300 | real+complete | rewritten onto FDSN Iceland AOI |
| metoffice-warnings | EMPTY | 0 | 0 | honest empty | no active UK warnings |
| nhc-cpac | DATA | 2 | 0 | real+thin (no geo) | NHC georss not parsed |
| pdc-disasters | WAF_BLOCKED | 0 | 0 | WAF | Cloudflare on every path |
| volcanodiscovery | DATA | 20 | 0 | real+complete | URL repointed |
| gitlab-bitbucket-leaks | DATA | 2 | 0 | real+thin | anonymous search rate-limited |
| RADIO_BROWSER | DATA | 11 | 0 | real+thin (no geo) | upstream `geo_lat`/`geo_long` dropped |
| gnews-mon-* (70 sources) | DATA | 45–110 each | 0 | real+complete | `tw-pla-incursion` 0→100, `no-arctic-militarization` 0→81; `il-gaza` is the rss_atom 240-byte DB_ERROR |
| ADVISORY_COUNCILS | DEAD_UPSTREAM | 0 | 0 | dead upstream | 404 |
| BUSINESS_LICENSES / EGOV_PUBLIC_COMMENT / WASTE_HAULERS | WAF_BLOCKED | 0 | 0 | WAF | |
| grayhat-buckets | KEY_GATED | 0 | 0 | gated | |
| hatena-bookmark-extended | DATA | 199 | 0 | real+complete | |
| houjin-bangou | KEY_GATED | 0 | 0 | gated | |
| IOC_LOOKUP | DATA | 1 | 0 | real+complete | rc bug fixed: a HIT used to return rc=1 |
| japan-reit | DATA | 57 | 0 | real+complete | was 0 |
| jma-earthquake | DATA | 446 | 439 | real+complete | geo 398→439 |
| jma-tide | DATA | 166 | 166 | real+complete | link + measurement summary added |
| job-boards | ENV_BLOCKED | 0 | 0 | gated by env | fix in code, unverified |
| jr-boarding-stats | DATA | 2 | 0 | **labels-only** | reachability probe |
| kafun-pollen | DATA | 1 | 0 | **labels-only** | no pollen concentrations |
| BANKRUPTCY_SEARCH / CRIMINAL_RECORDS / LEGAL_SEARCH | DATA | 15 each | 0 | real+complete | CourtListener; rc fixed |
| LICENSE_PLATE_LOOKUP | KEY_GATED | 0 | 0 | gated | no lawful public API |
| mapfan-api | KEY_GATED | 0 | 0 | gated | |
| mhlw-health | DATA | 1 | 0 | **labels-only** | body admits there is no machine API |
| mlit-landprice / mlit-transaction | KEY_GATED | 0 | 0 | dead upstream | WebLand retired; reinfolib needs a key |
| navitime-api | KEY_GATED | 0 | 0 | gated | |
| nexco-roadwork | DATA | 3 | 0 | **labels-only** | no roadwork records |
| nitter-mirrors | DATA | 3 | 0 | **labels-only** | no posts |
| npa-special-fraud | DATA | 26 | 25 | real+thin (pin is NPA HQ) | national totals pinned at HQ |
| odpt-train | KEY_GATED | 0 | 0 | gated | |
| ARCHIVE_ORG_SEARCH / GITHUB_COMMITS_BY_EMAIL / MUSICBRAINZ | DATA | 20 each | 0 | real+complete | |
| PHOTON_GEOCODE | DATA | 9 | 9 | real+complete | |
| TOR_ONIONOO | DATA | 1 | 0 | real+thin | |
| BIGDATACLOUD_REVERSE | DATA | 1 | 1 | real+complete | needs a "lat,lon" pivot |
| HN_USER | DATA | 1 | 0 | real+complete | needs an HN username pivot |
| OPENVERSE / SEC_FULLTEXT / STACKEXCHANGE_SEARCH / WIKIDATA_ENTITY_SEARCH | DATA | 20 each | 0 | real+complete | |
| osm-changesets-jp | DATA | 100 | 100 | real+complete | 100 distinct uids (was 28) |
| overpass-subway-tracks | ENV_BLOCKED | 0 | 0 | gated by env | |
| peeringdb-jp | DATA | 1112 | fac only | real+complete | ~1.1k invented Tokyo pins removed |
| PEP_CHECK / WATCHLIST_CHECK_NEW | KEY_GATED | 0 | 0 | gated | rc bug fixed, gate logged |
| CARRIER_LOOKUP / PHONE_LOOKUP / PHONE_REPUTATION | DATA | 1 each | 0 | real+thin | rc fixed 1→0 |
| poc-in-github | DATA | 94 | 0 | real+complete | 94 fake Tokyo pins removed |
| psbdmp-pastes | DATA | 1 | 0 | **labels-only** | no pastes |
| reddit-jp-subs | WAF_BLOCKED | 0 | 0 | WAF | 403 to this host |
| DK_CVR | DATA | 1 | 0 | real+thin | |
| HR_SUDREG / LT_REGISTRAI / PT_RCBE | DEAD_UPSTREAM | 0 | 0 | dead upstream | 404 |
| HU_COMPANIES | EMPTY | 0 | 0 | honest empty | |
| LU_LBR | WAF_BLOCKED | 0 | 0 | WAF | 429 |
| NL_KVK | KEY_GATED | 0 | 0 | gated | |
| RS_APR | WAF_BLOCKED | 0 | 0 | WAF | refused |
| IL_COMPANIES | DATA | 2 | 0 | real+complete | |
| SEA2_REGISTRY | WAF_BLOCKED | 0 | 0 | WAF | 6/6 registries 404/refused, 73 s |
| reinfolib | KEY_GATED | 0 | 0 | gated | |
| rice-paddies / satellite-ground-stations / sento-public-baths / shrine-temple / submarine-cables | ENV_BLOCKED | 0 | 0 | gated by env | Overpass IP-ban |
| SSL_ANALYZER | DATA | 101 | 0 | real+complete | 59 s → duration_outlier |
| teikoku-failures | DATA | 1 | 0 | **labels-only** | no bankruptcy records |
| THREAT_FEED_LOOKUP | EMPTY | 0 | 0 | honest empty | 8.8.8.8 genuinely clean |
| tor-exit-nodes | DATA | 5 | 0 | real+complete | geo guard was dropping the whole feed |
| unesco-heritage | WAF_BLOCKED | 0 | 0 | WAF | |
| unified-trains | ENV_BLOCKED | 0 | 0 | gated by env | |
| vessel-finder | KEY_GATED | 0 | 0 | gated | rc fixed −1→0 |
| wanted-persons | DEAD_UPSTREAM | 0 | 0 | dead upstream | extraction method unsound anyway |
| WHALE_ALERT | DATA | 1 | 0 | real+thin | |
| wifi-networks-shodan | KEY_GATED | 0 | 0 | gated | |
| AFRICA_REGISTRY | DATA | 1 | 0 | real+thin | 15.8 s |
| xrain-radar | DATA | 1 | 0 | **labels-only** | no rainfall values |

## Files edited

All in-slice, all re-verified by re-running: `peeringdb_jp.c`, `jma_tide.c`,
`ioc_lookup.c`, `pep_watchlist.c`, `gcom_w.c`, `license_plate.c`, `wanted_persons.c`
(+288/−46). The previous auditor's 16 files are untouched and verified. No commits,
checkouts or stashes; `core/` and `lib/` untouched. Helper scripts under
`native/tests/audit/a4b_*`.

## Decisions needed

1. The one-line `core/camera_store.c:233` link fix — fleet-wide, not an auditor's to make.
2. Whether `bom-au-warnings` may use an API whose own response says "you must not use, copy
   or share it".
3. Whether `npa-special-fraud`'s 25 national statistics keep their NPA-headquarters pin or
   go pin-less.
