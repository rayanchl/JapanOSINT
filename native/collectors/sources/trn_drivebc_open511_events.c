/* DriveBC — Open511 road events for British Columbia.
 * Endpoint: https://api.open511.gov.bc.ca/events?format=json
 * Emits: one intel row per event — the Open511 id, headline, status,
 * event_type, subtypes, severity, description, the affected roads (name, from,
 * to, direction), the geonames-linked area, and created/updated timestamps,
 * carrying the event's own GeoJSON geography.
 * Keyless.
 * Licence: DriveBC / BC Ministry of Transportation — Open Government Licence –
 * British Columbia.
 *
 * Parse notes: Open511 is a published open standard, so this parser is reusable
 * against other Open511 jurisdictions. `geography` is an embedded GeoJSON
 * geometry that may be a Point OR a LineString — both are handled, and the row
 * point is the first published vertex rather than a computed centre. Vendor
 * extensions are prefixed with '+' (e.g. "+ivr_message").
 *
 * PAGINATION — the walk was reading the wrong place and stopping after one
 * page. Measured 2026-08-24:
 *   • `pagination` sits at the TOP LEVEL of the document, not under `meta`.
 *     The old code asked for `meta.pagination.next_url`, which does not exist,
 *     so `next` was always NULL and the loop broke on its first turn.
 *   • DriveBC's default page is 50 events; `limit=500` returns all 239 current
 *     events in one short page. So the collector was storing 50 of 239 road
 *     incidents, every five minutes, with nothing in the output to show it.
 *   • DriveBC does not emit `next_url` at all — only `previous_url` and
 *     `offset` — so a full page is walked by advancing `offset`, which is the
 *     parameter the URL already carries. Other Open511 jurisdictions that do
 *     publish `next_url` (top level or nested under `meta`) are still honoured
 *     first, because a server-supplied link beats arithmetic.
 * A ceiling stop is disclosed as a collector-truncation-notice rather than
 * ending the run in silence.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

#define BC_PAGE_SIZE 500
#define BC_MAX_PAGES 20   /* exhaustive-ok: page-walk runaway guard; a stop with pages left emits a collector-truncation-notice */

static int emit_page(intel_sink *sink, cJSON *doc) {
  int n = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, cJSON_GetObjectItem(doc, "events")) {
    const char *id = jo_sv(e, "id");
    const char *headline = jo_sv(e, "headline");
    if (!id && !headline) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "DriveBC");
    trn_put_str(pr, "event_id", id);
    trn_put_str(pr, "headline", headline);
    trn_put_str(pr, "status", jo_sv(e, "status"));
    trn_put_str(pr, "event_type", jo_sv(e, "event_type"));
    trn_put_str(pr, "severity", jo_sv(e, "severity"));
    trn_put_str(pr, "created", jo_sv(e, "created"));
    trn_put_str(pr, "updated", jo_sv(e, "updated"));
    {
      const cJSON *sub = cJSON_GetObjectItem(e, "event_subtypes");
      if (cJSON_IsArray(sub))
        cJSON_AddItemToObject(pr, "event_subtypes", cJSON_Duplicate(sub, 1));
      const cJSON *roads = cJSON_GetObjectItem(e, "roads");
      if (cJSON_IsArray(roads))
        cJSON_AddItemToObject(pr, "roads", cJSON_Duplicate(roads, 1));
      const cJSON *areas = cJSON_GetObjectItem(e, "areas");
      if (cJSON_IsArray(areas))
        cJSON_AddItemToObject(pr, "areas", cJSON_Duplicate(areas, 1));
    }
    char *pj = cJSON_PrintUnformatted(pr);

    cJSON *geom = cJSON_GetObjectItem(e, "geography");
    char *gj = (geom && !cJSON_IsNull(geom)) ? cJSON_PrintUnformatted(geom)
                                             : NULL;
    double la, lo;
    int geo = trn_geom_first_pos(geom, &la, &lo);

    intel_item it = {0};
    it.remote_key      = id ? id : headline;
    it.title           = headline ? headline : id;
    it.summary         = jo_sv(e, "description");
    it.body            = jo_sv(e, "description");
    it.lang            = "en";
    it.published_at    = jo_sv(e, "updated");
    it.link            = "https://www.drivebc.ca/";
    it.record_type     = "road-incident";
    it.properties_json = pj ? pj : "{}";
    it.geometry_geojson = gj;
    it.tags_json = "[\"transport\",\"road\",\"british-columbia\",\"incident\"]";
    if (geo) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj); free(gj);
    cJSON_Delete(pr);
  }
  return n;
}

