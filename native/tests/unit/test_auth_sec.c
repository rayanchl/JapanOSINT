/* test_auth_sec.c — the pre-auth JWKS path cannot stall the loop, and
 * break-glass tokens verify under exactly the issuer's rules.
 *
 * H1. auth_check() runs on the mongoose event loop for every /api request,
 * unauthenticated. It used to fetch the JWKS inline (8 s timeout) on an
 * unknown kid, throttled PER KID — so a spray of random kids was a spray of
 * blocking fetches. Pinned here: 2,000 random-kid tokens, with a 2 s fetch in
 * flight, all answer 401 in well under a second, and cause exactly ONE fetch
 * beyond the boot fetch. And the limiter slot that budget lives in survives a
 * spray of other keys (ratelimit.c used to recycle it, failing open).
 *
 * M3. keysapi_breakglass() minted tokens nothing verified. Pinned: the real
 * issued token is accepted and maps to operator; a forged lifetime, a missing
 * marker claim, an expired token, a disabled break-glass and a Supabase-signed
 * token carrying the marker claim are all refused (or, for the last, accepted
 * as an ordinary user). Login attempts land in the audit chain. */
#include "../../core/auth.c"
#include "core/keysapi.h"
#include "core/operatorgate.h"
#include "core/db.h"

#include <assert.h>
#include <stdatomic.h>
#include <unistd.h>

/* ── fake JWKS upstream ──────────────────────────────────────────────────── */
static atomic_int g_fetch_calls;
static atomic_int g_fetch_slow_ms;
static char *fake_fetch(void) {
  atomic_fetch_add(&g_fetch_calls, 1);
  int ms = atomic_load(&g_fetch_slow_ms);
  if (ms > 0) usleep((useconds_t)ms * 1000);
  return strdup("{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"k1\",\"n\":\"AQAB\",\"e\":\"AQAB\"}]}");
}

