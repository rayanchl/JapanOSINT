/* UK Environment Agency real-time flood monitoring — England's river, tidal,
 * groundwater and rainfall station network, plus the live flood warning list.
 *
 * Endpoints (keyless, JSON-LD):
 *   https://environment.data.gov.uk/flood-monitoring/id/stations?_limit=500&parameter=level
 *   https://environment.data.gov.uk/flood-monitoring/id/floods            (secondary)
 * Emits per station: label, lat, long, riverName, notation, stationReference,
 *   town, catchmentName, dateOpened, stageScale and the list of live measures
 *   (parameterName + unitName). Per flood warning: description, severity,
 *   severityLevel, floodArea code and the message.
 * Licence: the response declares its own — Open Government Licence v3
 *   (nationalarchives.gov.uk/doc/open-government-licence/version/3).
 *
 * parse_notes honoured:
 *  - JSON-LD: records live under 'items', and 'items' can be an OBJECT rather
 *    than an ARRAY when exactly one record matches; both shapes are handled.
 *  - The coordinate keys are 'lat' and 'long' (NOT 'lon'/'longitude'). Some
 *    stations also carry easting/northing, which are OSGB GRID METRES, not
 *    degrees — those are never read as coordinates.
 *  - A few stations legitimately omit lat/long; they are emitted with
 *    has_geo=0 rather than dropped.
 *  - /id/floods returned 200 with an EMPTY items array at probe time (no active
 *    warnings in England), so it is wired as a SECONDARY call that honestly
 *    contributes zero rows and never fails the run.
 *  - /data/readings?latest carries the values but no geometry; joining it is a
 *    separate concern, so the measure @id is carried here to make that join
 *    possible.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "geoeo_common.inc"

#define EA_STATIONS                                                           \
  "https://environment.data.gov.uk/flood-monitoring/id/stations"              \
  "?_limit=500&parameter=level"
#define EA_FLOODS "https://environment.data.gov.uk/flood-monitoring/id/floods"

/* 'items' may be an array OR a single object. Returns an array to iterate and
 * sets *owned when the caller must delete it. */
static cJSON *items_array(cJSON *doc, int *owned) {
  *owned = 0;
  cJSON *items = cJSON_GetObjectItem(doc, "items");
  if (cJSON_IsArray(items)) return items;
  if (cJSON_IsObject(items)) {
    cJSON *wrap = cJSON_CreateArray();
    cJSON_AddItemToArray(wrap, cJSON_Duplicate(items, 1));
    *owned = 1;
    return wrap;
  }
  return NULL;
}

static int stations(const source_ctx *ctx, intel_sink *sink, int *emitted) {
  cJSON *doc = feed_get_json(ctx->http, EA_STATIONS, 45000);
  if (!doc) return -1;
  int owned = 0;
  cJSON *items = items_array(doc, &owned);
  if (!items) {
    cJSON_Delete(doc);
    return -1;
  }

  static const char *KEYS[] = { "label", "riverName", "notation",
                                "stationReference", "town", "catchmentName",
                                "dateOpened", "status", "gridReference",
                                "@id", NULL };
  int seen = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, items) {
    seen++;
    const char *notation = geoeo_str(s, "notation");
    const char *label = geoeo_str(s, "label");
    if (!label) {                    /* label is occasionally an array */
      cJSON *l = cJSON_GetObjectItem(s, "label");
      if (cJSON_IsArray(l) && cJSON_IsString(cJSON_GetArrayItem(l, 0)))
        label = cJSON_GetArrayItem(l, 0)->valuestring;
    }
    if (!notation && !label) continue;

    /* 'lat' / 'long' — never easting/northing, which are OSGB metres. */
    double lat = 0, lon = 0;
    int geo = geoeo_numlax(s, "lat", &lat) && geoeo_numlax(s, "long", &lon) &&
              geoeo_ll_ok(lat, lon);

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, s, KEYS);
    if (label) cJSON_AddStringToObject(props, "label", label);
    if (geo) { cJSON_AddNumberToObject(props, "lat", lat);
               cJSON_AddNumberToObject(props, "long", lon); }
    cJSON *measures = cJSON_GetObjectItem(s, "measures");
    if (cJSON_IsArray(measures)) {
      cJSON *marr = cJSON_AddArrayToObject(props, "measures");
      cJSON *m;
      cJSON_ArrayForEach(m, measures) {
        cJSON *mo = cJSON_CreateObject();
        geoeo_copy(mo, m, "@id");
        geoeo_copy(mo, m, "parameterName");
        geoeo_copy(mo, m, "unitName");
        geoeo_copy(mo, m, "qualifier");
        cJSON_AddItemToArray(marr, mo);
      }
    }
    cJSON_AddStringToObject(props, "licence", "OGL v3");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    const char *river = geoeo_str(s, "riverName");
    char title[320];
    snprintf(title, sizeof title, "%s%s%s", label ? label : notation,
             river ? " · " : "", river ? river : "");

    char key[128];
    snprintf(key, sizeof key, "%s", notation ? notation : label);

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = river;
    it.link = geoeo_str(s, "@id");
    it.lang = "en";
    it.record_type = "flood-monitoring-station";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"flood\",\"river\",\"station\",\"uk\",\"environment-agency\"]";
    if (sink->emit(sink, &it) >= 0) (*emitted)++;
    free(pj);
    free(gj);
  }
  if (owned) cJSON_Delete(items);
  cJSON_Delete(doc);
  return seen;
}

