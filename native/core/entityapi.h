/* core/entityapi.h — P7 Wave 2: /api/entities/... (read paths of
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
 * nor walked to.
 *
 * BREACH SCOPE. Tenant scoping alone was not enough: breach_index.c writes the
 * cleartext breached identifier into `entities` (canonical) and
 * `entity_mentions` (surface + the breach in source_id), and those rows used to
 * be tenant_id NULL — i.e. inside the "shared graph" disjunct above. Any
 * authenticated viewer of any tenant could therefore read the breach corpus out
 * of /search, /mentions and /breaches, bypassing httpd.c's breach_gate()
 * entirely. Those rows are now stamped ES_BREACH_TENANT at ingest (see
 * core/entitystore.h) and no longer satisfy the disjunct.
 *
 * Every function below has a `_scoped` twin taking `int is_operator`
 * (opgate_check(&usr) == 0). The un-suffixed name is the SAME function with
 * is_operator = 0 — kept as a fail-closed default for any caller that forgets
 * the flag. core/httpd.c evaluates opgate_check() once per entity request and
 * passes it to the _scoped forms; see the block comment in entityapi.c. */
#ifndef JO_ENTITYAPI_H
#define JO_ENTITYAPI_H
#include "db.h"

/* GET /api/entities/stats — corpus/graph counters. malloc'd, never NULL.
 * `entities` and `intel_items` are tenant-scoped; the extraction-pipeline and
 * mention/relationship counters are corpus-wide because those tables have no
 * tenant column to filter on. */
char *entityapi_stats(db_handle *db, const char *tenant);
/* `entities` and `mentions` exclude the breach scope unless is_operator. */
char *entityapi_stats_scoped(db_handle *db, const char *tenant, int is_operator);

/* GET /api/entities/search?q&type&limit — FTS (MeCab-segmented).
 * Empty q → {"results":[]}. NULL only on a SQL/MATCH failure (caller 500). */
char *entityapi_search(db_handle *db, const char *q, const char *type, int limit,
                       const char *tenant);
/* Breach-scoped entities are absent from entities_fts, so no MATCH can return
 * them for anyone. An operator additionally gets an EXACT identifier probe over
 * idx_entities_normkey(type, norm_key) — the breach pivot preserved without
 * putting the breach corpus into the search index. Those rows are tagged
 * "scope":"breach" so the client does not present them as ordinary hits. */
char *entityapi_search_scoped(db_handle *db, const char *q, const char *type,
                              int limit, const char *tenant, int is_operator);

/* GET /api/entities/:type/:id — profile. NULL if missing, type mismatch, or
 * not visible to `tenant` (caller → 404 {"error":"not_found"}). */
char *entityapi_get(db_handle *db, const char *type, const char *id,
                    const char *tenant);
/* Non-operator: `exposure` is null and `exposure_withheld` says why. It is not
 * zeroed — a fabricated {breach_count:0} reads as "clean" and the count itself
 * is the disclosure. */
char *entityapi_get_scoped(db_handle *db, const char *type, const char *id,
                           const char *tenant, int is_operator);

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
char *entityapi_graph_scoped(db_handle *db, const char *type, const char *id,
                             int depth, const char *rel_types, int exclude_hubs,
                             int max_nodes, const char *tenant, int is_operator);

/* GET /api/entities/:type/:id/mentions?limit&offset — NULL → 404. */
char *entityapi_mentions(db_handle *db, const char *type, const char *id,
                         int limit, int offset, const char *tenant);
/* Breach mentions (m.surface = the cleartext identifier, m.source_id = the
 * breach) are excluded unless is_operator. The response carries a `scope`
 * block saying the view is filtered — deliberately WITHOUT a withheld count,
 * because that count is the exposure fact being protected. */
char *entityapi_mentions_scoped(db_handle *db, const char *type, const char *id,
                                int limit, int offset, const char *tenant,
                                int is_operator);

