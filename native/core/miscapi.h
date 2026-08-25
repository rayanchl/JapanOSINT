/* core/miscapi.h — Wave-1 P7 routes that are self-contained reads off the
 * shared japanmap.db (no tenant/pipeline/collector-framework dependency).
 * Single C backend; faithful behaviour, not Node byte-parity. */
#ifndef JO_MISCAPI_H
#define JO_MISCAPI_H
#include "db.h"

/* GET /api/sources/:id — malloc'd typed row object, NULL → 404. */
char *miscapi_source_by_id(db_handle *db, const char *id);

/* PUT /api/sources/:id/schedule — body {"mode":"map_cron"|"search_only"}.
 * Returns the updated source row JSON; NULL if `id` unknown (→404); the
 * sentinel "\1bad" if the mode is invalid (caller → 400); the sentinel "\1err"
 * if the UPDATE itself failed (caller → 500). The last one exists because
 * sqlite3_changes() is per-connection and survives a failed step, so a busy
 * write used to be reported as a successful one. */
char *miscapi_set_schedule(db_handle *db, const char *id, const char *body);

/* GET /api/sources/:id/logs?limit&offset — newest first.
 * NULL when the source id is unknown (caller → 404).
 *
 * SHAPE CHANGED, deliberately. This returned a bare JSON array capped at 500
 * rows: with 800 rows stored it served 500 and the body said nothing about the
 * other 300, and a bare array has no key in which to say it. It now returns
 *
 *   {"data":[…],"page":{"limit":N,"offset":K,"total":M},
 *    "meta":{"fetched_at":"…","filters":{"source_id":"…"}}}
 *
 * — the envelope the rest of this API uses, with `total` a measured COUNT(*).
 * `offset` is new so the rows past the cap are reachable and not merely
 * counted. The reasoning, and the checks that no client or contract fixture
 * depended on the old shape, are recorded above the implementation. */
char *miscapi_source_logs(db_handle *db, const char *id, int limit, int offset);

/* GET /api/layers — the layer TAXONOMY (v2). A bare JSON array; each element
 * carries id/name/category, data_type + modality (JSON null = undeclared,
 * never inferred), kind ("curated" from core/layers.def | "declared" from a
 * source's own .layer | "generated" catch-all per record_type), its member
 * sources, a measured records_geocoded (null if the count query failed), and
 * the v1 temporal disposition. Every geocoded intel_items row is credited to
 * exactly one listed layer — the catch-all rows are what make that total.
 * `db` may be NULL (counts and generated layers are then null/absent).
 * Contract fixture: tests/contract/_api_layers_v2.json. */
char *miscapi_list_layers(db_handle *db);

/* GET /api/layers/:layerId/geojson is served by dataapi_layer_fc (dataapi.h):
 * the real fused FeatureCollection, bounded with in-band truncation meta. */

/* GET /api/follow/recent — collector-tap history. The C scheduler keeps no
 * cross-process ring buffer, so there is no backing store: returns a
 * {"error":"not_implemented",...} body the caller sends with HTTP 501. */
char *miscapi_follow_recent(int limit);

#endif
