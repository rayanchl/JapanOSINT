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
 * WHAT THIS DOES, AND WHAT IT USED TO DO. Every five minutes, from the
 * scheduler worker connection the pod is handed:
 *
 *   1. A PASSIVE checkpoint. It takes no writer lock and never calls the busy
 *      handler: it copies every frame no reader still needs and returns. On a
 *      quiet moment that is the whole log, and SQLite then reuses the WAL
 *      from its beginning as soon as the readers that predate it move on, so
 *      the file stops growing even when step 2 does not succeed.
 *   2. A TRUNCATE checkpoint, in up to WAL_TRUNC_TRIES short attempts of
 *      WAL_TRUNC_WAIT_MS each, with a gap between them. This is the step that
 *      shrinks the -wal file to zero.
 *
 * WHY THE TRUNCATE MUST BE SHORT. A RESTART/TRUNCATE checkpoint takes the
 * database's WRITER lock first and then waits — holding it — for readers whose
 * snapshots still need the log. This pod used to wait up to 20 s in one go.
 * db.c gives every connection busy_timeout=5000 (the scheduler workers and the
 * search pipeline raise theirs to 30 s; the event loop and the off-loop API
 * workers keep 5 s), so any reader holding a snapshot past 5 s (an export walk, an
 * /api/status build) made those writers fail with "database is locked" while
 * the pod sat on the lock: intel.c's emit() rolled back and returned -1, the scheduler's counting
 * sink skipped the error so the run line still looked whole, event-loop writes
 * stalled 5 s, and audit_write lost rows. The checkpoint that was meant to
 * protect the disk was costing records.
 *
 * Now the writer lock is held for at most WAL_TRUNC_WAIT_MS at a time — well
 * under the 5 s every other connection will wait — and released between
 * attempts so the writers queued behind it get through. A reader that outlives
 * every attempt costs a skipped TRUNCATE (the PASSIVE pass already folded what
 * it could), never a lost write. The pod says so, with the WAL size, so a
 * leaked statement is still a log line instead of a full disk.
 *
 * THE CONNECTION IS BORROWED. ctx->db is the scheduler worker's own handle
 * (busy_timeout=30000, set in scheduler.c's worker_thread). The pod used to
 * leave its 20 s timeout behind on it, silently changing how every later
 * collector run on that worker waited for the lock. The previous value is read
 * back with PRAGMA busy_timeout and restored on every path out.
 *
 * `_maint`: emits no intel_items, so it is exempt from records_drop. */
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include "source.h"
#include "core/db.h"
#include "third_party/sqlite3.h"

/* Per-attempt ceiling on how long the TRUNCATE may hold the writer lock while
 * it waits for readers. Must stay well below the busy_timeout of every other
 * connection (5000 ms from db.c) or this pod becomes the thing that makes
 * their writes fail. */
#define WAL_TRUNC_WAIT_MS 1000
#define WAL_TRUNC_TRIES   3
#define WAL_TRUNC_GAP_MS  500   /* writers queued behind an attempt run here */
#define WAL_LOUD_MB 1024LL   /* a WAL past this after a failed checkpoint is an incident */

static long long wal_bytes(sqlite3 *h) {
  const char *f = sqlite3_db_filename(h, "main");
  if (!f || !*f) return -1;
  char p[4096];
  snprintf(p, sizeof p, "%s-wal", f);
  struct stat st;
  return stat(p, &st) == 0 ? (long long)st.st_size : 0;
}

/* The connection's current busy timeout in ms, or -1 if it cannot be read. */
static int busy_timeout_of(sqlite3 *h) {
  sqlite3_stmt *s;
  int v = -1;
  if (sqlite3_prepare_v2(h, "PRAGMA busy_timeout", -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) v = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
  }
  return v;
}

static void sleep_ms(int ms) {
  struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
  nanosleep(&ts, NULL);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  if (!ctx || !ctx->db || !ctx->db->h) return -1;
  sqlite3 *h = ctx->db->h;
  long long before = wal_bytes(h);

  /* A connection that has never read the database has not opened its WAL
   * yet, and sqlite3_wal_checkpoint_v2() on it is a silent no-op: SQLITE_OK
   * with -1/-1 frame counts, i.e. "success" with nothing checkpointed.
   * Reading the schema cookie opens the WAL without touching any table. */
  sqlite3_exec(h, "PRAGMA schema_version", NULL, NULL, NULL);

  /* 1. PASSIVE — no writer lock, no busy handler. */
  int plog = -1, pckpt = -1;
  int prc = sqlite3_wal_checkpoint_v2(h, "main", SQLITE_CHECKPOINT_PASSIVE,
                                      &plog, &pckpt);

  /* 2. TRUNCATE in short, separated attempts; the caller's timeout is put
   *    back after each one, so no exit path can leave ours behind. */
  int saved = busy_timeout_of(h);
  if (saved < 0) saved = 5000;               /* db.c's default */
  int rc = SQLITE_BUSY, nlog = -1, nckpt = -1, tries = 0;
  while (tries < WAL_TRUNC_TRIES) {
    if (tries > 0) {
      if (ctx->cancel && *ctx->cancel) break;
      sleep_ms(WAL_TRUNC_GAP_MS);
    }
    tries++;
    sqlite3_busy_timeout(h, WAL_TRUNC_WAIT_MS);
    rc = sqlite3_wal_checkpoint_v2(h, "main", SQLITE_CHECKPOINT_TRUNCATE,
                                   &nlog, &nckpt);
    sqlite3_busy_timeout(h, saved);
    if (rc != SQLITE_BUSY) break;
  }
  long long after = wal_bytes(h);

  if (rc == SQLITE_OK && nlog < 0) {
    fprintf(stderr, "[wal] checkpoint did nothing: this connection is not in WAL "
            "mode (journal_mode changed?), -wal %lld MB\n", after >> 20);
    return -1;
  }
  if (rc == SQLITE_OK) {
    if (before > 64LL * 1024 * 1024 || nlog > 0)
      fprintf(stderr, "[wal] checkpoint TRUNCATE: %d of %d frames folded, -wal %lld MB -> %lld MB\n",
              nckpt, nlog, before >> 20, after >> 20);
    return 0;
  }
  if (rc == SQLITE_BUSY) {
    /* The PASSIVE pass is what still protects the disk here: when it folded
     * every frame, the next writer restarts the log from the top. */
    int all_folded = (prc == SQLITE_OK && plog >= 0 && pckpt == plog);
    fprintf(stderr, "[wal] TRUNCATE skipped after %d x %d ms: a reader is holding a "
            "snapshot open (leaked sqlite3_stmt?). PASSIVE folded %d of %d frames%s; "
            "-wal is %lld MB%s\n",
            tries, WAL_TRUNC_WAIT_MS, pckpt, plog,
            all_folded ? " (the log is reused once those readers move on)" : "",
            after >> 20,
            (after >> 20) > WAL_LOUD_MB ? " — this is how the disk filled on 2026-09-21" : "");
    return all_folded ? 0 : -1;
  }
  fprintf(stderr, "[wal] checkpoint failed rc=%d (%s), -wal %lld MB\n", rc, sqlite3_errstr(rc), before >> 20);
  return -1;
}

static const source_def wal_checkpoint_def = {
  .id = "wal-checkpoint", .collector = "_maint",
  .name = "WAL Checkpoint (PASSIVE, then bounded TRUNCATE)",
  .name_ja = "WAL チェックポイント（受動＋短時間の切り詰め）",
  .update_interval_sec = 300, .run = run,
  .category = "maintenance" };
REGISTER_SOURCE(wal_checkpoint_def)
