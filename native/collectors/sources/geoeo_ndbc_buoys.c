/* NOAA NDBC — the latest observation from every reporting NDBC and
 * international partner buoy worldwide, in one file.
 *
 * Endpoint (keyless):
 *   https://www.ndbc.noaa.gov/data/latest_obs/latest_obs.txt
 * Emits per buoy: station id, latitude, longitude, observation timestamp and
 *   every reported measurement column (WDIR, WSPD, GST, WVHT, DPD, APD, MWD,
 *   PRES, PTDY, ATMP, WTMP, DEWP, VIS, TIDE) that actually carried a value.
 * Licence: NOAA/NDBC public domain.
 *
 * parse_notes honoured:
 *  - Whitespace-delimited text with TWO leading '#' comment lines (names, then
 *    units). Both are skipped; the first supplies the column names so nothing
 *    is indexed by a hardcoded position.
 *  - LAT and LON are decimal degrees and were populated on every row; a row
 *    where either fails a full-string numeric parse is emitted with has_geo=0.
 *  - MISSING VALUES ARE THE LITERAL STRING "MM", not empty and not a number.
 *    Every numeric column can be "MM", and letting atof turn it into 0.0
 *    produces fake zero-metre wave heights — so a cell is only emitted when it
 *    parses completely as a number, which "MM" never does.
 *  - latest_obs.json does NOT exist (404); this text file is the path.
 *  - The station metadata list (activestations.xml, with lat/lon as ATTRIBUTES
 *    and a dart='y' subset) is a separate document and is not merged here.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

static const char *URL = "https://www.ndbc.noaa.gov/data/latest_obs/latest_obs.txt";
#define NDBC_MAXCOL 40

static int ws_split(char *line, char **out, int max) {
  int n = 0;
  char *p = line;
  while (*p && n < max) {
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) break;
    out[n++] = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    if (*p) *p++ = 0;
  }
  return n;
}

/* CASE-SENSITIVE on purpose: this header has both "MM" (month) and "mm"
 * (minute), so a case-insensitive lookup for "mm" silently returns the month
 * column and every timestamp comes out wrong. */
