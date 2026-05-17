/* collectors/transport/sources/mlit_n05_rail_history.c
 * Port of server/src/collectors/mlitN05RailHistory.js (createMlitKsjCollector).
 * Primary live path only (env → mirrors); _meta dropped per HARD RULE 8.
 * filter:(f)=>f.properties.status==='abolished' → abolished_only=1
 * (surfaced in-app as "Abandoned Rail"; active rail = unified-trains). */
#include "../../../source.h"
#include "../../../lib/mlit_ksj.h"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const char *const mirrors[] = {
    "https://nlftp.mlit.go.jp/ksj/gml/data/N05/N05-22/N05-22.geojson",
    "https://nlftp.mlit.go.jp/ksj/gml/data/N05/N05-21/N05-21.geojson",
  };
  int n = mlit_ksj_collect(ctx, sink, "N05", "MLIT_N05_GEOJSON_URL",
                           mirrors, 2, 1);
  return n >= 0 ? 0 : -1;
}

static const source_def mlit_n05_rail_history_def = {
  .id = "mlit-n05-rail-history", .collector = "transport",
  .name = "MLIT N05 Rail History", .name_ja = "MLIT N05 Rail History",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(mlit_n05_rail_history_def)
