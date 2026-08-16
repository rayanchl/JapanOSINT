/* collectors/sources/tsp_celestrak_space_weather.c
 * CelesTrak space weather indices — the canonical Kp/Ap/F10.7 file used for
 * satellite drag and HF propagation modelling.
 * Endpoint: https://celestrak.org/SpaceData/SW-Last5Years.csv  (keyless CSV)
 * Emits per day: the eight 3-hourly Kp values and their sum, the eight Ap
 *   values and the daily Ap average, Cp, C9, international sunspot number,
 *   observed and adjusted 10.7 cm solar flux with their 81-day centred and
 *   trailing means, and F10.7_DATA_TYPE (OBS / INT / PRD).
 * Geo: none (a planetary index has no location) — has_geo 0 (R2).
 *
 * parse_notes honoured:
 *  - "Comma-delimited with header; 32 columns, fixed order" — columns are
 *    nonetheless resolved BY NAME from the header row, so a column insertion
 *    upstream cannot silently shift the values.
 *  - "Kp values are integers x10 (e.g. 37 = Kp 3.7)": both the raw integer and
 *    the divided value are emitted, the divided one clearly named.
 *  - "F10.7_DATA_TYPE is OBS / INT / PRD — only OBS rows are measurements, the
 *    tail of the file is prediction": the flag is carried on every row so a
 *    prediction is never mistaken for a measurement.
 *  - "Trailing days can have empty cells": empty cells are omitted, not zeroed.
 * STATED BOUND: the file holds ~2,050 daily rows; only the last WINDOW_DAYS are
 *   emitted (the rest never change), capped at MAX_ROWS.
 * Licence: CelesTrak usage policy — free reuse, one retrieval per update
 *   cycle, attribution to CelesTrak.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SW_URL "https://celestrak.org/SpaceData/SW-Last5Years.csv"
#define WINDOW_DAYS 30
#define MAX_ROWS 120
#define MAXCOL 48

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
/* add "<key>" as the raw string and "<key>_value" as int/10 (Kp is x10) */
static void add_kp(cJSON *o, const char *key, const char *raw) {
  if (!raw) return;
  cJSON_AddStringToObject(o, key, raw);
  char *e = NULL;
  double d = strtod(raw, &e);
  if (e && e != raw) {
    char vk[32];
    snprintf(vk, sizeof vk, "%s_value", key);
    cJSON_AddNumberToObject(o, vk, d / 10.0);
  }
}
static void add_num(cJSON *o, const char *key, const char *raw) {
  if (!raw) return;
  char *e = NULL;
  double d = strtod(raw, &e);
  if (e && e != raw) cJSON_AddNumberToObject(o, key, d);
  else cJSON_AddStringToObject(o, key, raw);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "User-Agent: JapanOSINT/1.0 (space-weather)", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", SW_URL, hdrs, NULL, 0, 45000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[celestrak-space-weather] http status=%ld\n", hr.status);
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
  if (i_date < 0) {
    fprintf(stderr, "[celestrak-space-weather] no DATE column\n");
    free(body);
    return -1;
  }
  int i_kp[8], i_ap[8];
  for (int k = 0; k < 8; k++) {
    char nm[8];
    snprintf(nm, sizeof nm, "KP%d", k + 1); i_kp[k] = jo_col_of(hc, nh, nm);
    snprintf(nm, sizeof nm, "AP%d", k + 1); i_ap[k] = jo_col_of(hc, nh, nm);
  }
  int i_kpsum = jo_col_of(hc, nh, "KP_SUM");
  int i_apavg = jo_col_of(hc, nh, "AP_AVG");
  int i_cp    = jo_col_of(hc, nh, "CP");
  int i_c9    = jo_col_of(hc, nh, "C9");
  int i_isn   = jo_col_of(hc, nh, "ISN");
  int i_fobs  = jo_col_of(hc, nh, "F10.7_OBS");
  int i_fadj  = jo_col_of(hc, nh, "F10.7_ADJ");
  int i_ftyp  = jo_col_of(hc, nh, "F10.7_DATA_TYPE");
  int i_fc81  = jo_col_of(hc, nh, "F10.7_OBS_CENTER81");
  int i_fl81  = jo_col_of(hc, nh, "F10.7_OBS_LAST81");

  char cutoff[16];
  jo_days_ago_iso(WINDOW_DAYS, cutoff, sizeof cutoff);

  int n = 0, seen = 0;
  char *line;
  while ((line = jo_next_line_cr(&p)) != NULL) {
    if (!*line) continue;
    seen++;
    char *f[MAXCOL];
    int nf = tsp_split(line, f, MAXCOL);
    const char *date = tsp_cell(f, nf, i_date);
    if (!date || strcmp(date, cutoff) < 0) continue;
    if (n >= MAX_ROWS) break;

    const char *dtype = tsp_cell(f, nf, i_ftyp);
    const char *apavg = tsp_cell(f, nf, i_apavg);
    const char *fobs  = tsp_cell(f, nf, i_fobs);

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "date", date);
    cJSON *kparr = cJSON_CreateArray(), *aparr = cJSON_CreateArray();
    double kpmax = -1;
    for (int k = 0; k < 8; k++) {
      const char *kv = tsp_cell(f, nf, i_kp[k]);
      const char *av = tsp_cell(f, nf, i_ap[k]);
      if (kv) {
        char *e = NULL;
        double d = strtod(kv, &e);
        if (e && e != kv) {
          cJSON_AddItemToArray(kparr, cJSON_CreateNumber(d / 10.0));
          if (d / 10.0 > kpmax) kpmax = d / 10.0;
        }
      }
      if (av) {
        char *e = NULL;
        double d = strtod(av, &e);
        if (e && e != av) cJSON_AddItemToArray(aparr, cJSON_CreateNumber(d));
      }
    }
    cJSON_AddItemToObject(pr, "kp_3hourly", kparr);   /* already /10 */
    cJSON_AddItemToObject(pr, "ap_3hourly", aparr);
    add_kp(pr, "kp_sum", tsp_cell(f, nf, i_kpsum));
    add_num(pr, "ap_avg", apavg);
    add_num(pr, "cp", tsp_cell(f, nf, i_cp));
    add_num(pr, "c9", tsp_cell(f, nf, i_c9));
    add_num(pr, "sunspot_number", tsp_cell(f, nf, i_isn));
    add_num(pr, "f10_7_obs", fobs);
    add_num(pr, "f10_7_adj", tsp_cell(f, nf, i_fadj));
    add_num(pr, "f10_7_obs_center81", tsp_cell(f, nf, i_fc81));
    add_num(pr, "f10_7_obs_last81", tsp_cell(f, nf, i_fl81));
    if (dtype) cJSON_AddStringToObject(pr, "f10_7_data_type", dtype);
    cJSON_AddStringToObject(pr, "measurement_status",
      (dtype && strcmp(dtype, "OBS") == 0) ? "observed"
                                           : "not an observation (see f10_7_data_type)");
    cJSON_AddStringToObject(pr, "kp_units", "Kp values divided from the file's x10 integers");
    cJSON_AddStringToObject(pr, "source", "CelesTrak SW-Last5Years.csv");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[192], summary[224], ts[32];
    snprintf(ts, sizeof ts, "%sT00:00:00Z", date);
    if (kpmax >= 0)
      snprintf(title, sizeof title, "Space weather %s — max Kp %.1f%s%s",
               date, kpmax, dtype ? ", " : "", dtype ? dtype : "");
    else
      snprintf(title, sizeof title, "Space weather %s", date);
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s",
             apavg ? "Ap avg " : "", apavg ? apavg : "",
             fobs ? " · F10.7 " : "", fobs ? fobs : "",
             dtype ? " · " : "", dtype ? dtype : "");

    intel_item it = {0};
    it.remote_key      = date;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://celestrak.org/SpaceData/";
    it.lang            = "en";
    it.published_at    = ts;
    it.record_type     = "space-weather-index";
    it.properties_json = pj;
    it.tags_json       = "[\"space-weather\",\"geomagnetic\",\"hf-propagation\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[celestrak-space-weather] emitted %d of %d daily rows (>= %s)\n",
          n, seen, cutoff);
  return 0;
}

static const source_def tsp_celestrak_space_weather_def = {
  .id = "celestrak-space-weather", .collector = "satellite",
  .name = "CelesTrak space weather indices (Kp/Ap/F10.7)",
  .update_interval_sec = 21600, .run = run,
  .category = "satellite", .type = "dataset", .url = SW_URL,
  .description = "Daily geomagnetic and solar indices used for satellite drag and HF propagation modelling: 3-hourly Kp and Ap, sunspot number, 10.7 cm flux observed and adjusted, with observed/predicted flag.",
  .license = "CelesTrak usage policy — free reuse, one retrieval per update cycle, attribution to CelesTrak.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_celestrak_space_weather_def)
