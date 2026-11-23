/* test_routes.c — the route table (core/routes.c) and its fit with httpd.c.
 *
 *  (a) Every path literal httpd.c's dispatch chain matches (eq / starts / seg)
 *      is reachable through a table row. httpd.c is parsed at test time, so a
 *      matcher added to the chain without a row fails here by name — the same
 *      way it would be refused 404 at runtime.
 *  (b) The gates: every /api/admin/ and /api/db/ row is OPERATOR; the pre-auth
 *      set is exactly health, the search stream and break-glass login; writes
 *      on sources (cadence, settings, interval, schedule) are OPERATOR while
 *      their reads are not.
 *  (c) Matching semantics: params, prefixes, method sets, RF_STRICT.
 */
#include "../../core/routes.c"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef JO_REPO_ROOT
#error "JO_REPO_ROOT must be defined (tests/unit/run.sh does)"
#endif

static const route_row *look(const char *m, const char *u, route_status *st) {
  const route_row *r = NULL;
  route_status s = routes_lookup(m, strlen(m), u, strlen(u), &r);
  if (st) *st = s;
  return s == ROUTE_OK ? r : NULL;
}
static int served(const char *u) {
  static const char *M[] = { "GET", "POST", "PUT", "PATCH", "DELETE" };
  for (size_t i = 0; i < 5; i++) if (look(M[i], u, NULL)) return 1;
  return 0;
}

/* Read the next C string literal starting at *p (after skipping spaces and
 * commas); returns 0 if the next token is not a literal. */
static int lit(const char **p, char *out, size_t cap) {
  const char *s = *p;
  while (*s == ' ' || *s == ',' || *s == '\n' || *s == '\t') s++;
  if (*s != '"') return 0;
  s++;
  size_t n = 0;
  while (*s && *s != '"') {
    if (*s == '\\' && s[1]) s++;
    if (n + 1 < cap) out[n++] = *s;
    s++;
  }
  out[n] = 0;
  *p = *s ? s + 1 : s;
  return 1;
}

static void test_chain_has_rows(void) {
  char path[512];
  snprintf(path, sizeof path, "%s/native/core/httpd.c", JO_REPO_ROOT);
  FILE *f = fopen(path, "rb");
  assert(f && "cannot open httpd.c");
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  char *src = malloc((size_t) n + 1);
  assert(src && fread(src, 1, (size_t) n, f) == (size_t) n);
  src[n] = 0; fclose(f);

  const char *fnp = strstr(src, "static void fn(struct mg_connection");
  assert(fnp && "dispatch function not found");
  int checked = 0, missing = 0;
  for (const char *p = fnp; (p = strpbrk(p, "es")) != NULL; p++) {
    const char *q = p; int kind = 0;
    int bnd = !(p[-1] == '_' || (p[-1] >= 'a' && p[-1] <= 'z') || (p[-1] >= 'A' && p[-1] <= 'Z') || (p[-1] >= '0' && p[-1] <= '9'));
    if (!bnd) continue;
    if (!strncmp(q, "eq(u", 4)) { kind = 1; q += 4; }
    else if (!strncmp(q, "starts(u", 8)) { kind = 2; q += 8; }
    else if (!strncmp(q, "seg(u", 5)) { kind = 3; q += 5; }
    if (!kind) continue;
    char a[300] = {0}, b[300] = {0}, probe[700];
    if (!lit(&q, a, sizeof a)) continue;
    if (strncmp(a, "/api/", 5) && strncmp(a, "/admin/", 7)) continue;
    if (kind == 3) {
      if (!lit(&q, b, sizeof b)) continue;
      snprintf(probe, sizeof probe, "%sx%s", a, b);
    } else if (kind == 2 && a[strlen(a) - 1] == '/') {
      snprintf(probe, sizeof probe, "%sx", a);
    } else {
      snprintf(probe, sizeof probe, "%s", a);
    }
    /* Generic guards, not routes: "anything under /api/", the admin block's
     * own condition (every admin route has its own row), the break-glass
     * mount whose only route is /login. */
    if (!strcmp(a, "/api/") || !strcmp(a, "/api/admin/") ||
        !strcmp(a, "/admin/break-glass")) continue;
    checked++;
    if (!served(probe)) {
      fprintf(stderr, "NO ROUTE ROW for httpd.c matcher %s -> probe %s\n", a, probe);
      missing++;
    }
  }
  free(src);
  fprintf(stderr, "chain matchers checked: %d\n", checked);
  assert(checked > 120 && "the parser found suspiciously few matchers");
  assert(missing == 0);
}

