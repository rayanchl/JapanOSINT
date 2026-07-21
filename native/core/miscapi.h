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
 * sentinel "\1bad" if the mode is invalid (caller → 400). */
char *miscapi_set_schedule(db_handle *db, const char *id, const char *body);

/* GET /api/sources/:id/logs — malloc'd JSON array (newest first, capped).
 * NULL when the source id is unknown (caller → 404). */
char *miscapi_source_logs(db_handle *db, const char *id, int limit);

/* GET /api/layers — registry layers (grouped by source.layer), STRIP-filtered,
 * each carrying its contributing sources + time-slider disposition. Sources
 * with no layer (e.g. osint-search) are skipped — Node crashed on them
 * (null.replace). Always non-NULL; malloc'd JSON array. */
char *miscapi_list_layers(void);

/* GET /api/layers/:layerId/geojson — empty FC + _meta.sources for the
 * layer. NULL when no registry source maps to that layer (caller → 404). */
char *miscapi_layer_geojson(const char *layer_id);

/* GET /api/follow/recent — collector-tap history. The C scheduler keeps no
 * cross-process ring buffer yet, so this is an honest empty envelope. */
char *miscapi_follow_recent(int limit);

#endif
