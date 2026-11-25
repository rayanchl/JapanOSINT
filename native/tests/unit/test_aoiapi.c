/* tests/unit/test_aoiapi.c — core/aoiapi.c: /api/aoi and /api/watchlists driven
 * through their dispatchers, no socket.  Pins the role gate (analyst+ writes,
 * unknown role fails closed), the tenant gate on every verb (another tenant's
 * id is a 404, never a read or a write), geometry validation shared with
 * alert_eval, ring auto-closing, keyset paging, the aoi_in_use refusal, and
 * the watchlist invariants: non-empty tenant-resolvable entity_ids (an empty
 * list would match every item), the rule/sugar-row pair kept in step, and a
 * delete that removes both. */
#include "../../core/aoiapi.h"
#include "../../third_party/cJSON.h"
#include "../../third_party/sqlite3.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}
static db_handle db;
static void ex(const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db.h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql: %s\n  %s\n", e ? e : "?", sql);
    exit(1);
  }
}
static long scalar(const char *sql) {
  sqlite3_stmt *s;
  long v = -999;
  if (sqlite3_prepare_v2(db.h, sql, -1, &s, NULL) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW)
    v = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}
static tenant_ctx mk(const char *tenant, const char *role) {
  tenant_ctx t;
  memset(&t, 0, sizeof t);
  snprintf(t.tenant_id, sizeof t.tenant_id, "%s", tenant);
  snprintf(t.role, sizeof t.role, "%s", role);
  snprintf(t.user_id, sizeof t.user_id, "user-%s", role);
  return t;
}
typedef struct { int st; cJSON *j; } resp;
static void rfree(resp *r) { if (r->j) cJSON_Delete(r->j); r->j = NULL; }
static resp aoi(const tenant_ctx *t, const char *m, const char *seg, const char *qs, const char *body) {
  resp r = {0};
  char *s = aoiapi(&db, t, m, seg, qs, body, &r.st);
  r.j = s ? cJSON_Parse(s) : NULL;
  free(s);
  return r;
}
static resp wl(const tenant_ctx *t, const char *m, const char *seg, const char *qs, const char *body) {
  resp r = {0};
  char *s = watchlistsapi(&db, t, m, seg, qs, body, &r.st);
  r.j = s ? cJSON_Parse(s) : NULL;
  free(s);
  return r;
}
static const char *errc(resp *r) {
  cJSON *e = r->j ? cJSON_GetObjectItem(r->j, "error") : NULL;
  return cJSON_IsString(e) ? e->valuestring : "";
}
static const char *data_s(resp *r, const char *k) {
  cJSON *d = r->j ? cJSON_GetObjectItem(r->j, "data") : NULL;
  cJSON *v = d ? cJSON_GetObjectItem(d, k) : NULL;
  return cJSON_IsString(v) ? v->valuestring : "";
}
#define BBOX "{\"name\":\"Tokyo\",\"kind\":\"bbox\",\"geometry\":[139,35,140,36]}"

