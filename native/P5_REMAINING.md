# P5 — Remaining Collector Ports (handoff)

State after wave 11 (primitive build-out: zip/ws/SGP4, 2026-05-17):
**313 unique collector ids ported & registered** (build green; no real
duplicate .id; binary self-reports `[sched] 313 sources registered`).
**P5 COLLECTOR PORTING IS COMPLETE.** The only ids not authored by this
effort are the 4 owned by the concurrent P6-infra agent (cameras,
unified-flights, unified-stations, unified-station-footprints — their
Read/clusterer/poller pipeline, already building alongside).

Wave 11 built the 3 "missing primitives" and ported the 4 collectors that
were waiting on them — all live-verified end-to-end:
  * `lib/zipread.{c,h}` — single-entry ZIP + zlib raw inflate. Makefile
    gained `-lz`. → **gdelt-events** (lastupdate.txt → .export.CSV.zip →
    inflate → TSV → JA filter; smoke: emitted 2).
  * `lib/ws.{c,h}` — bounded-window WebSocket over libcurl's WS API.
    DISCOVERY: Apple's system libcurl is built WITHOUT websockets
    ("Unsupported protocol"); Makefile now prefers keg-only Homebrew curl
    (`-I/-L/opt/homebrew/opt/curl` + rpath) which has WS (8.16, OpenSSL).
    This libcurl swap also covers httpclient (strict superset). →
    **bluesky-jetstream-jp** (gold ref; smoke: emitted 5 real JP posts) +
    **certstream-jp** (bounded collect replaces the Node ring buffer;
    intel rows; exact-uid parity impossible vs Node's long-lived buffer —
    documented).
  * `lib/sgp4.{c,h}` — Vallado SGP4 (WGS-72, near-Earth; the algorithm
    satellite.js ports) + TLE parse + gstime + WGS-84 eciToGeodetic. →
    **satellite-tracking** (CelesTrak groups → propagate@now → JAPAN_BBOX;
    smoke: emitted 217). SDP4 deep-space objects skipped — documented
    post-parity (rarely low over Japan).

Toolkits delivered total: overpass, threatintel, csv, linecolor, probe,
htmlparse, mlit_ksj, unified(+capture), zipread, ws, sgp4 + feedlib
get_json_h/post_json/get_text. Session arc:
15→91→122→132→186→193→203→241→273→281→285→293→313. Nothing portable
remains; the 4 open ids are the concurrent agent's P6 infra.

Wave 10: 4 parallel agents hand-ported the 7 remaining tractable bespoke
collectors, all OK + build-verified: **chan-5ch** (per-board Shift_JIS
subject.txt loop via csv_decode_sjis → intel rows), **classifieds**
(multi-page Jmty HTML via htmlparse), **twitter-geo** + **niconico-ranking**
(SELECT geocoded rows FROM intel_items via `ctx->db->h` sqlite3_prepare_v2,
verbatim query + rowToFeature; pre-cutover upsertPosts ingestion is an
ancillary store side-effect, out-of-scope), **flight-adsb** (OpenSky +
4 adsb.lol quadrants + key-gated AeroDataBox, icao24 merge), **mlit-landprice**
(47-pref × 4-quarter loop + embedded MUNICIPALITY_CENTROIDS table),
**wanted-persons** (NPA Unicode-class regex family, modelled on the
`npa_important_wanted.c` gold ref). KEY ENABLER: `db_handle` exposes
`sqlite3 *h` (core/db.h) so intel_items DB-read collectors port directly
with the sqlite3 C API on `ctx->db->h`.

THE FINAL 8 — none is a faithful-port task; each needs separate infra:
  * **4 owned by the concurrent P6-infra agent** (~3,400 LOC in flight,
    DO NOT TOUCH): `cameras` (camera sweep sqlite), `unified-flights`
    (planeAdsbPoller), `unified-stations` + `unified-station-footprints`
    (stationClusterer / stationFootprintsStore precomputed sqlite).
  * **4 blocked on a NEW PRIMITIVE** (deliberate infra decision, not a
    port): `bluesky-jetstream-jp` + `certstream-jp` need a TLS **WebSocket
    client** (none in tree; also a poor fit for the run-once scheduler —
    these are continuous streams, likely want a long-lived worker);
    `gdelt-events` needs an in-process **zip reader** (third_party has only
    sqlite3 — add miniz / link `-lz`+a zip central-dir reader);
    `satellite-tracking` needs an **SGP4 TLE propagator** (~500 LOC
    orbital math from CelesTrak TLE text — standalone module). Each is a
    scoped follow-up best done with a fresh context budget.

