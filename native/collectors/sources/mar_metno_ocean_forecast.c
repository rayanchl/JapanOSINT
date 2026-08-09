/* collectors/sources/mar_metno_ocean_forecast.c
 * OSINT service — METNO_OCEAN_FORECAST. On-demand coordinate pivot
 * (ctx->entity = "lat,lon"): point ocean forecast — significant wave height
 * and direction, surface current speed and set, and sea-surface temperature.
 *
 * Endpoint: https://api.met.no/weatherapi/oceanforecast/2.0/complete
 *           ?lat=<lat>&lon=<lon>
 * Emits (all fetched): meta.updated_at, the first timeseries entry's time and
 *   its instant details (sea_surface_wave_height,
 *   sea_surface_wave_from_direction, sea_water_speed, sea_water_to_direction,
 *   sea_water_temperature), the forecast horizon length, and the grid point
 *   MET snapped the request to.
 * Keyless. Licence: MET Norway, NLOD / CC BY 4.0. Terms REQUIRE a unique
 *   identifying User-Agent with contact info — core/httpclient.c sets
 *   "JapanOSINT/1.0 (+native)" on every request.
 *
 * parse_notes trap — "geometry.coordinates = [lon, lat, altitude] is the grid
 * point MET snapped your request to (5.3037,60.1029 for a 5.3,60.1 request) —
 * echo that, not the requested point." The emitted lat/lon come from the
 * response geometry; the requested point is carried separately as
 * requested_lat/requested_lon (R2).
 * parse_notes trap — "Requests over land return HTTP 422; treat as honest
 * empty." Any non-200 returns 0.
 * parse_notes — "Values live under timeseries[].data.instant.details."
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static inline void add_det(cJSON *dst, const cJSON *det, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(det, k);
  if (cJSON_IsNumber(v)) cJSON_AddNumberToObject(dst, k, v->valuedouble);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  double qlat = 0, qlon = 0;
  if (!jo_parse_coord(ctx->entity, &qlat, &qlon)) return 0;   /* wrong shape */

  char url[200];
  snprintf(url, sizeof url,
           "https://api.met.no/weatherapi/oceanforecast/2.0/complete?lat=%.4f&lon=%.4f",
           qlat, qlon);

  char *body = jo_get(ctx, url, NULL, "METNO_OCEAN_FORECAST");
  if (!body) return 0;               /* 422 over land etc. -> honest empty */
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) return 0;

  cJSON *geom = cJSON_GetObjectItem(doc, "geometry");
  cJSON *co = geom ? cJSON_GetObjectItem(geom, "coordinates") : NULL;
  cJSON *gx = cJSON_GetArrayItem(co, 0), *gy = cJSON_GetArrayItem(co, 1);
  if (!cJSON_IsNumber(gx) || !cJSON_IsNumber(gy)) { cJSON_Delete(doc); return 0; }
  double glon = gx->valuedouble, glat = gy->valuedouble;

  cJSON *props = cJSON_GetObjectItem(doc, "properties");
  cJSON *meta = props ? cJSON_GetObjectItem(props, "meta") : NULL;
  const char *updated = meta ? jo_sv(meta, "updated_at") : NULL;
  cJSON *tsa = props ? cJSON_GetObjectItem(props, "timeseries") : NULL;
  cJSON *first = cJSON_IsArray(tsa) ? cJSON_GetArrayItem(tsa, 0) : NULL;
  if (!first) { cJSON_Delete(doc); return 0; }
  const char *when = jo_sv(first, "time");
  cJSON *data = cJSON_GetObjectItem(first, "data");
  cJSON *inst = data ? cJSON_GetObjectItem(data, "instant") : NULL;
  cJSON *det  = inst ? cJSON_GetObjectItem(inst, "details") : NULL;
  if (!det) { cJSON_Delete(doc); return 0; }

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "METNO_OCEAN_FORECAST");
  cJSON_AddNumberToObject(p, "requested_lat", qlat);
  cJSON_AddNumberToObject(p, "requested_lon", qlon);
  cJSON_AddNumberToObject(p, "lat", glat);        /* the grid point MET used */
  cJSON_AddNumberToObject(p, "lon", glon);
  cJSON_AddStringToObject(p, "geo_precision", "model-grid-point");
  if (updated) cJSON_AddStringToObject(p, "model_updated_at", updated);
  if (when)    cJSON_AddStringToObject(p, "forecast_time", when);
  add_det(p, det, "sea_surface_wave_height");
  add_det(p, det, "sea_surface_wave_from_direction");
  add_det(p, det, "sea_water_speed");
  add_det(p, det, "sea_water_to_direction");
  add_det(p, det, "sea_water_temperature");
  if (cJSON_IsArray(tsa))
    cJSON_AddNumberToObject(p, "timeseries_steps", cJSON_GetArraySize(tsa));
  cJSON_AddStringToObject(p, "source", "MET Norway oceanforecast 2.0");
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  const cJSON *wh = cJSON_GetObjectItem(det, "sea_surface_wave_height");
  const cJSON *sst = cJSON_GetObjectItem(det, "sea_water_temperature");
  const cJSON *cur = cJSON_GetObjectItem(det, "sea_water_speed");

  char title[160], summary[240];
  snprintf(title, sizeof title, "Ocean forecast %.4f, %.4f", glat, glon);
  snprintf(summary, sizeof summary,
           "wave %.2f m · current %.2f m/s · SST %.1f °C%s%s",
           cJSON_IsNumber(wh) ? wh->valuedouble : 0.0,
           cJSON_IsNumber(cur) ? cur->valuedouble : 0.0,
           cJSON_IsNumber(sst) ? sst->valuedouble : 0.0,
           when ? " · " : "", when ? when : "");

  intel_item it = {0};
  it.remote_key      = url;
  it.title           = title;
  it.summary         = summary;
  it.link            = url;
  it.lang            = "en";
  it.published_at    = when;
  it.record_type     = "ocean-forecast-point";
  it.has_geo         = 1;
  it.lat             = glat;
  it.lon             = glon;
  it.properties_json = pj ? pj : "{}";
  it.tags_json       = "[\"osint-search\",\"maritime\",\"sea-state\"]";
  sink->emit(sink, &it);
  free(pj);
  cJSON_Delete(doc);
  fprintf(stderr, "[METNO_OCEAN_FORECAST] emitted 1 (%.4f,%.4f)\n", glat, glon);
  return 0;
}

static const source_def mar_metno_ocean_forecast_def = {
  .id = "METNO_OCEAN_FORECAST", .collector = "osint",
  .name = "MET Norway ocean forecast (wave, current, SST) by coordinate",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "api",
  .url = "https://api.met.no/weatherapi/oceanforecast/2.0/complete",
  .description = "Point ocean forecast for a maritime coordinate: significant wave height and direction, surface current speed and set, and sea-surface temperature — sea-state context for a position or an incident location.",
  .license = "MET Norway, NLOD / CC BY 4.0 (identifying User-Agent required)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_metno_ocean_forecast_def)
