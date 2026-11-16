/* collectors/sources/tsp_satnogs_observations.c
 * SatNOGS Network observations — a rolling log of which amateur ground station
 * is receiving which satellite, on what frequency, right now.
 * Endpoint: https://network.satnogs.org/api/observations/?format=json (keyless)
 * Emits: observation id, start/end, ground_station id + name, station
 *   lat/lng/alt, norad_cat_id, transmitter uuid/mode/downlink/baud, pass
 *   geometry (rise/set azimuth, max altitude), status, vetted_status and the
 *   TLE lines used for the pass.
 * Geo (R2): station_lat / station_lng are the OBSERVING STATION's real
 *   position, which is what the row is about. No coordinate is derived from
 *   the satellite (that would need propagation, which is out of scope here).
 * parse_notes honoured: "Top-level JSON array, 25 per page, newest scheduled
 *   first; use ?page=N" — no longer true: ?page is refused since at least
 *   2026-11-07 (see run()), so one page is read and the rest disclosed.
 * Licence: SatNOGS / Libre Space Foundation — CC-BY-SA, keyless.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SATNOGS_OBS_BASE "https://network.satnogs.org/api/observations/?format=json"

static int emit_page(cJSON *arr, intel_sink *sink) {
  int n = 0;
  cJSON *ob;
  cJSON_ArrayForEach(ob, arr) {
    double oid;
    if (!jo_num(ob, "id", &oid)) continue;
    const char *start = jo_sv(ob, "start");
    const char *sname = jo_sv(ob, "station_name");
    double norad = 0;
    int has_norad = jo_num(ob, "norad_cat_id", &norad);
    if (!start || !has_norad) continue;      /* incomplete record -> no row */

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddNumberToObject(pr, "observation_id", oid);
    cJSON_AddNumberToObject(pr, "norad_cat_id", norad);
    cJSON_AddStringToObject(pr, "start", start);
    const char *s;
    double d;
    if ((s = jo_sv(ob, "end")))                 cJSON_AddStringToObject(pr, "end", s);
    if (sname) cJSON_AddStringToObject(pr, "station_name", sname);
    if (jo_num(ob, "ground_station", &d))      cJSON_AddNumberToObject(pr, "ground_station_id", d);
    if (jo_num(ob, "station_alt", &d))         cJSON_AddNumberToObject(pr, "station_alt_m", d);
    if ((s = jo_sv(ob, "transmitter_uuid")))    cJSON_AddStringToObject(pr, "transmitter_uuid", s);
    if ((s = jo_sv(ob, "transmitter_mode")))    cJSON_AddStringToObject(pr, "transmitter_mode", s);
    double dl = 0;
    int has_dl = jo_num(ob, "transmitter_downlink_low", &dl);
    if (has_dl) cJSON_AddNumberToObject(pr, "transmitter_downlink_low_hz", dl);
    if (jo_num(ob, "transmitter_baud", &d))    cJSON_AddNumberToObject(pr, "transmitter_baud", d);
    if (jo_num(ob, "rise_azimuth", &d))        cJSON_AddNumberToObject(pr, "rise_azimuth_deg", d);
    if (jo_num(ob, "set_azimuth", &d))         cJSON_AddNumberToObject(pr, "set_azimuth_deg", d);
    if (jo_num(ob, "max_altitude", &d))        cJSON_AddNumberToObject(pr, "max_altitude_deg", d);
    if ((s = jo_sv(ob, "status")))              cJSON_AddStringToObject(pr, "status", s);
    if ((s = jo_sv(ob, "vetted_status")))       cJSON_AddStringToObject(pr, "vetted_status", s);
    if ((s = jo_sv(ob, "tle0")))                cJSON_AddStringToObject(pr, "tle0", s);
    if ((s = jo_sv(ob, "tle1")))                cJSON_AddStringToObject(pr, "tle1", s);
    if ((s = jo_sv(ob, "tle2")))                cJSON_AddStringToObject(pr, "tle2", s);
    cJSON_AddStringToObject(pr, "source", "SatNOGS Network");

    /* R2: the coordinate belongs to the receiving station, and only when the
     * API actually returned it. */
    double lat = 0, lon = 0;
    int has_geo = 0;
    if (jo_num(ob, "station_lat", &lat) && jo_num(ob, "station_lng", &lon) &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0)) {
      has_geo = 1;
      cJSON_AddStringToObject(pr, "geo_subject", "receiving ground station");
    }

    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[224], summary[256], link[128], key[32];
    snprintf(key, sizeof key, "%.0f", oid);
    if (has_dl)
      snprintf(title, sizeof title, "%s receiving NORAD %.0f on %.4f MHz",
               sname ? sname : "SatNOGS station", norad, dl / 1e6);
    else
      snprintf(title, sizeof title, "%s receiving NORAD %.0f",
               sname ? sname : "SatNOGS station", norad);
    snprintf(link, sizeof link, "https://network.satnogs.org/observations/%.0f/", oid);
    const char *mode = jo_sv(ob, "transmitter_mode");
    double maxalt = 0;
    if (jo_num(ob, "max_altitude", &maxalt))
      snprintf(summary, sizeof summary, "%s · %s · max elevation %.1f deg",
               start, mode ? mode : "mode n/a", maxalt);
    else
      snprintf(summary, sizeof summary, "%s · %s", start, mode ? mode : "mode n/a");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = start;
    it.record_type     = "satnogs-observation";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"space\",\"satellite\",\"sigint\",\"satnogs\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

