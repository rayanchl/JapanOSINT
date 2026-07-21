/* collectors/transport/sources/mlit_n07_bus_routes.c
 * Port of server/src/collectors/mlitN07BusRoutes.js (createMlitKsjCollector).
 * Primary live path only (env → mirror); OSM fallback + _meta dropped per
 * HARD RULE 8. N07 geometry='preserve' (full LineStrings kept). */
#include "../../source.h"
#include "../../lib/mlit_ksj.h"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const char *const mirrors[] = {
    "https://nlftp.mlit.go.jp/ksj/gml/data/N07/N07-11/N07-11_BusRoute.geojson",
  };
  int n = mlit_ksj_collect(ctx, sink, "N07", "MLIT_N07_GEOJSON_URL",
                           mirrors, 1, 0);
  return n >= 0 ? 0 : -1;
}

static const source_def mlit_n07_bus_routes_def = {
  .id = "mlit-n07-bus-routes", .collector = "transport",
  .name = "MLIT N07 Bus Routes", .name_ja = "MLIT N07 Bus Routes",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(mlit_n07_bus_routes_def)
