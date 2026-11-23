/* test_run_gate_and_user_cap.c — the two API-02 / API-08 guards in core/httpd.c
 * that had no test of their own (docs/audit-2026-11-14/fix_api.md, "A new test
 * for tenant_is_personal / the personal run gate and the per-user cap was not
 * written").
 *
 * (a) collector_run_gate(): who may make the server run a collector.
 *     - a plain signed-in user owns an auto-provisioned PERSONAL workspace, so
 *       `owner` there proves nothing: 403 naming the personal workspace;
 *     - the same role in a TEAM workspace (a second member joined) passes;
 *     - tenants.personal=1 with two members is a team workspace, and with one
 *       member is personal again (the "once someone joins" rule);
 *     - a plain viewer of a team workspace is refused for the role;
 *     - a maintenance pod is operator-only, team owner or not;
 *     - a platform operator passes everywhere, personal workspace and pod
 *       included;
 *     - a foreign X-Tenant-Id is not_a_member, and a non-operator who passes
 *       is held to the per-user run budget (429 on the 11th in a minute).
 *
 * (b) worker_admit()/worker_release(): one user may hold JO_HTTP_USER_WORKERS
 *     (default 4) off-loop workers; the next is refused with the PER-USER flag
 *     (429 too_many_requests_in_flight, not the global 503), another user is
 *     unaffected, and a released slot is usable again. Slots are held by real
 *     threads started through worker_spawn(), because that trampoline is how
 *     the charge reaches the thread that releases it. */
#include "../../core/httpd.c"

#include "../../core/auth.h"
#include <assert.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <unistd.h>

static const char *SECRET = "run-gate-test-secret";

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
/* Verify a real token through auth_check(): tenant_resolve() consults the
 * thread-local verified flag that only auth_check() sets, so the gate must be
 * called right after this on the same thread. */
