/* core/isochrone.h — GET /api/isochrone: GTFS travel-time reachability
 * (roadmap item 32, the "standout" of the spatial-tools set).
 *
 * WHAT IT IS. Given an origin, a departure moment and a time budget, answer
 * "where can I actually get to?" using the real timetable: walk to nearby
 * stops, ride services that genuinely run on that service day, transfer on
 * foot, and walk off at the far end. The answer is a GeoJSON FeatureCollection
 * with one Polygon/MultiPolygon per travel-time band.
 *
 * WHY IT IS HONEST. Every second of the result traces to a row that exists:
 *
 *   - Ride edges come from gtfs_stop_times joined to gtfs_trips and
 *     gtfs_calendar, so a trip is only ridden if its service_id is marked
 *     running on the requested weekday AND the date falls inside
 *     [start_date, end_date]. A trip whose calendar row is missing is NOT
 *     ridden — ~3.6% of trips in the live corpus have no calendar row, and
 *     counting them would be asserting a service we cannot prove.
 *   - Walk edges are haversine distance over gtfs_stops coordinates at a
 *     declared speed. Nothing is interpolated from road geometry we do not
 *     have.
 *   - The polygon is a marching-squares contour of the field
 *         reach(cell) = min over reached stops of ( t_stop + walk(cell,stop) )
 *     sampled on a regular grid. It is NOT a convex hull: a hull would claim
 *     reachability for the gaps between routes, which is exactly the lie this
 *     endpoint exists to avoid. Where nothing is reachable, no polygon is
 *     emitted — an empty FeatureCollection with a `meta.note` is the correct
 *     answer, not a plausible-looking blob.
 *
 * TIME ZONE. GTFS times are local to the agency; every feed in this corpus is
 * Japanese, so `depart_at` is interpreted as JST wall-clock. A trailing 'Z'
 * (or an explicit +HH:MM offset) is converted to JST first. Times past
 * midnight are encoded by GTFS as departure_sec > 86400 on the PREVIOUS
 * service day; this implementation queries one service day only, so a
 * departure in the small hours under-reports late-night service. Documented
 * rather than papered over.
 *
 * COST AND CACHING. A 30-minute isochrone from a dense origin settles ~1200
 * stops over ~1500 timetable queries (measured against the live 2.9M-row
 * gtfs_stop_times). That is far too expensive for the mongoose event loop, so
 * httpd.c runs it on a detached thread with its own DB connection and answers
 * through MG_EV_WAKEUP. Results are cached in `collector_cache` keyed on
 * (origin rounded to ~11 m, budget, walk cap, band set, service day, departure
 * quantised to 15 min) — a given origin/time-bucket is stable until the feed
 * is refreshed.
 *
 * QUERY PARAMETERS (all optional except lat/lon)
 *   lat, lon        WGS84 origin. Required.
 *   max_min         time budget in minutes. Default 30, clamped 5..90.
 *   depart_at       ISO-8601. Accepts "YYYY-MM-DDTHH:MM[:SS]" (JST),
 *                   "...Z" / "...+09:00" (converted), or absent = now (JST).
 *   bands           comma-separated minute thresholds, e.g. "10,20,30".
 *                   Default = three even bands over max_min. Max 6, each
 *                   clamped to (0, max_min].
 *   max_walk_m      access/transfer walking cap in metres. Default 800,
 *                   clamped 100..2000. The final egress leg is allowed the
 *                   whole remaining budget (capped at 5 km) because walking
 *                   off the last stop is part of the journey, not a transfer.
 *   walk_kmh        walking speed. Default 4.8, clamped 2.0..7.0.
 *   max_transfers   ride legs beyond the first. Default 2, clamped 0..4.
 *
 * RESPONSE — a GeoJSON FeatureCollection with a foreign `meta` member, so it
 * can be handed straight to a map layer the way /api/layers/:id/geojson is:
 *
 *   { "type": "FeatureCollection",
 *     "features": [ { "type":"Feature",
 *                     "geometry": {"type":"MultiPolygon", ...},
 *                     "properties": {"band_min":30,"stops_within":1197} }, ... ],
 *     "meta": { "origin":{"lat":..,"lon":..}, "depart_at":"...+09:00",
 *               "service_date":"20260810", "weekday":"mon",
 *               "params":{...}, "stops_reached":1197, "rounds":3,
 *               "timetable_queries":1495, "truncated":false,
 *               "grid":{"cell_m":150,"nx":..,"ny":..},
 *               "elapsed_ms":..., "cached":false } }
 *
 * Bands are emitted LARGEST FIRST so a client painting them in order gets the
 * wide band underneath and the tight band on top without having to sort.
 */
#ifndef JO_ISOCHRONE_H
#define JO_ISOCHRONE_H
#include "db.h"

/* Runs the whole computation. `qs` is the raw (undecoded) query string.
 * Returns a malloc'd JSON body and sets *status; never returns NULL except on
 * allocation failure. Blocking and expensive — call it off the event loop. */
char *isochrone_run(db_handle *db, const char *qs, int *status);

#endif
