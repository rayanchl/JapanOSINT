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
 *   first; use ?page=N"; we walk PAGES pages politely.
 * Licence: SatNOGS / Libre Space Foundation — CC-BY-SA, keyless.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SATNOGS_OBS_BASE "https://network.satnogs.org/api/observations/?format=json"
#define PAGES 4

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

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, fetched_pages = 0;
  for (int page = 1; page <= PAGES; page++) {
    char url[192];
    if (page == 1) snprintf(url, sizeof url, "%s", SATNOGS_OBS_BASE);
    else           snprintf(url, sizeof url, "%s&page=%d", SATNOGS_OBS_BASE, page);
    cJSON *doc = feed_get_json(ctx->http, url, 30000);
    if (!doc) break;                   /* page N>1 missing is just the end */
    if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); break; }
    fetched_pages++;
    int got = cJSON_GetArraySize(doc);
    total += emit_page(doc, sink);
    cJSON_Delete(doc);
    if (got == 0) break;
  }
  if (fetched_pages == 0) {
    fprintf(stderr, "[satnogs-network-observations] fetch failed\n");
    return -1;
  }
  fprintf(stderr, "[satnogs-network-observations] emitted %d over %d page(s)\n",
          total, fetched_pages);
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
