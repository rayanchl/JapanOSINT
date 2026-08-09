/* Environment Canada realtime hydrometric stations (GeoMet OGC API - Features).
 * Endpoint: https://api.weather.gc.ca/collections/hydrometric-realtime/items
 *           ?limit=500&sortby=-DATETIME&f=json                       (keyless)
 * Emits one row per station observation actually returned: LEVEL (UNIT: metres,
 * station datum) and/or DISCHARGE (UNIT: m3/s) — whichever is present. Either
 * can be null on a level-only or flow-only station and a null is never
 * emitted as 0.
 *
 * ORDERING TRAP: without sortby=-DATETIME the default page is arbitrary/old,
 * so the sort is baked into the URL.
 * DATUM NOTE: LEVEL is metres above the STATION datum and is therefore NOT
 * comparable between stations; the row says so.
 * GEO (R2): geometry.coordinates = [lon, lat] straight from the GeoJSON.
 * Licence: Open Government Licence - Canada. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "canada-hydrometric-realtime"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = "https://api.weather.gc.ca/collections/hydrometric-realtime/"
                    "items?limit=500&sortby=-DATETIME&f=json";
  cJSON *doc = feed_get_json(ctx->http, url, 45000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  if (!cJSON_IsArray(feats)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no features[]\n"); return -1; }

  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const char *num = jo_sv(pr, "STATION_NUMBER");
    const char *name = jo_sv(pr, "STATION_NAME");
    const char *when = jo_sv(pr, "DATETIME");
    if (!num) continue;

    cJSON *lev = cJSON_GetObjectItem(pr, "LEVEL");
    cJSON *dis = cJSON_GetObjectItem(pr, "DISCHARGE");
    int have_lev = cJSON_IsNumber(lev), have_dis = cJSON_IsNumber(dis);
    if (!have_lev && !have_dis) continue;   /* nothing measured -> no row (R1) */

    int has_geo = 0; double lat = 0, lon = 0;
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    cJSON *co = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
    if (cJSON_IsArray(co) && cJSON_GetArraySize(co) >= 2) {
      cJSON *x = cJSON_GetArrayItem(co, 0), *y = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        lon = x->valuedouble; lat = y->valuedouble;      /* [lon, lat] */
        if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180 &&
            (lat != 0 || lon != 0)) has_geo = 1;
      }
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "station_number", num);
    if (name) cJSON_AddStringToObject(p, "station_name", name);
    const char *prov = jo_sv(pr, "PROV_TERR_STATE_LOC");
    if (prov) cJSON_AddStringToObject(p, "province", prov);
    if (when) cJSON_AddStringToObject(p, "observed_at", when);
    if (have_lev) {
      cJSON_AddNumberToObject(p, "level_m", lev->valuedouble);
      cJSON_AddStringToObject(p, "level_unit", "m");
      cJSON_AddStringToObject(p, "level_datum_note",
        "metres above the station datum; not comparable between stations");
    }
    if (have_dis) {
      cJSON_AddNumberToObject(p, "discharge_m3s", dis->valuedouble);
      cJSON_AddStringToObject(p, "discharge_unit", "m3/s");
    }
    cJSON_AddStringToObject(p, "country", "CA");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[128], title[320];
    snprintf(key, sizeof key, "%s|%s", num, when ? when : "");
    if (have_dis)
      snprintf(title, sizeof title, "%s %s: discharge %.3f m3/s",
               num, name ? name : "", dis->valuedouble);
    else
      snprintf(title, sizeof title, "%s %s: level %.3f m",
               num, name ? name : "", lev->valuedouble);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = when;
    row.lang            = "en";
    row.link            = "https://wateroffice.ec.gc.ca/";
    row.record_type     = "river-gauge";
    row.has_geo         = has_geo;
    row.lat = lat; row.lon = lon;
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"hydrology\",\"canada\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_canada_hydrometric_def = {
  .id = SRC, .collector = "environment",
  .name = "Environment Canada realtime hydrometric stations",
  .update_interval_sec = 900, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.weather.gc.ca/collections/hydrometric-realtime/items",
  .description = "Canadian national water-level (m) and discharge (m3/s) network, 5-minute telemetry with station geometry.",
  .license = "Open Government Licence - Canada (api.weather.gc.ca GeoMet-OGC-API), keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_canada_hydrometric_def)