/* GET /api/entities/:type/:id/breaches?limit&offset — the breaches this
 * entity appears in (roadmap item 23). Joins entity_mentions rows written by
 * breach_index.c (extractor='breach-ingest', source_id == breach slug) to
 * breach_meta for the catalog fields. Returns {data:[...],exposure:{...}};
 * NULL if the entity is missing or type mismatches (caller → 404).
 *
 * The ingest side already existed — es_upsert_entity dedups on
 * (type, norm_key), so a breach email and an intel-mentioned email are the
 * same node. This is purely the reverse read that was never surfaced.
 *
 * "Secrets are NOT reachable here: only the breach catalog metadata" — that
 * was the reasoning for leaving this route plain-auth, and it was wrong. Which
 * breaches a named identifier appears in IS the sensitive fact; the leaked
 * password is a second one. Every row here is breach-derived by construction
 * (WHERE extractor='breach-ingest'), so this endpoint is now PLATFORM-OPERATOR
 * ONLY and denies rather than returning an empty list — see
 * entityapi_breaches_scoped for why an empty list is both a false negative and
 * an exposure oracle. */
char *entityapi_breaches(db_handle *db, const char *type, const char *id,
                         int limit, int offset, const char *tenant);
/* Non-operator → NULL, i.e. denied, for EVERY entity (breached or not) so the
 * response cannot be used to distinguish the two. core/httpd.c replies 403
 * before calling this; the NULL is the second line of defence. */
char *entityapi_breaches_scoped(db_handle *db, const char *type, const char *id,
                                int limit, int offset, const char *tenant,
                                int is_operator);

/* GET /api/entities/:type/:id/merges?limit&offset — the identity-resolution
 * decisions recorded for this entity: {same, confidence, reason, decided_at}
 * plus the counterpart node. NULL if the entity is missing, the type
 * mismatches, or it is not visible to `tenant` (caller → 404).
 *
 * entity_merges has been written by es_record_merge() all along and read only
 * by es_merge_pair_exists() as a memo, so the graph acted on these decisions
 * and no analyst could see one. Rows with same=0 are returned too — a recorded
 * "these are NOT the same" is a finding, not noise.
 *
 * The response carries `meta` (returned/total/limit/offset/has_more) so a
 * bounded page says how much exists, per house rule 2. */
char *entityapi_merges(db_handle *db, const char *type, const char *id,
                       int limit, int offset, const char *tenant);
/* The COUNTERPART of each merge is joined under the same visibility disjunct
 * as every other read here — entity_by_id only bounds the node you entered
 * from, so without this a merge row would hand a viewer the id and canonical
 * value of a quarantined breach node. Rows whose counterpart is not visible
 * are absent, and `scope` says the view is filtered WITHOUT a withheld count
 * (that count is the exposure fact), exactly as entityapi_mentions_scoped
 * does. */
char *entityapi_merges_scoped(db_handle *db, const char *type, const char *id,
                              int limit, int offset, const char *tenant,
                              int is_operator);

/* GET /api/intel/items/:uid/entities — entities mentioned in an item, joining
 * entity_mentions → entities on item_uid. Works for both breach records
 * ("breach:..." uids) and normal intel items. {data:[{entity_id,type,value,
 * label}]}. Never NULL (empty array when the item has no mentions). Caller frees.
 *
 * Deliberately NOT tenant-parameterised: its callers (httpd.c's item route,
 * reportapi.c's case-report renderer) have already authorised the ITEM, and
 * the entities returned are exactly the ones that item mentions. Adding a
 * predicate here would filter a list that is already bounded by an
 * authorisation the caller performed.
 *
 * It IS breach-scoped, though — the caller authorising the item is not the
 * same as the caller being allowed to see breach material, and this route is
 * plain-auth while its sibling GET /api/intel/items/breach:<uid> is
 * breach_gate()d. Breach-scoped entities and mentions are excluded unless
 * is_operator, so a "breach:<keyid>" uid yields an empty chip list for
 * everyone else instead of the cleartext identifier. */
char *entityapi_item_entities(db_handle *db, const char *uid);
char *entityapi_item_entities_scoped(db_handle *db, const char *uid,
                                     int is_operator);

#endif
