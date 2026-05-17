/* core/station_clusterer.h — P6 sweep: Node→C port of
 * server/src/utils/stationClusterer.js (cross-mode station clusterer) +
 * the snap math from server/src/utils/transportSpatialSnap.js.
 *
 * Self-contained: stationNameFingerprint / levenshtein / normName (from
 * server/src/collectors/_dedupe.js) are inlined here as static fns; this
 * file does NOT depend on lib/unified.c or lib/dedupe (a concurrent session
 * owns lib/). Tables `station_clusters` + `station_line_dots` already exist
 * in native/core/schema.sql; the clusterer does a full DELETE+INSERT each
 * run.
 *
 * Constants ported verbatim from the JS:
 *   SPATIAL_RADIUS_M=150, LEVENSHTEIN_THRESHOLD=2, CELL_SIZE_DEG=0.005
 *   (clusterer) / 0.01 (snap), METERS_PER_DEG_LAT=111320,
 *   DEFAULT_RADIUS_M=300, MIN_RAIL_SEPARATION_M=3. LLM merges trusted at
 *   same=1 AND confidence>=0.7. cluster_uid = sha1(sorted member_uids
 *   joined '|') full 40-hex digest. Equirectangular distSqM /
 *   point-to-segment math kept bit-for-bit faithful to the formulas.
 *
 * NFKC is a pragmatic ASCII-ish approximation (no ICU): we apply
 * lowercase + the literal substring strips + katakana→hiragana fold; we do
 * NOT perform real Unicode NFKC compatibility decomposition (full-width →
 * half-width digits/latin, etc.). Parity with JS String.normalize('NFKC')
 * is abandoned — deterministic & post-parity acceptable (C is the sole
 * backend; cf. lib/unified.c / lib/linecolor.c same note). */
#ifndef JO_STATION_CLUSTERER_H
#define JO_STATION_CLUSTERER_H

#include "db.h"

/* runStationClusterer(): rebuild station_clusters + station_line_dots from
 * scratch (full DELETE+INSERT) out of intel_items rows whose source_id IN
 * ('unified-trains','unified-subways'). Returns the number of clusters
 * written, or -1 on a fatal DB error. (0 clusters when no input stations.)
 */
int station_clusterer_run(db_handle *db);

/* getAllLineDotFeatures(): a GeoJSON FeatureCollection of one Point per
 * (cluster, way) pair from station_line_dots, LEFT JOINed to
 * station_clusters for name/name_ja. Returns a heap string the caller must
 * free(); NULL only on allocation failure. Empty input → an empty FC. */
char *station_clusterer_linedots_fc_json(db_handle *db);

/* snapStationsToNearestLine(mode): build a segment index from `mode`'s
 * colored line geometries in intel_items (source_id 'unified-trains' /
 * 'unified-subways', LineString rows), snap every station of that mode to
 * the nearest track color, and UPDATE that station's intel_items.properties
 * line_color / line_colors[] in place. `mode` must be "train" or "subway".
 * Returns the number of stations whose properties changed, or -1 on a
 * fatal DB error.
 *
 * NOTE: the JS path runs getLinesByMode() through a chaikin/RDP/stitch
 * read-time smoothing pipeline (transportStore.getLinesByMode) before
 * snapping. That pipeline lives in a sibling port (core/transport_geom +
 * core/transport_store, owned elsewhere) and is intentionally NOT
 * reimplemented here — this snap reads the raw stored LineString geometry
 * directly. Snap result will differ slightly from JS where smoothing moved
 * vertices; documented approximation. */
int station_snap_stations(db_handle *db, const char *mode);

#endif
