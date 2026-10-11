/* test_evidence_chain.c — the evidence custody chain cannot fork, and its tail
 * read is an index seek.
 *
 * evidence_capture() read the chain tail with `ORDER BY chain_seq DESC LIMIT 1`
 * — no index on chain_seq, so a scan + sort of the table per capture — and
 * serialised tail-read/insert with a PROCESS mutex only. A second process on
 * the same database file (the CLI's --run beside the server) could read the
 * same tail and write the same chain_seq: a fork evidence_verify() reports as
 * tampering forever, since rows are never deleted. Pinned here:
 *   - the tail query uses idx_evidence_chain (EXPLAIN QUERY PLAN);
 *   - UNIQUE(chain_seq) exists, so a duplicate position is refused;
 *   - four forked PROCESSES capturing concurrently produce one unbroken chain
 *     that verifies, with every position taken exactly once. */
#include "../../core/evidence.c"
#include "core/db.h"
#include "third_party/cJSON.h"

#include <assert.h>
#include <stdlib.h>
#include <sys/wait.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql); exit(1); }
}
static long scalar(db_handle *db, const char *sql) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}

#define PROCS 4
#define PER   15

static int child(const char *path, int id) {
  db_handle own = {0};
  if (db_attach(&own, path) != 0) return 10;
  for (int i = 0; i < PER; i++) {
    char body[64], uid[32];
    snprintf(body, sizeof body, "proc %d capture %d", id, i);
    snprintf(uid, sizeof uid, "it-%d-%d", id, i);
    int rc = evidence_capture(&own, uid, "EV_TEST_SRC", "https://x.test/a",
                              "GET", NULL, 200, NULL, body, strlen(body),
                              "text/plain");
    if (rc != EV_CAPTURED) { fprintf(stderr, "child %d capture %d rc=%d\n", id, i, rc);
                             db_close(&own); return 11; }
  }
  db_close(&own);
  return 0;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  char evdir[] = "/tmp/jo-ev-chain-XXXXXX";
  assert(mkdtemp(evdir));
  setenv("JO_EVIDENCE_DIR", evdir, 1);
  unsetenv("JO_NO_EVIDENCE");

  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "INSERT INTO sources(id,name,type,category,capture_evidence)"
               " VALUES('EV_TEST_SRC','t','api','test',1)");

  /* One capture in this process creates the UNIQUE index lazily. */
  assert(evidence_capture(&db, "it-0", "EV_TEST_SRC", "https://x.test/0", "GET",
                          NULL, 200, NULL, "seed", 4, "text/plain") == EV_CAPTURED);
  assert(scalar(&db, "SELECT COUNT(*) FROM sqlite_master WHERE type='index'"
                     " AND name='idx_evidence_chain_seq_uniq'") == 1);

  /* The tail read is an index seek, not a scan + temp b-tree sort. */
  {
    sqlite3_stmt *s; int uses_idx = 0, sorts = 0;
    assert(sqlite3_prepare_v2(db.h,
      "EXPLAIN QUERY PLAN SELECT row_hash,chain_seq FROM evidence "
      "WHERE row_hash IS NOT NULL ORDER BY chain_seq DESC LIMIT 1",
      -1, &s, NULL) == SQLITE_OK);
    while (sqlite3_step(s) == SQLITE_ROW) {
      const char *d = (const char *)sqlite3_column_text(s, 3);
      if (d && strstr(d, "idx_evidence_chain")) uses_idx = 1;
      if (d && strstr(d, "TEMP B-TREE")) sorts = 1;
    }
    sqlite3_finalize(s);
    assert(uses_idx && !sorts && "chain tail reads idx_evidence_chain");
  }

  /* A second row at an existing position is refused outright. */
  {
    char *e = NULL;
    int rc = sqlite3_exec(db.h,
      "INSERT INTO evidence(id,item_uid,source_id,content_sha256,blob_path,"
      "prev_hash,row_hash,chain_seq) VALUES('dup','x','EV_TEST_SRC','ff','p',"
      "'GENESIS','bogus',1)", NULL, NULL, &e);
    sqlite3_free(e);
    assert((rc & 0xFF) == SQLITE_CONSTRAINT && "UNIQUE(chain_seq) refuses a fork");
  }

  /* Four PROCESSES, each with its own g_chain mutex — the in-process lock
   * cannot help here, only the database lock and the UNIQUE index can. */
  db_close(&db);
  pid_t pids[PROCS];
  for (int p = 0; p < PROCS; p++) {
    pids[p] = fork();
    assert(pids[p] >= 0);
    if (pids[p] == 0) _exit(child(dbp, p + 1));
  }
  for (int p = 0; p < PROCS; p++) {
    int st = 0;
    assert(waitpid(pids[p], &st, 0) == pids[p]);
    assert(WIFEXITED(st) && WEXITSTATUS(st) == 0 && "every child capture lands");
  }

  assert(db_attach(&db, dbp) == 0);
  long n = scalar(&db, "SELECT COUNT(*) FROM evidence WHERE row_hash IS NOT NULL");
  assert(n == 1 + PROCS * PER);
  assert(scalar(&db, "SELECT COUNT(DISTINCT chain_seq) FROM evidence") == n);
  assert(scalar(&db, "SELECT MAX(chain_seq) FROM evidence") == n);

  char *js = evidence_verify(&db);
  assert(js);
  cJSON *o = cJSON_Parse(js);
  free(js);
  assert(o);
  assert(cJSON_IsTrue(cJSON_GetObjectItem(o, "ok")) && "the concurrent chain verifies");
  cJSON_Delete(o);
  db_close(&db);

  char cmd[128];
  snprintf(cmd, sizeof cmd, "rm -rf %s", evdir);
  if (system(cmd) != 0) { /* scratch dir; best effort */ }
  printf("test_evidence_chain: ok (%ld chained rows from %d processes)\n", n, PROCS);
  return 0;
}
