/* lib/vecindex.{c,h} — ONE vector index.
 *
 * WHY THIS EXISTS. core/embed_pod.c (intel_vec) and core/service_vec.c
 * (service_vec) each re-implemented the same structure: create a sqlite-vec
 * vec0 table, probe the live server's dimension, record and enforce the
 * model's identity, write vectors, query KNN, fuse with an FTS arm. This repo
 * has paid for the "same structure written N times" shape before — the
 * uid-collision bug lived independently in four record paths and fixing three
 * left the fourth losing data (CLAUDE.md 4b). The plan (docs/semantic-
 * correlation-plan-2026-09-11.md §9.1) was to extract it before the fourth and
 * fifth copies (entity_vec, fewshot_vec, anomaly_vec, image_vec) existed.
 *
 * TWO LESSONS BAKED IN, both paid for:
 *   1. NEVER `ALTER TABLE … RENAME` a vec0 table. vec0 owns shadow tables
 *      named after the virtual table and RENAME does not move them, so the
 *      renamed table is no longer queryable — test_service_vec.c caught a build
 *      that reported 1,838 services indexed and a KNN that returned nothing.
 *      Nothing here renames; a schema change creates a NEW table and copies.
 *   2. PROBE THE LIVE SERVER for the dimension, never another index's
 *      metadata. Two indexes have two lifecycles.
 *
 * IDENTITY. An index records the model (JO_EMBED_MODEL, else what GET
 * /v1/models says) and dimension of its FIRST write, in the same transaction
 * as that write. A later server with a different model or width is a different
 * vector space; a nearest neighbour across two spaces is a number, not a
 * similarity. vecindex_check_identity() says which of the two changed and the
 * caller decides: the intel index REFUSES (its corpus took days to embed), the
 * service index REBUILDS (it takes minutes). "Cannot identify the model" is
 * never recorded as a model.
 *
 * INCREMENTAL UPSERT. An index with a done table keeps (key, text_hash,
 * embedded_at, meta_hash): an unchanged text costs a hash compare, not an
 * embed; a changed one is DELETE + INSERT (vec0 has no upsert); a changed
 * METADATA value with the same text is rewritten from the stored vector,
 * never re-embedded. Rows the server will not embed go to the fail table with
 * their hash and reason, and are retried when their text changes.
 *
 * WRITING A NEW INDEX is: a vecindex_spec (table names, key column, metadata
 * columns), VECINDEX_REGISTER(spec) so /api/status reports it, and a _maint
 * pod that builds vecindex_items and calls vecindex_sync(). See test_vecindex.c
 * for a complete one in ~50 lines.
 *
 * METRIC. vec0's default L2. llama-server returns unit vectors, so for them
 * d² = 2 − 2·cos and the RANKING equals cosine. Switching the column to
 * distance_metric=cosine would change every stored distance's meaning for
 * callers that print it (semsearchapi exposes `distance`) and buys no ranking
 * change, so it is not done. vec_cosine() is for code that wants the angle. */
#ifndef JO_VECINDEX_H
#define JO_VECINDEX_H

#include <stddef.h>
#include "../core/db.h"
#include "../core/llm.h"
#include "../third_party/cJSON.h"

struct http_client;

/* ---- the spec ------------------------------------------------------------ */

typedef enum { VI_TEXT = 0, VI_INT, VI_FLOAT } vecindex_coltype;

/* A metadata column, filterable INSIDE the KNN (sqlite-vec v0.1.9 "metadata
 * column"), or the one PARTITION KEY. A partition key shards the index — a KNN
 * constrained to one partition value never reads another's chunks — and is
 * what tenancy wants. vec0 cannot UPDATE a partition key in place; a value
 * that changes is rewritten (delete + insert) by vecindex_writer_put(). */
typedef struct {
  const char *name;
  vecindex_coltype type;
  int partition;                    /* 1 = PARTITION KEY (TEXT or INT)      */
} vecindex_col;

