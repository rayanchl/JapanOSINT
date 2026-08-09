/* collectors/sources/tsp_kiwisdr_receivers.c
 * KiwiSDR public receiver network — every publicly listed KiwiSDR software
 * defined receiver worldwide: a live map of distributed HF listening capability
 * and, incidentally, an inventory of internet-exposed SDR hardware and its
 * firmware level.
 * Endpoint: http://rx.linkfanel.net/kiwisdr_com.js   (keyless, ~900 KB)
 * Emits: receiver id, name, status, offline flag, SDR hardware, tuning range
 *   (bands, Hz), frequency offset, current and maximum users, Maidenhead grid,
 *   GPS-lock flag, altitude, location text, antenna description, measured band
 *   SNR, software version, uptime, updated and date.
 *
 * TRAPS handled, quoted from the manifest:
 *  - "NOT pure JSON: the body is JavaScript, 'var kiwisdr_com =' followed by a
 *    JSON array and a trailing ';'. Skip to the first '[' and parse to the
 *    matching ']'": we seek to the first '[' and use cJSON_ParseWithOpts with
 *    require_null_terminated=0, which stops at the end of the array and
 *    ignores the trailing ';'.
 *  - "Every value is a STRING."
 *  - "Geo is the 'gps' field formatted as '(lat, lon)' — strip the parentheses
 *    and split on ','": parsed with sscanf below. R2-safe: this is the
 *    receiver's own published position. "gps_good='0' means the position is
 *    the owner-declared one rather than GPS-locked; both are real upstream
 *    values" — the flag is carried in properties as geo_precision so the
 *    difference is visible.
 *  - "Served over plain HTTP only" — hence the http:// URL.
 * Licence: public receiver list auto-generated from kiwisdr.com/public for the
 *   open 'dyatlov' map maker; served without restriction or key. Receivers are
 *   voluntarily published by their owners.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KIWI_URL "http://rx.linkfanel.net/kiwisdr_com.js"

static void add_num_str(cJSON *o, const char *k, const char *v) {
  if (!v) return;
  char *e = NULL;
  double d = strtod(v, &e);
  if (e && e != v && !*e) cJSON_AddNumberToObject(o, k, d);
  else cJSON_AddStringToObject(o, k, v);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "User-Agent: JapanOSINT/1.0 (kiwisdr)", NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", KIWI_URL, hdrs, NULL, 0, 45000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[kiwisdr-receivers] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return -1;
  }
  char *body = hr.body;
  hr.body = NULL;
  http_response_free(&hr);

  const char *arr = strchr(body, '[');
  if (!arr) {
    fprintf(stderr, "[kiwisdr-receivers] no JSON array in the JS body\n");
    free(body);
    return -1;
  }
  /* require_null_terminated = 0 -> stops at the end of the array, so the
   * trailing ';' after the JS assignment does not break the parse */
  cJSON *doc = cJSON_ParseWithOpts(arr, NULL, 0);
  free(body);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[kiwisdr-receivers] array parse failed\n");
    if (doc) cJSON_Delete(doc);
    return -1;
  }

  int n = 0, nogeo = 0;
  cJSON *rx;
  cJSON_ArrayForEach(rx, doc) {
    const char *id   = jo_sv(rx, "id");
    const char *name = jo_sv(rx, "name");
    if (!id && !name) continue;                   /* no identity -> no row */

    const char *status = jo_sv(rx, "status");
    const char *grid   = jo_sv(rx, "grid");
    const char *gps    = jo_sv(rx, "gps");
    const char *good   = jo_sv(rx, "gps_good");
    const char *ant    = jo_sv(rx, "antenna");
    const char *bands  = jo_sv(rx, "bands");
    const char *users  = jo_sv(rx, "users");
    const char *umax   = jo_sv(rx, "users_max");

    /* gps is "(lat, lon)" */
    double lat = 0, lon = 0;
    int has_geo = 0;
    if (gps && sscanf(gps, " ( %lf , %lf )", &lat, &lon) == 2 &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0))
      has_geo = 1;
    else
      nogeo++;

    cJSON *pr = cJSON_CreateObject();
    jo_add_str(pr, "receiver_id", id);
    jo_add_str(pr, "name", name);
    jo_add_str(pr, "status", status);
    jo_add_str(pr, "offline", jo_sv(rx, "offline"));
    jo_add_str(pr, "sdr_hardware", jo_sv(rx, "sdr_hw"));
    jo_add_str(pr, "tuning_range_hz", bands);
    jo_add_str(pr, "frequency_offset", jo_sv(rx, "freq_offset"));
    add_num_str(pr, "users", users);
    add_num_str(pr, "users_max", umax);
    jo_add_str(pr, "maidenhead_grid", grid);
    jo_add_str(pr, "gps_raw", gps);
    jo_add_str(pr, "gps_good", good);
    add_num_str(pr, "altitude_asl_m", jo_sv(rx, "asl"));
    jo_add_str(pr, "location_text", jo_sv(rx, "loc"));
    jo_add_str(pr, "antenna", ant);
    jo_add_str(pr, "band_snr", jo_sv(rx, "snr"));
    jo_add_str(pr, "software_version", jo_sv(rx, "sw_version"));
    jo_add_str(pr, "uptime", jo_sv(rx, "uptime"));
    jo_add_str(pr, "updated", jo_sv(rx, "updated"));
    jo_add_str(pr, "date", jo_sv(rx, "date"));
    if (has_geo)
      cJSON_AddStringToObject(pr, "geo_precision",
        (good && good[0] == '1') ? "gps-locked receiver position"
                                 : "owner-declared receiver position");
    cJSON_AddStringToObject(pr, "source", "kiwisdr.com public receiver list (rx.linkfanel.net)");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[288], summary[288], key[96];
    snprintf(key, sizeof key, "%s", id ? id : name);
    snprintf(title, sizeof title, "KiwiSDR %s%s%s", name ? name : id,
             grid ? " · grid " : "", grid ? grid : "");
    snprintf(summary, sizeof summary, "%s%s%s%s%s%s%s",
             status ? status : "status n/a",
             (users && umax) ? " · " : "", users ? users : "",
             (users && umax) ? "/" : "", umax ? umax : "",
             ant ? " · " : "", ant ? ant : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.lang            = "en";
    it.record_type     = "sdr-receiver";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"sdr\",\"hf\",\"kiwisdr\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[kiwisdr-receivers] emitted %d (%d without a usable gps field)\n",
          n, nogeo);
  return 0;
}

static const source_def tsp_kiwisdr_receivers_def = {
  .id = "kiwisdr-receivers", .collector = "telecom",
  .name = "KiwiSDR public receiver network",
  .update_interval_sec = 3600, .run = run,
  .category = "telecom", .type = "dataset", .url = KIWI_URL,
  .description = "Every publicly listed KiwiSDR receiver worldwide: site coordinates, Maidenhead grid, tuning range, antenna description, measured band noise floor, firmware version and user counts.",
  .license = "Public receiver list auto-generated from kiwisdr.com/public for the open dyatlov map maker; served without restriction or key. Receivers are voluntarily published by their owners.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_kiwisdr_receivers_def)
