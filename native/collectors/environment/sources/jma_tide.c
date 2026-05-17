/* collectors/environment/sources/jma_tide.c
 * Port of server/src/collectors/jmaTide.js (tryJmaTide).
 * Two JMA bosai JSON fetches: tide_area.json (class20 → class30 → stations)
 * and tide_estimate.json (areas keyed by `<class20>_<class30>`). Builds one
 * Feature per station with latest observed level vs astronomical prediction.
 * SEED fallback intentionally not ported (JS uses it only when live empty).
 */
#include "../../../source.h"
#include "../../../lib/geojson.h"
#include "../../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define TIDE_AREA_URL     "https://www.jma.go.jp/bosai/tidelevel/const/tide_area.json"
#define TIDE_ESTIMATE_URL "https://www.jma.go.jp/bosai/tidelevel/data/tide/tide_estimate.json"

/* latestObserved: walk estimate[] backwards for a finite value, map to the
 * astro slot by offset from the end. Returns 1 and fills out params if found. */
static int latest_observed(const cJSON *estimate, const cJSON *astro,
                           double *level, int *has_astro, double *astro_v,
                           int *hours_ago) {
  if (!cJSON_IsArray(estimate)) return 0;
  int elen = cJSON_GetArraySize(estimate);
  for (int i = elen - 1; i >= 0; i--) {
    cJSON *v = cJSON_GetArrayItem((cJSON *)estimate, i);
    if (v && cJSON_IsNumber(v) && isfinite(v->valuedouble)) {
      *level = v->valuedouble;
      *hours_ago = elen - 1 - i;
      *has_astro = 0;
      *astro_v = 0;
      if (cJSON_IsArray(astro)) {
        int alen = cJSON_GetArraySize(astro);
        int aidx = alen - (elen - 1 - i) - 1;
        if (aidx >= 0 && aidx < alen) {
          cJSON *av = cJSON_GetArrayItem((cJSON *)astro, aidx);
          if (av && cJSON_IsNumber(av) && isfinite(av->valuedouble)) {
            *has_astro = 1;
            *astro_v = av->valuedouble;
          }
        }
      }
      return 1;
    }
  }
  return 0;
}

