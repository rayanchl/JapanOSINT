/* MET Norway severe weather alerts — the rare CAP-style warning feed that ships
 * usable geometry instead of region codes, covering mainland Norway, Svalbard
 * and the Norwegian sea areas.
 *
 * Endpoint (keyless):
 *   https://api.met.no/weatherapi/metalerts/2.0/current.json
 * Emits per feature: properties.event, eventAwarenessName, severity, certainty,
 *   area, riskMatrixColor, consequences/instruction where present, the
 *   when.interval validity window, and the real affected-area MultiPolygon.
 * Licence: MET Norway API is NLOD / CC BY 4.0 — free reuse with attribution.
 *   MET REQUIRES an identifying User-Agent carrying contact info on api.met.no
 *   requests, so one is set explicitly below (api.met.no 403s without it) and
 *   the contact URL must stay in it.
 *
 * GEOMETRY (from parse_notes): the geometry is a MultiPolygon — an array of
 * arrays of arrays of [lon,lat]. An emitter that assumes coordinates[0] is a
 * longitude would read a whole polygon as an ordinate. geoeo_geom() branches on
 * the declared type, takes the vertex mean of the FIRST ring of the FIRST
 * polygon as the representative pin, and stores the whole MultiPolygon as
 * geometry_geojson. Rings repeat their first vertex at the end, which the mean
 * tolerates. Zero alerts is normal in calm weather — R3 return 0.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "geoeo_common.inc"

static const char *URL =
    "https://api.met.no/weatherapi/metalerts/2.0/current.json";

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* Identifying UA with a contact URL — mandated by the MET Norway ToS. */
  const char *hdrs[] = {
    "User-Agent: JapanOSINT/1.0 (+https://github.com/ contact via repository issues)",
    "Accept: application/json", NULL
  };
  cJSON *doc = feed_get_json_h(ctx->http, URL, hdrs, 30000);
  if (!doc) {
    fprintf(stderr, "[metno-alerts] fetch/parse failed (UA required)\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  if (!cJSON_IsArray(feats)) {
    fprintf(stderr, "[metno-alerts] no features array\n");
    cJSON_Delete(doc);
    return -1;
  }

  static const char *KEYS[] = { "event", "eventAwarenessName", "severity",
                                "certainty", "urgency", "area", "county",
                                "description", "instruction", "consequences",
                                "riskMatrixColor", "awareness_level",
                                "awareness_type", "type", "geographicDomain",
                                NULL };
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    const char *ev = geoeo_str(p, "eventAwarenessName");
    if (!ev) ev = geoeo_str(p, "event");
    const char *area = geoeo_str(p, "area");
    const char *sev = geoeo_str(p, "severity");
    if (!ev && !area) continue;

    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);

    /* Validity window: feature.when.interval is an array of two timestamps. */
    const char *from = NULL, *to = NULL;
    cJSON *when = cJSON_GetObjectItem(f, "when");
    cJSON *iv = when ? cJSON_GetObjectItem(when, "interval") : NULL;
    if (cJSON_IsArray(iv)) {
      cJSON *a = cJSON_GetArrayItem(iv, 0), *b = cJSON_GetArrayItem(iv, 1);
      if (cJSON_IsString(a)) from = a->valuestring;
      if (cJSON_IsString(b)) to = b->valuestring;
    }

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, p, KEYS);
    geoeo_add_str(props, "valid_from", from);
    geoeo_add_str(props, "valid_to", to);
    if (geo) { cJSON_AddNumberToObject(props, "pin_latitude", lat);
               cJSON_AddNumberToObject(props, "pin_longitude", lon);
               cJSON_AddStringToObject(props, "geo_precision", "area-polygon"); }
    cJSON_AddStringToObject(props, "provider",
                            "Norwegian Meteorological Institute (MET Norway)");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[384];
    snprintf(title, sizeof title, "%s%s%s%s%s", sev ? sev : "",
             sev ? " — " : "", ev ? ev : "weather alert",
             area ? " · " : "", area ? area : "");

    char key[384];
    const char *ident = geoeo_str(p, "id");
    if (!ident) ident = geoeo_str(f, "id");
    if (ident) snprintf(key, sizeof key, "%s", ident);
    else snprintf(key, sizeof key, "%s|%s|%s", ev ? ev : "", area ? area : "",
                  from ? from : "");

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = geoeo_str(p, "description");
    it.body = geoeo_str(p, "consequences");
    it.lang = "no";
    it.published_at = from;
    it.record_type = "weather-warning";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"weather\",\"warning\",\"norway\",\"metno\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[metno-alerts] emitted %d alerts\n", n);
  return 0;
}

static const source_def geoeo_metno_def = {
  .id = "metno-alerts", .collector = "environment",
  .name = "MET Norway CAP Weather Alerts (GeoJSON)",
  .update_interval_sec = 900, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.met.no/weatherapi/metalerts/2.0/current.json",
  .description = "Norwegian Meteorological Institute severe weather alerts as a GeoJSON FeatureCollection with real affected-area MultiPolygons, covering Svalbard and Norwegian sea areas.",
  .license = "NLOD / CC BY 4.0 (MET Norway); identifying User-Agent with contact info required.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_metno_def)