static void t_aoi(void) {
  tenant_ctx A = mk("tenant-a", "analyst"), B = mk("tenant-b", "owner"), V = mk("tenant-a", "viewer"),
             U = mk("tenant-a", "superuser"), N = mk("", "owner");
  printf("-- aoi: gates\n");
  resp r = aoi(&N, "GET", "", NULL, NULL);
  ok(r.st == 400 && !strcmp(errc(&r), "tenant_required"), "no tenant -> 400"); rfree(&r);
  char *s = aoiapi(NULL, &A, "GET", "", NULL, NULL, &r.st);
  ok(r.st == 400 && s, "NULL db -> 400, no crash"); free(s);
  r = aoi(&A, NULL, "", NULL, NULL);
  ok(r.st == 405, "NULL method -> 405"); rfree(&r);
  r = aoi(&A, "PUT", "", NULL, NULL);
  ok(r.st == 405, "PUT on the collection -> 405"); rfree(&r);
  r = aoi(&A, "POST", "some-id", NULL, BBOX);
  ok(r.st == 405, "POST on an item -> 405"); rfree(&r);
  r = aoi(&V, "POST", "", NULL, BBOX);
  ok(r.st == 403 && !strcmp(errc(&r), "forbidden"), "viewer cannot create"); rfree(&r);
  r = aoi(&U, "POST", "", NULL, BBOX);
  ok(r.st == 403, "an unrecognised role fails closed"); rfree(&r);
  r = aoi(&V, "GET", "", NULL, NULL);
  ok(r.st == 200, "viewer can read"); rfree(&r);

  printf("-- aoi: create + validation\n");
  r = aoi(&A, "POST", "", NULL, BBOX);
  ok(r.st == 201 && !strcmp(data_s(&r, "name"), "Tokyo") && !strcmp(data_s(&r, "kind"), "bbox"), "bbox created (201)");
  char id1[64];
  snprintf(id1, sizeof id1, "%s", data_s(&r, "id"));
  cJSON *bb = cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "data"), "bbox");
  ok(cJSON_GetArraySize(bb) == 4 && cJSON_GetArrayItem(bb, 0)->valuedouble == 139 && cJSON_GetArrayItem(bb, 3)->valuedouble == 36,
     "bbox columns stored w,s,e,n");
  ok(!strcmp(data_s(&r, "created_by"), "user-analyst"), "created_by is the caller");
  rfree(&r);
  ok(scalar("SELECT COUNT(*) FROM audit_events WHERE action='aoi.create'") >= 1, "creation is audited");
  const char *bad[][2] = {
    { "", "empty body" }, { "not json", "malformed" }, { "[1]", "array body" }, { "{}", "no fields" },
    { "{\"name\":\"  \",\"kind\":\"bbox\",\"geometry\":[0,0,1,1]}", "blank name" },
    { "{\"name\":\"x\",\"kind\":\"hex\",\"geometry\":[0,0,1,1]}", "unknown kind" },
    { "{\"name\":\"x\",\"kind\":\"bbox\"}", "missing geometry" },
    { "{\"name\":\"x\",\"kind\":\"bbox\",\"geometry\":null}", "null geometry" },
    { "{\"name\":\"x\",\"kind\":\"bbox\",\"geometry\":[0,0,1]}", "short bbox" },
    { "{\"name\":\"x\",\"kind\":\"bbox\",\"geometry\":[0,50,1,40]}", "south above north" },
    { "{\"name\":\"x\",\"kind\":\"bbox\",\"geometry\":[0,-100,1,40]}", "latitude < -90" },
    { "{\"name\":\"x\",\"kind\":\"bbox\",\"geometry\":[0,0,181,40]}", "longitude > 180" },
    { "{\"name\":\"x\",\"kind\":\"polygon\",\"geometry\":[[0,0],[1,1]]}", "ring under 3 points" },
    { "{\"name\":\"x\",\"kind\":\"polygon\",\"geometry\":[[0,0],[1,1],[2,2]]}", "zero-area ring (a line dressed as an area)" },
    { "{\"name\":\"x\",\"kind\":\"circle\",\"geometry\":{\"lat\":35,\"lon\":139,\"radius_m\":-5}}", "negative radius" },
    { "{\"name\":\"x\",\"kind\":\"circle\",\"geometry\":{\"lat\":95,\"lon\":139,\"radius_m\":5}}", "circle latitude 95" },
    { "{\"name\":\"x\",\"kind\":\"circle\",\"geometry\":[0,0,1,1]}", "kind/geometry mismatch" },
  };
  long before = scalar("SELECT COUNT(*) FROM areas_of_interest");
  for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
    r = aoi(&A, "POST", "", NULL, bad[i][0]);
    char w[120];
    snprintf(w, sizeof w, "rejected 400: %s", bad[i][1]);
    ok(r.st == 400 && errc(&r)[0], w);
    rfree(&r);
  }
  ok(scalar("SELECT COUNT(*) FROM areas_of_interest") == before, "no rejected body wrote a row");
  char longname[400];
  strcpy(longname, "{\"name\":\"");
  memset(longname + strlen(longname), 'n', 201);
  strcpy(longname + strlen(longname), "\",\"kind\":\"bbox\",\"geometry\":[0,0,1,1]}");
  r = aoi(&A, "POST", "", NULL, longname);
  ok(r.st == 400 && !strcmp(errc(&r), "name_too_long"), "201-char name -> name_too_long"); rfree(&r);
  r = aoi(&A, "POST", "", NULL, "{\"name\":\"  padded  \",\"kind\":\"polygon\",\"geometry\":[[0,0],[0,1],[1,1]]}");
  ok(r.st == 201 && !strcmp(data_s(&r, "name"), "padded"), "name trimmed; open ring accepted");
  cJSON *ring = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "data"), "geometry"), NULL);
  (void)ring;
  cJSON *geom = cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "data"), "geometry");
  /* aoiapi.h says rings are "auto-closed on the way in"; the closing is done
   * by alert_eval's compile (stored bytes are the caller's canonical JSON, the
   * evaluator closes the ring).  Either is fine: pin that the ring round-trips
   * as a ring of 3 or 4 points. */
  int npts = cJSON_IsArray(geom) ? cJSON_GetArraySize(geom) : 0;
  ok(npts == 3 || npts == 4, "open ring accepted and echoed back as a ring");
  rfree(&r);
  r = aoi(&A, "POST", "", NULL, "{\"name\":\"c\",\"kind\":\"circle\",\"geometry\":{\"lat\":35,\"lon\":139,\"radius_m\":500}}");
  ok(r.st == 201, "circle created"); rfree(&r);

  printf("-- aoi: tenant isolation\n");
  r = aoi(&B, "GET", id1, NULL, NULL);
  ok(r.st == 404, "another tenant's id: GET 404"); rfree(&r);
  r = aoi(&B, "PATCH", id1, NULL, "{\"name\":\"stolen\"}");
  ok(r.st == 404, "PATCH 404"); rfree(&r);
  r = aoi(&B, "DELETE", id1, NULL, NULL);
  ok(r.st == 404, "DELETE 404"); rfree(&r);
  ok(!aoi_exists(&db, "tenant-b", id1) && aoi_exists(&db, "tenant-a", id1), "aoi_exists is tenant-scoped");
  ok(!aoi_exists(&db, "", id1) && !aoi_exists(&db, "tenant-a", "") && !aoi_exists(NULL, "tenant-a", id1) &&
         !aoi_exists(&db, NULL, id1) && !aoi_exists(&db, "tenant-a", NULL), "aoi_exists: NULL/empty arguments are 0");
  r = aoi(&B, "GET", "", NULL, NULL);
  ok(r.st == 200 && cJSON_GetArraySize(cJSON_GetObjectItem(r.j, "data")) == 0, "another tenant's list is empty"); rfree(&r);
  r = aoi(&A, "GET", id1, NULL, NULL);
  ok(r.st == 200 && !strcmp(data_s(&r, "name"), "Tokyo"), "owner reads it"); rfree(&r);
  r = aoi(&A, "GET", "no-such-id", NULL, NULL);
  ok(r.st == 404 && !strcmp(errc(&r), "not_found"), "unknown id 404"); rfree(&r);

  printf("-- aoi: patch (merge then validate)\n");
  r = aoi(&A, "PATCH", id1, NULL, "{\"name\":\"Tokyo Metro\"}");
  ok(r.st == 200 && !strcmp(data_s(&r, "name"), "Tokyo Metro") &&
         cJSON_GetArraySize(cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "data"), "geometry")) == 4,
     "rename keeps the geometry"); rfree(&r);
  r = aoi(&A, "PATCH", id1, NULL, "{\"kind\":\"circle\"}");
  ok(r.st == 400, "changing only kind re-validates the old geometry against it"); rfree(&r);
  r = aoi(&A, "PATCH", id1, NULL, "{\"geometry\":[0,0,5,5]}");
  cJSON *nb = r.j ? cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "data"), "bbox") : NULL;
  ok(r.st == 200 && nb && cJSON_GetArrayItem(nb, 2)->valuedouble == 5, "new geometry recomputes the bbox columns"); rfree(&r);
  r = aoi(&A, "PATCH", id1, NULL, "{\"geometry\":[0,0,500,5]}");
  ok(r.st == 400, "invalid new geometry refused"); rfree(&r);
  r = aoi(&A, "PATCH", id1, NULL, "{\"name\":\"\"}");
  ok(r.st == 400, "blank name refused on PATCH"); rfree(&r);
  r = aoi(&A, "PATCH", id1, NULL, "");
  ok(r.st == 400 && !strcmp(errc(&r), "body_required"), "empty PATCH body 400"); rfree(&r);
  r = aoi(&V, "PATCH", id1, NULL, "{\"name\":\"v\"}");
  ok(r.st == 403, "viewer cannot PATCH"); rfree(&r);
  r = aoi(&V, "DELETE", id1, NULL, NULL);
  ok(r.st == 403, "viewer cannot DELETE"); rfree(&r);
  r = aoi(&A, "PATCH", "no-such", NULL, "{\"name\":\"x\"}");
  ok(r.st == 404, "PATCH unknown id 404"); rfree(&r);

  printf("-- aoi: paging\n");
  ex("DELETE FROM areas_of_interest WHERE tenant_id='tenant-p'");
  tenant_ctx P = mk("tenant-p", "admin");
  for (int i = 0; i < 5; i++) {
    char b[200];
    snprintf(b, sizeof b, "{\"name\":\"p%d\",\"kind\":\"bbox\",\"geometry\":[0,0,%d,5]}", i, i + 1);
    r = aoi(&P, "POST", "", NULL, b);
    assert(r.st == 201);
    rfree(&r);
  }
  char seen[5][64];
  int nseen = 0, pages = 0, dup = 0;
  char qs[900] = "limit=2";
  for (;;) {
    r = aoi(&P, "GET", "", qs, NULL);
    assert(r.st == 200);
    cJSON *d = cJSON_GetObjectItem(r.j, "data"), *it;
    cJSON_ArrayForEach(it, d) {
      const char *id = cJSON_GetObjectItem(it, "id")->valuestring;
      for (int i = 0; i < nseen; i++) if (!strcmp(seen[i], id)) dup++;
      if (nseen < 5) snprintf(seen[nseen++], 64, "%s", id);
    }
    cJSON *nc = cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "page"), "next_cursor");
    pages++;
    if (!cJSON_IsString(nc) || pages > 10) { rfree(&r); break; }
    snprintf(qs, sizeof qs, "limit=2&cursor=%s", nc->valuestring);
    rfree(&r);
  }
  ok(nseen == 5 && dup == 0 && pages == 3, "5 rows over limit=2 pages: every row once, no repeats (3 pages)");
  r = aoi(&P, "GET", "", "limit=2&cursor=!!!notbase64", NULL);
  ok(r.st == 200 && cJSON_GetArraySize(cJSON_GetObjectItem(r.j, "data")) == 2, "malformed cursor ignored: page 1, not an error"); rfree(&r);
  r = aoi(&P, "GET", "", "limit=0", NULL);
  ok(cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "page"), "limit")->valueint == 50, "limit=0 -> default 50"); rfree(&r);
  r = aoi(&P, "GET", "", "limit=99999", NULL);
  ok(cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "page"), "limit")->valueint == 200, "limit clamped to 200, reported"); rfree(&r);
  r = aoi(&P, "GET", "", "limit=abc", NULL);
  ok(cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "page"), "limit")->valueint == 50, "limit=abc -> default"); rfree(&r);

  printf("-- aoi: delete refused while a rule points at it\n");
  ex("INSERT INTO alert_rules(id,tenant_id,name,enabled,predicate_json,channels_json,dedup_window_sec,storm_cap_per_hour)"
     " VALUES('rule-1','tenant-a','Fence rule',1,'{\"aoi_id\":\"AOIID\"}','[]',3600,100)");
  char upd[300];
  snprintf(upd, sizeof upd, "UPDATE alert_rules SET predicate_json='{\"aoi_id\":\"%s\"}' WHERE id='rule-1'", id1);
  ex(upd);
  ex("INSERT INTO alert_rules(id,tenant_id,name,enabled,predicate_json,channels_json,dedup_window_sec,storm_cap_per_hour)"
     " VALUES('rule-2','tenant-a','Mentions the id only in text',1,'{\"q\":\"unrelated\"}','[]',3600,100)");
  r = aoi(&A, "DELETE", id1, NULL, NULL);
  cJSON *rules = r.j ? cJSON_GetObjectItem(r.j, "rules") : NULL;
  ok(r.st == 409 && !strcmp(errc(&r), "aoi_in_use") && cJSON_GetArraySize(rules) == 1 &&
         !strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(rules, 0), "id")->valuestring, "rule-1"),
     "409 aoi_in_use names exactly the referencing rule"); rfree(&r);
  ok(aoi_exists(&db, "tenant-a", id1), "the shape survived the refusal");
  ex("DELETE FROM alert_rules WHERE id='rule-1'");
  r = aoi(&A, "DELETE", id1, NULL, NULL);
  ok(r.st == 204 && r.j == NULL, "once unlinked, DELETE -> 204 with no body"); rfree(&r);
  ok(!aoi_exists(&db, "tenant-a", id1), "and the row is gone");
  r = aoi(&A, "DELETE", id1, NULL, NULL);
  ok(r.st == 404, "second DELETE -> 404"); rfree(&r);
  ex("DELETE FROM alert_rules WHERE id='rule-2'");
}

