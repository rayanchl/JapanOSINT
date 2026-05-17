/* collectors/environment/sources/jma_tsunami.c
 * FEED source — port of server/src/collectors/jmaTsunami.js.
 * https://www.jma.go.jp/bosai/tsunami/data/list.json (key-free, ISO 6709 cod)
 * -> one Feature per advisory; emitted via geojson toolkit.
 * Honest empty on fetch failure / no active bulletins. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define API_URL "https://www.jma.go.jp/bosai/tsunami/data/list.json"

/* JMA ISO 6709 "+39.8+143.2-20000/" -> lat/lon/depthKm. mirrors parseIso6709. */
static int parse_iso6709(const char *cod, double *lat, double *lon,
                         int *has_depth, double *depth_km) {
  if (!cod) return 0;
  char *end;
  double a = strtod(cod, &end);
  if (end == cod) return 0;
  const char *p = end;
  double b = strtod(p, &end);
  if (end == p) return 0;
  if (!isfinite(a) || !isfinite(b)) return 0;
  *lat = a; *lon = b;
  *has_depth = 0;
  const char *q = end;
  if (*q == '+' || *q == '-') {
    char *e2;
    double dm = strtod(q, &e2);
    if (e2 != q && isfinite(dm)) { *has_depth = 1; *depth_km = fabs(dm) / 1000.0; }
  }
  return 1;
}

static const char *s_str(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *arr = feed_get_json(ctx->http, API_URL, 8000);
  if (!arr || !cJSON_IsArray(arr)) {
    if (arr) cJSON_Delete(arr);
    fprintf(stderr, "[jma-tsunami] upstream unavailable\n");
    return -1;
  }
  cJSON *features = cJSON_CreateArray();
  cJSON *t;
  cJSON_ArrayForEach(t, arr) {
    double lat = 0, lon = 0, depth = 0;
    int has_depth = 0;
    cJSON *codv = cJSON_GetObjectItem(t, "cod");
    if (!parse_iso6709(cJSON_IsString(codv) ? codv->valuestring : NULL,
                       &lat, &lon, &has_depth, &depth))
      continue;

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
    const char *eid = s_str(t, "eid");
    if (eid) cJSON_AddStringToObject(p, "bulletin_id", eid);
    else cJSON_AddNullToObject(p, "bulletin_id");
    const char *ttl = s_str(t, "ttl");
    if (!ttl) ttl = s_str(t, "en_ttl");
    if (ttl) cJSON_AddStringToObject(p, "title", ttl);
    else cJSON_AddNullToObject(p, "title");
    const char *ift = s_str(t, "ift");
    if (ift) cJSON_AddStringToObject(p, "information_type", ift);
    else cJSON_AddNullToObject(p, "information_type");
    const char *anm = s_str(t, "anm");
    if (anm) cJSON_AddStringToObject(p, "area_name", anm);
    else cJSON_AddNullToObject(p, "area_name");
    const char *en_anm = s_str(t, "en_anm");
    if (en_anm) cJSON_AddStringToObject(p, "area_name_en", en_anm);
    else cJSON_AddNullToObject(p, "area_name_en");
    cJSON *magv = cJSON_GetObjectItem(t, "mag");
    if (magv && cJSON_IsString(magv) && magv->valuestring[0])
      cJSON_AddNumberToObject(p, "magnitude", strtod(magv->valuestring, NULL));
    else if (magv && cJSON_IsNumber(magv))
      cJSON_AddNumberToObject(p, "magnitude", magv->valuedouble);
    else cJSON_AddNullToObject(p, "magnitude");
    if (has_depth) cJSON_AddNumberToObject(p, "depth_km", depth);
    else cJSON_AddNullToObject(p, "depth_km");
    const char *at = s_str(t, "at");
    if (at) cJSON_AddStringToObject(p, "origin_time", at);
    else cJSON_AddNullToObject(p, "origin_time");
    const char *rdt = s_str(t, "rdt");
    if (rdt) cJSON_AddStringToObject(p, "report_time", rdt);
    else cJSON_AddNullToObject(p, "report_time");
    const char *ser = s_str(t, "ser");
    if (ser) cJSON_AddStringToObject(p, "serial", ser);
    else cJSON_AddNullToObject(p, "serial");
    cJSON *kind = cJSON_GetObjectItem(t, "kind");
    cJSON_AddItemToObject(p, "kind",
      (kind && cJSON_IsArray(kind)) ? cJSON_Duplicate(kind, 1)
                                    : cJSON_CreateArray());
    const char *json = s_str(t, "json");
    if (json) cJSON_AddStringToObject(p, "json", json);
    else cJSON_AddNullToObject(p, "json");
    cJSON_AddStringToObject(p, "source", "jma_tsunami");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  cJSON_Delete(arr);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jma-tsunami] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def jma_tsunami_def = {
  .id = "jma-tsunami", .collector = "environment",
  .name = "JMA Tsunami Warnings", .name_ja = "気象庁 津波情報",
  .update_interval_sec = 60, .run = run,
};
REGISTER_SOURCE(jma_tsunami_def)
