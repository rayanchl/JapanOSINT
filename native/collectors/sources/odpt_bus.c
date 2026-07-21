/* collectors/transport/sources/odpt_bus.c
 * Port of server/src/collectors/odptRealtime.js::collectOdptBus.
 * Live ODPT v4 odpt:Bus pull when an ODPT token is set; honest empty
 * FeatureCollection without one. Token idiom mirrors odpt_transport.c. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *BASES[] = {
  "https://api.odpt.org/api/v4/",
  "https://api-challenge.odpt.org/api/v4/",
};

static const char *odpt_tok(void) {
  const char *t = getenv("ODPT_TOKEN");
  if (t && *t) return t;
  t = getenv("ODPT_CONSUMER_KEY");
  if (t && *t) return t;
  t = getenv("ODPT_CHALLENGE_TOKEN");
  if (t && *t) return t;
  return NULL;
}

static cJSON *odpt_get(http_client *http, const char *rdf, const char *tok) {
  for (size_t i = 0; i < sizeof BASES / sizeof BASES[0]; i++) {
    char url[512];
    snprintf(url, sizeof url, "%s%s?acl:consumerKey=%s", BASES[i], rdf, tok);
    cJSON *d = feed_get_json(http, url, 20000);
    if (d && cJSON_IsArray(d) && cJSON_GetArraySize(d) > 0) return d;
    if (d) cJSON_Delete(d);
  }
  return NULL;
}

static void s_or_null(cJSON *p, const char *outk, cJSON *r, const char *ink) {
  cJSON *v = cJSON_GetObjectItem(r, ink);
  if (v && cJSON_IsString(v) && v->valuestring[0])
    cJSON_AddStringToObject(p, outk, v->valuestring);
  else
    cJSON_AddItemToObject(p, outk, cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *tok = odpt_tok();
  if (!tok) {
    fprintf(stderr, "[odpt-bus] gated (no ODPT token)\n");
    return 0;
  }
  cJSON *rows = odpt_get(ctx->http, "odpt:Bus", tok);

  cJSON *features = cJSON_CreateArray();
  if (rows) {
    cJSON *r;
    cJSON_ArrayForEach(r, rows) {
      cJSON *la = cJSON_GetObjectItem(r, "geo:lat");
      cJSON *lo = cJSON_GetObjectItem(r, "geo:long");
      if (!la || cJSON_IsNull(la) || !lo || cJSON_IsNull(lo)) continue;
      double lat = cJSON_IsNumber(la) ? la->valuedouble
                 : (cJSON_IsString(la) ? strtod(la->valuestring, NULL) : 0);
      double lon = cJSON_IsNumber(lo) ? lo->valuedouble
                 : (cJSON_IsString(lo) ? strtod(lo->valuestring, NULL) : 0);

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *c = cJSON_CreateArray();
      cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(g, "coordinates", c);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
      cJSON *same = cJSON_GetObjectItem(r, "owl:sameAs");
      cJSON *atid = cJSON_GetObjectItem(r, "@id");
      if (same && cJSON_IsString(same) && same->valuestring[0])
        cJSON_AddStringToObject(p, "bus_uid", same->valuestring);
      else if (atid && cJSON_IsString(atid) && atid->valuestring[0])
        cJSON_AddStringToObject(p, "bus_uid", atid->valuestring);
      else
        cJSON_AddItemToObject(p, "bus_uid", cJSON_CreateNull());
      s_or_null(p, "operator", r, "odpt:operator");
      s_or_null(p, "route", r, "odpt:busroutePattern");
      cJSON_AddStringToObject(p, "source", "odpt_api");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
    cJSON_Delete(rows);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[odpt-bus] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def odpt_bus_def = {
  .id = "odpt-bus", .collector = "transport",
  .name = "ODPT Bus Data", .name_ja = "ODPT バスデータ",
   .update_interval_sec = 30, .run = run };
REGISTER_SOURCE(odpt_bus_def)
