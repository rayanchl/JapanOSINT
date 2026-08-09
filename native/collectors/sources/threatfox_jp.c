/* collectors/cyber/sources/threatfox_jp.c
 * Port of server/src/collectors/threatfoxJp.js (createThreatIntelCollector).
 * env ABUSE_CH_AUTH_KEY (fallback THREATFOX_AUTH_KEY) → POST get_iocs days=N,
 * JP-relevant + confidence filter, slice 300, TOKYO points. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/threatintel.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* TOKYO_LON/TOKYO_LAT deliberately removed — see the note at the feature
 * builder. Do not reintroduce a fallback coordinate for indicators. */
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

static int jp_relevant(cJSON *ioc) {
  cJSON *iv = cJSON_GetObjectItem(ioc, "ioc_value");
  char v[1024];
  jo_lower_buf((iv && cJSON_IsString(iv)) ? iv->valuestring : "", v, sizeof v);
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
  jo_lower_buf(joined, tl, sizeof tl);
  if (has_word(tl, "japan") || has_word(tl, "jp")) return 1;
  return 0;
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
      if (i >= 300) break;                     /* slice(0,300) */
      if (!jp_relevant(d)) continue;
      cJSON *clv = cJSON_GetObjectItem(d, "confidence_level");
      double cl = (clv && cJSON_IsNumber(clv)) ? clv->valuedouble : 0; /* ?? 0 */
      if (cl < (double)min_conf) continue;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      /* NO GEOMETRY. Every row used to be pinned at Tokyo Station
       * (139.6917, 35.6895) — an indicator of compromise is a hash, a URL or
       * an IP; it does not have a location, and stacking 300 of them on one
       * point is the exact fabrication the 2026-07-31 audit deleted from
       * cisa-kev-jp, poc-in-github and mastodon-jp-instances.
       *
       * It survived that sweep only because this source was already broken:
       * abuse.ch had moved threatfox-api behind an Auth-Key, so the collector
       * returned 401 and emitted zero rows, and a row-count audit cannot see
       * the geometry of rows that never arrive. Restoring the fetch without
       * removing this would have ADDED 300 fake Tokyo pins to the map.
       * lib/geojson.c treats an absent geometry correctly (no "null" string). */

      cJSON *pr = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON_AddNumberToObject(pr, "idx", i);
      jo_put_or_null(pr, "ioc_id", d, "id");
      jo_put_or_null(pr, "ioc_value", d, "ioc_value");
      jo_put_or_null(pr, "ioc_type", d, "ioc_type");
      jo_put_or_null(pr, "threat_type", d, "threat_type");
      jo_put_or_null(pr, "malware", d, "malware");
      jo_put_or_null(pr, "malware_alias", d, "malware_alias");
      jo_put_or_null(pr, "first_seen", d, "first_seen");
      jo_put_or_null(pr, "last_seen", d, "last_seen");
      jo_put_or_null(pr, "confidence_level", d, "confidence_level");
      jo_put_or_null(pr, "reporter", d, "reporter");
      jo_put_or_null(pr, "reference", d, "reference");
      jo_put_or_null(pr, "tags", d, "tags");
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
