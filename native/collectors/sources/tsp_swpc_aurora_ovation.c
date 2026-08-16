/* collectors/sources/tsp_swpc_aurora_ovation.c
 * NOAA SWPC OVATION aurora forecast grid — 30-minute-ahead global aurora
 * probability, i.e. where HF propagation and over-the-horizon links will
 * degrade and where GNSS scintillation is likely.
 * Endpoint: https://services.swpc.noaa.gov/json/ovation_aurora_latest.json
 *   (keyless, ~900 KB)
 * Emits one row per forecast grid cell above the probability threshold:
 *   probability %, the cell's longitude/latitude, the model Observation Time
 *   and Forecast Time.
 *
 * parse_notes honoured, verbatim where it matters:
 *  - "Coordinates are [longitude, latitude, probability%] with longitude 0..359
 *    (add -360 above 180 to get standard lon)": done below — without it every
 *    eastern-hemisphere cell would land in the wrong half of the map.
 *  - "64,800 cells — emit only cells above a threshold (e.g. aurora >= 10) or
 *    you will bury the map": threshold MIN_PROB, hard cap MAX_CELLS, both
 *    recorded in properties.
 *  - "Geo is genuine model output, precision is 1 degree: tag geo_precision
 *    accordingly": properties.geo_precision = "1-degree-model-grid" (R2 — the
 *    coordinate is the upstream's own grid cell, not a guess).
 * Licence: NOAA SWPC — US Government public domain, keyless.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OVATION_URL "https://services.swpc.noaa.gov/json/ovation_aurora_latest.json"
#define MIN_PROB 10
#define MAX_CELLS 3000

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, OVATION_URL, 45000);
  if (!doc) {
    fprintf(stderr, "[swpc-aurora-ovation] fetch failed\n");
    return -1;
  }
  cJSON *coords = cJSON_GetObjectItem(doc, "coordinates");
  if (!cJSON_IsArray(coords)) {
    fprintf(stderr, "[swpc-aurora-ovation] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }
  const char *obs = jo_sv(doc, "Observation Time");
  const char *fct = jo_sv(doc, "Forecast Time");

  int n = 0, above = 0;
  cJSON *c;
  cJSON_ArrayForEach(c, coords) {
    if (!cJSON_IsArray(c) || cJSON_GetArraySize(c) < 3) continue;
    cJSON *jlon = cJSON_GetArrayItem(c, 0);
    cJSON *jlat = cJSON_GetArrayItem(c, 1);
    cJSON *jpr  = cJSON_GetArrayItem(c, 2);
    if (!cJSON_IsNumber(jlon) || !cJSON_IsNumber(jlat) || !cJSON_IsNumber(jpr))
      continue;
    double prob = jpr->valuedouble;
    if (prob < MIN_PROB) continue;
    above++;
    if (n >= MAX_CELLS) continue;

    double lat = jlat->valuedouble;
    double lon = jlon->valuedouble;
    /* longitude arrives as 0..359; standard lon needs the eastern half shifted */
    if (lon > 180.0) lon -= 360.0;
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddNumberToObject(pr, "aurora_probability_pct", prob);
    cJSON_AddNumberToObject(pr, "cell_lat", lat);
    cJSON_AddNumberToObject(pr, "cell_lon", lon);
    if (obs) cJSON_AddStringToObject(pr, "observation_time", obs);
    if (fct) cJSON_AddStringToObject(pr, "forecast_time", fct);
    cJSON_AddStringToObject(pr, "geo_precision", "1-degree-model-grid");
    cJSON_AddNumberToObject(pr, "min_probability_emitted", MIN_PROB);
    cJSON_AddNumberToObject(pr, "cell_cap", MAX_CELLS);
    cJSON_AddStringToObject(pr, "source", "NOAA SWPC OVATION aurora model");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[192], summary[192], key[96];
    snprintf(key, sizeof key, "%s|%.0f|%.0f", fct ? fct : "ovation", lon, lat);
    snprintf(title, sizeof title, "Aurora %.0f%% at %.0f, %.0f", prob, lat, lon);
    snprintf(summary, sizeof summary, "OVATION forecast%s%s",
             fct ? " for " : "", fct ? fct : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://www.swpc.noaa.gov/products/aurora-30-minute-forecast";
    it.lang            = "en";
    it.published_at    = fct ? fct : obs;
    it.record_type     = "aurora-forecast-cell";
    it.has_geo         = 1;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"space-weather\",\"aurora\",\"hf-propagation\",\"noaa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[swpc-aurora-ovation] emitted %d of %d cells >= %d%%\n",
          n, above, MIN_PROB);
  return 0;                 /* a quiet magnetosphere is not an error (R3) */
}

static const source_def tsp_swpc_aurora_ovation_def = {
  .id = "swpc-aurora-ovation", .collector = "satellite",
  .name = "NOAA SWPC OVATION aurora forecast grid",
  .update_interval_sec = 1800, .run = run,
  .category = "satellite", .type = "api", .url = OVATION_URL,
  .description = "30-minute-ahead global aurora probability on a 1-degree grid — where HF propagation and over-the-horizon links will degrade and where GNSS scintillation is likely.",
  .license = "NOAA SWPC — US Government public domain, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_swpc_aurora_ovation_def)