typedef struct vecindex_spec {
  const char *name;          /* short label in status/coverage ("intel")     */
  const char *table;         /* the vec0 virtual table                       */
  const char *key_col;       /* its TEXT PRIMARY KEY column                  */
  const char *meta_table;    /* (k TEXT PRIMARY KEY, v TEXT)                 */
  const char *done_table;    /* (key, text_hash, embedded_at, meta_hash); NULL
                              * for an index rebuilt whole (service_vec)     */
  const char *fail_table;    /* (key, text_hash, failed_at, reason) or NULL  */
  const vecindex_col *cols;  /* metadata / partition columns, in order       */
  int ncols;
  const char *rebuild_hint;  /* appended to a refusal: how to start over     */
} vecindex_spec;

/* One metadata value. vec0 rejects NULL in a TEXT metadata column ("Expected
 * text … received NULL", measured on the vendored v0.1.9), so a NULL is
 * written as '' / 0 — callers that need "absent" must encode it themselves. */
typedef struct {
  int is_null;
  long long i;
  double f;
  const char *s;
} vecindex_val;

#define VI_S(str) ((vecindex_val){ .is_null = 0, .s = (str) })
#define VI_I(n)   ((vecindex_val){ .is_null = 0, .i = (long long)(n) })
#define VI_F(x)   ((vecindex_val){ .is_null = 0, .f = (double)(x) })

/* The text every dimension probe sends. Fixed so a stub server (and a test
 * counting what was sent) can tell a probe from a row. */
#define VECINDEX_PROBE_TEXT "dimension probe"
/* Rank-fusion constant: rank 1 contributes 1/61, rank 10 1/70, so an item
 * both arms like outranks one only one arm likes, and neither arm's score
 * scale (L2 distance vs bm25) has to be normalised against the other. */
#define VECINDEX_RRF_K 60.0

/* ---- the embedding server ------------------------------------------------ */

/* JO_EMBED_URL, or NULL when unset/empty (every index is then inert). */
const char *vecindex_base_url(void);
/* JO_EMBED_QUERY_URL when set, else JO_EMBED_URL; NULL when JO_EMBED_URL is
 * unset. A distinct URL string keys its own llm_worker thread, so a query
 * never queues behind a backfill batch. */
const char *vecindex_query_url(void);

/* Model identity: JO_EMBED_MODEL when set, else the basename of GET
 * <base>/v1/models data[0].id. 0 with `out` filled, or -1 with `out` holding
 * why it could not be determined. `http` may be NULL. */
int vecindex_detect_model(struct http_client *http, const char *base,
                          char *out, size_t cap);
/* What the server at `base` SAYS it serves, ignoring JO_EMBED_MODEL (a pin
 * cannot vouch for a second server). */
int vecindex_served_model(struct http_client *http, const char *base,
                          char *out, size_t cap);
/* vecindex_detect_model() for JO_EMBED_URL, successful answers cached for
 * JO_EMBED_MODEL_CHECK_TTL_MS (default 10 s) — for request paths. */
int vecindex_live_model(char *out, size_t cap);
/* vecindex_live_model(), plus — when JO_EMBED_QUERY_URL differs — proof that
 * the query server serves the same model. -1 with the reason otherwise. */
int vecindex_live_query_model(char *out, size_t cap);

/* One tiny embed; returns the server's dimension, or -1 (*st says why). */
int vecindex_probe_dim(llm_client *llm, int timeout_ms, llm_status *st);

/* Valid-UTF-8 copy (each bad byte → U+FFFD; *replaced may be NULL). */
char *vecindex_utf8_sanitize(const char *text, size_t *replaced);
/* Sanitised copy bounded to `max_bytes` on a UTF-8 boundary. */
char *vecindex_bound_text(const char *text, size_t max_bytes);

/* ---- meta + tables ------------------------------------------------------- */

int  vecindex_table_exists(db_handle *db, const char *name);
int  vecindex_exists(db_handle *db, const vecindex_spec *s);   /* vec0 table */
long vecindex_count(db_handle *db, const char *table);         /* 0 if absent */