/* A refusal served as an ARRAY: SatNOGS answers a deprecated request with
 * `[{"error":"…"}]`, which passes cJSON_IsArray and used to be read as one
 * observation-shaped page. Returns the message, or NULL for a real page. */
static const char *sat_error_array(const cJSON *doc) {
  if (!cJSON_IsArray(doc) || cJSON_GetArraySize(doc) != 1) return NULL;
  const cJSON *e = cJSON_GetArrayItem(doc, 0);  /* exhaustive-ok: a refusal is a ONE-element array (size checked above) */
  if (!cJSON_IsObject(e) || cJSON_GetArraySize(e) != 1) return NULL;
  return jo_sv(e, "error");
}

/* ONE page per run, disclosed (2026-11-07). The upstream retired `?page=N`:
 * measured that day, `?format=json&page=2` answers HTTP 400 with
 * `[{"error":"Parameter 'page' is now deprecated, please use 'Link' header
 * for getting URLs for next/previous pages."}]`, and page 1 carries
 * `Link: <…?cursor=…&format=json>; rel="next"`. So the PAGES loop below this
 * comment's predecessor read page 1, was refused on page 2 and called that
 * "the end" — three pages lost every run in silence. Following the cursor
 * needs the response's Link header, which jo_get/feed_get_json do not expose
 * (core/httpclient.h); until they do, the shortfall is a notice, not a guess. */
static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, SATNOGS_OBS_BASE, 30000);
  if (!doc) {
    fprintf(stderr, "[satnogs-network-observations] fetch failed\n");
    return -1;
  }
  const char *err = sat_error_array(doc);
  if (err) {
    fprintf(stderr, "[satnogs-network-observations] upstream refused: %s\n", err);
    cJSON_Delete(doc);
    return -1;
  }
  if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); return -1; }
  int got = cJSON_GetArraySize(doc);
  int total = emit_page(doc, sink);
  cJSON_Delete(doc);
  if (got > 0)
    jo_trunc_notice_scoped(sink, "satnogs-network-observations", "link-cursor",
                           SATNOGS_OBS_BASE, total, -1,
                           "only page 1 was read: ?page=N is refused (HTTP 400, "
                           "deprecated) and the next page is named only in the "
                           "Link response header, which this client does not expose",
                           "expose response headers from core/httpclient and follow "
                           "Link rel=next (a cursor URL)");
  fprintf(stderr, "[satnogs-network-observations] emitted %d (page 1 only)\n", total);
  return 0;
}

static const source_def tsp_satnogs_observations_def = {
  .id = "satnogs-network-observations", .collector = "satellite",
  .name = "SatNOGS Network recent/scheduled observations",
  .update_interval_sec = 1800, .run = run,
  .category = "satellite", .type = "api", .url = SATNOGS_OBS_BASE,
  .description = "Live feed of who is receiving which satellite from where: observing station coordinates, target NORAD id, downlink frequency and mode, pass geometry and the TLE used.",
  .license = "SatNOGS / Libre Space Foundation — CC-BY-SA, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_satnogs_observations_def)
