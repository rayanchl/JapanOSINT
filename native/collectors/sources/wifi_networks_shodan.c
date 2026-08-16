/* collectors/cyber/sources/wifi_networks_shodan.c
 * Port of server/src/collectors/wifiNetworksShodan.js.
 * Shodan host/search (country:JP wifi OR "wireless" port:80,8080), key in
 * querystring → slice(0,50) → FeatureCollection. Gated on SHODAN_API_KEY. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Real coordinate or nothing. The JS this was ported from used
 * `loc.latitude || 35.6812` / `loc.longitude || 139.7671`, i.e. every Shodan
 * match whose GeoIP had no position was pinned to Tokyo Station — the exact
 * stacking the 2026-07-31 purge removed elsewhere and one of the residual
 * add_tokyo_geom() sites. A host with no location is a host with no location. */
static int coord_of(cJSON *loc, const char *k, double *out) {
  cJSON *v = loc ? cJSON_GetObjectItem(loc, k) : NULL;
  if (v && cJSON_IsNumber(v) && v->valuedouble != 0.0) { *out = v->valuedouble; return 1; }
  return 0;
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

      double glat = 0, glon = 0;
      int has_geo = coord_of(loc, "longitude", &glon) &
                    coord_of(loc, "latitude", &glat);

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      if (has_geo) {
        cJSON *g = cJSON_CreateObject();
        cJSON_AddStringToObject(g, "type", "Point");
        cJSON *co = cJSON_CreateArray();
        cJSON_AddItemToArray(co, cJSON_CreateNumber(glon));
        cJSON_AddItemToArray(co, cJSON_CreateNumber(glat));
        cJSON_AddItemToObject(g, "coordinates", co);
        cJSON_AddItemToObject(f, "geometry", g);
      } else {
        cJSON_AddNullToObject(f, "geometry");   /* Shodan gave no position */
      }

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
      if (has_geo) {
        /* unified provenance vocabulary — Shodan's position is GeoIP, which
         * resolves to a city/ISP area, never to the device. */
        cJSON_AddStringToObject(p, "geo_provenance", "shodan-geoip");
        cJSON_AddStringToObject(p, "geo_precision", "city");
        cJSON_AddBoolToObject(p, "geo_uncertain", 1);
      }
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
