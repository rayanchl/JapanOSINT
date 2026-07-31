/* collectors/_enrich/sources/llm_enricher.c — the LLM entity enricher on the
 * SAME scheduler path every source uses (port of the entities scope of
 * server/src/utils/llmEnricher.js + scheduler.js's 5-min guarded cron). A
 * registered source with update_interval_sec=300; the existing scheduler runs
 * it serially (== scheduleGuardedRun skip-if-running). Gated on LLM_ENABLED
 * like Node. Writes via core/entitystore.h — the one entity-graph path,
 * draining the one intel_items the one sink fills (collectors + osint-search
 * alike). No new mechanism, no source_kind. */
#include "../../source.h"
#include "../../core/entity_enrich.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                         /* enricher writes via entitystore */
  const char *le = getenv("LLM_ENABLED");
  if (!le || strcmp(le, "true") != 0) {
    fprintf(stderr, "[llm-enricher] skipped (LLM_ENABLED != true)\n");
    return 0;
  }
  return entity_enrich_run(ctx->db, ctx->llm) == 0 ? 0 : -1;
}

static const source_def llm_enricher_def = {
  .id = "llm-enricher", .collector = "_enrich",
  .name = "LLM Entity Enricher", .name_ja = "エンティティ抽出",
  .update_interval_sec = 300, .run = run,
  .category = "enrichment" };
REGISTER_SOURCE(llm_enricher_def)
