/* test_source_trust.c — the trust table is built by one joined query, looked
 * up by bsearch, and rebuilt OFF the caller's thread.
 *
 * source_trust_score() is asked once per due source per second by the
 * scheduler's dispatcher. It used to rebuild its table inline — a 30-day
 * GROUP BY over fetch_log plus an O(fetch_log sources x anomaly sources)
 * strcmp loop — under the lock every lookup takes, so once per TTL the whole
 * fleet's dispatch stalled behind SQL. Pinned here:
 *   1. source_trust_load() returns the table sorted by id, with the anomaly
 *      counts and the sources-row fields joined onto the right source;
 *   2. source_trust_find() finds every row and nothing else;
 *   3. the first source_trust_score() answers "unrated" at once and starts a
 *      background build; once it lands, the score is the table's. */
#include "core/source_trust.h"
#include "core/db.h"
#include "third_party/sqlite3.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *err = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "exec failed: %s\n  %s\n", err ? err : "?", sql);
    assert(0);
  }
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);

  /* Three sources, inserted out of id order. C has anomalies, B is
   * quarantined, A is clean. */
  exec_ok(&db,
    "INSERT OR IGNORE INTO sources(id,name,type,category) VALUES"
    " ('UNIT_TRUST_C','c','api','test'),('UNIT_TRUST_A','a','api','test'),"
    " ('UNIT_TRUST_B','b','api','test');"
    "UPDATE sources SET last_success=datetime('now') WHERE id LIKE 'UNIT_TRUST_%';"
    "UPDATE sources SET quarantined_until=datetime('now','+1 day') WHERE id='UNIT_TRUST_B';");
  exec_ok(&db,
    "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<10)"
    " INSERT INTO fetch_log(source_id,status,records_fetched,duration_ms)"
    " SELECT 'UNIT_TRUST_C', CASE WHEN i<=5 THEN 'ok' ELSE 'error' END, 10, 5 FROM n"
    " UNION ALL SELECT 'UNIT_TRUST_A','ok',10,5 FROM n"
    " UNION ALL SELECT 'UNIT_TRUST_B','ok',10,5 FROM n;");
  /* An old run outside the window must not count. */
  exec_ok(&db,
    "INSERT INTO fetch_log(source_id,timestamp,status,records_fetched)"
    " VALUES ('UNIT_TRUST_A',datetime('now','-40 days'),'error',0);");
  exec_ok(&db,
    "INSERT INTO collector_anomaly(source_id,verdict) VALUES"
    " ('UNIT_TRUST_C','status_bad'),('UNIT_TRUST_C','status_bad'),"
    " ('UNIT_TRUST_C','records_drop');");

  int n = 0;
  source_trust *T = source_trust_load(&db, &n);
  assert(T && n >= 3);
  for (int i = 1; i < n; i++)
    assert(strcmp(T[i - 1].source_id, T[i].source_id) < 0 && "table not sorted");
  const source_trust *a = source_trust_find(T, n, "UNIT_TRUST_A");
  const source_trust *b = source_trust_find(T, n, "UNIT_TRUST_B");
  const source_trust *c = source_trust_find(T, n, "UNIT_TRUST_C");
  assert(a && b && c);
  assert(source_trust_find(T, n, "UNIT_TRUST_NONE") == NULL);
  assert(source_trust_find(T, n, NULL) == NULL);
  for (int i = 0; i < n; i++)
    assert(source_trust_find(T, n, T[i].source_id) == &T[i]);
  printf("  A runs=%ld ok=%.2f anomalies=%ld | B q=%d r=%.2f | C runs=%ld "
         "ok=%.2f anomalies=%ld r=%.3f\n", a->runs_30d, a->success_rate,
         a->anomalies_30d, b->quarantined, b->reliability, c->runs_30d,
         c->success_rate, c->anomalies_30d, c->reliability);
  assert(a->runs_30d == 10 && a->success_rate == 1.0 && a->anomalies_30d == 0);
  assert(b->quarantined == 1 && b->reliability == 0.0);
  assert(c->runs_30d == 10 && fabs(c->success_rate - 0.5) < 1e-9);
  assert(c->anomalies_30d == 3 && fabs(c->anomaly_rate - 0.3) < 1e-9);
  double c_rel = c->reliability;
  assert(a->reliability > c_rel);

  /* 3. score: unrated now, the table's value once the background build lands. */
  long b0 = source_trust_builds();
  double s0 = source_trust_score(&db, "UNIT_TRUST_C");
  assert(s0 == -1.0 && "the first call must not build the table inline");
  for (int t = 0; t < 10000 && source_trust_builds() == b0; t += 10) usleep(10 * 1000);
  assert(source_trust_builds() > b0 && "background rebuild never landed");
  double s1 = source_trust_score(&db, "UNIT_TRUST_C");
  printf("  score: %.1f before the background build, %.3f after\n", s0, s1);
  assert(fabs(s1 - c_rel) < 1e-9);
  assert(source_trust_score(&db, "UNIT_TRUST_NONE") == -1.0);

  free(T);
  db_close(&db);
  printf("test_source_trust: ok\n");
  return 0;
}
