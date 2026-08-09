# P6 Sweep-Ingest — status (accurate, 2026-05-17)

> **HISTORICAL SNAPSHOT.** "286 sources registered" is the state on
> **2026-05-17** and was already an undercount then (it did not expand
> registration macros). Current figure: `make -C native source-count` — 2,563
> on 2026-08-09. See [`../docs/collectors.md`](../docs/collectors.md).

Two sessions worked native/ in parallel. Canonical P5 collector-port handoff =
`native/P5_REMAINING.md` + memory (other session). This file = the sweep
(transport/camera producer + clusterer + read) status. Spec:
`/tmp/sweep_port_plan.md`. Build green, 286 sources registered.

## DONE & build-verified
**Transport ingest (other session):** `lib/unified.{c,h}` + 7 registered
`unified-*` sources (trains, subways, buses, airports, ais-ships, port-infra,
highway) — fuse upstream registered sources via a capture sink → dedupe →
emit to `intel_items` (source_id `unified-<mode>`). C scheduler runs them = the
runner.

**Sweep toolkits (this session, 3 agents, disjoint new files, all compile
-Wall -Wextra clean, integrated build green):**
- `core/station_footprints.{c,h}` — full port of stationFootprintsStore.js:
  `station_footprints_upsert_tx`, `station_footprints_link_clusters_tx`
  (bbox-contains cluster stamping), `station_footprints_fc_json`,
  `station_footprints_count`. Driver-tested.
- `core/station_clusterer.{c,h}` (~1750L) — full port of stationClusterer.js
  + transportSpatialSnap.js: `station_clusterer_run` (3-pass union-find:
  wikidata / spatial+fingerprint+levenshtein / LLM `llm_station_merges`;
  materialise mean-centroid + sha1(member_uids) cluster_uid; computeLineDots
  per cluster×{train,subway}), `station_snap_stations(mode)` (segment-index
  nearest-color snap → updates intel_items props), `station_clusterer_
  linedots_fc_json`. Parity-probed byte-identical (fingerprint/levenshtein/
  sha1/distSqM). NFKC = pragmatic ASCII-ish (documented; parity abandoned).
- `core/camera_store.{c,h}` — port of cameraStore.js (intel_items wrapper,
  uid `camera-discovery|<camera_uid>`, channel-union + non-null-wins merge,
  seen_count): `camera_upsert`, `camera_fc_json`, `camera_stats_json`.
- `collectors/infrastructure/sources/camera_discovery.c` — registered
  SRC_FEED (interval 3600). **LIVE (10):** osm_overpass (via lib/overpass),
  jma_volcano, mlit_river, expressway_cctv, broadcast_livecam,
  tourism_webcam, manual_ip_seed, webcamendirect_seed, earthcam (static
  arrays ported verbatim), shodan_api (env-gated).

## ALL 4 REMAINING ITEMS — DONE & build-verified (313 registered, green)
1. **Orchestration glue** ✓ — `collectors/transport/sources/
   transport_cluster_runner.c` (registered SRC_FEED, 3600 s — the existing
   scheduler runs it; NO scheduler.c edit → conflict-free). run() =
   `station_snap_stations("train")` → `("subway")` → `station_clusterer_run`
   → load osm-transport-station-boundaries Polygons → `station_footprints_
   upsert_tx` → `station_footprints_link_clusters_tx`. End-to-end smoke clean
   (`snap/clusters/footprints/linked` all execute, rc=0, no crash; 0s on an
   empty DB = correct). transportRunner §A.1.3 post-step order preserved.
2. **Read-side /api wiring** ✓ — `core/sweepapi.{c,h}` + a surgical
   `httpd.c` hook BEFORE the /api/data 501: `seg("/api/data/<id>")` →
   `sweepapi_data` serves unified-<mode> (intel_items lines+stations FC +
   _meta), cameras (`camera_fc_json`), unified-stations
   (`station_clusterer_linedots_fc_json`), unified-station-footprints
   (`station_footprints_fc_json`); non-sweep ids fall through to the 501.
   Auth-gated like all /api/* (401 w/o token, identical to /api/sources).
3. **14 camera channels** ✓ — all ported as registered `cam-*` SRC_FEED
   sources → `camera_upsert` into camera_store (insecam-scrape,
   skylinewebcams, webcamtaxi, geocam, worldcams, webcamera24, camstreamer,
   worldcam-eu, webcamendirect-list, camscape, tabi-cam, scs-com-ua,
   windy-api[keyed], youtube-live[keyed]). Compile/link/register clean.
   Documented faithful limitations (NOT faked): Cloudflare-WAF'd hosts → 0
   from a datacenter IP (same as Node from cloud); headless-Chromium render
   paths can't be expressed (server-rendered parts are faithful); per-detail
   /YouTube-upgrade concurrency collapsed to sequential (identical output);
   the LLM `geocodeFeatures` enrich pass belongs to the runner, not the
   source. keyed channels gate on getenv → 0 if unset.
4. **ship/flight upstreams** ✓ — marine-traffic / vessel-finder /
   maritime-ais were already ported by the concurrent session →
   **unified-ais-ships fully unblocked** (its capture ids match). NEW
   `plane_adsb.c` (`.id=plane-adsb`, OpenSky JP-bbox + adsb.lol 4-quadrant
   merge + classifyMilitary, SRC_FEED 60 s) — the forthcoming `unified-
   flights` C port (concurrent session's lib/unified domain) captures
   `plane-adsb`. AeroDataBox 2nd-cadence + in-mem cross-poll/WS dropped
   (a per-interval snapshot can't model them; flight-adsb already covers
   AeroDataBox) — documented in the file header.

## Residual (smaller, documented; not blocking)
- Read-time `stitchAndSmoothLines` (Chaikin×4 + RDP + degree-2 stitch) on
  unified-<mode> lines: sweepapi returns raw stored fragments (correct data;
  the smoothing is a cosmetic render transform — self-contained follow-up,
  algorithm in transportStore.js:62-135,475-618).
- `unified-flights` ✓ DONE — `collectors/transport/sources/unified_flights.c`
  (thin passthrough = unifiedFlights.js `getSnapshot()`; mirrors the
  concurrent session's bespoke `unified_ais_ships.c` shape using their
  `lib/unified.h` `unified_capture("plane-adsb")`, emits source_id
  `unified-flights`). Conflict-free new file; build-green, registered
  (SRC_FEED 60 s), `/api/data/unified-flights` wired via sweepapi. Chain
  complete: plane-adsb (OpenSky⊕adsb.lol fusion) → unified-flights → intel_
  items → /api/data.
- Camera WAF/Chromium/keyed channels yield 0 from this host until run from a
  residential IP / with keys — faithful, expected (matches Node-from-cloud).
