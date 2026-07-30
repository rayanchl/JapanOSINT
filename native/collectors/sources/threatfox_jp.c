/* collectors/cyber/sources/threatfox_jp.c
 * Port of server/src/collectors/threatfoxJp.js (createThreatIntelCollector).
 * env ABUSE_CH_AUTH_KEY (fallback THREATFOX_AUTH_KEY) → POST get_iocs days=N,
 * JP-relevant + confidence filter, slice 300, TOKYO points. */
#include "../../source.h"
#include "../../lib/threatintel.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TOKYO_LON 139.6917
#define TOKYO_LAT 35.6895
#define TF_URL "https://threatfox-api.abuse.ch/api/v1/"

static int is_word(unsigned char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '_';
}

/* /\.jp(\b|\/|:|$)/ on lowercased s */
static int has_dot_jp(const char *s) {
  for (const char *p = s; (p = strstr(p, ".jp")) != NULL; p += 3) {
    char nx = p[3];
    if (nx == '\0' || nx == '/' || nx == ':' || !is_word((unsigned char)nx))
      return 1;
  }
  return 0;
}

/* /\bWORD\b/ on lowercased s (WORD is all word-chars) */
static int has_word(const char *s, const char *w) {
  size_t wl = strlen(w);
  for (const char *p = s; (p = strstr(p, w)) != NULL; p += 1) {
    int lb = (p == s) || !is_word((unsigned char)p[-1]);
    int rb = !is_word((unsigned char)p[wl]);
    if (lb && rb) return 1;
  }
  return 0;
}

static void lc(const char *s, char *o, size_t n) {
  size_t j = 0;
  if (!s) { o[0] = '\0'; return; }
  for (; *s && j + 1 < n; s++) o[j++] = (char)tolower((unsigned char)*s);
  o[j] = '\0';
}

static int jp_relevant(cJSON *ioc) {
  cJSON *iv = cJSON_GetObjectItem(ioc, "ioc_value");
  char v[1024];
  lc((iv && cJSON_IsString(iv)) ? iv->valuestring : "", v, sizeof v);
  if (has_dot_jp(v)) return 1;
  cJSON *tags = cJSON_GetObjectItem(ioc, "tags");
  char joined[2048]; joined[0] = '\0';
  if (cJSON_IsArray(tags)) {
    cJSON *t; int first = 1;
    cJSON_ArrayForEach(t, tags) {
      if (!cJSON_IsString(t)) continue;
      if (!first) strncat(joined, ",", sizeof joined - strlen(joined) - 1);
      strncat(joined, t->valuestring, sizeof joined - strlen(joined) - 1);
      first = 0;
    }
  }
  char tl[2048];
  lc(joined, tl, sizeof tl);
  if (has_word(tl, "japan") || has_word(tl, "jp")) return 1;
  return 0;
}

static void put(cJSON *p, const char *k, cJSON *r, const char *ik) {
  cJSON *v = cJSON_GetObjectItem(r, ik);
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)ud;
  const char *ds = getenv("THREATFOX_DAYS");
  long days = (ds && *ds) ? strtol(ds, NULL, 10) : 3;
  const char *cs = getenv("THREATFOX_MIN_CONFIDENCE");
  long min_conf = (cs && *cs) ? strtol(cs, NULL, 10) : 75;

  char body[128];
  snprintf(body, sizeof body, "{\"query\":\"get_iocs\",\"days\":%ld}", days);
  char keyh[256];
  snprintf(keyh, sizeof keyh, "auth-key: %s", key);
  const char *hdrs[] = { keyh, "accept: application/json",
                         "content-type: application/json", NULL };
  http_response resp = {0};
  if (http_request(ctx->http, "POST", TF_URL, hdrs, body, strlen(body),
                    15000, 2, &resp) != 0 || resp.status < 200 ||
      resp.status >= 300 || !resp.body) {
    http_response_free(&resp);
    return NULL;
  }
  cJSON *json = cJSON_Parse(resp.body);
  http_response_free(&resp);
  if (!json) return NULL;

  cJSON *data = cJSON_GetObjectItem(json, "data");
  cJSON *features = cJSON_CreateArray();
  int i = 0;
  if (cJSON_IsArray(data)) {
    cJSON *d;
    cJSON_ArrayForEach(d, data) {
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
      if (!jp_relevant(d)) continue;
      cJSON *clv = cJSON_GetObjectItem(d, "confidence_level");
      double cl = (clv && cJSON_IsNumber(clv)) ? clv->valuedouble : 0; /* ?? 0 */
      if (cl < (double)min_conf) continue;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LON));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LAT));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *pr = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON_AddNumberToObject(pr, "idx", i);
      put(pr, "ioc_id", d, "id");
      put(pr, "ioc_value", d, "ioc_value");
      put(pr, "ioc_type", d, "ioc_type");
      put(pr, "threat_type", d, "threat_type");
      put(pr, "malware", d, "malware");
      put(pr, "malware_alias", d, "malware_alias");
      put(pr, "first_seen", d, "first_seen");
      put(pr, "last_seen", d, "last_seen");
      put(pr, "confidence_level", d, "confidence_level");
      put(pr, "reporter", d, "reporter");
      put(pr, "reference", d, "reference");
      put(pr, "tags", d, "tags");
      cJSON_AddStringToObject(pr, "source", "threatfox");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  cJSON_Delete(json);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *fb[] = { "THREATFOX_AUTH_KEY", NULL };
  int n = threatintel_collect(ctx, sink, "ABUSE_CH_AUTH_KEY", fb,
                              run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def threatfox_jp_def = {
  .id = "threatfox-jp", .collector = "cyber",
  .name = "abuse.ch ThreatFox (JP)", .name_ja = "abuse.ch ThreatFox 日本",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(threatfox_jp_def)
