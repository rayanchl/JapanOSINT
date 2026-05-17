/* collectors/cyber/sources/wifi_networks_mls.c
 * Port of server/src/collectors/wifiNetworksMls.js — curated common-Japan
 * WiFi BSSID anchors, gated on MLS_API_KEY (empty FeatureCollection / 0 rows
 * when unset; JS returns []). _meta envelope not ported. */
#include "../../../source.h"
#include "../../../lib/geojson.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

struct ap { const char *bssid, *ssid; int channel; double lat, lon; };
static const struct ap APS[] = {
  { "00:1A:79:00:00:01", "docomo Wi-Fi",          1, 35.6812, 139.7671 },
  { "00:1A:79:00:00:02", "au Wi-Fi SPOT",         6, 35.6896, 139.7006 },
  { "00:1A:79:00:00:03", "SoftBank Wi-Fi",       11, 35.6580, 139.7016 },
  { "00:26:5A:00:00:01", "FON_FREE_INTERNET",     1, 35.6835, 139.7021 },
  { "00:26:5A:00:00:02", "FON_FREE_INTERNET",     6, 35.7074, 139.6655 },
  { "00:0D:02:00:00:01", "NTT-SPOT",              1, 35.6825, 139.7650 },
  { "00:0D:02:00:00:02", "Japan-Free-WiFi",       6, 35.6586, 139.7454 },
  { "00:0D:02:00:00:03", "Japan-Free-WiFi",      11, 35.7148, 139.7967 },
  { "AC:22:0B:00:00:01", "LAWSON_Free_Wi-Fi",     1, 35.6920, 139.7030 },
  { "AC:22:0B:00:00:02", "FamilyMart_Wi-Fi",      6, 35.6590, 139.7000 },
  { "AC:22:0B:00:00:03", "7SPOT",                11, 35.6984, 139.7731 },
  { "00:1B:8B:00:00:01", "at_STARBUCKS_Wi2",      1, 35.6710, 139.7650 },
  { "00:1B:8B:00:00:02", "Wi2premium_club",       6, 35.6867, 139.7660 },
  { "00:1B:8B:00:00:03", "TRAVEL_JAPAN_Wi-Fi",   11, 34.7024, 135.4959 },
  { "00:23:69:00:00:01", "Metro_Free_Wi-Fi",      1, 35.6717, 139.7637 },
  { "00:23:69:00:00:02", "Shinkansen_Free_Wi-Fi", 6, 35.1709, 136.8815 },
  { "00:23:69:00:00:03", "JR-EAST_FREE_Wi-Fi",   11, 35.6812, 139.7671 },
  { "00:26:5A:00:00:03", "DOUTOR_FREE_Wi-Fi",     1, 35.7300, 139.7120 },
  { "00:26:5A:00:00:04", "KANSAI-FREE-WIFI",      6, 34.4320, 135.2302 },
  { "00:26:5A:00:00:05", "FREE_Wi-Fi_NARITA",    11, 35.7720, 140.3929 },
};
#define NAP ((int)(sizeof(APS)/sizeof(APS[0])))

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("MLS_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[wifi-networks-mls] gated (no MLS_API_KEY)\n");
    return 0;
  }

  cJSON *features = cJSON_CreateArray();
  for (int i = 0; i < NAP; i++) {
    const struct ap *a = &APS[i];
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(a->lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(a->lat));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();          /* EXACT JS key order */
    char idbuf[16]; snprintf(idbuf, sizeof idbuf, "MLS_%d", i);
    cJSON_AddStringToObject(p, "id", idbuf);
    cJSON_AddStringToObject(p, "bssid", a->bssid);
    cJSON_AddStringToObject(p, "ssid", a->ssid);
    cJSON_AddNumberToObject(p, "channel", a->channel);
    cJSON_AddStringToObject(p, "source", "mozilla_mls");
    cJSON_AddItemToObject(f, "properties", p);

    cJSON_AddItemToArray(features, f);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[wifi-networks-mls] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def wifi_networks_mls_def = {
  .id = "wifi-networks-mls", .collector = "cyber",
  .name = "Mozilla Location Services WiFi",
  .name_ja = "Mozilla \xe3\x83\xad\xe3\x82\xb1\xe3\x83\xbc\xe3\x82\xb7\xe3\x83\xa7\xe3\x83\xb3\xe3\x82\xb5\xe3\x83\xbc\xe3\x83\x93\xe3\x82\xb9 WiFi",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(wifi_networks_mls_def)
