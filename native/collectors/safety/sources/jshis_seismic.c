/* collectors/safety/sources/jshis_seismic.c
 * Port of server/src/collectors/jshisSeismic.js. NIED J-SHIS PSHM geojson;
 * slice(0,1000), pass geometry through, mesh_id = JSHIS_<sha1(JSON.stringify
 * (geometry))[:20]> (== intelHashKey). SEED branch dropped (rule 8). */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>

#define API_URL "https://www.j-shis.bosai.go.jp/map/api/pshm/Y2020/PT01/T30_I50_PS.geojson"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *data = feed_get_json(ctx->http, API_URL, 15000);
  if (!data) return -1;

  cJSON *src = cJSON_GetObjectItem(data, "features");
  if (!src || !cJSON_IsArray(src) || cJSON_GetArraySize(src) == 0) {
    cJSON_Delete(data);
    return -1;                                  /* JS: return null → not live */
  }

  cJSON *features = cJSON_CreateArray();
  int i = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, src) {
    if (i++ >= 1000) break;                      /* slice(0,1000) */
    cJSON *geom = cJSON_GetObjectItem(f, "geometry");

    /* mesh_id = `JSHIS_${intelHashKey(JSON.stringify(f.geometry))}` */
    char *gs = geom ? cJSON_PrintUnformatted(geom) : NULL;
    const char *parts[1] = { gs ? gs : "undefined" };
    char hk[21]; feed_hash_key(hk, parts, 1);
    char mid[40]; snprintf(mid, sizeof mid, "JSHIS_%s", hk);
    if (gs) free(gs);

    cJSON *nf = cJSON_CreateObject();
    cJSON_AddStringToObject(nf, "type", "Feature");
    cJSON_AddItemToObject(nf, "geometry",
      geom ? cJSON_Duplicate(geom, 1) : cJSON_CreateNull());

    cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
    cJSON_AddStringToObject(p, "mesh_id", mid);
    /* f.properties?.T30_I50_PS || null  (falsy → null) */
    cJSON *fp = cJSON_GetObjectItem(f, "properties");
    cJSON *t30 = fp ? cJSON_GetObjectItem(fp, "T30_I50_PS") : NULL;
    int truthy = 0;
    if (t30) {
      if (cJSON_IsNumber(t30)) truthy = (cJSON_GetNumberValue(t30) != 0);
      else if (cJSON_IsString(t30)) truthy = (t30->valuestring && t30->valuestring[0]);
      else if (cJSON_IsBool(t30)) truthy = cJSON_IsTrue(t30);
      else if (cJSON_IsObject(t30) || cJSON_IsArray(t30)) truthy = 1;
    }
    cJSON_AddItemToObject(p, "prob_6lower_30yr",
      truthy ? cJSON_Duplicate(t30, 1) : cJSON_CreateNull());
    cJSON_AddStringToObject(p, "country", "JP");
    cJSON_AddStringToObject(p, "source", "jshis_api");
    cJSON_AddItemToObject(nf, "properties", p);
    cJSON_AddItemToArray(features, nf);
  }
  cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jshis-seismic] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def jshis_seismic_def = {
  .id = "jshis-seismic", .collector = "safety",
  .name = "NIED J-SHIS Seismic Hazard", .name_ja = "NIED J-SHIS 地震ハザード",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(jshis_seismic_def)
