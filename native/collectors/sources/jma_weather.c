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
    cJSON *f = gj_point_feature(TOKYO_LON, TOKYO_LAT);

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
    /* Readable identity for the UI: geojson_emit_features maps
     * properties.title/summary/link/published_at onto the intel_item, and
     * without them the row renders blank everywhere. All four are built from
     * the fetched payload — nothing invented. */
    {
      const char *area = (ta && cJSON_IsString(ta) && ta->valuestring[0])
                       ? ta->valuestring : "\xe6\x9d\xb1\xe4\xba\xac\xe9\x83\xbd";
      char title[256];
      snprintf(title, sizeof title,
               "%s \xe3\x81\xae\xe5\xa4\xa9\xe6\xb0\x97\xe6\xa6\x82\xe6\xb3\x81", area); /* …の天気概況 */
      cJSON_AddStringToObject(p, "title", title);
      if (txt && cJSON_IsString(txt) && txt->valuestring[0])
        cJSON_AddStringToObject(p, "summary", txt->valuestring);
      cJSON_AddStringToObject(p, "link",
        "https://www.jma.go.jp/bosai/forecast/#area_type=offices&area_code=130000");
      if (rd && cJSON_IsString(rd) && rd->valuestring[0])
        cJSON_AddStringToObject(p, "published_at", rd->valuestring);
    }
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
