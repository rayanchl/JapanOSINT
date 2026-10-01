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

/* Fetch `url` and emit every feature — ACROSS PAGES.
 *
 * geojson_emit_doc() takes a document already in hand, so it can only see the
 * first page, and that is what the whole VGEO family was built on: one GET, one
 * emit. 643 of its 770 URLs ask the upstream for a SLICE — overwhelmingly
 * ArcGIS `query?...&resultRecordCount=500&f=geojson` — so those layers were
 * being read 500 features at a time, once, forever, with no sign in the output
 * that anything was missing. An ArcGIS response says so itself: it sets
 * `exceededTransferLimit: true` when it has held features back.
 *
 * Three next-page signals, in decreasing authority:
 *
 *   1. `exceededTransferLimit` — ArcGIS FeatureServer/MapServer telling us
 *      outright that it truncated. Advance `resultOffset` by the record count.
 *   2. a `links` entry with `rel: "next"` — OGC API Features, WFS3. Follow it
 *      verbatim; no arithmetic, no guessing.
 *   3. `numberMatched` / `totalFeatures` larger than what we hold, with a
 *      cursor the URL already declares (`startIndex`, `resultOffset`,
 *      `offset`). The remainder is the upstream's own arithmetic.
 *
 * Stops on the first of: no signal, a page with no features, a page identical
 * to the previous one (an ignored cursor), or the ceiling
 * ($JO_GEOJSON_PAGE_MAX, default 20) — which is disclosed as a
 * collector-truncation-notice rather than a log line.
 *
 * Returns features emitted (>= 0), or -1 if the FIRST fetch failed, so a dead
 * endpoint stays distinguishable from an honest empty (R3). */
int geojson_emit_paged(intel_sink *sink, const char *source_id,
                       http_client *http, const char *url, int timeout_ms);

/* One Point Feature, properties not yet attached:
 *   {"type":"Feature","geometry":{"type":"Point","coordinates":[lon,lat]}}
 * The caller adds "properties" (and anything else) afterwards.
 *
 * 240 collectors wrote these same nine cJSON calls by hand. Key insertion
 * order here deliberately matches those copies byte for byte, because
 * featureUid's hash fallback (geojson.c) fingerprints the PRINTED geometry —
 * reordering the keys would re-uid every feature that has no natural id. */
cJSON *gj_point_feature(double lon, double lat);

#endif
