/* collectors/telecom/sources/docomo_insight.c
 * Port of server/src/collectors/docomoInsight.js.
 * NTT Docomo Insight Data flow/visitor analytics — paid, contract-only, NO
 * public API. Gated honest-empty on DOCOMO_INSIGHT_API_KEY. With key:
 * attempt contract endpoint, build point features; honest empty on failure. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int coord(cJSON *r, double *lon, double *lat) {
  cJSON *v;
  if ((v = cJSON_GetObjectItem(r, "lon")) && cJSON_IsNumber(v)) *lon = v->valuedouble;
  else if ((v = cJSON_GetObjectItem(r, "longitude")) && cJSON_IsNumber(v)) *lon = v->valuedouble;
  else {
    cJSON *g = cJSON_GetObjectItem(r, "geometry");
    cJSON *c = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
    cJSON *x = c ? cJSON_GetArrayItem(c, 0) : NULL;
    if (!x || !cJSON_IsNumber(x)) return 0;
    *lon = x->valuedouble;
  }
  if ((v = cJSON_GetObjectItem(r, "lat")) && cJSON_IsNumber(v)) *lat = v->valuedouble;
  else if ((v = cJSON_GetObjectItem(r, "latitude")) && cJSON_IsNumber(v)) *lat = v->valuedouble;
  else {
    cJSON *g = cJSON_GetObjectItem(r, "geometry");
    cJSON *c = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
    cJSON *y = c ? cJSON_GetArrayItem(c, 1) : NULL;
    if (!y || !cJSON_IsNumber(y)) return 0;
    *lat = y->valuedouble;
  }
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("DOCOMO_INSIGHT_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[docomo-insight] gated (no DOCOMO_INSIGHT_API_KEY)\n");
    return 0;
  }
  char auth[600];
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", key);
  const char *hdrs[] = { auth, NULL };
  cJSON *data = feed_get_json_h(ctx->http,
    "https://api.docomo-datasquare.co.jp/v1/insight/flow?area=japan",
    hdrs, 20000);

  cJSON *rows = NULL;
  if (data) {
    cJSON *r = cJSON_GetObjectItem(data, "features");
    if (r && cJSON_IsArray(r)) rows = r;
    else if ((r = cJSON_GetObjectItem(data, "data")) && cJSON_IsArray(r)) rows = r;
  }
  if (!rows || cJSON_GetArraySize(rows) == 0) {
    if (data) cJSON_Delete(data);
    fprintf(stderr, "[docomo-insight] unavailable (no rows)\n");
    return -1;
  }

  cJSON *features = cJSON_CreateArray();
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    double lon, lat;
    if (!coord(r, &lon, &lat)) continue;
    cJSON *f = gj_point_feature(lon, lat);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddItemToObject(p, "area_code", jo_pick_dup(r, "area_code", "areaCode"));
    cJSON_AddItemToObject(p, "visitors", jo_pick_dup(r, "visitors", "value"));
    cJSON_AddItemToObject(p, "datetime", jo_pick_dup(r, "datetime", "timestamp"));
    cJSON_AddStringToObject(p, "source", "docomo_insight_api");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[docomo-insight] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def docomo_insight_def = {
  .id = "docomo-insight", .collector = "telecom",
  .name = "Docomo Insight", .name_ja = "ドコモ・インサイト",
  .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(docomo_insight_def)
