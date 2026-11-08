/* test_tenant_header.c — X-Tenant-Id must name a workspace the caller is in.
 *
 * tenant_resolve() used to fall back SILENTLY to the caller's first workspace
 * when X-Tenant-Id named one they are not a member of. Probed 2026-11-07:
 * `X-Tenant-Id: platform` answered 200 with the operator's own data, and a
 * write aimed at another workspace landed in the personal one — the client
 * was told it succeeded "there" while it happened "here". A non-member header
 * is now TENANT_NOT_MEMBER (httpd.c's tenant_or_reply maps it to 403
 * not_a_member), except on GET /api/me, which resolves leniently so a client
 * holding a stale id can learn the workspace it actually has.
 *
 * Pinned: a member header picks that workspace; no header (NULL or "") picks
 * the first; a non-member header is -403 on the strict path and the first
 * workspace on the lenient one; an unknown user's first request still
 * provisions their personal workspace even when it carries a stale header. */
#include "core/tenantapi.h"
#include "core/db.h"
#include "core/auth.h"

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

static auth_user user(const char *sub, const char *email) {
  auth_user u; memset(&u, 0, sizeof u);
  snprintf(u.id, sizeof u.id, "%s", sub);
  snprintf(u.email, sizeof u.email, "%s", email);
  return u;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "INSERT INTO tenants(id,slug,name) VALUES ('tA','ta','A'),"
               "('tB','tb','B'),('tX','tx','X')");
  exec_ok(&db, "INSERT INTO users(id,supabase_user_id,email) VALUES "
               "('uM','sub-m','m@x.test'),('uO','sub-o','o@x.test')");
  /* uM: first tA (older), then tB. uO owns tX only. */
  exec_ok(&db, "INSERT INTO memberships(user_id,tenant_id,role,created_at) VALUES "
               "('uM','tA','analyst','2026-01-01 00:00:00'),"
               "('uM','tB','viewer','2026-02-01 00:00:00'),"
               "('uO','tX','owner','2026-01-01 00:00:00')");

  auth_user m = user("sub-m", "m@x.test");
  tenant_ctx tc;

  memset(&tc, 0, sizeof tc);
  assert(tenant_resolve(&db, &m, "tB", &tc) == 0);
  assert(!strcmp(tc.tenant_id, "tB") && !strcmp(tc.role, "viewer"));
  printf("  member header tB      -> %s (%s)\n", tc.tenant_id, tc.role);

  memset(&tc, 0, sizeof tc);
  assert(tenant_resolve(&db, &m, NULL, &tc) == 0);
  assert(!strcmp(tc.tenant_id, "tA") && !strcmp(tc.role, "analyst"));
  memset(&tc, 0, sizeof tc);
  assert(tenant_resolve(&db, &m, "", &tc) == 0);
  assert(!strcmp(tc.tenant_id, "tA"));
  printf("  no header / empty     -> %s\n", tc.tenant_id);

  /* Another real workspace, and the probe's literal "platform". */
  memset(&tc, 0, sizeof tc);
  assert(tenant_resolve(&db, &m, "tX", &tc) == TENANT_NOT_MEMBER);
  assert(tenant_resolve(&db, &m, "platform", &tc) == TENANT_NOT_MEMBER);
  assert(TENANT_NOT_MEMBER == -403);
  printf("  non-member tX/platform -> %d (strict)\n", TENANT_NOT_MEMBER);

  /* GET /api/me's path: still resolves, and says which workspace it used. */
  memset(&tc, 0, sizeof tc);
  assert(tenant_resolve_lenient(&db, &m, "tX", &tc) == 0);
  assert(!strcmp(tc.tenant_id, "tA"));
  memset(&tc, 0, sizeof tc);
  assert(tenant_resolve_lenient(&db, &m, "tB", &tc) == 0);
  assert(!strcmp(tc.tenant_id, "tB"));
  printf("  /api/me lenient tX    -> %s\n", tc.tenant_id);

  /* No user at all is still 401 on both paths. */
  auth_user none; memset(&none, 0, sizeof none);
  assert(tenant_resolve(&db, &none, "tA", &tc) == -401);
  assert(tenant_resolve_lenient(&db, &none, NULL, &tc) == -401);

  /* A brand-new account's first request with a stale header: refused, but
   * its personal workspace exists afterwards, so /api/me then finds it. */
  auth_user n = user("sub-new", "new@x.test");
  memset(&tc, 0, sizeof tc);
  assert(tenant_resolve(&db, &n, "tA", &tc) == TENANT_NOT_MEMBER);
  memset(&tc, 0, sizeof tc);
  assert(tenant_resolve_lenient(&db, &n, "tA", &tc) == 0);
  assert(tc.tenant_id[0] && strcmp(tc.tenant_id, "tA") && !strcmp(tc.role, "owner"));
  { sqlite3_stmt *st; int nm = -1;
    assert(sqlite3_prepare_v2(db.h, "SELECT count(*) FROM memberships m JOIN users u "
             "ON u.id=m.user_id WHERE u.supabase_user_id='sub-new'", -1, &st, NULL) == SQLITE_OK);
    if (sqlite3_step(st) == SQLITE_ROW) nm = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    assert(nm == 1 && "provisioned once, not once per call"); }
  printf("  new account, stale hdr -> -403, then personal %s\n", tc.tenant_id);

  db_close(&db);
  printf("test_tenant_header: all passed\n");
  return 0;
}
