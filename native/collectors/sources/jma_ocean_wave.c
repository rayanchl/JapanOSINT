/* collectors/environment/sources/jma_ocean_wave.c
 * Port of server/src/collectors/jmaOceanWave.js — live JMA bosai/wave path:
 * pointinfo.json (stations) → per-station swjp/<number>.json hourly series →
 * latest finite-height reading → one Feature/station. Seed buoy fallback
 * (rule 7) dropped. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define POINTINFO "https://www.jma.go.jp/bosai/wave/const/pointinfo.json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *stations = feed_get_json(ctx->http, POINTINFO, 8000);
  if (!cJSON_IsArray(stations) || cJSON_GetArraySize(stations) == 0) {
    if (stations) cJSON_Delete(stations);
    return -1;                                      /* tryJmaWave → null */
  }

  cJSON *features = cJSON_CreateArray();
  cJSON *s;
  cJSON_ArrayForEach(s, stations) {
    cJSON *numv = cJSON_GetObjectItem(s, "number");
    char numbuf[32] = {0};
    if (numv && cJSON_IsNumber(numv))
      snprintf(numbuf, sizeof numbuf, "%g", numv->valuedouble);
    else if (numv && cJSON_IsString(numv) && numv->valuestring)
      snprintf(numbuf, sizeof numbuf, "%s", numv->valuestring);
    if (!numbuf[0]) continue;

    char url[160];
    snprintf(url, sizeof url,
      "https://www.jma.go.jp/bosai/wave/data/swjp/%s.json", numbuf);
    cJSON *series = feed_get_json(ctx->http, url, 8000);
    if (!cJSON_IsArray(series) || cJSON_GetArraySize(series) == 0) {
      if (series) cJSON_Delete(series);
      continue;
    }

    /* walk backwards for the last entry with a finite numeric height */
    cJSON *latest = NULL;
    int cnt = cJSON_GetArraySize(series);
    for (int i = cnt - 1; i >= 0; i--) {
      cJSON *r = cJSON_GetArrayItem(series, i);
      cJSON *h = r ? cJSON_GetObjectItem(r, "height") : NULL;
      if (h && cJSON_IsNumber(h) && isfinite(h->valuedouble)) { latest = r; break; }
    }
    if (!latest) { cJSON_Delete(series); continue; }

    cJSON *hv = cJSON_GetObjectItem(latest, "height");
    cJSON *pv = cJSON_GetObjectItem(latest, "period");
    cJSON *dv = cJSON_GetObjectItem(latest, "date");
    cJSON *nm = cJSON_GetObjectItem(s, "name");
    cJSON *ll = cJSON_GetObjectItem(s, "latlon");

    int geocoded = 0;
    double lat = 0, lon = 0;
    if (cJSON_IsArray(ll) && cJSON_GetArraySize(ll) >= 2) {
      cJSON *la = cJSON_GetArrayItem(ll, 0);
      cJSON *lo = cJSON_GetArrayItem(ll, 1);
      if (la && cJSON_IsNumber(la) && lo && cJSON_IsNumber(lo) &&
          isfinite(la->valuedouble) && isfinite(lo->valuedouble)) {
        lat = la->valuedouble; lon = lo->valuedouble; geocoded = 1;
      }
    }

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    if (geocoded) {
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);
    } else {
      cJSON_AddItemToObject(f, "geometry", cJSON_CreateNull());
    }

    cJSON *p = cJSON_CreateObject();               /* EXACT JS key order */
    char bid[64];
    snprintf(bid, sizeof bid, "JMA_WAVE_%s", numbuf);
    cJSON_AddStringToObject(p, "buoy_id", bid);
    cJSON_AddItemToObject(p, "name",
      (nm && cJSON_IsString(nm) && nm->valuestring)
        ? cJSON_CreateString(nm->valuestring) : cJSON_CreateNull());
    /* wave_height_m = Math.round(height)/100 */
    double wh = round(hv->valuedouble) / 100.0;
    cJSON_AddNumberToObject(p, "wave_height_m", wh);
    cJSON_AddItemToObject(p, "period_s",
      (pv && cJSON_IsNumber(pv)) ? cJSON_Duplicate(pv, 1) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "observed_at",
      (dv && cJSON_IsString(dv) && dv->valuestring)
        ? cJSON_CreateString(dv->valuestring) : cJSON_CreateNull());
    cJSON_AddStringToObject(p, "country", "JP");
    cJSON_AddStringToObject(p, "source", "jma_bosai_wave");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);

    cJSON_Delete(series);
  }
  cJSON_Delete(stations);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jma-ocean-wave] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def jma_ocean_wave_def = {
  .id = "jma-ocean-wave", .collector = "environment",
  .name = "JMA Ocean Wave",
  .name_ja = "\xE6\xB0\x97\xE8\xB1\xA1\xE5\xBA\x81 \xE6\xB3\xA2\xE6\xB5\xAA\xE8\xA6\xB3\xE6\xB8\xAC",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(jma_ocean_wave_def)
