/* lib/geojson.h — GeoJSON Feature → intel, port of
 * collectorMirror.featureToMasterItem + featureUid + geometryCentroid +
 * pickText + inferRecordType. One toolkit for the whole FeatureCollection
 * family; each such collector becomes a tiny fetch+emit source.c.
 *
 * DEVIATION from JS geometryCentroid: the representative lat/lon written to
 * intel_items is the middle-segment midpoint for lines and the outer-ring
 * vertex mean for polygons, NOT the bbox centre, which for a concave shape
 * can land off the geometry entirely and is what the alert/geofence read path
 * tests. See the comment on centroid() in geojson.c. */
#ifndef JO_GEOJSON_H
#define JO_GEOJSON_H
#include "../source.h"
#include "../third_party/cJSON.h"

/* Emit one intel row per feature in `features` (a cJSON array). uid =
 * featureUid (NATIVE_ID_KEYS → feature.id → sha1(JSON.stringify{g,p})[:16]).
 * Returns #emitted. */
int geojson_emit_features(intel_sink *sink, const char *source_id,
                          cJSON *features);

/* Convenience: parse a FeatureCollection / {features:[...]} / array and emit. */
int geojson_emit_doc(intel_sink *sink, const char *source_id, cJSON *doc);

#endif
