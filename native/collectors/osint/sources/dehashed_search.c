/* collectors/osint/sources/dehashed_search.c
 * OSINT service — faithful port of OSINTsaas osint_tools/dehashed_search.c
 * (dehashed_search → handle_dehashed_search). Canonical SERVICE id in
 * osint_dispatcher.c: {SERVICE_DEHASHED_SEARCH, handle_dehashed_search,
 * "DEHASHED_SEARCH", true}. Entity = email/username/ip/phone/hash/domain/
 * name (auto-detected by detect_query_type, mirrored verbatim). Keys
 * DEHASHED_EMAIL + DEHASHED_API_KEY are read but the upstream Dehashed call
 * is itself only a structured placeholder (it never sends Basic auth), and
 * LeakCheck is keyless — so this is NOT a hard gate; upstream always returns
 * a success=true row. We reproduce its exact `root`:
 *  {query,timestamp,detected_type,results:[<dehashed-stub|err>,<leakcheck>],
 *   total_breaches_found,compromised,recommendation|note,disclaimer}
 * success=true, confidence 90 if total_found>0 else 70. Emits one
 * osint_service_result row (body = {success,confidence,data}). */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void uri_encode(const char *in, char *out, size_t cap) {
  static const char *keep = "-_.!~*'()";
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && w + 4 < cap; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(keep, c)) out[w++] = (char)c;
    else { snprintf(out + w, cap - w, "%%%02X", c); w += 3; }
  }
  out[w] = 0;
}

typedef enum { D_EMAIL, D_USERNAME, D_IP, D_PHONE, D_PASSWORD,
               D_HASH, D_NAME, D_DOMAIN } dtype_t;

/* Verbatim port of OSINTsaas detect_query_type. */
static dtype_t detect_type(const char *q) {
  if (!q) return D_EMAIL;
  if (strchr(q, '@')) return q[0] == '@' ? D_DOMAIN : D_EMAIL;
  int dots = 0, isip = 1;
  for (const char *p = q; *p && isip; p++) {
    if (*p == '.') dots++;
    else if (!isdigit((unsigned char)*p)) isip = 0;
  }
  if (isip && dots == 3) return D_IP;
  int dc = 0, tc = 0;
  for (const char *p = q; *p; p++) {
    if (isdigit((unsigned char)*p)) dc++;
    if (!isspace((unsigned char)*p)) tc++;
  }
  if (tc > 0 && (double)dc / tc > 0.7 && dc >= 7) return D_PHONE;
  size_t len = strlen(q);
  if (len == 32 || len == 40 || len == 64) {
    int hex = 1;
    for (const char *p = q; *p && hex; p++) {
      char c = tolower((unsigned char)*p);
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) hex = 0;
    }
    if (hex) return D_HASH;
  }
  if (strchr(q, '.') && !strchr(q, ' ')) return D_DOMAIN;
  if (strchr(q, ' ')) return D_NAME;
  return D_USERNAME;
}

static const char *field_name(dtype_t t) {
  switch (t) {
    case D_EMAIL: return "email";
    case D_USERNAME: return "username";
    case D_IP: return "ip_address";
    case D_PHONE: return "phone";
    case D_PASSWORD: return "password";
    case D_HASH: return "hashed_password";
    case D_NAME: return "name";
    case D_DOMAIN: return "domain";
    default: return "email";
  }
}

/* Dehashed: upstream returns a structured placeholder (no live call). */
static cJSON *query_dehashed(const char *q, dtype_t t) {
  const char *email = getenv("DEHASHED_EMAIL");
  const char *key = getenv("DEHASHED_API_KEY");
  if (!email || !key || !*email || !*key) {
    fprintf(stderr, "[DEHASHED_SEARCH] gated (no DEHASHED_EMAIL/API_KEY)\n");
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "error", "Dehashed credentials not configured");
    cJSON_AddStringToObject(r, "note",
      "Set DEHASHED_EMAIL and DEHASHED_API_KEY environment variables");
    return r;
  }
  char enc[512];
  uri_encode(q, enc, sizeof enc);
  char url[1024];
  snprintf(url, sizeof url, "https://api.dehashed.com/search?query=%s:%s",
           field_name(t), enc);
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "Dehashed");
  cJSON_AddStringToObject(r, "query", q);
  cJSON_AddStringToObject(r, "search_type", field_name(t));
  cJSON_AddStringToObject(r, "api_url", url);
  cJSON_AddStringToObject(r, "note",
    "Dehashed API requires HTTP Basic Auth. Configure API client for full integration.");
  return r;
}