#define BC_BASE "https://api.open511.gov.bc.ca"

static int run(const source_ctx *ctx, intel_sink *sink) {
  char first[512], url[512];
  snprintf(first, sizeof first,
           BC_BASE "/events?format=json&limit=%d", BC_PAGE_SIZE);
  snprintf(url, sizeof url, "%s", first);

  int n = 0, pages = 0, ok = 0, truncated = 0;
  long offset = 0;
  while (pages < BC_MAX_PAGES) {
    cJSON *doc = feed_get_json(ctx->http, url, 30000);
    if (!doc) { if (pages > 0) truncated = 1; break; }
    ok = 1;
    n += emit_page(sink, doc);
    pages++;

    const cJSON *evs = cJSON_GetObjectItem(doc, "events");
    int got = cJSON_IsArray(evs) ? cJSON_GetArraySize(evs) : 0;

    /* the standard's own next link, top level first, then nested under meta */
    const cJSON *pg = cJSON_GetObjectItem(doc, "pagination");
    if (!cJSON_IsObject(pg))
      pg = cJSON_GetObjectItem(cJSON_GetObjectItem(doc, "meta"), "pagination");
    const char *next = pg ? jo_sv(pg, "next_url") : NULL;

    if (next && *next) {
      /* Open511 next_url is document-relative ("/events?...") */
      if (next[0] == '/') snprintf(url, sizeof url, BC_BASE "%s", next);
      else                snprintf(url, sizeof url, "%s", next);
    } else if (got >= BC_PAGE_SIZE) {
      /* A full page with no link: advance the offset this URL already names.
       * A SHORT page is the upstream saying it is finished, and following it
       * would be inventing a page nobody offered. */
      offset += BC_PAGE_SIZE;
      snprintf(url, sizeof url, BC_BASE "/events?format=json&limit=%d&offset=%ld",
               BC_PAGE_SIZE, offset);
    } else {
      cJSON_Delete(doc);
      break;                                     /* upstream is exhausted */
    }
    cJSON_Delete(doc);
    if (pages >= BC_MAX_PAGES) truncated = 1;    /* ceiling, not the upstream */
  }
  if (!ok) {
    fprintf(stderr, "[drivebc-open511-events] fetch/parse failed\n");
    return -1;
  }

  if (truncated)
    jo_trunc_notice(sink, "drivebc-open511-events", first, n, -1,
                    "the Open511 page walk stopped at its ceiling, or a "
                    "mid-walk fetch failed, while DriveBC was still handing "
                    "over full pages (it publishes no event total)",
                    "raise BC_MAX_PAGES in collectors/sources/"
                    "trn_drivebc_open511_events.c");

  fprintf(stderr, "[drivebc-open511-events] emitted %d over %d page(s)%s\n",
          n, pages, truncated ? " (TRUNCATED — notice emitted)" : "");
  return 0;
}

static const source_def trn_drivebc_open511_events_def = {
  .id = "drivebc-open511-events", .collector = "transport",
  .name = "DriveBC — Open511 road events for British Columbia",
  .update_interval_sec = 300, .run = run,
  .category = "transport", .type = "api",
  .url = "https://api.open511.gov.bc.ca/events?format=json",
  .description = "Live British Columbia road incidents, closures and construction in the Open511 standard — geolocated, severity-graded, with structured road/segment references.",
  .license = "Open Government Licence – British Columbia (DriveBC / BC MoTI).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_drivebc_open511_events_def)
