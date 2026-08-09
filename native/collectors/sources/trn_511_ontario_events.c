/* Ontario 511 — road incidents, closures and roadworks.
 * Endpoint: https://511on.ca/api/v2/get/event
 * Emits: one intel row per road event — event ID, roadway name, direction of
 * travel, the free-text description, EventType/EventSubType, the full-closure
 * flag, lanes affected, the reported and planned-end timestamps, height/weight
 * restrictions and the far end of the affected segment when published, pinned
 * on the event's own Latitude/Longitude. Keyless.
 * Licence: Ontario MTO 511 open API — Open Government Licence – Ontario.
 *
 * Parse notes: top-level array. Reported / PlannedEndDate / LastUpdated are
 * UNIX SECONDS, not ISO strings, and are converted here. LatitudeSecondary and
 * LongitudeSecondary mark the far end of a segment when present and are carried
 * in properties rather than used as a second point. Restrictions is an object
 * of nullable numbers. EncodedPolyline is Google-encoded and is passed through
 * untouched rather than decoded into a fake geometry.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, "https://511on.ca/api/v2/get/event",
                             30000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[511-ontario-events] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, doc) {
    char eid[64];
    trn_idfield(e, "ID", eid, sizeof eid);
    const char *road = jo_sv(e, "RoadwayName");
    const char *desc = jo_sv(e, "Description");
    if (!eid[0] && !road) continue;

    char reported[40] = {0}, planned[40] = {0}, updated[40] = {0};
    double t;
    if (trn_num(e, "Reported", &t))        trn_iso_from_unix(t, reported, sizeof reported);
    if (trn_num(e, "PlannedEndDate", &t))  trn_iso_from_unix(t, planned, sizeof planned);
    if (trn_num(e, "LastUpdated", &t))     trn_iso_from_unix(t, updated, sizeof updated);

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Ontario MTO (511)");
    trn_put_str(pr, "event_id", eid);
    trn_put_str(pr, "roadway_name", road);
    trn_put_str(pr, "direction_of_travel", jo_sv(e, "DirectionOfTravel"));
    trn_put_str(pr, "event_type", jo_sv(e, "EventType"));
    trn_put_str(pr, "event_subtype", jo_sv(e, "EventSubType"));
    trn_put_str(pr, "lanes_affected", jo_sv(e, "LanesAffected"));
    trn_put_bool(pr, "is_full_closure", e, "IsFullClosure");
    trn_put_str(pr, "reported", reported);
    trn_put_str(pr, "planned_end", planned);
    trn_put_str(pr, "last_updated", updated);
    trn_put_num(pr, "latitude_secondary", e, "LatitudeSecondary");
    trn_put_num(pr, "longitude_secondary", e, "LongitudeSecondary");
    trn_put_str(pr, "encoded_polyline", jo_sv(e, "EncodedPolyline"));
    {
      const cJSON *res = cJSON_GetObjectItem(e, "Restrictions");
      if (cJSON_IsObject(res))
        cJSON_AddItemToObject(pr, "restrictions", cJSON_Duplicate(res, 1));
    }
    char *pj = cJSON_PrintUnformatted(pr);

    char title[384];
    const char *ty = jo_sv(e, "EventType");
    if (road && ty) snprintf(title, sizeof title, "%s — %s", road, ty);
    else if (road)  snprintf(title, sizeof title, "%s", road);
    else            snprintf(title, sizeof title, "Ontario road event %s", eid);

    intel_item it = {0};
    it.remote_key      = eid[0] ? eid : road;
    it.title           = title;
    it.summary         = desc;
    it.body            = desc;
    it.lang            = "en";
    it.published_at    = reported[0] ? reported : NULL;
    it.link            = "https://511on.ca/";
    it.record_type     = "road-incident";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"road\",\"ontario\",\"incident\"]";
    double la, lo;
    if (trn_num(e, "Latitude", &la) && trn_num(e, "Longitude", &lo) &&
        trn_geo_ok(la, lo)) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[511-ontario-events] emitted %d\n", n);
  return 0;                    /* a quiet road network is an honest 0 (R3) */
}

static const source_def trn_511_ontario_events_def = {
  .id = "511-ontario-events", .collector = "transport",
  .name = "Ontario 511 — road incidents, closures and roadworks",
  .update_interval_sec = 300, .run = run,
  .category = "transport", .type = "api",
  .url = "https://511on.ca/api/v2/get/event",
  .description = "Live Ontario road events — incidents, closures, construction — with coordinates, lanes affected, full-closure flag and height/weight restrictions.",
  .license = "Open Government Licence – Ontario (MTO 511 open API).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_511_ontario_events_def)