static void login(const char *sub, const char *email, auth_user *u) {
  char pl[512];
  snprintf(pl, sizeof pl,
           "{\"sub\":\"%s\",\"email\":\"%s\",\"email_verified\":true,"
           "\"aud\":\"authenticated\",\"exp\":%ld}", sub, email, (long)time(NULL) + 600);
  const char *hdr = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
  char *h = b64u((const unsigned char *)hdr, strlen(hdr));
  char *p = b64u((const unsigned char *)pl, strlen(pl));
  char si[1024]; snprintf(si, sizeof si, "%s.%s", h, p);
  unsigned char mac[32]; unsigned int ml = 32;
  HMAC(EVP_sha256(), SECRET, (int)strlen(SECRET), (unsigned char *)si, strlen(si), mac, &ml);
  char *sg = b64u(mac, 32);
  char bearer[2048]; snprintf(bearer, sizeof bearer, "Bearer %s.%s", si, sg);
  free(h); free(p); free(sg);
  memset(u, 0, sizeof *u);
  assert(auth_check(bearer, u) == AUTH_ALLOW);
}
static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  int rc = sqlite3_exec(db->h, sql, NULL, NULL, &e);
  if (rc != SQLITE_OK) fprintf(stderr, "%s: %s\n", sql, e ? e : "?");
  assert(rc == SQLITE_OK);
  sqlite3_free(e);
}
static void user_id(db_handle *db, const char *sub, char *out, size_t cap) {
  sqlite3_stmt *s;
  assert(sqlite3_prepare_v2(db->h, "SELECT id FROM users WHERE supabase_user_id=?1",
                            -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, sub, -1, SQLITE_STATIC);
  assert(sqlite3_step(s) == SQLITE_ROW);
  snprintf(out, cap, "%s", (const char *)sqlite3_column_text(s, 0));
  sqlite3_finalize(s);
}
/* gate as `sub`, in workspace `xt` (NULL = their default) */
static int gate(db_handle *db, const char *sub, const char *xt, int pod,
                const char *rl, char *body, size_t cap) {
  auth_user u;
  char em[64]; snprintf(em, sizeof em, "%s@x.test", sub);
  login(sub, em, &u);
  body[0] = 0;
  return collector_run_gate(db, &u, xt, pod, rl, body, cap);
}

/* ── (b) held workers ───────────────────────────────────────────────────── */
static pthread_mutex_t g_hold_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_hold_cv = PTHREAD_COND_INITIALIZER;
static int g_hold_go, g_hold_started;
static void *holder(void *arg) {
  (void)arg;
  pthread_mutex_lock(&g_hold_mu);
  g_hold_started++;
  pthread_cond_broadcast(&g_hold_cv);
  while (!g_hold_go) pthread_cond_wait(&g_hold_cv, &g_hold_mu);
  pthread_mutex_unlock(&g_hold_mu);
  worker_release();                       /* reads the slot worker_spawn handed it */
  return NULL;
}
static int admit_as(const char *uid, pthread_t *th) {
  snprintf(g_req_uid, sizeof g_req_uid, "%s", uid);
  if (!worker_admit()) return 0;
  assert(worker_spawn(th, holder, NULL) == 0);
  return 1;
}
static int user_live(const char *uid) {
  int n = 0;
  pthread_mutex_lock(&g_wk_mu);
  for (int i = 0; i < WK_USER_SLOTS; i++)
    if (g_wk_user[i].n > 0 && !strcmp(g_wk_user[i].uid, uid)) n += g_wk_user[i].n;
  pthread_mutex_unlock(&g_wk_mu);
  return n;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  setenv("SUPABASE_JWT_SECRET", SECRET, 1);
  unsetenv("SUPABASE_URL");
  unsetenv("JO_JWT_ISS");
  unsetenv("PLATFORM_OPERATOR_IDS");
  unsetenv("PLATFORM_OPERATOR_EMAILS");
  unsetenv("JO_HTTP_USER_WORKERS");       /* user_worker_cap() reads it once: default 4 */
  auth_init();
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  char body[512];

  /* ── (a) the run gate ─────────────────────────────────────────────────── */
  /* a first-time user: personal workspace, `owner` in it */
  assert(gate(&db, "sub-p", NULL, 0, NULL, body, sizeof body) == 403);
  assert(strstr(body, "personal workspace") && "the refusal names the personal workspace");
  printf("  personal-workspace owner refused: %s\n", body);

  /* two more users, then a team workspace they share */
  assert(gate(&db, "sub-t1", NULL, 0, NULL, body, sizeof body) == 403);
  assert(gate(&db, "sub-t2", NULL, 0, NULL, body, sizeof body) == 403);
  char u1[64], u2[64];
  user_id(&db, "sub-t1", u1, sizeof u1);
  user_id(&db, "sub-t2", u2, sizeof u2);
  exec_ok(&db, "INSERT INTO tenants(id,slug,name) VALUES ('tTeam','team','Team')");
  char sql[256];
  snprintf(sql, sizeof sql, "INSERT INTO memberships(tenant_id,user_id,role) VALUES "
           "('tTeam','%s','owner'),('tTeam','%s','viewer')", u1, u2);
  exec_ok(&db, sql);

  assert(gate(&db, "sub-t1", "tTeam", 0, NULL, body, sizeof body) == 0 &&
         "owner of a team workspace may run a collector");
  assert(gate(&db, "sub-t2", "tTeam", 0, NULL, body, sizeof body) == 403 &&
         strstr(body, "owner/admin") && "a plain viewer may not");
  snprintf(sql, sizeof sql, "UPDATE memberships SET role='admin' WHERE tenant_id='tTeam' "
           "AND user_id='%s'", u2);
  exec_ok(&db, sql);
  assert(gate(&db, "sub-t2", "tTeam", 0, NULL, body, sizeof body) == 0 &&
         "admin of a team workspace may");

  /* a maintenance pod is operator-only, team owner or not */
  assert(gate(&db, "sub-t1", "tTeam", 1, NULL, body, sizeof body) == 403 &&
         strstr(body, "maintenance pod"));

  /* once-someone-joins: personal=1 with two members is a team workspace, with
   * one member it is personal again */
  exec_ok(&db, "UPDATE tenants SET personal=1 WHERE id='tTeam'");
  assert(tenant_is_personal(&db, "tTeam") == 0 && "flagged personal, two members: team");
  assert(gate(&db, "sub-t1", "tTeam", 0, NULL, body, sizeof body) == 0);
  snprintf(sql, sizeof sql, "DELETE FROM memberships WHERE tenant_id='tTeam' AND user_id='%s'", u2);
  exec_ok(&db, sql);
  assert(tenant_is_personal(&db, "tTeam") == 1 && "flagged personal, one member: personal");
  assert(gate(&db, "sub-t1", "tTeam", 0, NULL, body, sizeof body) == 403 &&
         strstr(body, "personal workspace"));
  exec_ok(&db, "UPDATE tenants SET personal=0 WHERE id='tTeam'");
  assert(tenant_is_personal(&db, "tTeam") == 0);
  assert(tenant_is_personal(&db, "no-such-tenant") == 1 && "unknown tenant fails closed");
  assert(tenant_is_personal(&db, "") == 1 && "empty tenant fails closed");

  /* a workspace that is not the caller's */
  assert(gate(&db, "sub-p", "tTeam", 0, NULL, body, sizeof body) == 403 &&
         strstr(body, "not_a_member"));

  /* the budget: 10 runs a minute, the 11th is 429, and it is per key */
  for (int i = 0; i < 10; i++)
    assert(gate(&db, "sub-t1", "tTeam", 0, "budget-user-1", body, sizeof body) == 0);
  assert(gate(&db, "sub-t1", "tTeam", 0, "budget-user-1", body, sizeof body) == 429 &&
         strstr(body, "collector_run_rate_limited"));
  assert(gate(&db, "sub-t1", "tTeam", 0, "budget-user-2", body, sizeof body) == 0 &&
         "another key has its own budget");

  /* a platform operator: personal workspace and pod included, no budget */
  setenv("PLATFORM_OPERATOR_IDS", "sub-p", 1);
  assert(gate(&db, "sub-p", NULL, 0, NULL, body, sizeof body) == 0 &&
         "operator passes from a personal workspace");
  assert(gate(&db, "sub-p", NULL, 1, NULL, body, sizeof body) == 0 && "operator may run a pod");
  assert(gate(&db, "sub-t1", "tTeam", 1, NULL, body, sizeof body) == 403 &&
         "the operator list is per user");
  unsetenv("PLATFORM_OPERATOR_IDS");
  assert(gate(&db, "sub-p", NULL, 0, NULL, body, sizeof body) == 403 &&
         "no operator configured: refused again");
  printf("  collector_run_gate: ok\n");

  /* ── (b) the per-user worker cap ──────────────────────────────────────── */
  assert(user_worker_cap() == 4 && "default per-user cap");
  assert(worker_cap() >= 6 && "global pool leaves room for the test");
  pthread_t th[8]; int nth = 0;
  for (int i = 0; i < 4; i++) {
    assert(admit_as("user-A", &th[nth++]) == 1);
  }
  assert(user_live("user-A") == 4);
  snprintf(g_req_uid, sizeof g_req_uid, "%s", "user-A");
  assert(worker_admit() == 0 && "the fifth worker of one user is refused");
  assert(g_wk_refused_user == 1 && "... and the refusal is about THAT user (429), not the pool");
  assert(user_live("user-A") == 4 && "a refused admit charged nothing");

  /* the reply for that refusal */
  struct mg_connection *c = calloc(1, sizeof *c);
  assert(c);
  reply_busy(c);
  assert(c->send.len > 0 && memmem(c->send.buf, c->send.len, "429", 3) &&
         memmem(c->send.buf, c->send.len, "too_many_requests_in_flight", 27) &&
         "per-user refusal is a 429, not a 503");
  mg_iobuf_free(&c->send);

  /* another user is not affected */
  assert(admit_as("user-B", &th[nth++]) == 1 && "one user's flood is not another's refusal");
  assert(g_wk_refused_user == 0);
  assert(user_live("user-B") == 1);

  /* release: every holder gives its slot back through the thread-local charge */
  pthread_mutex_lock(&g_hold_mu);
  g_hold_go = 1;
  pthread_cond_broadcast(&g_hold_cv);
  pthread_mutex_unlock(&g_hold_mu);
  for (int i = 0; i < nth; i++) pthread_join(th[i], NULL);
  assert(user_live("user-A") == 0 && user_live("user-B") == 0 && "all slots returned");
  assert(g_wk_live == 0 && "the global count returned too");

  g_hold_go = 0;
  assert(admit_as("user-A", &th[0]) == 1 && "capacity is usable again after release");
  pthread_mutex_lock(&g_hold_mu);
  g_hold_go = 1;
  pthread_cond_broadcast(&g_hold_cv);
  pthread_mutex_unlock(&g_hold_mu);
  pthread_join(th[0], NULL);
  assert(g_wk_live == 0);

  /* a request with no identified caller is charged to the pool only */
  g_req_uid[0] = 0;
  for (int i = 0; i < 6; i++) {
    g_hold_go = 0;
    assert(worker_admit() == 1);
    g_wk_pending = -1;
    worker_release();
  }
  assert(g_wk_live == 0);
  printf("  per-user worker cap: ok\n");

  db_close(&db);
  printf("test_run_gate_and_user_cap: all passed\n");
  return 0;
}