/* JS: x || null  — string value or JSON null. */
static cJSON *str_or_null(const cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  if (v && cJSON_IsString(v) && v->valuestring[0])
    return cJSON_CreateString(v->valuestring);
  return cJSON_CreateNull();
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *areas     = feed_get_json(ctx->http, TIDE_AREA_URL, 10000);
  cJSON *estimates = feed_get_json(ctx->http, TIDE_ESTIMATE_URL, 10000);
  cJSON *estAreas  = estimates ? cJSON_GetObjectItem(estimates, "areas") : NULL;
  if (!areas || !estimates || !estAreas) {
    if (areas) cJSON_Delete(areas);
    if (estimates) cJSON_Delete(estimates);
    return -1;
  }
  cJSON *estTime = cJSON_GetObjectItem(estimates, "time");

  cJSON *features = cJSON_CreateArray();
  cJSON *area;
  cJSON_ArrayForEach(area, areas) {
    const char *class20Code = area->string;
    cJSON *class30s = cJSON_GetObjectItem(area, "class30s");
    if (!cJSON_IsArray(class30s)) continue;
    cJSON *areaName = cJSON_GetObjectItem(area, "name");

    cJSON *class30;
    cJSON_ArrayForEach(class30, class30s) {
      cJSON *c30code = cJSON_GetObjectItem(class30, "code");
      char estKey[128];
      snprintf(estKey, sizeof estKey, "%s_%s",
               class20Code ? class20Code : "",
               (c30code && cJSON_IsString(c30code)) ? c30code->valuestring
                 : (c30code && cJSON_IsNumber(c30code) ? "" : ""));
      if (c30code && cJSON_IsNumber(c30code))
        snprintf(estKey, sizeof estKey, "%s_%g",
                 class20Code ? class20Code : "", c30code->valuedouble);

      cJSON *est = cJSON_GetObjectItem(estAreas, estKey);
      int haveObs = 0, hasAstro = 0, hoursAgo = 0;
      double levelCm = 0, astroCm = 0;
      if (est) {
        cJSON *e = cJSON_GetObjectItem(est, "estimate");
        cJSON *a = cJSON_GetObjectItem(est, "astro");
        haveObs = latest_observed(e, a, &levelCm, &hasAstro, &astroCm,
                                  &hoursAgo);
      }

      cJSON *stations = cJSON_GetObjectItem(class30, "stations");
      if (!cJSON_IsArray(stations)) continue;
      cJSON *station;
      cJSON_ArrayForEach(station, stations) {
        cJSON *latv = cJSON_GetObjectItem(station, "lat");
        cJSON *lonv = cJSON_GetObjectItem(station, "lon");
        double lat = latv ? (cJSON_IsString(latv) ? atof(latv->valuestring)
                              : latv->valuedouble) : NAN;
        double lon = lonv ? (cJSON_IsString(lonv) ? atof(lonv->valuestring)
                              : lonv->valuedouble) : NAN;
        int geocoded = isfinite(lat) && isfinite(lon)
                       && (latv != NULL) && (lonv != NULL);

        cJSON *feat = cJSON_CreateObject();
        cJSON_AddStringToObject(feat, "type", "Feature");
        if (geocoded) {
          cJSON *g = cJSON_CreateObject();
          cJSON_AddStringToObject(g, "type", "Point");
          cJSON *co = cJSON_CreateArray();
          cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
          cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
          cJSON_AddItemToObject(g, "coordinates", co);
          cJSON_AddItemToObject(feat, "geometry", g);
        } else {
          cJSON_AddItemToObject(feat, "geometry", cJSON_CreateNull());
        }

        cJSON *p = cJSON_CreateObject();         /* EXACT JS key order */
        cJSON *codev = cJSON_GetObjectItem(station, "code");
        char sidb[96];
        if (codev && cJSON_IsString(codev))
          snprintf(sidb, sizeof sidb, "JMA_TIDE_%s", codev->valuestring);
        else if (codev && cJSON_IsNumber(codev))
          snprintf(sidb, sizeof sidb, "JMA_TIDE_%g", codev->valuedouble);
        else
          snprintf(sidb, sizeof sidb, "JMA_TIDE_");
        cJSON_AddStringToObject(p, "station_id", sidb);
        /* name: station.name || `Station ${station.code}` */
        cJSON *nmv = cJSON_GetObjectItem(station, "name");
        if (nmv && cJSON_IsString(nmv) && nmv->valuestring[0]) {
          cJSON_AddStringToObject(p, "name", nmv->valuestring);
        } else {
          char nb[96];
          if (codev && cJSON_IsString(codev))
            snprintf(nb, sizeof nb, "Station %s", codev->valuestring);
          else if (codev && cJSON_IsNumber(codev))
            snprintf(nb, sizeof nb, "Station %g", codev->valuedouble);
          else
            snprintf(nb, sizeof nb, "Station ");
          cJSON_AddStringToObject(p, "name", nb);
        }
        cJSON_AddItemToObject(p, "level_cm",
          haveObs ? cJSON_CreateNumber(levelCm) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "astro_cm",
          (haveObs && hasAstro) ? cJSON_CreateNumber(astroCm)
                                : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "anomaly_cm",
          (haveObs && hasAstro) ? cJSON_CreateNumber(levelCm - astroCm)
                                : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "hours_ago",
          haveObs ? cJSON_CreateNumber(hoursAgo) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "class20_name",
          (areaName && cJSON_IsString(areaName) && areaName->valuestring[0])
            ? cJSON_CreateString(areaName->valuestring) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "address", str_or_null(station, "addr"));
        cJSON_AddItemToObject(p, "reference",
          str_or_null(station, "reference"));
        /* operator_type: station.typeName || station.type || null */
        cJSON *tn = cJSON_GetObjectItem(station, "typeName");
        cJSON *ty = cJSON_GetObjectItem(station, "type");
        if (tn && cJSON_IsString(tn) && tn->valuestring[0])
          cJSON_AddStringToObject(p, "operator_type", tn->valuestring);
        else if (ty && cJSON_IsString(ty) && ty->valuestring[0])
          cJSON_AddStringToObject(p, "operator_type", ty->valuestring);
        else if (ty && cJSON_IsNumber(ty))
          cJSON_AddItemToObject(p, "operator_type",
            cJSON_Duplicate(ty, 1));
        else
          cJSON_AddItemToObject(p, "operator_type", cJSON_CreateNull());
        cJSON_AddStringToObject(p, "country", "JP");
        cJSON_AddItemToObject(p, "observed_at",
          (estTime && cJSON_IsString(estTime) && estTime->valuestring[0])
            ? cJSON_CreateString(estTime->valuestring) : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "source", "jma_bosai_tidelevel");
        cJSON_AddItemToObject(feat, "properties", p);
        cJSON_AddItemToArray(features, feat);
      }
    }
  }

  cJSON_Delete(areas);
  cJSON_Delete(estimates);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jma-tide] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def jma_tide_def = {
  .id = "jma-tide", .collector = "environment",
  .name = "JMA Tide Levels", .name_ja = "気象庁 潮位情報",
   .update_interval_sec = 600, .run = run,
};
REGISTER_SOURCE(jma_tide_def)
