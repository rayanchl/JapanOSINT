/* collectors/osint/sources/dehashed_search.c
 * OSINT service — faithful port of OSINTsaas osint_tools/dehashed_search.c
 * (dehashed_search → handle_dehashed_search). Canonical SERVICE id in
 * osint_dispatcher.c: {SERVICE_DEHASHED_SEARCH, handle_dehashed_search,
 * "DEHASHED_SEARCH", true}. Entity = email/username/ip/phone/hash/domain/
 * name (auto-detected by detect_query_type, mirrored verbatim). Two real
 * fetches: DeHashed via HTTP Basic auth (DEHASHED_EMAIL + DEHASHED_API_KEY —
 * skipped silently, no fabricated row, when either is absent), and the keyless
 * LeakCheck public endpoint. Neither is a hard gate; with no keys and no
 * breach the source is honestly empty. PER-RECORD EMIT: one osint_service_result
 * row per real breach source returned (body = {success,confidence,data}); no
 * breaches → emit nothing. The earlier port carried a Dehashed stub that never
 * authenticated — that placeholder is gone; the call below sends real Basic
 * auth and returns NULL on any failure. */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

/* base64 encode (for HTTP Basic auth). */
static char *b64(const char *in) {
  static const char *t =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t n = strlen(in);
  char *out = malloc(((n + 2) / 3) * 4 + 1);
  if (!out) return NULL;
  size_t o = 0, i = 0;
  while (i < n) {
    unsigned int a = (unsigned char)in[i++];
    unsigned int b = i < n ? (unsigned char)in[i++] : 0;
    unsigned int c = i < n ? (unsigned char)in[i++] : 0;
    unsigned int trip = (a << 16) | (b << 8) | c;
    out[o++] = t[(trip >> 18) & 0x3F];
    out[o++] = t[(trip >> 12) & 0x3F];
    out[o++] = (i > n + 1) ? '=' : t[(trip >> 6) & 0x3F];
    out[o++] = (i > n)     ? '=' : t[trip & 0x3F];
  }
  out[o] = 0;
  return out;
}

/* Dehashed: real authenticated fetch (HTTP Basic with DEHASHED_EMAIL +
 * DEHASHED_API_KEY). Keys absent → skip silently (NULL, no fabricated row).
 * Returns the parsed JSON `entries` array (caller owns the duplicated array)
 * or NULL on any failure/no-data. */
static cJSON *query_dehashed(http_client *http, const char *q, dtype_t t) {
  const char *email = getenv("DEHASHED_EMAIL");
  const char *key = getenv("DEHASHED_API_KEY");
  if (!email || !key || !*email || !*key) return NULL;   /* skip silently */

  char enc[512];
  jo_uri_encode_buf(q, enc, sizeof enc);
  char url[1024];
  snprintf(url, sizeof url, "https://api.dehashed.com/search?query=%s:%s",
           field_name(t), enc);

  char cred[640];
  snprintf(cred, sizeof cred, "%s:%s", email, key);
  char *b = b64(cred);
  if (!b) return NULL;
  char auth[896];
  snprintf(auth, sizeof auth, "Authorization: Basic %s", b);
  free(b);
  const char *hdr[3] = { auth, "Accept: application/json", NULL };

  http_response hr = {0};
  int hc = http_request(http, "GET", url, hdr, NULL, 0, 20000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *j = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!j) return NULL;
  cJSON *entries = cJSON_GetObjectItem(j, "entries");
  cJSON *dup = (entries && cJSON_IsArray(entries) && cJSON_GetArraySize(entries) > 0)
               ? cJSON_Duplicate(entries, 1) : NULL;
  cJSON_Delete(j);
  return dup;
}

/* LeakCheck public endpoint (keyless). Returns the real `sources` array on a
 * positive hit (caller owns the duplicate), else NULL (failure / no breach). */
static cJSON *query_leakcheck(http_client *http, const char *q) {
  char enc[512];
  jo_uri_encode_buf(q, enc, sizeof enc);
  char url[640];
  snprintf(url, sizeof url, "https://leakcheck.io/api/public?check=%s", enc);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *j = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!j) return NULL;
  cJSON *found = cJSON_GetObjectItem(j, "found");
  cJSON *dup = NULL;
  if (found && cJSON_IsNumber(found) && found->valueint > 0) {
    cJSON *src = cJSON_GetObjectItem(j, "sources");
    if (src && cJSON_IsArray(src) && cJSON_GetArraySize(src) > 0)
      dup = cJSON_Duplicate(src, 1);
  }
  cJSON_Delete(j);
  return dup;
}

/* Emit one intel item for a single breach/source. remote_key =
 * breach:<source>:<entity>. Returns 1 if emitted. */
static int emit_breach(intel_sink *sink, const char *q, const char *prov,
                       const char *src_name, cJSON *fields /*borrowed*/) {
  if (!src_name || !*src_name) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "entity", q);
  cJSON_AddStringToObject(data, "breach_source", src_name);
  cJSON_AddStringToObject(data, "provider", prov);
  /* carry any real extra fields (e.g. breach date) when present */
  if (fields && cJSON_IsObject(fields)) {
    cJSON *f;
    cJSON_ArrayForEach(f, fields) {
      if (f->string && strcmp(f->string, "name") != 0)
        cJSON_AddItemToObject(data, f->string, cJSON_Duplicate(f, 1));
    }
  }

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 90);
  cJSON_AddItemToObject(env, "data", cJSON_Duplicate(data, 1));
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DEHASHED_SEARCH");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddStringToObject(props, "breach_source", src_name);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 90);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[640]; snprintf(rk, sizeof rk, "breach:%s:%s", src_name, q);
  char title[420]; snprintf(title, sizeof title, "Breach: %s — %s", src_name, q);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "breach exposure";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"DEHASHED_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(env);
  return rc >= 0 ? 1 : 0;
}

/* Emit one item per source entry of a breach array. LeakCheck `sources` items
 * may be bare strings or objects {name,date}; Dehashed `entries` carry a
 * `database_name`. Returns count emitted. */
static int emit_from_array(intel_sink *sink, const char *q, const char *prov,
                           cJSON *arr) {
  int n = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    if (cJSON_IsString(e)) {
      n += emit_breach(sink, q, prov, e->valuestring, NULL);
    } else if (cJSON_IsObject(e)) {
      cJSON *nm = cJSON_GetObjectItem(e, "name");
      if (!nm) nm = cJSON_GetObjectItem(e, "database_name");
      if (nm && cJSON_IsString(nm))
        n += emit_breach(sink, q, prov, nm->valuestring, e);
    }
  }
  return n;
}

/* PER-RECORD EMIT: one item per real breach source found (LeakCheck public,
 * plus authenticated Dehashed when keys are set). No breaches / no keys →
 * emit nothing, return 0. */
static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;

  dtype_t t = detect_type(q);
  int emitted = 0;

  cJSON *lc = query_leakcheck(ctx->http, q);
  if (lc) { emitted += emit_from_array(sink, q, "LeakCheck", lc); cJSON_Delete(lc); }

  cJSON *dh = query_dehashed(ctx->http, q, t);   /* NULL unless keys set + hits */
  if (dh) { emitted += emit_from_array(sink, q, "Dehashed", dh); cJSON_Delete(dh); }

  (void)emitted;
  return 0;   /* honest empty is not an error */
}

static const source_def dehashed_search_def = {
  .id = "DEHASHED_SEARCH", .collector = "osint",
  .name = "Dehashed Search", .name_ja = "Dehashed検索",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/dehashed-search",
  .description = "Queries Dehashed and LeakCheck for breach exposure of an identifier",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(dehashed_search_def)