The toolkit+fanout method (and the by-hand tractable tail) is fully
exhausted. Toolkits delivered total: overpass, threatintel, csv, linecolor,
probe, htmlparse, mlit_ksj, unified (+capture) + feedlib
get_json_h/post_json/get_text. Session arc:
15→91→122→132→186→193→203→241→273→281→285→293.

Wave 9: added `unified_capture(ctx, upstream_id, cJSON*out)` to
`lib/unified.{c,h}` (public capture-sink runner — runs an already-
registered upstream in-memory, allSettled parity) and hand-ported the
bespoke **unified-ais-ships** (custom mmsi/imo/name+coord fusion,
freshest-`last_position_update`-wins + fill-null + sources∪; validated:
captured 514 real marine-traffic features via the sink, vessel-finder
key-gated 0). Plus faithful 0-row ports of **dam-water-level** /
**drone-nofly** (tryMlit* always returns null → `if(!live) features=[]`;
SEED dropped per RULE 8; reachability fetch kept for host parity).
npa-important-wanted + npa-traffic-accidents were completed by concurrent
continuations (the former had a `*/`-in-regex-comment build-break fixed
here — RECURRING FOOTGUN: never leave `*/` inside a ported comment).

THE REMAINING 15 — each a distinct design problem, NOT a wave:
  * **separate P6 infra (4)** — `unified-flights` (passthrough to
    utils/planeAdsbPoller.js: needs an OpenSky+AeroDataBox in-memory
    poller thread), `unified-stations` + `unified-station-footprints`
    (`collectUnifiedStations/FootprintsRead` over stationClusterer /
    stationFootprintsStore precomputed sqlite), `cameras` (camera sweep
    sqlite). Build their backing pipeline first; not a collector port.
  * **websockets (2)** — bluesky-jetstream-jp, certstream-jp (need a wss
    client in feedlib; out of the fetch model).
  * **DB-read over intel_items (2)** — twitter-geo, niconico-ranking
    (db.prepare(stmtSelectGeocoded) re-emit; needs a C db query helper).
  * **heavy/loop hand-ports (7)** — flight-adsb (OpenSky + 4 adsb.lol
    quadrants + per-airport AeroDataBox loop merge), mlit-landprice
    (47-pref loop + MUNICIPALITY_CENTROIDS table), chan-5ch (per-board
    Shift_JIS subject.txt loop — csv_decode_sjis available), classifieds
    (multi-page Jmty HTML regex — htmlparse available), satellite-tracking
    (SGP4 TLE orbital propagation), wanted-persons (Unicode-class kanji
    regex + trig spread + non-deterministic uid), gdelt-events (GDELT
    zip→csv — needs an unzip). chan-5ch/classifieds are the most tractable
    next hand-ports (existing toolkits cover them).

Wave 8 ported the 5 factory unified-* via `lib/unified.h` + the
`unified_trains.c` gold ref: unified-trains (gold), unified-subways,
unified-buses, unified-airports, unified-port-infra (2-agent fan-out, all
OK, build-verified). One build-blocker fixed: a concurrent-continuation
agent wrote `npa_important_wanted.c` (a documented non-fit) with a `*/`
inside a regex comment — comment rewritten to unblock the shared build
(the THREATINTEL/RSS/OVERPASS_TILED/most-OTHER+CSV tails were absorbed by
those background continuations + the wave-7 fan-out).

