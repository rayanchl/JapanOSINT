/* test_cors.c — cross-origin access is an explicit allow-list or nothing.
 *
 * Audit of 2026-10-02, open item 11: the VITE_API_HOST deployment (client on
 * another origin) could not work — every authenticated call is preflighted,
 * and OPTIONS reached the auth gate and got a 401 — while every JSON reply
 * carried `Access-Control-Allow-Origin: *`.
 *
 * Holds (core/cors.c, which every httpd.c reply path takes its headers from):
 *   - JO_CORS_ORIGINS unset: no CORS header at all;
 *   - set: a listed origin is reflected exactly, with Vary: Origin; an
 *     unlisted one (including a look-alike suffix) gets Vary only;
 *   - `*`, `null`, paths and non-http schemes are refused at load;
 *   - preflight: 204 with methods and the three headers the client sends for
 *     a listed origin, 403 for an unlisted one, never credentials, never `*`. */
#include "../../core/cors.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char v(const char *o) { return cors_verdict(o, o ? strlen(o) : 0); }

int main(void) {
  unsetenv("JO_CORS_ORIGINS");
  assert(cors_init() == 0 && !cors_enabled());
  assert(v("https://app.example.com") == 0);
  assert(!strcmp(cors_headers(v("https://app.example.com")), "") &&
         "unset: no CORS header at all (the same-origin deployment)");
  printf("  unset: no CORS headers: ok\n");

  setenv("JO_CORS_ORIGINS",
         " https://App.Example.com/ ,http://localhost:3000,*,null,"
         "ftp://files.example,https://x.example/path,https://app.example.com", 1);
  assert(cors_init() == 2 && cors_enabled() &&
         "two usable origins; wildcard, null, a path, a non-http scheme and a "
         "duplicate are refused");

  unsigned char ok = v("https://app.example.com");
  assert(ok >= 2);
  const char *h = cors_headers(ok);
  assert(strstr(h, "Access-Control-Allow-Origin: https://app.example.com\r\n"));
  assert(strstr(h, "Vary: Origin\r\n"));
  assert(strstr(h, "Access-Control-Expose-Headers: ") && strstr(h, "Content-Disposition"));
  assert(!strstr(h, "Credentials") && !strstr(h, "Origin: *"));
  assert(v("http://localhost:3000") >= 2);
  printf("  a listed origin is reflected exactly, with Vary: Origin: ok\n");

  const char *bad[] = { "https://evil.example", "https://app.example.com.evil.example",
                        "http://app.example.com", "https://app.example.com:8443",
                        "null", "", NULL };
  for (int i = 0; bad[i]; i++) {
    unsigned char b = v(bad[i]);
    assert(b == 1);
    assert(!strcmp(cors_headers(b), "Vary: Origin\r\n") &&
           "an unlisted origin is never named, but the response still varies");
  }
  assert(v(NULL) == 1);
  printf("  unlisted and look-alike origins get Vary only: ok\n");

  int st = 0;
  const char *pr = cors_preflight_response(ok, 1, &st);
  assert(st == 204 && !strncmp(pr, "HTTP/1.1 204", 12));
  assert(strstr(pr, "Access-Control-Allow-Origin: https://app.example.com\r\n"));
  assert(strstr(pr, "Access-Control-Allow-Methods: ") && strstr(pr, "PATCH") &&
         strstr(pr, "DELETE"));
  assert(strstr(pr, "Access-Control-Allow-Headers: Authorization, Content-Type, X-Tenant-Id"));
  assert(strstr(pr, "Vary: Origin") && strstr(pr, "Content-Length: 0\r\n\r\n"));
  assert(!strstr(pr, "Credentials") && !strstr(pr, ": *"));
  pr = cors_preflight_response(v("https://evil.example"), 1, &st);
  assert(st == 403 && !strstr(pr, "Access-Control-Allow-Origin") &&
         strstr(pr, "cors_origin_not_allowed"));
  { const char *body = strstr(pr, "\r\n\r\n") + 4;
    char cl[32]; snprintf(cl, sizeof cl, "Content-Length: %zu\r\n", strlen(body));
    assert(strstr(pr, cl) && "the refusal's Content-Length matches its body"); }
  pr = cors_preflight_response(v(NULL), 0, &st);
  assert(st == 204 && !strstr(pr, "Access-Control-Allow-Origin") && strstr(pr, "Allow: "));
  printf("  preflight: 204 for a listed origin, 403 otherwise, no credentials: ok\n");

  unsetenv("JO_CORS_ORIGINS");
  assert(cors_init() == 0 && !strcmp(cors_headers(ok), "") &&
         "a verdict from before a reload names nothing once CORS is off");

  printf("\nall passed\n");
  return 0;
}
