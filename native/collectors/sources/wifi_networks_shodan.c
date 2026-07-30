/* collectors/cyber/sources/wifi_networks_shodan.c
 * Port of server/src/collectors/wifiNetworksShodan.js.
 * Shodan host/search (country:JP wifi OR "wireless" port:80,8080), key in
 * querystring → slice(0,50) → FeatureCollection. Gated on SHODAN_API_KEY. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* JS x || fallback for a coordinate: number !=0 → number, else fallback. */
static double coord_or(cJSON *loc, const char *k, double fb) {
  cJSON *v = loc ? cJSON_GetObjectItem(loc, k) : NULL;
  if (v && cJSON_IsNumber(v) && v->valuedouble != 0.0) return v->valuedouble;
  return fb;
}

/* passthrough (== JS m.x; absent → JSON null, abuse-style faithful) */
static void passthru(cJSON *p, const char *k, cJSON *m, const char *ik) {
  cJSON *v = cJSON_GetObjectItem(m, ik);
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *k = getenv("SHODAN_API_KEY");
  if (!k || !*k) {
    fprintf(stderr, "[wifi-networks-shodan] gated (no SHODAN_API_KEY)\n");
    return 0;
  }
  /* encodeURIComponent('country:JP wifi OR "wireless" port:80,8080') */
  char url[512];
  snprintf(url, sizeof url,
    "https://api.shodan.io/shodan/host/search?key=%s&query="
    "country%%3AJP%%20wifi%%20OR%%20%%22wireless%%22%%20port%%3A80%%2C8080",
    k);
  cJSON *data = feed_get_json(ctx->http, url, 15000);
  cJSON *matches = data ? cJSON_GetObjectItem(data, "matches") : NULL;

  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(matches)) {
    int i = 0;
    cJSON *m;
    cJSON_ArrayForEach(m, matches) {
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
      cJSON *loc = cJSON_GetObjectItem(m, "location");

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(coord_or(loc, "longitude", 139.7671)));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(coord_or(loc, "latitude", 35.6812)));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
      char idb[32];
      snprintf(idb, sizeof idb, "SHODAN_%d", i);
      cJSON_AddStringToObject(p, "id", idb);
      passthru(p, "ip", m, "ip_str");
      passthru(p, "port", m, "port");
      passthru(p, "org", m, "org");
      passthru(p, "isp", m, "isp");
      passthru(p, "product", m, "product");
      cJSON *cityv = loc ? cJSON_GetObjectItem(loc, "city") : NULL;
      cJSON_AddItemToObject(p, "city",
        cityv ? cJSON_Duplicate(cityv, 1) : cJSON_CreateNull());
      cJSON_AddStringToObject(p, "source", "shodan_wifi");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[wifi-networks-shodan] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def wifi_networks_shodan_def = {
  .id = "wifi-networks-shodan", .collector = "cyber",
  .name = "Shodan WiFi Devices", .name_ja = "Shodan WiFi デバイス",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(wifi_networks_shodan_def)
