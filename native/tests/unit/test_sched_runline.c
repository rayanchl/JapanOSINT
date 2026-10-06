/* test_sched_runline.c — the scheduler's run line must not hide lost records.
 *
 *   [sched] <id> run rc=<rc> records=<n> <ms>ms stored=<s>[ notices=<k>]
 *           [ failed=<f> EMIT-FAILED: …][ UID-COLLISION: …]
 *
 * Two ways a run could lose records and still print a clean line:
 *
 *  1. `records=` excludes collector-*-notice rows, `stored=` (distinct uids)
 *     included them, and the collision test compared the two directly. So one
 *     notice hid one collapsed record: 2,000 records with one uid collision
 *     plus a truncation notice read records=2000 stored=2000 — house rule 4b's
 *     defect, invisible. stored= must stay the number the DB holds for the
 *     source (the registry sweep checks it against COUNT(*)), so the note now
 *     compares records against stored MINUS the distinct notice uids.
 *
 *  2. emit() < 0 — a ROLLBACK under "database is locked", a failed COMMIT, an
 *     item with no uid — was ignored by the counting sink. A lock storm that
 *     cost a feed half its rows read as a quieter feed. It is now `failed=` on
 *     the run line and in the fetch_log error column.
 *
 * Drives the public scheduler_run_source() with in-process fake sources, no
 * network, and reads the run line back from stderr. */
#include "core/scheduler.h"
#include "core/intel.h"
#include "core/db.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void emit_rec(intel_sink *sink, const char *key, const char *rt) {
  intel_item it; memset(&it, 0, sizeof it);
  it.remote_key = key;
  it.title = key ? key : "no key";
  it.record_type = rt ? rt : "unit-record";
  sink->emit(sink, &it);
}

/* 2,000 records, record 1 keyed onto record 0 (one collision), then a notice. */
static int run_collide(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx;
  char k[32];
  for (int i = 0; i < 2000; i++) {
    snprintf(k, sizeof k, "k%d", i == 1 ? 0 : i);
    emit_rec(sink, k, NULL);
  }
  emit_rec(sink, "collector-truncation-notice", "collector-truncation-notice");
  return 0;
}

/* 10 records, all distinct, plus one notice: a clean run. */
static int run_clean(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx;
  char k[32];
  for (int i = 0; i < 10; i++) { snprintf(k, sizeof k, "c%d", i); emit_rec(sink, k, NULL); }
  emit_rec(sink, "collector-truncation-notice", "collector-truncation-notice");
  return 0;
}

/* 8 storable records and 2 the sink must refuse (no uid, no remote_key). */
static int run_refused(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx;
  char k[32];
  for (int i = 0; i < 8; i++) { snprintf(k, sizeof k, "f%d", i); emit_rec(sink, k, NULL); }
  emit_rec(sink, NULL, NULL);
  emit_rec(sink, NULL, NULL);
  return 0;
}

static const source_def D_COLLIDE = { .id = "UNIT_RL_COLLIDE", .collector = "unit",
  .name = "unit", .update_interval_sec = 3600, .run = run_collide };
static const source_def D_CLEAN = { .id = "UNIT_RL_CLEAN", .collector = "unit",
  .name = "unit", .update_interval_sec = 3600, .run = run_clean };
static const source_def D_REFUSED = { .id = "UNIT_RL_REFUSED", .collector = "unit",
  .name = "unit", .update_interval_sec = 3600, .run = run_refused };

/* Run `d` with stderr captured; return the malloc'd "[sched] <id> run" line. */
static char *run_line(db_handle *db, const source_def *d, const char *cap_path) {
  fflush(stderr);
  int saved = dup(2);
  int fd = open(cap_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  assert(fd >= 0);
  dup2(fd, 2); close(fd);
  scheduler_run_source(db, d, NULL);
  fflush(stderr);
  dup2(saved, 2); close(saved);

  FILE *f = fopen(cap_path, "r");
  assert(f);
  char buf[4096], want[128], *out = NULL;
  snprintf(want, sizeof want, "[sched] %s run ", d->id);
  while (fgets(buf, sizeof buf, f))
    if (strstr(buf, want)) { free(out); out = strdup(buf); }
  fclose(f);
  assert(out && "no run line");
  return out;
}

static void add_source_row(db_handle *db, const char *id) {
  sqlite3_stmt *s;
  assert(sqlite3_prepare_v2(db->h, "INSERT OR IGNORE INTO sources(id,name,type,category)"
                            " VALUES (?1,'unit','api','unit')", -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_step(s); sqlite3_finalize(s);
}

static long db_rows(db_handle *db, const char *id) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(db->h, "SELECT count(*) FROM intel_items WHERE source_id=?1",
                            -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  char cap[1100];
  snprintf(cap, sizeof cap, "%s.stderr", dbp);

  /* 1. the notice no longer hides the collision */
  add_source_row(&db, D_COLLIDE.id);
  char *l = run_line(&db, &D_COLLIDE, cap);
  printf("  %s", l);
  assert(strstr(l, " records=2000 "));
  assert(strstr(l, " stored=2000"));             /* == what the DB holds */
  assert(db_rows(&db, D_COLLIDE.id) == 2000);    /* 1999 records + 1 notice */
  assert(strstr(l, " notices=1"));
  assert(strstr(l, "UID-COLLISION: 1 of 2000") &&
         "a notice row hid a collapsed record");
  assert(!strstr(l, "failed="));
  free(l);

  /* a clean run with a notice raises nothing */
  add_source_row(&db, D_CLEAN.id);
  l = run_line(&db, &D_CLEAN, cap);
  printf("  %s", l);
  assert(strstr(l, " records=10 ") && strstr(l, " stored=11") && strstr(l, " notices=1"));
  assert(!strstr(l, "UID-COLLISION") && !strstr(l, "failed="));
  free(l);

  /* 2. refused emits are on the run line and in fetch_log */
  add_source_row(&db, D_REFUSED.id);
  l = run_line(&db, &D_REFUSED, cap);
  printf("  %s", l);
  assert(strstr(l, " records=8 ") && strstr(l, " stored=8"));
  assert(strstr(l, " failed=2 ") && "refused emits were not reported");
  free(l);
  {
    sqlite3_stmt *s;
    assert(sqlite3_prepare_v2(db.h, "SELECT status,records_fetched,error FROM fetch_log"
                              " WHERE source_id=?1 ORDER BY id DESC LIMIT 1", -1, &s,
                              NULL) == SQLITE_OK);
    sqlite3_bind_text(s, 1, D_REFUSED.id, -1, SQLITE_TRANSIENT);
    assert(sqlite3_step(s) == SQLITE_ROW);
    const char *err = (const char *)sqlite3_column_text(s, 2);
    printf("  fetch_log: status=%s records=%d error=%s\n",
           (const char *)sqlite3_column_text(s, 0), sqlite3_column_int(s, 1),
           err ? err : "(null)");
    assert(err && strstr(err, "2 emitted record(s) refused"));
    sqlite3_finalize(s);
  }

  unlink(cap);
  db_close(&db);
  printf("test_sched_runline: ok\n");
  return 0;
}
