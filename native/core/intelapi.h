/* core/intelapi.h — port of intelStore.rowToItem + buildProvenance for the
 * /api/intel routes. JSON built with cJSON in the EXACT Node key order and
 * printed unformatted (== JSON.stringify; proven byte-equal in P5). */
#ifndef JO_INTELAPI_H
#define JO_INTELAPI_H
#include "db.h"

/* GET /api/intel/items/:uid body. Returns malloc'd `{"data":{...}}` (200) or
 * NULL if the uid doesn't exist (caller → 404 {"error":"not_found"}). */
char *intelapi_item_by_uid(db_handle *db, const char *uid);

/* GET /api/intel/items filter set — faithful port of intelStore.listItems({})
 * args. All optional (NULL = not applied); limit<=0 → 50 (clamped 1..200).
 * `cursor` = base64url JSON {"p":pub_or_fetched,"u":uid} keyset token. */
typedef struct {
  const char *source;          /* intel_items.source_id = ?               */
  const char *q;               /* FTS over intel_items_fts (fts_query_expr)*/
  const char *q_alt;           /* second query OR'd with q — the client's
                                * translated counterpart of `q` (?qAlt=).
                                * Ignored unless `q` is also set.          */
  const char *lang;            /* intel_items.language = ?                 */
  const char *since;           /* COALESCE(published_at,fetched_at) >= ?   */
  const char *until;           /* COALESCE(published_at,fetched_at) <= ?   */
  const char *tag;             /* json_each(tags).value = ?                */
  const char *record_type;     /* intel_items.record_type = ?             */
  const char *sub_source_id;   /* intel_items.sub_source_id = ?           */
  const char *has_geom;        /* "yes" → lat NOT NULL; "no" → lat NULL    */
  const char *cursor;          /* base64url {"p":..,"u":..} keyset page    */
  int         limit;
} intel_items_query;

/* GET /api/intel/items — malloc'd envelope {data,page,meta}. A query with
 * only limit set (all filters NULL) is byte-identical to the old behaviour. */
char *intelapi_list_items(db_handle *db, const intel_items_query *q);

/* GET /api/sources — malloc'd JSON array of all `sources` rows. */
char *api_sources_list(db_handle *db);

/* GET /api/intel/sources — malloc'd envelope {data,meta}: registry joined
 * with per-source intel_items aggregates, ttl + is_intel, freshness-sorted.
 *
 * Each row also carries `parent_id`: non-null only on the camera discovery
 * channels (registry layer "cameras"), whose item counts are rolled up into
 * the `camera-discovery` row they belong to. See the rollup comment in
 * intelapi.c — without it the parent reports 0 items next to a dozen sibling
 * rows that are in fact its own channels. */
char *intelapi_intel_sources(db_handle *db);

/* intelCatalog.js INTEL_SOURCE_SET membership — the source ids that emit
 * kind:'intel'. 0 for NULL.
 *
 * Defined ONCE, in intelapi.c. statusapi.c used to carry a byte-identical
 * 34-entry copy appended to its STRIP_LAYER_IDS array (Node imported the same
 * set into both modules, and the port copied it twice). Two copies meant two
 * places to update, and both had already drifted the same way: three of the
 * ids — boj-stats, jcg-navarea, nict-atlas — name collectors that were deleted
 * in the 66-source removal sweep and no longer exist in either registry. */
int intelapi_is_intel_id(const char *id);

#endif
