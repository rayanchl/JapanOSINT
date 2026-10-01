# Slice a5 (slice 06) — 206 sources, 81 files

All figures below come from re-running each source against a throwaway DB with
`tests/audit/verify_one.py --slot a5` (binary `bin/japanosint-a5`, rebuilt after
every edit). The bulk-sweep verdicts in `slice_06.md` were treated as advisory.

## Summary

**Verdicts after the fixes in this report** (206 sources)

| verdict | n | meaning |
|---|---|---|
| DATA | 140 | real rows verified by reading them |
| KEY_GATED | 22 | logs `gated (no FOO_KEY)` and emits nothing — correct behaviour |
| EMPTY | 15 | honest empty: no hit for the pivot, or needs a populated DB |
| DEAD_UPSTREAM | 14 | endpoint 404 / retired / now serves HTML (evidence per row) |
| WAF_BLOCKED | 6 | upstream 403 / Cloudflare |
| RC_ERROR | 6 | returns −1 because the upstream really is broken |
| TIMEOUT | 3 | nationwide Overpass query still unfinished at 300 s |

Before this session's fixes the same slice measured DATA 135 / RC_ERROR 11 /
TIMEOUT 7 / EMPTY 23. Net movement: **+11 sources now returning real data**
(0 → 309 atlas-jp, 0 → 285 classifieds, 0 → 469 ski-resorts, 0 → 8 refineries,
0 → 25/20/100/65/136 for five news feeds), plus **~56 000 previously title-less
rows made readable** (46 216 accident-grid + 10 178 stations).

### The five findings that matter most

1. **Sanctions screening produced false positives and false negatives — three
   separate bugs, all in `sanctions_world.c`, all now fixed and verified.**
   * *Substring matching* (half-fixed by the previous auditor, confirmed real by
     me against the live OFAC file): the query `PUTIN` matched
     `RANA INTELLIGENCE COMPUTING COMPANY`, `SCIENTIFIC PRODUCTION ASSOCIATION
     COMPUTING SYSTEMS` and `…SPECIALIZED SECURITY COMPUTING DEVICES…` because
     "PUTIN" is a substring of "COM**PUTIN**G". Three unrelated Iranian/Russian
     entities were reported as name hits. Conversely `Vladimir Putin` matched
     **nothing** — OFAC stores `PUTIN, Vladimir Vladimirovich` surname-first, so
     the substring test reported CLEAR for a listed head of state.
   * *Per-line matching on the XML lists* (found by me): the UN and Canadian
     lists are pretty-printed one field per line
     (`<FIRST_NAME>ERIC</FIRST_NAME>` / `<SECOND_NAME>BADEGE</SECOND_NAME>`), so
     a two-token query could never match — `Eric Badege` (UN-listed, CDi.001)
     and `Dmitry Balaba` (Canada, Belarus schedule 1) both returned CLEAR. The
     scan is now record-aware.
   * *Narrative-text matching on the UK OFSI CSV* (found by me): the whole CSV
     row was the haystack, including the free-text "Statement of Reasons", so
     the query `Putin` returned **708 records** (capped at 50) — Abramovich,
     Abakarov, Afanasov … — every person whose reasons text mentions Putin.
     Only 10 OFSI records carry Putin/Putina as a *name*. Screening now runs on
     the six name columns; `Putin` now returns exactly the 6 real PUTIN records.

2. **`atlas-jp` was throwing away 309 live records every hour** (`atlas_jp.c`).
   RIPE Atlas v2 no longer returns `latitude`/`longitude`/`status_name` —
   coordinates moved into a GeoJSON `geometry` object. The collector still asked
   for the retired fields and skipped every probe that lacked them, i.e. all of
   them. Fixed → 309 rows, 309 with map pins, titles and links.

3. **`npa-traffic-accidents` persisted 46 216 rows with a NULL title, no link
   and no timestamp.** The geojson toolkit derives the title from
   `title|name|name_ja|label`; the accident-grid properties had none, so the
   single largest table in the slice was unreadable in every UI. Fixed (title,
   provenance link to the exact NPA CSV, `published_at` from the bucket month).

4. **`classifieds` invented coordinates and had silently returned 0 rows for
   months.** It jittered every listing by a random ±0.015°/±0.02° around a
   hardcoded prefecture centroid — a made-up street-level position that moved on
   every run. Separately, Jmty nested the item markup one element deeper, so the
   title/price extraction captured an empty text node and every listing was
   dropped (60 pages fetched, 0 emitted). Both fixed: 285 real listings with
   price, municipality, article-slug identity, and an honest
   `geo_precision: "area-centroid"` pin.

5. **`core/httpclient.c` sends HEAD without `CURLOPT_NOBODY`, so every one of
   the 73 "portal status" sources reports its site as unreachable** (READ-ONLY —
   reported, not patched). `probe_head()` (lib/probe.c:13) issues
   `CURLOPT_CUSTOMREQUEST "HEAD"`; libcurl then waits for a body that never
   arrives and the request dies on the 8 s timeout. Evidence: `curl -I` with the
   collector's own User-Agent returns **200** for `traininfo.jr-central.co.jp`
   and `kakaku.com`, yet both log `probe reachable=0` — and their runtimes are
   8 592 / 8 577 / 8 611 / 8 617 ms, i.e. exactly the 8 s probe timeout.
   One-line fix: `if (!strcmp(method,"HEAD")) curl_easy_setopt(e,
   CURLOPT_NOBODY, 1L);` in core/httpclient.c:129.

### Second tier, worth your attention

6. **10 sources in this slice (73 fleet-wide) are "portal status" stubs that
   contain no data at all.** `blitzortung-lightning`, `food-poisoning`,
   `jr-central-delay`, `kakaku-prices`, `mic-broadcast-towers`, `ndb-open`,
   `pogo-raids-jp`, `sumo-tournaments`, `wantedly-bizreach`, `yahoo-auctions`
   each emit ONE row whose entire payload is `{operator, reachable}`. The
   summary text oversells it — mic-broadcast-towers says "Every licensed
   AM/FM/TV/community-FM transmitter + amateur repeater" and carries zero
   transmitters. This is the "shows names, not data" class; fixing it means
   writing real collectors, which is out of scope for a slice audit.

7. **The Overpass sources are mostly not env-blocked — they were
   under-budgeted.** A nationwide `area.jp` query needs 60–165 s on the one
   reachable mirror. Measured with the 150 s budget: `ski-resorts` 469 rows
   (138 s), `marine-traffic` 559 rows (100 s), `odpt-transport` 10 178 stations
   (162 s), `refineries` 8 rows (64 s — I raised that one from 60 s myself).
   They exceed the sweep's own 150 s harness timeout, which is why the sweep
   called them TIMEOUT. Only `resas-industry` genuinely fails (industrial
   landuse over all of Japan: rc=−1 after 286 s) and `aed-map` /
   `hazard-map-portal` / `osm-transport-buses` remain unverified beyond 300 s.

