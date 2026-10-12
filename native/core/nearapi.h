/* core/nearapi.h — "everything within X of here" (roadmap item 32, the
 * proximity half).
 *
 *   GET /api/intel/items?near=<lat>,<lon>&radius_m=<m>[&source&record_type
 *                        &since&until&q&qAlt&limit&lang_view]
 *
 * httpd.c dispatches here only when `near` is present; every other
 * /api/intel/items request goes to intelapi_list_items() exactly as before, so
 * this adds a mode rather than changing one.
 *
 * TEXT FILTER (inline, and the fallback of the semantic mode below). `q`
 * (+ `qAlt`) is the feed's own MATCH expression
 * (intelapi_fts_match), applied to the bbox candidates before the scan
 * ceiling, and disclosed the same way (meta.q_applied, meta.notes). It used to
 * be ignored, so a text filter returned every row in the radius.
 *
 * TRANSLATION. `lang_view` is applied by httpd.c to this envelope with the
 * same call the feed uses (translate_shape_items); it used to return before
 * that step, so near rows never carried a translation.
 *
 * SHAPE. bbox prefilter on idx_intel_items_geom — a partial index over
 * (lat, lon) WHERE lat IS NOT NULL — then an exact haversine refine, ordered
 * by distance ascending. That is the same two-stage test alert_eval.c's
 * geofence evaluation performs (`circle` predicates: cheap bbox from the cap's
 * widest longitudinal span, then haversine), and the same reason applies here:
 * the index cannot express a great-circle distance, so the bbox is a superset
 * that the refine trims. The two share the constant and the formula but not
 * the code, because alert_eval evaluates ONE candidate point against a shape
 * while this walks an index — merging them would make the hot ingest path
 * carry a cursor.
 *
 * ROW CONTRACT. Rows are rendered by intelapi_item_by_uid(), not by a second
 * row builder in this file. That is deliberate and it is the whole reason this
 * module is only a query: the codebase already has 22 contracts defined in two
 * or more places and nine of them disagree, so a proximity search returning
 * subtly different item JSON from every other item endpoint would be a new
 * instance of the exact defect the audit named. The cost is one primary-key
 * lookup per returned row, bounded by `limit` (max 200).
 *
 * The only addition to each row is `distance_m` (metres from the query point,
 * rounded to whole metres — sub-metre precision would be fiction given the
 * geocoding provenance of most of these rows).
 *
 * TENANCY. Scoped with "tenant_id IN (?, 'legacy')": the caller's own rows
 * plus the shared corpus — the predicate the plain /api/intel/items path has
 * carried since a3b26ee (intel_items.tenant_id is NOT NULL, so the
 * "IS NULL OR" spelling used for `entities` would match nothing here).
 *
 * PAGING. Ordered by distance, so the base64url keyset cursor used by
 * /api/intel/items (which encodes published_at + uid) cannot express a page
 * boundary here. `page.next_cursor` is therefore always null and `page.total`
 * carries the true count of matches inside the radius — the client can widen
 * the radius or raise the limit, but it is never told a truncated list is
 * complete.
 */
#ifndef JO_NEARAPI_H
#define JO_NEARAPI_H
#include "db.h"

/* `near` is the raw "lat,lon" value; `qs` is the full query string (for the
 * optional filters). Returns malloc'd JSON and sets *status. */
char *nearapi_items(db_handle *db, const char *tenant, const char *near,
                    const char *qs, int *status);

/* SEMANTIC q (S6). The same query, with `q` RANKING the radius instead of
 * filtering it: the radius prefilter and refine run as above (without the
 * FTS MATCH), then the nearest NEAR_SEM_MAX in-radius rows are ordered by the
 * cosine of their stored vector (intel_vec_v2) with q's embedding; rows with
 * no vector follow by distance with `similarity: null`. meta.q_mode is
 * "semantic", meta.semantic counts ranked / unembedded / beyond_cap and
 * carries coverage. When q cannot be embedded (no JO_EMBED_URL, no index, a
 * refused or swapped model) this IS nearapi_items() — the FTS filter — with
 * meta.q_mode "fts" and meta.semantic_unavailable giving the reason. Embeds:
 * a semsearch_route_fn that httpd.c runs off the event loop when q is set
 * (`near` is the route's id). Applies ?lang_view= itself. */
char *nearapi_items_route(db_handle *db, const char *tenant, const char *near,
                          const char *qs, int is_operator, int *status);

#endif
