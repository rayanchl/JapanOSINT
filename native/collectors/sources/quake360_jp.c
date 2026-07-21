/* collectors/cyber/sources/quake360_jp.c
 * Port of server/src/collectors/quake360Jp.js.
 * POST https://quake.360.net/api/v3/search/quake_service with header
 * X-QuakeToken: <QUAKE_API_KEY> and body {query,start,size,include,latest}.
 * json.data[] → Point features (geometry null if lat/lon not finite).
 * Key-gated: no QUAKE_API_KEY → 0 rows (mirrors JS quake_no_key).
 */
#include "../../source.h"
#include "../../lib/geojson.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://quake.360.net/api/v3/search/quake_service"
#define TIMEOUT_MS 20000

/* JS: r.a || null  — string value or JSON null. */
static cJSON *str_or_null(const cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  if (v && cJSON_IsString(v) && v->valuestring[0])
    return cJSON_CreateString(v->valuestring);
  if (v && cJSON_IsNumber(v))
    return cJSON_CreateNumber(v->valuedouble);
  return cJSON_CreateNull();
}

/* JS: a || b || null  — first non-empty string of two location keys. */
static cJSON *str_pair_or_null(const cJSON *loc, const char *ka,
                               const char *kb) {
  if (loc) {
    cJSON *a = cJSON_GetObjectItem(loc, ka);
    if (a && cJSON_IsString(a) && a->valuestring[0])
      return cJSON_CreateString(a->valuestring);
    cJSON *b = cJSON_GetObjectItem(loc, kb);
    if (b && cJSON_IsString(b) && b->valuestring[0])
      return cJSON_CreateString(b->valuestring);
  }
  return cJSON_CreateNull();
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("QUAKE_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[quake360-jp] gated (no QUAKE_API_KEY)\n");
    return 0;
  }

  const char *q = getenv("QUAKE_QUERY");
  if (!q || !*q) q = "country: \"Japan\"";

  cJSON *req = cJSON_CreateObject();
  cJSON_AddStringToObject(req, "query", q);
  cJSON_AddNumberToObject(req, "start", 0);
  cJSON_AddNumberToObject(req, "size", 100);
  cJSON *inc = cJSON_AddArrayToObject(req, "include");
  const char *inc_keys[] = { "ip", "port", "hostname", "transport", "asn",
                             "org", "service", "location", "time" };
  for (size_t i = 0; i < sizeof inc_keys / sizeof inc_keys[0]; i++)
    cJSON_AddItemToArray(inc, cJSON_CreateString(inc_keys[i]));
  cJSON_AddBoolToObject(req, "latest", 1);
  char *body = cJSON_PrintUnformatted(req);
  cJSON_Delete(req);

  char keyh[512];
  snprintf(keyh, sizeof keyh, "X-QuakeToken: %s", key);
  const char *hdrs[] = { keyh, "Content-Type: application/json",
                         "Accept: application/json", NULL };
  cJSON *json = feed_post_json(ctx->http, API_URL, body, hdrs, TIMEOUT_MS);
  free(body);
  if (!json) return -1;

  /* json.code && code !== 0 && code !== '0' → error (no rows). */
  cJSON *code = cJSON_GetObjectItem(json, "code");
  if (code) {
    int bad = 0;
    if (cJSON_IsNumber(code) && code->valuedouble != 0) bad = 1;
    else if (cJSON_IsString(code) && code->valuestring[0]
             && strcmp(code->valuestring, "0") != 0) bad = 1;
    if (bad) {
      cJSON *msg = cJSON_GetObjectItem(json, "message");
      fprintf(stderr, "[quake360-jp] error: %s\n",
        (msg && cJSON_IsString(msg)) ? msg->valuestring : "Quake error code");
      cJSON_Delete(json);
      return -1;
    }
  }

  cJSON *features = cJSON_CreateArray();
  cJSON *data = cJSON_GetObjectItem(json, "data");
  if (cJSON_IsArray(data)) {
    int i = 0;
    cJSON *r;
    cJSON_ArrayForEach(r, data) {
      cJSON *loc = cJSON_GetObjectItem(r, "location");
      cJSON *latv = loc ? cJSON_GetObjectItem(loc, "latitude") : NULL;
      cJSON *lonv = loc ? cJSON_GetObjectItem(loc, "longitude") : NULL;
      int has_geo = latv && cJSON_IsNumber(latv) && lonv && cJSON_IsNumber(lonv);

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      if (has_geo) {
        cJSON *g = cJSON_CreateObject();
        cJSON_AddStringToObject(g, "type", "Point");
        cJSON *co = cJSON_CreateArray();
        cJSON_AddItemToArray(co, cJSON_CreateNumber(lonv->valuedouble));
        cJSON_AddItemToArray(co, cJSON_CreateNumber(latv->valuedouble));
        cJSON_AddItemToObject(g, "coordinates", co);
        cJSON_AddItemToObject(f, "geometry", g);
      } else {
        cJSON_AddItemToObject(f, "geometry", cJSON_CreateNull());
      }

      cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
      cJSON *ipv = cJSON_GetObjectItem(r, "ip");
      cJSON *portv = cJSON_GetObjectItem(r, "port");
      const char *ips = (ipv && cJSON_IsString(ipv)) ? ipv->valuestring : "";
      char idbuf[256];
      if (portv && cJSON_IsNumber(portv))
        snprintf(idbuf, sizeof idbuf, "QUAKE_%s_%g_%d", ips, portv->valuedouble, i);
      else if (portv && cJSON_IsString(portv))
        snprintf(idbuf, sizeof idbuf, "QUAKE_%s_%s_%d", ips, portv->valuestring, i);
      else
        snprintf(idbuf, sizeof idbuf, "QUAKE_%s__%d", ips, i);
      cJSON_AddStringToObject(p, "id", idbuf);
      cJSON_AddItemToObject(p, "ip",
        ipv ? cJSON_Duplicate(ipv, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "port",
        portv ? cJSON_Duplicate(portv, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "hostname", str_or_null(r, "hostname"));
      cJSON_AddItemToObject(p, "transport", str_or_null(r, "transport"));
      cJSON_AddItemToObject(p, "asn", str_or_null(r, "asn"));
      cJSON_AddItemToObject(p, "org", str_or_null(r, "org"));
      cJSON *svc = cJSON_GetObjectItem(r, "service");
      cJSON_AddItemToObject(p, "service_name",
        svc ? str_or_null(svc, "name") : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "product",
        svc ? str_or_null(svc, "product") : cJSON_CreateNull());
      /* banner: typeof service.response === 'string' ? slice(0,240) : null */
      cJSON *resp = svc ? cJSON_GetObjectItem(svc, "response") : NULL;
      if (resp && cJSON_IsString(resp)) {
        char b[241];
        snprintf(b, sizeof b, "%.240s", resp->valuestring);
        cJSON_AddStringToObject(p, "banner", b);
      } else {
        cJSON_AddItemToObject(p, "banner", cJSON_CreateNull());
      }
      cJSON_AddItemToObject(p, "city",
        str_pair_or_null(loc, "city_en", "city_cn"));
      cJSON_AddItemToObject(p, "province",
        str_pair_or_null(loc, "province_en", "province_cn"));
      cJSON_AddItemToObject(p, "country",
        str_pair_or_null(loc, "country_en", "country_cn"));
      cJSON_AddItemToObject(p, "last_seen", str_or_null(r, "time"));
      cJSON_AddStringToObject(p, "source", "quake360_v3");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  cJSON_Delete(json);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[quake360-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def quake360_jp_def = {
  .id = "quake360-jp", .collector = "cyber",
  .name = "Quake (360 NetLab)", .name_ja = "Quake 360 日本",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(quake360_jp_def)
