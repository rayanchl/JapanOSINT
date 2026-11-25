/* core/metricsapi.c — see metricsapi.h. */
#include "metricsapi.h"
#include "scheduler.h"
#include "../source.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* One COUNT over the last hour of fetch_log; -1 = could not be read. */
static long long count_last_hour(db_handle *db, const char *extra_where) {
  char sql[256];
  snprintf(sql, sizeof sql,
           "SELECT COUNT(*) FROM fetch_log WHERE timestamp >= datetime('now','-1 hour')%s",
           extra_where ? extra_where : "");
  sqlite3_stmt *s = NULL;
  long long n = -1;
  if (db && db->h && sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  }
  sqlite3_finalize(s);
  return n;
}

static int put(char **buf, size_t *len, size_t *cap, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
static int put(char **buf, size_t *len, size_t *cap, const char *fmt, ...) {
  for (;;) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(*buf + *len, *cap - *len, fmt, ap);
    va_end(ap);
    if (n < 0) return -1;
    if ((size_t)n < *cap - *len) { *len += (size_t)n; return 0; }
    size_t nc = *cap * 2 + (size_t)n + 1;
    char *nb = realloc(*buf, nc);
    if (!nb) return -1;
    *buf = nb; *cap = nc;
  }
}

static int gauge(char **b, size_t *l, size_t *c, const char *name,
                 const char *help, long long v) {
  return put(b, l, c, "# HELP %s %s\n# TYPE %s gauge\n%s %lld\n", name, help, name, name, v);
}

char *metricsapi_render(db_handle *db) {
  long long registered = registry_count(), scheduled = 0, pivots = 0, pods = 0;
  const source_def **all = registry_all();
  for (int i = 0; i < registered; i++) {
    const source_def *d = all[i];
    if (!d) continue;
    if (sched_is_pod(d)) { pods++; continue; }
    if (registry_is_entity_pivot(d)) pivots++;
    else if (d->update_interval_sec > 0) scheduled++;
  }
  size_t len = 0, cap = 2048;
  char *b = malloc(cap);
  if (!b) return NULL;
  b[0] = 0;
  int bad = 0;
  bad |= gauge(&b, &len, &cap, "japanosint_sources_registered",
               "Sources registered in this binary.", registered);
  bad |= gauge(&b, &len, &cap, "japanosint_sources_scheduled",
               "Registered sources that run on an interval (not pods).", scheduled);
  bad |= gauge(&b, &len, &cap, "japanosint_sources_pivots",
               "Registered entity pivots (run on demand with an entity).", pivots);
  bad |= gauge(&b, &len, &cap, "japanosint_sources_pods",
               "Registered maintenance pods (collector begins with _).", pods);
  long long runs = count_last_hour(db, NULL);
  long long fails = count_last_hour(db, " AND status='error'");
  if (runs >= 0)
    bad |= gauge(&b, &len, &cap, "japanosint_collector_runs_1h",
                 "fetch_log rows written in the last hour.", runs);
  if (fails >= 0)
    bad |= gauge(&b, &len, &cap, "japanosint_collector_failures_1h",
                 "fetch_log rows with status=error in the last hour.", fails);
  if (bad) { free(b); return NULL; }
  return b;
}