THE REMAINING 19 ARE IRREDUCIBLE (no toolkit/fanout possible — each is an
individual hand-port or separate infra):
  * **4 bespoke P6 infra** (NOT createUnifiedCollector): `unified-ais-ships`
    (custom mmsi/imo/name+coord freshness fusion — hand-port; `lib/unified.h`
    exposes `unified_norm_name` for it), `unified-flights` (passthrough to
    utils/planeAdsbPoller.js — needs an OpenSky+AeroDataBox in-memory poller),
    `unified-stations` + `unified-station-footprints`
    (`collectUnifiedStations/FootprintsRead` — read the stationClusterer /
    stationFootprintsStore precomputed sqlite), `cameras` (reads the camera
    sweep sqlite — the actual sweep-ingest pipeline). These need their
    backing pipeline built, not a collector port.
  * **websockets**: bluesky-jetstream-jp, certstream-jp (wss streams).
  * **DB-read**: twitter-geo, niconico-ranking (db.prepare over intel_items).
  * **no live path**: dam-water-level, drone-nofly (seed-only; tryMlit* always
    returns null → faithful port emits 0 rows — could be 0-row stubs).
  * **heavy/loop**: flight-adsb (multi-source ICAO merge), mlit-landprice
    (47-pref loop + MUNICIPALITY_CENTROIDS table), chan-5ch (per-board SJIS
    loop), satellite-tracking (SGP4 TLE propagation), npa-traffic-accidents
    (~60MB Shift_JIS stream-agg), classifieds (multi-page Jmty HTML regex),
    wanted-persons (Unicode-class kanji regex), gdelt-events (GDELT zip→csv).
  Each needs individual care; the toolkit+fanout method is fully exhausted.

**P6 = the `unified-*` composition layer.** Delivered `lib/unified.{c,h}`:
faithful port of utils/unifiedCollectorTemplate.js (`createUnifiedCollector`)
+ collectors/_dedupe.js (`mergeFeatureCollections`/`dedupeByKeys`/`normName`/
`countBySource`). Mechanism: upstreams are ALREADY-REGISTERED source_defs;
`unified_collect` looks each up via `registry_get`, runs it against an
in-memory CAPTURE sink (reconstructs Feature from
it->geometry_geojson+properties_json — every upstream uses
geojson_emit_features), Promise.allSettled parity (rc<0 ⇒ that upstream
contributes 0), tag-kind, merge, filter, dedupeByKeys (FNV hashmap; first
nonempty keyfn else coord-grid `_c:lon,lat:normName`; merge fill-null +
sources[]), postProcess, emit via geojson_emit_features (no _meta, RULE 8).
Each unified source.c supplies keyfns/filter/postProcess as static C fns.
Gold ref `transport/sources/unified_trains.c` (validated: compiles/links/
registers; smoke shows it correctly runs upstream `mlit-n02-stations` via
the capture sink). normName/toFixed are pragmatic (no ICU NFKC; %.*f) —
only the id-less coord-grid fallback + countBySource depend on them, a
documented post-parity deviation (cf. linecolor.c).

REMAINING 25 — all hand-work / genuine non-fits:
  * **5 factory unified-\*** (subways/buses/airports/port-infra/
    station-footprints/stations) — thin wrappers over `lib/unified.h`, copy
    `unified_trains.c`; each needs its upstreams + kind + dedupeKeys +
    filter (isSubwayFeature etc.) + ensureLineColor read from its
    `server/src/collectors/unifiedX.js`. STILL PORTABLE via the gold ref.
  * **3 bespoke DB_READ**: `unified-ais-ships` (custom mmsi/imo/name+coord
    freshness fusion, does NOT use the factory — hand-port with
    unified_norm_name exposed for it), `unified-flights` (passthrough to
    utils/planeAdsbPoller.js in-memory OpenSky+AeroDataBox snapshot — needs
    a poller, separate), `cameras` (reads the camera sweep sqlite — the
    actual sweep-ingest dependency, separate infra).
  * **JSON_API 10 / HTML 3 / CSV 2 / OTHER 1** = irreducible: websockets
    (bluesky-jetstream-jp, certstream-jp), SGP4 (satellite-tracking),
    DB-read (twitter-geo, niconico-ranking), seed-only no-live-path
    (dam-water-level, drone-nofly), 47-pref loop+centroid (mlit-landprice),
    multi-source ICAO merge (flight-adsb), 60MB Shift_JIS stream
    (npa-traffic-accidents), per-board SJIS loop (chan-5ch), multi-page
    HTML (classifieds), Unicode-class regex (wanted-persons,
    npa-important-wanted), GDELT zip→csv (gdelt-events), pref-police
    multi-page (pref-police-crime). Each = individual hand-port; not a wave.

