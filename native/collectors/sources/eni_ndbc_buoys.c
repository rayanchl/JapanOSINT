/* NOAA NDBC marine buoy observations.
 * Endpoint: https://www.ndbc.noaa.gov/data/latest_obs/latest_obs.txt (keyless)
 * This is the network-wide latest-observation file: the same realtime2 feed as
 * https://www.ndbc.noaa.gov/data/realtime2/<station>.txt but for every station
 * AND — crucially for R2 — carrying each station's own LAT/LON columns, which
 * the per-station file does not. Station coordinates are therefore never
 * guessed.
 *
 * Layout: two comment lines — line 1 the field names ("#STN LAT LON YYYY MM DD
 * hh mm WDIR WSPD ..."), line 2 the UNITS row ("#text deg deg yr mo day hr mn
 * degT m/s ..."). Missing values are the literal token "MM"; splitting on
 * whitespace is safe because MM occupies the column, and "MM" is rejected
 * explicitly rather than being read as a number. Times are UTC.
 * Emits per station only the fields that actually carry a value, each with its
 * unit: wind_dir_degt, wind_speed_mps, gust_mps, wave_height_m, dom_period_s,
 * avg_period_s, mean_wave_dir_degt, pressure_hpa, pressure_tendency_hpa,
 * air_temp_c, water_temp_c, dewpoint_c, visibility_nmi, tide_ft.
 * Licence: NOAA public domain. */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "ndbc-buoy-observations"
#define NFIELD 22

/* column index -> (property name, unit); NULL for the id/time columns */
static const struct { const char *name, *unit; } FIELDS[NFIELD] = {
  {NULL,NULL},{NULL,NULL},{NULL,NULL},                 /* STN LAT LON        */
  {NULL,NULL},{NULL,NULL},{NULL,NULL},{NULL,NULL},{NULL,NULL},  /* Y M D h m */
  {"wind_dir_degt",         "degT"},
  {"wind_speed_mps",        "m/s"},
  {"gust_mps",              "m/s"},
  {"wave_height_m",         "m"},
  {"dom_period_s",          "s"},
  {"avg_period_s",          "s"},
  {"mean_wave_dir_degt",    "degT"},
  {"pressure_hpa",          "hPa"},
  {"pressure_tendency_hpa", "hPa"},
  {"air_temp_c",            "degC"},
  {"water_temp_c",          "degC"},
  {"dewpoint_c",            "degC"},
  {"visibility_nmi",        "nmi"},
  {"tide_ft",               "ft"}
};

/* Whitespace tokenizer. Safe here (and only here) because the NDBC layout puts
 * the literal token "MM" in every missing column — no field is ever empty, so
 * collapsing runs of spaces cannot slide the column index. */
static int split_ws(char *s, char **out, int max) {
  int n = 0;
  char *p = s;
  while (*p && n < max) {
    while (*p == ' ' || *p == '\t' || *p == '\r') p++;
    if (!*p) break;
    out[n++] = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r') p++;
    if (*p) *p++ = 0;
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *txt = feed_get_text(ctx->http,
    "https://www.ndbc.noaa.gov/data/latest_obs/latest_obs.txt", 45000);
  if (!txt) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  int n = 0;
  char *save = txt, *line;
  while ((line = save) && *line) {
    char *nl = strchr(line, '\n');
    if (nl) { *nl = 0; save = nl + 1; } else save = line + strlen(line);
    if (!*line || line[0] == '#') continue;          /* names + units rows */

    char buf[512];
    snprintf(buf, sizeof buf, "%s", line);
    char *f[NFIELD];
    int nf = split_ws(buf, f, NFIELD);
    if (nf < 8) continue;

    const char *stn = f[0];
    char *e1 = NULL, *e2 = NULL;
    double lat = strtod(f[1], &e1), lon = strtod(f[2], &e2);
    if (e1 == f[1] || e2 == f[2]) continue;          /* no station geometry */
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) continue;

    char when[32] = {0};
    if (nf >= 8)
      snprintf(when, sizeof when, "%s-%s-%sT%s:%s:00Z",
               f[3], f[4], f[5], f[6], f[7]);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "station_id", stn);
    cJSON_AddStringToObject(p, "observed_at_utc", when);
    int have = 0;
    double wspd = 0; int have_wspd = 0;
    for (int i = 8; i < NFIELD && i < nf; i++) {
      if (!FIELDS[i].name) continue;
      const char *tok = f[i];
      if (!tok || !*tok || strcmp(tok, "MM") == 0) continue;   /* missing */
      char *e = NULL;
      double v = strtod(tok, &e);
      if (e == tok) continue;
      cJSON_AddNumberToObject(p, FIELDS[i].name, v);
      char uk[64];
      snprintf(uk, sizeof uk, "%s_unit", FIELDS[i].name);
      cJSON_AddStringToObject(p, uk, FIELDS[i].unit);
      if (strcmp(FIELDS[i].name, "wind_speed_mps") == 0) { wspd = v; have_wspd = 1; }
      have = 1;
    }
    if (!have) { cJSON_Delete(p); continue; }        /* all MM -> no row (R1) */
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char link[128], title[224];
    snprintf(link, sizeof link, "https://www.ndbc.noaa.gov/station_page.php?station=%s", stn);
    if (have_wspd)
      snprintf(title, sizeof title, "NDBC buoy %s: wind %.1f m/s at %s", stn, wspd, when);
    else
      snprintf(title, sizeof title, "NDBC buoy %s observation at %s", stn, when);

    intel_item row = {0};
    row.remote_key      = stn;
    row.title           = title;
    row.summary         = title;
    row.published_at    = when[0] ? when : NULL;
    row.lang            = "en";
    row.link            = link;
    row.record_type     = "marine-buoy";
    row.has_geo         = 1;
    row.lat = lat; row.lon = lon;
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"marine\",\"noaa\",\"buoy\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  free(txt);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_ndbc_buoys_def = {
  .id = SRC, .collector = "environment",
  .name = "NOAA NDBC marine buoy observations",
  .update_interval_sec = 1800, .run = run,
  .category = "environment", .type = "dataset",
  .url = "https://www.ndbc.noaa.gov/data/latest_obs/latest_obs.txt",
  .description = "Real-time wind, wave, pressure and sea-temperature observations from the US National Data Buoy Center network, with station coordinates from the feed itself.",
  .license = "NOAA public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_ndbc_buoys_def)
