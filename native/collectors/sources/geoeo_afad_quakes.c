/* AFAD (Turkish Disaster and Emergency Management Presidency) seismic catalogue.
 *
 * Endpoint (keyless):
 *   https://deprem.afad.gov.tr/apiv2/event/filter?start=<ts>&end=<ts>&minmag=3&limit=100
 * Emits: eventID, date, latitude, longitude, depth, magnitude + type, location,
 *        country, province, district, rms — every value from the response.
 * Licence: AFAD public open API (deprem.afad.gov.tr/apiv2). No credential, no
 *          stated prohibition on reuse.
 *
 * parse_notes honoured:
 *  - latitude/longitude/depth/magnitude are JSON STRINGS ("34.92383"), so
 *    cJSON_IsNumber() rejects every one of them — geoeo_numlax() accepts a
 *    number or a fully-parsing numeric string and rejects anything else.
 *  - start/end are MANDATORY and must be "YYYY-MM-DD HH:MM:SS" url-encoded
 *    (%20); omitting them returns an error. They are built here from the
 *    current UTC time (a rolling 7-day window) rather than hardcoded.
 *  - 'neighborhood' and 'lastUpdateDate' are frequently JSON null, so every
 *    optional field is copied with geoeo_copy(), which drops nulls instead of
 *    stringifying them.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

static void utc_stamp(time_t t, char *out, size_t n) {
  struct tm tmv;
#if defined(_WIN32)
  gmtime_s(&tmv, &t);
#else
  gmtime_r(&t, &tmv);
#endif
  snprintf(out, n, "%04d-%02d-%02d%%20%02d:%02d:%02d", tmv.tm_year + 1900,
           tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL);
  char s_start[40], s_end[40];
  utc_stamp(now - 7 * 24 * 3600, s_start, sizeof s_start);
  utc_stamp(now, s_end, sizeof s_end);

  char url[512];
  snprintf(url, sizeof url,
           "https://deprem.afad.gov.tr/apiv2/event/filter?start=%s&end=%s"
           "&minmag=3&limit=100",
           s_start, s_end);

  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) {
    fprintf(stderr, "[afad-quakes] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "result");
  if (!cJSON_IsArray(arr)) {
    fprintf(stderr, "[afad-quakes] response is not an event array\n");
    cJSON_Delete(doc);
    return -1;
  }

  static const char *TEXT_KEYS[] = { "eventID", "date", "type", "location",
                                     "country", "province", "district",
                                     "neighborhood", "rms", "lastUpdateDate",
                                     NULL };
  int n = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    const char *eid = geoeo_str(e, "eventID");
    if (!eid) continue;
    const char *when = geoeo_str(e, "date");
    const char *loc = geoeo_str(e, "location");
    const char *mtyp = geoeo_str(e, "type");

    double lat = 0, lon = 0, dep = 0, mag = 0;
    int has_lat = geoeo_numlax(e, "latitude", &lat);
    int has_lon = geoeo_numlax(e, "longitude", &lon);
    int geo = has_lat && has_lon && geoeo_ll_ok(lat, lon);
    int has_dep = geoeo_numlax(e, "depth", &dep);
    int has_mag = geoeo_numlax(e, "magnitude", &mag);

    char title[448];
    if (has_mag)
      snprintf(title, sizeof title, "%s%s%.1f — %s", mtyp ? mtyp : "M",
               mtyp ? " " : "", mag, loc ? loc : "Turkey region");
    else
      snprintf(title, sizeof title, "AFAD event %s — %s", eid,
               loc ? loc : "Turkey region");

    char summary[192];
    if (has_dep)
      snprintf(summary, sizeof summary, "depth %.2f km", dep);
    else
      summary[0] = 0;

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, e, TEXT_KEYS);
    if (has_mag) cJSON_AddNumberToObject(props, "magnitude", mag);
    if (has_dep) cJSON_AddNumberToObject(props, "depth_km", dep);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    cJSON_AddStringToObject(props, "agency", "AFAD");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    intel_item it = {0};
    it.remote_key = eid;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.lang = "tr";
    it.published_at = when;
    it.record_type = "earthquake";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"earthquake\",\"seismic\",\"turkey\",\"afad\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[afad-quakes] emitted %d\n", n);
  return 0;
}

static const source_def geoeo_afad_def = {
  .id = "afad-quakes", .collector = "environment",
  .name = "AFAD Turkey Earthquake Catalogue",
  .update_interval_sec = 900, .run = run,
  .category = "environment", .type = "api",
  .url = "https://deprem.afad.gov.tr/apiv2/event/filter",
  .description = "Turkish Disaster and Emergency Management Presidency seismic catalogue covering Turkey, the Aegean, Caucasus and northern Iraq/Iran with dense local coverage down to M3.",
  .license = "AFAD public open API; no credential, no stated prohibition on reuse.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_afad_def)
