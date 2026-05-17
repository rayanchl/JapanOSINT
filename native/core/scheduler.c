/* core/scheduler.c — timer-wheel cron (scheduleGuardedRun parity: per-source
 * interval, skip-if-running). P4: minimal single-thread loop; the bulk waves
 * (P5) reuse it unchanged. */
#include "scheduler.h"
#include "../source.h"
#include "intel.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

int scheduler_run_source(db_handle *db, const source_def *d,
                         const char *entity) {
  intel_sink sink = intel_sink_make(db, d->id, "legacy");
  volatile int cancel = 0;
  http_client *http = http_client_new();   /* sources expect ctx->http set */
  llm_client llm; llm_init(&llm, http);
  source_ctx ctx = {0};
  ctx.source_id = d->id;
  ctx.entity = entity;
  ctx.db = db;
  ctx.http = http;
  ctx.llm = &llm;
  ctx.cancel = &cancel;
  int rc = d->run(&ctx, &sink);
  http_client_free(http);
  fprintf(stderr, "[sched] %s run rc=%d\n", d->id, rc);
  return rc;
}

/* sources.schedule_mode for `id` (default 'map_cron' if row/col absent).
 * Returns 1 if search_only (scheduler must NOT run it for the map). */
static int is_search_only(db_handle *db, const char *id) {
  sqlite3_stmt *s; int so = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT schedule_mode FROM sources WHERE id=?1", -1, &s, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s,0)!=SQLITE_NULL)
      so = strcmp((const char *)sqlite3_column_text(s,0), "search_only") == 0;
    sqlite3_finalize(s);
  }
  return so;
}

void scheduler_loop(db_handle *db) {
  const source_def **all = registry_all();
  int n = registry_count();
  time_t *next = calloc(n, sizeof(time_t));
  time_t now0 = time(NULL);
  /* Stagger first runs 3s apart so 90+ sources don't storm upstreams at boot
   * (many do nationwide tiled Overpass). Then each follows its own interval. */
  for (int i = 0; i < n; i++) next[i] = now0 + (time_t)i * 3;
  fprintf(stderr, "[sched] %d sources registered\n", n);
  for (;;) {
    time_t now = time(NULL);
    for (int i = 0; i < n; i++) {
      const source_def *d = all[i];
      /* Unified: schedule any source with an interval; on-demand sources
       * (interval<=0, e.g. OSINT-pivot) are run by the pipeline, not here.
       * No source_kind branch — a source is a source. */
      if (d->update_interval_sec <= 0) continue;
      if (now < next[i]) continue;
      /* User-set per-source mode: search_only sources don't cron for the
       * map (the search tab triggers them on demand). */
      if (is_search_only(db, d->id)) { next[i] = now + d->update_interval_sec; continue; }
      scheduler_run_source(db, d, NULL);
      next[i] = now + d->update_interval_sec;
    }
    sleep(1);
  }
}

static void *sched_thread(void *arg) {
  scheduler_loop((db_handle *)arg);   /* infinite; lives for process lifetime */
  return NULL;
}

void scheduler_start_background(db_handle *db) {
  if (getenv("JO_NO_SCHED")) {
    fprintf(stderr, "[sched] background scheduler disabled (JO_NO_SCHED)\n");
    return;
  }
  pthread_t t;
  if (pthread_create(&t, NULL, sched_thread, db) == 0) {
    pthread_detach(t);
    fprintf(stderr, "[sched] background scheduler started (serve + refresh)\n");
  } else {
    fprintf(stderr, "[sched] pthread_create failed; serving without scheduler\n");
  }
}
