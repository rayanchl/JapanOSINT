/* Two FDSN-ws `event` services that only speak the pipe-separated TEXT format.
 *
 * Endpoints (both keyless):
 *   https://geofon.gfz.de/fdsnws/event/1/query?format=text&limit=50&orderby=time
 *   https://earthquakescanada.nrcan.gc.ca/api/fdsnws/event/1/query?format=text&limit=50
 * Emits: EventID, origin time, latitude, longitude, depth (km), magnitude +
 *        magnitude type, event location name, event type — every value taken
 *        from the response row.
 * Licences: GEOFON/GFZ data policy — free use for research and operational
 *   purposes with citation of GFZ, no prohibition on programmatic access.
 *   NRCan — Open Government Licence Canada 2.0 (copy/modify/redistribute with
 *   attribution).
 *
 * parse_notes honoured:
 *  - format=json / format=geojson 400 (GEOFON) and 422 (NRCan) on these two
 *    implementations; only format=text works, hence this parser.
 *  - GEOFON has 14 columns, NRCan only 8, so NOTHING is indexed by a hardcoded
 *    position: the '#'-prefixed header line is read and the column indices are
 *    resolved by name per service.
 *  - FDSN text permits empty fields. A row with an empty Latitude or Longitude
 *    is emitted with has_geo=0 — never with 0,0.
 *  - NRCan's EventLocationName is bilingual with an embedded '/'
 *    ("181 km SW of Port Hardy, BC/181 km SO de Port Hardy, BC"). Splitting on
 *    '/' would corrupt place names, so it is kept whole.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"
#include <strings.h>

#define FDSN_MAXCOL 32

/* Split one line on '|' in place; each field trimmed. */
static int fdsn_split(char *line, char **out, int max) {
  int n = 0;
  char *p = line;
  out[n++] = p;
  while (*p && n < max) {
    if (*p == '|') {
      *p = 0;
      out[n++] = p + 1;
    }
    p++;
  }
  for (int i = 0; i < n; i++) geoeo_trim(out[i]);
  return n;
}

static int fdsn_col(char **hdr, int hn, const char *name) {
  for (int i = 0; i < hn; i++)
    if (hdr[i] && strcasecmp(hdr[i], name) == 0) return i;
  return -1;
}

static const char *fdsn_get(char **f, int fn, int idx) {
  if (idx < 0 || idx >= fn) return NULL;
  return f[idx][0] ? f[idx] : NULL;
}