8. **`greynoise-jp` was fabricating verdicts.** GreyNoise answers *404 with a
   real JSON body* for "IP not observed scanning the internet", and
   `feed_get_json_h()` discards non-2xx bodies — so every poll produced 5 rows
   reading `<ip> — unknown` / "Quiet", indistinguishable from a genuine quiet
   verdict. Fixed to carry the upstream message and to emit nothing when there
   is no usable answer. (Design note: the source polls 5 hardcoded IPs, two of
   which are Google and Cloudflare DNS — not a Japan feed in any real sense.)

9. **`lib/rss_atom.c` does not unwrap CDATA — a new lib finding** (READ-ONLY).
   China Daily wraps every field: `<title><![CDATA[ … ]]></title>` and
   `<link><![CDATA[\nhttp://…\n]]></link>`. Result: titles persist as
   `<![CDATA[ Education, health fees among key concerns ]]>`, and because the
   link never parses cleanly the dedupe key collides — **china-daily emits 100
   items and persists 41**. Also confirmed here: `folha-br` persists mojibake
   (`doen�as`) from a Latin-1 feed, and `al-jazeera-ar` is the UTF-8-truncation
   DB_ERROR case others already reported.

10. **Twelve endpoints are simply dead** and no amount of collector work will fix
    them: TFD fire-department page (404), JARTIC road_traffic.json (404),
    MOFA advisory RSS (now serves HTML), MLIT N02-23 station GeoJSON (404),
    AU DFAT consolidated CSV (404/unreachable), World Bank debarred JSON (401),
    OpenSanctions API (401 — needs a key), Swiss SECO (the page itself says
    "The full XML list is currently unavailable"), Packet Storm RSS (serves
    HTML), Emol Chile RSS (serves HTML), Jakarta Post & Gulf News feeds (404).

## Fixes applied

