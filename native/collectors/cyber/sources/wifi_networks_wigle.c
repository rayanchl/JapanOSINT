/* collectors/cyber/sources/wifi_networks_wigle.c
 * Port of server/src/collectors/wifiNetworksWigle.js — PRIMARY tryWigleAPI()
 * path. WiGLE search API, gated on WIGLE_API_KEY (0 rows when unset). The
 * silent OSM Overpass fallback (JS runs both via Promise.allSettled and
 * concatenates) is intentionally not ported (correctness-neutral). */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>

static void passthru(cJSON *p, const char *outk, cJSON *r, const char *ink) {
  cJSON *v = cJSON_GetObjectItem(r, ink);
  cJSON_AddItemToObject(p, outk, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("WIGLE_API_KEY");
  if (!key || !*key) { fprintf(stderr, "[wifi-networks-wigle] no API key\n"); return -1; }

  char auth[256];
  snprintf(auth, sizeof auth, "Authorization: Basic %s", key);
  const char *hdrs[] = { auth, NULL };
  cJSON *data = feed_get_json_h(ctx->http,
    "https://api.wigle.net/api/v2/network/search?country=JP&latrange1=30"
    "&latrange2=45&longrange1=129&longrange2=146&resultsPerPage=100",
    hdrs, 10000);
  if (!data) return -1;

  cJSON *results = cJSON_GetObjectItem(data, "results");
  if (!cJSON_IsArray(results)) { cJSON_Delete(data); return -1; }

  cJSON *features = cJSON_CreateArray();
  int i = 0;
  cJSON *net;
  cJSON_ArrayForEach(net, results) {
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON *lon = cJSON_GetObjectItem(net, "trilong");
    cJSON *lat = cJSON_GetObjectItem(net, "trilat");
    cJSON_AddItemToArray(co, lon ? cJSON_Duplicate(lon, 1) : cJSON_CreateNull());
    cJSON_AddItemToArray(co, lat ? cJSON_Duplicate(lat, 1) : cJSON_CreateNull());
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();                 /* EXACT JS key order */
    char id[32];
    snprintf(id, sizeof id, "WIGLE_%d", i);
    cJSON_AddStringToObject(p, "id", id);
    passthru(p, "ssid", net, "ssid");
    passthru(p, "bssid", net, "netid");
    passthru(p, "encryption", net, "encryption");
    passthru(p, "channel", net, "channel");
    passthru(p, "last_seen", net, "lastupdt");
    cJSON_AddStringToObject(p, "source", "wigle_api");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    i++;
  }
  cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[wifi-networks-wigle] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def wifi_networks_wigle_def = {
  .id = "wifi-networks-wigle", .collector = "cyber",
  .name = "WiGLE WiFi Networks", .name_ja = "WiGLE WiFi ネットワーク",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(wifi_networks_wigle_def)