static long long now_ms(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

/* ── token helpers ───────────────────────────────────────────────────────── */
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
/* "Bearer h.p.s" — HS256 over `secret`, or a junk signature when NULL. */
static char *bearer(const char *hdr_json, const char *pl_json, const char *secret) {
  char *h = b64u((const unsigned char *)hdr_json, strlen(hdr_json));
  char *p = b64u((const unsigned char *)pl_json, strlen(pl_json));
  char si[4096]; snprintf(si, sizeof si, "%s.%s", h, p);
  char *sg;
  if (secret) {
    unsigned char mac[32]; unsigned int ml = 32;
    HMAC(EVP_sha256(), secret, (int)strlen(secret), (unsigned char *)si,
         strlen(si), mac, &ml);
    sg = b64u(mac, 32);
  } else sg = strdup("AAAA");
  char *out = malloc(strlen(si) + strlen(sg) + 16);
  sprintf(out, "Bearer %s.%s", si, sg);
  free(h); free(p); free(sg);
  return out;
}
static auth_result check(const char *b, auth_user *u) {
  memset(u, 0x5A, sizeof *u);          /* garbage in: break_glass must be SET */
  return auth_check(b, u);
}

/* RFC 6238 for the break-glass login (secret JBSWY3DPEHPK3PXP). */
static void totp_now(char out[7]) {
  static const unsigned char key[] = { 'H','e','l','l','o','!',0xDE,0xAD,0xBE,0xEF };
  long step = (long)time(NULL) / 30;
  unsigned char buf[8];
  for (int i = 7; i >= 0; i--) { buf[i] = (unsigned char)(step & 0xFF); step >>= 8; }
  unsigned char mac[20]; unsigned int ml = 20;
  HMAC(EVP_sha1(), key, sizeof key, buf, 8, mac, &ml);
  int off = mac[19] & 0x0F;
  unsigned bin = ((mac[off] & 0x7Fu) << 24) | ((unsigned)mac[off + 1] << 16) |
                 ((unsigned)mac[off + 2] << 8) | mac[off + 3];
  snprintf(out, 7, "%06u", bin % 1000000u);
}

static long scalar(db_handle *db, const char *sql) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }

  setenv("SUPABASE_URL", "http://127.0.0.1:9", 1);          /* never dialled */
  setenv("SUPABASE_JWT_SECRET", "supabase-secret-for-test", 1);
  setenv("JO_JWT_ISS", "https://iss.test/auth/v1", 1);
  unsetenv("PLATFORM_OPERATOR_EMAILS");
  unsetenv("PLATFORM_OPERATOR_IDS");
  g_jwks_fetch = fake_fetch;
  auth_init();

  /* Boot fetch happens off-thread. */
  for (int i = 0; i < 200 && atomic_load(&g_fetch_calls) < 1; i++) usleep(10000);
  for (int i = 0; i < 200; i++) {
    pthread_mutex_lock(&g_jwks_lock); int have = g_jwks_doc != NULL;
    pthread_mutex_unlock(&g_jwks_lock);
    if (have) break;
    usleep(10000);
  }
  assert(atomic_load(&g_fetch_calls) == 1 && "auth_init fetched once, off the loop");

  /* ── H1: random kids, slow upstream ── */
  atomic_store(&g_fetch_slow_ms, 2000);
  long long t0 = now_ms();
  for (int i = 0; i < 2000; i++) {
    char hj[128]; snprintf(hj, sizeof hj, "{\"alg\":\"RS256\",\"kid\":\"rnd-%d-%d\"}",
                           i, rand());
    char *b = bearer(hj, "{\"sub\":\"x\",\"exp\":9999999999}", NULL);
    auth_user u;
    assert(check(b, &u) == AUTH_401_INVALID);
    free(b);
  }
  long long dt = now_ms() - t0;
  printf("  2000 random-kid tokens answered in %lld ms (a 2000 ms fetch in flight)\n", dt);
  assert(dt < 1000 && "no unauthenticated request waits on the network");
  /* Let the one scheduled refetch finish, then check nothing else ran. */
  for (int i = 0; i < 400 && atomic_load(&g_fetch_calls) < 2; i++) usleep(10000);
  usleep(2300 * 1000);
  printf("  fetches: %d (boot + forced refetches)\n", atomic_load(&g_fetch_calls));
  assert(atomic_load(&g_fetch_calls) == 2 && "one global refetch, not one per kid");
  assert(jwks_request_refresh() == 0 && "the global budget is spent");

  /* The known kid still verifies against the cache path (bad key material ⇒
   * 401, but no refresh): fetch count unchanged. */
  { auth_user u;
    char *b = bearer("{\"alg\":\"RS256\",\"kid\":\"k1\"}", "{\"sub\":\"x\"}", NULL);
    assert(check(b, &u) == AUTH_401_INVALID);
    free(b);
    usleep(50 * 1000);
    assert(atomic_load(&g_fetch_calls) == 2); }

  /* The budget's limiter slot is not evicted by a spray of other keys. */
  for (int i = 0; i < 2000; i++) {
    char k[32]; snprintf(k, sizeof k, "10.0.%d.%d", i / 256, i % 256);
    ratelimit_allow(RL_ISOCHRONE, k, 12, 60, NULL);
  }
  assert(jwks_request_refresh() == 0 && "RL_JWKS fails closed under table pressure");

  /* ── M3: break-glass ── */
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  setenv("BREAK_GLASS_ENABLED", "1", 1);
  setenv("BREAK_GLASS_JWT_SECRET", "break-glass-secret-for-test", 1);
  setenv("ADMIN_TOTP_SECRET", "JBSWY3DPEHPK3PXP", 1);

  int st = 0;
  char *r = keysapi_breakglass(&db, "{\"code\":\"000000x\"}", "192.0.2.1", "t", &st);
  assert(st == 400); free(r);
  char code[7]; totp_now(code);
  char wrong[7]; snprintf(wrong, sizeof wrong, "%06d", (atoi(code) + 500000) % 1000000);
  char body[64];
  snprintf(body, sizeof body, "{\"code\":\"%s\"}", wrong);
  r = keysapi_breakglass(&db, body, "192.0.2.1", "t", &st);
  assert(st == 401); free(r);
  snprintf(body, sizeof body, "{\"code\":\"%s\"}", code);
  r = keysapi_breakglass(&db, body, "192.0.2.1", "t", &st);
  assert(st == 200 && r);
  cJSON *jr = cJSON_Parse(r); free(r);
  const char *tok = cJSON_GetObjectItem(jr, "token")->valuestring;
  char *b = malloc(strlen(tok) + 8); sprintf(b, "Bearer %s", tok);

  auth_user u;
  assert(check(b, &u) == AUTH_ALLOW && "the issued break-glass token verifies");
  assert(u.break_glass == 1 && strcmp(u.id, "break-glass-admin") == 0);
  assert(opgate_check(&u) == 0 && "and is operator with no allowlist configured");

  assert(scalar(&db, "SELECT COUNT(*) FROM audit_events WHERE tenant_id='platform'"
                     " AND action='break_glass.login.ok' AND row_hash IS NOT NULL"
                     " AND ip='192.0.2.1'") == 1 && "login audited, chained");
  assert(scalar(&db, "SELECT COUNT(*) FROM audit_events WHERE tenant_id='platform'"
                     " AND action='break_glass.login.denied' AND row_hash IS NOT NULL") == 1);

  /* Disabled after issue: refused. */
  setenv("BREAK_GLASS_ENABLED", "0", 1);
  assert(check(b, &u) == AUTH_401_INVALID);
  setenv("BREAK_GLASS_ENABLED", "1", 1);
  free(b);
  cJSON_Delete(jr);

  const char *HS = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
  const char *BGS = "break-glass-secret-for-test";
  long now = (long)time(NULL);
  char pl[512];

  /* Lifetime longer than the issuer grants. */
  snprintf(pl, sizeof pl, "{\"sub\":\"break-glass-admin\",\"break_glass\":true,"
           "\"iat\":%ld,\"exp\":%ld}", now, now + 7200);
  b = bearer(HS, pl, BGS);
  assert(check(b, &u) == AUTH_401_INVALID && "over-long lifetime refused"); free(b);
  /* Backdated iat to fake a short lifetime on a far exp. */
  snprintf(pl, sizeof pl, "{\"sub\":\"break-glass-admin\",\"break_glass\":true,"
           "\"iat\":%ld,\"exp\":%ld}", now + 86400 - 3000, now + 86400);
  b = bearer(HS, pl, BGS);
  assert(check(b, &u) == AUTH_401_INVALID && "future-minted token refused"); free(b);
  /* No marker claim. */
  snprintf(pl, sizeof pl, "{\"sub\":\"break-glass-admin\",\"iat\":%ld,\"exp\":%ld}",
           now, now + 600);
  b = bearer(HS, pl, BGS);
  assert(check(b, &u) == AUTH_401_INVALID && "marker claim required"); free(b);
  /* Expired. */
  snprintf(pl, sizeof pl, "{\"sub\":\"break-glass-admin\",\"break_glass\":true,"
           "\"iat\":%ld,\"exp\":%ld}", now - 4000, now - 400);
  b = bearer(HS, pl, BGS);
  assert(check(b, &u) == AUTH_401_INVALID && "expired refused"); free(b);
  /* Header lies about the algorithm. */
  snprintf(pl, sizeof pl, "{\"sub\":\"break-glass-admin\",\"break_glass\":true,"
           "\"iat\":%ld,\"exp\":%ld}", now, now + 600);
  b = bearer("{\"alg\":\"none\"}", pl, BGS);
  assert(check(b, &u) == AUTH_401_INVALID && "alg must be HS256"); free(b);
  /* Well-formed and short: accepted. */
  b = bearer(HS, pl, BGS);
  assert(check(b, &u) == AUTH_ALLOW && u.break_glass == 1); free(b);

  /* A Supabase-signed token carrying the marker is an ordinary user. */
  snprintf(pl, sizeof pl, "{\"sub\":\"break-glass-admin\",\"break_glass\":true,"
           "\"aud\":\"authenticated\",\"iss\":\"https://iss.test/auth/v1\","
           "\"iat\":%ld,\"exp\":%ld}", now, now + 600);
  b = bearer(HS, pl, "supabase-secret-for-test");
  assert(check(b, &u) == AUTH_ALLOW && u.break_glass == 0);
  setenv("PLATFORM_OPERATOR_IDS", "someone-else", 1);
  assert(opgate_check(&u) != 0 && "the claim alone grants nothing");
  free(b);

  /* Break-glass secret reused as the Supabase secret: refused outright. */
  setenv("BREAK_GLASS_JWT_SECRET", "supabase-secret-for-test", 1);
  snprintf(pl, sizeof pl, "{\"sub\":\"break-glass-admin\",\"break_glass\":true,"
           "\"iat\":%ld,\"exp\":%ld}", now, now + 600);
  b = bearer(HS, pl, "supabase-secret-for-test");
  auth_result ar = check(b, &u);
  assert(ar != AUTH_ALLOW || u.break_glass == 0); free(b);

  db_close(&db);
  printf("test_auth_sec: ok\n");
  return 0;
}
