/* test_case_roles.c — a case viewer reads; a contributor or lead writes.
 *
 * casesapi.c's can_write() used to accept ANY row in case_members, so a member
 * added to a case as `viewer` could edit the case, pin and unpin findings and
 * post to the activity trail — a read-only role in name only. Decided
 * 2026-10-09: content writes are analyst+ in the workspace, or the case's lead
 * or a contributor. Reading stays open to every workspace member.
 *
 * Every caller here is a workspace VIEWER, so the case role is the only thing
 * that can grant a write; the analyst row shows the workspace role still does. */
#include "core/casesapi.h"
#include "core/tenantapi.h"
#include "core/db.h"

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
  db_close(&db);
  printf("test_case_roles: ok\n");
  return 0;
}
