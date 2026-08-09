/* core/entityapi.h — P7 Wave 2: /api/entities/* (read paths of
 * entityStore.js). Pure SQLite over the shared entity graph. Single C backend
 * (faithful behaviour, not Node byte-parity).
 *
 * TENANT SCOPE. Every read below takes the caller's tenant id and filters
 * `entities` on "(tenant_id IS NULL OR tenant_id = :tenant)" — the shared
 * pre-tenancy graph plus this tenant's private nodes, which is exportapi.c's
 * predicate for the same table. This subtree previously had NO tenant
 * predicate at all and httpd.c reached it without resolving a tenant; the
 * routes are unchanged, the visibility rule is not.
 *
 * entity_mentions / entity_relationships carry no tenant column. Each entry
 * point resolves the entity first, so an invisible node can neither be read
 * nor walked to. */
#ifndef JO_ENTITYAPI_H
#define JO_ENTITYAPI_H
#include "db.h"

/* GET /api/entities/stats — corpus/graph counters. malloc'd, never NULL.
 * `entities` and `intel_items` are tenant-scoped; the extraction-pipeline and
 * mention/relationship counters are corpus-wide because those tables have no
 * tenant column to filter on. */
char *entityapi_stats(db_handle *db, const char *tenant);

/* GET /api/entities/search?q&type&limit — FTS (MeCab-segmented).
 * Empty q → {"results":[]}. NULL only on a SQL/MATCH failure (caller 500). */
char *entityapi_search(db_handle *db, const char *q, const char *type, int limit,
                       const char *tenant);

/* GET /api/entities/:type/:id — profile. NULL if missing, type mismatch, or
 * not visible to `tenant` (caller → 404 {"error":"not_found"}). */
char *entityapi_get(db_handle *db, const char *type, const char *id,
                    const char *tenant);

/* GET /api/entities/:type/:id/graph?depth&rel_types&exclude_hubs&max_nodes
 * — ego-network (BFS, fan-out 25/node, depth 1..3). NULL → 404.
 *
 * rel_types     comma-separated allowlist ("asserted,pivot_discovered");
 *               NULL/empty = every edge type. Analyst-asserted edges and
 *               statistical co-mention are different claims; the canvas needs
 *               to be able to ask for one without the other.
 * exclude_hubs  degree ceiling; a neighbour above it is still rendered but is
 *               never expanded. 0 = no guard. Without this a 2-hop network
 *               around a prefecture returns most of the graph.
 * max_nodes     hard node cap (default 300, ceiling 2000).
 *
 * The response carries a `meta` block (node_count, max_nodes, hubs_collapsed,
 * nodes_over_cap, truncated) so the client can say "showing 300 of N" instead
 * of silently presenting a partial graph as if it were complete. */
char *entityapi_graph(db_handle *db, const char *type, const char *id, int depth,
                      const char *rel_types, int exclude_hubs, int max_nodes,
                      const char *tenant);

/* GET /api/entities/:type/:id/mentions?limit&offset — NULL → 404. */
char *entityapi_mentions(db_handle *db, const char *type, const char *id,
                         int limit, int offset, const char *tenant);

/* GET /api/entities/:type/:id/breaches?limit&offset — the breaches this
 * entity appears in (roadmap item 23). Joins entity_mentions rows written by
 * breach_index.c (extractor='breach-ingest', source_id == breach slug) to
 * breach_meta for the catalog fields. Returns {data:[...],exposure:{...}};
 * NULL if the entity is missing or type mismatches (caller → 404).
 *
 * The ingest side already existed — es_upsert_entity dedups on
 * (type, norm_key), so a breach email and an intel-mentioned email are the
 * same node. This is purely the reverse read that was never surfaced.
 * Secrets are NOT reachable here: only the breach catalog metadata and the
 * synthetic item_uid are returned; plaintext stays behind the operator
 * reveal path in breach_adapter.h. */
char *entityapi_breaches(db_handle *db, const char *type, const char *id,
                         int limit, int offset, const char *tenant);

/* GET /api/intel/items/:uid/entities — entities mentioned in an item, joining
 * entity_mentions → entities on item_uid. Works for both breach records
 * ("breach:..." uids) and normal intel items. {data:[{entity_id,type,value,
 * label}]}. Never NULL (empty array when the item has no mentions). Caller frees.
 *
 * Deliberately NOT tenant-parameterised: its callers (httpd.c's item route,
 * reportapi.c's case-report renderer) have already authorised the ITEM, and
 * the entities returned are exactly the ones that item mentions. Adding a
 * predicate here would filter a list that is already bounded by an
 * authorisation the caller performed. */
char *entityapi_item_entities(db_handle *db, const char *uid);

#endif
