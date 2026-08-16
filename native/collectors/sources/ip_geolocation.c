/* collectors/osint/sources/ip_geolocation.c
 * OSINT service — port of net.js ipGeolocation(). On-demand (interval 0); the
 * OSINT dispatcher runs it with ctx->entity = an IP/host. Upstream:
 * ip-api.com free JSON (no key); fields=66846719 (the full field set net.js
 * requests). Mirrors net.js gating: !r.ok → fail; j.status!=="success" → fail;
 * else ok(j).
 *
 * PER-RECORD EMIT: single-entity lookup yielding ONE real record. Emits ONE
 * clean intel_item carrying the real ip-api geo fields directly (NOT a
 * {success,confidence,data} envelope). remote_key = "ip:<ip>". has_geo/lat/lon
 * are set from the response. On HTTP failure / status!=success → emit NOTHING
 * and return 0 (honest empty). */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* encodeURIComponent: keep A-Za-z0-9 and - _ . ! ~ * ' ( ); %-encode rest. */
static void uri_encode(const char *in, char *out, size_t cap) {
  static const char *keep = "-_.!~*'()";
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && w + 4 < cap; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(keep, c)) {
      out[w++] = (char)c;
    } else {
      snprintf(out + w, cap - w, "%%%02X", c);
      w += 3;
    }
  }
  out[w] = 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return 0;

  char enc[256];
  uri_encode(entity, enc, sizeof enc);
  char url[512];
  snprintf(url, sizeof url,
           "http://ip-api.com/json/%s?fields=66846719", enc);

  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 10000, 2, &hr);
  long status = hr.status;
  cJSON *j = (hc == 0 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);

  /* net.js: if (!r.ok) → fail. HTTP failure → honest empty. */
  if (hc != 0 || status < 200 || status >= 300 || !j) {
    if (j) cJSON_Delete(j);
    return 0;
  }

  /* net.js: if (j.status !== 'success') → fail. Not found → honest empty. */
  const cJSON *st = cJSON_GetObjectItemCaseSensitive(j, "status");
  if (!cJSON_IsString(st) || strcmp(st->valuestring, "success") != 0) {
    cJSON_Delete(j);
    return 0;
  }

  /* Body = the real ip-api geo object (full j minus the status flag). */
  cJSON_DeleteItemFromObjectCaseSensitive(j, "status");
  char *bj = cJSON_PrintUnformatted(j);

  const cJSON *city_v = cJSON_GetObjectItemCaseSensitive(j, "city");
  const cJSON *cc_v   = cJSON_GetObjectItemCaseSensitive(j, "country");
  const cJSON *q_v    = cJSON_GetObjectItemCaseSensitive(j, "query");
  const cJSON *lat_v  = cJSON_GetObjectItemCaseSensitive(j, "lat");
  const cJSON *lon_v  = cJSON_GetObjectItemCaseSensitive(j, "lon");
  const char *city = (cJSON_IsString(city_v) && city_v->valuestring) ? city_v->valuestring : "";
  const char *cc   = (cJSON_IsString(cc_v) && cc_v->valuestring) ? cc_v->valuestring : "";
  const char *ip   = (cJSON_IsString(q_v) && q_v->valuestring) ? q_v->valuestring : entity;

  int has_geo = 0; double lat = 0, lon = 0;
  if (cJSON_IsNumber(lat_v) && cJSON_IsNumber(lon_v)) {
    lat = lat_v->valuedouble;
    lon = lon_v->valuedouble;
    has_geo = 1;
  }

  char title[360];
  snprintf(title, sizeof title, "%s — %s, %s", ip,
           city[0] ? city : "?", cc[0] ? cc : "?");

  char summary[200];
  snprintf(summary, sizeof summary, "%s%s%s",
           city[0] ? city : "", (city[0] && cc[0]) ? ", " : "", cc);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "IP_GEOLOCATION");
  cJSON_AddStringToObject(props, "entity", entity);
  if (city[0]) cJSON_AddStringToObject(props, "city", city);
  if (cc[0])   cJSON_AddStringToObject(props, "country", cc);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "ip:%s", ip);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = summary[0] ? summary : NULL;
  it.has_geo         = has_geo;
  it.lat             = lat;
  it.lon             = lon;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"IP_GEOLOCATION\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(j);
  return rc >= 0 ? 0 : -1;
}

static const source_def ip_geolocation_def = {
  .id = "IP_GEOLOCATION", .collector = "osint",
  .name = "IP Geolocation", .name_ja = "IPジオロケーション",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/ip-geolocation",
  .description = "Geolocate an IP/host via ip-api.com.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(ip_geolocation_def)
