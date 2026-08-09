/* USGS NWIS instantaneous values — live gage height from every active stream
 * gauge in a state, each with its surveyed coordinate. Flood monitoring at the
 * individual-river level, keyless and per-state.
 *
 * Endpoint (keyless):
 *   https://waterservices.usgs.gov/nwis/iv/?format=json&stateCd=<st>
 *   &parameterCd=00065&siteStatus=active          (00065 = gage height, ft)
 * Emits per gauge: site name, USGS site code, the surveyed
 *   latitude/longitude, the variable name/unit and the most recent reading with
 *   its timestamp and qualifiers.
 * Licence: USGS public domain. The response carries the standard "Provisional
 *   data are subject to revision" note, which is surfaced on every row.
 *
 * parse_notes honoured:
 *  - Deeply nested WaterML-in-JSON: value → timeSeries[] → sourceInfo /
 *    variable / values[0].value[].
 *  - Coordinates live at timeSeries[i].sourceInfo.geoLocation.geogLocation
 *    .{latitude,longitude} as real numbers with srs EPSG:4326. A series without
 *    them is emitted with has_geo=0.
 *  - Measurement values are STRINGS and a MISSING reading is the sentinel
 *    "-999999" (also exposed as variable.noDataValue). Emitting that as a river
 *    level would be a lie, so those readings are dropped and the gauge is
 *    emitted without a value.
 *  - ~792 KB per state, so states are picked deliberately (a short list) rather
 *    than looping all 50.
 *  - waterwatch.usgs.gov/webservices/floodstage is DECOMMISSIONED (returns a
 *    blog page); this is the live path.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

/* Deliberately chosen states, not a 50-state sweep. */
static const char *STATES[] = { "ca", "tx", "fl", "ny", NULL };

