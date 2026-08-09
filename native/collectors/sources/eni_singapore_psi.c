/* Singapore NEA Pollutant Standards Index.
 * Endpoint: https://api.data.gov.sg/v1/environment/psi                (keyless)
 * Emits one row per (region, parameter): value with the CORRECT unit for that
 * parameter — psi_* and *_sub_index are DIMENSIONLESS INDEX numbers, while
 * pm25/pm10/so2_twenty_four_hourly, no2_one_hour_max, o3_* and co_* are
 * concentrations. Labelling an index as a concentration is exactly the unit
 * error this domain is prone to, so the unit comes from a per-parameter table
 * below rather than a blanket "ug/m3".
 *
 * SHAPE TRAP: `readings` is TRANSPOSED — an object keyed by PARAMETER, each
 * holding an object keyed by REGION (north/south/east/west/central). It is not
 * a list of stations; we iterate parameters, then regions.
 * GEO (R2): region_metadata[].label_location is a REGION LABEL POINT, not a
 * station, so rows carry "geo_precision": "area-centroid".
 * Licence: Singapore Open Data Licence (data.gov.sg), keyless. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "singapore-psi"

/* A parameter is an INDEX unless it is a named concentration. */
static const char *unit_for(const char *param) {
  if (strstr(param, "sub_index") || strncmp(param, "psi", 3) == 0)
    return "index (dimensionless)";
  if (strncmp(param, "co_", 3) == 0) return "mg/m3";
  return "ug/m3";
}

typedef struct { const char *name; double lat, lon; int ok; } reg_t;

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http,
                             "https://api.data.gov.sg/v1/environment/psi", 20000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  reg_t regs[16]; int nreg = 0;
  cJSON *meta = cJSON_GetObjectItem(doc, "region_metadata");
  cJSON *m;
  if (cJSON_IsArray(meta)) cJSON_ArrayForEach(m, meta) {
    if (nreg >= 16) break;
    const char *nm = jo_sv(m, "name");
    if (!nm) continue;
    reg_t *e = &regs[nreg++];
    e->name = nm; e->ok = 0; e->lat = 0; e->lon = 0;
    cJSON *ll = cJSON_GetObjectItem(m, "label_location");
    if (ll) {
      cJSON *la = cJSON_GetObjectItem(ll, "latitude");
      cJSON *lo = cJSON_GetObjectItem(ll, "longitude");
      if (cJSON_IsNumber(la) && cJSON_IsNumber(lo)) {
        e->lat = la->valuedouble; e->lon = lo->valuedouble;
        if (e->lat != 0 || e->lon != 0) e->ok = 1;
      }
    }
  }

  cJSON *items = cJSON_GetObjectItem(doc, "items");
  if (!cJSON_IsArray(items) || cJSON_GetArraySize(items) == 0) {
    cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no items[]\n"); return 0;
  }
  cJSON *item = cJSON_GetArrayItem(items, cJSON_GetArraySize(items) - 1);
  const char *when = jo_sv(item, "timestamp");
  cJSON *readings = cJSON_GetObjectItem(item, "readings");
  if (!cJSON_IsObject(readings)) {
    cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no readings{}\n"); return 0;
  }

  int n = 0;
  cJSON *param;
  /* transposed: parameter -> region -> value */
  cJSON_ArrayForEach(param, readings) {
    const char *pname = param->string;
    if (!pname || !cJSON_IsObject(param)) continue;
    const char *unit = unit_for(pname);
    cJSON *rv;
    cJSON_ArrayForEach(rv, param) {
      const char *rname = rv->string;
      if (!rname || !cJSON_IsNumber(rv)) continue;

      const reg_t *rg = NULL;
      for (int i = 0; i < nreg; i++)
        if (strcmp(regs[i].name, rname) == 0) { rg = &regs[i]; break; }

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "region", rname);
      cJSON_AddStringToObject(p, "parameter", pname);
      cJSON_AddNumberToObject(p, "value", rv->valuedouble);
      cJSON_AddStringToObject(p, "unit", unit);
      if (when) cJSON_AddStringToObject(p, "timestamp", when);
      if (rg && rg->ok)
        cJSON_AddStringToObject(p, "geo_precision", "area-centroid");
      cJSON_AddStringToObject(p, "country", "SG");
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      char key[160], title[256];
      snprintf(key, sizeof key, "%s|%s|%s", rname, pname, when ? when : "");
      snprintf(title, sizeof title, "Singapore %s %s = %.1f %s",
               rname, pname, rv->valuedouble, unit);

      intel_item row = {0};
      row.remote_key      = key;
      row.title           = title;
      row.summary         = title;
      row.published_at    = when;
      row.lang            = "en";
      row.link            = "https://www.haze.gov.sg/";
      row.record_type     = "air-quality-index";
      if (rg && rg->ok) { row.has_geo = 1; row.lat = rg->lat; row.lon = rg->lon; }
      row.properties_json = pj;
      row.tags_json       = "[\"environment\",\"air-quality\",\"singapore\"]";
      if (sink->emit(sink, &row) >= 0) n++;
      free(pj);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_singapore_psi_def = {
  .id = SRC, .collector = "environment",
  .name = "Singapore NEA Pollutant Standards Index",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.data.gov.sg/v1/environment/psi",
  .description = "Singapore NEA hourly PSI and component pollutant readings for five regions, with index values and concentrations kept distinctly labelled.",
  .license = "Singapore Open Data Licence (data.gov.sg), keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_singapore_psi_def)
