/* US Water Quality Portal results (EPA STORET + USGS NWIS + state/tribal).
 * Endpoints (keyless):
 *   https://www.waterqualitydata.us/data/Result/search?statecode=US:55
 *     &mimeType=csv&sorted=no&dataProfile=narrowResult&startDateLo=<mm-dd-yyyy>
 *   https://www.waterqualitydata.us/data/Station/search?statecode=US:55
 *     &mimeType=csv&sorted=no
 * Emits one row per result: CharacteristicName + ResultMeasureValue +
 * ResultMeasure/MeasureUnitCode — the measurement triple, with the unit taken
 * verbatim from the file (mg/L, ug/L, deg C, ...), never assumed.
 *
 * CSV TRAP, quoted from the source survey: "DO NOT index blindly: many trailing
 * columns are empty and several fields (CharacteristicName, ResultCommentText)
 * contain commas and are quoted - a real RFC4180 parser is required, and
 * lib/csv.h must be used rather than strtok." This collector therefore uses
 * csv_parse(text, 1), which keys cells by HEADER NAME, so an empty or shifted
 * trailing column cannot slide the measurement into the wrong field.
 * GEO (R2): coordinates come from the parallel /data/Station/search CSV joined
 * on MonitoringLocationIdentifier; no station match -> no geometry.
 * Responses are large, so the window is bounded with startDateLo.
 * Licence: US public domain (EPA / USGS / NWQMC). Keyless. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/csv.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SRC "water-quality-portal"
/* percent-encoded "US:55" (Wisconsin); doubled %% because these string
 * literals are concatenated into snprintf FORMAT strings. */
#define STATE "US%%3A55"

static const char *cell(const cJSON *row, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(row, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

typedef struct { const char *id; double lat, lon; } wqstn_t;

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL) - 30 * 24 * 3600;
  struct tm tmv; gmtime_r(&now, &tmv);
  char since[16];
  strftime(since, sizeof since, "%m-%d-%Y", &tmv);   /* WQP wants mm-dd-yyyy */

  /* --- station coordinates --- */
  wqstn_t *stns = NULL; int nstn = 0;
  cJSON *srows = NULL;
  char surl[320];
  snprintf(surl, sizeof surl,
    "https://www.waterqualitydata.us/data/Station/search?statecode=" STATE
    "&mimeType=csv&sorted=no");
  char *scsv = feed_get_text(ctx->http, surl, 90000);
  if (scsv) {
    srows = csv_parse(scsv, 1);
    free(scsv);
    int m = cJSON_GetArraySize(srows);
    stns = calloc(m > 0 ? (size_t)m : 1, sizeof(wqstn_t));
    cJSON *r;
    if (stns) cJSON_ArrayForEach(r, srows) {
      const char *id = cell(r, "MonitoringLocationIdentifier");
      const char *la = cell(r, "LatitudeMeasure");
      const char *lo = cell(r, "LongitudeMeasure");
      if (!id || !la || !lo) continue;
      char *e1 = NULL, *e2 = NULL;
      double lat = strtod(la, &e1), lon = strtod(lo, &e2);
      if (e1 == la || e2 == lo) continue;
      if ((lat == 0 && lon == 0) || lat < -90 || lat > 90 ||
          lon < -180 || lon > 180) continue;
      if (nstn >= m) break;
      stns[nstn].id = id; stns[nstn].lat = lat; stns[nstn].lon = lon; nstn++;
    }
  }

  /* --- results --- */
  char url[448];
  snprintf(url, sizeof url,
    "https://www.waterqualitydata.us/data/Result/search?statecode=" STATE
    "&mimeType=csv&sorted=no&dataProfile=narrowResult&startDateLo=%s", since);
  char *csv = feed_get_text(ctx->http, url, 120000);
  if (!csv) {
    free(stns); if (srows) cJSON_Delete(srows);
    fprintf(stderr, "[" SRC "] fetch failed\n");
    return -1;
  }
  cJSON *rows = csv_parse(csv, 1);      /* RFC4180, keyed by header name */
  free(csv);

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const char *ch = cell(r, "CharacteristicName");
    const char *vs = cell(r, "ResultMeasureValue");
    const char *un = cell(r, "ResultMeasure/MeasureUnitCode");
    const char *loc = cell(r, "MonitoringLocationIdentifier");
    if (!ch || !vs || !un) continue;      /* no complete triple -> no row (R1) */
    char *e = NULL;
    double v = strtod(vs, &e);
    if (e == vs) continue;               /* non-numeric (detect-limit text) */

    const wqstn_t *st = NULL;
    if (loc && stns)
      for (int i = 0; i < nstn; i++)
        if (strcmp(stns[i].id, loc) == 0) { st = &stns[i]; break; }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "characteristic", ch);
    cJSON_AddNumberToObject(p, "value", v);
    cJSON_AddStringToObject(p, "unit", un);
    if (loc) cJSON_AddStringToObject(p, "monitoring_location", loc);
    const char *org = cell(r, "OrganizationFormalName");
    if (org) cJSON_AddStringToObject(p, "organization", org);
    const char *dt = cell(r, "ActivityStartDate");
    if (dt) cJSON_AddStringToObject(p, "activity_start_date", dt);
    const char *frac = cell(r, "ResultSampleFractionText");
    if (frac) cJSON_AddStringToObject(p, "sample_fraction", frac);
    cJSON_AddStringToObject(p, "state", "US:55");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *rid = cell(r, "ResultIdentifier");
    char key[192], title[320];
    snprintf(key, sizeof key, "%s", rid ? rid : "");
    if (!key[0])
      snprintf(key, sizeof key, "%s|%s|%s", loc ? loc : "", ch, dt ? dt : "");
    snprintf(title, sizeof title, "%s: %s = %.4g %s",
             loc ? loc : "WQP site", ch, v, un);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = dt;
    row.lang            = "en";
    row.link            = "https://www.waterqualitydata.us/";
    row.record_type     = "water-quality-result";
    if (st) { row.has_geo = 1; row.lat = st->lat; row.lon = st->lon; }
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"water-quality\",\"usa\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(rows);
  free(stns);
  if (srows) cJSON_Delete(srows);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_water_quality_portal_def = {
  .id = SRC, .collector = "environment",
  .name = "US Water Quality Portal results (EPA/USGS/states)",
  .update_interval_sec = 86400, .run = run,
  .category = "environment", .type = "dataset",
  .url = "https://www.waterqualitydata.us/data/Result/search",
  .description = "Federated US water-quality chemistry results with the unit reported by the source organisation and station coordinates.",
  .license = "US public domain (EPA/USGS/NWQMC). Keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_water_quality_portal_def)
