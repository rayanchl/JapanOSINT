/* collectors/cyber/sources/urlhaus_jp.c
 * Port of server/src/collectors/urlhausJp.js (createThreatIntelCollector).
 * env ABUSE_CH_AUTH_KEY (fallback URLHAUS_AUTH_KEY) → POST recent urls,
 * JP-host filter, slice 200. Emitted WITHOUT geometry: URLhaus gives no host
 * coordinates, so we do not invent any (the JS upstream stamped every row with a
 * hardcoded Tokyo point — dropped here per the no-fabricated-data rule). */
#include "../../source.h"
#include "../../lib/threatintel.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define UH_URL "https://urlhaus-api.abuse.ch/v1/urls/recent/"

static int is_word(unsigned char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '_';
}

/* /\.jp(\b|\/|:|$)/ on lowercased s */
static int has_dot_jp_b(const char *s) {
  for (const char *p = s; (p = strstr(p, ".jp")) != NULL; p += 3) {
    char nx = p[3];
    if (nx == '\0' || nx == '/' || nx == ':' || !is_word((unsigned char)nx))
      return 1;
  }
  return 0;
}
/* /\.jp\b/ on lowercased s */
static int has_dot_jp_wb(const char *s) {
  for (const char *p = s; (p = strstr(p, ".jp")) != NULL; p += 3) {
    if (!is_word((unsigned char)p[3])) return 1;
  }
  return 0;
}

static void lc(const char *s, char *o, size_t n) {
  size_t j = 0;
  if (!s) { o[0] = '\0'; return; }
  for (; *s && j + 1 < n; s++) o[j++] = (char)tolower((unsigned char)*s);
  o[j] = '\0';
}

static const char *S(cJSON *r, const char *k) {
  cJSON *v = cJSON_GetObjectItem(r, k);
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

/* host = entry.host || entry.url || '' */
static int is_jp(cJSON *e) {
  const char *h = S(e, "host");
  if (!h || !h[0]) h = S(e, "url");
  char host[2048];
  lc(h ? h : "", host, sizeof host);
  if (has_dot_jp_b(host)) return 1;
  const char *ref = S(e, "urlhaus_reference");
  char r[2048];
  lc(ref ? ref : "", r, sizeof r);
  return has_dot_jp_wb(r);
}

static void put(cJSON *p, const char *k, cJSON *r, const char *ik) {
  cJSON *v = cJSON_GetObjectItem(r, ik);
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)ud;
  const char *body = "{}";
  char keyh[256];
  snprintf(keyh, sizeof keyh, "auth-key: %s", key);
  const char *hdrs[] = { keyh, "accept: application/json",
                         "content-type: application/json", NULL };
  http_response resp = {0};
  if (http_request(ctx->http, "POST", UH_URL, hdrs, body, strlen(body),
                    15000, 2, &resp) != 0 || resp.status < 200 ||
      resp.status >= 300 || !resp.body) {
    http_response_free(&resp);
    return NULL;
  }
  cJSON *json = cJSON_Parse(resp.body);
  http_response_free(&resp);
  if (!json) return NULL;

  cJSON *urls = cJSON_GetObjectItem(json, "urls");
  cJSON *features = cJSON_CreateArray();
  int i = 0;
  if (cJSON_IsArray(urls)) {
    cJSON *u;
    cJSON_ArrayForEach(u, urls) {
      if (!is_jp(u)) continue;
      if (i >= 200) break;                     /* slice(0,200) of filtered */

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      /* no geometry: URLhaus gives no coordinates — emit as non-geo intel */

      cJSON *pr = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON_AddNumberToObject(pr, "idx", i);
      put(pr, "url", u, "url");
      put(pr, "host", u, "host");
      put(pr, "threat", u, "threat");
      put(pr, "tags", u, "tags");
      put(pr, "url_status", u, "url_status");
      put(pr, "date_added", u, "date_added");
      put(pr, "reporter", u, "reporter");
      put(pr, "reference", u, "urlhaus_reference");
      cJSON_AddStringToObject(pr, "source", "urlhaus");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  cJSON_Delete(json);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *fb[] = { "URLHAUS_AUTH_KEY", NULL };
  int n = threatintel_collect(ctx, sink, "ABUSE_CH_AUTH_KEY", fb,
                              run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def urlhaus_jp_def = {
  .id = "urlhaus-jp", .collector = "cyber",
  .name = "abuse.ch URLhaus (JP)", .name_ja = "abuse.ch URLhaus 日本",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(urlhaus_jp_def)