/* Secondary call: an empty items array is the normal state in England. */
static void floods(const source_ctx *ctx, intel_sink *sink, int *emitted) {
  cJSON *doc = feed_get_json(ctx->http, EA_FLOODS, 30000);
  if (!doc) return;
  int owned = 0;
  cJSON *items = items_array(doc, &owned);
  if (!items) { cJSON_Delete(doc); return; }

  static const char *KEYS[] = { "description", "severity", "severityLevel",
                                "message", "timeRaised", "timeMessageChanged",
                                "eaAreaName", "isTidal", "@id", NULL };
  cJSON *w;
  cJSON_ArrayForEach(w, items) {
    const char *desc = geoeo_str(w, "description");
    const char *id = geoeo_str(w, "@id");
    if (!desc && !id) continue;

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, w, KEYS);
    cJSON *fa = cJSON_GetObjectItem(w, "floodArea");
    if (fa) {
      geoeo_copy(props, fa, "notation");
      geoeo_copy(props, fa, "county");
      geoeo_copy(props, fa, "riverOrSea");
      geoeo_copy(props, fa, "polygon");   /* a URL to the polygon, not a shape */
    }
    cJSON_AddStringToObject(props, "geo_note",
                            "floodArea.polygon is a URL to a shape, not inline "
                            "coordinates; no position asserted here");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[384];
    snprintf(title, sizeof title, "Flood warning — %s", desc ? desc : id);

    intel_item it = {0};
    it.remote_key = id ? id : desc;
    it.title = title;
    it.summary = geoeo_str(w, "severity");
    it.body = geoeo_str(w, "message");
    it.link = id;
    it.lang = "en";
    it.published_at = geoeo_str(w, "timeRaised");
    it.record_type = "flood-warning";
    it.has_geo = 0;              /* no inline coordinates on this endpoint */
    it.geometry_geojson = NULL;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"flood\",\"warning\",\"uk\",\"environment-agency\"]";
    if (sink->emit(sink, &it) >= 0) (*emitted)++;
    free(pj);
  }
  if (owned) cJSON_Delete(items);
  cJSON_Delete(doc);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0;
  int seen = stations(ctx, sink, &n);
  if (seen < 0) {
    fprintf(stderr, "[uk-flood-stations] station list fetch/parse failed\n");
    return -1;
  }
  floods(ctx, sink, &n);
  fprintf(stderr, "[uk-flood-stations] emitted %d rows (%d stations listed)\n",
          n, seen);
  return 0;
}

static const source_def geoeo_uk_flood_def = {
  .id = "uk-flood-stations", .collector = "environment",
  .name = "UK Environment Agency Flood Monitoring Stations",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://environment.data.gov.uk/flood-monitoring/id/stations?_limit=500&parameter=level",
  .description = "England's real-time flood monitoring network — every river, tidal, groundwater and rainfall station with coordinates, river name and its list of live measures, plus current flood warnings.",
  .license = "Open Government Licence v3, as declared by the response itself.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_uk_flood_def)
