/* collectors/transport/sources/mlit_c02_ports.c
 * Port of server/src/collectors/mlitC02Ports.js (createMlitKsjCollector).
 * Primary live path only (env → mirrors); OSM fallback + _meta dropped
 * per HARD RULE 8 / the documented fallback-drop decision. */
#include "../../../source.h"
#include "../../../lib/mlit_ksj.h"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const char *const mirrors[] = {
    "https://nlftp.mlit.go.jp/ksj/gml/data/C02/C02-22/C02-22.geojson",
    "https://nlftp.mlit.go.jp/ksj/gml/data/C02/C02-06/C02-06.geojson",
  };
  int n = mlit_ksj_collect(ctx, sink, "C02", "MLIT_C02_GEOJSON_URL",
                           mirrors, 2, 0);
  return n >= 0 ? 0 : -1;
}

static const source_def mlit_c02_ports_def = {
  .id = "mlit-c02-ports", .collector = "transport",
  .name = "MLIT C02 Ports", .name_ja = "MLIT C02 Ports",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(mlit_c02_ports_def)
