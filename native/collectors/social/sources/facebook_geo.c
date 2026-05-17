/* collectors/social/sources/facebook_geo.c
 * Port of server/src/collectors/facebookGeo.js (FeatureCollection).
 * Graph API place search around Tokyo; gated on FACEBOOK_ACCESS_TOKEN
 * (JS: no token / failure → empty FeatureCollection → 0 rows).
 * Feature props order: id, platform, place_name, checkins, source. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *tok = getenv("FACEBOOK_ACCESS_TOKEN");
  if (!tok || !*tok) {
    fprintf(stderr, "[facebook-geo] gated (no FACEBOOK_ACCESS_TOKEN)\n");
    return 0;
  }

  char url[512];
  snprintf(url, sizeof url,
    "https://graph.facebook.com/v18.0/search?type=place"
    "&center=35.6762,139.6503&distance=50000"
    "&fields=name,location,checkins&access_token=%s", tok);

  cJSON *data = feed_get_json(ctx->http, url, 8000);
  if (!data) return -1;

  cJSON *arr = cJSON_GetObjectItem(data, "data");
  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(arr)) {
    cJSON *place;
    cJSON_ArrayForEach(place, arr) {
      cJSON *loc = cJSON_GetObjectItem(place, "location");
      cJSON *lon = loc ? cJSON_GetObjectItem(loc, "longitude") : NULL;
      cJSON *lat = loc ? cJSON_GetObjectItem(loc, "latitude") : NULL;
      /* filter: longitude != null && latitude != null */
      if (!lon || cJSON_IsNull(lon) || !lat || cJSON_IsNull(lat)) continue;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_Duplicate(lon, 1));
      cJSON_AddItemToArray(co, cJSON_Duplicate(lat, 1));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *p = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON *idv = cJSON_GetObjectItem(place, "id");
      cJSON_AddStringToObject(p, "id",
        (idv && cJSON_IsString(idv) && idv->valuestring)
          ? idv->valuestring : "");
      cJSON_AddStringToObject(p, "platform", "facebook");
      cJSON *nm = cJSON_GetObjectItem(place, "name");
      cJSON_AddStringToObject(p, "place_name",
        (nm && cJSON_IsString(nm) && nm->valuestring) ? nm->valuestring : "");
      cJSON *ck = cJSON_GetObjectItem(place, "checkins");
      cJSON_AddNumberToObject(p, "checkins",
        (ck && cJSON_IsNumber(ck)) ? ck->valuedouble : 0); /* checkins||0 */
      cJSON_AddStringToObject(p, "source", "facebook_api");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
  }
  cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[facebook-geo] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def facebook_geo_def = {
  .id = "facebook-geo", .collector = "social",
  .name = "Facebook Check-ins", .name_ja = "Facebook チェックイン",
   .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(facebook_geo_def)
