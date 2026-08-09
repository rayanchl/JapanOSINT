/* Smithsonian Global Volcanism Program — the two GeoServer WFS layers.
 *
 * Endpoints (keyless, outputFormat=application/json → clean GeoJSON):
 *   .../GVP-VOTW/ows?...typeName=GVP-VOTW:Smithsonian_VOTW_Holocene_Volcanoes
 *   .../GVP-VOTW/ows?...typeName=GVP-VOTW:E3WebApp_Eruptions1960
 * Emits:
 *   registry  — Volcano_Number, Volcano_Name, Primary_Volcano_Type,
 *               Last_Eruption_Year, Country, Region, Subregion,
 *               Geological_Summary + the upstream Point geometry.
 *   eruptions — VolcanoNumber, VolcanoName, ExplosivityIndexMax (VEI),
 *               StartDate/EndDate, ContinuingEruption + the upstream geometry.
 * Licence: Smithsonian Global Volcanism Program (Volcanoes of the World);
 *          free reuse with citation of GVP. These are the WFS/GeoServer
 *          products, not the weekly RSS already wired as gvp-volcano-activity.
 *
 * parse_notes honoured:
 *  - The two layers use DIFFERENT field-naming conventions (Volcano_Name with
 *    underscores vs VolcanoName without), so they get two parsers; a shared one
 *    would silently emit nothing.
 *  - A handful of GVP records have no confirmed position, so the geometry
 *    object can be null — geoeo_geom() returns 0 and leaves the geometry
 *    column NULL rather than the string "null".
 *  - Last_Eruption_Year and ExplosivityIndexMax are null for many records;
 *    geoeo_copy() drops nulls instead of stringifying them.
 *  - ContinuingEruption is the STRING "False"/"True", not a JSON boolean — a
 *    truthiness test on the string would mark every eruption as ongoing.
 *  - maxFeatures is set on both requests; without it the whole ~1,350-record
 *    table is pulled every run.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"
#include <strings.h>

static const char *REG_URL =
    "https://webservices.volcano.si.edu/geoserver/GVP-VOTW/ows?service=WFS"
    "&version=1.0.0&request=GetFeature"
    "&typeName=GVP-VOTW:Smithsonian_VOTW_Holocene_Volcanoes"
    "&outputFormat=application/json&maxFeatures=2000";

static const char *ERU_URL =
    "https://webservices.volcano.si.edu/geoserver/GVP-VOTW/ows?service=WFS"
    "&version=1.0.0&request=GetFeature"
    "&typeName=GVP-VOTW:E3WebApp_Eruptions1960"
    "&outputFormat=application/json&maxFeatures=200&sortBy=StartDate+D";

/* "20251229" → "2025-12-29". Returns 1 on a well-formed 8-digit date. */
static int yyyymmdd_iso(const char *s, char *out, size_t n) {
  if (!s || strlen(s) != 8) return 0;
  for (int i = 0; i < 8; i++)
    if (!isdigit((unsigned char)s[i])) return 0;
  snprintf(out, n, "%.4s-%.2s-%.2s", s, s + 2, s + 4);
  return 1;
}

static cJSON *gvp_fetch(const source_ctx *ctx, const char *sid,
                        const char *url) {
  cJSON *doc = feed_get_json(ctx->http, url, 40000);
  if (!doc) {
    fprintf(stderr, "[%s] fetch/parse failed\n", sid);
    return NULL;
  }
  if (!cJSON_IsArray(cJSON_GetObjectItem(doc, "features"))) {
    /* A WFS ServiceExceptionReport comes back with HTTP 200 — a silent
     * failure that must not be reported as an honest empty. */
    fprintf(stderr, "[%s] no features array (WFS exception?)\n", sid);
    cJSON_Delete(doc);
    return NULL;
  }
  return doc;
}

static int registry_run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = gvp_fetch(ctx, "gvp-holocene-volcanoes", REG_URL);
  if (!doc) return -1;

  static const char *KEYS[] = { "Volcano_Number", "Volcano_Name",
                                "Primary_Volcano_Type", "Last_Eruption_Year",
                                "Country", "Region", "Subregion",
                                "Geological_Summary", "Tectonic_Setting",
                                "Elevation", NULL };
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, cJSON_GetObjectItem(doc, "features")) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    const char *name = geoeo_str(p, "Volcano_Name");
    double vnum = 0;
    int has_vnum = geoeo_numlax(p, "Volcano_Number", &vnum);
    if (!name && !has_vnum) continue;

    char key[64];
    if (has_vnum) snprintf(key, sizeof key, "%lld", (long long)vnum);
    else snprintf(key, sizeof key, "%s", name);

    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);

    const char *ctry = geoeo_str(p, "Country");
    const char *vtype = geoeo_str(p, "Primary_Volcano_Type");

    char title[320];
    snprintf(title, sizeof title, "%s%s%s", name ? name : key,
             ctry ? " — " : "", ctry ? ctry : "");

    char summary[256];
    double lastyr = 0;
    int has_last = geoeo_numlax(p, "Last_Eruption_Year", &lastyr);
    if (has_last)
      snprintf(summary, sizeof summary, "%s · last eruption %d",
               vtype ? vtype : "volcano", (int)lastyr);
    else
      snprintf(summary, sizeof summary, "%s · no dated eruption",
               vtype ? vtype : "volcano");

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, p, KEYS);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    cJSON_AddStringToObject(props, "catalog",
                            "Smithsonian GVP Volcanoes of the World");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char link[128];
    if (has_vnum)
      snprintf(link, sizeof link,
               "https://volcano.si.edu/volcano.cfm?vn=%lld", (long long)vnum);
    else link[0] = 0;

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = summary;
    it.body = geoeo_str(p, "Geological_Summary");
    it.link = link[0] ? link : NULL;
    it.lang = "en";
    it.record_type = "volcano";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"volcano\",\"registry\",\"gvp\",\"smithsonian\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[gvp-holocene-volcanoes] emitted %d\n", n);
  return 0;
}

