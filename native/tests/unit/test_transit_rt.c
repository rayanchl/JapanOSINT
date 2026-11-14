/* test_transit_rt.c — the departures board reads the GTFS-RT tables that
 * collectors/sources/gtfs_rt_jp.c writes (2026-11-12).
 *
 * WHAT IS BEING PROTECTED.
 *   1. a stop update that carries only an ARRIVAL delay still puts that delay
 *      on the departures board (GTFS-RT: the departure is that late too).
 *      Measured on the Japanese feeds: buscatch publishes arrival only, 1,093
 *      of 14,616 updates, and reading departure_delay_s alone showed none;
 *   2. an alert reaches only its OWN operator's board: route ids are an
 *      operator's namespace, and two operators both running a route "3" used
 *      to see each other's alerts;
 *   3. "_" in a route id is matched literally, not as a LIKE wildcard;
 *   4. a delay RECEIVED more than 30 minutes ago is not shown as live (under
 *      the default 6 h cadence an unrefreshed row would otherwise be). */
#include "core/transitapi.h"
#include "core/db.h"
#include "cJSON.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void exec_ok(db_handle *db, const char *q) {
  char *err = NULL;
  if (sqlite3_exec(db->h, q, NULL, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", err ? err : "?", q);
    abort();
  }
}

static int has_alert(cJSON *alerts, const char *header) {
  int n = cJSON_GetArraySize(alerts);
  for (int i = 0; i < n; i++) {
    cJSON *h = cJSON_GetObjectItem(cJSON_GetArrayItem(alerts, i), "header_text");
    if (h && cJSON_IsString(h) && !strcmp(h->valuestring, header)) return 1;
  }
  return 0;
}

int main(void) {
  db_handle db;
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "DELETE FROM gtfs_rt_trip_updates; DELETE FROM gtfs_rt_alerts;");

  /* A station whose stop is S1, served by operator A's route "3" and route
   * "1_2". Times past 24:00 are legal GTFS and keep the trips "upcoming"
   * whatever the wall clock reads. */
  exec_ok(&db,
    "INSERT OR REPLACE INTO intel_items(uid,source_id,title,fetched_at,"
    "record_type,properties) VALUES('stations|TRT1','stations','Test stop',"
    "strftime('%Y-%m-%dT%H:%M:%SZ','now'),'station','{\"stop_id\":\"TRT_S1\"}')");
  exec_ok(&db,
    "INSERT OR REPLACE INTO gtfs_trips(org_id,feed_id,trip_id,route_id,headsign)"
    " VALUES('orgA','f','T1','3','North'),('orgA','f','T2','1_2','East')");
  exec_ok(&db,
    "INSERT OR REPLACE INTO gtfs_stop_times(org_id,feed_id,trip_id,stop_sequence,"
    "stop_id,arrival_sec,departure_sec) VALUES"
    "('orgA','f','T1',5,'TRT_S1',89990,90000),"
    "('orgA','f','T2',3,'TRT_S1',91990,92000)");
  /* arrival-only update: the shape buscatch publishes */
  exec_ok(&db,
    "INSERT INTO gtfs_rt_trip_updates(org_id,trip_id,route_id,stop_id,"
    "stop_sequence,arrival_delay_s,departure_delay_s,reported_at,received_at)"
    " VALUES('orgA','T1','3','TRT_S1',5,120,NULL,0,"
    "strftime('%Y-%m-%dT%H:%M:%SZ','now'))");
  /* a stale update on the other trip: received two hours ago */
  exec_ok(&db,
    "INSERT INTO gtfs_rt_trip_updates(org_id,trip_id,route_id,stop_id,"
    "stop_sequence,arrival_delay_s,departure_delay_s,reported_at,received_at)"
    " VALUES('orgA','T2','1_2','TRT_S1',3,600,600,0,"
    "strftime('%Y-%m-%dT%H:%M:%SZ','now','-2 hours'))");
  exec_ok(&db,
    "INSERT INTO gtfs_rt_alerts(org_id,alert_id,route_ids,header_text,"
    "reported_at,received_at) VALUES"
    "('orgA','a1','[\"3\"]','A route 3 diverted',0,'x'),"
    "('orgB','b1','[\"3\"]','B route 3 cancelled',0,'x'),"
    "('orgA','a2','[\"1x2\"]','A route 1x2 wildcard bait',0,'x'),"
    "('orgA','a3','[\"1_2\"]','A route 1_2 delayed',0,'x')");

  char *out = transitapi(&db, "/station/TRT1/summary", "");
  assert(out);
  cJSON *j = cJSON_Parse(out);
  assert(j);
  cJSON *deps = cJSON_GetObjectItem(j, "departures");
  assert(deps && cJSON_GetArraySize(deps) >= 1);
  cJSON *t1 = NULL, *t2 = NULL;
  for (int i = 0; i < cJSON_GetArraySize(deps); i++) {
    cJSON *d = cJSON_GetArrayItem(deps, i);
    cJSON *tid = cJSON_GetObjectItem(d, "trip_id");
    if (tid && cJSON_IsString(tid) && !strcmp(tid->valuestring, "T1")) t1 = d;
    if (tid && cJSON_IsString(tid) && !strcmp(tid->valuestring, "T2")) t2 = d;
  }
  assert(t1 && t2);
  cJSON *dl = cJSON_GetObjectItem(t1, "delay_s");
  assert(dl && cJSON_IsNumber(dl) && dl->valueint == 120);
  printf("  an arrival-only stop update puts its delay on the departures board: ok\n");
  cJSON *dl2 = cJSON_GetObjectItem(t2, "delay_s");
  assert(!dl2 || cJSON_IsNull(dl2));
  printf("  a delay received two hours ago is not shown as live: ok\n");

  cJSON *alerts = cJSON_GetObjectItem(j, "alerts");
  assert(alerts);
  assert(has_alert(alerts, "A route 3 diverted"));
  assert(!has_alert(alerts, "B route 3 cancelled"));
  printf("  another operator's alert on a same-named route stays off the board: ok\n");
  assert(has_alert(alerts, "A route 1_2 delayed"));
  assert(!has_alert(alerts, "A route 1x2 wildcard bait"));
  printf("  \"_\" in a route id is matched literally, not as a wildcard: ok\n");

  cJSON_Delete(j);
  free(out);
  db_close(&db);
  printf("all passed\n");
  return 0;
}