static int fdsn_collect(const source_ctx *ctx, intel_sink *sink,
                        const char *sid, const char *url, const char *tags) {
  char *body = feed_get_text(ctx->http, url, 30000);
  if (!body) {
    fprintf(stderr, "[%s] fetch failed\n", sid);
    return -1;
  }

  int c_id = -1, c_time = -1, c_lat = -1, c_lon = -1, c_dep = -1;
  int c_mtyp = -1, c_mag = -1, c_loc = -1, c_type = -1;
  int have_header = 0, n = 0;

  char *save = body;
  for (char *line = save; line && *line;) {
    char *nl = strchr(line, '\n');
    if (nl) *nl = 0;
    char *cur = line;
    line = nl ? nl + 1 : NULL;
    geoeo_trim(cur);
    if (!cur[0]) continue;

    char *fields[FDSN_MAXCOL];
    if (cur[0] == '#') {
      if (have_header) continue;                    /* only the first header */
      char *h = cur + 1;
      int hn = fdsn_split(h, fields, FDSN_MAXCOL);
      c_id = fdsn_col(fields, hn, "EventID");
      c_time = fdsn_col(fields, hn, "Time");
      c_lat = fdsn_col(fields, hn, "Latitude");
      c_lon = fdsn_col(fields, hn, "Longitude");
      c_dep = fdsn_col(fields, hn, "Depth/km");
      c_mtyp = fdsn_col(fields, hn, "MagType");
      c_mag = fdsn_col(fields, hn, "Magnitude");
      c_loc = fdsn_col(fields, hn, "EventLocationName");
      c_type = fdsn_col(fields, hn, "EventType");
      have_header = 1;
      continue;
    }
    if (!have_header) continue;      /* never guess columns without a header */

    int fn = fdsn_split(cur, fields, FDSN_MAXCOL);
    const char *eid = fdsn_get(fields, fn, c_id);
    if (!eid) continue;
    const char *when = fdsn_get(fields, fn, c_time);
    const char *loc = fdsn_get(fields, fn, c_loc);
    const char *mtyp = fdsn_get(fields, fn, c_mtyp);
    const char *etype = fdsn_get(fields, fn, c_type);

    double lat = 0, lon = 0, dep = 0, mag = 0;
    const char *slat = fdsn_get(fields, fn, c_lat);
    const char *slon = fdsn_get(fields, fn, c_lon);
    /* Empty lat OR empty lon → no location at all. */
    int geo = slat && slon && geoeo_strtod(slat, &lat) &&
              geoeo_strtod(slon, &lon) && geoeo_ll_ok(lat, lon);
    int has_dep = geoeo_strtod(fdsn_get(fields, fn, c_dep), &dep);
    int has_mag = geoeo_strtod(fdsn_get(fields, fn, c_mag), &mag);

    char title[384];
    if (has_mag)
      snprintf(title, sizeof title, "M%.1f%s%s — %s", mag, mtyp ? " " : "",
               mtyp ? mtyp : "", loc ? loc : eid);
    else
      snprintf(title, sizeof title, "%s — %s", eid, loc ? loc : "seismic event");

    char summary[192];
    if (has_dep)
      snprintf(summary, sizeof summary, "depth %.1f km%s%s", dep,
               etype ? " · " : "", etype ? etype : "");
    else
      snprintf(summary, sizeof summary, "%s", etype ? etype : "");

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "event_id", eid);
    geoeo_add_str(props, "time", when);
    geoeo_add_str(props, "location_name", loc);
    geoeo_add_str(props, "mag_type", mtyp);
    geoeo_add_str(props, "event_type", etype);
    if (has_mag) cJSON_AddNumberToObject(props, "magnitude", mag);
    if (has_dep) cJSON_AddNumberToObject(props, "depth_km", dep);
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

    intel_item it = {0};
    it.remote_key = eid;
    it.title = title;
    it.summary = summary;
    it.lang = "en";
    it.published_at = when;
    it.record_type = "earthquake";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = tags;
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }

  free(body);
  if (!have_header) {
    fprintf(stderr, "[%s] no '#' header line — unparseable\n", sid);
    return -1;
  }
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

static int geofon_run(const source_ctx *ctx, intel_sink *sink) {
  return fdsn_collect(
      ctx, sink, "geofon-quakes",
      "https://geofon.gfz.de/fdsnws/event/1/query?format=text&limit=50&orderby=time",
      "[\"earthquake\",\"seismic\",\"global\",\"gfz\"]");
}

static int nrcan_run(const source_ctx *ctx, intel_sink *sink) {
  return fdsn_collect(
      ctx, sink, "nrcan-quakes",
      "https://earthquakescanada.nrcan.gc.ca/api/fdsnws/event/1/query?format=text&limit=50",
      "[\"earthquake\",\"seismic\",\"canada\",\"nrcan\"]");
}

static const source_def geoeo_geofon_def = {
  .id = "geofon-quakes", .collector = "environment",
  .name = "GEOFON Global Earthquake Bulletin (GFZ)",
  .update_interval_sec = 300, .run = geofon_run,
  .category = "environment", .type = "api",
  .url = "https://geofon.gfz.de/fdsnws/event/1/query?format=text&limit=50&orderby=time",
  .description = "GFZ Potsdam GEOFON global seismic network solutions — an independent global earthquake catalogue with different station coverage from USGS and EMSC.",
  .license = "GEOFON/GFZ data policy: free use for research and operational purposes with citation of GFZ.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_geofon_def)

static const source_def geoeo_nrcan_def = {
  .id = "nrcan-quakes", .collector = "environment",
  .name = "Earthquakes Canada (NRCan) FDSN Event Service",
  .update_interval_sec = 900, .run = nrcan_run,
  .category = "environment", .type = "api",
  .url = "https://earthquakescanada.nrcan.gc.ca/api/fdsnws/event/1/query?format=text&limit=50",
  .description = "Natural Resources Canada national earthquake catalogue — Canadian and Arctic events, including quarry blasts flagged as such, well below the USGS global threshold.",
  .license = "Open Government Licence — Canada 2.0; copy, modify and redistribute with attribution.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_nrcan_def)
