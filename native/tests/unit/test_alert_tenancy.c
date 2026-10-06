/* test_alert_tenancy.c — which tenants' rules a row meets, and that one match
 * is one event (core/alert_eval.c, core/db.c).
 *
 *  1. Collector rows are stored under tenant 'legacy' — the shared corpus,
 *     readable by every tenant — but eval_item() matched only rules whose
 *     tenant equalled the row's, so no real tenant's rule ever fired on
 *     collected data. A 'legacy' row now meets every tenant's rules (each
 *     event under the RULE's tenant); a tenant-owned row only its owner's.
 *  2. Cross-tenant evaluation must not leak private entities: a rule may
 *     match only its own tenant's entities plus the shared ones.
 *  3. Dedup was check-then-insert with no constraint, so the ingest path and
 *     the entity sweep could both fire one (rule, item). A UNIQUE index (with
 *     a migration that removes existing duplicates) and INSERT OR IGNORE.
 *
 * Includes alert_eval.c for its statics; everything else is linked. */
#include "../../core/alert_eval.c"

#include <assert.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "SQL failed: %s\n  %s\n", e ? e : "?", sql);
    sqlite3_free(e);
    assert(0);
  }
}

static long scalar(db_handle *db, const char *sql) {
  sqlite3_stmt *s; long v = -1;
  assert(sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) v = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}

static void item(db_handle *db, const char *uid, const char *src,
                 const char *tenant) {
  char q[512];
  snprintf(q, sizeof q,
    "INSERT INTO intel_items(uid,source_id,title,fetched_at,record_type,"
    "tenant_id) VALUES('%s','%s','t',strftime('%%Y-%%m-%%dT%%H:%%M:%%SZ','now'),"
    "'news','%s')", uid, src, tenant);
  exec_ok(db, q);
}

static long events(db_handle *db, const char *rule, const char *uid,
                   const char *tenant) {
  char q[512];
  snprintf(q, sizeof q, "SELECT count(*) FROM alert_events WHERE rule_id='%s'"
           " AND item_uid='%s' AND tenant_id='%s'", rule, uid, tenant);
  return scalar(db, q);
}

