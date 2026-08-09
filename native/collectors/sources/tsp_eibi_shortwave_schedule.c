/* collectors/sources/tsp_eibi_shortwave_schedule.c
 * EiBi international shortwave broadcast schedule — the reference list analysts
 * use to identify an unknown HF signal: international broadcasters, military
 * and navy stations, time signals, numbers stations and utility transmissions.
 * Endpoint: http://www.eibispace.de/dx/sked-<season>.csv   (keyless)
 * Emits per scheduled transmission: frequency in kHz, UTC time span, days of
 *   operation, ITU country code, station name, language code, target area,
 *   remarks (which carry the transmitter-site code), persistence code and the
 *   start/stop validity dates.
 *
 * TRAPS handled, quoted from the manifest:
 *  - "SEMICOLON-delimited, and the header row is unusual: 'kHz:75;
 *    Time(UTC):93;...' — the ':NN' suffix is a column width hint, strip it":
 *    the header parser truncates each name at ':' before matching.
 *  - "Plain HTTP only; the HTTPS certificate on this host has EXPIRED, so
 *    fetch over http:// or the TLS handshake fails": the URL is http://.
 *  - "Filename encodes the broadcast season (sked-b25 = B season 2025,
 *    sked-a26 = A season 2026) — the collector must derive or probe the
 *    current season file": the season is derived from today's month and year
 *    and, if that file is not served, the neighbouring seasons are probed in
 *    order. The file actually used is recorded in every row.
 *  - "No coordinates in the CSV; the 'Remarks' column carries a two-letter
 *    transmitter-site code that maps to a site list published separately in
 *    README.TXT (with DMS coordinates) — join only if you actually parse that
 *    list, never guess": that site list is NOT fetched here, so every row is
 *    has_geo=0 (R2) and the remarks code is emitted raw for a later join.
 *  - The file is ISO-8859-1, not UTF-8; bytes are widened to UTF-8 when the
 *    body does not validate as UTF-8, so no invalid bytes reach the database.
 * Licence: explicitly free — the distribution README states "All my frequency
 *   lists are free of cost, and any person is absolutely free to download, use,
 *   copy, or distribute" them. Attribution to Eibi Klingenfuss / eibispace.de.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_ROWS 12000
#define MAXCOL 24

static int semi_split(char *line, char **out, int max) {
  int n = 0;
  char *p = line;
  while (n < max) {
    char *s = p;
    while (*p && *p != ';') p++;
    if (*p == ';') { *p = '\0'; out[n++] = s; p++; }
    else { out[n++] = s; break; }
  }
  return n;
}
static char *trim(char *s) {
  while (*s == ' ') s++;
  size_t n = strlen(s);
  while (n && s[n - 1] == ' ') s[--n] = '\0';
  return s;
}
/* header names carry a ':NN' column-width hint — cut it off */
static void strip_width_hint(char *s) {
  char *c = strchr(s, ':');
  if (c) *c = '\0';
}
static const char *cell(char **f, int nf, int i) {
  if (i < 0 || i >= nf || !f[i] || !f[i][0]) return NULL;
  return f[i];
}
static int is_utf8(const char *s, size_t n) {
  const unsigned char *p = (const unsigned char *)s;
  for (size_t i = 0; i < n; ) {
    unsigned char c = p[i];
    size_t need;
    if (c < 0x80) { i++; continue; }
    else if ((c & 0xE0) == 0xC0) need = 1;
    else if ((c & 0xF0) == 0xE0) need = 2;
    else if ((c & 0xF8) == 0xF0) need = 3;
    else return 0;
    if (i + need >= n) return 0;
    for (size_t k = 1; k <= need; k++)
      if ((p[i + k] & 0xC0) != 0x80) return 0;
    i += need + 1;
  }
  return 1;
}
/* ISO-8859-1 -> UTF-8; malloc'd, caller frees. */
static char *latin1_to_utf8(const char *s, size_t n) {
  char *out = (char *)malloc(n * 2 + 1);
  if (!out) return NULL;
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) out[j++] = (char)c;
    else { out[j++] = (char)(0xC0 | (c >> 6)); out[j++] = (char)(0x80 | (c & 0x3F)); }
  }
  out[j] = '\0';
  return out;
}

