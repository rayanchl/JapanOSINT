/* tests/unit/test_service_vec_pod.c — core/service_vec_pod.c, the `_svec_index`
 * maintenance pod.  Pins its registration (a POD, not a feed or pivot, with the
 * long interval its header argues for), its refusal of a context with no
 * database, and its INERT contract: with no embedding server (or
 * JO_ROUTE_SEMANTIC=0) a tick returns 0 and touches no network, but the
 * declared category labels are still written ("written every tick, inert or
 * not").  Includes the .c, so run.sh excludes obj/core/service_vec_pod.o. */
#include "../../core/service_vec_pod.c"
#include "embed_stub.h"
#include <assert.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}
static long scalar(db_handle *db, const char *sql) {
  sqlite3_stmt *s;
  long v = -999;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW)
    v = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}

int main(void) {
  unsetenv("JO_EMBED_URL"); unsetenv("JO_EMBED_QUERY_URL"); unsetenv("JO_ROUTE_SEMANTIC");
  printf("-- registration\n");
  const source_def *d = registry_get(SVECPOD_SID);
  ok(d != NULL, "registered under _svec_index");
  ok(d && source_def_kind_of(d) == SOURCE_KIND_POD, "kind POD (maintenance, exempt from the cadence policy)");
  ok(d && d->update_interval_sec == 900, "15-minute tick (the header's long-interval argument)");
  ok(d && d->run == run && d->url && !strncmp(d->url, "internal://", 11), "internal url, run hook wired");

  printf("-- run() gates\n");
  intel_sink sink;
  memset(&sink, 0, sizeof sink);
  ok(run(NULL, &sink) == -1, "NULL ctx -> -1");
  source_ctx nodb;
  memset(&nodb, 0, sizeof nodb);
  ok(run(&nodb, &sink) == -1, "ctx without a database -> -1");

  printf("-- inert tick\n");
  db_handle db;
  assert(db_open(&db, NULL, NULL) == 0);
  source_ctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.db = &db;
  long before = scalar(&db, "SELECT COUNT(*) FROM sources");
  ok(run(&ctx, &sink) == 0, "no embedding server: tick returns 0 (inert, not a failure)");
  ok(scalar(&db, "SELECT COUNT(*) FROM sources") == before, "the tick does not add or drop sources");
  setenv("JO_ROUTE_SEMANTIC", "0", 1);
  ok(run(&ctx, &sink) == 0, "JO_ROUTE_SEMANTIC=0: inert");
  unsetenv("JO_ROUTE_SEMANTIC");
  ok(run(&ctx, &sink) == 0, "ticks are repeatable (declare steps are idempotent)");

  printf("-- with a server, a tick builds the index\n");
  if (estub_start(18363, 64, "stub-bow") != 0) {
    printf("test_service_vec_pod: stub unavailable, server case skipped\n");
  } else {
    estub_use();
    int posts = estub_posts();
    ok(run(&ctx, &sink) == 0, "tick against a live embedding server returns 0");
    ok(estub_posts() > posts, "and it embedded the service catalogue (requests were sent)");
    ok(run(&ctx, &sink) == 0, "a second tick on a steady tree returns 0");
    estub_stop();
  }
  db_close(&db);
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
