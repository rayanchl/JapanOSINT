/* collectors/pod/wal_checkpoint_pod.c — fold the WAL back into the database
 * on a schedule, forcibly.
 *
 * WHY A POD. core/db.c sets journal_size_limit=256MB, and that limit is only
 * applied AFTER a checkpoint completes. SQLite's automatic checkpoints are
 * PASSIVE: they copy what they can and stop at the first frame some reader's
 * snapshot still needs. On this server some connection is always inside a
 * read transaction (the API, the enricher, the embed backfill, the scheduler's
 * own status sweeps), so a passive checkpoint never reaches the end of the
 * log, the log is never truncated, and the limit never fires.
 *
 * Measured 2026-09-21: after 15 hours of cold fill the database was 57.9 GB
 * and its -wal was 98.2 GB. The host volume hit 0.1 GB free, WSL refused to
 * start its VM, and every measurement process on the box died mid-run. The
 * same shape at 9.75 GB is what the db.c comment already records from
 * 2026-09-15; the cap did not help either time because nothing ever completed
 * a checkpoint for it to apply to.
 *
 * WHAT THIS DOES. Every five minutes, one TRUNCATE checkpoint from a worker
 * connection. TRUNCATE waits (busy handler) for the readers that are holding
 * old snapshots to finish their current statements, blocks new WRITERS for
 * that wait — readers are not blocked — then copies every frame and truncates
 * the -wal to zero. Readers' statements are short here (a page of results, a
 * batch of rows), so the wait is normally milliseconds. If a reader is
 * genuinely stuck for the whole 20 s the pod says so, with the WAL size, so a
 * leaked statement is a log line instead of a full disk.
 *
 * `_maint`: emits no intel_items, so it is exempt from records_drop. */
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include "source.h"
#include "core/db.h"
#include "third_party/sqlite3.h"

#define WAL_WAIT_MS 20000
#define WAL_LOUD_MB 1024LL   /* a WAL past this after a failed checkpoint is an incident */

static long long wal_bytes(sqlite3 *h) {
  const char *f = sqlite3_db_filename(h, "main");
  if (!f || !*f) return -1;
  char p[4096];
  snprintf(p, sizeof p, "%s-wal", f);
  struct stat st;
  return stat(p, &st) == 0 ? (long long)st.st_size : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  if (!ctx || !ctx->db || !ctx->db->h) return -1;
  sqlite3 *h = ctx->db->h;
  long long before = wal_bytes(h);
  sqlite3_busy_timeout(h, WAL_WAIT_MS);
  int nlog = -1, nckpt = -1;
  int rc = sqlite3_wal_checkpoint_v2(h, "main", SQLITE_CHECKPOINT_TRUNCATE, &nlog, &nckpt);
  long long after = wal_bytes(h);
  if (rc == SQLITE_OK) {
    if (before > 64LL * 1024 * 1024 || nlog > 0)
      fprintf(stderr, "[wal] checkpoint TRUNCATE: %d of %d frames folded, -wal %lld MB -> %lld MB\n",
              nckpt, nlog, before >> 20, after >> 20);
    return 0;
  }
  if (rc == SQLITE_BUSY) {
    fprintf(stderr, "[wal] checkpoint could not complete in %d s: a connection is holding a "
            "read snapshot open across statements (leaked sqlite3_stmt?). -wal is %lld MB%s\n",
            WAL_WAIT_MS / 1000, before >> 20,
            (before >> 20) > WAL_LOUD_MB ? " — this is how the disk filled on 2026-09-21" : "");
    return -1;
  }
  fprintf(stderr, "[wal] checkpoint failed rc=%d (%s), -wal %lld MB\n", rc, sqlite3_errstr(rc), before >> 20);
  return -1;
}

static const source_def wal_checkpoint_def = {
  .id = "wal-checkpoint", .collector = "_maint",
  .name = "WAL Checkpoint (forced TRUNCATE)",
  .name_ja = "WAL チェックポイント（強制切り詰め）",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(wal_checkpoint_def)
