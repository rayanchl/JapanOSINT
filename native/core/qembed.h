/* core/qembed.{c,h} — query-time embeddings for the OSINT pipeline.
 *
 * WHY THIS EXISTS. semsearchapi.c and service_vec.c each embed one text on
 * the interactive slot behind the same four gates (a server is configured,
 * the index exists and is not refused, the live query server serves the
 * model the index was built with, the dimension matches). The pipeline now
 * needs that in five more places — the corpus lookup's vector arm, prompt
 * record selection, the runtime relevance check, few-shot retrieval and the
 * resolver fallback — and the second copy of a gate is where it starts to
 * drift. This is the one copy for those callers; the two older ones are left
 * alone (other owners).
 *
 * THE INERT CONTRACT. Every function here returns failure WITHOUT TOUCHING
 * THE NETWORK when JO_EMBED_URL is unset, and every caller treats failure as
 * "do exactly what you did before". So an unconfigured deployment behaves
 * byte for byte as it did before any of these features existed.
 *
 * Embeds go to vecindex_query_url() (JO_EMBED_QUERY_URL, else JO_EMBED_URL)
 * on the interactive lane with bound_queue_wait, because every caller here
 * runs inside a user's search. Texts are bounded with embed_bound_text()
 * (JO_EMBED_MAX_CHARS, UTF-8 safe) — the same bound the indexes were built
 * with, so a query and a document are cut the same way.
 *
 *   JO_EMBED_QUERY_TIMEOUT_MS   per-request budget (default 15000, as
 *                               /api/intel/semantic uses)
 */
#ifndef JO_QEMBED_H
#define JO_QEMBED_H

#include <stddef.h>
#include "db.h"
#include "llm.h"
#include "../lib/vecindex.h"

/* Texts per /v1/embeddings request. llama-server takes an array, but one
 * request holding thousands of inputs is one request that times out whole. */
#define QEMBED_BATCH 32

/* 1 when an embedding server is configured (no network is touched). */
int qembed_enabled(void);

/* Embed texts[0..n) on the interactive query slot, QEMBED_BATCH at a time.
 * 0 with *out = n*dim floats (row-major, caller frees) and *dim set; -1 on
 * any failure (*st says why; LLM_ERR_BAD_REQUEST when nothing is configured),
 * nothing allocated. All batches must agree on the dimension. */
int qembed_texts(const char *const *texts, int n, float **out, int *dim,
                 llm_status *st);

/* May index `s` be queried with a vector from the live query server? 1 when
 * it exists, is not refused, has a recorded model equal to what the query
 * server serves, and a recorded dim (*dim_out, may be NULL). 0 with `why`
 * (may be NULL) otherwise. Never embeds. */
int qembed_index_usable(db_handle *db, const vecindex_spec *s, int *dim_out,
                        char *why, size_t cap);

/* The k nearest intel_items to `text` in intel_vec_v2, restricted to
 * `tenant` + the shared 'legacy' corpus INSIDE the KNN (an unscoped NULL
 * tenant reads every partition, as the FTS path does). Same gates as
 * /api/intel/semantic. Returns the hit count (*out malloc'd,
 * vecindex_hits_free), or -1 with `why` filled — "unavailable" is -1, never
 * a fabricated empty answer. */
int qembed_intel_knn(db_handle *db, const char *tenant, const char *text, int k,
                     vecindex_hit **out, char *why, size_t cap);

#endif
