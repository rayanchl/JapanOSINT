/* test_case_roles.c — a case viewer reads; a contributor or lead writes.
 *
 * casesapi.c's can_write() used to accept ANY row in case_members, so a member
 * added to a case as `viewer` could edit the case, pin and unpin findings and
 * post to the activity trail — a read-only role in name only. Decided
 * 2026-10-09: content writes are analyst+ in the workspace, or the case's lead
 * or a contributor. Reading stays open to every workspace member.
 *
 * Every caller here is a workspace VIEWER, so the case role is the only thing
 * that can grant a write; the analyst row shows the workspace role still does.
 *
 * The LIST (GET /api/cases) carries the caller's role on each case as
 * `my_case_role`, the same value the detail route carries, so a client can
 * offer a lead or contributor the status change can_write() accepts. It used
 * not to, and the web list hid the status buttons from every workspace viewer.
 * list_roles() checks it per caller AND per case: one user holds a different
 * role on each case, so a value read from the wrong roster row would show. */
#include "core/casesapi.h"
#include "core/tenantapi.h"
#include "core/db.h"
#include "third_party/cJSON.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  int rc = sqlite3_exec(db->h, sql, NULL, NULL, &e);
  if (rc != SQLITE_OK) fprintf(stderr, "%s: %s\n", sql, e ? e : "?");
  assert(rc == SQLITE_OK);
  sqlite3_free(e);
}

static tenant_ctx as(const char *user, const char *role) {
  tenant_ctx t; memset(&t, 0, sizeof t);
  snprintf(t.tenant_id, sizeof t.tenant_id, "tA");
  snprintf(t.user_id, sizeof t.user_id, "%s", user);
  snprintf(t.role, sizeof t.role, "%s", role);
  return t;
}

/* PATCH the case summary and POST a comment; returns the two statuses. */
static void try_write(db_handle *db, const char *user, const char *role,
                      int *patch_st, int *comment_st, int *read_st) {
  tenant_ctx t = as(user, role);
  char *b = casesapi(db, &t, "PATCH", "case-a", "", "{\"summary\":\"edited\"}", NULL, patch_st);
  free(b);
  b = casesapi(db, &t, "POST", "case-a", "activity", "{\"body\":\"a comment\"}", NULL, comment_st);
  free(b);
  b = casesapi(db, &t, "GET", "case-a", "", NULL, NULL, read_st);
  free(b);
}

/* GET /api/cases as `user`; asserts each listed case's my_case_role (NULL =
 * JSON null, i.e. off that case's roster) and that the detail route agrees. */
