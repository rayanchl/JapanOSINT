/* core/service_vec_pod.c — builds the semantic service index off the request
 * path.
 *
 * WHY THIS EXISTS. service_vec_catalogue() used to build the index on demand,
 * inside the search request that first needed it. That is one embedding
 * request per 32 services, and it was measured on 2026-09-11 (twice, 273 s and
 * 280 s) for the 1,838 entity pivots against multilingual-e5-large on CPU.
 * A first search stalling for four and a half minutes is not a feature that
 * degrades gracefully — and a client that gives up mid-build leaves the work
 * for the next request to redo from the start.
 *
 * So the build moved here: a `_maint` pod like embed_pod.c, on the scheduler's
 * own threads, where taking minutes is normal and being cancelled is safe.
 * While it runs, routing falls back to registry order with the bound stated
 * in-band — the same documented fallback as an unconfigured server, not a
 * silent degradation.
 *
 * Inert unless JO_EMBED_URL is set, and cheap when there is nothing to do:
 * service_vec_build() returns immediately unless the registry signature, the
 * model or the embedding dimension changed, so a tick on a steady tree is one
 * meta read and one probe request.
 *
 * The interval is deliberately long. The only things that invalidate the index
 * are a registry change (which needs a rebuild of the binary, i.e. a restart)
 * and a model change under a running process; neither is a per-minute event,
 * and each tick that DOES rebuild costs several minutes of embedding server
 * time that the intel backfill pod also wants.
 */
#include "service_vec.h"
#include "embed_pod.h"
#include "db.h"
#include "../source.h"
#include <stdio.h>
#include <stdlib.h>

#define SVECPOD_SID "_svec_index"

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                      /* the index is the output, not intel rows */
  if (!embed_base_url() || !*embed_base_url()) return 0;   /* inert, announced
                                                              by service_vec */
  const char *sem = getenv("JO_ROUTE_SEMANTIC");
  if (sem && sem[0] == '0' && sem[1] == 0) return 0;

  if (!ctx || !ctx->db) return -1;
  /* Own connection, like every other pod that writes: a transaction belongs to
   * a connection, not a thread (core/db.h, "THE RULE"). */
  db_handle own;
  db_handle *db = db_worker_open(&own, ctx->db);
  int n = service_vec_build(db);
  db_worker_close(&own);
  if (n < 0) return -1;            /* reason already recorded in meta + stderr */
  return 0;
}

static const source_def svec_pod_def = {
  .id = SVECPOD_SID, .collector = "_maint",
  .name = "Semantic Service Index",
  .name_ja = "サービス意味検索インデックス",
  .description = "Builds the sqlite-vec + fts5 index behind semantic service "
                 "routing, off the search request path. Inert unless "
                 "JO_EMBED_URL is set.",
  .url = "internal://service-index",
  .update_interval_sec = 900, .run = run };
REGISTER_SOURCE(svec_pod_def)
