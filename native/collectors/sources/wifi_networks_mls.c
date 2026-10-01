/* collectors/cyber/sources/wifi_networks_mls.c
 *
 * FABRICATION REMOVED (2026-08-09). This file used to carry a hardcoded table
 * of 20 access points — sequential invented BSSIDs (00:1A:79:00:00:01,
 * …:02, …:03, then 00:26:5A:…, 00:0D:02:…) placed at Tokyo/Osaka/Narita
 * landmark coordinates — and emitted them as Points tagged
 * `"source": "mozilla_mls"`, every 86400 s. There was no fetch anywhere in the
 * file. The key gate made it worse rather than better: the rows appeared only
 * once an operator set MLS_API_KEY, i.e. exactly when they were expecting real
 * Mozilla Location Service data.
 *
 * There is no endpoint to replace it with: Mozilla retired MLS and the geolocate
 * / geosubmit APIs no longer serve BSSID observations. Per the source authoring
 * contract (R1 real fetch, R2 no invented geometry) the correct behaviour is to
 * emit nothing, so that is what this does. The source stays registered so its
 * id, catalog entry and any operator config keep resolving; if an MLS successor
 * with a public BSSID endpoint appears, wire the fetch in here.
 */
#include "source.h"
#include <stdio.h>

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx; (void)sink;
  fprintf(stderr, "[wifi-networks-mls] no upstream: Mozilla Location Service "
                  "is retired and publishes no BSSID observations; emitting "
                  "nothing rather than placeholder access points\n");
  return 0;                                /* nothing fetched is not an error */
}

static const source_def wifi_networks_mls_def = {
  .id = "wifi-networks-mls", .collector = "cyber",
  .name = "Mozilla Location Services WiFi",
  .name_ja = "Mozilla \xe3\x83\xad\xe3\x82\xb1\xe3\x83\xbc\xe3\x82\xb7\xe3\x83\xa7\xe3\x83\xb3\xe3\x82\xb5\xe3\x83\xbc\xe3\x83\x93\xe3\x82\xb9 WiFi",
  .description = "Retired upstream (Mozilla Location Service shut down); emits no rows.",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(wifi_networks_mls_def)
