/* collectors/sources/cyi_subdomain_center.c
 * OSINT service — SUBDOMAIN_CENTER. Domain pivot against ARPSyndicate's
 * independently-built subdomain corpus. Different collection method from the
 * CT-log/search-engine enumeration the existing SUBDOMAIN_FINDER uses, so it
 * surfaces names those miss.
 * Endpoint: https://api.subdomain.center/?domain=<domain>             (keyless)
 * parse_notes: "Bare JSON array of strings — no envelope, no counts. Unknown
 * domains return []; that is an honest empty, so return 0 not -1 (R3). Results
 * include noisy wildcard-expansion artefacts, so sanity-check that each name
 * actually ends with the queried domain." Both checks are implemented: an empty
 * array returns 0, and every name must be the domain itself or end in
 * "." + domain or it is dropped.
 * Emits one row per surviving subdomain — each a literal string from the reply.
 * No coordinates -> has_geo 0 (R2).
 * Licence: ARPSyndicate free API; rate-limited, attribution requested.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static int looks_like_domain(const char *s) {
  if (!s || !*s) return 0;
  if (strchr(s, '@') || strchr(s, '/') || strchr(s, ' ') || strchr(s, ':')) return 0;
  size_t n = strlen(s);
  if (n < 3 || n > 253) return 0;
  const char *dot = strrchr(s, '.');
  if (!dot || dot == s || !dot[1]) return 0;
  for (const char *p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (!(isalnum(c) || c == '-' || c == '.' || c == '_' || c >= 0x80)) return 0;
  }
  if ((unsigned char)dot[1] < 0x80 && !isalpha((unsigned char)dot[1])) return 0;
  return 1;
}

static int ci_eq(const char *a, const char *b) {
  for (; *a && *b; a++, b++)
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
  return *a == *b;
}

/* name must BE the domain or end with "." + domain */
static int under_domain(const char *name, const char *domain) {
  size_t nl = strlen(name), dl = strlen(domain);
  if (nl == dl) return ci_eq(name, domain);
  if (nl < dl + 1) return 0;
  if (name[nl - dl - 1] != '.') return 0;
  return ci_eq(name + nl - dl, domain);
}

static char *http_get(const source_ctx *ctx, const char *url, long *status) {
  http_response hr = {0};
  const char *hdrs[] = { "Accept: application/json", NULL };
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 30000, 1, &hr);
  *status = hr.status;
  if (rc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  char *b = hr.body; hr.body = NULL; http_response_free(&hr);
  return b;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_domain(q)) return 0;             /* wrong shape -> no-op */

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char url[512];
  snprintf(url, sizeof url, "https://api.subdomain.center/?domain=%s", enc);
  free(enc);

  long status = 0;
  char *body = http_get(ctx, url, &status);
  if (!body) {
    fprintf(stderr, "[SUBDOMAIN_CENTER] http status=%ld\n", status);
    if (status >= 400 && status < 500) return 0;
    return -1;
  }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[SUBDOMAIN_CENTER] unparseable body\n"); return -1; }
  if (!cJSON_IsArray(root)) {
    cJSON_Delete(root);
    fprintf(stderr, "[SUBDOMAIN_CENTER] unexpected shape for %s\n", q);
    return 0;
  }

  int n = 0, dropped = 0;
  const cJSON *e;
  cJSON_ArrayForEach(e, root) {
    if (!cJSON_IsString(e) || !e->valuestring || !e->valuestring[0]) continue;
    const char *name = e->valuestring;
    if (!under_domain(name, q)) { dropped++; continue; }   /* wildcard artefact */

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "service", "SUBDOMAIN_CENTER");
    cJSON_AddStringToObject(p, "subdomain", name);
    cJSON_AddStringToObject(p, "parent_domain", q);
    cJSON_AddStringToObject(p, "source", "api.subdomain.center");
    cJSON_AddBoolToObject(p, "success", 1);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char summary[320];
    snprintf(summary, sizeof summary, "Subdomain of %s (Subdomain Center)", q);

    intel_item it = {0};
    it.remote_key      = name;
    it.title           = name;
    it.summary         = summary;
    it.link            = url;
    it.lang            = "en";
    it.record_type     = "subdomain";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"cyber\",\"dns\",\"subdomain\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(root);
  fprintf(stderr, "[SUBDOMAIN_CENTER] emitted %d for %s (%d out-of-scope dropped)\n",
          n, q, dropped);
  return 0;                        /* an unknown domain returns [] -> 0 (R3) */
}

static const source_def cyi_subdomain_center_def = {
  .id = "SUBDOMAIN_CENTER", .collector = "osint",
  .name = "Subdomain Center (ARPSyndicate)",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://api.subdomain.center/",
  .description = "An independently-built subdomain corpus, keyless — a different collection method from CT-log and search-engine enumeration, so it surfaces names those miss.",
  .license = "ARPSyndicate free API; rate-limited, attribution requested.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cyi_subdomain_center_def)
