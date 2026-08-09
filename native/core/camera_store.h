/* core/camera_store.h — faithful port of server/src/utils/cameraStore.js.
 *
 * Like the JS, there is NO typed `cameras` table (retired in the Phase B
 * cutover). This is a thin wrapper over the polymorphic `intel_items`
 * master, keyed `camera-discovery|<camera_uid>`, record_type='camera'.
 *
 * Writes go through the existing intel_sink chokepoint (core/intel.c →
 * upsertItems + FTS/MeCab + entity/alert hooks) exactly as the JS
 * upsertCamera() routes through intelStore.upsertItemSync — the camera
 * merge semantics (discovery_channels[] union, existing-non-null-wins,
 * seen_count++, first_seen_at preservation) are computed here against the
 * prior intel_items row's properties JSON, then handed to the sink.
 *
 * Reads (camera_fc_json) are direct sqlite3 SELECTs
 * over intel_items mirroring selectGeoFeatures({sourceId}).
 * Returned JSON strings are heap-allocated; the caller frees() them. */
#ifndef JO_CAMERA_STORE_H
#define JO_CAMERA_STORE_H

#include "db.h"
#include "../source.h"
#include "../third_party/cJSON.h"

/* upsertCamera(feature, channel): merge a single GeoJSON camera Feature into
 * intel_items via `sink`. Returns 1 if a NEW row, 0 if updated, <0 on
 * error/skip (missing camera_uid or non-finite coords — mirrors the JS
 * `return null`). `feature` is borrowed (not freed/mutated). */
int camera_upsert(db_handle *db, intel_sink *sink, cJSON *feature,
                  const char *channel);

/* getAllCameras(): selectGeoFeatures({sourceId:'camera-discovery'}) wrapped
 * with the JS `_meta` block → FeatureCollection JSON. Caller frees. */
char *camera_fc_json(db_handle *db);

/* getDiscoveryFeed({limit,cursor,channel}) — faithful port of
 * cameraStore.js: keyset-paged historical camera-discovery events,
 * newest-first by COALESCE(last_seen_at,fetched_at). Returns malloc'd
 * {"events":[{ts,kind:"historical",channel,camera:Feature,run_id:null}...],
 * "cursor":<base64 "ts|uid"|null>} (== Node {events,cursor}). limit clamps
 * 1..5000 (default 500). Caller frees; NULL only on prepare failure. */
char *camera_discovery_feed(db_handle *db, int limit,
                            const char *cursor, const char *channel);

#endif