/* LeakCheck public endpoint (keyless). */
static cJSON *query_leakcheck(http_client *http, const char *q) {
  char enc[512];
  uri_encode(q, enc, sizeof enc);
  char url[640];
  snprintf(url, sizeof url, "https://leakcheck.io/api/public?check=%s", enc);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    http_response_free(&hr);
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "source", "LeakCheck");
    cJSON_AddStringToObject(r, "status", "error");
    cJSON_AddStringToObject(r, "note",
      "LeakCheck public API may be rate limited or unavailable");
    return r;
  }
  cJSON *j = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!j) {
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "source", "LeakCheck");
    cJSON_AddStringToObject(r, "status", "parse_error");
    return r;
  }
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "LeakCheck");
  cJSON *found = cJSON_GetObjectItem(j, "found");
  if (found && cJSON_IsNumber(found)) {
    cJSON_AddNumberToObject(r, "breaches_found", found->valueint);
    if (found->valueint > 0) {
      cJSON_AddBoolToObject(r, "compromised", 1);
      cJSON *src = cJSON_GetObjectItem(j, "sources");
      if (src && cJSON_IsArray(src))
        cJSON_AddItemToObject(r, "sources", cJSON_Duplicate(src, 1));
    } else {
      cJSON_AddBoolToObject(r, "compromised", 0);
    }
  }
  cJSON_Delete(j);
  return r;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  dtype_t t = detect_type(q);
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", q);
  cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));
  cJSON_AddStringToObject(root, "detected_type", field_name(t));

  cJSON *sources = cJSON_CreateArray();
  cJSON_AddItemToArray(sources, query_dehashed(q, t));
  cJSON_AddItemToArray(sources, query_leakcheck(ctx->http, q));
  cJSON_AddItemToObject(root, "results", sources);

  int total = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, sources) {
    cJSON *f = cJSON_GetObjectItem(s, "breaches_found");
    if (f && cJSON_IsNumber(f)) total += f->valueint;
  }
  cJSON_AddNumberToObject(root, "total_breaches_found", total);
  if (total > 0) {
    cJSON_AddBoolToObject(root, "compromised", 1);
    cJSON_AddStringToObject(root, "recommendation",
      "This identifier appears in breach databases. Consider: "
      "1) Changing associated passwords, "
      "2) Enabling 2FA, "
      "3) Monitoring for suspicious activity");
  } else {
    cJSON_AddBoolToObject(root, "compromised", 0);
    cJSON_AddStringToObject(root, "note",
      "No breaches found in queried databases. Note that this does not guarantee "
      "the identifier has never been compromised.");
  }
  cJSON_AddStringToObject(root, "disclaimer",
    "Breach data is provided for security awareness and authorized investigations only. "
    "Do not use this service for harassment, identity theft, or unauthorized access.");

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", total > 0 ? 90 : 70);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DEHASHED_SEARCH");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", total > 0 ? 90 : 70);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300], title[320];
  snprintf(rk, sizeof rk, "dehashed:%s", q);
  snprintf(title, sizeof title, "DEHASHED_SEARCH — %s", q);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = total > 0 ? "breach exposure found" : "no breach exposure";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"DEHASHED_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def dehashed_search_def = {
  .id = "DEHASHED_SEARCH", .collector = "osint",
  .name = "Dehashed Search", .name_ja = "Dehashed検索",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(dehashed_search_def)