static void list_roles(db_handle *db, const char *user, const char *role,
                       const char *want_a, const char *want_b, const char *want_c) {
  tenant_ctx t = as(user, role);
  int st = 0;
  char *b = casesapi(db, &t, "GET", "", "", NULL, NULL, &st);
  assert(st == 200 && b);
  cJSON *j = cJSON_Parse(b);
  cJSON *data = cJSON_GetObjectItem(j, "data");
  assert(cJSON_IsArray(data) && cJSON_GetArraySize(data) == 3);
  const char *want[3] = { want_a, want_b, want_c };
  const char *ids[3] = { "case-a", "case-b", "case-c" };
  int seen = 0;
  cJSON *row;
  cJSON_ArrayForEach(row, data) {
    const char *id = cJSON_GetObjectItem(row, "id")->valuestring;
    cJSON *mine = cJSON_GetObjectItem(row, "my_case_role");
    assert(mine && "every row carries the field, null when off the roster");
    for (int i = 0; i < 3; i++) {
      if (strcmp(id, ids[i])) continue;
      seen++;
      if (want[i]) assert(cJSON_IsString(mine) && !strcmp(mine->valuestring, want[i]));
      else         assert(cJSON_IsNull(mine));
      /* the detail route is the reference: the list must say what it says */
      int ds = 0;
      char *d = casesapi(db, &t, "GET", id, "", NULL, NULL, &ds);
      cJSON *dj = cJSON_Parse(d);
      cJSON *dm = cJSON_GetObjectItem(cJSON_GetObjectItem(dj, "data"), "my_case_role");
      assert(ds == 200 && dm);
      assert(cJSON_IsNull(dm) ? cJSON_IsNull(mine)
                              : (cJSON_IsString(mine) && !strcmp(dm->valuestring, mine->valuestring)));
      cJSON_Delete(dj); free(d);
    }
  }
  assert(seen == 3);
  printf("  list as %-3s: case-a %-11s case-b %-11s case-c %s\n", user[0] ? user : "-",
         want_a ? want_a : "null", want_b ? want_b : "null", want_c ? want_c : "null");
  cJSON_Delete(j);
  free(b);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "INSERT INTO tenants(id,slug,name) VALUES ('tA','ta','A')");
  exec_ok(&db, "INSERT INTO users(id,email) VALUES ('uL','l@x.test'),('uC','c@x.test'),"
               "('uV','v@x.test'),('uO','o@x.test'),('uA','a@x.test')");
  exec_ok(&db, "INSERT INTO memberships(tenant_id,user_id,role) VALUES ('tA','uL','viewer'),"
               "('tA','uC','viewer'),('tA','uV','viewer'),('tA','uO','viewer'),('tA','uA','analyst')");
  exec_ok(&db, "INSERT INTO cases(id,tenant_id,name) VALUES ('case-a','tA','a')");
  exec_ok(&db, "INSERT INTO case_members(case_id,user_id,role) VALUES ('case-a','uL','lead'),"
               "('case-a','uC','contributor'),('case-a','uV','viewer')");

  struct { const char *user, *role, *what; int write; } rows[] = {
    { "uL", "viewer",  "case lead",                      1 },
    { "uC", "viewer",  "case contributor",               1 },
    { "uV", "viewer",  "case VIEWER",                    0 },
    { "uO", "viewer",  "workspace viewer off the roster", 0 },
    { "uA", "analyst", "workspace analyst off the roster", 1 },
  };
  for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
    int ps = 0, cs = 0, rs = 0;
    try_write(&db, rows[i].user, rows[i].role, &ps, &cs, &rs);
    printf("  %-34s PATCH %d, comment %d, read %d\n", rows[i].what, ps, cs, rs);
    assert(rs == 200 && "every workspace member reads the case");
    if (rows[i].write) {
      assert(ps == 200 && (cs == 200 || cs == 201));
    } else {
      assert(ps == 403 && cs == 403 && "a read-only caller is refused, not ignored");
    }
  }
  printf("  a case viewer reads only; lead and contributor write: ok\n");

  /* The list. Each user holds a DIFFERENT role on each case (and none on
   * some), so a role read from the wrong case or the wrong user shows up. */
  exec_ok(&db, "INSERT INTO cases(id,tenant_id,name) VALUES ('case-b','tA','b'),('case-c','tA','c')");
  exec_ok(&db, "INSERT INTO case_members(case_id,user_id,role) VALUES "
               "('case-b','uL','viewer'),('case-b','uC','lead'),('case-b','uV','contributor'),"
               "('case-c','uV','lead')");
  /* another workspace's case, with a roster row for one of these users: never listed */
  exec_ok(&db, "INSERT INTO tenants(id,slug,name) VALUES ('tB','tb','B')");
  exec_ok(&db, "INSERT INTO cases(id,tenant_id,name) VALUES ('case-x','tB','x')");
  exec_ok(&db, "INSERT INTO case_members(case_id,user_id,role) VALUES ('case-x','uL','lead')");
  list_roles(&db, "uL", "viewer",  "lead",        "viewer",      NULL);
  list_roles(&db, "uC", "viewer",  "contributor", "lead",        NULL);
  list_roles(&db, "uV", "viewer",  "viewer",      "contributor", "lead");
  list_roles(&db, "uO", "viewer",  NULL,          NULL,          NULL);
  list_roles(&db, "uA", "analyst", NULL,          NULL,          NULL);
  list_roles(&db, "",   "viewer",  NULL,          NULL,          NULL);   /* no user id */
  printf("  the list carries each caller's role on each case: ok\n");
  db_close(&db);
  printf("test_case_roles: ok\n");
  return 0;
}
