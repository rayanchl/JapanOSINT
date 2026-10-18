/* NASA EONET — the curated feed of currently-open natural events (wildfires,
 * storms, volcanoes, icebergs, floods, dust and haze), each linked back to the
 * authoritative source system.
 *
 * Endpoint (keyless): https://eonet.gsfc.nasa.gov/api/v3/events?limit=20000&status=open
 * Emits: event id, title, description, category ids/titles, source ids and
 *   URLs, the most recent observation's date, magnitudeValue/magnitudeUnit
 *   when the category carries them, and that observation's geometry.
 * Licence: NASA EONET, US Government open data, no key, no restriction.
 *
 * GEOMETRY IS AN ARRAY, NOT AN OBJECT (from parse_notes): event.geometry is a
 * LIST of timestamped observations, each with its own `type` ("Point" or
 * "Polygon") and `coordinates`. The LAST element is the current position, and
 * icebergs/storms carry dozens of entries. A Point gives [lon,lat] while a
 * Polygon gives nested rings — geoeo_geom() branches on the declared type, so a
 * ring array is never read as a longitude.
 * `closed` is JSON null for open events and is never stringified.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/pagewalk.h"
#include "geoeo_common.inc"

/* THE LIMIT WAS THE WHOLE QUESTION HERE, and `offset` is not the way out.
 * Measured 2026-10-18: EONET currently publishes 7,222 OPEN events and this
 * collector stored the first 50 of them — 7,172 live wildfires, storms,
 * volcanoes, icebergs and floods dropped on every run. The v3 API takes
 * `offset`, ACCEPTS it with HTTP 200, and IGNORES it: offset=50 returns the
 * same 50 events as offset=0, byte for byte. So there is no cursor to advance
 * and no next link to follow — writing `offset=0` here would make pw_walk
 * re-emit page 1 twenty times (rule 4d's confident wrong answer, in paging
 * form). What the service does honour is `limit`: it is unbounded in practice
 * and one request at limit=20000 returns all 7,222 in 5 MB.
 *
 * So the ask is deliberately ABOVE the collection — the open set grows and
 * shrinks as events open and close — and the walk is still run through pw_walk
 * so that if the page ever comes back exactly full (meaning the open set has
 * reached 20,000 and this is a slice again) the run files a truncation notice
 * instead of going quiet. */
static const char *URL =
    "https://eonet.gsfc.nasa.gov/api/v3/events?limit=20000&status=open";

static cJSON *eonet_fetch(const source_ctx *c, const char *url, void *ud) {
  (void)ud;
  return feed_get_json(c->http, url, 120000);   /* 5 MB */
}

/* pw_emit_fn. `seen` is the page's event count, not the emitted count: an event
 * with no id is skipped below, and reporting the emitted count would make a
 * full page look short — stopping the walk and suppressing its own truncation
 * notice (lib/pagewalk.h). */
static int eonet_emit_page(const source_ctx *ctx, intel_sink *sink,
                           const char *sid, cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)ud;
  cJSON *events = cJSON_GetObjectItem(doc, "events");
  if (!cJSON_IsArray(events)) {
    fprintf(stderr, "[%s] no events array\n", sid);
    *seen = 0;
    return 0;
  }
  *seen = cJSON_GetArraySize(events);

  int n = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, events) {
    const char *eid = geoeo_str(e, "id");
    const char *title = geoeo_str(e, "title");
    if (!eid || !title) continue;

    /* LAST observation = current position. */
    cJSON *geoms = cJSON_GetObjectItem(e, "geometry");
    cJSON *last = NULL;
    if (cJSON_IsArray(geoms)) {
      int gn = cJSON_GetArraySize(geoms);
      if (gn > 0) last = cJSON_GetArrayItem(geoms, gn - 1);
    }
    double lat = 0, lon = 0;
    char *gj = NULL;
    int geo = geoeo_geom(last, &lat, &lon, &gj);

    const char *obs_date = last ? geoeo_str(last, "date") : NULL;
    double magv = 0;
    int has_mag = last ? geoeo_num(last, "magnitudeValue", &magv) : 0;
    const char *magu = last ? geoeo_str(last, "magnitudeUnit") : NULL;

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "event_id", eid);
    cJSON_AddStringToObject(props, "title", title);
    geoeo_copy(props, e, "description");
    geoeo_copy(props, e, "link");
    if (obs_date) cJSON_AddStringToObject(props, "observed_at", obs_date);
    if (has_mag) cJSON_AddNumberToObject(props, "magnitudeValue", magv);
    geoeo_add_str(props, "magnitudeUnit", magu);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    if (last) geoeo_copy(props, last, "type");

    const char *cat0 = NULL;
    cJSON *cats = cJSON_GetObjectItem(e, "categories");
    if (cJSON_IsArray(cats)) {
      cJSON *carr = cJSON_AddArrayToObject(props, "categories");
      cJSON *c;
      cJSON_ArrayForEach(c, cats) {
        const char *cid = geoeo_str(c, "id");
        const char *ct = geoeo_str(c, "title");
        if (cid) {
          cJSON_AddItemToArray(carr, cJSON_CreateString(cid));
          if (!cat0) cat0 = ct ? ct : cid;
        }
      }
    }
    const char *link = NULL;
    cJSON *srcs = cJSON_GetObjectItem(e, "sources");
    if (cJSON_IsArray(srcs)) {
      cJSON *sarr = cJSON_AddArrayToObject(props, "sources");
      cJSON *s;
      cJSON_ArrayForEach(s, srcs) {
        const char *su = geoeo_str(s, "url");
        const char *si = geoeo_str(s, "id");
        if (su) {
          cJSON_AddItemToArray(sarr, cJSON_CreateString(su));
          if (!link) link = su;
        } else if (si) {
          cJSON_AddItemToArray(sarr, cJSON_CreateString(si));
        }
      }
    }
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char summary[256];
    if (has_mag)
      snprintf(summary, sizeof summary, "%s%s%.2f %s", cat0 ? cat0 : "",
               cat0 ? " · " : "", magv, magu ? magu : "");
    else
      snprintf(summary, sizeof summary, "%s", cat0 ? cat0 : "");

    intel_item it = {0};
    it.remote_key = eid;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.body = geoeo_str(e, "description");
    it.link = link;
    it.lang = "en";
    it.published_at = obs_date;
    it.record_type = "natural-event";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"natural-event\",\"eonet\",\"nasa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = pw_walk(ctx, sink, "nasa-eonet-events", URL,
                  eonet_fetch, eonet_emit_page, NULL);
  if (n < 0) {
    fprintf(stderr, "[nasa-eonet-events] fetch/parse failed\n");
    return -1;
  }
  return 0;
}

static const source_def geoeo_eonet_def = {
  .id = "nasa-eonet-events", .collector = "environment",
  .name = "NASA EONET Natural Event Tracker",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://eonet.gsfc.nasa.gov/api/v3/events?limit=20000&status=open",
  .description = "NASA's curated feed of currently-open natural events — wildfires, storms, volcanoes, icebergs, floods, dust and haze — each linked back to the authoritative source system.",
  .license = "NASA EONET, US Government open data; no key, no restriction.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_eonet_def)
