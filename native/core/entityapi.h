/* core/entityapi.h — P7 Wave 2: /api/entities/* (read paths of
 * entityStore.js). Pure SQLite over the shared entity graph; no tenant /
 * pipeline / collector-framework dependency. Single C backend (faithful
 * behaviour, not Node byte-parity). */
#ifndef JO_ENTITYAPI_H
#define JO_ENTITYAPI_H
#include "db.h"

/* GET /api/entities/stats — corpus/graph counters. malloc'd, never NULL. */
char *entityapi_stats(db_handle *db);

/* GET /api/entities/search?q&type&limit — FTS (MeCab-segmented).
 * Empty q → {"results":[]}. NULL only on a SQL/MATCH failure (caller 500). */
char *entityapi_search(db_handle *db, const char *q, const char *type, int limit);

/* GET /api/entities/:type/:id — profile. NULL if missing or type mismatch
 * (caller → 404 {"error":"not_found"}). */
char *entityapi_get(db_handle *db, const char *type, const char *id);

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
                      const char *rel_types, int exclude_hubs, int max_nodes);

/* GET /api/entities/:type/:id/mentions?limit&offset — NULL → 404. */
char *entityapi_mentions(db_handle *db, const char *type, const char *id,
                         int limit, int offset);

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
                         int limit, int offset);

/* GET /api/intel/items/:uid/entities — entities mentioned in an item, joining
 * entity_mentions → entities on item_uid. Works for both breach records
 * ("breach:..." uids) and normal intel items. {data:[{entity_id,type,value,
 * label}]}. Never NULL (empty array when the item has no mentions). Caller frees. */
char *entityapi_item_entities(db_handle *db, const char *uid);

#endif
