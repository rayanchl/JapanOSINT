/* NOAA CO-OPS — live water level at US tide stations, the observational feed
 * behind storm-surge and coastal-flood assessment.
 *
 * Endpoints (keyless, used together):
 *   catalogue  https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/
 *              stations.json?type=waterlevels          (station list + lat/lon)
 *   values     https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?
 *              date=latest&station=<id>&product=water_level&datum=MLLW&
 *              units=metric&time_zone=gmt&format=json  (ONE station per call)
 * Emits per station: station id and name, the surveyed lat/lon, the latest
 *   water level with its timestamp, sigma, flags and quality code, and the
 *   datum/units the value is expressed in.
 * Licence: NOAA public domain.
 *
 * parse_notes honoured:
 *  - datagetter serves ONE station at a time, so the catalogue is fetched once
 *    and a bounded slice of stations is polled per run (COOPS_MAX_STATIONS);
 *    hammering all 301 every 30 minutes would be abusive.
 *  - metadata.lat / metadata.lon come back as STRINGS in the datagetter
 *    response but as NUMBERS in the station catalogue — both are read through a
 *    number-or-numeric-string parse.
 *  - An error is returned as {"error":{"message":...}} WITH HTTP 200, so the
 *    error key is checked before the data key on every response.
 *  - data[].v is a string; q='p' means preliminary and is carried so the
 *    analyst sees the provisional flag.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

#define COOPS_CATALOG                                                         \
  "https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations.json"      \
  "?type=waterlevels"
#define COOPS_MAX_STATIONS 25

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *cat = feed_get_json(ctx->http, COOPS_CATALOG, 40000);
  if (!cat) {
    fprintf(stderr, "[coops-water-levels] station catalogue fetch failed\n");
    return -1;
  }
  if (cJSON_GetObjectItem(cat, "error")) {
    fprintf(stderr, "[coops-water-levels] catalogue returned an error body\n");
    cJSON_Delete(cat);
    return -1;
  }
  cJSON *stations = cJSON_GetObjectItem(cat, "stations");
  if (!cJSON_IsArray(stations)) {
    fprintf(stderr, "[coops-water-levels] no stations array\n");
    cJSON_Delete(cat);
    return -1;
  }

  int n = 0, polled = 0;
  cJSON *st;
  cJSON_ArrayForEach(st, stations) {
    if (polled >= COOPS_MAX_STATIONS) break;
    const char *sid = geoeo_str(st, "id");
    const char *sname = geoeo_str(st, "name");
    if (!sid) continue;

    /* Catalogue coordinates (numbers); 'lng' is the mdapi spelling. */
    double lat = 0, lon = 0;
    int geo = geoeo_numlax(st, "lat", &lat) &&
              (geoeo_numlax(st, "lng", &lon) || geoeo_numlax(st, "lon", &lon)) &&
              geoeo_ll_ok(lat, lon);

    char url[448];
    snprintf(url, sizeof url,
             "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?"
             "date=latest&station=%s&product=water_level&datum=MLLW"
             "&units=metric&time_zone=gmt&format=json",
             sid);
    cJSON *doc = feed_get_json(ctx->http, url, 25000);
    polled++;
    if (!doc) continue;
    if (cJSON_GetObjectItem(doc, "error")) {  /* HTTP 200 + error body */
      cJSON_Delete(doc);
      continue;
    }

    cJSON *meta = cJSON_GetObjectItem(doc, "metadata");
    if (!geo && meta) {                        /* metadata lat/lon are STRINGS */
      geo = geoeo_numlax(meta, "lat", &lat) && geoeo_numlax(meta, "lon", &lon) &&
            geoeo_ll_ok(lat, lon);
    }
    if (!sname && meta) sname = geoeo_str(meta, "name");

    cJSON *data = cJSON_GetObjectItem(doc, "data");
    cJSON *d0 = cJSON_IsArray(data) ? cJSON_GetArrayItem(data, 0) : NULL;
    double v = 0, sigma = 0;
    int has_v = d0 ? geoeo_numlax(d0, "v", &v) : 0;
    int has_s = d0 ? geoeo_numlax(d0, "s", &sigma) : 0;
    const char *when = d0 ? geoeo_str(d0, "t") : NULL;
    const char *qual = d0 ? geoeo_str(d0, "q") : NULL;
    const char *flags = d0 ? geoeo_str(d0, "f") : NULL;
    if (!has_v) { cJSON_Delete(doc); continue; }   /* no reading → no row */

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "station_id", sid);
    geoeo_add_str(props, "station_name", sname);
    cJSON_AddNumberToObject(props, "water_level", v);
    cJSON_AddStringToObject(props, "datum", "MLLW");
    cJSON_AddStringToObject(props, "units", "metric (m)");
    if (has_s) cJSON_AddNumberToObject(props, "sigma", sigma);
    geoeo_add_str(props, "observed_at", when);
    geoeo_add_str(props, "quality", qual);
    geoeo_add_str(props, "flags", flags);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    char title[288];
    snprintf(title, sizeof title, "%s (%s) — %.3f m MLLW",
             sname ? sname : sid, sid, v);

    char summary[160];
    snprintf(summary, sizeof summary, "%s%s%s", when ? when : "",
             qual ? " · quality " : "", qual ? qual : "");

    char link[192];
    snprintf(link, sizeof link,
             "https://tidesandcurrents.noaa.gov/stationhome.html?id=%s", sid);

    char key[128];
    snprintf(key, sizeof key, "%s|%s", sid, when ? when : "");

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = summary;
    it.link = link;
    it.lang = "en";
    it.published_at = when;
    it.record_type = "tide-station-reading";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"tides\",\"water-level\",\"coastal\",\"coops\",\"noaa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
    cJSON_Delete(doc);
  }

  cJSON_Delete(cat);
  fprintf(stderr, "[coops-water-levels] emitted %d readings from %d stations "
                  "polled\n", n, polled);
  return 0;
}

static const source_def geoeo_coops_def = {
  .id = "coops-water-levels", .collector = "environment",
  .name = "NOAA CO-OPS Tides and Water Levels",
  .update_interval_sec = 1800, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter",
  .description = "NOAA Center for Operational Oceanographic Products live water level for US tide stations — the observational feed behind storm-surge and coastal-flood assessment.",
  .license = "NOAA public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_coops_def)
