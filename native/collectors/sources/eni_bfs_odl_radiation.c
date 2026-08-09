/* German BfS ODL gamma dose rate network (national radiological early warning).
 * Endpoint (WFS returning GeoJSON, keyless):
 *   https://www.imis.bfs.de/ogc/opendata/ows?service=WFS&version=1.1.0
 *   &request=GetFeature&typeName=opendata:odlinfo_odl_1h_latest
 *   &outputFormat=application/json
 * Emits one row per IN-SERVICE station: value (UNIT taken from the feature's
 * own `unit` field, uSv/h) decomposed into value_cosmic + value_terrestrial,
 * the measurement hour, station id/kenn/name and coordinates.
 *
 * STATUS TRAP: site_status 1 = "in Betrieb". Stations with any other status are
 * DROPPED rather than emitting a stale value.
 * FLAGGING: normal readings are ~0.05-0.20 uSv/h; anything above 0.5 uSv/h is
 * marked elevated=true (a flag, not a filter).
 * GEO (R2): geometry.coordinates = [lon, lat] from the WFS itself.
 * Licence: BfS open data WFS (DL-DE->BY-2.0 / open data), keyless. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "bfs-odl-radiation-de"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url =
    "https://www.imis.bfs.de/ogc/opendata/ows?service=WFS&version=1.1.0"
    "&request=GetFeature&typeName=opendata:odlinfo_odl_1h_latest"
    "&outputFormat=application/json";
  cJSON *doc = feed_get_json(ctx->http, url, 45000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  if (!cJSON_IsArray(feats)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no features[]\n"); return -1; }

  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;

    /* site_status 1 = in Betrieb; anything else -> stale, drop the station */
    cJSON *ss = cJSON_GetObjectItem(pr, "site_status");
    int status = cJSON_IsNumber(ss) ? (int)ss->valuedouble
               : (cJSON_IsString(ss) && ss->valuestring[0] ? atoi(ss->valuestring) : -1);
    if (status != 1) continue;

    cJSON *val = cJSON_GetObjectItem(pr, "value");
    if (!cJSON_IsNumber(val)) continue;              /* no measurement (R1) */
    const char *unit = jo_sv(pr, "unit");
    if (!unit) unit = "uSv/h";
    const char *kenn = jo_sv(pr, "kenn");
    const char *name = jo_sv(pr, "name");
    const char *endm = jo_sv(pr, "end_measure");

    int has_geo = 0; double lat = 0, lon = 0;
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    cJSON *co = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
    if (cJSON_IsArray(co) && cJSON_GetArraySize(co) >= 2) {
      cJSON *x = cJSON_GetArrayItem(co, 0), *y = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        lon = x->valuedouble; lat = y->valuedouble;
        if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) has_geo = 1;
      }
    }

    cJSON *p = cJSON_CreateObject();
    if (kenn) cJSON_AddStringToObject(p, "station_kenn", kenn);
    if (name) cJSON_AddStringToObject(p, "station_name", name);
    cJSON_AddNumberToObject(p, "dose_rate", val->valuedouble);
    cJSON_AddStringToObject(p, "unit", unit);
    cJSON *vc = cJSON_GetObjectItem(pr, "value_cosmic");
    cJSON *vt = cJSON_GetObjectItem(pr, "value_terrestrial");
    if (cJSON_IsNumber(vc)) cJSON_AddNumberToObject(p, "value_cosmic", vc->valuedouble);
    if (cJSON_IsNumber(vt)) cJSON_AddNumberToObject(p, "value_terrestrial", vt->valuedouble);
    const char *stm = jo_sv(pr, "start_measure");
    if (stm) cJSON_AddStringToObject(p, "start_measure", stm);
    if (endm) cJSON_AddStringToObject(p, "end_measure", endm);
    const char *sst = jo_sv(pr, "site_status_text");
    if (sst) cJSON_AddStringToObject(p, "site_status_text", sst);
    cJSON *vd = cJSON_GetObjectItem(pr, "validated");
    if (cJSON_IsBool(vd)) cJSON_AddBoolToObject(p, "validated", cJSON_IsTrue(vd));
    if (val->valuedouble > 0.5) cJSON_AddBoolToObject(p, "elevated", 1);
    cJSON_AddStringToObject(p, "country", "DE");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[256];
    snprintf(title, sizeof title, "%s %s: %.3f %s",
             kenn ? kenn : "ODL", name ? name : "", val->valuedouble, unit);

    intel_item row = {0};
    row.remote_key      = kenn ? kenn : (name ? name : title);
    row.title           = title;
    row.summary         = title;
    row.published_at    = endm;
    row.lang            = "de";
    row.link            = "https://odlinfo.bfs.de/";
    row.record_type     = "radiation-station";
    row.has_geo         = has_geo;
    row.lat = lat; row.lon = lon;
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"radiation\",\"germany\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_bfs_odl_radiation_def = {
  .id = SRC, .collector = "environment",
  .name = "German BfS ODL gamma dose rate network",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://www.imis.bfs.de/ogc/opendata/ows?service=WFS&request=GetFeature&typeName=opendata:odlinfo_odl_1h_latest",
  .description = "Hourly gamma dose rate (uSv/h) from the ~1,700-station German Federal Office for Radiation Protection ODL early-warning network, with coordinates.",
  .license = "BfS open data WFS (DL-DE->BY-2.0), keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_bfs_odl_radiation_def)
