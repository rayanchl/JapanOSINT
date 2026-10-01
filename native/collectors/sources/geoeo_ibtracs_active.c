/* IBTrACS ACTIVE — the WMO-sanctioned merged best-track archive, active subset.
 * Every currently-live tropical cyclone's 6-hourly track with each warning
 * centre's independent position and intensity fix side by side.
 *
 * Endpoint (keyless):
 *   https://www.ncei.noaa.gov/data/international-best-track-archive-for-climate-
 *   stewardship-ibtracs/v04r01/access/csv/ibtracs.ACTIVE.list.v04r01.csv
 * Emits per track fix: SID, SEASON, BASIN, NAME, ISO_TIME, NATURE, LAT, LON,
 *   WMO_WIND, WMO_PRES, WMO_AGENCY, DIST2LAND, LANDFALL, USA_SSHS and the
 *   RSMC Tokyo columns (TOKYO_LAT/TOKYO_LON/TOKYO_GRADE).
 * Licence: NOAA NCEI, public domain / open data. Cite IBTrACS v04r01.
 *
 * parse_notes honoured:
 *  - The file has TWO header lines: line 1 is the column names, line 2 is the
 *    UNITS ("degrees_north", ...). The units line is skipped explicitly; left
 *    in, it becomes a row with LAT="degrees_north".
 *  - The ACTIVE subset (~107 KB) is used. ibtracs.last3years is 9.5 MB and
 *    re-downloading it every cycle would be abusive.
 *  - Empty cells are SPACE-PADDED (" "), not empty strings, so every value goes
 *    through a trimming full-string numeric parse; agency columns that are
 *    blank for a basin therefore yield "no value", not 0.0.
 *  - A fix whose canonical LAT or LON does not parse is emitted with has_geo=0.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/csv.h"
#include "geoeo_common.inc"

static const char *URL =
    "https://www.ncei.noaa.gov/data/international-best-track-archive-for-"
    "climate-stewardship-ibtracs/v04r01/access/csv/ibtracs.ACTIVE.list.v04r01.csv";

/* Trimmed cell text, or NULL when the cell is blank/space-padded. */
static const char *cell(cJSON *row, const char *k, char *buf, size_t n) {
  const cJSON *v = cJSON_GetObjectItem(row, k);
  if (!cJSON_IsString(v) || !v->valuestring) return NULL;
  snprintf(buf, n, "%s", v->valuestring);
  geoeo_trim(buf);
  return buf[0] ? buf : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, URL, 60000);
  if (!body) {
    fprintf(stderr, "[ibtracs-active] fetch failed\n");
    return -1;
  }
  cJSON *rows = csv_parse(body, 1);
  free(body);
  if (!cJSON_IsArray(rows)) {
    fprintf(stderr, "[ibtracs-active] CSV parse failed\n");
    cJSON_Delete(rows);
    return -1;
  }

  static const char *TEXT_COLS[] = { "SID", "SEASON", "BASIN", "SUBBASIN",
                                     "NAME", "ISO_TIME", "NATURE", "WMO_AGENCY",
                                     "TRACK_TYPE", "USA_SSHS", "TOKYO_GRADE",
                                     NULL };
  static const char *NUM_COLS[] = { "WMO_WIND", "WMO_PRES", "DIST2LAND",
                                    "LANDFALL", "STORM_SPEED", "STORM_DIR",
                                    "TOKYO_LAT", "TOKYO_LON", NULL };

  int n = 0, idx = 0;
  cJSON *row;
  cJSON_ArrayForEach(row, rows) {
    /* Row 0 of the parsed body is the UNITS line, not data. */
    if (idx++ == 0) continue;

    char b1[128], b2[128], b3[128], b4[128];
    const char *sid = cell(row, "SID", b1, sizeof b1);
    const char *iso = cell(row, "ISO_TIME", b2, sizeof b2);
    if (!sid || !iso) continue;
    const char *name = cell(row, "NAME", b3, sizeof b3);
    const char *basin = cell(row, "BASIN", b4, sizeof b4);

    char lb[64], ob[64];
    const char *slat = cell(row, "LAT", lb, sizeof lb);
    const char *slon = cell(row, "LON", ob, sizeof ob);
    double lat = 0, lon = 0;
    int geo = slat && slon && geoeo_strtod(slat, &lat) &&
              geoeo_strtod(slon, &lon) && geoeo_ll_ok(lat, lon);

    cJSON *props = cJSON_CreateObject();
    for (int i = 0; TEXT_COLS[i]; i++) {
      char tb[160];
      const char *v = cell(row, TEXT_COLS[i], tb, sizeof tb);
      if (v) cJSON_AddStringToObject(props, TEXT_COLS[i], v);
    }
    for (int i = 0; NUM_COLS[i]; i++) {
      char nb[64];
      double d = 0;
      const char *v = cell(row, NUM_COLS[i], nb, sizeof nb);
      if (v && geoeo_strtod(v, &d)) cJSON_AddNumberToObject(props, NUM_COLS[i], d);
    }
    if (geo) { cJSON_AddNumberToObject(props, "LAT", lat);
               cJSON_AddNumberToObject(props, "LON", lon); }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON *wind = cJSON_GetObjectItem(props, "WMO_WIND");
    double kt = cJSON_IsNumber(wind) ? wind->valuedouble : 0;
    int has_kt = cJSON_IsNumber(wind) ? 1 : 0;
    cJSON_Delete(props);

    char title[288];
    snprintf(title, sizeof title, "%s (%s) track fix %s",
             name ? name : sid, basin ? basin : "?", iso);

    char summary[160];
    if (has_kt) snprintf(summary, sizeof summary, "WMO wind %.0f kt", kt);
    else summary[0] = 0;

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    char key[192];
    snprintf(key, sizeof key, "%s|%s", sid, iso);

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.lang = "en";
    it.published_at = iso;
    it.record_type = "cyclone-track-fix";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"cyclone\",\"best-track\",\"ibtracs\",\"noaa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(rows);
  /* Out of season the ACTIVE file has headers and no data rows — R3 return 0. */
  fprintf(stderr, "[ibtracs-active] emitted %d track fixes\n", n);
  return 0;
}

static const source_def geoeo_ibtracs_def = {
  .id = "ibtracs-active", .collector = "environment",
  .name = "IBTrACS Active Tropical Cyclone Tracks (NOAA NCEI)",
  .update_interval_sec = 10800, .run = run,
  .category = "environment", .type = "dataset",
  .url = "https://www.ncei.noaa.gov/data/international-best-track-archive-for-climate-stewardship-ibtracs/v04r01/access/csv/ibtracs.ACTIVE.list.v04r01.csv",
  .description = "The WMO-sanctioned merged best-track archive, ACTIVE subset — every live cyclone's 6-hourly track with each warning centre's independent fix, including the RSMC Tokyo columns.",
  .license = "NOAA NCEI, public domain / open data. Cite IBTrACS v04r01.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_ibtracs_def)
