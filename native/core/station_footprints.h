/* core/station_footprints.h — faithful port of
 * server/src/utils/stationFootprintsStore.js.
 *
 * Persistent store for nationwide OSM station-building polygons. Backs the
 * `unified-station-footprints` map layer (bbox-filtered Polygon FC) and the
 * cluster->footprint linker run by the transport runner (stamp cluster_uid
 * onto the footprint whose bbox contains a cluster centroid).
 *
 * Table `station_footprints` already exists in core/schema.sql. All JSON is
 * built with cJSON and printed unformatted (== JSON.stringify); the read FC
 * mirrors rowToFeature() field-for-field, including the empty-FC-when-empty
 * behavior and the geometry-null row filter. */
#ifndef JO_STATION_FOOTPRINTS_H
#define JO_STATION_FOOTPRINTS_H

#include "db.h"
#include "../third_party/cJSON.h"

/* upsertFootprintsTx(features[]) — one SQLite transaction (BEGIN/COMMIT;
 * ROLLBACK on failure). `features` must be a cJSON array of GeoJSON Features
 * (footprint_id from feature.properties.footprint_id; bbox computed from the
 * Polygon outer ring; ON CONFLICT(footprint_id) preserves first_seen_at and
 * refreshes name/name_ja/geometry/bbox/source + last_seen_at). Features with
 * no footprint_id or no usable bbox are skipped (== JS returns null).
 * Returns the count of rows upserted (new + updated), or -1 on a DB error. */
int station_footprints_upsert_tx(db_handle *db, cJSON *features);

/* linkClustersToFootprintsTx(clusters[]) — one SQLite transaction. First
 * clears every cluster_uid (UPDATE … SET cluster_uid=NULL), then for each
 * {cluster_uid,lat,lon} stamps cluster_uid onto every footprint whose bbox
 * contains the point (bbox_min_lon<=lon<=bbox_max_lon AND
 * bbox_min_lat<=lat<=bbox_max_lat). `clusters` is a cJSON array of objects.
 * Returns the number of link writes performed, or -1 on a DB error. */
int station_footprints_link_clusters_tx(db_handle *db, cJSON *clusters);

/* getAllFootprints() wrapped as a FeatureCollection. Returns a malloc'd
 * `{"type":"FeatureCollection","features":[...]}` string (caller frees).
 * Each feature mirrors rowToFeature: geometry = JSON.parse(geometry) (rows
 * whose geometry fails to parse are dropped, == .filter(f=>f.geometry)),
 * properties = {footprint_id,cluster_uid,name,name_ja,mode_set} with
 * mode_set = JSON.parse(c.mode_set) || []. LEFT JOIN station_clusters for
 * mode_set. Empty table -> features:[]. NULL only on a DB/alloc error. */
char *station_footprints_fc_json(db_handle *db);

/* footprintCount() — SELECT COUNT(*) FROM station_footprints. -1 on error. */
int station_footprints_count(db_handle *db);

#endif
