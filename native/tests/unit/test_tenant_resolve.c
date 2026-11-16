/* test_tenant_resolve.c — workspace resolution and membership management
 * (core/tenantapi.c): API-04, API-05, API-06 and API-13 of the 2026-11-14
 * audit.
 *
 *  - API-13  an X-Tenant-Id that is not one of the caller's memberships is -403
 *            (HTTP 403), never a silent fall-back to their first workspace; an
 *            ABSENT header keeps the fall-back.
 *  - API-04  invites are claimed on the CURRENT token's email claim, and only
 *            when that claim is verified. users.email follows the token.
 *  - API-05  "invite an existing user" adds at once only when that account has
 *            PROVEN the address; a squatter that merely claimed it does not
 *            receive the membership — the invite is queued instead.
 *  - API-06  only an owner (or the member themself) may change or remove an
 *            owner membership.
 *
 * Tokens are real HS256 JWTs run through auth_check(), because the verified
 * flag tenant_resolve() consults is the thread-local that auth.c sets while
 * verifying the token — there is no other way to set it. */
#include "../../core/tenantapi.c"

#include "../../core/auth.h"
#include <assert.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

static const char *SECRET = "tenant-resolve-test-secret";

static char *b64u(const unsigned char *in, size_t n) {
  static const char A[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  char *o = malloc(4 * ((n + 2) / 3) + 1);
  size_t j = 0;
  for (size_t i = 0; i < n; i += 3) {
    unsigned v = (unsigned)in[i] << 16;
    if (i + 1 < n) v |= (unsigned)in[i + 1] << 8;
    if (i + 2 < n) v |= in[i + 2];
    o[j++] = A[(v >> 18) & 63];
    o[j++] = A[(v >> 12) & 63];
    if (i + 1 < n) o[j++] = A[(v >> 6) & 63];
    if (i + 2 < n) o[j++] = A[v & 63];
  }
  o[j] = 0;
  return o;
}

/* Mint a token, verify it through auth_check() (which sets the thread-local
 * verified flag), then resolve the tenant as the request path does. */
static int resolve(db_handle *db, const char *sub, const char *email,
                   int verified, const char *xt, tenant_ctx *t, auth_user *uo) {
  char pl[512];
  snprintf(pl, sizeof pl,
           "{\"sub\":\"%s\",\"email\":\"%s\",\"email_verified\":%s,"
           "\"aud\":\"authenticated\",\"exp\":%ld}",
           sub, email, verified ? "true" : "false", (long)time(NULL) + 600);
  const char *hdr = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
  char *h = b64u((const unsigned char *)hdr, strlen(hdr));
  char *p = b64u((const unsigned char *)pl, strlen(pl));
  char si[1024]; snprintf(si, sizeof si, "%s.%s", h, p);
  unsigned char mac[32]; unsigned int ml = 32;
  HMAC(EVP_sha256(), SECRET, (int)strlen(SECRET), (unsigned char *)si, strlen(si), mac, &ml);
  char *sg = b64u(mac, 32);
  char bearer[2048]; snprintf(bearer, sizeof bearer, "Bearer %s.%s", si, sg);
  free(h); free(p); free(sg);
  auth_user u; memset(&u, 0, sizeof u);
  assert(auth_check(bearer, &u) == AUTH_ALLOW);
  if (uo) *uo = u;
  return tenant_resolve(db, &u, xt, t);
}

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  int rc = sqlite3_exec(db->h, sql, NULL, NULL, &e);
  if (rc != SQLITE_OK) fprintf(stderr, "%s: %s\n", sql, e ? e : "?");
  assert(rc == SQLITE_OK);
  sqlite3_free(e);
}
static long scalar(db_handle *db, const char *sql) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}
static tenant_ctx ctx(const char *tid, const char *uid, const char *role) {
  tenant_ctx t; memset(&t, 0, sizeof t);
  snprintf(t.tenant_id, sizeof t.tenant_id, "%s", tid);
  snprintf(t.user_id, sizeof t.user_id, "%s", uid);
  snprintf(t.role, sizeof t.role, "%s", role);
  return t;
}
static int members(db_handle *db, const tenant_ctx *t, const char *method,
                   const char *seg, const char *act, const char *body, char **out) {
  int st = 0;
  char *r = tenantapi_members(db, t, method, seg, act, body, &st);
  if (out) *out = r; else free(r);
  return st;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  setenv("SUPABASE_JWT_SECRET", SECRET, 1);
  unsetenv("SUPABASE_URL");
  unsetenv("JO_JWT_ISS");
  auth_init();
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);

  /* ── API-13: a foreign X-Tenant-Id is refused ───────────────────────── */
  tenant_ctx t1, t2;
  assert(resolve(&db, "sub-1", "one@x.test", 1, NULL, &t1, NULL) == 0);
  assert(resolve(&db, "sub-2", "two@x.test", 1, NULL, &t2, NULL) == 0);
  assert(strcmp(t1.tenant_id, t2.tenant_id) != 0);
  tenant_ctx tx;
  assert(resolve(&db, "sub-1", "one@x.test", 1, t1.tenant_id, &tx, NULL) == 0 &&
         !strcmp(tx.tenant_id, t1.tenant_id) && "own workspace id is accepted");
  assert(resolve(&db, "sub-1", "one@x.test", 1, t2.tenant_id, &tx, NULL) == -403 &&
         "another user's workspace id is a 403, not a fall-back");
  assert(resolve(&db, "sub-1", "one@x.test", 1, "no-such-tenant", &tx, NULL) == -403);
  assert(resolve(&db, "sub-1", "one@x.test", 1, "", &tx, NULL) == 0 &&
         !strcmp(tx.tenant_id, t1.tenant_id) && "an empty header keeps the fall-back");
  printf("  API-13 foreign X-Tenant-Id: ok\n");

  /* ── API-04: invites follow the CURRENT verified claim ──────────────── */
  exec_ok(&db, "INSERT INTO tenants(id,slug,name) VALUES ('tV','tv','V')");
  exec_ok(&db, "INSERT INTO tenant_invites(id,tenant_id,email,role) "
               "VALUES ('i1','tV','victim@corp.test','admin')");
  tenant_ctx a;
  /* attacker signs up UNVERIFIED as the victim's address: nothing is claimed */
  assert(resolve(&db, "sub-att", "victim@corp.test", 0, NULL, &a, NULL) == 0);
  assert(scalar(&db, "SELECT COUNT(*) FROM memberships m JOIN users u ON u.id=m.user_id "
                     "WHERE u.supabase_user_id='sub-att' AND m.tenant_id='tV'") == 0);
  /* ...then verifies a DIFFERENT address: the stored victim address must not be
   * matched against the new verified flag */
  assert(resolve(&db, "sub-att", "attacker@evil.test", 1, NULL, &a, NULL) == 0);
  assert(scalar(&db, "SELECT COUNT(*) FROM memberships m JOIN users u ON u.id=m.user_id "
                     "WHERE u.supabase_user_id='sub-att' AND m.tenant_id='tV'") == 0 &&
         "a verified OTHER address does not claim the victim's invite");
  assert(scalar(&db, "SELECT COUNT(*) FROM users WHERE supabase_user_id='sub-att' "
                     "AND email='attacker@evil.test'") == 1 && "users.email follows the token");
  assert(scalar(&db, "SELECT COUNT(*) FROM tenant_invites WHERE id='i1'") == 1);
  /* the real owner of the address, verified, claims it */
  tenant_ctx v;
  assert(resolve(&db, "sub-vic", "Victim@Corp.test", 1, NULL, &v, NULL) == 0);
  assert(scalar(&db, "SELECT COUNT(*) FROM memberships m JOIN users u ON u.id=m.user_id "
                     "WHERE u.supabase_user_id='sub-vic' AND m.tenant_id='tV' "
                     "AND m.role='admin'") == 1 && "a verified claim consumes the invite");
  assert(scalar(&db, "SELECT COUNT(*) FROM tenant_invites WHERE id='i1'") == 0);
  printf("  API-04 invite claim on the current verified email: ok\n");

  /* ── API-05: invite an existing account only when it proved the address ── */
  tenant_ctx owner = ctx("tV", "uOwner", "owner");
  exec_ok(&db, "INSERT INTO users(id,supabase_user_id,email) VALUES "
               "('uOwner','s-own','owner@corp.test'),('uAdmin','s-adm','admin@corp.test'),"
               "('uOwner2','s-own2','owner2@corp.test')");
  exec_ok(&db, "INSERT INTO memberships(tenant_id,user_id,role) VALUES "
               "('tV','uOwner','owner'),('tV','uAdmin','admin'),('tV','uOwner2','owner')");
  tenant_ctx admin = ctx("tV", "uAdmin", "admin");
  /* squatter: stored email, never verified */
  assert(resolve(&db, "sub-sq", "squat@corp.test", 0, NULL, &a, NULL) == 0);
  char *r = NULL;
  assert(members(&db, &admin, "POST", "invite", "",
                 "{\"email\":\"squat@corp.test\",\"role\":\"analyst\"}", &r) == 200);
  assert(strstr(r, "\"invited\"") && "an unverified account gets a queued invite");
  free(r);
  assert(scalar(&db, "SELECT COUNT(*) FROM memberships m JOIN users u ON u.id=m.user_id "
                     "WHERE u.supabase_user_id='sub-sq' AND m.tenant_id='tV'") == 0);
  /* a verified account is added at once */
  assert(resolve(&db, "sub-ok", "good@corp.test", 1, NULL, &a, NULL) == 0);
  assert(members(&db, &admin, "POST", "invite", "",
                 "{\"email\":\"good@corp.test\",\"role\":\"analyst\"}", &r) == 200);
  assert(strstr(r, "\"added\""));
  free(r);
  printf("  API-05 invite existing user: verified only: ok\n");

  /* ── API-06: owner is a boundary ─────────────────────────────────────── */
  assert(members(&db, &admin, "PATCH", "uOwner", "role", "{\"role\":\"viewer\"}", NULL) == 403 &&
         "an admin cannot demote an owner");
  assert(members(&db, &admin, "DELETE", "uOwner", "", NULL, NULL) == 403 &&
         "an admin cannot remove an owner");
  assert(scalar(&db, "SELECT COUNT(*) FROM memberships WHERE tenant_id='tV' "
                     "AND role='owner'") == 2);
  assert(members(&db, &admin, "PATCH", "uAdmin", "role", "{\"role\":\"viewer\"}", NULL) == 200 &&
         "an admin may still change a non-owner");
  assert(members(&db, &owner, "PATCH", "uOwner2", "role", "{\"role\":\"admin\"}", NULL) == 200 &&
         "an owner may demote another owner");
  assert(members(&db, &owner, "PATCH", "uOwner", "role", "{\"role\":\"viewer\"}", NULL) == 400 &&
         "the last-owner guard still holds");
  printf("  API-06 owner membership is owner-only: ok\n");

  db_close(&db);
  printf("test_tenant_resolve: ok\n");
  return 0;
}