static int collect_state(const source_ctx *ctx, intel_sink *sink,
                         const char *st, int *emitted) {
  char url[320];
  snprintf(url, sizeof url,
           "https://waterservices.usgs.gov/nwis/iv/?format=json&stateCd=%s"
           "&parameterCd=00065&siteStatus=active",
           st);
  cJSON *doc = feed_get_json(ctx->http, url, 60000);
  if (!doc) return -1;

  cJSON *val = cJSON_GetObjectItem(doc, "value");
  cJSON *series = val ? cJSON_GetObjectItem(val, "timeSeries") : NULL;
  if (!cJSON_IsArray(series)) {
    cJSON_Delete(doc);
    return -1;
  }

  int seen = 0;
  cJSON *ts;
  cJSON_ArrayForEach(ts, series) {
    seen++;
    cJSON *si = cJSON_GetObjectItem(ts, "sourceInfo");
    const char *sname = geoeo_str(si, "siteName");
    const char *scode = NULL;
    cJSON *codes = si ? cJSON_GetObjectItem(si, "siteCode") : NULL;
    if (cJSON_IsArray(codes))
      scode = geoeo_str(cJSON_GetArrayItem(codes, 0), "value");
    if (!sname && !scode) continue;

    double lat = 0, lon = 0;
    int geo = 0;
    cJSON *gl = si ? cJSON_GetObjectItem(si, "geoLocation") : NULL;
    cJSON *gg = gl ? cJSON_GetObjectItem(gl, "geogLocation") : NULL;
    if (gg)
      geo = geoeo_num(gg, "latitude", &lat) &&
            geoeo_num(gg, "longitude", &lon) && geoeo_ll_ok(lat, lon);

    cJSON *var = cJSON_GetObjectItem(ts, "variable");
    const char *vname = NULL;
    cJSON *vn = var ? cJSON_GetObjectItem(var, "variableName") : NULL;
    if (cJSON_IsString(vn)) vname = vn->valuestring;
    const char *unit = NULL;
    cJSON *uc = var ? cJSON_GetObjectItem(var, "unit") : NULL;
    if (uc) unit = geoeo_str(uc, "unitCode");

    /* noDataValue, when published, is the authoritative sentinel. */
    double nodata = -999999.0;
    cJSON *ndv = var ? cJSON_GetObjectItem(var, "noDataValue") : NULL;
    if (cJSON_IsNumber(ndv)) nodata = ndv->valuedouble;

    /* Most recent reading. */
    double reading = 0;
    int has_reading = 0;
    const char *when = NULL, *quals = NULL;
    cJSON *vals = cJSON_GetObjectItem(ts, "values");
    cJSON *v0 = cJSON_IsArray(vals) ? cJSON_GetArrayItem(vals, 0) : NULL;
    cJSON *plist = v0 ? cJSON_GetObjectItem(v0, "value") : NULL;
    if (cJSON_IsArray(plist)) {
      int pn = cJSON_GetArraySize(plist);
      for (int i = pn - 1; i >= 0 && !has_reading; i--) {
        cJSON *pv = cJSON_GetArrayItem(plist, i);
        double d = 0;
        if (!geoeo_numlax(pv, "value", &d)) continue;
        if (d == nodata) continue;             /* sentinel — never emit it */
        reading = d;
        has_reading = 1;
        when = geoeo_str(pv, "dateTime");
        cJSON *q = cJSON_GetObjectItem(pv, "qualifiers");
        if (cJSON_IsArray(q) && cJSON_IsString(cJSON_GetArrayItem(q, 0)))
          quals = cJSON_GetArrayItem(q, 0)->valuestring;
      }
    }

    cJSON *props = cJSON_CreateObject();
    geoeo_add_str(props, "site_name", sname);
    geoeo_add_str(props, "site_code", scode);
    geoeo_add_str(props, "state", st);
    geoeo_add_str(props, "variable", vname);
    geoeo_add_str(props, "unit", unit);
    geoeo_add_str(props, "qualifiers", quals);
    geoeo_add_str(props, "observed_at", when);
    if (has_reading) cJSON_AddNumberToObject(props, "value", reading);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon);
               cJSON_AddStringToObject(props, "srs", "EPSG:4326"); }
    cJSON_AddStringToObject(props, "data_note",
                            "Provisional data are subject to revision (USGS)");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    char title[320];
    if (has_reading)
      snprintf(title, sizeof title, "%s — %.2f %s",
               sname ? sname : scode, reading, unit ? unit : "");
    else
      snprintf(title, sizeof title, "%s — no current reading",
               sname ? sname : scode);

    char key[160];
    snprintf(key, sizeof key, "%s|00065", scode ? scode : sname);

    char link[160];
    if (scode)
      snprintf(link, sizeof link,
               "https://waterdata.usgs.gov/monitoring-location/%s/", scode);
    else link[0] = 0;

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = vname;
    it.link = link[0] ? link : NULL;
    it.lang = "en";
    it.published_at = when;
    it.record_type = "river-gauge";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"water\",\"river\",\"gauge\",\"flood\",\"usgs\"]";
    if (sink->emit(sink, &it) >= 0) (*emitted)++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  return seen;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0, ok = 0;
  for (int i = 0; STATES[i]; i++)
    if (collect_state(ctx, sink, STATES[i], &n) >= 0) ok++;
  if (!ok) {
    fprintf(stderr, "[usgs-water-levels] every state request failed\n");
    return -1;
  }
  fprintf(stderr, "[usgs-water-levels] emitted %d gauges across %d states\n",
          n, ok);
  return 0;
}

static const source_def geoeo_usgs_water_def = {
  .id = "usgs-water-levels", .collector = "environment",
  .name = "USGS NWIS Instantaneous River Levels",
  .update_interval_sec = 1800, .run = run,
  .category = "environment", .type = "api",
  .url = "https://waterservices.usgs.gov/nwis/iv/?format=json&stateCd=ca&parameterCd=00065&siteStatus=active",
  .description = "USGS instantaneous-values service — live gage height from active stream gauges with their surveyed coordinates. Flood monitoring at the individual-river level.",
  .license = "USGS public domain; provisional data are subject to revision.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_usgs_water_def)
