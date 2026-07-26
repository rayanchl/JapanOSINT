/* collectors/sources/content_change_pod.c — retention sweep for the content
 * snapshot store (roadmap item 26, core/content_change.c).
 *
 * The per-key cap is enforced inline on every stored snapshot; this pod
 * handles the two things that cannot be done inline: age-based expiry, and
 * reclaiming keys nothing fetches any more (a source whose URLs changed
 * would otherwise leave its old ref_keys behind forever).
 *
 * `_maint` for the usual reason — it emits no intel_items, so fetch_log would
 * record records=0 on every run and trip records_drop against its own
 * baseline. It also inherits the scheduler's skip-if-running serialisation. */
#include "../../source.h"
#include "../../core/content_change.h"

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                    /* retention writes nothing to intel */
  return content_change_sweep(ctx->db, 20000, ctx->cancel) < 0 ? -1 : 0;
}

static const source_def content_change_def = {
  .id = "content-change-retention", .collector = "_maint",
  .name = "Content Snapshot Retention",
  .name_ja = "コンテンツ差分スナップショット保持",
  .update_interval_sec = 900, .run = run };
REGISTER_SOURCE(content_change_def)
