/* Environment and Climate Change Canada — two collections of the standards
 * compliant OGC API - Features service at api.weather.gc.ca.
 *
 * Endpoints (keyless, clean GeoJSON):
 *   https://api.weather.gc.ca/collections/hydrometric-realtime/items?limit=500&f=json
 *   https://api.weather.gc.ca/collections/swob-realtime/items?limit=500&f=json
 * Emits:
 *   hydrometric — STATION_NUMBER, STATION_NAME, PROV_TERR_STATE_LOC, DATETIME,
 *                 LEVEL (m), DISCHARGE (m3/s), LEVEL_SYMBOL_EN + the station
 *                 Point geometry.
 *   swob        — stn_nam-value, tc_id-value, wmo_synop_id-value, date_tm-value,
 *                 every other '<element>-value' observation the station
 *                 reported, the SWOB-ML source url, and the station Point.
 * Licence: Open Government Licence — Canada 2.0, reuse with attribution.
 *
 * parse_notes honoured:
 *  - The item id is PER-OBSERVATION ("05AB047.2026-07-02T07:00:00Z"), so the
 *    SAME station repeats with identical geometry across consecutive
 *    timestamps. Both collections are de-duplicated to the newest observation
 *    per station (STATION_NUMBER / tc_id-value); without that, hundreds of
 *    markers stack on one point.
 *  - A default unsorted request returns OLD timestamps, so a rolling datetime
 *    filter is applied to both requests.
 *  - SWOB GEOMETRY HAS THREE ORDINATES: [lon, lat, elevation_m]. Code that
 *    validates array length == 2 drops every record; geoeo_geom() reads the
 *    first two and ignores the rest.
 *  - SWOB property names use the unusual '<element>-value' / '<element>-uom'
 *    convention, so a lookup for "stn_nam" finds nothing — the "-value" suffix
 *    is spelled out at every use.
 *  - LEVEL_SYMBOL_EN/FR and DISCHARGE are frequently null and are dropped
 *    rather than stringified.
 *  - 'alerts' is NOT a valid collection id on this service (404) and is not
 *    requested.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "geoeo_common.inc"

static void iso_days_ago(int days, char *out, size_t n) {
  time_t t = time(NULL) - (time_t)days * 86400;
  struct tm tmv;
#if defined(_WIN32)
  gmtime_s(&tmv, &t);
#else
  gmtime_r(&t, &tmv);
#endif
  snprintf(out, n, "%04d-%02d-%02dT%02d:00:00Z", tmv.tm_year + 1900,
           tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour);
}

/* 1 when another feature for the same station carries a newer (or equal but
 * earlier-listed) timestamp — i.e. this one is not the current observation. */
static int superseded(cJSON *feats, int self_idx, const char *idkey,
                      const char *tkey) {
  cJSON *self = cJSON_GetArrayItem(feats, self_idx);
  cJSON *sp = cJSON_GetObjectItem(self, "properties");
  const char *sid = geoeo_str(sp, idkey);
  if (!sid) return 0;
  const char *st = geoeo_str(sp, tkey);
  int i = 0;
  cJSON *o;
  cJSON_ArrayForEach(o, feats) {
    int idx = i++;
    if (idx == self_idx) continue;
    cJSON *op = cJSON_GetObjectItem(o, "properties");
    const char *oid = geoeo_str(op, idkey);
    if (!oid || strcmp(oid, sid) != 0) continue;
    const char *ot = geoeo_str(op, tkey);
    if (!ot) continue;
    if (!st) return 1;
    int c = strcmp(ot, st);                 /* ISO-8601 sorts lexically */
    if (c > 0) return 1;
    if (c == 0 && idx < self_idx) return 1;
  }
  return 0;
}

static cJSON *ogc_fetch(const source_ctx *ctx, const char *sid,
                        const char *collection, int days) {
  char since[40];
  iso_days_ago(days, since, sizeof since);
  char url[384];
  snprintf(url, sizeof url,
           "https://api.weather.gc.ca/collections/%s/items?limit=500&f=json"
           "&datetime=%s/..",
           collection, since);
  cJSON *doc = feed_get_json(ctx->http, url, 60000);
  if (!doc) {
    fprintf(stderr, "[%s] fetch/parse failed\n", sid);
    return NULL;
  }
  if (!cJSON_IsArray(cJSON_GetObjectItem(doc, "features"))) {
    fprintf(stderr, "[%s] no features array\n", sid);
    cJSON_Delete(doc);
    return NULL;
  }
  return doc;
}

