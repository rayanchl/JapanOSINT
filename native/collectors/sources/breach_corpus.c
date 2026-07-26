/* collectors/sources/breach_corpus.c — loads the breach CATALOG from the
 * committed seed TSV (docs/breach-corpus.seed.tsv) into breach_meta.
 *
 * This is the collector behind the "request → parse → save" flow in the app's
 * Breach corpus console; running it is the "save" step. It reads a local file
 * and makes no network calls: the seed is the identity manifest (names, dates,
 * account counts), never credentials. Leaked records arrive by a separate,
 * operator-driven path (/api/admin/breach/ingest → breach_index_ingest).
 *
 * `collector` is "_breach" on purpose. scheduler.c skips fetch_log_write() and
 * anomaly_detect() for underscore-prefixed collectors, and this job needs that
 * skip: catalog rows are SOURCES, not intel items, so it emits zero rows
 * through the sink and would otherwise log records=0 on every run and trip
 * records_drop against its own baseline. That is what this job looks like, not
 * a fault. It stays fully visible and runnable either way — db_seed_sources()
 * gives every registered id a `sources` row, so it appears in /api/status and
 * answers POST /api/intel/sources/breach-corpus/run like any other source. */
#include "../../source.h"
#include "../../core/breach_meta.h"
#include <stdio.h>

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                     /* the catalog is written through breach_meta */
  if (!ctx || !ctx->db) return -1;

  breach_seed_stats st = {0};
  int n = breach_meta_load_seed_tsv(ctx->db, NULL, &st);
  if (n < 0) return -1;

  if (ctx->progress) {
    char msg[512];
    snprintf(msg, sizeof msg,
             "{\"stage\":\"catalog\",\"format\":\"%s\",\"rows_read\":%d,"
             "\"rows_parsed\":%d,\"rows_skipped\":%d,\"rows_new\":%d,"
             "\"rows_existing\":%d,\"loaded\":%d}",
             st.format, st.rows_read, st.rows_parsed, st.rows_skipped,
             st.rows_new, st.rows_existing, n);
    ctx->progress(msg);
  }
  return 0;
}

static const source_def breach_corpus_def = {
  .id = "breach-corpus", .collector = "_breach",
  .name = "Breach corpus catalog",
  .name_ja = "漏洩カタログ",
  .update_interval_sec = 86400, .run = run,
  .category = "breach", .type = "dataset",
  .url = "internal://docs/breach-corpus.seed.tsv",
  .description = "Loads the local HIBP-style breach catalog seed into breach_meta.",
  .free_tier = 1 };
REGISTER_SOURCE(breach_corpus_def)
