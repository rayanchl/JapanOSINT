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
 * extensions are prefixed with '+' (e.g. "+ivr_message"). Results are paginated
 * via meta/pagination.next_url, which is followed up to a bounded page count.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

#define BC_MAX_PAGES 20

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

static int run(const source_ctx *ctx, intel_sink *sink) {
  char url[512];
  snprintf(url, sizeof url, "https://api.open511.gov.bc.ca/events?format=json");

  int n = 0, pages = 0, ok = 0;
  while (pages < BC_MAX_PAGES) {
    cJSON *doc = feed_get_json(ctx->http, url, 30000);
    if (!doc) break;
    ok = 1;
    n += emit_page(sink, doc);
    pages++;
    const cJSON *pg = cJSON_GetObjectItem(
        cJSON_GetObjectItem(doc, "meta"), "pagination");
    const char *next = pg ? jo_sv(pg, "next_url") : NULL;
    if (!next) { cJSON_Delete(doc); break; }
    snprintf(url, sizeof url, "%s", next);
    cJSON_Delete(doc);
  }
  if (!ok) {
    fprintf(stderr, "[drivebc-open511-events] fetch/parse failed\n");
    return -1;
  }
  fprintf(stderr, "[drivebc-open511-events] emitted %d over %d page(s)\n",
          n, pages);
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
