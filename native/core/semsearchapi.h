/* core/semsearchapi.h — GET /api/intel/semantic: vector and hybrid search
 * over the sqlite-vec index that core/embed_pod.c maintains — and the other
 * routes that rank by that index (/similar below).
 *
 *   ?q=<text>            required
 *   &mode=hybrid|vector  default hybrid
 *   &limit=N             rows returned, 1..200 (default 50)
 *   &k=N                 candidates per arm, 1..1000 (default 200)
 *   &source=a[,b…]       source_id, one or a comma list (<= 32)  — IN the KNN
 *   &record_type=T       record_type =                           — IN the KNN
 *   &since=&until=       the row's date (EMBED_ROW_TS: published_at when ISO,
 *                        else fetched_at), any text strftime reads — IN the KNN
 *                        as `ts >= / <=`
 *   &lang=L              intel_items.language =  — POST-filter (the index has
 *                        no language column)
 *   &bbox=w,s,e,n        lat/lon inside the box  — POST-filter
 *
 * FILTERS. The four "IN the KNN" filters are metadata columns of
 * intel_vec_v2, so the KNN answers k rows that already satisfy them — a
 * filter applied after a KNN over everything would starve a narrow filter of
 * candidates. lang and bbox have no column; they are applied to the k
 * candidates and every row they remove is COUNTED in meta.filters.dropped,
 * with meta.filters.post_note saying a post-filter cannot reach past k. While
 * a pre-metadata index is being migrated nothing can run in the KNN, so every
 * filter becomes a counted post-filter and meta.filters.in_knn says so. An
 * unreadable since/until/bbox is a 400 — a filter that could not be read is
 * never silently dropped.
 *
 * vector: embed q, k nearest in intel_vec_v2 WITHIN the caller's tenant
 *         partitions (tenant + 'legacy', filtered inside the KNN — see
 *         embed_pod.c "METADATA INSIDE THE KNN"), join intel_items.
 *         `meta.knn_partitioned` says whether that filter ran in the KNN
 *         (false while a pre-metadata index is being migrated).
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
 * list shape (rendered by intelapi_row_to_item, the feed's own builder) plus
 * a `semantic` object
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
#include "../lib/vecindex.h"

/* sqlite-vec answers at most 4096 rows per KNN. */
#define SEMSEARCH_KNN_MAX     4096
#define SEMSEARCH_MAX_SOURCES 32

/* The request's filters as text; NULL/"" = not applied. */
typedef struct {
  const char *source;        /* one id, or a comma-separated list          */
  const char *record_type;
  const char *since, *until; /* any time text SQLite's strftime reads      */
  const char *lang;
  const char *bbox;          /* "w,s,e,n"                                   */
} semsearch_filters;

/* The same, resolved: the source list split, the dates as epoch seconds (the
 * unit of the index's `ts` column), the box as numbers. */
typedef struct {
  char *srcbuf;
  const char *src[SEMSEARCH_MAX_SOURCES];
  int nsrc;
  const char *rtype, *lang;
  int has_since, has_until;
  long long since_ts, until_ts;
  int has_bbox;
  double bw, bs, be, bn;
} semsearch_rf;

/* 0, or -1 with `err` = invalid_since | invalid_until | invalid_bbox |
 * too_many_sources | server_error. `rf` is freed with semsearch_rf_free(). */
int  semsearch_resolve(db_handle *db, const semsearch_filters *f,
                       semsearch_rf *rf, char *err, size_t cap);
void semsearch_rf_free(semsearch_rf *rf);

/* Every check a query embed needs (server configured, index built and not
 * refused, live model = the index's model) then the embed itself, on the
 * interactive lane. NULL on success with *qv (malloc'd) and *dim; otherwise a
 * malloc'd error body (503/502/500 with coverage) and *status set. Makes
 * network calls: never on the event loop. */
char *semsearch_embed(db_handle *db, const char *q, float **qv, int *dim,
                      long *embed_ms, int *status);

/* The k nearest (k <= SEMSEARCH_KNN_MAX) to `qv` among `tenant`'s rows and
 * the shared corpus (tenant NULL = every row), with rf's source/record_type/
 * date filters INSIDE the KNN. *in_knn (may be NULL) = 0 while the index has
 * no metadata columns (a migration is pending): then nothing was filtered and
 * the caller must post-check. Returns hits or -1 (err filled). */
int  semsearch_knn(db_handle *db, const char *tenant, const float *qv, int dim,
                   int k, const semsearch_rf *rf, vecindex_hit **out,
                   int *in_knn, char *err, size_t errcap);

char *semsearchapi_query(db_handle *db, const char *tenant, const char *q,
                         const char *mode, int limit, int k, int is_operator,
                         int *status);
/* The same with filters (f may be NULL). */
char *semsearchapi_query_ex(db_handle *db, const char *tenant, const char *q,
                            const char *mode, int limit, int k, int is_operator,
                            const semsearch_filters *f, int *status);

/* THE WORKER-ROUTE SHAPE. Every vector route answers through one signature so
 * httpd.c runs them all through one off-loop job (sem_spawn): `tenant` is the
 * resolved tenant (NULL/"" = unscoped), `id` the path parameter (an item uid,
 * a case id; "" when the route has none), `qs` the raw query string, parsed
 * here — a value too long for its buffer is a 414, never half a filter. */
typedef char *(*semsearch_route_fn)(db_handle *db, const char *tenant,
                                    const char *id, const char *qs,
                                    int is_operator, int *status);

/* GET /api/intel/semantic — semsearchapi_query_ex() over the query string. */
char *semsearchapi_http(db_handle *db, const char *tenant, const char *id,
                        const char *qs, int is_operator, int *status);

/* GET /api/intel/items/:uid/similar?k=N (1..200, default 20) plus the S1
 * filters. The k nearest rows to the item's OWN stored vector
 * (vecindex_load; no embedding call, so no server need answer), inside the
 * caller's tenant partitions, the item itself excluded (meta.excluded_self).
 * 404 when the item is not visible to the caller; 409 `not_embedded` with
 * coverage (and the server's refusal reason when there is one) when the item
 * has no vector yet; 503 when there is no index at all. */
char *semsearchapi_similar(db_handle *db, const char *tenant, const char *uid,
                           const char *qs, int is_operator, int *status);

#endif
