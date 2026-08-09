/* collectors/sources/mar_noaa_coops_stations.c
 * NOAA CO-OPS tide and water-level station registry (NWLON/PORTS).
 *
 * Endpoint: https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/
 *           stations.json?type=waterlevels
 * Emits (all fetched): station id, name, state, shefcode, tidal/greatlakes
 *   flags, affiliations, tideType, timezone/timezonecorr, portscode, and the
 *   surveyed gauge position.
 * Keyless. Licence: NOAA / US Government work, public domain.
 *
 * parse_notes — "Real coordinate is stations[].lat and stations[].lng (decimal
 * degrees, note the field is 'lng' not 'lon'). These are surveyed gauge
 * positions." Read from lat/lng exactly; a station without both numbers is
 * emitted with has_geo=0 (R2).
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define COOPS_URL "https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/" \
                  "stations.json?type=waterlevels"

static inline void add_bool(cJSON *dst, const cJSON *src, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(src, k);
  if (cJSON_IsBool(v)) cJSON_AddBoolToObject(dst, k, cJSON_IsTrue(v));
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, COOPS_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[noaa-coops-stations] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_GetObjectItem(doc, "stations");
  if (!cJSON_IsArray(arr) && cJSON_IsArray(doc)) arr = doc;
  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, arr) {
    if (!cJSON_IsObject(s)) continue;
    const char *id = jo_sv(s, "id");
    const char *name = jo_sv(s, "name");
    if (!id) continue;

    /* 'lng', not 'lon' */
    const cJSON *latj = cJSON_GetObjectItem(s, "lat");
    const cJSON *lngj = cJSON_GetObjectItem(s, "lng");
    int geo = cJSON_IsNumber(latj) && cJSON_IsNumber(lngj);
    double lat = geo ? latj->valuedouble : 0;
    double lon = geo ? lngj->valuedouble : 0;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "station_id", id);
    jo_copy_str(p, s, "name");
    jo_copy_str(p, s, "state");
    jo_copy_str(p, s, "shefcode");
    jo_copy_str(p, s, "affiliations");
    jo_copy_str(p, s, "tideType");
    jo_copy_str(p, s, "timezone");
    jo_copy_str(p, s, "portscode");
    jo_copy_str(p, s, "observedst");
    add_bool(p, s, "tidal");
    add_bool(p, s, "greatlakes");
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "surveyed-gauge-position");
    }
    cJSON_AddStringToObject(p, "source", "NOAA CO-OPS");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[200], summary[200], link[200];
    const char *st = jo_sv(s, "state");
    snprintf(title, sizeof title, "%s (%s)", name ? name : id, id);
    snprintf(summary, sizeof summary, "NOAA water-level station%s%s%s%s",
             st ? " · " : "", st ? st : "",
             jo_sv(s, "affiliations") ? " · " : "",
             jo_sv(s, "affiliations") ? jo_sv(s, "affiliations") : "");
    snprintf(link, sizeof link,
             "https://tidesandcurrents.noaa.gov/stationhome.html?id=%s", id);

    intel_item it = {0};
    it.remote_key      = id;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.record_type     = "tide-gauge-station";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"tide-gauge\",\"noaa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[noaa-coops-stations] emitted %d\n", n);
  return 0;
}

static const source_def mar_noaa_coops_stations_def = {
  .id = "noaa-coops-stations", .collector = "maritime",
  .name = "NOAA CO-OPS tide and water-level station registry",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "api", .url = COOPS_URL,
  .description = "The NWLON/PORTS station registry: every US water-level station with its id, name, coordinates and available product set — the index needed before pulling observations.",
  .license = "NOAA / US Government work, public domain",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_noaa_coops_stations_def)