void vecindex_ensure_meta(db_handle *db, const vecindex_spec *s);
/* Creates done/fail tables (and adds done.meta_hash to an older one). */
void vecindex_ensure_side_tables(db_handle *db, const vecindex_spec *s);
/* CREATE VIRTUAL TABLE IF NOT EXISTS at `dim`. 0 / -1. */
int  vecindex_create(db_handle *db, const vecindex_spec *s, int dim);
/* DROP TABLE IF EXISTS — the vec0 table only, never a rename. */
int  vecindex_drop(db_handle *db, const vecindex_spec *s);

int   vecindex_meta_get(db_handle *db, const vecindex_spec *s, const char *k,
                        char *out, size_t cap);              /* 1 = found */
char *vecindex_meta_dup(db_handle *db, const vecindex_spec *s, const char *k);
int   vecindex_meta_set(db_handle *db, const vecindex_spec *s, const char *k,
                        const char *v);                      /* v NULL = NULL */
int   vecindex_meta_set_long(db_handle *db, const vecindex_spec *s,
                             const char *k, long v);
long  vecindex_meta_long(db_handle *db, const vecindex_spec *s, const char *k);

/* ---- identity ------------------------------------------------------------ */

typedef enum {
  VI_ID_OK = 0,         /* recorded model and dim match the live server     */
  VI_ID_EMPTY,          /* nothing recorded yet: the first write records    */
  VI_ID_MODEL_MISMATCH, /* a different model (same width or not)            */
  VI_ID_DIM_MISMATCH    /* same/unknown model, different width              */
} vecindex_identity;

/* Compare what the index recorded with `live_model` and `live_dim` (0 = not
 * probed: the dim is not compared). `why` (may be NULL) gets a sentence
 * naming both sides and the spec's rebuild hint. */
vecindex_identity vecindex_check_identity(db_handle *db, const vecindex_spec *s,
                                          const char *live_model, int live_dim,
                                          char *why, size_t cap);
/* Record model + dim (call inside the transaction of the first write). */
int vecindex_record_identity(db_handle *db, const vecindex_spec *s,
                             const char *model, int dim);
int vecindex_refuse(db_handle *db, const vecindex_spec *s, const char *why);
/* 1 when refused (why filled), else 0. A refused index must not be queried. */
int vecindex_refused(db_handle *db, const vecindex_spec *s, char *why, size_t cap);

/* ---- transactions -------------------------------------------------------- */

/* BEGIN IMMEDIATE, CHECKED: unchecked, a BUSY left later statements in
 * autocommit and a vec row could land without its done row. */
int vecindex_begin(db_handle *db);
/* COMMIT when ok (ROLLBACK if that fails), else ROLLBACK. 0 = committed. */
int vecindex_end(db_handle *db, int ok);

/* ---- writing ------------------------------------------------------------- */

typedef struct {
  db_handle *db;
  const vecindex_spec *s;
  sqlite3_stmt *del, *ins, *done, *unfail;
} vecindex_writer;

/* Prepare the writer's statements. Call inside a transaction (the writer does
 * not open one). 0 / -1. */
int  vecindex_writer_open(vecindex_writer *w, db_handle *db, const vecindex_spec *s);
/* DELETE then INSERT the vector (always — vec0 has no upsert, and a vec row
 * left without its done row by any earlier partial write would otherwise fail
 * the INSERT forever); upsert the done row (when the spec has one) with
 * text_hash/meta_hash/mark; clear a fail row. `vals` holds spec->ncols values
 * (NULL when ncols == 0). 0 / -1 (sqlite3_errmsg says why). */
int  vecindex_writer_put(vecindex_writer *w, const char *key, const float *vec,
                         int dim, const vecindex_val *vals,
                         unsigned long long text_hash,
                         unsigned long long meta_hash, const char *mark);
void vecindex_writer_close(vecindex_writer *w);

/* Read one stored vector by key: *out malloc'd (caller frees), *dim set.
 * 0 found, 1 not found, -1 error. */
int vecindex_load(db_handle *db, const vecindex_spec *s, const char *key,
                  float **out, int *dim);

/* cos(a, b); 0 when either is the zero vector. */
double vec_cosine(const float *a, const float *b, int dim);

/* FNV-1a 64 — the text hash the done table stores. */
unsigned long long vecindex_hash(const char *s);
/* Hash of a row's metadata values (spec order), for the done table. */
unsigned long long vecindex_vals_hash(const vecindex_spec *s, const vecindex_val *vals);

