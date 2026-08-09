/* collectors/sources/tsp_celestrak_eop.c
 * CelesTrak Earth orientation parameters — the polar motion / UT1-UTC / LOD /
 * nutation corrections and the leap-second count that any accurate ground-track
 * or GNSS frame conversion needs.
 * Endpoint: https://celestrak.org/SpaceData/EOP-Last5Years.csv  (keyless CSV)
 * Emits per day: DATE, MJD, polar motion X and Y (arcsec), UT1-UTC (s), LOD
 *   (s), DPSI/DEPS and DX/DY nutation corrections, DAT (integer TAI-UTC leap
 *   seconds) and DATA_TYPE (O = observed, P = predicted).
 * Geo: none — Earth orientation is a global quantity. has_geo 0 (R2).
 *
 * parse_notes honoured:
 *  - "Comma-delimited with header, 12 columns" — resolved BY NAME anyway.
 *  - "DATA_TYPE 'O' = observed, 'P' = predicted": carried on every row, and
 *    restated in plain words so a prediction is never read as a measurement.
 *  - "DAT is the integer TAI-UTC leap second count (37 currently) — a cheap way
 *    to detect a future leap-second announcement": DAT is emitted as a number
 *    and any change from the previous row is flagged in properties.
 * STATED BOUND: only the last WINDOW_DAYS of the ~1,700-row file are emitted
 *   (earlier rows never change), capped at MAX_ROWS.
 * Licence: CelesTrak usage policy — free reuse with attribution, derived from
 *   IERS.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EOP_URL "https://celestrak.org/SpaceData/EOP-Last5Years.csv"
#define WINDOW_DAYS 30
#define MAX_ROWS 120
#define MAXCOL 24

static int tsp_split(char *line, char **out, int max) {
  int n = 0;
  char *p = line;
  while (n < max) {
    char *start = p;
    while (*p && *p != ',') p++;
    if (*p == ',') { *p = '\0'; out[n++] = start; p++; }
    else { out[n++] = start; break; }
  }
  return n;
}
static const char *tsp_cell(char **f, int nf, int idx) {
  if (idx < 0 || idx >= nf || !f[idx] || !f[idx][0]) return NULL;
  return f[idx];
}
static void add_num(cJSON *o, const char *key, const char *raw) {
  if (!raw) return;
  char *e = NULL;
  double d = strtod(raw, &e);
  if (e && e != raw) cJSON_AddNumberToObject(o, key, d);
  else cJSON_AddStringToObject(o, key, raw);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "User-Agent: JapanOSINT/1.0 (eop)", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", EOP_URL, hdrs, NULL, 0, 45000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[celestrak-eop] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return -1;
  }
  char *body = hr.body;
  hr.body = NULL;
  http_response_free(&hr);

  char *p = body;
  char *head = jo_next_line_cr(&p);
  if (!head || !*head) { free(body); return -1; }
  char *hc[MAXCOL];
  int nh = tsp_split(head, hc, MAXCOL);
  int i_date = jo_col_of(hc, nh, "DATE");
  int i_mjd  = jo_col_of(hc, nh, "MJD");
  int i_x    = jo_col_of(hc, nh, "X");
  int i_y    = jo_col_of(hc, nh, "Y");
  int i_ut1  = jo_col_of(hc, nh, "UT1-UTC");
  int i_lod  = jo_col_of(hc, nh, "LOD");
  int i_dpsi = jo_col_of(hc, nh, "DPSI");
  int i_deps = jo_col_of(hc, nh, "DEPS");
  int i_dx   = jo_col_of(hc, nh, "DX");
  int i_dy   = jo_col_of(hc, nh, "DY");
  int i_dat  = jo_col_of(hc, nh, "DAT");
  int i_typ  = jo_col_of(hc, nh, "DATA_TYPE");
  if (i_date < 0) {
    fprintf(stderr, "[celestrak-eop] no DATE column\n");
    free(body);
    return -1;
  }

  char cutoff[16];
  jo_days_ago_iso(WINDOW_DAYS, cutoff, sizeof cutoff);

  int n = 0, seen = 0;
  double prev_dat = -1;
  char *line;
  while ((line = jo_next_line_cr(&p)) != NULL) {
    if (!*line) continue;
    seen++;
    char *f[MAXCOL];
    int nf = tsp_split(line, f, MAXCOL);
    const char *date = tsp_cell(f, nf, i_date);
    if (!date) continue;
    /* track DAT across the whole file so a change is detected even when the
     * previous row falls outside the emitted window */
    const char *sdat = tsp_cell(f, nf, i_dat);
    double dat = -1;
    if (sdat) { char *e = NULL; double d = strtod(sdat, &e); if (e && e != sdat) dat = d; }
    int leap_change = (prev_dat >= 0 && dat >= 0 && dat != prev_dat);
    if (dat >= 0) prev_dat = dat;

    if (strcmp(date, cutoff) < 0) continue;
    if (n >= MAX_ROWS) break;

    const char *typ = tsp_cell(f, nf, i_typ);
    const char *ut1 = tsp_cell(f, nf, i_ut1);

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "date", date);
    add_num(pr, "mjd", tsp_cell(f, nf, i_mjd));
    add_num(pr, "polar_motion_x_arcsec", tsp_cell(f, nf, i_x));
    add_num(pr, "polar_motion_y_arcsec", tsp_cell(f, nf, i_y));
    add_num(pr, "ut1_utc_s", ut1);
    add_num(pr, "lod_s", tsp_cell(f, nf, i_lod));
    add_num(pr, "dpsi_arcsec", tsp_cell(f, nf, i_dpsi));
    add_num(pr, "deps_arcsec", tsp_cell(f, nf, i_deps));
    add_num(pr, "dx_arcsec", tsp_cell(f, nf, i_dx));
    add_num(pr, "dy_arcsec", tsp_cell(f, nf, i_dy));
    if (dat >= 0) cJSON_AddNumberToObject(pr, "tai_utc_leap_seconds", dat);
    if (leap_change) cJSON_AddBoolToObject(pr, "leap_second_changed", 1);
    if (typ) cJSON_AddStringToObject(pr, "data_type", typ);
    cJSON_AddStringToObject(pr, "measurement_status",
      (typ && (typ[0] == 'O')) ? "observed" : "predicted (see data_type)");
    cJSON_AddStringToObject(pr, "source", "CelesTrak EOP-Last5Years.csv (IERS derived)");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[192], summary[224], ts[32];
    snprintf(ts, sizeof ts, "%sT00:00:00Z", date);
    snprintf(title, sizeof title, "Earth orientation %s%s%s", date,
             typ ? " (" : "", typ ? (typ[0] == 'O' ? "observed)" : "predicted)") : "");
    snprintf(summary, sizeof summary, "%s%s%s%.0f%s",
             ut1 ? "UT1-UTC " : "", ut1 ? ut1 : "no UT1-UTC",
             dat >= 0 ? " s · TAI-UTC " : "", dat >= 0 ? dat : 0.0,
             dat >= 0 ? " s" : "");

    intel_item it = {0};
    it.remote_key      = date;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://celestrak.org/SpaceData/";
    it.lang            = "en";
    it.published_at    = ts;
    it.record_type     = "earth-orientation";
    it.properties_json = pj;
    it.tags_json       = "[\"space\",\"geodesy\",\"gnss\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[celestrak-eop] emitted %d of %d daily rows (>= %s)\n",
          n, seen, cutoff);
  return 0;
}

static const source_def tsp_celestrak_eop_def = {
  .id = "celestrak-eop", .collector = "satellite",
  .name = "CelesTrak Earth orientation parameters",
  .update_interval_sec = 86400, .run = run,
  .category = "satellite", .type = "dataset", .url = EOP_URL,
  .description = "Daily polar motion, UT1-UTC, length-of-day and nutation corrections plus the current TAI-UTC leap-second count, flagged observed or predicted.",
  .license = "CelesTrak usage policy — free reuse with attribution, derived from IERS.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_celestrak_eop_def)
