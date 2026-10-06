/* core/cors.c — see cors.h. */
#include "cors.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define CORS_METHODS "GET, POST, PUT, PATCH, DELETE, OPTIONS"
/* What the web client actually sends (auth/session.js adds the first and
 * third to every /api call; JSON bodies set the second). */
#define CORS_REQ_HEADERS "Authorization, Content-Type, X-Tenant-Id"
/* Response headers the client reads beyond the CORS-safelisted ones:
 * Content-Disposition (case/export downloads), X-JO-Evidence-SHA256
 * (IntelItemPage evidence download), Retry-After (503 server_busy). */
#define CORS_EXPOSE "Content-Disposition, X-JO-Evidence-SHA256, X-JO-Still-SHA256, Retry-After"

static char g_origin[CORS_MAX_ORIGINS][CORS_ORIGIN_MAX];
static char g_hdr[CORS_MAX_ORIGINS][CORS_HDR_MAX];
static char g_pre[CORS_MAX_ORIGINS][CORS_ORIGIN_MAX + 512];
static int  g_n = 0;

static const char *PRE_REFUSED =
  "HTTP/1.1 403 Forbidden\r\nContent-Type: application/json\r\n"
  "Vary: Origin\r\nContent-Length: 35\r\n\r\n"
  "{\"error\":\"cors_origin_not_allowed\"}";
static const char *PRE_NO_ORIGIN =
  "HTTP/1.1 204 No Content\r\nAllow: " CORS_METHODS "\r\n"
  "Vary: Origin\r\nContent-Length: 0\r\n\r\n";

/* Normalise one configured entry in place: trim, drop trailing '/', lower-case
 * (browsers serialise Origin with a lower-case scheme and host). Returns 0 if
 * the entry is usable. */
static int normalise(char *s, size_t n) {
  while (n && isspace((unsigned char) *s)) { memmove(s, s + 1, n); n--; }
  while (n && (isspace((unsigned char) s[n - 1]) || s[n - 1] == '/')) s[--n] = 0;
  if (!n) return -1;
  for (size_t i = 0; i < n; i++) s[i] = (char) tolower((unsigned char) s[i]);
  if (!strcmp(s, "*") || !strcmp(s, "null")) return -2;
  const char *rest = NULL;
  if (!strncmp(s, "https://", 8)) rest = s + 8;
  else if (!strncmp(s, "http://", 7)) rest = s + 7;
  if (!rest || !*rest) return -3;
  if (strchr(rest, '/') || strchr(rest, '*') || strchr(rest, '?') ||
      strchr(rest, '#') || strchr(rest, '@')) return -3;
  for (const char *p = s; *p; p++)
    if ((unsigned char) *p < 0x21 || *p == ',' || *p == '"') return -3;
  return 0;
}

int cors_init(void) {
  g_n = 0;
  const char *v = getenv("JO_CORS_ORIGINS");
  if (!v || !*v) return 0;
  const char *p = v;
  while (*p) {
    const char *comma = strchr(p, ',');
    size_t len = comma ? (size_t) (comma - p) : strlen(p);
    char e[CORS_ORIGIN_MAX];
    if (len >= sizeof e) {
      fprintf(stderr, "[cors] JO_CORS_ORIGINS entry longer than %d bytes ignored\n",
              CORS_ORIGIN_MAX - 1);
    } else {
      memcpy(e, p, len); e[len] = 0;
      int r = normalise(e, len);
      if (r == -2)
        fprintf(stderr, "[cors] JO_CORS_ORIGINS: '%s' refused — list exact origins, "
                        "a wildcard or null cannot be an allow-list\n", e);
      else if (r == -3)
        fprintf(stderr, "[cors] JO_CORS_ORIGINS: '%s' is not scheme://host[:port]; "
                        "ignored\n", e);
      else if (r == 0) {
        int dup = 0;
        for (int i = 0; i < g_n; i++) if (!strcmp(g_origin[i], e)) dup = 1;
        if (!dup && g_n < CORS_MAX_ORIGINS) {
          snprintf(g_origin[g_n], sizeof g_origin[g_n], "%s", e);
          snprintf(g_hdr[g_n], sizeof g_hdr[g_n],
                   "Access-Control-Allow-Origin: %s\r\nVary: Origin\r\n"
                   "Access-Control-Expose-Headers: " CORS_EXPOSE "\r\n", e);
          snprintf(g_pre[g_n], sizeof g_pre[g_n],
                   "HTTP/1.1 204 No Content\r\n"
                   "Access-Control-Allow-Origin: %s\r\nVary: Origin\r\n"
                   "Access-Control-Allow-Methods: " CORS_METHODS "\r\n"
                   "Access-Control-Allow-Headers: " CORS_REQ_HEADERS "\r\n"
                   "Access-Control-Max-Age: 600\r\n"
                   "Content-Length: 0\r\n\r\n", e);
          g_n++;
        } else if (!dup) {
          fprintf(stderr, "[cors] more than %d origins in JO_CORS_ORIGINS; "
                          "'%s' and later entries ignored\n", CORS_MAX_ORIGINS, e);
        }
      }
    }
    if (!comma) break;
    p = comma + 1;
  }
  return g_n;
}

int cors_enabled(void) { return g_n > 0; }

unsigned char cors_verdict(const char *origin, size_t len) {
  if (g_n == 0) return 0;
  if (!origin || !len || len >= CORS_ORIGIN_MAX) return 1;
  for (int i = 0; i < g_n; i++)
    if (strlen(g_origin[i]) == len && !strncasecmp(g_origin[i], origin, len))
      return (unsigned char) (2 + i);
  return 1;
}

const char *cors_headers(unsigned char verdict) {
  if (verdict == 0 || g_n == 0) return "";
  if (verdict == 1 || verdict - 2 >= g_n) return "Vary: Origin\r\n";
  return g_hdr[verdict - 2];
}

const char *cors_preflight_response(unsigned char verdict, int has_origin,
                                    int *status) {
  if (verdict >= 2 && verdict - 2 < g_n) {
    if (status) *status = 204;
    return g_pre[verdict - 2];
  }
  if (!has_origin) {                   /* not a browser preflight at all */
    if (status) *status = 204;
    return PRE_NO_ORIGIN;
  }
  if (status) *status = 403;
  return PRE_REFUSED;
}
