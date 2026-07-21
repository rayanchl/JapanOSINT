/* collectors/transport/sources/mlit_p11_bus_stops.c
 * Port of server/src/collectors/mlitP11BusStops.js (createMlitKsjCollector).
 * Primary live path only (env → mirrors); OSM fallback + _meta dropped per
 * HARD RULE 8. P11 geometry='preserve'. */
#include "../../source.h"
#include "../../lib/mlit_ksj.h"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const char *const mirrors[] = {
    "https://nlftp.mlit.go.jp/ksj/gml/data/P11/P11-22/P11-22.geojson",
    "https://nlftp.mlit.go.jp/ksj/gml/data/P11/P11-10/P11-10.geojson",
  };
  int n = mlit_ksj_collect(ctx, sink, "P11", "MLIT_P11_GEOJSON_URL",
                           mirrors, 2, 0);
  return n >= 0 ? 0 : -1;
}

static const source_def mlit_p11_bus_stops_def = {
  .id = "mlit-p11-bus-stops", .collector = "transport",
  .name = "MLIT P11 Bus Stops", .name_ja = "MLIT P11 Bus Stops",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(mlit_p11_bus_stops_def)
