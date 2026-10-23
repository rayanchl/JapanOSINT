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

/* ── the fetch-failure disclosure the SCHEDULER files (2026-10-22) ────────
 *
 * collector-fetch-failure was lib/hpengine.c's and only hpengine's, so ~8,600
 * VJSON/bespoke rows stored NOTHING when a fetch was refused — identical in
 * the database to a source that ran and found nothing, which is the invisible
 * nothing house rule 1 is about. Measured on the registry sweep: 288 of its
 * first 3,612 rows, all of them vsrc.
 *
 * These four fakes pin the decision, and the one that matters most is
 * `run_empty`: an HONEST EMPTY must stay silent, or the fix would file a
 * failure notice for every quiet feed in the fleet and the disclosure would
 * become noise nobody reads. The fakes make no HTTP request, so hosts == 0 and
 * run_status() cannot excuse a non-zero rc — which is the case a bespoke
 * collector hits when it gives up before the network. */
static int run_silent_fail(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx; (void)sink;
  return -1;                 /* emits nothing, discloses nothing */
}
/* rc=0 and nothing emitted: the collector went, looked, and found nothing. */
static int run_empty(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx; (void)sink;
  return 0;
}
/* Fails, but files its own notice — the shape hpengine produces. One
 * disclosure is enough; a second, thinner one on top would be worse. */
static int run_own_notice(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx;
  emit_rec(sink, "collector-shape-notice", "collector-shape-notice");
  return -1;
}
/* Fails by return code but DID store records: run_status() already reads this
 * as the count-idiom misfiring, so it is not a failure and owes nothing. */
static int run_fail_with_records(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx;
  emit_rec(sink, "wr0", NULL);
  emit_rec(sink, "wr1", NULL);
  return -1;
}
/* THE CASE `run_status()` CALLS "ok" AND THE DATABASE CANNOT READ. Every host
 * answered (so the health verdict is green, and stays green — this test does
 * not change it) and the collector still reported a failure, which is what a
 * maintenance or rate-limit page served as a 200 looks like from here.
 * Measured 2026-10-23: `il-knesset-dates` fetches 131,618 bytes of HTML and
 * returns rc=-1, and 32 of the sweep's 80 EMITS_NOTHING rows came back rc=-1.
 * The host log is only ever written by a real transfer, so it is seeded
 * directly — see http_client_test_log_host(). */
static int run_answered_then_failed(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  http_client_test_log_host(ctx->http, "https://answered.example/odata", 200);
  return -1;
}
/* The same host, answering, and a collector that says so: an honest empty must
 * stay silent whatever the host did. This is the control for the one above. */
static int run_answered_then_empty(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  http_client_test_log_host(ctx->http, "https://answered.example/odata", 200);
  return 0;
}

static const source_def D_SILENT = { .id = "UNIT_RL_SILENT", .collector = "unit",
  .name = "unit", .update_interval_sec = 3600, .run = run_silent_fail };
static const source_def D_EMPTY = { .id = "UNIT_RL_EMPTY", .collector = "unit",
  .name = "unit", .update_interval_sec = 3600, .run = run_empty };
static const source_def D_OWNNOTE = { .id = "UNIT_RL_OWNNOTE", .collector = "unit",
  .name = "unit", .update_interval_sec = 3600, .run = run_own_notice };
static const source_def D_FAILREC = { .id = "UNIT_RL_FAILREC", .collector = "unit",
  .name = "unit", .update_interval_sec = 3600, .run = run_fail_with_records };
static const source_def D_ANSFAIL = { .id = "UNIT_RL_ANSFAIL", .collector = "unit",
  .name = "unit", .update_interval_sec = 3600, .run = run_answered_then_failed };
static const source_def D_ANSEMPTY = { .id = "UNIT_RL_ANSEMPTY", .collector = "unit",
  .name = "unit", .update_interval_sec = 3600, .run = run_answered_then_empty };
/* A maintenance pod writes its own tables and nothing to intel_items by
 * design, so it must never be told it failed to fetch. */
static const source_def D_POD = { .id = "UNIT_RL_POD", .collector = "_maint",
  .name = "unit", .update_interval_sec = 3600, .run = run_silent_fail };

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

/* How many collector-fetch-failure rows this source holds, and the properties
 * of the newest one (caller frees). */