static int col_of(char **h, int hn, const char *name) {
  for (int i = 0; i < hn; i++)
    if (strcmp(h[i], name) == 0) return i;
  return -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, URL, 45000);
  if (!body) {
    fprintf(stderr, "[ndbc-buoy-latest] fetch failed\n");
    return -1;
  }

  static const char *MEASURES[] = { "WDIR", "WSPD", "GST", "WVHT", "DPD",
                                    "APD", "MWD", "PRES", "PTDY", "ATMP",
                                    "WTMP", "DEWP", "VIS", "TIDE", NULL };
  char *hdr_names[NDBC_MAXCOL];
  int hn = 0, comments = 0, n = 0;
  int c_stn = -1, c_lat = -1, c_lon = -1;
  int c_y = -1, c_mo = -1, c_d = -1, c_h = -1, c_mi = -1;

  for (char *line = body; line && *line;) {
    char *nl = strchr(line, '\n');
    if (nl) *nl = 0;
    char *cur = line;
    line = nl ? nl + 1 : NULL;
    geoeo_trim(cur);
    if (!cur[0]) continue;

    if (cur[0] == '#') {
      comments++;
      if (comments == 1) {                      /* names line */
        hn = ws_split(cur + 1, hdr_names, NDBC_MAXCOL);
        c_stn = col_of(hdr_names, hn, "STN");
        if (c_stn < 0) c_stn = 0;
        c_lat = col_of(hdr_names, hn, "LAT");
        c_lon = col_of(hdr_names, hn, "LON");
        c_y = col_of(hdr_names, hn, "YYYY");
        c_mo = col_of(hdr_names, hn, "MM");
        c_d = col_of(hdr_names, hn, "DD");
        c_h = col_of(hdr_names, hn, "hh");
        c_mi = col_of(hdr_names, hn, "mm");
      }
      continue;                                  /* units line skipped too */
    }
    if (hn == 0) continue;                       /* never guess columns */

    char *f[NDBC_MAXCOL];
    int fn = ws_split(cur, f, NDBC_MAXCOL);
    if (fn < 3 || c_stn >= fn) continue;
    const char *stn = f[c_stn];
    if (!stn || !stn[0]) continue;

    double lat = 0, lon = 0;
    int geo = c_lat >= 0 && c_lon >= 0 && c_lat < fn && c_lon < fn &&
              geoeo_strtod(f[c_lat], &lat) && geoeo_strtod(f[c_lon], &lon) &&
              geoeo_ll_ok(lat, lon);

    char when[40];
    when[0] = 0;
    if (c_y >= 0 && c_mo >= 0 && c_d >= 0 && c_h >= 0 && c_mi >= 0 &&
        c_y < fn && c_mo < fn && c_d < fn && c_h < fn && c_mi < fn) {
      double y, mo, d, h, mi;
      if (geoeo_strtod(f[c_y], &y) && geoeo_strtod(f[c_mo], &mo) &&
          geoeo_strtod(f[c_d], &d) && geoeo_strtod(f[c_h], &h) &&
          geoeo_strtod(f[c_mi], &mi))
        snprintf(when, sizeof when, "%04d-%02d-%02dT%02d:%02d:00Z", (int)y,
                 (int)mo, (int)d, (int)h, (int)mi);
    }

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "station", stn);
    if (when[0]) cJSON_AddStringToObject(props, "observed_at", when);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    int measured = 0;
    double wvht = 0, wspd = 0;
    int has_wvht = 0, has_wspd = 0;
    for (int i = 0; MEASURES[i]; i++) {
      int c = col_of(hdr_names, hn, MEASURES[i]);
      if (c < 0 || c >= fn) continue;
      double v = 0;
      if (!geoeo_strtod(f[c], &v)) continue;     /* "MM" never parses */
      cJSON_AddNumberToObject(props, MEASURES[i], v);
      measured++;
      if (strcmp(MEASURES[i], "WVHT") == 0) { wvht = v; has_wvht = 1; }
      if (strcmp(MEASURES[i], "WSPD") == 0) { wspd = v; has_wspd = 1; }
    }
    cJSON_AddNumberToObject(props, "reported_measures", measured);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    char title[192];
    snprintf(title, sizeof title, "NDBC buoy %s", stn);

    char summary[192];
    int w = 0;
    summary[0] = 0;
    if (has_wvht)
      w += snprintf(summary + w, sizeof summary - (size_t)w,
                    "waves %.1f m", wvht);
    if (has_wspd && w < (int)sizeof summary)
      snprintf(summary + w, sizeof summary - (size_t)w, "%swind %.1f m/s",
               w ? " · " : "", wspd);

    char link[128];
    snprintf(link, sizeof link, "https://www.ndbc.noaa.gov/station_page.php?station=%s",
             stn);

    intel_item it = {0};
    it.remote_key = stn;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.link = link;
    it.lang = "en";
    it.published_at = when[0] ? when : NULL;
    it.record_type = "buoy-observation";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"buoy\",\"ocean\",\"weather\",\"ndbc\",\"noaa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }

  free(body);
  if (hn == 0) {
    fprintf(stderr, "[ndbc-buoy-latest] no '#' header line — unparseable\n");
    return -1;
  }
  fprintf(stderr, "[ndbc-buoy-latest] emitted %d buoys\n", n);
  return 0;
}

static const source_def geoeo_ndbc_def = {
  .id = "ndbc-buoy-latest", .collector = "environment",
  .name = "NOAA NDBC Latest Buoy Observations",
  .update_interval_sec = 1800, .run = run,
  .category = "environment", .type = "dataset",
  .url = "https://www.ndbc.noaa.gov/data/latest_obs/latest_obs.txt",
  .description = "The latest observation from all reporting NDBC and international partner buoys worldwide — wind, waves, pressure and sea temperature with each buoy's position.",
  .license = "NOAA/NDBC public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_ndbc_def)
