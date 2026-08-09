/* NOAA CO-OPS tide-station water level.
 * Endpoint: https://api.tidesandcurrents.noaa.gov/api/prod/datagetter
 *   ?date=latest&station=<id>&product=water_level&datum=MLLW&units=metric
 *   &time_zone=gmt&format=json                                       (keyless)
 * Emits one row per station that returned a reading: water_level (UNIT: metres
 * above the requested datum MLLW, units=metric), the reading time (UTC), the
 * QC flag q ("p" preliminary / "v" verified) and the sigma s.
 *
 * ERROR TRAP: an out-of-service station returns {"error":{"message":...}} with
 * HTTP 200 — the error key is checked before parsing so a failed station is
 * skipped rather than parsed as data.
 * STRING TRAP: metadata.lat/lon and data[].v/.t are all STRINGS; they are
 * strtod'ed, and a non-numeric token means no row.
 * Licence: NOAA/NOS, public domain. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "noaa-coops-water-level"

/* Station ids only; every emitted value AND every coordinate comes from the
 * station's own response metadata (R1/R2). */
static const char *STATIONS[] = {
  "8454000", "8518750", "8443970", "8724580", "8770570",
  "9414290", "9447130", "9410170", "8638610", "8761724", NULL
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0, fetched = 0;
  for (int i = 0; STATIONS[i]; i++) {
    char url[320];
    snprintf(url, sizeof url,
      "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?date=latest"
      "&station=%s&product=water_level&datum=MLLW&units=metric&time_zone=gmt"
      "&format=json", STATIONS[i]);
    cJSON *doc = feed_get_json(ctx->http, url, 20000);
    if (!doc) continue;
    fetched = 1;

    /* HTTP 200 + {"error":{...}} means the station is out of service */
    if (cJSON_GetObjectItem(doc, "error")) { cJSON_Delete(doc); continue; }

    cJSON *meta = cJSON_GetObjectItem(doc, "metadata");
    cJSON *data = cJSON_GetObjectItem(doc, "data");
    if (!meta || !cJSON_IsArray(data) || cJSON_GetArraySize(data) == 0) {
      cJSON_Delete(doc); continue;
    }
    cJSON *d = cJSON_GetArrayItem(data, 0);
    const char *vs = jo_sv(d, "v"), *when = jo_sv(d, "t");
    if (!vs || !when) { cJSON_Delete(doc); continue; }
    char *end = NULL;
    double v = strtod(vs, &end);
    if (end == vs) { cJSON_Delete(doc); continue; }

    const char *name = jo_sv(meta, "name");
    const char *sid  = jo_sv(meta, "id");
    const char *lats = jo_sv(meta, "lat"), *lons = jo_sv(meta, "lon");
    int has_geo = 0; double lat = 0, lon = 0;
    if (lats && lons) {
      char *e1 = NULL, *e2 = NULL;
      lat = strtod(lats, &e1); lon = strtod(lons, &e2);
      if (e1 != lats && e2 != lons && (lat != 0 || lon != 0)) has_geo = 1;
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "station_id", sid ? sid : STATIONS[i]);
    if (name) cJSON_AddStringToObject(p, "station_name", name);
    cJSON_AddNumberToObject(p, "water_level", v);
    cJSON_AddStringToObject(p, "unit", "m");
    cJSON_AddStringToObject(p, "datum", "MLLW");
    cJSON_AddStringToObject(p, "observed_at_utc", when);
    const char *q = jo_sv(d, "q");
    if (q) cJSON_AddStringToObject(p, "qc_flag", q);
    const char *sg = jo_sv(d, "s");
    if (sg) { char *e3 = NULL; double sd = strtod(sg, &e3);
              if (e3 != sg) cJSON_AddNumberToObject(p, "sigma_m", sd); }
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[256];
    snprintf(title, sizeof title, "%s (%s): water level %.3f m MLLW",
             name ? name : "NOAA station", sid ? sid : STATIONS[i], v);

    intel_item row = {0};
    row.remote_key      = sid ? sid : STATIONS[i];
    row.title           = title;
    row.summary         = title;
    row.published_at    = when;
    row.lang            = "en";
    row.link            = url;
    row.record_type     = "tide-station";
    row.has_geo         = has_geo;
    row.lat = lat; row.lon = lon;
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"tide\",\"noaa\",\"coastal\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
    cJSON_Delete(doc);
  }
  if (!fetched) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_noaa_coops_water_level_def = {
  .id = SRC, .collector = "environment",
  .name = "NOAA CO-OPS station water level",
  .update_interval_sec = 600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter",
  .description = "Real-time water level (metres above MLLW) at NOAA tide stations, with station coordinates from the station metadata.",
  .license = "NOAA/NOS, public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_noaa_coops_water_level_def)
