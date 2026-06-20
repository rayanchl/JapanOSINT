/* collectors/cyber/sources/wifi_networks.c
 * Port of server/src/collectors/wifiNetworks.js — composite that fans out to
 * the three already-registered WiFi sub-collectors (wigle / shodan / mls) and
 * merges their FeatureCollections. No `kind` tags, no dedupe, no filter, no
 * postProcess. _meta envelope dropped per RULE 8.
 * NOTE: no source_registry.gen.c row for "wifi-networks"; category derived
 * from the WiFi domain → "cyber" (matches the three sub-collectors). */
#include "../../source.h"
#include "../../lib/unified.h"
#include "../../third_party/cJSON.h"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const unified_upstream ups[] = {
    { "wifi-networks-wigle",  NULL },
    { "wifi-networks-shodan", NULL },
    { "wifi-networks-mls",    NULL },
  };
  int n = unified_collect(ctx, sink, "wifi-networks", ups, 3,
                          NULL, 0, 4, NULL, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def wifi_networks_def = {
  .id = "wifi-networks", .collector = "cyber",
  .name = "WiFi Networks (merged)",
  .name_ja = "WiFi \xe3\x83\x8d\xe3\x83\x83\xe3\x83\x88\xe3\x83\xaf\xe3\x83\xbc\xe3\x82\xaf (\xe7\xb5\xb1\xe5\x90\x88)",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(wifi_networks_def)
