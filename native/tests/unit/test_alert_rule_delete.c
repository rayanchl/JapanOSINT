/* test_alert_rule_delete.c — deleting an alert rule keeps its inbox events.
 *
 * The events a rule raised are the inbox's record of what matched; deleting the
 * rule used to delete them too (and left their alert_deliveries rows pointing
 * at nothing). Now, inside one transaction:
 *   - the events stay, stamped with the rule's name (alert_events.rule_name),
 *     so the inbox still says which rule fired;
 *   - deliveries not yet attempted are cancelled (status 'skipped') — nothing
 *     is sent on behalf of a rule that no longer exists; sent ones are kept;
 *   - another tenant's DELETE naming the rule changes nothing. */
#include "core/alertsapi.h"
#include "core/db.h"
#include "third_party/cJSON.h"
#include "third_party/sqlite3.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql); exit(1); }
}
static int count(db_handle *db, const char *sql) {
  sqlite3_stmt *s; int n = -1;
  assert(sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
  sqlite3_finalize(s);
  return n;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "INSERT INTO tenants(id,slug,name) VALUES ('tA','ta','A'),('tB','tb','B')");
  exec_ok(&db, "INSERT INTO users(id,email) VALUES ('uA','a@x.test'),('uB','b@x.test')");
  exec_ok(&db, "INSERT INTO memberships(tenant_id,user_id,role) VALUES ('tA','uA','owner'),('tB','uB','owner')");
  exec_ok(&db, "INSERT INTO alert_rules(id,tenant_id,name) VALUES ('r1','tA','Port of Kobe watch')");
  exec_ok(&db, "INSERT INTO alert_events(id,tenant_id,rule_id,item_uid) VALUES"
               " ('e1','tA','r1','src|1'),('e2','tA','r1','src|2')");
  exec_ok(&db, "INSERT INTO alert_deliveries(event_id,channel_idx,channel_type,status) VALUES"
               " ('e1',0,'webhook','ok'),('e2',0,'webhook','pending')");

  int st = 0;
  /* Every member reads every rule, so a rule names its author. created_by was
   * stored on insert and never sent; the web client had nothing to show. */
  exec_ok(&db, "INSERT INTO alert_rules(id,tenant_id,name,created_by) VALUES ('r2','tA','Kobe berths','uA')");
  char *b = alertsapi(&db, "tA", "uA", "GET", "", "", NULL, 0, NULL, &st);
  assert(st == 200 && b && strstr(b, "\"created_by\":\"uA\"") && "the list names the author");
  assert(strstr(b, "\"created_by\":null") && "a rule with no recorded author says so");
  free(b);
  b = alertsapi(&db, "tA", "uA", "GET", "r2", "", NULL, 0, NULL, &st);
  assert(st == 200 && b && strstr(b, "\"created_by\":\"uA\"") && "and so does one rule");
  free(b);
  exec_ok(&db, "DELETE FROM alert_rules WHERE id='r2'");
  printf("  rules name their author (list and single), null when unrecorded: ok\n");

  b = alertsapi(&db, "tB", "uB", "DELETE", "r1", "", NULL, 0, NULL, &st);
  free(b);
  assert(count(&db, "SELECT COUNT(*) FROM alert_rules WHERE id='r1'") == 1 &&
         "another tenant's delete removes nothing");

  b = alertsapi(&db, "tA", "uA", "DELETE", "r1", "", NULL, 0, NULL, &st);
  free(b);
  assert(st == 204);
  assert(count(&db, "SELECT COUNT(*) FROM alert_rules WHERE id='r1'") == 0 && "the rule is gone");
  assert(count(&db, "SELECT COUNT(*) FROM alert_events WHERE rule_id='r1'") == 2 && "its events are kept");
  assert(count(&db, "SELECT COUNT(*) FROM alert_events WHERE rule_id='r1'"
                    " AND rule_name='Port of Kobe watch'") == 2 && "stamped with the rule's name");
  assert(count(&db, "SELECT COUNT(*) FROM alert_deliveries WHERE event_id='e2' AND status='skipped'") == 1 &&
         "an undelivered send is cancelled");
  assert(count(&db, "SELECT COUNT(*) FROM alert_deliveries WHERE event_id='e1' AND status='ok'") == 1 &&
         "a send that already happened is left as history");
  printf("  delete keeps events, names them, cancels pending sends, ignores other tenants: ok\n");

  b = alerteventsapi(&db, "tA", "uA", "GET", "", "", &st);
  assert(b && st == 200);
  assert(strstr(b, "\"e1\"") && strstr(b, "\"e2\"") && "the inbox still lists both events");
  assert(strstr(b, "Port of Kobe watch") && "and names the deleted rule");
  free(b);
  printf("  inbox lists the kept events under the deleted rule's name: ok\n");
  db_close(&db);
  printf("\nall passed\n");
  return 0;
}