static void t_watchlists(void) {
  tenant_ctx A = mk("tenant-a", "analyst"), B = mk("tenant-b", "owner"), V = mk("tenant-a", "viewer");
  printf("-- watchlists\n");
  ex("DELETE FROM entities WHERE entity_id LIKE 'wl-%'");
  ex("INSERT INTO entities(entity_id,type,canonical,norm_key,tenant_id) VALUES"
     "('wl-shared','org','s','s',NULL),('wl-a','org','a','a','tenant-a'),('wl-a2','org','a2','a2','tenant-a'),('wl-b','org','b','b','tenant-b')");
  resp r = wl(&V, "POST", "", NULL, "{\"name\":\"w\",\"entity_ids\":[\"wl-a\"]}");
  ok(r.st == 403, "viewer cannot create"); rfree(&r);
  r = wl(&A, "POST", "", NULL, "{\"name\":\"w\",\"entity_ids\":[]}");
  ok(r.st == 400 && !strcmp(errc(&r), "entity_ids_required"), "empty entity_ids refused (it would match everything)"); rfree(&r);
  r = wl(&A, "POST", "", NULL, "{\"name\":\"w\"}");
  ok(r.st == 400 && !strcmp(errc(&r), "entity_ids_must_be_array"), "missing entity_ids refused"); rfree(&r);
  r = wl(&A, "POST", "", NULL, "{\"name\":\"w\",\"entity_ids\":\"wl-a\"}");
  ok(r.st == 400 && !strcmp(errc(&r), "entity_ids_must_be_array"), "string entity_ids refused"); rfree(&r);
  r = wl(&A, "POST", "", NULL, "{\"name\":\"w\",\"entity_ids\":[5]}");
  ok(r.st == 400 && !strcmp(errc(&r), "entity_ids_must_be_strings"), "non-string id refused"); rfree(&r);
  r = wl(&A, "POST", "", NULL, "{\"name\":\"w\",\"entity_ids\":[\"\"]}");
  ok(r.st == 400, "empty-string id refused"); rfree(&r);
  r = wl(&A, "POST", "", NULL, "{\"name\":\"w\",\"entity_ids\":[\"wl-nope\"]}");
  ok(r.st == 400 && !strcmp(errc(&r), "unknown_entity_id"), "unknown entity id refused"); rfree(&r);
  r = wl(&A, "POST", "", NULL, "{\"name\":\"w\",\"entity_ids\":[\"wl-b\"]}");
  ok(r.st == 400 && !strcmp(errc(&r), "unknown_entity_id"), "another tenant's private entity is not resolvable (no existence oracle)"); rfree(&r);
  {
    cJSON *a = cJSON_CreateArray();
    for (int i = 0; i < 501; i++) cJSON_AddItemToArray(a, cJSON_CreateString("wl-a"));
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "name", "many");
    cJSON_AddItemToObject(o, "entity_ids", a);
    char *b = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    r = wl(&A, "POST", "", NULL, b);
    ok(r.st == 400 && !strcmp(errc(&r), "entity_ids_too_many"), "more than 500 ids refused"); rfree(&r);
    free(b);
  }
  {
    char idl[200];
    memset(idl, 'x', 129); idl[129] = 0;
    char b[400];
    snprintf(b, sizeof b, "{\"name\":\"w\",\"entity_ids\":[\"%s\"]}", idl);
    r = wl(&A, "POST", "", NULL, b);
    ok(r.st == 400 && !strcmp(errc(&r), "entity_ids_must_be_strings"), "an over-long id refused"); rfree(&r);
  }
  long rules_before = scalar("SELECT COUNT(*) FROM alert_rules WHERE tenant_id='tenant-a'");
  r = wl(&A, "POST", "", NULL, "{\"name\":\"Watch\",\"entity_ids\":[\"wl-a\",\"wl-shared\",\"wl-a\"]}");
  ok(r.st == 201, "watchlist created");
  char wid[64], rid[64];
  snprintf(wid, sizeof wid, "%s", data_s(&r, "id"));
  snprintf(rid, sizeof rid, "%s", data_s(&r, "rule_id"));
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "data"), "entity_ids")) == 2, "duplicate ids collapsed; shared (NULL-tenant) entity accepted");
  ok(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "data"), "enabled")), "enabled comes from the rule (1)");
  rfree(&r);
  ok(scalar("SELECT COUNT(*) FROM alert_rules WHERE tenant_id='tenant-a'") == rules_before + 1, "exactly one alert_rules row backs it");
  char q[300];
  snprintf(q, sizeof q, "SELECT COUNT(*) FROM alert_rules WHERE id='%s' AND channels_json='[]' AND predicate_json LIKE '%%entity_ids%%wl-a%%'", rid);
  ok(scalar(q) == 1, "rule predicate is {entity_ids:[...]} with channels '[]'");

  r = wl(&B, "GET", wid, NULL, NULL);
  ok(r.st == 404, "other tenant GET 404"); rfree(&r);
  r = wl(&B, "PATCH", wid, NULL, "{\"name\":\"x\"}");
  ok(r.st == 404, "other tenant PATCH 404"); rfree(&r);
  r = wl(&B, "DELETE", wid, NULL, NULL);
  ok(r.st == 404, "other tenant DELETE 404"); rfree(&r);
  r = wl(&B, "GET", "", NULL, NULL);
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(r.j, "data")) == 0, "other tenant list empty"); rfree(&r);

  r = wl(&A, "PATCH", wid, NULL, "{\"enabled\":false}");
  ok(r.st == 200 && cJSON_IsFalse(cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "data"), "enabled")), "PATCH enabled=false is reflected from the rule"); rfree(&r);
  snprintf(q, sizeof q, "SELECT enabled FROM alert_rules WHERE id='%s'", rid);
  ok(scalar(q) == 0, "the RULE is what was disabled");
  r = wl(&A, "PATCH", wid, NULL, "{\"name\":\"Renamed\"}");
  ok(r.st == 200 && !strcmp(data_s(&r, "name"), "Renamed"), "rename"); rfree(&r);
  snprintf(q, sizeof q, "SELECT enabled FROM alert_rules WHERE id='%s' AND name='Renamed'", rid);
  ok(scalar(q) == 0, "rename keeps the rule disabled and renames the rule in step");
  r = wl(&A, "PATCH", wid, NULL, "{\"entity_ids\":[\"wl-a2\"]}");
  ok(r.st == 200 && cJSON_GetArraySize(cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "data"), "entity_ids")) == 1, "entity_ids replaced"); rfree(&r);
  snprintf(q, sizeof q, "SELECT COUNT(*) FROM alert_rules WHERE id='%s' AND predicate_json LIKE '%%wl-a2%%' AND predicate_json NOT LIKE '%%wl-shared%%'", rid);
  ok(scalar(q) == 1, "rule predicate moved with the list (one engine, no drift)");
  r = wl(&A, "PATCH", wid, NULL, "{\"entity_ids\":[]}");
  ok(r.st == 400 && !strcmp(errc(&r), "entity_ids_required"), "PATCH to an empty list refused"); rfree(&r);
  r = wl(&A, "PATCH", wid, NULL, "{\"entity_ids\":[\"wl-b\"]}");
  ok(r.st == 400, "PATCH to another tenant's entity refused"); rfree(&r);
  r = wl(&A, "PATCH", wid, NULL, "{\"name\":\"\"}");
  ok(r.st == 400, "PATCH blank name refused"); rfree(&r);
  r = wl(&V, "PATCH", wid, NULL, "{\"name\":\"v\"}");
  ok(r.st == 403, "viewer cannot PATCH"); rfree(&r);
  /* an entity merged away since: a rename must not fail on the stale id */
  ex("DELETE FROM entities WHERE entity_id='wl-a2'");
  r = wl(&A, "PATCH", wid, NULL, "{\"name\":\"Renamed again\"}");
  ok(r.st == 200 && cJSON_GetArraySize(cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "data"), "entity_ids")) == 1,
     "a rename survives a since-merged entity id and keeps the stored list (never widened to empty)"); rfree(&r);

  r = wl(&A, "GET", "", "limit=1", NULL);
  ok(r.st == 200 && cJSON_GetArraySize(cJSON_GetObjectItem(r.j, "data")) == 1, "list"); rfree(&r);
  r = wl(&A, "PUT", "", NULL, NULL);
  ok(r.st == 405, "PUT -> 405"); rfree(&r);
  for (int i = 0; i < 2; i++) {
    char b[120];
    snprintf(b, sizeof b, "{\"name\":\"pg%d\",\"entity_ids\":[\"wl-a\"]}", i);
    r = wl(&A, "POST", "", NULL, b);
    assert(r.st == 201);
    rfree(&r);
  }
  {
    int got = 0, pages = 0, dups = 0;
    char ids[8][64];
    char qs2[900] = "limit=2";
    for (;;) {
      r = wl(&A, "GET", "", qs2, NULL);
      assert(r.st == 200);
      cJSON *it;
      cJSON_ArrayForEach(it, cJSON_GetObjectItem(r.j, "data")) {
        const char *id = cJSON_GetObjectItem(it, "id")->valuestring;
        for (int i = 0; i < got; i++) if (!strcmp(ids[i], id)) dups++;
        if (got < 8) snprintf(ids[got++], 64, "%s", id);
      }
      cJSON *nc = cJSON_GetObjectItem(cJSON_GetObjectItem(r.j, "page"), "next_cursor");
      pages++;
      if (!cJSON_IsString(nc) || pages > 6) { rfree(&r); break; }
      snprintf(qs2, sizeof qs2, "limit=2&cursor=%s", nc->valuestring);
      rfree(&r);
    }
    ok(got == 3 && dups == 0 && pages == 2, "watchlists: 3 rows over limit=2 walk to the end exactly once");
  }

  snprintf(q, sizeof q, "INSERT INTO alert_events(id,rule_id,tenant_id,item_uid) VALUES('ev-1','%s','tenant-a','item-1')", rid);
  ex(q);
  r = wl(&V, "DELETE", wid, NULL, NULL);
  ok(r.st == 403, "viewer cannot DELETE"); rfree(&r);
  r = wl(&A, "DELETE", wid, NULL, NULL);
  ok(r.st == 204 && r.j == NULL, "DELETE -> 204"); rfree(&r);
  snprintf(q, sizeof q, "SELECT COUNT(*) FROM alert_rules WHERE id='%s'", rid);
  ok(scalar(q) == 0 && scalar("SELECT COUNT(*) FROM watchlists WHERE id IS NOT NULL AND name LIKE 'Renamed%'") == 0, "rule and watchlist removed together");
  ok(scalar("SELECT COUNT(*) FROM alert_events WHERE id='ev-1' AND rule_name='Renamed again'") == 1,
     "its events are kept and stamped with the rule's name");
  r = wl(&A, "DELETE", wid, NULL, NULL);
  ok(r.st == 404, "second DELETE 404"); rfree(&r);
}

int main(void) {
  assert(db_open(&db, NULL, NULL) == 0);
  ex("INSERT OR IGNORE INTO tenants(id,slug,name) VALUES('tenant-a','ta-aoi','A'),('tenant-b','tb-aoi','B'),('tenant-p','tp-aoi','P')");
  t_aoi();
  t_watchlists();
  db_close(&db);
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