Wave 7 = 4 parallel agents over the 51 JSON_API+OVERPASS ids → +~40 OK
(simple fetch-JSON→`geojson_emit_features`; OVERPASS-primary-with-fallback
ported as primary-only per RULE 8; API-key collectors getenv-gated → 0 rows
when unset; `stadiums`/`water-infra` Overpass single, slice-cap dropped).
One post-build fix: `nra_radiation.c` missing `<stdlib.h>` (strtod) — added.
The remaining 57 are the irreducible tail: **DB_READ 10 = P6**
(unified-airports/ais-ships/buses/flights/port-infra/station-footprints/
stations/subways/trains + cameras — read the sweep/store sqlite, BLOCKED
until P6 ingest exists); JSON_API 11 + HTML_SCRAPE 7 + OTHER 7 = genuine
non-fits (per-item fetch loops e.g. jma-forecast-area/mlit-landprice 47-pref,
multi-source merge e.g. peeringdb/flight-adsb/bike-share-gbfs, websockets
bluesky-jetstream-jp, SGP4 satellite-tracking, DB-read twitter-geo/
niconico-ranking); THREATINTEL 7 + CSV 7 bespoke over existing toolkits;
RSS 6 wants a tiny multi-feed rss_atom wrapper; OVERPASS_TILED 2 = cell-towers
(OpenCellID multi-source) + 1. **The toolkit+fanout method is now exhausted
for P5** — residual is hand-work or P6-blocked.

Wave 6 added `lib/mlit_ksj.{c,h}` — faithful port of
server/src/utils/mlitNormalizer.js (KSJ_CONFIG per-dataset alias maps +
geometryToPoint [Point/midpoint/centroid/preserve] + normalizeKsjFeature with
exact JS key order `{[idField]:idValue,...norm,...extras,source}`). Primary
env→mirrors path only; OSM Overpass fallback + `_meta` envelope dropped per
RULE 8 / the documented JSON_API fallback-drop decision (0 rows when no MLIT
mirror responds = correctness-neutral, same contract as every other port).
Gold ref `transport/sources/mlit_n02_stations.c` (runs clean, emits 0 with
env unset + MLIT .geojson mirrors 404). +6: mlit-n02-stations, mlit-c02-ports,
mlit-n07-bus-routes, mlit-p02-airports, mlit-p11-bus-stops,
mlit-n05-rail-history (abolished_only=1).

HONEST TAIL ASSESSMENT — the toolkit+fanout method is now ~exhausted; the
residual 97 splits into:
  * **JSON_API 29 + OVERPASS 22** — biggest lever, NO new toolkit. Most
    OVERPASS are JSON-primary-with-Overpass-fallback → really JSON_API: port
    the primary fetch-JSON→`geojson_emit_features` path, drop the fallback
    (RULE 8). Same 4-agent parallel pattern as waves 2/4/5 still applies to
    the *simple-fetch* subset; the rest are the bespoke non-fits below.
  * 10 DB_READ (unified-*/cameras) — **= P6**. BLOCKED on the sweep ingest
    existing (transport runner / camera fan-out); NOT a port, separate infra.
  * ~irreducibly bespoke — per-collector hand-ports, NO shared toolkit:
    multi-source merges (PeeringDB net+ix+fac, RESAS+OSM, flight 3-source fuse),
    per-item fetch loops (per-domain/per-CVE/per-prefecture), websockets
    (certstream-jp, bluesky-jetstream), regex-engine HTML (bounded-gap/
    label-anchored capture, kanji classes), non-deterministic uid
    (real-estate Math.random jitter), huge Shift_JIS CSV stream-agg
    (npa-traffic-accidents ~60MB), multi-feed publisher-hash RSS. Many CANNOT
    be a faithful *simple* single-fetch port; each needs individual care and
    is not amenable to gold-template parallel fanout. Estimate: hand-work,
    not a wave.