/* ── 3, threaded: two connections fire the same (rule, item) at once ─── */
typedef struct { const char *dbp; const char *uid; } race_arg;
static pthread_barrier_t g_bar;
static void *racer(void *vp) {
  race_arg *a = vp;
  db_handle d = {0};
  assert(db_open(&d, a->dbp, NULL) == 0);
  sqlite3_busy_timeout(d.h, 5000);
  pthread_barrier_wait(&g_bar);
  fire(&d, "rZ", "tA", a->uid);
  db_close(&d);
  return NULL;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  assert(dbp && *dbp && "run.sh sets JO_DB to a scratch database");
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "INSERT INTO tenants(id,slug,name) VALUES ('tA','ta','A'),"
               "('tB','tb','B')");
  exec_ok(&db,
    "INSERT INTO alert_rules(id,tenant_id,name,predicate_json,dedup_window_sec)"
    " VALUES ('rA','tA','A watch','{\"source_ids\":[\"src-x\"]}',0),"
    "        ('rB','tB','B watch','{\"source_ids\":[\"src-x\"]}',0),"
    "        ('rZ','tA','race','{\"source_ids\":[\"src-z\"]}',0)");

  /* ── 1 ── */
  item(&db, "src-x|1", "src-x", "legacy");
  alert_eval_on_item(&db, "legacy", "src-x|1");
  assert(events(&db, "rA", "src-x|1", "tA") == 1 &&
         "a tenant's rule fires on the shared corpus");
  assert(events(&db, "rB", "src-x|1", "tB") == 1 &&
         "...every tenant's, each under its own tenant");
  item(&db, "src-x|2", "src-x", "tA");
  alert_eval_on_item(&db, "tA", "src-x|2");
  assert(events(&db, "rA", "src-x|2", "tA") == 1);
  assert(scalar(&db, "SELECT count(*) FROM alert_events WHERE rule_id='rB' AND"
                     " item_uid='src-x|2'") == 0 &&
         "a tenant-owned row never reaches another tenant's rule");
  printf("  legacy row -> every tenant's rules (event under the rule's tenant); "
         "tenant row -> owner only: ok\n");

  /* ── 3, sequential: evaluating again is not a second event, even with
   * dedup_window_sec=0 ── */
  alert_eval_on_item(&db, "legacy", "src-x|1");
  assert(events(&db, "rA", "src-x|1", "tA") == 1);
  assert(events(&db, "rB", "src-x|1", "tB") == 1);

  /* ── 2 ── tB's private entity on a shared row */
  exec_ok(&db,
    "INSERT INTO entities(entity_id,type,canonical,norm_key,tenant_id) VALUES"
    " ('ent-b','person','Secret B','secret b','tB'),"
    " ('ent-g','company','Shared Co','shared co',NULL)");
  exec_ok(&db,
    "INSERT INTO alert_rules(id,tenant_id,name,predicate_json,dedup_window_sec)"
    " VALUES ('rAe','tA','A probes B','{\"entity_ids\":[\"ent-b\"]}',0),"
    "        ('rBe','tB','B own','{\"entity_ids\":[\"ent-b\"]}',0),"
    "        ('rAg','tA','A shared','{\"entity_ids\":[\"ent-g\"]}',0)");
  g_checked_set = 0;              /* new rules: skip the cache's recheck delay */
  item(&db, "src-y|1", "src-y", "legacy");
  exec_ok(&db,
    "INSERT INTO entity_mentions(entity_id,item_uid,source_id,field,extractor)"
    " VALUES ('ent-b','src-y|1','src-y','body','t'),"
    "        ('ent-g','src-y|1','src-y','body','t')");
  alert_eval_on_item(&db, "legacy", "src-y|1");
  assert(events(&db, "rBe", "src-y|1", "tB") == 1);
  assert(events(&db, "rAg", "src-y|1", "tA") == 1);
  assert(scalar(&db, "SELECT count(*) FROM alert_events WHERE rule_id='rAe'") == 0
         && "tA cannot learn that tB's private entity is in the corpus");
  printf("  cross-tenant entity terms scoped to the rule's tenant: ok\n");

  /* ── 3, threaded ── */
  assert(scalar(&db, "SELECT count(*) FROM sqlite_master WHERE type='index'"
                     " AND name='ux_alert_events_rule_item'") == 1);
  const int ROUNDS = 40;
  for (int k = 0; k < ROUNDS; k++) {
    char uid[32];
    snprintf(uid, sizeof uid, "src-z|%d", k);
    item(&db, uid, "src-z", "legacy");
    pthread_barrier_init(&g_bar, NULL, 2);
    race_arg a = { dbp, uid };
    pthread_t t1, t2;
    assert(pthread_create(&t1, NULL, racer, &a) == 0);
    assert(pthread_create(&t2, NULL, racer, &a) == 0);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_barrier_destroy(&g_bar);
  }
  assert(scalar(&db, "SELECT count(*) FROM alert_events WHERE rule_id='rZ'")
         == ROUNDS && "two racing fires are one event");
  printf("  %d racing double-fires -> %d events: ok\n", ROUNDS, ROUNDS);

  /* ── 3, migration: a database that already holds duplicates ── */
  exec_ok(&db, "DROP INDEX ux_alert_events_rule_item");
  exec_ok(&db,
    "INSERT INTO alert_events(id,tenant_id,rule_id,item_uid,matched_at,"
    "suppressed,reason) VALUES"
    " ('d1','tA','rA','dup|1','2026-01-01 00:00:02',1,'storm_cap'),"
    " ('d2','tA','rA','dup|1','2026-01-01 00:00:03',0,NULL),"
    " ('d3','tA','rA','dup|1','2026-01-01 00:00:04',0,NULL)");
  exec_ok(&db, "INSERT INTO alert_deliveries(event_id,channel_idx,channel_type,"
               "status) VALUES ('d2',0,'webhook','ok'),('d3',0,'webhook','pending')");
  db_close(&db);
  assert(db_open(&db, NULL, NULL) == 0);          /* the boot migration */
  assert(scalar(&db, "SELECT count(*) FROM alert_events WHERE item_uid='dup|1'") == 1);
  assert(scalar(&db, "SELECT count(*) FROM alert_events WHERE id='d2'") == 1 &&
         "kept: the earliest deliverable row");
  assert(scalar(&db, "SELECT count(*) FROM alert_deliveries WHERE event_id='d3'") == 0);
  assert(scalar(&db, "SELECT count(*) FROM alert_deliveries WHERE event_id='d2'") == 1);
  assert(scalar(&db, "SELECT count(*) FROM sqlite_master WHERE type='index'"
                     " AND name='ux_alert_events_rule_item'") == 1);
  printf("  migration: duplicates removed (deliverable kept), unique index built: ok\n");

  db_close(&db);
  printf("test_alert_tenancy: ok\n");
  return 0;
}
