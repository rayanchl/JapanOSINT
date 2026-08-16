/* collectors/sources/tsp_igs_gnss_stations.c
 * IGS global GNSS tracking network — every permanent International GNSS
 * Service reference station, with surveyed coordinates and the hardware fitted.
 * Endpoint: https://network.igs.org/api/public/stations/?format=json (keyless)
 * Emits: 9-character station name, status, operating agencies, networks, city /
 *   state / country, antenna type + serial + radome, receiver type + firmware,
 *   tracked satellite systems, DOMES number, data centre, last data time,
 *   real-time systems and join date.
 * Geo (R2): from llh = [latitude, longitude, ellipsoidal height in metres] —
 *   parse_notes warns "note: lat FIRST", the opposite of GeoJSON order, so the
 *   indices are taken deliberately and range-checked. These are surveyed
 *   station monuments, the highest-quality coordinates in this batch.
 * parse_notes honoured: "Envelope {\"data\":[...]}"; "omit 'length' for the
 *   whole network".
 * Licence: IGS Data Policy — data and products provided free and openly,
 *   attribution to IGS expected.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IGS_URL "https://network.igs.org/api/public/stations/?format=json"

/* copy an array of strings (agencies, networks, satellite_system, ...) */
static void add_strlist(cJSON *dst, const char *key, const cJSON *src) {
  if (!cJSON_IsArray(src)) return;
  cJSON *out = cJSON_CreateArray();
  const cJSON *e;
  cJSON_ArrayForEach(e, src) {
    if (cJSON_IsString(e) && e->valuestring && e->valuestring[0])
      cJSON_AddItemToArray(out, cJSON_CreateString(e->valuestring));
    else {
      const char *nm = jo_sv(e, "name");
      if (nm) cJSON_AddItemToArray(out, cJSON_CreateString(nm));
    }
  }
  cJSON_AddItemToObject(dst, key, out);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, IGS_URL, 45000);
  if (!doc) {
    fprintf(stderr, "[igs-gnss-stations] fetch failed\n");
    return -1;
  }
  cJSON *arr = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(arr)) arr = cJSON_IsArray(doc) ? doc : NULL;
  if (!arr) {
    fprintf(stderr, "[igs-gnss-stations] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *st;
  cJSON_ArrayForEach(st, arr) {
    const char *name = jo_sv(st, "name");
    if (!name) continue;                          /* no identity -> no row */

    const char *city    = jo_sv(st, "city");
    const char *country = jo_sv(st, "country");
    const char *status  = jo_sv(st, "status");
    const char *domes   = jo_sv(st, "domes_number");
    const char *recv    = jo_sv(st, "receiver_type");
    const char *ant     = jo_sv(st, "antenna_type");

    /* llh is [lat, lon, height] — lat FIRST (not GeoJSON order) */
    double lat = 0, lon = 0, hgt = 0;
    int has_geo = 0, has_h = 0;
    cJSON *llh = cJSON_GetObjectItem(st, "llh");
    if (cJSON_IsArray(llh) && cJSON_GetArraySize(llh) >= 2) {
      cJSON *a = cJSON_GetArrayItem(llh, 0), *b = cJSON_GetArrayItem(llh, 1);
      cJSON *c = cJSON_GetArrayItem(llh, 2);
      if (cJSON_IsNumber(a) && cJSON_IsNumber(b)) {
        double la = a->valuedouble, lo = b->valuedouble;
        if (la >= -90.0 && la <= 90.0 && lo >= -180.0 && lo <= 180.0 &&
            !(la == 0.0 && lo == 0.0)) {
          lat = la; lon = lo; has_geo = 1;
        }
      }
      if (cJSON_IsNumber(c)) { hgt = c->valuedouble; has_h = 1; }
    }

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "station_name", name);
    jo_add_str(pr, "status", status);
    jo_add_str(pr, "city", city);
    jo_add_str(pr, "state", jo_sv(st, "state"));
    jo_add_str(pr, "country", country);
    jo_add_str(pr, "domes_number", domes);
    jo_add_str(pr, "antenna_type", ant);
    jo_add_str(pr, "antenna_serial_number", jo_sv(st, "antenna_serial_number"));
    jo_add_str(pr, "radome_type", jo_sv(st, "radome_type"));
    jo_add_str(pr, "receiver_type", recv);
    jo_add_str(pr, "firmware", jo_sv(st, "firmware"));
    jo_add_str(pr, "data_center", jo_sv(st, "data_center"));
    jo_add_str(pr, "last_data_time", jo_sv(st, "last_data_time"));
    jo_add_str(pr, "join_date", jo_sv(st, "join_date"));
    add_strlist(pr, "agencies", cJSON_GetObjectItem(st, "agencies"));
    add_strlist(pr, "networks", cJSON_GetObjectItem(st, "networks"));
    add_strlist(pr, "satellite_system", cJSON_GetObjectItem(st, "satellite_system"));
    add_strlist(pr, "real_time_systems", cJSON_GetObjectItem(st, "real_time_systems"));
    cJSON *xyz = cJSON_GetObjectItem(st, "xyz");
    if (cJSON_IsArray(xyz) && cJSON_GetArraySize(xyz) == 3) {
      cJSON *e = cJSON_CreateArray();
      for (int i = 0; i < 3; i++) {
        cJSON *v = cJSON_GetArrayItem(xyz, i);
        if (cJSON_IsNumber(v)) cJSON_AddItemToArray(e, cJSON_CreateNumber(v->valuedouble));
      }
      cJSON_AddItemToObject(pr, "ecef_xyz_m", e);
    }
    if (has_h) cJSON_AddNumberToObject(pr, "ellipsoidal_height_m", hgt);
    if (has_geo) cJSON_AddStringToObject(pr, "geo_precision", "surveyed-monument");
    cJSON_AddStringToObject(pr, "source", "IGS network API");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[224], summary[256], link[192];
    snprintf(title, sizeof title, "IGS %s%s%s%s%s", name,
             city ? " — " : "", city ? city : "",
             country ? ", " : "", country ? country : "");
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             status ? status : "status n/a",
             recv ? " · " : "", recv ? recv : "",
             ant ? " · " : "", ant ? ant : "");
    snprintf(link, sizeof link, "https://network.igs.org/%s", name);

    intel_item it = {0};
    it.remote_key      = name;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = jo_sv(st, "last_data_time");
    it.record_type     = "gnss-tracking-station";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"gnss\",\"geodesy\",\"infrastructure\",\"igs\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[igs-gnss-stations] emitted %d\n", n);
  return 0;
}

static const source_def tsp_igs_gnss_stations_def = {
  .id = "igs-gnss-stations", .collector = "infrastructure",
  .name = "IGS global GNSS tracking network stations",
  .update_interval_sec = 86400, .run = run,
  .category = "infrastructure", .type = "api", .url = IGS_URL,
  .description = "Every permanent IGS GNSS tracking station with surveyed coordinates, DOMES number, receiver and antenna hardware, tracked constellations, operating agency and last data timestamp.",
  .license = "IGS Data Policy — data and products provided free and openly, attribution to IGS expected.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_igs_gnss_stations_def)
