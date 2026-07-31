/* collectors/sources/upload_gc_pod.c — reaps abandoned chunked uploads
 * (core/uploadapi.c).
 *
 * An upload that is begun and never committed would otherwise leak its
 * staged parts to disk forever, and a crash between mkdir and INSERT leaves
 * a scratch directory with no row at all. This pod handles both, plus
 * expiring terminal rows.
 *
 * `_maint` for the usual reason: it emits no intel_items, so fetch_log would
 * record records=0 on every run and trip records_drop against its own
 * baseline. It also inherits the scheduler's skip-if-running serialisation. */
#include "../../source.h"
#include "../../core/uploadapi.h"

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                    /* reaping writes nothing to intel */
  return uploadapi_gc(ctx->db, ctx->cancel) < 0 ? -1 : 0;
}

static const source_def upload_gc_def = {
  .id = "upload-gc", .collector = "_maint",
  .name = "Abandoned Upload Reaper",
  .name_ja = "未完了アップロード回収",
  .update_interval_sec = 900, .run = run,
  .category = "maintenance" };
REGISTER_SOURCE(upload_gc_def)
