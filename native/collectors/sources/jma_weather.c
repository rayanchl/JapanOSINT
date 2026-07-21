/* collectors/environment/sources/jma_weather.c
 * Port of server/src/collectors/jmaWeather.js. Only the single live Tokyo
 * overview-forecast feature is real upstream; the JS seed-supplement for the
 * other 46 prefectures is dropped (rule 8). The unused area.json fetch in the
 * JS Promise.all is omitted (its result is never read for the live feature). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>

#define TOKYO_URL "https://www.jma.go.jp/bosai/forecast/data/overview_forecast/130000.json"

/* PREFECTURES entry for code 130000 */
#define TOKYO_LON 139.692
#define TOKYO_LAT 35.689

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *fc = feed_get_json(ctx->http, TOKYO_URL, 5000);
  cJSON *features = cJSON_CreateArray();

  if (fc) {
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LON));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LAT));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
    cJSON_AddStringToObject(p, "prefecture_code", "130000");
    cJSON_AddStringToObject(p, "prefecture_name", "\xe6\x9d\xb1\xe4\xba\xac\xe9\x83\xbd");
    cJSON *txt = cJSON_GetObjectItem(fc, "text");
    cJSON_AddStringToObject(p, "weather_overview",
      (txt && cJSON_IsString(txt)) ? txt->valuestring : "");
    cJSON *rd = cJSON_GetObjectItem(fc, "reportDatetime");
    cJSON_AddItemToObject(p, "report_datetime",
      (rd && !cJSON_IsNull(rd)) ? cJSON_Duplicate(rd, 1) : cJSON_CreateNull());
    cJSON *ta = cJSON_GetObjectItem(fc, "targetArea");
    cJSON_AddStringToObject(p, "target_area",
      (ta && cJSON_IsString(ta)) ? ta->valuestring : "");
    cJSON_AddStringToObject(p, "source", "jma_live");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    cJSON_Delete(fc);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jma-weather] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def jma_weather_def = {
  .id = "jma-weather", .collector = "environment",
  .name = "JMA Weather Forecast", .name_ja = "気象庁 天気予報",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(jma_weather_def)
