/* core/semsearchapi.h — GET /api/intel/semantic: vector and hybrid search
 * over the sqlite-vec index that core/embed_pod.c maintains.
 *
 *   ?q=<text>            required
 *   &mode=hybrid|vector  default hybrid
 *   &limit=N             rows returned, 1..200 (default 50)
 *   &k=N                 candidates per arm, 1..1000 (default 200)
 *
 * vector: embed q, k nearest in intel_vec, join intel_items.
 *
 * METRIC. vec0 is declared without `distance_metric`, so `distance` is L2,
 * not cosine. llama-server returns L2-normalised embeddings by default
 * (--embd-normalize 2), and for unit vectors L2 is a monotonic function of
 * cosine (d^2 = 2 - 2cos), so the RANKING is the cosine ranking; the number
 * in `semantic.distance` is the L2 distance (0..2), not 1 - cos.
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
 * vector arm returned but the tenant may not see are dropped from the
 * response; `is_operator` callers additionally get meta.tenant_withheld (other
 * tenants' rows) and meta.index_orphans (vectors whose row is gone). Those
 * counts are not given to tenants: how many of another tenant's rows match a
 * query is information about that tenant.
 *
 * Returns a malloc'd body and sets *status: 200, 400 (bad q/mode), 503 (no
 * embedding server configured, no index yet, the pod has REFUSED the index,
 * or the server's live model is not the one the index was built with — with
 * coverage in the body so the caller knows which), 502 (the embedding call
 * failed, or the server's model could not be identified — the reason is in
 * `detail`), 500.
 *
 * It makes network calls (GET /v1/models, POST /v1/embeddings), so it must
 * not run on the HTTP event loop: httpd.c runs it on a worker thread with
 * its own DB connection. */
#ifndef JO_SEMSEARCHAPI_H
#define JO_SEMSEARCHAPI_H
#include "db.h"

char *semsearchapi_query(db_handle *db, const char *tenant, const char *q,
                         const char *mode, int limit, int k, int is_operator,
                         int *status);

#endif