/* ---- incremental embedding ---------------------------------------------- */

/* One row to embed. key/text/vals are borrowed. in_done/in_failed and the
 * stored hashes come from the done/fail tables (vecindex_item_state() or the
 * caller's own join). */
typedef struct {
  const char *key;
  const char *text;                 /* already bounded + sanitised          */
  const vecindex_val *vals;         /* spec->ncols values, or NULL          */
  unsigned long long hash, meta_hash;
  int in_done, in_failed, has_done_meta;
  unsigned long long done_hash, done_meta_hash, failed_hash;
} vecindex_item;

/* Fill in_done/in_failed and the stored hashes from the side tables. Also
 * sets item->hash (from text) and item->meta_hash (from vals). */
void vecindex_item_state(db_handle *db, const vecindex_spec *s, vecindex_item *it);

/* Settled with this exact text already (embedded, or failed on it)? */
int vecindex_item_unchanged(const vecindex_item *it);

typedef struct {
  long walked, sent, embedded, unchanged, failed, failed_seen, rewritten;
} vecindex_stats;

/* Mark unchanged rows seen: done rows get embedded_at := mark, failed rows
 * failed_at := mark; a done row whose METADATA changed is rewritten from its
 * stored vector (no embed). One transaction. 0 / -1 (recorded). */
int vecindex_mark_seen(db_handle *db, const vecindex_spec *s,
                       vecindex_item **rows, int n, const char *mark,
                       const char *now, vecindex_stats *st);

/* Embed and store rows[0..m) (m <= 256). On return *n_done = how many LEADING
 * rows are settled (stored, or recorded as failed). Returns 0 (all settled),
 * -1 (stop: server/db failure, recorded in meta last_error; rows from *n_done
 * on are untouched), -2 (refused: the server's dimension differs from the
 * index's; recorded in meta refused). *dim_io is the index's dim (0 = none
 * yet: the first write creates the table and records model + dim in the same
 * transaction). A refused batch is retried row by row; a row that fails alone
 * while the server demonstrably works goes to the fail table. */
int vecindex_embed_items(db_handle *db, const vecindex_spec *s, llm_client *llm,
                         vecindex_item **rows, int m, int *dim_io,
                         const char *model, const char *mark, const char *now,
                         vecindex_stats *st, int *n_done);

/* The whole incremental cycle for an index small enough to hold in memory:
 * look each item up, mark the unchanged, embed the rest `batch` at a time.
 * Identity is checked first (model + probed dim) and a mismatch REFUSES
 * (recorded; -2). Returns 0, -1 or -2. */
int vecindex_sync(db_handle *db, const vecindex_spec *s, llm_client *llm,
                  const char *model, vecindex_item *items, int n, int batch,
                  const char *mark, const char *now, vecindex_stats *st);

/* Record an SQL failure as data (meta last_error) and on stderr. */
void vecindex_sql_error(db_handle *db, const vecindex_spec *s, const char *now,
                        const char *what);

/* ---- post-batch hooks ---------------------------------------------------- */

/* What a hook is handed after a batch COMMITTED. Everything is borrowed and
 * valid only for the call. */
typedef struct {
  const vecindex_spec *spec;
  int n, dim;
  const char *const *keys;          /* n                                    */
  const float *vecs;                /* n * dim, row-major                   */
  const vecindex_val *vals;         /* n * spec->ncols, or NULL             */
} vecindex_batch;

/* THE CONTRACT. A hook runs on the thread that wrote the batch — for the
 * embedding pods that is the scheduler's maintenance lane — after the batch's
 * transaction committed, with the writer's own connection. It must be FAST
 * (it delays the next batch), must NOT call the embedding server (the pod's
 * send budget and the server's queue are not its to spend), may write in its
 * own small transaction on `db`, and must not keep any pointer it is handed.
 * Its failure is its own: nothing it returns can undo the committed batch. */
typedef void (*vecindex_hook_fn)(db_handle *db, const vecindex_batch *b, void *user);

