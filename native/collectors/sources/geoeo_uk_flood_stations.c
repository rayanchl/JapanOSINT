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
#include "lib/jocore.h"          /* jo_trunc_notice — the shared R7 disclosure */
#include "geoeo_common.inc"

#define EA_STATIONS                                                           \
  "https://environment.data.gov.uk/flood-monitoring/id/stations"              \
  "?_limit=500&parameter=level"
#define EA_STATIONS_BASE                                                      \
  "https://environment.data.gov.uk/flood-monitoring/id/stations"              \
  "?parameter=level"
#define EA_FLOODS "https://environment.data.gov.uk/flood-monitoring/id/floods"

/* `_limit` is the EA's page size and `_offset` walks the pages. The station
 * list is 376 rows at parameter=level today, so a single `_limit=500` request
 * happened to cover it — but "happens to fit" is not a guarantee, and the day
 * the network passes 500 stations a single-page read would clip the tail with
 * no error and no notice. The walk below ends on a short page instead. */
#define EA_PAGE_SIZE 500
#define EA_MAX_PAGES 40   /* exhaustive-ok: offset-walk runaway guard; an early stop emits a collector-truncation-notice */

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

static int stations_page(const source_ctx *ctx, intel_sink *sink, int *emitted,
                         int offset) {
  char url[224];
  snprintf(url, sizeof url, "%s&_limit=%d&_offset=%d", EA_STATIONS_BASE,
           EA_PAGE_SIZE, offset);
  cJSON *doc = feed_get_json(ctx->http, url, 45000);
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
    /* JSON-LD: `label` is a string on most stations and an ARRAY on the ones
     * the EA has recorded under more than one name. The first is the display
     * label; the others are alternate names a search would otherwise never
     * match, so they are kept as labels_all rather than dropped. */
    const char *label = geoeo_str(s, "label");
    cJSON *label_arr = NULL;
    if (!label) {
      cJSON *l = cJSON_GetObjectItem(s, "label");
      if (cJSON_IsArray(l) && cJSON_GetArraySize(l) > 0) {
        cJSON *e;
        cJSON_ArrayForEach(e, l) {
          if (!cJSON_IsString(e) || !e->valuestring || !e->valuestring[0]) continue;
          if (!label) label = e->valuestring;
          if (!label_arr) label_arr = cJSON_CreateArray();
          cJSON_AddItemToArray(label_arr, cJSON_CreateString(e->valuestring));
        }
      }
    }
    if (!notation && !label) { cJSON_Delete(label_arr); continue; }

    /* 'lat' / 'long' — never easting/northing, which are OSGB metres. */
    double lat = 0, lon = 0;
    int geo = geoeo_numlax(s, "lat", &lat) && geoeo_numlax(s, "long", &lon) &&
              geoeo_ll_ok(lat, lon);

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, s, KEYS);
    if (label) cJSON_AddStringToObject(props, "label", label);
    if (label_arr && cJSON_GetArraySize(label_arr) > 1)
      cJSON_AddItemToObject(props, "labels_all", cJSON_Duplicate(label_arr, 1));
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
    cJSON_Delete(label_arr);
  }
  if (owned) cJSON_Delete(items);
  cJSON_Delete(doc);
  return seen;
}

/* Walk the offsets until the EA stops handing back full pages. */
static int stations(const source_ctx *ctx, intel_sink *sink, int *emitted) {
  int max_pages = EA_MAX_PAGES;
  const char *penv = getenv("JO_EA_STATION_PAGES");
  if (penv && *penv) { int v = atoi(penv); if (v > 0) max_pages = v; }

  int total_seen = 0, offset = 0;
  for (int page = 0; page < max_pages; page++) {
    int seen = stations_page(ctx, sink, emitted, offset);
    if (seen < 0) {
      if (page == 0) return -1;              /* the first fetch failed (R3) */
      jo_trunc_notice(sink, "uk-flood-stations", EA_STATIONS_BASE, total_seen,
                      -1,
                      "the station page walk stopped when a page failed to "
                      "fetch or parse; later stations were not read",
                      "re-run the collector; the walk restarts at _offset=0");
      break;
    }
    total_seen += seen;
    if (seen < EA_PAGE_SIZE) break;          /* short page = last page      */
    offset += seen;
    if (page + 1 == max_pages)
      jo_trunc_notice(sink, "uk-flood-stations", EA_STATIONS_BASE, total_seen,
                      -1,
                      "the page-walk ceiling stopped the run while the EA was "
                      "still returning full pages of stations",
                      "raise $JO_EA_STATION_PAGES");
  }
  return total_seen;
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
