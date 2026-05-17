/* core/transitapi.h — Node→C port of server/src/routes/transit.js.
 *
 * Faithfully reproduces every router.get/post in transit.js. One dispatcher:
 *
 *   char *transitapi(db_handle *db,
 *                    const char *path_after_api_transit,
 *                    const char *query);
 *
 * `path_after_api_transit` is the request path with the `/api/transit` mount
 * prefix already stripped (e.g. "/routes", "/station/<uid>/summary",
 * "/gtfs/operators", "/active-trips"). `query` is the raw URL query string
 * (may be NULL / ""; "?" not included). Returns a malloc'd JSON body the
 * caller must free(), or NULL if the subpath is not a transit route (caller
 * then emits 404/501).
 *
 * STATUS / ERROR CONTRACT
 * -----------------------
 * transit.js uses Express res.status(...).json(...): a route can answer 200
 * OR 400/404/500 with a JSON body. This dispatcher has no status out-param,
 * so for every *recognized* subpath it returns the JSON body the route would
 * emit — including its 400/404/500 error objects verbatim, e.g.
 *   {"error":"mode must be train|subway|bus"}
 *   {"error":"station not found"}
 * The caller serves these with HTTP 200 (body still byte-identical to JS).
 * NULL is reserved for an *unknown* subpath only. Documented deviation: the
 * HTTP status code of error responses is not preserved (the JSON body is).
 *
 * LIVE (served from real SQLite tables, byte-faithful to transit.js SQL):
 *   GET  /routes                         intel_items LineString rows
 *   GET  /gtfs/stop/:stopId/departures   gtfs_* (getDeparturesAt port)
 *   GET  /gtfs/operators                 gtfs_operators (listHydratedOperators)
 *   GET  /active-trips                   gtfs_* (getActiveTripsAt port)
 *   GET  /station/:stationUid/summary    intel_items + gtfs_* + station_clusters
 *                                        + odpt_station_timetable (both the
 *                                        legacy-uid and 40-hex cluster paths)
 *   GET  /vehicle/:tripId/info           gtfs_trips/stop_times/routes/rt
 *
 * GRACEFUL-EMPTY (the upstream-fetch subsystem these depend on is not yet
 * ported to C — see DEVIATIONS; transit.js itself already returns the same
 * shape when the feed is absent / hydrate fills nothing, so this is faithful
 * to its no-data path, not a fabrication):
 *   POST /gtfs/hydrate/:orgId            hydrateOperator() pulls a remote
 *                                        GTFS-JP zip. No network collector in
 *                                        C → returns the same success-shaped
 *                                        body with zeroed counts:
 *                                        {"ok":true,"orgId":"<id>","skipped":
 *                                        true,"reason":"hydrate_not_ported"}.
 *   GET  /station-boundaries             collector imports osmTransport-
 *                                        StationBoundaries (Overpass). Not
 *                                        ported → {"type":"FeatureCollection",
 *                                        "features":[]} (== an Overpass miss).
 *
 * DEVIATIONS (documented, no fabrication):
 *  1. Error HTTP status codes collapse to 200; JSON bodies are exact (above).
 *  2. /routes: getLinesByMode() in JS runs the stitch/Chaikin/RDP read-time
 *     smoothing pipeline (transportStore + transport_geom). That pipeline is
 *     intentionally NOT reimplemented (same stance as station_clusterer.h /
 *     sweepapi.c — raw stored LineString fragments are returned). Geometry
 *     differs slightly where smoothing moved/merged vertices; identity
 *     (line_uid/name/line_color), bbox clip, MAX_FEATURES stride cap and the
 *     _meta.truncated shape are all exact.
 *  3. Lazy-hydrate hooks (orgId GTFS-JP feed + ODPT StationTimetable /
 *     TrainInformation refresh fired from the summary routes) are no-ops:
 *     the C build has no upstream-fetch layer. transit.js already falls
 *     through to empty departures/odpt_departures when hydrate can't fill the
 *     tables in budget, so the summary routes still emit the exact same
 *     response shape from whatever rows are already present (typically the
 *     weekly-cron-populated set).
 *  4. NFKC/locale: none needed here (no name folding on this surface).
 *  5. Wall-clock: secondsSinceMidnight / HH:MM / YYYYMMDD use the process
 *     LOCAL timezone, exactly like the JS `new Date()` calls.
 */
#ifndef JO_TRANSITAPI_H
#define JO_TRANSITAPI_H

#include "db.h"

/* Dispatch the /api/transit subtree. Returns malloc'd JSON (caller frees),
 * or NULL when `path_after_api_transit` matches no transit route. */
char *transitapi(db_handle *db, const char *path_after_api_transit,
                 const char *query);

#endif
