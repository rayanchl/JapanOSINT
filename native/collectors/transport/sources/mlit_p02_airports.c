/* collectors/transport/sources/mlit_p02_airports.c
 * Port of server/src/collectors/mlitP02Airports.js (createMlitKsjCollector).
 * Primary live path only (env → mirrors); OSM fallback + _meta dropped per
 * HARD RULE 8. P02 geometry='centroid' (airport polygons → point). */
#include "../../../source.h"
#include "../../../lib/mlit_ksj.h"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const char *const mirrors[] = {
    "https://nlftp.mlit.go.jp/ksj/gml/data/P02/P02-22/P02-22.geojson",
    "https://nlftp.mlit.go.jp/ksj/gml/data/P02/P02-13/P02-13.geojson",
  };
  int n = mlit_ksj_collect(ctx, sink, "P02", "MLIT_P02_GEOJSON_URL",
                           mirrors, 2, 0);
  return n >= 0 ? 0 : -1;
}

static const source_def mlit_p02_airports_def = {
  .id = "mlit-p02-airports", .collector = "transport",
  .name = "MLIT P02 Airports", .name_ja = "MLIT P02 Airports",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(mlit_p02_airports_def)