static void test_gates(void) {
  assert(routes_selfcheck() == NULL);
  size_t n; const route_row *t = routes_table(&n);
  assert(n > 100);

  /* every /api/admin/ and /api/db/ row is OPERATOR */
  int admin = 0;
  for (size_t i = 0; i < n; i++) {
    if (!strncmp(t[i].pattern, "/api/admin/", 11) || !strncmp(t[i].pattern, "/api/db/", 8)) {
      admin++;
      if (t[i].gate != RG_OPERATOR) fprintf(stderr, "not operator: %s\n", t[i].pattern);
      assert(t[i].gate == RG_OPERATOR);
    }
  }
  assert(admin >= 30);
  /* and the platform credential store, breach corpus, metrics, db explorer */
  assert(look("GET", "/api/keys", NULL)->gate == RG_OPERATOR);
  assert(look("PUT", "/api/keys/SHODAN", NULL)->gate == RG_OPERATOR);
  assert(look("GET", "/api/breach/search", NULL)->gate == RG_OPERATOR);
  assert(look("GET", "/api/metrics", NULL)->gate == RG_OPERATOR);
  assert(look("GET", "/api/admin/routes", NULL)->gate == RG_OPERATOR);

  /* pre-auth set is exactly the intended one */
  const char *pub[8]; int np = 0;
  for (size_t i = 0; i < n; i++) if (t[i].gate == RG_PUBLIC) pub[np++] = t[i].pattern;
  assert(np == 3);
  assert(!strcmp(pub[0], "/api/health"));
  assert(!strcmp(pub[1], "/api/search/stream/:id"));
  assert(!strcmp(pub[2], "/admin/break-glass/login"));
  assert(look("GET", "/api/search/stream/abc", NULL)->gate == RG_PUBLIC);

  /* source writes are operator, source reads are not */
  assert(look("PUT",  "/api/sources/cadence", NULL)->gate == RG_OPERATOR);
  assert(look("POST", "/api/sources/cadence", NULL)->gate == RG_OPERATOR);
  assert(look("GET",  "/api/sources/cadence", NULL)->gate == RG_AUTH);
  assert(look("PUT",  "/api/sources/jma-quake/settings", NULL)->gate == RG_OPERATOR);
  assert(look("GET",  "/api/sources/jma-quake/settings", NULL)->gate == RG_AUTH);
  assert(look("PUT",  "/api/sources/jma-quake/interval", NULL)->gate == RG_OPERATOR);
  assert(look("DELETE", "/api/sources/jma-quake/interval", NULL)->gate == RG_OPERATOR);
  assert(look("GET",  "/api/sources/jma-quake/interval", NULL)->gate == RG_AUTH);
  assert(look("PUT",  "/api/sources/jma-quake/schedule", NULL)->gate == RG_OPERATOR);
  assert(look("POST", "/api/status/jma-quake/probe", NULL)->gate == RG_OPERATOR);

  /* collector runs and workspace roles */
  assert(look("POST", "/api/intel/sources/x/run", NULL)->gate == RG_RUN);
  assert(look("POST", "/api/data/cameras/trigger", NULL)->gate == RG_RUN);
  assert(look("POST", "/api/uploads/abc/part", NULL)->gate == RG_ROLE_ANALYST);
  assert(look("POST", "/api/alerts/preview", NULL)->gate == RG_ROLE_ANALYST);
  assert(look("GET",  "/api/cases", NULL)->gate == RG_TENANT);
  assert(look("GET",  "/api/tenant-keys/policy", NULL)->gate == RG_TENANT);
  assert(look("GET",  "/api/tenant-keys/SHODAN", NULL)->gate == RG_OPERATOR);
  assert(look("PUT",  "/api/tenant-keys/SHODAN", NULL)->gate == RG_TENANT);
}

