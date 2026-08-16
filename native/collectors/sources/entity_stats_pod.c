/* collectors/sources/entity_stats_pod.c — internal pod that recomputes
 * significance (PMI / lift) over the entity co-mention graph (roadmap 22).
 *
 * `collector` is "_maint" on purpose. scheduler.c skips fetch_log_write() and
 * anomaly_detect() for underscore-prefixed collectors, and this job needs
 * that skip on both counts: it emits zero intel_items, so it would log
 * records=0 forever and trip records_drop against its own baseline; and a
 * first full sweep over a real corpus runs for minutes, which trips
 * duration_outlier. Neither is a fault — that is simply what this job looks
 * like. It also inherits the scheduler's skip-if-running serialisation, so
 * two sweeps can never overlap.
 *
 * 900s interval against entity_stats.c's 6h TTL: new edges (stats_at NULL)
 * are picked up within 15 minutes, the corpus is fully rescored every 6
 * hours, and a sweep too large for one tick continues on the next. */
#include "source.h"
#include "core/entity_stats.h"

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                    /* scoring writes through entity_stats */
  return entity_stats_recompute(ctx->db, ctx->cancel) < 0 ? -1 : 0;
}

static const source_def entity_stats_def = {
  .id = "entity-stats", .collector = "_maint",
  .name = "Entity Correlation Scoring",
  .name_ja = "エンティティ相関スコア",
  .update_interval_sec = 900, .run = run };
REGISTER_SOURCE(entity_stats_def)
