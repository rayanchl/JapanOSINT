/* collectors/sources/tsp_wspr_live.c
 * WSPRnet live propagation reports via wspr.live — global weak-signal HF
 * propagation telemetry: every WSPR reception report worldwide.
 * Endpoint: https://db1.wspr.live/?query=<SQL>  (keyless ClickHouse HTTP
 *   interface). The query asks only for the last WINDOW_MIN minutes and is
 *   LIMITed, as the operators' fair-use request requires.
 * Emits per reception report: time, band code, transmitter callsign and
 *   locator, receiver callsign and locator, received frequency in Hz, SNR,
 *   transmit power, drift, path distance and azimuth, plus both stations'
 *   coordinates as the table itself publishes them.
 *
 * parse_notes honoured, quoted:
 *  - "ClickHouse HTTP interface: the whole query is URL-encoded in ?query=.
 *    Always append 'FORMAT JSONEachRow' (newline-delimited JSON objects, one
 *    per line)": the SQL is %-encoded at runtime and the reply is parsed one
 *    line at a time, each line its own JSON object.
 *  - "ALWAYS include a time predicate and LIMIT — the table has billions of
 *    rows": both are in the query below and neither is optional.
 *  - "the table also exposes tx_lat/tx_lon/rx_lat/rx_lon directly, which is
 *    preferable. Either way the position is grid-square resolution — set
 *    properties.geo_precision='maidenhead-grid'": the row's geometry is the
 *    TRANSMITTER's published position, tagged as maidenhead-grid precision
 *    (R2 — real upstream values, honestly labelled as coarse); the receiver's
 *    position rides in properties.
 *  - "band is a coded integer (-1 = 136 kHz LF)": emitted as the code it is,
 *    with no invented band name.
 * Licence: wspr.live is the open ClickHouse mirror of the WSPRnet archive,
 *   free for research with no key; the operators ask that queries be reasonable
 *   and not faster than the 2-minute WSPR cycle — hence a 10-minute interval.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WSPR_HOST "https://db1.wspr.live/?query="
#define WINDOW_MIN 10
#define ROW_LIMIT 500

static const char *WSPR_SQL =
  "SELECT time,band,tx_sign,tx_loc,rx_sign,rx_loc,frequency,snr,power,drift,"
  "distance,azimuth,tx_lat,tx_lon,rx_lat,rx_lon FROM wspr.rx "
  "WHERE time > subtractMinutes(now(),10) ORDER BY time DESC LIMIT 500 "
  "FORMAT JSONEachRow";

/* RFC-3986 %-encode; malloc'd, caller frees. */
static char *urlenc(const char *s) {
  size_t n = strlen(s);
  char *out = (char *)malloc(n * 3 + 1);
  if (!out) return NULL;
  static const char hx[] = "0123456789ABCDEF";
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out[j++] = (char)c;
    else { out[j++] = '%'; out[j++] = hx[c >> 4]; out[j++] = hx[c & 15]; }
  }
  out[j] = '\0';
  return out;
}
static int numv(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (v && cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *e = NULL;
    double d = strtod(v->valuestring, &e);
    if (e && e != v->valuestring) { *out = d; return 1; }
  }
  return 0;
}
static void add_num(cJSON *dst, const cJSON *src, const char *sk, const char *dk) {
  double d;
  if (numv(src, sk, &d)) cJSON_AddNumberToObject(dst, dk, d);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *q = urlenc(WSPR_SQL);
  if (!q) return -1;
  size_t ulen = strlen(WSPR_HOST) + strlen(q) + 1;
  char *url = (char *)malloc(ulen);
  if (!url) { free(q); return -1; }
  snprintf(url, ulen, "%s%s", WSPR_HOST, q);
  free(q);

  const char *hdrs[] = { "User-Agent: JapanOSINT/1.0 (wspr.live)", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 45000, 1, &hr);
  free(url);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[wspr-live] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return -1;
  }
  char *body = hr.body;
  hr.body = NULL;
  http_response_free(&hr);

  int n = 0, lines = 0;
  char *p = body, *line;
  while ((line = jo_next_line_cr(&p)) != NULL) {
    if (!*line) continue;
    lines++;
    cJSON *r = cJSON_Parse(line);            /* one JSON object per line */
    if (!r) continue;

    const char *tm = jo_sv(r, "time");
    const char *tx = jo_sv(r, "tx_sign"), *rxs = jo_sv(r, "rx_sign");
    if (!tm || !tx || !rxs) { cJSON_Delete(r); continue; }
    const char *txloc = jo_sv(r, "tx_loc"), *rxloc = jo_sv(r, "rx_loc");

    double lat = 0, lon = 0;
    int has_geo = 0;
    if (numv(r, "tx_lat", &lat) && numv(r, "tx_lon", &lon) &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0))
      has_geo = 1;

    cJSON *pr = cJSON_CreateObject();
    jo_add_str(pr, "time", tm);
    jo_add_str(pr, "tx_sign", tx);
    jo_add_str(pr, "tx_locator", txloc);
    jo_add_str(pr, "rx_sign", rxs);
    jo_add_str(pr, "rx_locator", rxloc);
    add_num(pr, r, "band", "band_code");
    add_num(pr, r, "frequency", "frequency_hz");
    add_num(pr, r, "snr", "snr_db");
    add_num(pr, r, "power", "tx_power_dbm");
    add_num(pr, r, "drift", "drift_hz_per_min");
    add_num(pr, r, "distance", "path_distance_km");
    add_num(pr, r, "azimuth", "path_azimuth_deg");
    add_num(pr, r, "rx_lat", "rx_latitude");
    add_num(pr, r, "rx_lon", "rx_longitude");
    if (has_geo) {
      cJSON_AddNumberToObject(pr, "tx_latitude", lat);
      cJSON_AddNumberToObject(pr, "tx_longitude", lon);
      cJSON_AddStringToObject(pr, "geo_subject", "transmitting station");
      cJSON_AddStringToObject(pr, "geo_precision", "maidenhead-grid");
    }
    cJSON_AddNumberToObject(pr, "query_window_minutes", WINDOW_MIN);
    cJSON_AddNumberToObject(pr, "query_row_limit", ROW_LIMIT);
    cJSON_AddStringToObject(pr, "source", "wspr.live (WSPRnet open mirror)");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    double freq = 0, snr = 0;
    int has_f = numv(r, "frequency", &freq), has_s = numv(r, "snr", &snr);
    char title[288], summary[288], key[160];
    snprintf(key, sizeof key, "%s|%s|%s|%.0f", tm, tx, rxs, has_f ? freq : 0.0);
    if (has_f)
      snprintf(title, sizeof title, "%s heard by %s on %.6f MHz",
               tx, rxs, freq / 1e6);
    else
      snprintf(title, sizeof title, "%s heard by %s", tx, rxs);
    char snrtxt[48] = "";
    if (has_s) snprintf(snrtxt, sizeof snrtxt, " · SNR %.0f dB", snr);
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             tm, txloc ? " · " : "", txloc ? txloc : "",
             rxloc ? " -> " : "", rxloc ? rxloc : "");
    strncat(summary, snrtxt, sizeof summary - strlen(summary) - 1);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://wspr.live/";
    it.lang            = "en";
    it.published_at    = tm;
    it.record_type     = "wspr-reception-report";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"amateur-radio\",\"hf-propagation\",\"wspr\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(r);
  }

  free(body);
  fprintf(stderr, "[wspr-live] emitted %d of %d NDJSON lines (last %d min)\n",
          n, lines, WINDOW_MIN);
  return 0;                     /* a quiet band is not an error (R3) */
}

static const source_def tsp_wspr_live_def = {
  .id = "wspr-live", .collector = "telecom",
  .name = "WSPRnet live propagation reports (wspr.live)",
  .update_interval_sec = 600, .run = run,
  .category = "telecom", .type = "api", .url = "https://db1.wspr.live/",
  .description = "Global weak-signal HF propagation telemetry: every WSPR reception report in the last ten minutes with transmitter and receiver callsigns, Maidenhead locators, exact received frequency, SNR, transmit power, path distance and azimuth.",
  .license = "wspr.live — open ClickHouse mirror of the WSPRnet archive, free for research, no key; fair-use request honoured with a windowed, LIMITed query.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_wspr_live_def)
