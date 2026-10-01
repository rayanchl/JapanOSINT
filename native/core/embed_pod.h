/* core/embed_pod.h — embedding backfill for hybrid semantic search.
 *
 * A `_maint` pod (registered from embed_pod.c with REGISTER_SOURCE, like
 * collectors/pod/simhash_pod.c) that embeds `title + ' ' + summary` (or the
 * first 512 bytes of body when there is no summary) of intel_items rows into
 * a sqlite-vec `vec0` virtual table, `intel_vec(uid TEXT PRIMARY KEY,
 * embedding float[N])`. N is whatever the configured embedding server
 * answers with on the first call; it is recorded together with the model
 * name in `intel_vec_meta`, and a later run against a different model or
 * dimension REFUSES rather than mixing two spaces in one index — a nearest
 * neighbour across two models is a number, not a similarity.
 *
 * The pod is inert unless JO_EMBED_URL names an embedding llama-server
 * (`--embedding`, :8082 in scripts/start-llama.sh). Inert is announced on
 * stderr and in the coverage block, never silent.
 *
 * Knobs (all env, all optional):
 *   JO_EMBED_URL           base URL of the embedding server; empty = disabled
 *   JO_EMBED_MODEL         model name sent in the request and recorded in meta
 *                          (else GET /v1/models is asked, else "unknown")
 *   JO_EMBED_SINCE_DAYS    rows whose COALESCE(published_at,fetched_at) is
 *                          older than this are out of scope (default 180)
 *   JO_EMBED_RECORD_TYPES  comma-separated record_type allowlist; default =
 *                          everything except the two collector notices
 *   JO_EMBED_BATCH         texts per /v1/embeddings request (default 32)
 *   JO_EMBED_MAX_PER_RUN   rows per scheduler tick (default 2000) — keeps a
 *                          tick short so cancel is honoured promptly
 *   JO_EMBED_MAX_CHARS     input text bound in bytes (default 1000; cut on a
 *                          UTF-8 boundary). Not a discard: the row is still
 *                          embedded and stored whole, only the model input
 *                          is bounded, and the bound is recorded in meta.
 *
 * Coverage is DATA: embedded_count / eligible_count / model / dim live in
 * intel_vec_meta and every /api/intel/semantic response carries them in
 * `meta.coverage`, so a caller can see that 40,000 of 12,900,000 rows are
 * searchable rather than mistake a thin index for a thin corpus. */
#ifndef JO_EMBED_POD_H
#define JO_EMBED_POD_H

#include "db.h"
#include "../third_party/cJSON.h"

/* Names shared with semsearchapi.c. */
#define EMBED_VEC_TABLE  "intel_vec"
#define EMBED_META_TABLE "intel_vec_meta"

/* The configured embedding base URL, or NULL when the pod is disabled. */
const char *embed_base_url(void);

/* 1 when the vec0 table exists on this connection (i.e. at least one run has
 * created it), else 0. semsearchapi uses it to answer an honest 503 rather
 * than a prepare error. */
int embed_table_exists(db_handle *db);

/* Model name and dimension recorded in intel_vec_meta, 0/"" when nothing has
 * been embedded yet. Returns the dim (0 = no index). `model` is filled with
 * at most `cap` bytes. */
int embed_index_dim(db_handle *db, char *model, size_t cap);

/* Build the `coverage` object for a semantic response:
 *   {enabled, model, dim, embedded_count, eligible_count, since_days,
 *    record_types, max_chars, sweep, last_error, refused}
 * Every field is read from intel_vec_meta / env, nothing is estimated. Always
 * returns an object (never NULL) — an absent index is reported, not hidden. */
cJSON *embed_coverage_json(db_handle *db);

/* Bound `text` to JO_EMBED_MAX_CHARS bytes on a UTF-8 boundary into a
 * malloc'd copy. Exposed so the query side embeds the user's text under the
 * same bound the index was built with. */
char *embed_bound_text(const char *text);

#endif