/* Register a hook for one index. Up to 16 hooks in all; -1 when full. */
int  vecindex_hook_register(const vecindex_spec *s, vecindex_hook_fn fn, void *user);
void vecindex_hook_unregister(const vecindex_spec *s, vecindex_hook_fn fn, void *user);
/* Call every hook registered for b->spec. */
void vecindex_fire_hooks(db_handle *db, const vecindex_batch *b);

/* ---- coverage ------------------------------------------------------------ */

/* The common coverage shape, every field read from meta / the side tables:
 *   {index, table, index_present, model, dim, embedded_count, eligible_count,
 *    eligible_counted_at, failed_count, failed_sample, backlog, state,
 *    refused, last_error, last_run_at, last_tick}
 * eligible_count/backlog are null unless MEASURED (meta eligible_count) —
 * an estimate would let a caller compute a ratio that was never observed.
 * Never NULL. */
cJSON *vecindex_coverage_json(db_handle *db, const vecindex_spec *s);
/* The compact form for /api/status:
 *   {embedded, eligible, failed, refused, backlog, model, dim, state, last_run} */
cJSON *vecindex_coverage_compact(db_handle *db, const vecindex_spec *s);

/* Every registered index, so /api/status can list them without knowing them. */
void vecindex_register(const vecindex_spec *s);
int  vecindex_all(const vecindex_spec *const **out);
/* {"<name>": compact, ...} for every registered index. */
cJSON *vecindex_status_json(db_handle *db);

#define VECINDEX_REGISTER(spec) \
  __attribute__((constructor)) static void vecindex_reg_##spec(void) { \
    vecindex_register(&(spec)); }

/* ---- querying ------------------------------------------------------------ */

/* A filter applied INSIDE the KNN. `op` is one of = != < <= > >= IN. A filter
 * on the PARTITION KEY takes = (one value) or IN (several) — the KNN is run
 * once per partition value and merged, because sqlite-vec answers `k` rows
 * PER IN-value (measured: k=1 with two tenants in IN returned 2 rows), and a
 * caller asking for k must get k. A metadata IN is native (TEXT/INT only). */
typedef struct {
  const char *col;
  const char *op;
  const vecindex_val *vals;
  int nvals;
} vecindex_filter;

typedef struct { char *key; double distance; } vecindex_hit;

/* The k nearest keys to q (dim must be the index's), nearest first. Returns
 * the number of hits (malloc'd into *out; vecindex_hits_free), or -1 with
 * `err` filled (a bad column/op is an error, never a silent unfiltered
 * query). */
int  vecindex_knn(db_handle *db, const vecindex_spec *s, const float *q, int dim,
                  int k, const vecindex_filter *f, int nf, vecindex_hit **out,
                  char *err, size_t errcap);
void vecindex_hits_free(vecindex_hit *h, int n);

/* Reciprocal-rank fusion of a vector arm and an FTS arm. */
typedef struct {
  char *key;
  int vrank, frank;                 /* 1-based; 0 = that arm did not return it */
  double distance;                  /* vector arm's distance (vrank > 0)     */
  double score;
} vecindex_cand;
typedef struct { vecindex_cand *v; int n, cap; } vecindex_fused;

/* Add (or credit) `key` with its rank in one arm; NULL on OOM. */
vecindex_cand *vecindex_fused_add(vecindex_fused *f, const char *key,
                                  int vrank, int frank, double distance);
/* hits[i] gets vector rank i+1. Returns how many were added. */
int  vecindex_fused_add_hits(vecindex_fused *f, const vecindex_hit *h, int n);
/* `SELECT <key_col> FROM <fts> WHERE <fts> MATCH ?expr ORDER BY bm25 LIMIT k`,
 * each row credited with its FTS rank. Returns rows added, or -1 (no table /
 * bad expression: "this arm has nothing to say", not a fatal error). */
int  vecindex_fts_arm(db_handle *db, const char *fts_table, const char *key_col,
                      const char *match_expr, int k, vecindex_fused *f);
/* Score desc, then vector rank (absent last), then key — deterministic. */
void vecindex_fused_sort(vecindex_fused *f);
void vecindex_fused_free(vecindex_fused *f);

#endif