--- earlier ---
**186 / ~308 collectors ported & registered** (build green; 186 source.c, all
compile/link/register). **114 worklist ids remain** (`/tmp/rem4.txt`):
JSON_API 29, OVERPASS 22, HTML_SCRAPE 18, DB_READ 10, OTHER 8,
THREATINTEL 7, OVERPASS_TILED 7, CSV 7, RSS 6. Wave 4 added `lib/probe.c`
(`probe_head` fetchHead + `probe_iso_now`) + `boj_stats.c` gold ref, and
ported 53 OTHER fetchHead portal-status stubs (one `<id>|<key>` intel row w/
reachable status — faithful, not 0-row). The remaining 114 are the bespoke /
toolkit-blocked tail: HTML_SCRAPE 18 needs `lib/html_scrape.c` (biggest
remaining lever); ~6 mlit-ksj (OVERPASS_TILED + mlit-n05 in OTHER) need
`lib/mlit_ksj.c`; DB_READ 10 stays blocked on sweep ingest; the rest
(JSON_API/OVERPASS-as-JSON/THREATINTEL/CSV/RSS-multifeed + 8 OTHER =
websocket/composition/multi-source-merge) are per-collector hand-ports with
no shared toolkit. Earlier snapshot below. Wave 3 added `lib/linecolor.c`
(computeLineColor) + `feed_post_json`, and ported all OVERPASS_WAYS (3) +
osm-transport-* (5) + the POST-body cluster (osv-dev, quake360-jp,
misskey-timeline). Remaining toolkit levers: **`lib/mlit_ksj.c`** (KSJ_CONFIG
field maps + geometryToPoint + try-mirror; unlocks mlit-c02/n02/n07/p11/n05 ≈
5-6, see `server/src/utils/mlitNormalizer.js`), **`lib/html_scrape.c`**
(cheerio subset → 18 HTML), **`lib/probe.c`** (fetchHead→1 status intel row →
the 62 OTHER portal-status stubs, faithful & uniform). Then the bespoke
JSON_API/THREATINTEL/CSV/RSS-multifeed tails by hand. DB_READ (10) stays
blocked on sweep ingest.

--- earlier snapshot (wave 2) ---
**122 / ~308 collectors ported & registered** (build green; 122 source.c).
Wave 1 = 91 (Overpass/ThreatIntel toolkits + refs). Wave 2 = +31 JSON_API
(bespoke fetch-JSON→FeatureCollection|intel; 32 of the 63 were NEEDS-MANUAL =
multi-source merge / per-item fetch loop / POST-with-JSON-body / websocket /
DB-read / CSV / HTML — correctly not force-fit). **178 worklist ids remain.**
New recurring lever: add `feed_post_json(http,url,body,headers,timeout)` to
`lib/feedlib.c` — many NEEDS-MANUAL (osv-dev, quake360-jp, misskey-timeline,
quake360, and the censys/threatfox/urlhaus hand-rolled POSTs) need POST+JSON
body; one helper unblocks a cluster. Toolkits live in `native/lib/`:
`overpass.{c,h}` (fetchOverpass/Tiled/Ways + `ov_tag`), `threatintel.{c,h}`
(createThreatIntelCollector factory), `csv.{c,h}` (parseCsv + Shift_JIS via
iconv), `feedlib` gained `feed_get_json_h`/`feed_get_text`. Gold reference
source.c: `government/sources/embassies.c` (OVERPASS single),
`health/sources/hospital_map.c` (OVERPASS_TILED),
`cyber/sources/abuseipdb_jp.c` (THREATINTEL), `news/sources/nhk_news_rss.c`
(RSS). Porting guide: `/tmp/P5_PORTING_GUIDE.md` (regenerate if lost).

**209 worklist ids remain** (`/tmp/p5_worklist.tsv`, `/tmp/remaining_ids.txt`).
By family, with the concrete unblock:

