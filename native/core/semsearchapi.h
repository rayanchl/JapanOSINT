/* core/semsearchapi.h — GET /api/intel/semantic: vector and hybrid search
 * over the sqlite-vec index that core/embed_pod.c maintains.
 *
 *   ?q=<text>            required
 *   &mode=hybrid|vector  default hybrid
 *   &limit=N             rows returned, 1..200 (default 50)
 *   &k=N                 candidates per arm, 1..1000 (default 200)
 *
 * vector: embed q, k nearest by cosine distance in intel_vec, join
 *         intel_items.
 * hybrid: Reciprocal Rank Fusion (k=60) of that vector top-K and an FTS5
 *         top-K (fts_query_expr, ordered by bm25). A row found by only one
 *         arm is kept with its single contribution, lower down — fusion
 *         never drops what an arm returned.
 *
 * The envelope is {data:[item…], meta:{…}}. Items have the /api/intel/items
 * list shape (see intelapi.c row_to_item — the SELECT is duplicated here
 * because that function is static) plus a `semantic` object
 * {score, vector_rank, fts_rank, distance} saying WHY the row is where it is.
 * meta states every size in the pipeline — vector_hits, fts_hits, fused,
 * shown, total — and `coverage` (embed_coverage_json) says how much of the
 * corpus the vector arm could see at all; a thin index must never read as a
 * thin corpus.
 *
 * `tenant` is matched as `tenant_id IN (?,'legacy')` like every other reader
 * of intel_items (see intel_items_query::tenant in intelapi.h). Rows the
 * vector arm returned but the tenant may not see are counted in
 * meta.tenant_withheld, never silently skipped.
 *
 * Returns a malloc'd body and sets *status: 200, 400 (bad q/mode), 503 (no
 * embedding server configured, or no index yet — with coverage in the body
 * so the caller knows which), 502 (the embedding call failed — the llm_status
 * code is in `detail`), 500. */
#ifndef JO_SEMSEARCHAPI_H
#define JO_SEMSEARCHAPI_H
#include "db.h"

char *semsearchapi_query(db_handle *db, const char *tenant, const char *q,
                         const char *mode, int limit, int k, int *status);

#endif
