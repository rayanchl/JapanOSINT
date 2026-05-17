/* collectors/transport/sources/msil_umishiru.c
 * Port of server/src/collectors/msilUmishiru.js. JCG MDA "Umishiru" ports API
 * (key-gated). With UMISHIRU_API_KEY → single bbox GET → Features. Without a
 * key the JS emits only the curated SEED, which rule 8 drops → 0 rows. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>

#define API_URL "https://portal.msil.go.jp/api/v1/ports?bbox=122,24,146,46&limit=1000"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *k = getenv("UMISHIRU_API_KEY");
  if (!k || !*k) {
    fprintf(stderr, "[msil-umishiru] gated (no UMISHIRU_API_KEY)\n");
    return 0;                                   /* JS: SEED-only → dropped */
  }

  char keyh[256];
  snprintf(keyh, sizeof keyh, "Ocp-Apim-Subscription-Key: %s", k);
  const char *hdrs[] = { keyh, NULL };
  cJSON *data = feed_get_json_h(ctx->http, API_URL, hdrs, 10000);
  if (!data) return -1;

  /* list = Array.isArray(data) ? data : (data?.features ?? data?.items ?? []) */
  cJSON *list = NULL;
  if (cJSON_IsArray(data)) list = data;
  else {
    cJSON *fe = cJSON_GetObjectItem(data, "features");
    if (cJSON_IsArray(fe)) list = fe;
    else {
      cJSON *it = cJSON_GetObjectItem(data, "items");
      if (cJSON_IsArray(it)) list = it;
    }
  }

  cJSON *features = cJSON_CreateArray();
  if (list) {
    cJSON *p;
    cJSON_ArrayForEach(p, list) {
      /* lat = p?.lat ?? p?.geometry?.coordinates?.[1] */
      cJSON *lat = cJSON_GetObjectItem(p, "lat");
      cJSON *lon = cJSON_GetObjectItem(p, "lon");
      cJSON *geom = cJSON_GetObjectItem(p, "geometry");
      cJSON *gc = geom ? cJSON_GetObjectItem(geom, "coordinates") : NULL;
      if ((!lat || cJSON_IsNull(lat)) && gc && cJSON_IsArray(gc))
        lat = cJSON_GetArrayItem(gc, 1);
      if ((!lon || cJSON_IsNull(lon)) && gc && cJSON_IsArray(gc))
        lon = cJSON_GetArrayItem(gc, 0);
      if (!lat || cJSON_IsNull(lat) || !lon || cJSON_IsNull(lon)) continue;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(cJSON_GetNumberValue(lon)));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(cJSON_GetNumberValue(lat)));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *pr = cJSON_CreateObject();          /* EXACT JS key order */
      /* name = p.name ?? p.properties?.name ?? null */
      cJSON *nm = cJSON_GetObjectItem(p, "name");
      if (!nm || cJSON_IsNull(nm)) {
        cJSON *pp = cJSON_GetObjectItem(p, "properties");
        nm = pp ? cJSON_GetObjectItem(pp, "name") : NULL;
      }
      cJSON_AddItemToObject(pr, "name",
        (nm && !cJSON_IsNull(nm)) ? cJSON_Duplicate(nm, 1) : cJSON_CreateNull());
      cJSON_AddStringToObject(pr, "source", "msil_umishiru");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
    }
  }
  cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[msil-umishiru] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def msil_umishiru_def = {
  .id = "msil-umishiru", .collector = "transport",
  .name = "MSIL Umishiru Public APIs", .name_ja = "海しる 公開API",
   .update_interval_sec = 900, .run = run,
};
REGISTER_SOURCE(msil_umishiru_def)