| bucket | n | unblock / next-wave recipe |
|---|---|---|
| **JSON_API** | 63 | No new toolkit. Bespoke per-collector: `feed_get_json[_h]` → map → build Features → `geojson_emit_features` (jma_earthquake.c pattern). **Also absorbs the ~30 NEEDS-MANUAL "OVERPASS/THREATINTEL" ports that are really a live JSON API primary with Overpass/seed only as fallback** (mlit-river, nra-radiation, soramame, resas-tourism/industry, tabelog-restaurants, ev-charging, maritime-ais, marine-traffic, vessel-finder, wifi-networks-wigle, …): port the primary API path, drop the fallback (rule 8). Fan out like this wave. |
| **OTHER** | 62 | Mostly `fetchHead`+`intelEnvelope` reachability-probe stubs that emit **0 data rows** — low value; a `lib/probe.c` (HEAD→single status intel row) ports them uniformly, or defer as cosmetic. A few are real (mlit-n05-rail-history KSJ, phishing-feeds-jp, wifi-networks/unified-highway composition) → JSON_API/manual. |
| **OVERPASS** | 22 | NEEDS-MANUAL from this wave: most are JSON_API-primary (→ JSON_API bucket); `stadiums`/`admin-boundaries` post-map `.slice()` cap (acceptable to port uncapped, note it); `airport-infra`/`bridge-tunnel-infra`/`police-crime`/`port-infra` have **no row in source_registry.gen.c** (decide id/metadata first). |
| **HTML_SCRAPE** | 18 | Needs `lib/html_scrape.c` — a cheerio-subset (tag/class/id selector + text/attr extract). Build that toolkit, then fan out like overpass. |
| **OVERPASS_TILED** | 11 | NEEDS-MANUAL: `createMlitKsjCollector` factory (mlit-c02-ports, mlit-n02-stations, mlit-n07-bus-routes, mlit-p02-airports, mlit-p11-bus-stops) and `createOsmTransportCollector` factory (osm-transport-buses/ports/subways/trains) — **port these 2 JS factories as `lib/` toolkits (same leverage as overpass/threatintel) → unlocks all 11**. `cell-towers` = OpenCellID-keyed multi-source. |
| **DB_READ** | 10 | `unified-*` + `cameras` read the sweep/store sqlite. Gated on the sweep ingest existing (P5/P6 transport runner + camera fan-out). Port after those land. |
| **THREATINTEL** | 7 | Multi-fetch loops / multi-endpoint merge / CSV (cloudflare-radar-jp, crtsh-historical, hudson-rock-jp, ioda-jp, shadowserver-jp, sslbl-jp, virustotal-jp). Bespoke; port by hand using threatintel.c + a loop. |
| **CSV** | 7 | `csv.c` toolkit ready. Bespoke per file (npa-missing-persons reiwa-kanji, gdelt zip→csv, grid Shift_JIS). Hand-port like JSON_API. |
| **RSS** | 6 | Multi-feed publisher-hash variants (jp-news-rss, yahoo-news-jp-rss, mofa-travel-advisory, sans-isc, my-jvn, gtfs-jp). Need a small multi-feed wrapper over rss_atom (per-feed tag + publisher property + intelHashKey uid). |
| **OVERPASS_WAYS** | 3 | overpass-rail-tracks / overpass-subway-tracks need `_lineColor.js` → `lib/linecolor.c`; osm-transport-station-boundaries uses `createOsmTransportCollector` (geometry:'way',Polygon). Port `_lineColor` + osm factory → unlocks. |

**Highest-leverage next steps** (mirror this session's toolkit-first method):
1. Port `createMlitKsjCollector` + `createOsmTransportCollector` factories →
   `lib/` (unlocks 11 TILED + 3 WAYS + several OTHER).
2. Build `lib/html_scrape.c` (unlocks 18 HTML).
3. JSON_API fan-out wave (63 + ~30 reclassified NEEDS-MANUAL) — biggest count,
   no toolkit needed, same 4-agent parallel pattern.
4. `lib/linecolor.c`, multi-feed RSS wrapper, `lib/probe.c` (small, each
   unlocks 3–62).
DB_READ (10) stays blocked until the sweep ingest exists.

## Ops note
Scheduler-alongside-serve is now wired (`--serve` starts a background
`scheduler_loop` thread; `JO_NO_SCHED=1` opts out; boot runs staggered 3s
apart). Follow-up (not blocking P5): `scheduler_loop` runs sources **serially**
on one thread, so a slow nationwide tiled-Overpass source (~90s) delays the
rest of that cycle. Node used guarded async runs. A small worker pool (or
per-source threads with a concurrency cap + the existing skip-if-running
guard) is the eventual fix; harmless for now since it never blocks serving.
