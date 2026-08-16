/* US Drought Monitor area statistics — the weekly percentage of an area in each
 * drought class D0 through D4, the standard national drought indicator.
 *
 * Endpoint (keyless):
 *   https://usdmdataservices.unl.edu/api/USStatistics/
 *   GetDroughtSeverityStatisticsByAreaPercent?aoi=us&startdate=M/D/YYYY
 *   &enddate=M/D/YYYY&statisticsType=1
 * Emits per weekly map: MapDate, AreaOfInterest, None, D0, D1, D2, D3, D4,
 *        ValidStart, ValidEnd.
 * Licence: National Drought Mitigation Center (Univ. of Nebraska-Lincoln) with
 *   USDA and NOAA; USDM data are freely available for reuse with attribution.
 *
 * parse_notes honoured:
 *  - The path looks like JSON REST but returns CSV by default; that is what is
 *    parsed here (no Accept header is sent, matching the probed behaviour).
 *  - QUERY DATES MUST BE M/D/YYYY. ISO dates return an EMPTY BODY with HTTP
 *    200 — the exact silent-empty failure mode this contract exists to catch —
 *    so a zero-length body is treated as a FETCH FAILURE (-1), not an honest
 *    empty. The window is built from the current date in M/D/YYYY form.
 *  - NO GEOMETRY: rows are area aggregates keyed by AreaOfInterest ("CONUS",
 *    "Total"), so has_geo=0 on every row.
 *  - The sibling StateStatistics/GetDroughtSeverityStatisticsByArea path with
 *    aoi=us returned an empty body and is deliberately not used.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/csv.h"
#include "geoeo_common.inc"

static void mdy(time_t t, char *out, size_t n) {
  struct tm tmv;
#if defined(_WIN32)
  gmtime_s(&tmv, &t);
#else
  gmtime_r(&t, &tmv);
#endif
  /* M/D/YYYY — the ONLY form this API accepts. Components masked into range so
   * the formatted width is provably bounded. */
  snprintf(out, n, "%d/%d/%d", (tmv.tm_mon + 1) % 100, tmv.tm_mday % 100,
           (tmv.tm_year + 1900) % 10000);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL);
  char start[24], end[24];
  mdy(now - 28 * 24 * 3600, start, sizeof start);
  mdy(now, end, sizeof end);

  char url[448];
  snprintf(url, sizeof url,
           "https://usdmdataservices.unl.edu/api/USStatistics/"
           "GetDroughtSeverityStatisticsByAreaPercent?aoi=us&startdate=%s"
           "&enddate=%s&statisticsType=1",
           start, end);

  char *body = feed_get_text(ctx->http, url, 30000);
  if (!body) {
    fprintf(stderr, "[usdm-drought] fetch failed\n");
    return -1;
  }
  if (!body[0]) {
    /* Empty body with HTTP 200 = the query was rejected, not "no drought". */
    fprintf(stderr, "[usdm-drought] empty body — query rejected upstream\n");
    free(body);
    return -1;
  }

  cJSON *rows = csv_parse(body, 1);
  free(body);
  if (!cJSON_IsArray(rows)) {
    fprintf(stderr, "[usdm-drought] CSV parse failed\n");
    cJSON_Delete(rows);
    return -1;
  }

  static const char *CLASSES[] = { "None", "D0", "D1", "D2", "D3", "D4", NULL };
  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const char *mapdate = geoeo_str(r, "MapDate");
    const char *aoi = geoeo_str(r, "AreaOfInterest");
    if (!mapdate || !aoi) continue;

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "MapDate", mapdate);
    cJSON_AddStringToObject(props, "AreaOfInterest", aoi);
    geoeo_copy(props, r, "ValidStart");
    geoeo_copy(props, r, "ValidEnd");
    geoeo_copy(props, r, "StatisticFormatID");
    double d0 = 0, d4 = 0;
    int has_d0 = 0, has_d4 = 0;
    for (int i = 0; CLASSES[i]; i++) {
      double v = 0;
      if (!geoeo_numlax(r, CLASSES[i], &v)) continue;
      cJSON_AddNumberToObject(props, CLASSES[i], v);
      if (strcmp(CLASSES[i], "D0") == 0) { d0 = v; has_d0 = 1; }
      if (strcmp(CLASSES[i], "D4") == 0) { d4 = v; has_d4 = 1; }
    }
    cJSON_AddStringToObject(props, "geo_note",
                            "area aggregate keyed by AreaOfInterest; no "
                            "coordinate is published with the statistic");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[224];
    if (has_d0)
      snprintf(title, sizeof title, "US Drought Monitor %s — %s: %.2f%% D0+",
               mapdate, aoi, d0);
    else
      snprintf(title, sizeof title, "US Drought Monitor %s — %s", mapdate, aoi);

    char summary[160];
    if (has_d4) snprintf(summary, sizeof summary, "D4 exceptional: %.2f%%", d4);
    else summary[0] = 0;

    char key[128];
    snprintf(key, sizeof key, "%s|%s", mapdate, aoi);

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.lang = "en";
    it.published_at = geoeo_str(r, "ValidStart");
    it.record_type = "drought-statistic";
    it.has_geo = 0;                      /* area aggregate — no coordinate */
    it.geometry_geojson = NULL;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"drought\",\"usdm\",\"usa\",\"climate\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);                 /* cJSON_PrintUnformatted allocates; was leaked */
  }
  cJSON_Delete(rows);
  fprintf(stderr, "[usdm-drought] emitted %d weekly statistics\n", n);
  return 0;
}

static const source_def geoeo_usdm_def = {
  .id = "usdm-drought", .collector = "environment",
  .name = "US Drought Monitor Area Statistics",
  .update_interval_sec = 86400, .run = run,
  .category = "environment", .type = "api",
  .url = "https://usdmdataservices.unl.edu/api/USStatistics/GetDroughtSeverityStatisticsByAreaPercent",
  .description = "Weekly US Drought Monitor severity coverage — percentage of the area in each drought class D0 through D4, the standard national drought indicator.",
  .license = "National Drought Mitigation Center (UNL) with USDA and NOAA; free reuse with attribution.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_usdm_def)
