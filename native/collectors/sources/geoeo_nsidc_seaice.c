/* NSIDC Sea Ice Index — the daily Arctic sea ice extent series, the canonical
 * climate indicator for polar sea ice.
 *
 * Endpoint (keyless):
 *   https://noaadata.apps.nsidc.org/NOAA/G02135/north/daily/data/
 *   N_seaice_extent_daily_v4.0.csv
 * Emits per day (the recent tail only): Year, Month, Day, Extent (million
 *   sq km), Missing (million sq km) and the Source Data string.
 * Licence: NSIDC/NOAA, freely available for research and reuse with citation
 *   (Sea Ice Index, doi:10.7265/N5K072F8).
 *
 * parse_notes honoured:
 *  - PATH VERSION MATTERS: the widely-cited *_v3.0.csv path now 404s; v4.0 is
 *    the live file.
 *  - TWO header lines — names, then units. The units line is skipped by index.
 *    Its trailing prose sentence contains commas, so it is never naive-split;
 *    a real CSV reader parses the file and the units row is simply dropped.
 *  - 'Source Data' is a bracketed python-list string containing commas inside
 *    quotes — handled by the quoted-field CSV parser, not by splitting.
 *  - NO GEOMETRY: this is a hemispheric aggregate, so has_geo=0 on every row.
 *  - 1.8 MB growing by one row a day; only the recent tail is emitted rather
 *    than re-ingesting 15,000 historical rows on every daily run.
 *  - The southern hemisphere series is the same path with south/ and S_
 *    substituted; both are fetched here, and a failure of the second does not
 *    fail the run.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/csv.h"
#include "geoeo_common.inc"

#define NSIDC_TAIL 45

static int hemisphere(const source_ctx *ctx, intel_sink *sink, const char *dir,
                      const char *prefix, const char *label, int *emitted) {
  char url[288];
  snprintf(url, sizeof url,
           "https://noaadata.apps.nsidc.org/NOAA/G02135/%s/daily/data/"
           "%s_seaice_extent_daily_v4.0.csv",
           dir, prefix);
  char *body = feed_get_text(ctx->http, url, 90000);
  if (!body) return -1;
  cJSON *rows = csv_parse(body, 1);
  free(body);
  if (!cJSON_IsArray(rows)) {
    cJSON_Delete(rows);
    return -1;
  }

  int total = cJSON_GetArraySize(rows);
  int first = total - NSIDC_TAIL;
  if (first < 1) first = 1;            /* index 0 is the UNITS line, not data */

  for (int i = first; i < total; i++) {
    cJSON *r = cJSON_GetArrayItem(rows, i);
    double y = 0, mo = 0, d = 0, ext = 0, miss = 0;
    if (!geoeo_numlax(r, "Year", &y) || !geoeo_numlax(r, "Month", &mo) ||
        !geoeo_numlax(r, "Day", &d))
      continue;                        /* the units row never parses as a date */
    int has_ext = geoeo_numlax(r, "Extent", &ext);
    int has_miss = geoeo_numlax(r, "Missing", &miss);

    char iso[16];
    snprintf(iso, sizeof iso, "%04d-%02d-%02d", (int)y, (int)mo, (int)d);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddNumberToObject(props, "year", y);
    cJSON_AddNumberToObject(props, "month", mo);
    cJSON_AddNumberToObject(props, "day", d);
    if (has_ext) cJSON_AddNumberToObject(props, "extent_million_km2", ext);
    if (has_miss) cJSON_AddNumberToObject(props, "missing_million_km2", miss);
    geoeo_copy(props, r, "Source Data");
    cJSON_AddStringToObject(props, "hemisphere", label);
    cJSON_AddStringToObject(props, "geo_note",
                            "hemispheric aggregate; no coordinate applies");
    cJSON_AddStringToObject(props, "citation",
                            "NSIDC Sea Ice Index, doi:10.7265/N5K072F8");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[192];
    if (has_ext)
      snprintf(title, sizeof title, "%s sea ice extent %s — %.3f million km2",
               label, iso, ext);
    else
      snprintf(title, sizeof title, "%s sea ice extent %s", label, iso);

    char key[96];
    snprintf(key, sizeof key, "%s|%s", prefix, iso);

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.lang = "en";
    it.published_at = iso;
    it.record_type = "sea-ice-extent";
    it.has_geo = 0;                     /* hemispheric aggregate */
    it.geometry_geojson = NULL;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"sea-ice\",\"climate\",\"arctic\",\"nsidc\"]";
    if (sink->emit(sink, &it) >= 0) (*emitted)++;
    free(pj);
  }
  cJSON_Delete(rows);
  return total;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0;
  int north = hemisphere(ctx, sink, "north", "N", "Northern", &n);
  if (north < 0) {
    fprintf(stderr, "[nsidc-sea-ice-extent] northern series fetch failed\n");
    return -1;
  }
  hemisphere(ctx, sink, "south", "S", "Southern", &n);   /* best-effort */
  fprintf(stderr, "[nsidc-sea-ice-extent] emitted %d daily rows\n", n);
  return 0;
}

static const source_def geoeo_nsidc_def = {
  .id = "nsidc-sea-ice-extent", .collector = "environment",
  .name = "NSIDC Sea Ice Index Daily Extent",
  .update_interval_sec = 86400, .run = run,
  .category = "environment", .type = "dataset",
  .url = "https://noaadata.apps.nsidc.org/NOAA/G02135/north/daily/data/N_seaice_extent_daily_v4.0.csv",
  .description = "The NSIDC Sea Ice Index daily Arctic (and Antarctic) ice extent series — the canonical climate indicator for polar sea ice, updated daily.",
  .license = "NSIDC/NOAA, free for research and reuse with citation (Sea Ice Index, doi:10.7265/N5K072F8).",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_nsidc_def)
