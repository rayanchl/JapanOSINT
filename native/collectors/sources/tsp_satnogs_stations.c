/* collectors/sources/tsp_satnogs_stations.c
 * SatNOGS Network ground stations — the global open network of amateur
 * satellite receiving stations.
 * Endpoint: https://network.satnogs.org/api/stations/?format=json  (keyless)
 * Emits: station id, name, status (Online/Offline/Testing), altitude, QTH
 *   Maidenhead locator, min_horizon, observation count, success rate, client
 *   version, owner, last_seen, and every antenna's band + frequency range.
 * Geo (R2): lat + lng are the STATION's own published position — real upstream
 *   coordinates, not an address geocode. Rows without usable coordinates are
 *   emitted without geometry rather than being placed anywhere.
 * parse_notes honoured: "Top-level JSON array (not paginated envelope)";
 *   "2.8 MB response; antenna[] is a nested array".
 * Licence: SatNOGS / Libre Space Foundation — CC-BY-SA, keyless API,
 *   attribution to SatNOGS required.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SATNOGS_STATIONS_URL "https://network.satnogs.org/api/stations/?format=json"
#define MAX_ROWS 6000

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, SATNOGS_STATIONS_URL, 60000);
  if (!doc) {
    fprintf(stderr, "[satnogs-network-stations] fetch/parse failed\n");
    return -1;
  }
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[satnogs-network-stations] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *st;
  cJSON_ArrayForEach(st, doc) {
    if (n >= MAX_ROWS) break;
    double sid;
    if (!jo_num(st, "id", &sid)) continue;
    const char *name = jo_sv(st, "name");
    if (!name) continue;                         /* no title -> no row (R1) */

    const char *status = jo_sv(st, "status");
    const char *qth    = jo_sv(st, "qthlocator");

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddNumberToObject(pr, "station_id", sid);
    cJSON_AddStringToObject(pr, "name", name);
    if (status) cJSON_AddStringToObject(pr, "status", status);
    if (qth)    cJSON_AddStringToObject(pr, "qth_locator", qth);
    double d;
    if (jo_num(st, "altitude", &d))    cJSON_AddNumberToObject(pr, "altitude_m", d);
    if (jo_num(st, "min_horizon", &d)) cJSON_AddNumberToObject(pr, "min_horizon_deg", d);
    if (jo_num(st, "observations", &d))cJSON_AddNumberToObject(pr, "observations", d);
    if (jo_num(st, "success_rate", &d))cJSON_AddNumberToObject(pr, "success_rate", d);
    const char *s;
    if ((s = jo_sv(st, "client_version"))) cJSON_AddStringToObject(pr, "client_version", s);
    if ((s = jo_sv(st, "owner")))          cJSON_AddStringToObject(pr, "owner", s);
    if ((s = jo_sv(st, "last_seen")))      cJSON_AddStringToObject(pr, "last_seen", s);
    if ((s = jo_sv(st, "description")))    cJSON_AddStringToObject(pr, "description", s);

    /* antenna[] is a nested array: keep band + tuning range per antenna */
    cJSON *ants = cJSON_GetObjectItem(st, "antenna");
    cJSON *alist = cJSON_CreateArray();
    int nant = 0;
    if (cJSON_IsArray(ants)) {
      cJSON *a;
      cJSON_ArrayForEach(a, ants) {
        cJSON *ao = cJSON_CreateObject();
        const char *band = jo_sv(a, "band");
        const char *atyp = jo_sv(a, "antenna_type_name");
        if (!atyp) atyp = jo_sv(a, "antenna_type");
        if (band) cJSON_AddStringToObject(ao, "band", band);
        if (atyp) cJSON_AddStringToObject(ao, "type", atyp);
        if (jo_num(a, "frequency", &d))     cJSON_AddNumberToObject(ao, "frequency_hz", d);
        if (jo_num(a, "frequency_max", &d)) cJSON_AddNumberToObject(ao, "frequency_max_hz", d);
        cJSON_AddItemToArray(alist, ao);
        nant++;
      }
    }
    cJSON_AddItemToObject(pr, "antennas", alist);
    cJSON_AddStringToObject(pr, "source", "SatNOGS Network");

    /* R2: geometry ONLY from the station's own returned lat/lng. */
    double lat = 0, lon = 0;
    int has_geo = 0;
    if (jo_num(st, "lat", &lat) && jo_num(st, "lng", &lon) &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0))
      has_geo = 1;

    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[192], summary[256], link[128], key[32];
    snprintf(key, sizeof key, "%.0f", sid);
    snprintf(title, sizeof title, "SatNOGS station %s (#%.0f)", name, sid);
    snprintf(link, sizeof link, "https://network.satnogs.org/stations/%.0f/", sid);
    snprintf(summary, sizeof summary, "%s · %d antenna(s)%s%s",
             status ? status : "status n/a", nant,
             qth ? " · grid " : "", qth ? qth : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = jo_sv(st, "last_seen");
    it.record_type     = "satnogs-ground-station";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"space\",\"satellite\",\"ground-station\",\"satnogs\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[satnogs-network-stations] emitted %d\n", n);
  return 0;
}

static const source_def tsp_satnogs_stations_def = {
  .id = "satnogs-network-stations", .collector = "satellite",
  .name = "SatNOGS Network ground stations",
  .update_interval_sec = 21600, .run = run,
  .category = "satellite", .type = "api", .url = SATNOGS_STATIONS_URL,
  .description = "Global open network of amateur satellite ground stations: real coordinates, altitude, Maidenhead locator, per-antenna band and frequency range, online/offline status and observation counts.",
  .license = "SatNOGS / Libre Space Foundation — CC-BY-SA, keyless API, attribution required.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_satnogs_stations_def)
