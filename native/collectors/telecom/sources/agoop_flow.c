/* collectors/telecom/sources/agoop_flow.c
 * Port of server/src/collectors/agoopFlow.js.
 * Agoop flow-population (GPS-derived) — paid, contract-only, NO public API.
 * Gated honest-empty on AGOOP_API_KEY. With key: attempt contract endpoint,
 * build point features; honest empty on failure. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include "../../../third_party/cJSON.h"
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

static cJSON *pick(cJSON *r, const char *a, const char *b) {
  cJSON *v = cJSON_GetObjectItem(r, a);
  if (v && !cJSON_IsNull(v)) return cJSON_Duplicate(v, 1);
  v = cJSON_GetObjectItem(r, b);
  if (v && !cJSON_IsNull(v)) return cJSON_Duplicate(v, 1);
  return cJSON_CreateNull();
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("AGOOP_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[agoop-flow] gated (no AGOOP_API_KEY)\n");
    return 0;
  }
  char auth[600];
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", key);
  const char *hdrs[] = { auth, NULL };
  cJSON *data = feed_get_json_h(ctx->http,
    "https://api.agoop.net/v1/flow-population?area=japan", hdrs, 20000);

  cJSON *rows = NULL;
  if (data) {
    cJSON *r = cJSON_GetObjectItem(data, "features");
    if (r && cJSON_IsArray(r)) rows = r;
    else if ((r = cJSON_GetObjectItem(data, "data")) && cJSON_IsArray(r)) rows = r;
  }
  if (!rows || cJSON_GetArraySize(rows) == 0) {
    if (data) cJSON_Delete(data);
    fprintf(stderr, "[agoop-flow] unavailable (no rows)\n");
    return -1;
  }

  cJSON *features = cJSON_CreateArray();
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    double lon, lat;
    if (!coord(r, &lon, &lat)) continue;
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddItemToObject(p, "mesh_code", pick(r, "mesh_code", "meshCode"));
    cJSON_AddItemToObject(p, "flow_population", pick(r, "flow_population", "value"));
    cJSON_AddItemToObject(p, "datetime", pick(r, "datetime", "timestamp"));
    cJSON_AddStringToObject(p, "source", "agoop_flow_api");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[agoop-flow] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def agoop_flow_def = {
  .id = "agoop-flow", .collector = "telecom",
  .name = "Agoop People Flow", .name_ja = "Agoop 人流データ",
  .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(agoop_flow_def)