static int eruptions_run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = gvp_fetch(ctx, "gvp-recent-eruptions", ERU_URL);
  if (!doc) return -1;

  static const char *KEYS[] = { "VolcanoNumber", "VolcanoName", "Activity_ID",
                                "ExplosivityIndexMax", "StartDate", "EndDate",
                                "ContinuingEruption", "ActivityType", NULL };
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, cJSON_GetObjectItem(doc, "features")) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    const char *name = geoeo_str(p, "VolcanoName");
    const char *start = geoeo_str(p, "StartDate");
    double actid = 0;
    int has_act = geoeo_numlax(p, "Activity_ID", &actid);
    double vnum = 0;
    int has_vnum = geoeo_numlax(p, "VolcanoNumber", &vnum);
    if (!name && !has_act) continue;

    char key[96];
    if (has_act) snprintf(key, sizeof key, "act-%lld", (long long)actid);
    else snprintf(key, sizeof key, "%s|%s", name, start ? start : "");

    /* Coordinates are duplicated in geometry.coordinates and
     * LatitudeDecimal/LongitudeDecimal — prefer the geometry, fall back to the
     * decimal properties, and only if BOTH are absent emit has_geo=0. */
    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(cJSON_GetObjectItem(f, "geometry"), &lat, &lon, &gj);
    if (!geo) {
      double dl = 0, dn = 0;
      if (geoeo_numlax(p, "LatitudeDecimal", &dl) &&
          geoeo_numlax(p, "LongitudeDecimal", &dn) && geoeo_ll_ok(dl, dn)) {
        lat = dl; lon = dn; geo = 1;
        cJSON *g = geoeo_mk_point(lon, lat);
        gj = cJSON_PrintUnformatted(g);
        cJSON_Delete(g);
      }
    }

    /* ContinuingEruption is the STRING "True"/"False". */
    const char *cont = geoeo_str(p, "ContinuingEruption");
    int ongoing = cont && strcasecmp(cont, "true") == 0;

    double vei = 0;
    int has_vei = geoeo_numlax(p, "ExplosivityIndexMax", &vei);

    char iso[16];
    int has_iso = yyyymmdd_iso(start, iso, sizeof iso);

    char title[320];
    snprintf(title, sizeof title, "Eruption — %s%s", name ? name : key,
             ongoing ? " (continuing)" : "");

    char summary[256];
    int w = 0;
    summary[0] = 0;
    if (has_vei)
      w += snprintf(summary + w, sizeof summary - (size_t)w, "VEI %d",
                    (int)vei);
    if (start && w < (int)sizeof summary)
      w += snprintf(summary + w, sizeof summary - (size_t)w, "%sstart %s",
                    w ? " · " : "", has_iso ? iso : start);
    if (ongoing && w < (int)sizeof summary)
      snprintf(summary + w, sizeof summary - (size_t)w, "%songoing",
               w ? " · " : "");

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, p, KEYS);
    cJSON_AddBoolToObject(props, "continuing", ongoing);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    cJSON_AddStringToObject(props, "catalog", "Smithsonian GVP Eruptions 1960+");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char link[128];
    if (has_vnum)
      snprintf(link, sizeof link,
               "https://volcano.si.edu/volcano.cfm?vn=%lld", (long long)vnum);
    else link[0] = 0;

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.link = link[0] ? link : NULL;
    it.lang = "en";
    it.published_at = has_iso ? iso : NULL;
    it.record_type = "volcanic-eruption";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"volcano\",\"eruption\",\"gvp\",\"smithsonian\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[gvp-recent-eruptions] emitted %d\n", n);
  return 0;
}

static const source_def geoeo_gvp_registry_def = {
  .id = "gvp-holocene-volcanoes", .collector = "geospatial",
  .name = "Smithsonian GVP Holocene Volcano Registry (WFS)",
  .update_interval_sec = 604800, .run = registry_run,
  .category = "geospatial", .type = "dataset",
  .url = "https://webservices.volcano.si.edu/geoserver/GVP-VOTW/ows",
  .description = "The authoritative global gazetteer of the ~1,350 volcanoes active in the Holocene, with coordinates, type, country and last known eruption year.",
  .license = "Smithsonian Global Volcanism Program; free reuse with citation of GVP (Volcanoes of the World).",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_gvp_registry_def)

static const source_def geoeo_gvp_eruptions_def = {
  .id = "gvp-recent-eruptions", .collector = "environment",
  .name = "Smithsonian GVP Eruptions Since 1960 (WFS)",
  .update_interval_sec = 86400, .run = eruptions_run,
  .category = "environment", .type = "dataset",
  .url = "https://webservices.volcano.si.edu/geoserver/GVP-VOTW/ows",
  .description = "Dated eruption events worldwide since 1960 with VEI, start/end dates and an ongoing flag — the only global source that says which eruptions are still in progress.",
  .license = "Smithsonian Global Volcanism Program; citation required, reuse permitted.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_gvp_eruptions_def)