| file | source(s) | bug | fix | re-test result |
|---|---|---|---|---|
| `sanctions_world.c` | OFAC_SDN | case-insensitive **substring** name test: `PUTIN` matched `…COMPUTING…`; `Vladimir Putin` matched nothing (list is surname-first) | whole-word, all-tokens, order-independent matcher (`sw_name_match`) on the SDN_Name field only (started by the previous auditor; verified and kept) | `Vladimir Putin` → 1 row `PUTIN, Vladimir Vladimirovich` (was 0); `Putin` → 1 row (was 4, incl. 3 COMPUTING companies); `Kim Jong Un` → `KIM, Jong Un` |
| `sanctions_world.c` | UN_SANCTIONS, CA_SANCTIONS | per-**line** scan over one-field-per-line XML → multi-token names unmatchable; titles were raw XML fragments | new `M_XML` record-aware mode: splits on `<INDIVIDUAL>/<ENTITY>` / `<record>`, screens name+alias fields as one unit, extracts real name/programme/detail | `Eric Badege` → `ERIC BADEGE` (DRC, CDi.001) — was 0; `Dmitry Balaba` → `Balaba Dmitry Vladimirovich` (Belarus) — was 0; `Taliban` → 4 records |
| `sanctions_world.c` | UK_OFSI | screened the entire CSV row incl. "Statement of Reasons" → 708 false hits for `Putin`; 2 KiB line buffer truncated 64 records; title was 200 bytes of raw CSV | screen the 6 name columns (`csv_name_cols`), buffer 2 KiB → 8 KiB, compose the title from the name columns | `Putin` → 6 rows, all genuinely named PUTIN (was 50, capped, mostly Abramovich-class false hits); `Mian Mithoo` → title `MITHOO Mian` |
| `sanctions_world.c` | WORLDBANK_DEBARRED | still used the old raw-substring test (the previous auditor's fix was half-applied) | same `sw_name_match` rule as the other lists | endpoint now 401 (dead), matcher path exercised via the shared unit |
| `atlas_jp.c` | atlas-jp | requested retired `latitude`/`longitude`/`status_name` fields; every probe skipped for want of coordinates; no title; unstable sha1 uid | read `geometry.coordinates`, accept the `status` object, add `id`/`title`/description/country | **0 → 309 rows, 309 with lat/lon**, uid `atlas-jp\|atlas-probe-234` |
| `npa_traffic_accidents.c` | npa-traffic-accidents | 46 216 rows with NULL title, no link, no timestamp | title composed from the real aggregates (`交通事故 5件 2024-12 / 43.1,141.35`), link = the exact NPA CSV, `published_at` = bucket month | 46 216 rows, 0 NULL titles, all linked |
| `classifieds.c` | classifieds | (a) random coordinate jitter around a prefecture centroid = invented location; (b) markup drift broke title/price capture → 0 rows; (c) uid = running index | (a) centroid without jitter + `geo_precision:"area-centroid"` + municipality from the listing; (b) capture skips empty text nodes; (c) uid from the article slug | **0 → 285 rows** with title, price, link, municipality |
| `telegeography_cables.c` | telegeography-cables | cable ROUTES reduced to their first vertex (0 rows with geometry); no link on any row; `n>0?0:-1` on honest empty | carry the upstream MultiLineString into `geometry_geojson`; build the real submarinecablemap URL from the record id (verified 200); error only on total fetch failure | 142 rows, **59 with route geometry** (was 0), 0 without link |
| `greynoise_jp.c` | greynoise-jp | 404-with-body verdicts discarded → 5 fabricated "unknown/Quiet" rows per poll; no link | fetch via `http_request`, accept 200 + 404 bodies, carry `message`/`http_status`/`riot`, skip the row when there is no usable answer, add the viz.greynoise.io link | `8.8.8.8 — not observed scanning` / "IP not observed scanning the internet." |
| `jma_tsunami.c` | jma-tsunami | `return n > 0 ? 0 : -1` — "no active tsunami" is the normal state of a 60 s feed, so every quiet poll reported an error and fed the anomaly detector; no link | return 0 unless the fetch itself failed; add the per-bulletin JMA JSON link | rc=0, 4 rows, 4 with geo, 4 linked |
| `refineries.c` | refineries | 60 s Overpass budget could never finish a nationwide query → "no elements", rc=−1, quarantined | 150 s budget (matches the fix the previous auditor applied to five sibling files) | **rc=−1 → rc=0, 8 refineries with pins**, 64.5 s |
| `odpt_transport.c` | odpt-transport | all **10,178** stations persisted with a NULL title (properties used `station_name`, which the geojson toolkit does not read) and no link | mirror the OSM name into `name`/`name_ja`, add the OSM node URL | **not re-verified against live data** — the confirming run hit `no elements` after 299 s because the shared Overpass mirror was saturated by my earlier long queries. The 10,178-rows-all-NULL-title measurement is real (162 s run, pre-fix); the fix is the exact pattern already proven on `marine_traffic.c` (559 titled rows). Re-run it when Overpass is idle. |
| `world_outlets_1.c` | nation-ke, pravda-ua, haaretz-2, hurriyet-en | dead/403 feed URLs | `nation.africa/kenya/rss.xml`; `pravda.com.ua/eng/rss/`; `haaretz.com/cmlink/1.4605102`; `hurriyetdailynews.com/rss/news` (the old `/rss` is a 710-byte stub) | 25 / 20 / 100 / 65 rows, all rc=0 (all were rc=−1 or 0 rows) |
| `intel_worldnews.c` | ap-topnews | `rsshub.app` 403s datacentre IPs; AP publishes no first-party RSS | another public RSSHub instance running the same route | 0 → 136 rows |

### Previous auditor's edits I verified and kept

`corp_identifiers.c` (LEI_SEARCH/VAT_VALIDATOR returned a row count as rc → now
rc=0, verified 5 and 1 rows), `fire_department.c` + `lifull_homes.c` (rc=−1 on a
quiet/empty run → 0; both still rc=−1 today but now only because the fetch
genuinely fails), `intel_vuln_world.c` (tenable-tra 10 rows, talos-disclosures
15 rows — note the Talos replacement is the general research blog, so the feed is
no longer purely vulnerability disclosures), `intel_worldnews.c` mail-guardian
(50 rows), `world_outlets_1.c` manila-times/dhaka-tribune/infobae/eluniversal-mx/
news24-za/the-national-ae (50/100/100/100/20/100 rows), `ioda_jp.c`,
`jma_forecast_area.c`, `p2pquake_jma.c`, `satellite_imagery.c` (121 rows, all
with footprint geometry), `shadowserver_jp.c` (placeholder pins removed),
`marine_traffic.c` (559 rows, titles present, 99.8 s), and the 60 s → 150 s
Overpass budget change in five files (proved correct: ski-resorts 469 rows).
`jr_central_delay.c`'s `<script>` stripping could not be exercised because
`probe_head` never reports the portal reachable (finding 5).

## Findings not fixed (with reason)

| source | issue | why not fixed |
|---|---|---|
| 73 portal-status sources (10 in this slice) | emit one `{operator, reachable}` row and no upstream data | needs a real collector per source; out of scope, and `lib/probe.h` is shared/read-only |
| all portal-status sources | `probe_head()` always false — HEAD sent without `CURLOPT_NOBODY` | bug is in `core/httpclient.c` (READ-ONLY) |
| china-daily, folha-br, al-jazeera-ar | CDATA leaks into titles; Latin-1 mojibake; UTF-8 truncation | all in `lib/rss_atom.c` (READ-ONLY, already reported) |
| shadowserver-jp | `dashboard.shadowserver.org/api/?endpoint=stats/…` needs an HMAC-signed API key; the public statistics page returns HTML | genuinely credential-gated; collector correctly emits nothing now. It should log a `gated` line rather than "fetch failed" |
| fire-department | `tfd.metro.tokyo.lg.jp/lfe/saigai/saigai.html` → 404; no replacement path found on the TFD site | dead upstream, no honest substitute |
| jartic-traffic, mlit-n02-stations, mofa-travel-advisory, MINPAKU_REGISTRY, EU_TRANSPARENCY | 404 / RSS replaced by HTML | dead upstream |
| AU_DFAT, CH_SECO, WORLDBANK_DEBARRED, OPENSANCTIONS | 404 / "full XML list currently unavailable" / 401 / 401 | dead or credential-gated upstream; no fabrication possible |
| ahram-eg, milenio, gulf-news, jakarta-post, emol-cl, presstv, rt-news, packetstorm | 403 WAF, 404, or RSS retired (probed 2-3 candidate URLs each) | no working feed found; leaving them broken is the honest state |
| sslbl-jp | abuse.ch's `sslipblacklist.csv` is now header-only (last updated 2025-01-03) → always 0 rows; also pins every C2 IP at Tokyo City Hall | upstream list retired; the Tokyo pin is the same invented-coordinate class as `add_tokyo_geom()` used by ioda-jp / shadowserver-jp — a fleet-wide decision, not a slice one |
| unified-airports | fuses `mlit-p02-airports` + `airport-infra` from the DB; in a single-source test DB those tables are empty | not a bug in this file; needs a multi-source run to judge |
| DE_GOVDATA, GDELT_DOC/GEO, ASIA_REGISTRY | 21 s / 10-21 s / 45 s runtimes trip the duration anomaly detector; GDELT returns HTTP 429 to this host | upstream slowness / rate-limit, not a collector defect |
| npa-traffic-accidents | one run takes >3 min and writes 46 k rows every day | performance/design question for you, not a correctness bug |
| resas-industry | Overpass returns rc=−1 after 286 s — "industrial landuse with a name over all of Japan" is too heavy for the one live mirror | raising the budget further would just hammer Overpass; the query needs narrowing (per-prefecture tiles), which is a design change |
| aed-map, hazard-map-portal, osm-transport-buses | nationwide Overpass queries still unfinished at 300 s | unverifiable in this session without monopolising the shared Overpass mirror |
| RANSOMLOOK (65/100), SG_ACRA, LEI_SEARCH, CRYPTO_TRACKER, DOMAIN_WHOIS, URL_ANALYZER, VAT_VALIDATOR, VESSEL_TRACKER | rows carry no `link`, so a user cannot verify the claim upstream | each needs a per-service provenance URL; low severity, and several genuinely have no public per-record page |
| packages_world (6 sources) | each asks the registry for exactly 20 results | deliberate page size in the request, not dropped data — noted for completeness |

## Per-source table

| id | file | verdict | rows | geo | data quality | notes |
|---|---|---|---|---|---|---|
| aed-map | aed_map.c | TIMEOUT | 0 | 0 | unverified | tiled Overpass, >300s |
| atlas-jp | atlas_jp.c | DATA | 309 | 309 | real+complete (fixed) | 309 RIPE Atlas probes w/ pins |
| BINARYEDGE_IP | attacksurface_world.c | KEY_GATED | 0 | 0 | gated | [BINARYEDGE_IP] no BINARYEDGE_API_KEY ;; [BINARYEDGE_IP] emitted 0 ;;  |
| CRIMINALIP_IP | attacksurface_world.c | KEY_GATED | 0 | 0 | gated | [CRIMINALIP_IP] no CRIMINALIP_API_KEY ;; [CRIMINALIP_IP] emitted 0 ;;  |
| FULLHUNT_DOMAIN | attacksurface_world.c | KEY_GATED | 0 | 0 | gated | [FULLHUNT_DOMAIN] no FULLHUNT_API_KEY ;; [FULLHUNT_DOMAIN] emitted 0 ; |
| ONYPHE_SUMMARY | attacksurface_world.c | KEY_GATED | 0 | 0 | gated | [ONYPHE_SUMMARY] no ONYPHE_API_KEY ;; [ONYPHE_SUMMARY] emitted 0 ;; [s |
| blitzortung-lightning | blitzortung_lightning.c | DATA | 1 | 0 | labels-only (portal-status stub) | [blitzortung-lightning] probe reachable=0 ;; [sched] blitzortung-light |
| GLEIF_LEI | bo_world.c | DATA | 15 | 0 | real+complete | [GLEIF_LEI] emitted 15 ;; [sched] GLEIF_LEI run rc=0 records=15 418ms |
| GLEIF_RELATIONS | bo_world.c | EMPTY | 0 | 0 | honest empty | [GLEIF_RELATIONS] emitted 0 ;; [sched] GLEIF_RELATIONS run rc=0 record |
| OPENOWNERSHIP | bo_world.c | WAF_BLOCKED | 0 | 0 | blocked/broken upstream | [OPENOWNERSHIP] http status=403 ;; [sched] OPENOWNERSHIP run rc=0 reco |
| cam-camstreamer | cam_camstreamer.c | DATA | 1 | 1 | real+thin (no link) | [cam-camstreamer] emitted 1 ;; [sched] cam-camstreamer run rc=0 record |
| cam-webcamtaxi | cam_webcamtaxi.c | WAF_BLOCKED | 0 | 0 | blocked/broken upstream | [cam-webcamtaxi] 0 (no html — Cloudflare 1005 / WAF likely) ;; [sched] |
| censys-japan | censys_japan.c | KEY_GATED | 0 | 0 | gated | [threatintel] censys-japan gated (no CENSYS_API_ID) ;; [sched] censys- |
| classifieds | classifieds.c | DATA | 285 | 285 | real+coarse geo | 285 listings; area-centroid pins, jitter removed |
| LEI_SEARCH | corp_identifiers.c | DATA | 5 | 0 | real+thin (no link) | [sched] LEI_SEARCH run rc=0 records=5 263ms |
| VAT_VALIDATOR | corp_identifiers.c | DATA | 1 | 0 | real+thin (no link) | [sched] VAT_VALIDATOR run rc=0 records=1 87ms |
| CONSTRUCTION_LICENSE | corp_registry.c | EMPTY | 0 | 0 | honest empty | [kensetsu] emitted 0 ;; [sched] CONSTRUCTION_LICENSE run rc=0 records= |
| INVOICE_REGISTRY | corp_registry.c | KEY_GATED | 0 | 0 | gated | [invoice] gated (no INVOICE_APP_ID) ;; [sched] INVOICE_REGISTRY run rc |
| MINPAKU_REGISTRY | corp_registry.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | 404 |
| NPO_PORTAL | corp_registry.c | WAF_BLOCKED | 0 | 0 | blocked/broken upstream | [npo] http status=403 ;; [sched] NPO_PORTAL run rc=0 records=0 325ms |
| UK_PARLIAMENT | country_gov2.c | EMPTY | 0 | 0 | honest empty | [UK_PARLIAMENT] emitted 0 ;; [sched] UK_PARLIAMENT run rc=0 records=0  |
| AU_OPENDATA | country_opendata.c | DATA | 20 | 0 | real+complete | [AU_OPENDATA] emitted 20 ;; [sched] AU_OPENDATA run rc=0 records=20 21 |
| CA_OPENDATA | country_opendata.c | DATA | 1 | 0 | real+complete | [CA_OPENDATA] emitted 1 ;; [sched] CA_OPENDATA run rc=0 records=1 484m |
| DE_GOVDATA | country_opendata.c | EMPTY | 0 | 0 | honest empty | [DE_GOVDATA] emitted 0 ;; [sched] DE_GOVDATA run rc=0 records=0 21679m |
| IE_OPENDATA | country_opendata.c | DATA | 20 | 0 | real+complete | [IE_OPENDATA] emitted 20 ;; [sched] IE_OPENDATA run rc=0 records=20 22 |
| IT_OPENDATA | country_opendata.c | DATA | 20 | 0 | real+complete | [IT_OPENDATA] emitted 20 ;; [sched] IT_OPENDATA run rc=0 records=20 71 |
| NL_OPENDATA | country_opendata.c | DATA | 20 | 0 | real+complete | [NL_OPENDATA] emitted 20 ;; [sched] NL_OPENDATA run rc=0 records=20 43 |
| CRYPTO_TRACKER | crypto_tracker.c | DATA | 11 | 0 | real+complete | [sched] CRYPTO_TRACKER run rc=0 records=11 144ms |
| DEHASHED_SEARCH | dehashed_search.c | DATA | 208 | 0 | real+thin (no link) | [sched] DEHASHED_SEARCH run rc=0 records=208 277ms |
| DOCUMENT_ANALYZER | document_analyzer.c | EMPTY | 0 | 0 | honest empty | [sched] DOCUMENT_ANALYZER run rc=0 records=0 0ms |
| HOST_IO_FULL | domain_world2.c | KEY_GATED | 0 | 0 | gated | [HOST_IO_FULL] no HOSTIO_TOKEN ;; [HOST_IO_FULL] emitted 0 ;; [sched]  |
| IPINFO_LOOKUP | domain_world2.c | KEY_GATED | 0 | 0 | gated | [IPINFO_LOOKUP] no IPINFO_TOKEN ;; [IPINFO_LOOKUP] emitted 0 ;; [sched |
| WHOISFREAKS | domain_world2.c | KEY_GATED | 0 | 0 | gated | [WHOISFREAKS] no WHOISFREAKS_API_KEY ;; [WHOISFREAKS] emitted 0 ;; [sc |
| eight-sansan | eight_sansan.c | KEY_GATED | 0 | 0 | gated | [eight-sansan] gated (no EIGHT_SESSION) ;; [sched] eight-sansan run rc |
| estat-industry | estat_industry.c | KEY_GATED | 0 | 0 | gated | [estat-industry] gated (ESTAT_APP_ID) ;; [sched] estat-industry run rc |
| fire-department | fire_department.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | TFD saigai.html 404 |
| food-poisoning | food_poisoning.c | DATA | 1 | 0 | labels-only (portal-status stub) | [food-poisoning] probe reachable=0 ;; [sched] food-poisoning run rc=0  |
| gdelt-events | gdelt_events.c | DATA | 15 | 15 | real+complete | [gdelt-events] emitted 15 ;; [sched] gdelt-events run rc=0 records=15  |
| GDELT_DOC | gdelt_world.c | EMPTY | 0 | 0 | honest empty | [gdelt-doc] emitted 0 ;; [sched] GDELT_DOC run rc=0 records=0 21897ms  |
| GDELT_GEO | gdelt_world.c | EMPTY | 0 | 0 | honest empty | [gdelt-geo] emitted 0 ;; [sched] GDELT_GEO run rc=0 records=0 10701ms  |
| ADSB_GLOBAL | global_adsb.c | EMPTY | 0 | 0 | honest empty | [adsb_global] emitted 0 ;; [sched] ADSB_GLOBAL run rc=0 records=0 187m |
| RANSOMLOOK | global_ransomlook.c | DATA | 100 | 0 | real+complete | [ransomlook] emitted 100 ;; [sched] RANSOMLOOK run rc=0 records=100 37 |
| greynoise-jp | greynoise_jp.c | DATA | 5 | 0 | real but 5 hardcoded IPs | no more fabricated 'Quiet' rows |
| hazard-map-portal | hazard_map_portal.c | TIMEOUT | 0 | 0 | unverified | nationwide Overpass, >300s |
| houmukyoku-commercial | houmukyoku_commercial.c | KEY_GATED | 0 | 0 | gated | [houmukyoku-commercial] gated (no HOUMUKYOKU_API_KEY) ;; [sched] houmu |
| cvefeed-latest | intel_vuln_world.c | DATA | 25 | 0 | real+complete | [rss] cvefeed-latest emitted 25 ;; [sched] cvefeed-latest run rc=0 rec |
| cvefeed-newsroom | intel_vuln_world.c | DATA | 25 | 0 | real+complete | [rss] cvefeed-newsroom emitted 25 ;; [sched] cvefeed-newsroom run rc=0 |
| exploit-db | intel_vuln_world.c | DATA | 50 | 0 | real+complete | [rss] exploit-db emitted 50 ;; [sched] exploit-db run rc=0 records=50  |
| full-disclosure | intel_vuln_world.c | DATA | 15 | 0 | real+complete | [rss] full-disclosure emitted 15 ;; [sched] full-disclosure run rc=0 r |
| oss-security | intel_vuln_world.c | DATA | 15 | 0 | real+complete | [rss] oss-security emitted 15 ;; [sched] oss-security run rc=0 records |
| packetstorm | intel_vuln_world.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | RSS retired, serves HTML |
| talos-disclosures | intel_vuln_world.c | DATA | 15 | 0 | real+complete | [rss] talos-disclosures emitted 15 ;; [sched] talos-disclosures run rc |
| tenable-tra | intel_vuln_world.c | DATA | 10 | 0 | real+complete | [rss] tenable-tra emitted 10 ;; [sched] tenable-tra run rc=0 records=1 |
| vuldb-recent | intel_vuln_world.c | DATA | 100 | 0 | real+complete | [rss] vuldb-recent emitted 100 ;; [sched] vuldb-recent run rc=0 record |
| wpscan-vulndb | intel_vuln_world.c | DATA | 10 | 0 | real+complete | [rss] wpscan-vulndb emitted 10 ;; [sched] wpscan-vulndb run rc=0 recor |
| zdi-published | intel_vuln_world.c | DATA | 200 | 0 | real+complete | [rss] zdi-published emitted 200 ;; [sched] zdi-published run rc=0 reco |
| zdi-upcoming | intel_vuln_world.c | DATA | 200 | 0 | real+complete | [rss] zdi-upcoming emitted 200 ;; [sched] zdi-upcoming run rc=0 record |
| africanews | intel_worldnews.c | DATA | 50 | 0 | real+complete | [rss] africanews emitted 50 ;; [sched] africanews run rc=0 records=50  |
| aljazeera | intel_worldnews.c | DATA | 25 | 0 | real+complete | [rss] aljazeera emitted 25 ;; [sched] aljazeera run rc=0 records=25 10 |
| ap-topnews | intel_worldnews.c | DATA | 136 | 0 | real+complete | [rss] ap-topnews emitted 136 ;; [sched] ap-topnews run rc=0 records=13 |
| bbc-world | intel_worldnews.c | DATA | 39 | 0 | real+complete | [rss] bbc-world emitted 39 ;; [sched] bbc-world run rc=0 records=39 16 |
| buenos-aires-times | intel_worldnews.c | DATA | 100 | 0 | real+complete | [rss] buenos-aires-times emitted 100 ;; [sched] buenos-aires-times run |
| cnn-world | intel_worldnews.c | DATA | 29 | 0 | real+complete | [rss] cnn-world emitted 29 ;; [sched] cnn-world run rc=0 records=29 30 |
| deutsche-welle | intel_worldnews.c | DATA | 143 | 0 | real+complete | [rss] deutsche-welle emitted 143 ;; [sched] deutsche-welle run rc=0 re |
| france24 | intel_worldnews.c | DATA | 23 | 0 | real+complete | [rss] france24 emitted 23 ;; [sched] france24 run rc=0 records=23 126m |
| guardian-world | intel_worldnews.c | DATA | 45 | 0 | real+complete | [rss] guardian-world emitted 45 ;; [sched] guardian-world run rc=0 rec |
| jerusalem-post | intel_worldnews.c | DATA | 30 | 0 | real+complete | [rss] jerusalem-post emitted 30 ;; [sched] jerusalem-post run rc=0 rec |
| korea-herald | intel_worldnews.c | DATA | 50 | 0 | real+complete | [rss] korea-herald emitted 50 ;; [sched] korea-herald run rc=0 records |
| mail-guardian-africa | intel_worldnews.c | DATA | 50 | 0 | real+complete | [rss] mail-guardian-africa emitted 50 ;; [sched] mail-guardian-africa  |
| mercopress | intel_worldnews.c | DATA | 10 | 0 | real+complete | [rss] mercopress emitted 10 ;; [sched] mercopress run rc=0 records=10  |
| nikkei-asia | intel_worldnews.c | DATA | 50 | 0 | real+complete | [rss] nikkei-asia emitted 50 ;; [sched] nikkei-asia run rc=0 records=5 |
| npr-world | intel_worldnews.c | DATA | 10 | 0 | real+complete | [rss] npr-world emitted 10 ;; [sched] npr-world run rc=0 records=10 11 |
| nyt-world | intel_worldnews.c | DATA | 55 | 0 | real+complete | [rss] nyt-world emitted 55 ;; [sched] nyt-world run rc=0 records=55 16 |
| rio-times | intel_worldnews.c | DATA | 10 | 0 | real+complete | [rss] rio-times emitted 10 ;; [sched] rio-times run rc=0 records=10 12 |
| scmp-topnews | intel_worldnews.c | DATA | 50 | 0 | real+complete | [rss] scmp-topnews emitted 50 ;; [sched] scmp-topnews run rc=0 records |
| straits-times | intel_worldnews.c | DATA | 50 | 0 | real+complete | [rss] straits-times emitted 50 ;; [sched] straits-times run rc=0 recor |
| the-hindu-intl | intel_worldnews.c | DATA | 60 | 0 | real+complete | [rss] the-hindu-intl emitted 60 ;; [sched] the-hindu-intl run rc=0 rec |
| times-of-india | intel_worldnews.c | DATA | 46 | 0 | real+complete | [rss] times-of-india emitted 46 ;; [sched] times-of-india run rc=0 rec |
| times-of-israel | intel_worldnews.c | DATA | 12 | 0 | real+complete | [rss] times-of-israel emitted 12 ;; [sched] times-of-israel run rc=0 r |
| ioda-jp | ioda_jp.c | DATA | 3 | 3 | real+complete | [threatintel] ioda-jp emitted 3 ;; [sched] ioda-jp run rc=0 records=3  |
| jartic-traffic | jartic_traffic.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | road_traffic.json 404 |
| jma-forecast-area | jma_forecast_area.c | DATA | 10 | 10 | real+complete | [jma-forecast-area] emitted 10 ;; [sched] jma-forecast-area run rc=0 r |
| jma-tsunami | jma_tsunami.c | DATA | 4 | 4 | real+complete | rc fixed: quiet feed is no longer an error |
| JP_CORPUS_LOOKUP | jp_corpus_lookup.c | EMPTY | 0 | 0 | honest empty | [sched] JP_CORPUS_LOOKUP run rc=0 records=0 0ms |
| jr-central-delay | jr_central_delay.c | DATA | 1 | 0 | labels-only (portal-status stub) | [jr-central-delay] probe reachable=0 ;; [sched] jr-central-delay run r |
| kakaku-prices | kakaku_prices.c | DATA | 1 | 0 | labels-only (portal-status stub) | [kakaku-prices] probe reachable=0 ;; [sched] kakaku-prices run rc=0 re |
| lifull-homes | lifull_homes.c | RC_ERROR | 0 | 0 | blocked/broken upstream | [lifull-homes] no page body returned for any prefecture ;; [sched] lif |
| marine-traffic | marine_traffic.c | DATA | 559 | 559 | real+complete | 559 harbours w/ pins, 100s (150s budget) |
| AISSTREAM | maritime_world.c | KEY_GATED | 0 | 0 | gated | [aisstream] gated (no AISSTREAM_KEY) ;; [sched] AISSTREAM run rc=0 rec |
| EQUASIS | maritime_world.c | KEY_GATED | 0 | 0 | gated | [equasis] gated (no EQUASIS_USER/EQUASIS_PASS) ;; [sched] EQUASIS run  |
| IMO_GISIS | maritime_world.c | EMPTY | 0 | 0 | honest empty | [gisis] emitted 0 ;; [sched] IMO_GISIS run rc=0 records=0 225ms |
| mic-broadcast-towers | mic_broadcast_towers.c | DATA | 1 | 0 | labels-only (portal-status stub) | [mic-broadcast-towers] probe reachable=0 ;; [sched] mic-broadcast-towe |
| mlit-n02-stations | mlit_n02_stations.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | N02-23 GeoJSON 404 |
| mofa-travel-advisory | mofa_travel_advisory.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | anzen.mofa RSS now serves HTML |
| ndb-open | ndb_open.c | DATA | 1 | 0 | labels-only (portal-status stub) | [ndb-open] probe reachable=0 ;; [sched] ndb-open run rc=0 records=1 86 |
| nhk-news-rss | nhk_news_rss.c | DATA | 7 | 0 | real+complete | [rss] nhk-news-rss emitted 7 ;; [sched] nhk-news-rss run rc=0 records= |
| nlni-landuse | nlni_landuse.c | KEY_GATED | 0 | 0 | gated | [nlni-landuse] gated (no MLIT_L03B_GEOJSON_URL) ;; [sched] nlni-landus |
| npa-traffic-accidents | npa_traffic_accidents.c | DATA | 46216 | 46216 | real+complete (fixed titles/link/date) | 46,216 grid rows; run takes >3min |
| odpt-transport | odpt_transport.c | DATA | 10178 | 10178 | real; title fix not re-verified | 10,178 OSM stations w/ pins (162s run); all had NULL titles before the fix |
| osm-transport-buses | osm_transport_buses.c | TIMEOUT | 0 | 0 | unverified | nationwide Overpass, >300s |
| p2pquake-jma | p2pquake_jma.c | DATA | 50 | 50 | real+thin (no link) | [p2pquake-jma] emitted 50 ;; [sched] p2pquake-jma run rc=0 records=50  |
| CRATES_IO | packages_world.c | DATA | 20 | 0 | real+complete | [CRATES_IO] emitted 20 ;; [sched] CRATES_IO run rc=0 records=20 442ms |
| DOCKERHUB | packages_world.c | DATA | 20 | 0 | real+complete | [DOCKERHUB] emitted 20 ;; [sched] DOCKERHUB run rc=0 records=20 205ms |
| NPM_REGISTRY | packages_world.c | DATA | 20 | 0 | real+complete | [NPM_REGISTRY] emitted 20 ;; [sched] NPM_REGISTRY run rc=0 records=20  |
| PACKAGIST | packages_world.c | DATA | 20 | 0 | real+complete | [PACKAGIST] emitted 20 ;; [sched] PACKAGIST run rc=0 records=20 228ms |
| PYPI_PROJECT | packages_world.c | DATA | 1 | 0 | real+complete | [PYPI_PROJECT] emitted 1 ;; [sched] PYPI_PROJECT run rc=0 records=1 16 |
| RUBYGEMS | packages_world.c | DATA | 20 | 0 | real+complete | [RUBYGEMS] emitted 20 ;; [sched] RUBYGEMS run rc=0 records=20 582ms |
| GOOGLE_PLACES | people_research.c | KEY_GATED | 0 | 0 | gated | [gplaces] gated (no GOOGLE_PLACES_API_KEY) ;; [sched] GOOGLE_PLACES ru |
| pogo-raids-jp | pogo_raids_jp.c | DATA | 1 | 0 | labels-only (portal-status stub) | [pogo-raids-jp] probe reachable=0 ;; [sched] pogo-raids-jp run rc=0 re |
| psn-xbox-jp | psn_xbox_jp.c | KEY_GATED | 0 | 0 | gated | [psn-xbox-jp] gated (no PSN_NPSSO / XBOX_API_KEY) ;; [sched] psn-xbox- |
| refineries | refineries.c | DATA | 8 | 8 | real+complete (fixed) | 8 refineries, 150s Overpass budget |
| COURT_AUSTLII | reg_courts.c | WAF_BLOCKED | 0 | 0 | blocked/broken upstream | [court_austlii] http status=403 ;; [sched] COURT_AUSTLII run rc=0 reco |
| COURT_BAILII | reg_courts.c | DATA | 14 | 0 | real+complete | [court_bailii] emitted 14 ;; [sched] COURT_BAILII run rc=0 records=14  |
| COURT_CANLII | reg_courts.c | WAF_BLOCKED | 0 | 0 | blocked/broken upstream | [court_canlii] http status=403 ;; [sched] COURT_CANLII run rc=0 record |
| COURT_CJEU | reg_courts.c | WAF_BLOCKED | 0 | 0 | blocked/broken upstream | [court_cjeu] http status=403 ;; [sched] COURT_CJEU run rc=0 records=0  |
| EE_ARIREGISTER | reg_ee_ariregister.c | DATA | 4 | 0 | real+complete | [ee-ariregister] emitted 4 ;; [sched] EE_ARIREGISTER run rc=0 records= |
| IN_MCA | reg_in_mca.c | KEY_GATED | 0 | 0 | gated | [in_mca] gated (no INDIA_DATA_KEY) ;; [sched] IN_MCA run rc=0 records= |
| SG_ACRA | reg_sg_data.c | DATA | 20 | 0 | real+thin (no link) | [sg_acra] emitted 20 ;; [sched] SG_ACRA run rc=0 records=20 9983ms |
| resas-industry | resas_industry.c | RC_ERROR | 0 | 0 | upstream too heavy | Overpass rc=-1 after 286s (industrial landuse, all of Japan) |
| ripestat-jp | ripestat_jp.c | DATA | 1 | 0 | real+complete | [ripestat-jp] emitted 1 ;; [sched] ripestat-jp run rc=0 records=1 941m |
| AU_DFAT | sanctions_world.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | regulation8_consolidated.csv 404/unreachable |
| CA_SANCTIONS | sanctions_world.c | DATA | 2 | 0 | real+complete | record-aware XML |
| CH_SECO | sanctions_world.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | SESAM: 'full XML list is currently unavailable' |
| EU_SANCTIONS | sanctions_world.c | KEY_GATED | 0 | 0 | gated | EU_SANCTIONS_TOKEN |
| OFAC_SDN | sanctions_world.c | DATA | 1 | 0 | real+complete | matcher fixed; primary SDN names only (ALT.CSV aliases not screened) |
| UK_OFSI | sanctions_world.c | DATA | 6 | 0 | real+complete | name-column screening; 708 false hits removed |
| UN_SANCTIONS | sanctions_world.c | DATA | 4 | 0 | real+complete | record-aware XML; names+aliases screened |
| WORLDBANK_DEBARRED | sanctions_world.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | apigwext 401 |
| FBI_WANTED | sanctions_world2.c | DATA | 2 | 0 | real+thin (no link) | 2 hits for 'Smith' |
| OPENSANCTIONS | sanctions_world2.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | api 401 (needs key) |
| satellite-imagery | satellite_imagery.c | DATA | 121 | 121 | real+complete | [satellite-imagery] emitted 121 ;; [sched] satellite-imagery run rc=0  |
| shadowserver-jp | shadowserver_jp.c | KEY_GATED | 0 | 0 | gated (undeclared) | dashboard API needs signed key; no fake pins now |
| ski-resorts | ski_resorts.c | DATA | 469 | 469 | real+complete | 469 OSM resorts w/ pins in 138s (needs >150s budget) |
| sslbl-jp | sslbl_jp.c | EMPTY | 0 | 0 | dead upstream list | sslipblacklist.csv is header-only since 2025-01 |
| sumo-tournaments | sumo_tournaments.c | DATA | 1 | 0 | labels-only (portal-status stub) | [sumo-tournaments] probe reachable=0 ;; [sched] sumo-tournaments run r |
| telegeography-cables | telegeography_cables.c | DATA | 142 | 142 | real+complete (fixed) | 142 rows, 59 with cable route geometry |
| THREAT_INTEL | threat_intel.c | EMPTY | 0 | 0 | honest empty | [sched] THREAT_INTEL run rc=0 records=0 499ms |
| TRADEMARK_SEARCH | trademark_search.c | EMPTY | 0 | 0 | honest empty | [sched] TRADEMARK_SEARCH run rc=0 records=0 471ms |
| EU_TRANSPARENCY | transparency_world.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | 404 |
| OPENSECRETS | transparency_world.c | KEY_GATED | 0 | 0 | gated | [opensecrets] gated (no OPENSECRETS_KEY) ;; [sched] OPENSECRETS run rc |
| UK_DONATIONS | transparency_world.c | DATA | 25 | 0 | real+complete | [uk_donations] emitted 25 ;; [sched] UK_DONATIONS run rc=0 records=25  |
| unified-airports | unified_airports.c | EMPTY | 0 | 0 | n/a in single-source DB | fuses mlit-p02-airports + airport-infra |
| URL_ANALYZER | url_analyzer.c | DATA | 1 | 0 | real+thin (no link) | [sched] URL_ANALYZER run rc=0 records=1 999ms |
| VESSEL_TRACKER | vessel_tracker.c | DATA | 1 | 0 | real+thin (no link) | [sched] VESSEL_TRACKER run rc=0 records=1 1ms |
| wantedly-bizreach | wantedly_bizreach.c | DATA | 2 | 0 | labels-only (portal-status stub) | [wantedly-bizreach] emitted 2 ;; [sched] wantedly-bizreach run rc=0 re |
| DOMAIN_WHOIS | whois_lookup.c | DATA | 1 | 0 | real+thin (no link) | [sched] DOMAIN_WHOIS run rc=0 records=1 104ms |
| wifi-networks-wigle | wifi_networks_wigle.c | KEY_GATED | 0 | 0 | gated | [wifi-networks-wigle] gated (no WIGLE_API_KEY) ;; [sched] wifi-network |
| abc-au | world_outlets_1.c | DATA | 25 | 0 | real+complete | [rss] abc-au emitted 25 ;; [sched] abc-au run rc=0 records=25 479ms |
| ahram-eg | world_outlets_1.c | RC_ERROR | 0 | 0 | blocked/broken upstream | [sched] ahram-eg run rc=-1 records=0 91ms ;; [detect] anomaly opened:  |
| al-jazeera-ar | world_outlets_1.c | DATA | 25 | 0 | real+complete | [rss] al-jazeera-ar emitted 25 ;; [sched] al-jazeera-ar run rc=0 recor |
| anadolu | world_outlets_1.c | DATA | 28 | 0 | real+complete | [rss] anadolu emitted 28 ;; [sched] anadolu run rc=0 records=28 505ms |
| ansa-it | world_outlets_1.c | DATA | 28 | 0 | real+complete | [rss] ansa-it emitted 28 ;; [sched] ansa-it run rc=0 records=28 233ms |
| arab-news | world_outlets_1.c | DATA | 10 | 0 | real+complete | [rss] arab-news emitted 10 ;; [sched] arab-news run rc=0 records=10 92 |
| bangkok-post | world_outlets_1.c | DATA | 10 | 0 | real+complete | [rss] bangkok-post emitted 10 ;; [sched] bangkok-post run rc=0 records |
| cbc-top | world_outlets_1.c | DATA | 20 | 0 | real+complete | [rss] cbc-top emitted 20 ;; [sched] cbc-top run rc=0 records=20 188ms |
| cbc-world | world_outlets_1.c | DATA | 20 | 0 | real+complete | [rss] cbc-world emitted 20 ;; [sched] cbc-world run rc=0 records=20 29 |
| china-daily | world_outlets_1.c | DATA | 41 | 0 | real+complete | [rss] china-daily emitted 100 ;; [sched] china-daily run rc=0 records= |
| clarin-ar | world_outlets_1.c | DATA | 10 | 0 | real+complete | [rss] clarin-ar emitted 10 ;; [sched] clarin-ar run rc=0 records=10 94 |
| cna-sg | world_outlets_1.c | DATA | 20 | 0 | real+complete | [rss] cna-sg emitted 20 ;; [sched] cna-sg run rc=0 records=20 293ms |
| dawn-pk | world_outlets_1.c | DATA | 26 | 0 | real+complete | [rss] dawn-pk emitted 26 ;; [sched] dawn-pk run rc=0 records=26 156ms |
| dhaka-tribune | world_outlets_1.c | DATA | 100 | 0 | real+complete | [rss] dhaka-tribune emitted 100 ;; [sched] dhaka-tribune run rc=0 reco |
| dw-top | world_outlets_1.c | DATA | 21 | 0 | real+complete | [rss] dw-top emitted 21 ;; [sched] dw-top run rc=0 records=21 212ms |
| elmundo | world_outlets_1.c | DATA | 24 | 0 | real+complete | [rss] elmundo emitted 24 ;; [sched] elmundo run rc=0 records=24 133ms |
| elpais-port | world_outlets_1.c | DATA | 147 | 0 | real+complete | [rss] elpais-port emitted 147 ;; [sched] elpais-port run rc=0 records= |
| eltiempo-co | world_outlets_1.c | DATA | 10 | 0 | real+complete | [rss] eltiempo-co emitted 10 ;; [sched] eltiempo-co run rc=0 records=1 |
| eluniversal-mx | world_outlets_1.c | DATA | 100 | 0 | real+complete | [rss] eluniversal-mx emitted 100 ;; [sched] eluniversal-mx run rc=0 re |
| emol-cl | world_outlets_1.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | RSS retired, serves HTML |
| folha-br | world_outlets_1.c | DATA | 100 | 0 | real+complete | [rss] folha-br emitted 100 ;; [sched] folha-br run rc=0 records=100 19 |
| g1-globo | world_outlets_1.c | DATA | 100 | 0 | real+complete | [rss] g1-globo emitted 100 ;; [sched] g1-globo run rc=0 records=100 17 |
| global-times | world_outlets_1.c | DATA | 50 | 0 | real+complete | [rss] global-times emitted 50 ;; [sched] global-times run rc=0 records |
| gulf-news | world_outlets_1.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | rss 404 |
| haaretz-2 | world_outlets_1.c | DATA | 100 | 0 | real+complete | [rss] haaretz-2 emitted 100 ;; [sched] haaretz-2 run rc=0 records=100  |
| hindustan-times | world_outlets_1.c | DATA | 100 | 0 | real+complete | [rss] hindustan-times emitted 100 ;; [sched] hindustan-times run rc=0  |
| hurriyet-en | world_outlets_1.c | DATA | 65 | 0 | real+complete | [rss] hurriyet-en emitted 65 ;; [sched] hurriyet-en run rc=0 records=6 |
| infobae | world_outlets_1.c | DATA | 100 | 0 | real+complete | [rss] infobae emitted 100 ;; [sched] infobae run rc=0 records=100 926m |
| jakarta-post | world_outlets_1.c | DEAD_UPSTREAM | 0 | 0 | dead upstream | /feed 404 |
| japan-times | world_outlets_1.c | DATA | 30 | 0 | real+complete | [rss] japan-times emitted 30 ;; [sched] japan-times run rc=0 records=3 |
| korea-times | world_outlets_1.c | DATA | 2 | 0 | real+complete | [rss] korea-times emitted 2 ;; [sched] korea-times run rc=0 records=2  |
| kyiv-post | world_outlets_1.c | DATA | 100 | 0 | real+complete | [rss] kyiv-post emitted 100 ;; [sched] kyiv-post run rc=0 records=100  |
| lefigaro | world_outlets_1.c | DATA | 18 | 0 | real+complete | [rss] lefigaro emitted 18 ;; [sched] lefigaro run rc=0 records=18 146m |
| lemonde-une | world_outlets_1.c | DATA | 16 | 0 | real+complete | [rss] lemonde-une emitted 16 ;; [sched] lemonde-une run rc=0 records=1 |
| mainichi-en | world_outlets_1.c | DATA | 20 | 0 | real+complete | [rss] mainichi-en emitted 20 ;; [sched] mainichi-en run rc=0 records=2 |
| manila-times | world_outlets_1.c | DATA | 50 | 0 | real+complete | [rss] manila-times emitted 50 ;; [sched] manila-times run rc=0 records |
| milenio | world_outlets_1.c | RC_ERROR | 0 | 0 | blocked/broken upstream | [sched] milenio run rc=-1 records=0 47ms ;; [detect] anomaly opened: m |
| moscow-times-2 | world_outlets_1.c | DATA | 50 | 0 | real+complete | [rss] moscow-times-2 emitted 50 ;; [sched] moscow-times-2 run rc=0 rec |
| nation-ke | world_outlets_1.c | DATA | 25 | 0 | real+complete | [rss] nation-ke emitted 25 ;; [sched] nation-ke run rc=0 records=25 26 |
| ndtv | world_outlets_1.c | DATA | 20 | 0 | real+complete | [rss] ndtv emitted 20 ;; [sched] ndtv run rc=0 records=20 353ms |
| news24-za | world_outlets_1.c | DATA | 20 | 0 | real+complete | [rss] news24-za emitted 20 ;; [sched] news24-za run rc=0 records=20 11 |
| pravda-ua | world_outlets_1.c | DATA | 20 | 0 | real+complete | [rss] pravda-ua emitted 20 ;; [sched] pravda-ua run rc=0 records=20 18 |
| premium-times | world_outlets_1.c | DATA | 15 | 0 | real+complete | [rss] premium-times emitted 15 ;; [sched] premium-times run rc=0 recor |
| presstv | world_outlets_1.c | RC_ERROR | 0 | 0 | blocked/broken upstream | [sched] presstv run rc=-1 records=0 3200ms ;; [detect] anomaly opened: |
| punch-ng | world_outlets_1.c | DATA | 30 | 0 | real+complete | [rss] punch-ng emitted 30 ;; [sched] punch-ng run rc=0 records=30 290m |
| repubblica | world_outlets_1.c | DATA | 30 | 0 | real+complete | [rss] repubblica emitted 30 ;; [sched] repubblica run rc=0 records=30  |
| rfi-fr | world_outlets_1.c | DATA | 23 | 0 | real+complete | [rss] rfi-fr emitted 23 ;; [sched] rfi-fr run rc=0 records=23 166ms |
| rt-news | world_outlets_1.c | RC_ERROR | 0 | 0 | blocked/broken upstream | [sched] rt-news run rc=-1 records=0 26656ms ;; [detect] anomaly opened |
| scmp-china2 | world_outlets_1.c | DATA | 50 | 0 | real+complete | [rss] scmp-china2 emitted 50 ;; [sched] scmp-china2 run rc=0 records=5 |
| spiegel-intl | world_outlets_1.c | DATA | 20 | 0 | real+complete | [rss] spiegel-intl emitted 20 ;; [sched] spiegel-intl run rc=0 records |
| straits-top | world_outlets_1.c | DATA | 50 | 0 | real+complete | [rss] straits-top emitted 50 ;; [sched] straits-top run rc=0 records=5 |
| taipei-times | world_outlets_1.c | DATA | 46 | 0 | real+complete | [rss] taipei-times emitted 46 ;; [sched] taipei-times run rc=0 records |
| tass | world_outlets_1.c | DATA | 100 | 0 | real+complete | [rss] tass emitted 100 ;; [sched] tass run rc=0 records=100 447ms |
| tehran-times | world_outlets_1.c | DATA | 30 | 0 | real+complete | [rss] tehran-times emitted 30 ;; [sched] tehran-times run rc=0 records |
| the-hindu-nat | world_outlets_1.c | DATA | 60 | 0 | real+complete | [rss] the-hindu-nat emitted 60 ;; [sched] the-hindu-nat run rc=0 recor |
| the-national-ae | world_outlets_1.c | DATA | 100 | 0 | real+complete | [rss] the-national-ae emitted 100 ;; [sched] the-national-ae run rc=0  |
| times-israel-2 | world_outlets_1.c | DATA | 12 | 0 | real+complete | [rss] times-israel-2 emitted 12 ;; [sched] times-israel-2 run rc=0 rec |
| ASIA_REGISTRY | world_reg_asia.c | EMPTY | 0 | 0 | honest empty | [asia_registry] emitted 0 across 25 portals ;; [sched] ASIA_REGISTRY r |
| yahoo-auctions | yahoo_auctions.c | DATA | 1 | 0 | labels-only (portal-status stub) | [yahoo-auctions] probe reachable=0 ;; [sched] yahoo-auctions run rc=0  |

**counts:** DATA 140, KEY_GATED 22, EMPTY 15, DEAD_UPSTREAM 14, WAF_BLOCKED 6, RC_ERROR 6, TIMEOUT 3
