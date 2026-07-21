/* collectors/environment/sources/jma_earthquake.c
 * FEED source — port of server/src/collectors/jmaEarthquake.js.
 * https://www.jma.go.jp/bosai/quake/data/list.json -> one intel row/quake.
 * uid = jma-earthquake|<eid> (matches Node featureUid earthquake_id key). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://www.jma.go.jp/bosai/quake/data/list.json"

/* ISO 6709 "+38.7+141.9-50000/" -> lat,lon (first two signed numbers). */
static int parse_cod(const char *cod, double *lat, double *lon) {
  if (!cod) return 0;
  char *end;
  double a = strtod(cod, &end);
  if (end == cod) return 0;
  double b = strtod(end, &end);
  if (b == 0 && end == cod) return 0;
  *lat = a; *lon = b;
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *arr = feed_get_json(ctx->http, API_URL, 20000);
  if (!arr || !cJSON_IsArray(arr)) { if (arr) cJSON_Delete(arr); return -1; }
  int n = 0;
  cJSON *eq;
  cJSON_ArrayForEach(eq, arr) {
    const cJSON *eid = cJSON_GetObjectItem(eq, "eid");
    const cJSON *ttl = cJSON_GetObjectItem(eq, "ttl");
    const cJSON *anm = cJSON_GetObjectItem(eq, "anm");
    const cJSON *mag = cJSON_GetObjectItem(eq, "mag");
    const cJSON *at  = cJSON_GetObjectItem(eq, "at");
    const cJSON *cod = cJSON_GetObjectItem(eq, "cod");
    if (!eid || !cJSON_IsString(eid)) continue;

    double lat = 0, lon = 0;
    int geo = parse_cod(cJSON_IsString(cod) ? cod->valuestring : NULL, &lat, &lon);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "earthquake_id", eid->valuestring);
    if (mag) cJSON_AddStringToObject(props, "magnitude",
        cJSON_IsString(mag) ? mag->valuestring : "");
    if (anm && cJSON_IsString(anm))
      cJSON_AddStringToObject(props, "place", anm->valuestring);
    char *pj = cJSON_PrintUnformatted(props);

    intel_item it = {0};
    it.remote_key   = eid->valuestring;
    it.title        = (ttl && cJSON_IsString(ttl)) ? ttl->valuestring : NULL;
    it.summary      = (anm && cJSON_IsString(anm)) ? anm->valuestring : NULL;
    it.published_at = (at  && cJSON_IsString(at))  ? at->valuestring  : NULL;
    it.record_type  = "jma-earthquake";
    it.lang         = "ja";
    it.has_geo      = geo;
    it.lat = lat; it.lon = lon;
    it.properties_json = pj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj);
    cJSON_Delete(props);
  }
  cJSON_Delete(arr);
  fprintf(stderr, "[jma-earthquake] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def jma_earthquake_def = {
  .id = "jma-earthquake", .collector = "environment",
  .name = "JMA Earthquake", .name_ja = "気象庁 地震情報",
   .update_interval_sec = 60, .run = run,
};
REGISTER_SOURCE(jma_earthquake_def)