static long db_fetch_failures(db_handle *db, const char *id, char **props) {
  sqlite3_stmt *s; long n = -1;
  if (props) *props = NULL;
  assert(sqlite3_prepare_v2(db->h,
            "SELECT count(*) FROM intel_items WHERE source_id=?1"
            " AND record_type='collector-fetch-failure'", -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  if (props && n > 0 &&
      sqlite3_prepare_v2(db->h,
          "SELECT properties FROM intel_items WHERE source_id=?1"
          " AND record_type='collector-fetch-failure' LIMIT 1", -1, &s,
          NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *p = (const char *)sqlite3_column_text(s, 0);
      if (p) *props = strdup(p);
    }
    sqlite3_finalize(s);
  }
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

  /* 1b. the run also lands on the sources row the dashboards read: a run that
   * stored records is online, with the run's distinct-uid count, duration and
   * a last_success stamp. Before this write existed every row stayed at the
   * seed's 'pending' / 0 forever (see sources_row_record in scheduler.c). */
  {
    sqlite3_stmt *s;
    assert(sqlite3_prepare_v2(db.h, "SELECT status,records_count,response_time_ms,"
             "last_check,last_success FROM sources WHERE id=?1", -1, &s, NULL) == SQLITE_OK);
    sqlite3_bind_text(s, 1, D_COLLIDE.id, -1, SQLITE_STATIC);
    assert(sqlite3_step(s) == SQLITE_ROW);
    const char *st = (const char *)sqlite3_column_text(s, 0);
    assert(st && !strcmp(st, "online") && "a run that stored records is online");
    /* 2000 uids were stored: 1999 records plus the collision NOTICE. The row
     * counts DATA records, so a notice never inflates a source's record count. */
    assert(sqlite3_column_int(s, 1) == 1999 && "records_count is the stored DATA count");
    assert(sqlite3_column_type(s, 2) != SQLITE_NULL && "response_time_ms recorded");
    assert(sqlite3_column_type(s, 3) != SQLITE_NULL && sqlite3_column_type(s, 4) != SQLITE_NULL);
    sqlite3_finalize(s);
  }

  /* 2. refused emits are on the run line and in fetch_log */
  add_source_row(&db, D_REFUSED.id);
  l = run_line(&db, &D_REFUSED, cap);
  printf("  %s", l);
  assert(strstr(l, " records=8 ") && strstr(l, " stored=8"));
  assert(strstr(l, " failed=2 ") && "refused emits were not reported");
  assert(strstr(l, " bytes=0 requests=0") && "the run line carries the transfer size");
  free(l);
  {
    sqlite3_stmt *s;
    assert(sqlite3_prepare_v2(db.h, "SELECT status,records_fetched,error,bytes_fetched FROM fetch_log"
                              " WHERE source_id=?1 ORDER BY id DESC LIMIT 1", -1, &s,
                              NULL) == SQLITE_OK);
    sqlite3_bind_text(s, 1, D_REFUSED.id, -1, SQLITE_TRANSIENT);
    assert(sqlite3_step(s) == SQLITE_ROW);
    const char *err = (const char *)sqlite3_column_text(s, 2);
    printf("  fetch_log: status=%s records=%d error=%s\n",
           (const char *)sqlite3_column_text(s, 0), sqlite3_column_int(s, 1),
           err ? err : "(null)");
    assert(err && strstr(err, "2 emitted record(s) refused"));
    /* transfer bytes are RECORDED for every run — 0 for these in-process fakes,
     * which made no HTTP request, and never NULL on a row this build wrote */
    assert(sqlite3_column_type(s, 3) != SQLITE_NULL && sqlite3_column_int64(s, 3) == 0);
    sqlite3_finalize(s);
  }

  /* 3. A run that left NOTHING behind and failed now says so as DATA. */
  add_source_row(&db, D_SILENT.id);
  l = run_line(&db, &D_SILENT, cap);
  printf("  %s", l);
  {
    char *props = NULL;
    assert(db_fetch_failures(&db, D_SILENT.id, &props) == 1 &&
           "a refused fetch stored no disclosure — rule 1's invisible nothing");
    assert(props);
    /* Only what the run exposed. NO http_status: this collector never reported
     * one, and inventing it is the mistake the notice exists to prevent. */
    assert(!strstr(props, "http_status") &&
           "the scheduler must not name a status nobody measured");
    assert(strstr(props, "\"run_rc\":-1"));
    assert(strstr(props, "\"hosts_contacted\":0"));
    assert(strstr(props, "made no HTTP request at all"));
    assert(strstr(props, "\"transport_error\":"));
    assert(strstr(props, "NOTHING was stored for this run"));
    free(props);
    /* The disclosure counts as a record, exactly as hpengine's does, so a dead
     * source reads records=1 stored=1 rather than 0/0 (CLAUDE.md says so). */
    assert(strstr(l, " records=1 ") && strstr(l, " stored=1"));
    assert(!strstr(l, "UID-COLLISION") && !strstr(l, "failed="));
  }
  free(l);
  /* and the run's own output corroborates the row, which is what the registry
   * sweep matches on — it has no [hp:…] line to read for a bespoke collector */
  {
    FILE *f = fopen(cap, "r"); char buf[4096]; int seen = 0;
    assert(f);
    while (fgets(buf, sizeof buf, f))
      if (strstr(buf, "[sched] UNIT_RL_SILENT fetch-failure:")) seen = 1;
    fclose(f);
    assert(seen && "the fetch-failure is not on stderr for the sweep to match");
  }

  /* 3b. Running it AGAIN updates that one row rather than adding another: a
   * permanently dead source must not grow a row per run. */
  l = run_line(&db, &D_SILENT, cap);
  free(l);
  assert(db_fetch_failures(&db, D_SILENT.id, NULL) == 1 &&
         "the disclosure is not upsert-keyed; a dead source grows a row a run");

  /* 3c. AN HONEST EMPTY STAYS SILENT. This is the half that makes the fix
   * safe: rc=0 with no records is a source that went and found nothing, and
   * filing a failure for it would put a false notice on every quiet feed. */
  add_source_row(&db, D_EMPTY.id);
  l = run_line(&db, &D_EMPTY, cap);
  printf("  %s", l);
  assert(strstr(l, " records=0 ") && strstr(l, " stored=0"));
  assert(db_fetch_failures(&db, D_EMPTY.id, NULL) == 0 &&
         "an honest empty was reported as a fetch failure");
  free(l);

  /* 3d. A collector that disclosed something ITSELF gets no second notice —
   * which is also what keeps every hpengine row out of this path, since the
   * engine's own fetch-failure is emitted through this same sink. */
  add_source_row(&db, D_OWNNOTE.id);
  l = run_line(&db, &D_OWNNOTE, cap);
  printf("  %s", l);
  assert(strstr(l, " notices=1"));
  assert(db_fetch_failures(&db, D_OWNNOTE.id, NULL) == 0 &&
         "a row that disclosed for itself was given a second, thinner notice");
  free(l);

  /* 3e. Non-zero rc with records stored is the count-idiom misfiring, which
   * run_status() already reads as success — so it owes no disclosure. */
  add_source_row(&db, D_FAILREC.id);
  l = run_line(&db, &D_FAILREC, cap);
  printf("  %s", l);
  assert(strstr(l, " records=2 ") && strstr(l, " stored=2"));
  assert(db_fetch_failures(&db, D_FAILREC.id, NULL) == 0 &&
         "a run that stored records was reported as a fetch failure");
  free(l);

  /* 3f. A maintenance pod writes its own tables and nothing to intel_items by
   * design. Telling it that it failed to fetch would be this tool making the
   * confusion the POD verdict exists to expose (audit_registry_emit.py). */
  add_source_row(&db, D_POD.id);
  l = run_line(&db, &D_POD, cap);
  printf("  %s", l);
  assert(db_fetch_failures(&db, D_POD.id, NULL) == 0 &&
         "a maintenance pod was told it failed to fetch");
  free(l);

  /* 3g. THE SECOND TRIGGER (2026-10-23). Every host answered, so run_status()
   * says "ok" and the dashboards stay green — and the collector still reported
   * a failure and stored nothing, so the database could not tell this run from
   * an empty source. 32 of the sweep's 80 EMITS_NOTHING rows came back rc=-1,
   * `il-knesset-dates` fetching 131,618 bytes of HTML among them. The record
   * must name BOTH readings rather than pick one, and must say what to look
   * at: the body, not the status. */
  add_source_row(&db, D_ANSFAIL.id);
  l = run_line(&db, &D_ANSFAIL, cap);
  printf("  %s", l);
  { char *props = NULL;
    assert(db_fetch_failures(&db, D_ANSFAIL.id, &props) == 1 && props &&
           "a host that answered while the collector failed disclosed nothing");
    assert(strstr(props, "EVERY host answered") &&
           strstr(props, "success status") &&
           strstr(props, "published nothing") &&
           "the notice resolved an ambiguity it cannot see");
    assert(strstr(props, "READ THE BODY, not the status") &&
           "the remedy does not say where to look");
    assert(strstr(props, "\"hosts_answered\":1") && strstr(props, "\"run_rc\":-1"));
    free(props); }
  free(l);

  /* 3h. …and the control: the SAME answering host with a collector that says
   * rc=0 is an honest empty and stays silent. Without this, 3g's trigger could
   * be "any run that reached a host and stored nothing", which is every quiet
   * feed in the fleet. */
  add_source_row(&db, D_ANSEMPTY.id);
  l = run_line(&db, &D_ANSEMPTY, cap);
  printf("  %s", l);
  assert(strstr(l, " records=0 ") && strstr(l, " stored=0"));
  assert(db_fetch_failures(&db, D_ANSEMPTY.id, NULL) == 0 &&
         "an honest empty from an answering host was reported as a failure");
  free(l);

  unlink(cap);
  db_close(&db);
  printf("test_sched_runline: ok\n");
  return 0;
}
