/* DWD current weather report per station (POI), straight off opendata.dwd.de.
 * Endpoint: https://opendata.dwd.de/weather/weather_reports/poi/<id>-BEOB.csv
 * Keyless, SEMICOLON separated, with THREE header lines:
 *   line 1  machine parameter names (cells 0-1 are labels, 2.. the parameters)
 *   line 2  the UNITS row — its FIRST cell is the STATION ID, then "Unit",
 *           then one unit per parameter ("Grad C", "hPa", "%", "km/h", ...)
 *   line 3  German column labels ("Datum";"Uhrzeit (UTC)";...)
 * Data starts at line 4, newest hour first. Time column is UTC.
 *
 * DECIMAL-COMMA TRAP, quoted from the source survey: "DECIMAL COMMA ('17,1' =
 * 17.1) - a plain atof on the raw token returns 17." de_num() below rewrites
 * the comma to a point before strtod.
 * MISSING-VALUE TRAP: missing values are the literal '---', NEVER empty, so a
 * column-count check cannot catch a slide; '---' is tested for explicitly.
 * COLUMN SAFETY: split_semi() is positional and never collapses empty fields.
 * GEO (R2): this file carries NO coordinates and the DWD POI directory does not
 * publish them alongside, so rows are emitted WITHOUT geometry rather than
 * pinned to a guessed station location.
 * Licence: GeoNutzV / DWD open data — free reuse with attribution to
 * Deutscher Wetterdienst. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "dwd-poi-observations"
#define MAXCOL 96

/* DWD POI station ids (the file itself confirms the id in the units row). */
static const char *STATIONS[] = { "10381", "10865", "10147", "10637", "10488",
                                  "10739", NULL };

/* Positional split on ';' that never collapses empty fields. */
static int split_semi(char *s, char **out, int max) {
  int n = 0;
  out[n++] = s;
  for (char *p = s; *p && n < max; p++)
    if (*p == ';') { *p = 0; out[n++] = p + 1; }
  return n;
}
static char *trimc(char *s) {
  while (*s == ' ' || *s == '\r') s++;
  size_t l = strlen(s);
  while (l && (s[l-1] == ' ' || s[l-1] == '\r' || s[l-1] == '\n')) s[--l] = 0;
  return s;
}
/* German decimal comma -> point, then strtod. */
static int de_num(const char *tok, double *out) {
  if (!tok || !*tok) return 0;
  if (strcmp(tok, "---") == 0) return 0;         /* explicit missing token */
  char buf[64]; size_t j = 0;
  for (const char *p = tok; *p && j + 1 < sizeof buf; p++)
    buf[j++] = (*p == ',') ? '.' : *p;
  buf[j] = 0;
  char *e = NULL;
  double v = strtod(buf, &e);
  if (e == buf) return 0;
  *out = v;
  return 1;
}
static char *next_line(char **save) {
  char *l = *save;
  if (!l || !*l) return NULL;
  char *nl = strchr(l, '\n');
  if (nl) { *nl = 0; *save = nl + 1; } else *save = l + strlen(l);
  return l;
}

static int collect_station(const source_ctx *ctx, intel_sink *sink,
                           const char *sid, int *fetched) {
  char url[192];
  snprintf(url, sizeof url,
    "https://opendata.dwd.de/weather/weather_reports/poi/%s-BEOB.csv", sid);
  char *txt = feed_get_text(ctx->http, url, 30000);
  if (!txt) return 0;
  *fetched = 1;

  char *save = txt;
  char *l1 = next_line(&save);   /* parameter names  */
  char *l2 = next_line(&save);   /* UNITS + station id in cell 0 */
  char *l3 = next_line(&save);   /* German labels */
  char *l4 = next_line(&save);   /* newest data row */
  if (!l1 || !l2 || !l3 || !l4) { free(txt); return 0; }

  char b1[8192], b2[4096], b4[4096];
  snprintf(b1, sizeof b1, "%s", l1);
  snprintf(b2, sizeof b2, "%s", l2);
  snprintf(b4, sizeof b4, "%s", l4);

  char *names[MAXCOL], *units[MAXCOL], *vals[MAXCOL];
  int nn = split_semi(b1, names, MAXCOL);
  int nu = split_semi(b2, units, MAXCOL);
  int nv = split_semi(b4, vals,  MAXCOL);
  if (nn < 3 || nu < 3 || nv < 3) { free(txt); return 0; }

  const char *file_station = trimc(units[0]);   /* units row cell 0 = id */
  const char *date = trimc(vals[0]);            /* dd.mm.yy */
  const char *hour = trimc(vals[1]);            /* HH:MM, UTC */

  int n = 0;
  int lim = nn < nu ? nn : nu;
  if (nv < lim) lim = nv;
  for (int i = 2; i < lim; i++) {
    const char *pname = trimc(names[i]);
    const char *unit  = trimc(units[i]);
    char *tok = trimc(vals[i]);
    if (!*pname || !*unit) continue;
    if (strcmp(unit, "CODE_TABLE") == 0) continue;   /* a code, not a measure */
    double v;
    if (!de_num(tok, &v)) continue;             /* '---' or empty -> no row */

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "station_id", file_station);
    cJSON_AddStringToObject(p, "parameter", pname);
    cJSON_AddNumberToObject(p, "value", v);
    cJSON_AddStringToObject(p, "unit", unit);
    cJSON_AddStringToObject(p, "date_ddmmyy", date);
    cJSON_AddStringToObject(p, "time_utc", hour);
    cJSON_AddStringToObject(p, "country", "DE");
    cJSON_AddStringToObject(p, "geo_note",
      "the DWD POI file carries no station coordinates; none invented");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[192], title[288];
    snprintf(key, sizeof key, "%s|%s|%s %s", file_station, pname, date, hour);
    snprintf(title, sizeof title, "DWD %s %s = %.2f %s (%s %s UTC)",
             file_station, pname, v, unit, date, hour);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "de";
    row.link            = url;
    row.record_type     = "weather-station";
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"weather\",\"germany\",\"dwd\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  free(txt);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, fetched = 0;
  for (int i = 0; STATIONS[i]; i++)
    total += collect_station(ctx, sink, STATIONS[i], &fetched);
  if (!fetched) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  fprintf(stderr, "[" SRC "] emitted %d\n", total);
  return 0;
}

static const source_def eni_dwd_poi_observations_def = {
  .id = SRC, .collector = "environment",
  .name = "DWD current weather report per station (POI)",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "dataset",
  .url = "https://opendata.dwd.de/weather/weather_reports/poi/10381-BEOB.csv",
  .description = "Hourly synoptic observations for DWD reporting stations, each value carrying the unit stated in the file's own units row (decimal comma converted).",
  .license = "GeoNutzV / DWD open data — free reuse with attribution to Deutscher Wetterdienst.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_dwd_poi_observations_def)