static void test_matching(void) {
  route_status st;
  /* unknown path under /api: no row */
  assert(!look("GET", "/api/nope", &st) && st == ROUTE_NOT_FOUND);
  /* a typo'd admin path has no row either (the old chain tail dumped a table) */
  assert(!look("GET", "/api/admin/xxxsources", &st) && st == ROUTE_NOT_FOUND);
  /* single-segment param does not swallow a deeper path */
  assert(!look("GET", "/api/status/a/b", &st) && st == ROUTE_NOT_FOUND);
  assert(look("GET", "/api/status/a", NULL));
  /* param + literal tail behaves like seg() */
  assert(look("GET", "/api/sources/a/detail", NULL));
  assert(!look("GET", "/api/sources//detail", NULL));
  /* strict rows answer 405 and do not fall to a broader prefix */
  assert(!look("POST", "/api/alert-events/e1/deliveries", &st) &&
         st == ROUTE_METHOD_NOT_ALLOWED);
  assert(look("POST", "/api/alert-events/e1", NULL));
  assert(!look("GET", "/api/intel/sources/x/run", &st) &&
         st == ROUTE_METHOD_NOT_ALLOWED);
  assert(!look("POST", "/api/sources/x/settings", &st) &&
         st == ROUTE_METHOD_NOT_ALLOWED);
  /* ordering: sub-routes win over the item catch-all */
  assert(look("GET", "/api/intel/items/u1/reveal", NULL)->gate == RG_OPERATOR);
  assert(look("GET", "/api/intel/items/u1", NULL)->gate == RG_TENANT);
  assert(look("GET", "/api/transit/x/departures", NULL));
  assert(look("GET", "/api/transit", NULL));
  /* a pathological method still reaches an ANY row */
  assert(look("FOO", "/api/me", NULL));

  char *js = routes_json();
  assert(js && strstr(js, "\"gate\":\"OPERATOR\"") && strstr(js, "\"runs_on\":\"WORKER\""));
  free(js);
}

/* API-07: every /api/uploads verb (begin, part, commit, status, abort) is a
 * worker row, and httpd.c no longer calls uploadapi() on the shared handle from
 * the event loop: the only call left is upl_thread's, on its own connection. */
static void test_uploads_off_loop(void) {
  static const char *M[] = { "GET", "POST", "DELETE" };
  static const char *U[] = { "/api/uploads", "/api/uploads/begin",
                             "/api/uploads/u1", "/api/uploads/u1/part",
                             "/api/uploads/u1/commit" };
  for (size_t i = 0; i < 3; i++)
    for (size_t j = 0; j < 5; j++) {
      const route_row *r = look(M[i], U[j], NULL);
      assert(r && r->runs == RR_WORKER);
    }
  char path[512];
  snprintf(path, sizeof path, "%s/native/core/httpd.c", JO_REPO_ROOT);
  FILE *f = fopen(path, "rb");
  assert(f);
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  char *src = malloc((size_t) n + 1);
  assert(src && fread(src, 1, (size_t) n, f) == (size_t) n);
  src[n] = 0; fclose(f);
  int calls = 0;
  for (const char *p = src; (p = strstr(p, "uploadapi(")) != NULL; p++) {
    if (p > src && (p[-1] == '_' || p[-1] == '.')) continue;
    if (strncmp(p, "uploadapi(db,", 13) == 0) calls++;           /* worker */
    else if (strncmp(p, "uploadapi(g_db", 14) == 0) {
      fprintf(stderr, "httpd.c still runs uploadapi() on the shared handle\n");
      assert(0);
    }
  }
  assert(calls == 1);
  free(src);
}

int main(void) {
  test_uploads_off_loop();
  test_matching();
  test_gates();
  test_chain_has_rows();
  printf("test_routes: ok\n");
  return 0;
}
