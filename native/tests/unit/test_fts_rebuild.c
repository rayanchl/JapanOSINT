/* test_fts_rebuild.c — the intel_items_fts rebuild commits in batches and
 * resumes after an interruption.
 *
 * It used to be ONE transaction over the whole corpus (while db.c's comment
 * described per-batch commits). Holds, with a batch of 7 over 50 rows:
 *   1. a full rebuild indexes every row exactly once, carries translate.c's
 *      keywords over, stamps _fts_meta with FTS_SCHEMA_VERSION and leaves no
 *      progress/snapshot table behind;
 *   2. a rebuild that fails mid-way keeps the batches it committed (a whole
 *      number of batches, never a partial one) and a progress row naming the
 *      last committed uid; the snapshot of keywords survives with it;
 *   3. the next run RESUMES from there — no row indexed twice, none missing,
 *      keywords on both sides of the interruption intact;
 *   4. JO_FTS_REBUILD=0 leaves a pending rebuild pending. */
#define FTS_REBUILD_BATCH 7
#include "../../core/fts_schema.c"
#include "../../core/db.h"
#include "../../core/intel.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static db_handle db;

static void sql(const char *q) {
  char *e = NULL;
  if (sqlite3_exec(db.h, q, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", q);
    exit(1);
  }
}

static long long n(const char *q) { return scalar_i64(db.h, q); }

static void rerun(void) {
  pthread_mutex_lock(&g_mu); g_ran = 0; pthread_mutex_unlock(&g_mu);
  fts_schema_migrate(&db);
}

static void mark_stale(void) {
  sql("UPDATE _fts_meta SET version=2 WHERE name='intel_items_fts'");
}

static void check_complete(const char *what) {
  assert(n("SELECT count(*) FROM intel_items_fts") == 50 && "every row, once");
  assert(n("SELECT count(*) FROM intel_items_fts_uid_map") == 50);
  assert(n("SELECT count(*) FROM intel_items_fts_uid_map m JOIN intel_items_fts f"
           " ON f.rowid=m.rowid AND f.uid=m.uid") == 50 && "map points at its row");
  assert(n("SELECT count(*) FROM intel_items_fts WHERE keywords='kw-r03'") == 1);
  assert(n("SELECT count(*) FROM intel_items_fts WHERE keywords='kw-r47'") == 1);
  assert(n("SELECT version FROM _fts_meta WHERE name='intel_items_fts'") ==
         FTS_SCHEMA_VERSION);
  assert(!table_exists(db.h, "_fts_rebuild") && !table_exists(db.h, "_fts_rebuild_kw"));
  assert(n("SELECT count(*) FROM intel_items_fts WHERE intel_items_fts MATCH "
           "'\"観測\"'") == 50 && "content is searchable");
  printf("  %s: 50/50 indexed once, keywords kept, meta=v%d, no leftovers: ok\n",
         what, FTS_SCHEMA_VERSION);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  assert(db_open(&db, NULL, NULL) == 0);

  intel_sink k = intel_sink_make(&db, "rb", NULL);
  for (int i = 0; i < 50; i++) {
    char key[16], title[64];
    snprintf(key, sizeof key, "r%02d", i);
    snprintf(title, sizeof title, "観測点 %d", i);
    intel_item it = {0};
    it.remote_key = key; it.title = title;
    assert(k.emit(&k, &it) == 1);
  }
  intel_sink_free(&k);
  sql("UPDATE intel_items_fts SET keywords='kw-r03' WHERE uid='rb|r03'");
  sql("UPDATE intel_items_fts SET keywords='kw-r47' WHERE uid='rb|r47'");

  /* 1 */
  mark_stale();
  rerun();
  check_complete("full rebuild");

  /* 2: fail on the uid_map insert of r30 — batches r00..r06, …, r21..r27
   * commit (4 x 7 = 28 rows); the batch holding r30 rolls back. */
  mark_stale();
  sql("CREATE TEMP TRIGGER boom BEFORE INSERT ON intel_items_fts_uid_map "
      "WHEN NEW.uid='rb|r30' BEGIN SELECT RAISE(ABORT,'injected'); END");
  rerun();
  assert(table_exists(db.h, "_fts_rebuild") && "progress row kept");
  long long kept = n("SELECT count(*) FROM intel_items_fts_uid_map");
  assert(kept == 28 && "whole batches only");
  assert(n("SELECT count(*) FROM intel_items_fts") == 28);
  char *last = NULL;
  {
    sqlite3_stmt *s;
    assert(sqlite3_prepare_v2(db.h, "SELECT last_uid FROM _fts_rebuild", -1, &s, NULL) == 0);
    assert(sqlite3_step(s) == SQLITE_ROW);
    last = strdup((const char *)sqlite3_column_text(s, 0));
    sqlite3_finalize(s);
  }
  assert(strcmp(last, "rb|r27") == 0 && "progress names the last committed uid");
  assert(n("SELECT count(*) FROM _fts_rebuild_kw") == 2 && "keyword snapshot survives");
  free(last);
  printf("  interrupted at r30: 28 rows in 4 committed batches, progress=rb|r27: ok\n");

  /* 4 */
  sql("DROP TRIGGER boom");
  setenv("JO_FTS_REBUILD", "0", 1);
  rerun();
  assert(n("SELECT count(*) FROM intel_items_fts") == 28 && table_exists(db.h, "_fts_rebuild"));
  unsetenv("JO_FTS_REBUILD");
  printf("  JO_FTS_REBUILD=0 leaves the pending rebuild pending: ok\n");

  /* 3 */
  rerun();
  check_complete("resumed rebuild");

  db_close(&db);
  printf("\nall passed\n");
  return 0;
}