static int hydro_run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = ogc_fetch(ctx, "gc-hydrometric", "hydrometric-realtime", 2);
  if (!doc) return -1;
  cJSON *feats = cJSON_GetObjectItem(doc, "features");

  static const char *KEYS[] = { "STATION_NUMBER", "STATION_NAME",
                                "PROV_TERR_STATE_LOC", "DATETIME",
                                "LEVEL_SYMBOL_EN", "DISCHARGE_SYMBOL_EN",
                                "IDENTIFIER", NULL };
  int n = 0, idx = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    int self = idx++;
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    const char *stn = geoeo_str(p, "STATION_NUMBER");
    const char *name = geoeo_str(p, "STATION_NAME");
    if (!stn) continue;
    if (superseded(feats, self, "STATION_NUMBER", "DATETIME")) continue;

    const char *when = geoeo_str(p, "DATETIME");
    double level = 0, disch = 0;
    int has_level = geoeo_numlax(p, "LEVEL", &level);
    int has_disch = geoeo_numlax(p, "DISCHARGE", &disch);

    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, p, KEYS);
    if (has_level) cJSON_AddNumberToObject(props, "LEVEL_m", level);
    if (has_disch) cJSON_AddNumberToObject(props, "DISCHARGE_m3s", disch);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    cJSON_AddStringToObject(props, "provider",
                            "Environment and Climate Change Canada");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[320];
    if (has_level)
      snprintf(title, sizeof title, "%s (%s) — level %.3f m",
               name ? name : stn, stn, level);
    else if (has_disch)
      snprintf(title, sizeof title, "%s (%s) — discharge %.1f m3/s",
               name ? name : stn, stn, disch);
    else
      snprintf(title, sizeof title, "%s (%s)", name ? name : stn, stn);

    char summary[160];
    if (has_level && has_disch)
      snprintf(summary, sizeof summary, "discharge %.1f m3/s", disch);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key = stn;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.lang = "en";
    it.published_at = when;
    it.record_type = "hydrometric-station";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"water\",\"hydrometric\",\"canada\",\"eccc\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[gc-hydrometric] emitted %d stations (newest per station)\n",
          n);
  return 0;
}

static int swob_run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = ogc_fetch(ctx, "gc-swob-observations", "swob-realtime", 1);
  if (!doc) return -1;
  cJSON *feats = cJSON_GetObjectItem(doc, "features");

  int n = 0, idx = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    int self = idx++;
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    /* '<element>-value' naming: a lookup for "stn_nam" finds nothing. */
    const char *tcid = geoeo_str(p, "tc_id-value");
    const char *name = geoeo_str(p, "stn_nam-value");
    const char *wmo = geoeo_str(p, "wmo_synop_id-value");
    const char *when = geoeo_str(p, "date_tm-value");
    if (!tcid && !name) continue;
    if (tcid && superseded(feats, self, "tc_id-value", "date_tm-value"))
      continue;

    /* [lon, lat, elevation_m] — three ordinates, not two. */
    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_scalars(props, p);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    cJSON_AddStringToObject(props, "provider",
                            "Environment and Climate Change Canada");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[320];
    snprintf(title, sizeof title, "%s%s%s%s", name ? name : tcid,
             tcid ? " (" : "", tcid ? tcid : "", tcid ? ")" : "");

    char summary[160];
    snprintf(summary, sizeof summary, "%s%s", wmo ? "WMO " : "",
             wmo ? wmo : "");

    intel_item it = {0};
    it.remote_key = tcid ? tcid : name;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.link = geoeo_str(p, "url");
    it.lang = "en";
    it.published_at = when;
    it.record_type = "surface-weather-observation";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"weather\",\"observation\",\"canada\",\"swob\",\"eccc\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[gc-swob-observations] emitted %d stations\n", n);
  return 0;
}

static const source_def geoeo_gc_hydro_def = {
  .id = "gc-hydrometric", .collector = "environment",
  .name = "Environment Canada Hydrometric Realtime (OGC API)",
  .update_interval_sec = 1800, .run = hydro_run,
  .category = "environment", .type = "api",
  .url = "https://api.weather.gc.ca/collections/hydrometric-realtime/items?limit=500&f=json",
  .description = "ECCC's OGC API - Features endpoint for real-time water level and discharge at national hydrometric stations, returned as clean GeoJSON.",
  .license = "Open Government Licence — Canada 2.0; reuse with attribution.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_gc_hydro_def)

static const source_def geoeo_gc_swob_def = {
  .id = "gc-swob-observations", .collector = "environment",
  .name = "Environment Canada Surface Weather Observations (SWOB)",
  .update_interval_sec = 3600, .run = swob_run,
  .category = "environment", .type = "api",
  .url = "https://api.weather.gc.ca/collections/swob-realtime/items?limit=500&f=json",
  .description = "Per-minute and hourly automated surface weather observations from the Canadian station network as GeoJSON points, with the raw SWOB-ML document linked per record.",
  .license = "Open Government Licence — Canada 2.0.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_gc_swob_def)
