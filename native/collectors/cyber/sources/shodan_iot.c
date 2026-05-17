/* collectors/cyber/sources/shodan_iot.c
 * Port of server/src/collectors/shodanIot.js.
 * Shodan host/search?query=country:JP (key in querystring) → FeatureCollection.
 * Gated on SHODAN_API_KEY. No seed. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* JS Number.isFinite(x): true only for a real JSON number. */
static int finite_num(cJSON *v, double *out) {
  if (v && cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  return 0;
}

/* v || 'def'  (string-or-default) */
static void s_or(cJSON *p, const char *k, cJSON *m, const char *ik,
                 const char *def) {
  cJSON *v = cJSON_GetObjectItem(m, ik);
  if (v && cJSON_IsString(v) && v->valuestring[0])
    cJSON_AddStringToObject(p, k, v->valuestring);
  else
    cJSON_AddStringToObject(p, k, def);
}

static void passthru(cJSON *p, const char *k, cJSON *m, const char *ik) {
  cJSON *v = cJSON_GetObjectItem(m, ik);
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *k = getenv("SHODAN_API_KEY");
  if (!k || !*k) {
    fprintf(stderr, "[shodan-iot] gated (no SHODAN_API_KEY)\n");
    return 0;
  }
  char url[512];
  snprintf(url, sizeof url,
    "https://api.shodan.io/shodan/host/search?key=%s&query=country:JP&facets=port",
    k);
  cJSON *data = feed_get_json(ctx->http, url, 10000);
  cJSON *matches = data ? cJSON_GetObjectItem(data, "matches") : NULL;

  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(matches)) {
    int i = 0;
    cJSON *m;
    cJSON_ArrayForEach(m, matches) {
      cJSON *loc = cJSON_GetObjectItem(m, "location");
      double lon, lat;
      int hasgeo = loc &&
        finite_num(cJSON_GetObjectItem(loc, "longitude"), &lon) &&
        finite_num(cJSON_GetObjectItem(loc, "latitude"), &lat);
      if (!hasgeo) continue;            /* .filter(Number.isFinite...) */

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
      char idb[32];
      snprintf(idb, sizeof idb, "SHODAN_LIVE_%d", i);
      cJSON_AddStringToObject(p, "id", idb);
      passthru(p, "ip", m, "ip_str");
      passthru(p, "port", m, "port");
      s_or(p, "product", m, "product", "unknown");
      s_or(p, "device_type", m, "devicetype", "unknown");
      s_or(p, "os", m, "os", "unknown");
      cJSON *dv = cJSON_GetObjectItem(m, "data");
      const char *ds = (dv && cJSON_IsString(dv)) ? dv->valuestring : "";
      char banner[201];
      size_t bl = strlen(ds);
      if (bl > 200) bl = 200;
      memcpy(banner, ds, bl);
      banner[bl] = '\0';
      cJSON_AddStringToObject(p, "banner", banner);
      cJSON *cityv = loc ? cJSON_GetObjectItem(loc, "city") : NULL;
      cJSON_AddStringToObject(p, "city",
        (cityv && cJSON_IsString(cityv)) ? cityv->valuestring : "");
      passthru(p, "last_seen", m, "timestamp");
      cJSON_AddStringToObject(p, "source", "shodan_api");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[shodan-iot] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def shodan_iot_def = {
  .id = "shodan-iot", .collector = "cyber",
  .name = "Shodan IoT Devices", .name_ja = "Shodan IoT デバイス",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(shodan_iot_def)
