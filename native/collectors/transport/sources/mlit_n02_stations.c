/* collectors/transport/sources/mlit_n02_stations.c
 * Port of server/src/collectors/mlitN02Stations.js (createMlitKsjCollector).
 * REFERENCE source.c for the MLIT KSJ family (lib/mlit_ksj.h).
 * Primary live path only: env MLIT_N02_GEOJSON_URL → mirrors. The OSM
 * Overpass fallback + _meta envelope are dropped per HARD RULE 8 /
 * the documented JSON_API fallback-drop decision (0 rows when no MLIT
 * mirror responds is correctness-neutral, same as every other port). */
#include "../../../source.h"
#include "../../../lib/mlit_ksj.h"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const char *const mirrors[] = {
    "https://nlftp.mlit.go.jp/ksj/gml/data/N02/N02-23/N02-23_Station.geojson",
    "https://nlftp.mlit.go.jp/ksj/gml/data/N02/N02-22/N02-22_Station.geojson",
  };
  int n = mlit_ksj_collect(ctx, sink, "N02", "MLIT_N02_GEOJSON_URL",
                           mirrors, 2, 0);
  return n >= 0 ? 0 : -1;
}

static const source_def mlit_n02_stations_def = {
  .id = "mlit-n02-stations", .collector = "transport",
  .name = "MLIT N02 Rail Stations", .name_ja = "国土数値情報 鉄道駅 N02",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(mlit_n02_stations_def)
