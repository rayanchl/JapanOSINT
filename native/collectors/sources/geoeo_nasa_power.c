/* NASA POWER — Prediction Of Worldwide Energy Resources. Satellite-derived
 * daily meteorology, solar irradiance and agroclimatology for any coordinate on
 * earth. The climatological baseline for judging whether an observed condition
 * is anomalous.
 *
 * Endpoint (keyless):
 *   https://power.larc.nasa.gov/api/temporal/daily/point?parameters=T2M
 *   &community=RE&longitude=<lon>&latitude=<lat>&start=YYYYMMDD&end=YYYYMMDD
 *   &format=JSON
 * Emits one row per day with a real value: the parameter, its unit, the date,
 *   the value, the source model named in header.sources, and the site
 *   coordinate the service returned.
 * Licence: NASA POWER, US Government, freely available with no restrictions;
 *   NASA asks for citation of the POWER Project.
 *
 * COORDINATE-PIVOT SERVICE: the caller supplies the point, so
 * update_interval_sec is 0 (on-demand) and ctx->entity is read as "lat,lon".
 * With no entity the collector logs one line and returns 0.
 *
 * parse_notes honoured:
 *  - The response IS a GeoJSON Feature: geometry.coordinates is
 *    [lon, lat, elevation_m] — THREE ordinates, the third being SITE ELEVATION,
 *    not an altitude of measurement. geoeo_geom() reads the first two and the
 *    elevation is carried separately as a property.
 *  - properties.parameter is an OBJECT KEYED BY PARAMETER, then by YYYYMMDD
 *    DATE STRING — not an array. Both levels are iterated as object members and
 *    the DATE IS THE KEY, not a value.
 *  - MISSING DATA IS THE SENTINEL -999.0, declared in header.fill_value.
 *    Emitting that as a temperature would be a lie, so the sentinel (read from
 *    the header when present) is dropped, not stored.
 *  - The geometry is the ECHOED QUERY point. That is honest — it is what the
 *    service returned — and it is labelled geo_precision "echoed-query".
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

static int parse_entity_ll(const char *e, double *lat, double *lon) {
  if (!e || !*e) return 0;
  char buf[128];
  snprintf(buf, sizeof buf, "%s", e);
  char *sep = strpbrk(buf, ",; \t/");
  if (!sep) return 0;
  *sep = 0;
  if (!geoeo_strtod(buf, lat)) return 0;
  if (!geoeo_strtod(sep + 1, lon)) return 0;
  return geoeo_ll_ok(*lat, *lon);
}

static void yyyymmdd(time_t t, char *out, size_t n) {
  struct tm tmv;
#if defined(_WIN32)
  gmtime_s(&tmv, &t);
#else
  gmtime_r(&t, &tmv);
#endif
  /* Components masked into range so the formatted width is provably bounded. */
  snprintf(out, n, "%04d%02d%02d", (tmv.tm_year + 1900) % 10000,
           (tmv.tm_mon + 1) % 100, tmv.tm_mday % 100);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  double qlat = 0, qlon = 0;
  if (!parse_entity_ll(ctx->entity, &qlat, &qlon)) {
    fprintf(stderr, "[nasa-power-climate] no 'lat,lon' entity supplied — "
                    "nothing to look up\n");
    return 0;
  }

  /* POWER daily lags real time; ask for a window that is already published. */
  time_t now = time(NULL);
  char start[16], end[16];
  yyyymmdd(now - 40 * 24 * 3600, start, sizeof start);
  yyyymmdd(now - 10 * 24 * 3600, end, sizeof end);

  char url[448];
  snprintf(url, sizeof url,
           "https://power.larc.nasa.gov/api/temporal/daily/point"
           "?parameters=T2M,PRECTOTCORR&community=RE&longitude=%.5f"
           "&latitude=%.5f&start=%s&end=%s&format=JSON",
           qlon, qlat, start, end);

  cJSON *doc = feed_get_json(ctx->http, url, 45000);
  if (!doc) {
    fprintf(stderr, "[nasa-power-climate] fetch/parse failed\n");
    return -1;
  }

  /* geometry.coordinates = [lon, lat, elevation_m] */
  double lat = 0, lon = 0, elev = 0;
  char *gj = NULL;
  cJSON *geom = cJSON_GetObjectItem(doc, "geometry");
  int geo = geoeo_geom(geom, &lat, &lon, &gj);
  int has_elev = 0;
  if (geom) {
    cJSON *c = cJSON_GetObjectItem(geom, "coordinates");
    cJSON *e3 = cJSON_IsArray(c) ? cJSON_GetArrayItem(c, 2) : NULL;
    if (cJSON_IsNumber(e3)) { elev = e3->valuedouble; has_elev = 1; }
  }

  cJSON *header = cJSON_GetObjectItem(doc, "header");
  double fill = -999.0;
  if (header) geoeo_num(header, "fill_value", &fill);
  const char *sources = NULL;
  cJSON *src = header ? cJSON_GetObjectItem(header, "sources") : NULL;
  if (cJSON_IsArray(src) && cJSON_IsString(cJSON_GetArrayItem(src, 0)))
    sources = cJSON_GetArrayItem(src, 0)->valuestring;
  else if (cJSON_IsString(src))
    sources = src->valuestring;

  cJSON *props_root = cJSON_GetObjectItem(doc, "properties");
  cJSON *params = props_root ? cJSON_GetObjectItem(props_root, "parameter")
                             : NULL;
  if (!cJSON_IsObject(params)) {
    fprintf(stderr, "[nasa-power-climate] no properties.parameter object\n");
    free(gj);
    cJSON_Delete(doc);
    return -1;
  }
  cJSON *units = cJSON_GetObjectItem(doc, "parameters");

  int n = 0, dropped = 0;
  cJSON *pmember;
  cJSON_ArrayForEach(pmember, params) {          /* keyed BY PARAMETER */
    const char *pname = pmember->string;
    if (!pname || !cJSON_IsObject(pmember)) continue;
    const char *unit = NULL;
    cJSON *ud = units ? cJSON_GetObjectItem(units, pname) : NULL;
    if (ud) unit = geoeo_str(ud, "units");

    cJSON *dmember;
    cJSON_ArrayForEach(dmember, pmember) {       /* then BY YYYYMMDD KEY */
      const char *daykey = dmember->string;      /* the DATE is the KEY */
      if (!daykey || !cJSON_IsNumber(dmember)) continue;
      double v = dmember->valuedouble;
      if (v == fill) { dropped++; continue; }    /* -999.0 sentinel */

      char iso[16];
      if (strlen(daykey) == 8)
        snprintf(iso, sizeof iso, "%.4s-%.2s-%.2s", daykey, daykey + 2,
                 daykey + 4);
      else
        snprintf(iso, sizeof iso, "%s", daykey);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "parameter", pname);
      cJSON_AddStringToObject(props, "date", iso);
      cJSON_AddNumberToObject(props, "value", v);
      geoeo_add_str(props, "units", unit);
      geoeo_add_str(props, "source_model", sources);
      if (has_elev) cJSON_AddNumberToObject(props, "site_elevation_m", elev);
      if (geo) {
        cJSON_AddNumberToObject(props, "latitude", lat);
        cJSON_AddNumberToObject(props, "longitude", lon);
        cJSON_AddStringToObject(props, "geo_precision", "echoed-query");
      }
      cJSON_AddStringToObject(props, "citation", "NASA POWER Project");
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      char title[256];
      snprintf(title, sizeof title, "%s %s: %.3f %s", pname, iso, v,
               unit ? unit : "");

      char key[160];
      snprintf(key, sizeof key, "%.4f,%.4f|%s|%s", geo ? lat : qlat,
               geo ? lon : qlon, pname, daykey);

      intel_item it = {0};
      it.remote_key = key;
      it.title = title;
      it.summary = sources;
      it.lang = "en";
      it.published_at = iso;
      it.record_type = "climate-observation";
      it.has_geo = geo;
      it.lat = lat;
      it.lon = lon;
      it.geometry_geojson = gj;              /* same footprint on every row */
      it.properties_json = pj ? pj : "{}";
      it.tags_json = "[\"climate\",\"weather\",\"nasa\",\"power\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(pj);
    }
  }

  free(gj);
  cJSON_Delete(doc);
  fprintf(stderr, "[nasa-power-climate] emitted %d daily values (%d fill "
                  "sentinels dropped)\n", n, dropped);
  return 0;
}

static const source_def geoeo_power_def = {
  .id = "nasa-power-climate", .collector = "environment",
  .name = "NASA POWER Climate and Solar Point API",
  .update_interval_sec = 0, .run = run,
  .category = "environment", .type = "api",
  .url = "https://power.larc.nasa.gov/api/temporal/daily/point",
  .description = "NASA Prediction Of Worldwide Energy Resources — satellite-derived daily meteorology, solar irradiance and agroclimatology for any coordinate on earth. Entity: 'lat,lon'.",
  .license = "NASA POWER, US Government, freely available with no restrictions; cite the POWER Project.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_power_def)