/* Build the ordered list of season files to probe, current guess first. */
static int season_candidates(char cand[4][16]) {
  time_t t = time(NULL);
  struct tm tmv;
  int ok;
#if defined(_WIN32)
  ok = (gmtime_s(&tmv, &t) == 0);
#else
  ok = (gmtime_r(&t, &tmv) != NULL);
#endif
  int yy = ok ? (tmv.tm_year + 1900) % 100 : 25;
  int mon = ok ? tmv.tm_mon + 1 : 1;
  int prev = (yy + 99) % 100;
  int n = 0;
  /* A season runs late March to late October; B season the rest. */
  if (mon >= 4 && mon <= 9) {
    snprintf(cand[n++], 16, "a%02d", yy);
    snprintf(cand[n++], 16, "b%02d", prev);
    snprintf(cand[n++], 16, "b%02d", yy);
  } else if (mon >= 11) {
    snprintf(cand[n++], 16, "b%02d", yy);
    snprintf(cand[n++], 16, "a%02d", yy);
    snprintf(cand[n++], 16, "b%02d", prev);
  } else if (mon <= 2) {
    snprintf(cand[n++], 16, "b%02d", prev);
    snprintf(cand[n++], 16, "a%02d", yy);
    snprintf(cand[n++], 16, "b%02d", yy);
  } else {                              /* March or October: transition */
    snprintf(cand[n++], 16, "%c%02d", mon == 3 ? 'a' : 'b', yy);
    snprintf(cand[n++], 16, "%c%02d", mon == 3 ? 'b' : 'a', mon == 3 ? prev : yy);
    snprintf(cand[n++], 16, "a%02d", prev);
  }
  snprintf(cand[n++], 16, "a%02d", prev);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char cand[4][16];
  int ncand = season_candidates(cand);
  const char *hdrs[] = { "User-Agent: JapanOSINT/1.0 (eibi-schedule)", NULL };

  char *body = NULL;
  char season[16] = "";
  char url[128] = "";
  int transport_ok = 0;
  for (int i = 0; i < ncand && !body; i++) {
    snprintf(url, sizeof url, "http://www.eibispace.de/dx/sked-%s.csv", cand[i]);
    http_response hr = {0};
    int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 45000, 1, &hr);
    if (rc == 0) transport_ok = 1;
    if (rc == 0 && hr.status == 200 && hr.body && hr.body_len > 1000) {
      size_t blen = hr.body_len;
      char *raw = hr.body;
      hr.body = NULL;
      http_response_free(&hr);
      if (!is_utf8(raw, blen)) {          /* EiBi ships ISO-8859-1 */
        char *conv = latin1_to_utf8(raw, blen);
        if (conv) { free(raw); raw = conv; }
      }
      body = raw;
      snprintf(season, sizeof season, "%.15s", cand[i]);
    } else {
      fprintf(stderr, "[eibi-shortwave-schedule] sked-%s.csv status=%ld\n",
              cand[i], hr.status);
      http_response_free(&hr);
    }
  }
  if (!body) {
    fprintf(stderr, "[eibi-shortwave-schedule] no season file served\n");
    return transport_ok ? 0 : -1;   /* reachable but nothing there -> 0 (R3) */
  }

  char *p = body;
  char *head = jo_next_line_cr(&p);
  if (!head || !*head) {
    free(body);
    return -1;
  }
  char *hc[MAXCOL];
  int nh = semi_split(head, hc, MAXCOL);
  for (int i = 0; i < nh; i++) { hc[i] = trim(hc[i]); strip_width_hint(hc[i]); }
  int i_khz  = jo_col_of(hc, nh, "kHz");
  int i_time = jo_col_of(hc, nh, "Time(UTC)");
  int i_days = jo_col_of(hc, nh, "Days");
  int i_itu  = jo_col_of(hc, nh, "ITU");
  int i_stn  = jo_col_of(hc, nh, "Station");
  int i_lng  = jo_col_of(hc, nh, "Lng");
  int i_tgt  = jo_col_of(hc, nh, "Target");
  int i_rem  = jo_col_of(hc, nh, "Remarks");
  int i_p    = jo_col_of(hc, nh, "P");
  int i_st   = jo_col_of(hc, nh, "Start");
  int i_sp   = jo_col_of(hc, nh, "Stop");
  if (i_khz < 0 || i_stn < 0) {
    fprintf(stderr, "[eibi-shortwave-schedule] header shape changed; not parsing\n");
    free(body);
    return -1;
  }

  int n = 0, seen = 0;
  char *line;
  while ((line = jo_next_line_cr(&p)) != NULL) {
    if (!*line) continue;
    seen++;
    if (n >= MAX_ROWS) break;
    char *f[MAXCOL];
    int nf = semi_split(line, f, MAXCOL);
    for (int i = 0; i < nf; i++) f[i] = trim(f[i]);

    const char *khz = cell(f, nf, i_khz);
    const char *stn = cell(f, nf, i_stn);
    if (!khz || !stn) continue;                   /* no identity -> no row */
    const char *tm  = cell(f, nf, i_time);
    const char *itu = cell(f, nf, i_itu);
    const char *rem = cell(f, nf, i_rem);
    const char *tgt = cell(f, nf, i_tgt);

    cJSON *pr = cJSON_CreateObject();
    jo_add_str(pr, "frequency_khz", khz);
    {
      char *e = NULL;
      double v = strtod(khz, &e);
      if (e && e != khz) cJSON_AddNumberToObject(pr, "frequency_khz_value", v);
    }
    jo_add_str(pr, "time_utc", tm);
    jo_add_str(pr, "days", cell(f, nf, i_days));
    jo_add_str(pr, "itu_country_code", itu);
    jo_add_str(pr, "station", stn);
    jo_add_str(pr, "language_code", cell(f, nf, i_lng));
    jo_add_str(pr, "target_area", tgt);
    jo_add_str(pr, "remarks", rem);
    jo_add_str(pr, "persistence_code", cell(f, nf, i_p));
    jo_add_str(pr, "valid_start", cell(f, nf, i_st));
    jo_add_str(pr, "valid_stop", cell(f, nf, i_sp));
    cJSON_AddStringToObject(pr, "season_file", season);
    cJSON_AddStringToObject(pr, "geo_note",
      "the schedule carries no coordinates; the Remarks column holds a "
      "transmitter-site code whose site list is published separately and is "
      "not fetched here, so no location is asserted");
    cJSON_AddStringToObject(pr, "source", "EiBi shortwave schedule (eibispace.de)");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[288], summary[288], key[192];
    snprintf(key, sizeof key, "%s|%s|%s|%s", season, khz, tm ? tm : "", stn);
    snprintf(title, sizeof title, "%s kHz %.120s%s%s", khz, stn,
             tm ? " " : "", tm ? tm : "");
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             itu ? itu : "country n/a",
             tgt ? " · target " : "", tgt ? tgt : "",
             rem ? " · " : "", rem ? rem : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "http://www.eibispace.de/";
    it.lang            = "en";
    it.record_type     = "shortwave-schedule-entry";
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"shortwave\",\"hf\",\"broadcast\"]";
    /* R2: no coordinates anywhere in this file. */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[eibi-shortwave-schedule] emitted %d of %d rows (season %s)\n",
          n, seen, season);
  return 0;
}

static const source_def tsp_eibi_shortwave_schedule_def = {
  .id = "eibi-shortwave-schedule", .collector = "telecom",
  .name = "EiBi international shortwave broadcast schedule",
  .update_interval_sec = 604800, .run = run,
  .category = "telecom", .type = "dataset",
  .url = "http://www.eibispace.de/dx/",
  .description = "The reference schedule of what transmits on HF worldwide: frequency, UTC time span, days, ITU country, station, language, target area and transmitter-site code — broadcasters, military, navy, time signals, numbers stations and utilities.",
  .license = "EiBi frequency lists are explicitly free: 'any person is absolutely free to download, use, copy, or distribute' them. Attribution to Eibi Klingenfuss / eibispace.de.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_eibi_shortwave_schedule_def)
