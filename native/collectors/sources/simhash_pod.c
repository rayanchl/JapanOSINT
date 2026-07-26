/* collectors/sources/simhash_pod.c — backfills near-duplicate fingerprints
 * for intel_items rows predating roadmap item 25 (core/simhash.c).
 *
 * `_maint` for the same reason as entity_stats_pod.c: it emits no
 * intel_items, so fetch_log would record records=0 forever and trip
 * records_drop against its own baseline, and a large sweep trips
 * duration_outlier. It also inherits the scheduler's skip-if-running
 * serialisation, so two sweeps never overlap.
 *
 * The backfill walks in ascending publication order on purpose: a row being
 * fingerprinted can then never be older than one already processed, so it can
 * never displace an existing cluster representative — the whole migration
 * costs 4 band inserts + 1 UPDATE per row with no relabelling at all. */
#include "../../source.h"
#include "../../core/simhash.h"

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                  /* fingerprinting writes in place */
  return simhash_backfill(ctx->db, 20000, ctx->cancel) < 0 ? -1 : 0;
}

static const source_def simhash_def = {
  .id = "simhash-backfill", .collector = "_maint",
  .name = "Near-Duplicate Fingerprint Backfill",
  .name_ja = "近似重複フィンガープリント補完",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(simhash_def)
