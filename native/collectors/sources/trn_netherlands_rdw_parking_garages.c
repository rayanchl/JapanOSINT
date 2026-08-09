/* Netherlands RDW — national parking-garage registry with coordinates.
 * Endpoint: https://opendata.rdw.nl/resource/t5pc-eb34.json?$limit=1000
 * Emits: one intel row per garage — areamanagerid, areaid, areadesc, usageid
 * and the start/end validity dates, pinned on the garage's own coordinates.
 * Keyless.
 * Licence: RDW (Netherlands Vehicle Authority) open data via Socrata — CC0 /
 * Dutch open government data.
 *
 * Parse notes: Socrata SoDA JSON. The coordinates are STRINGS inside a nested
 * `location` object (location.latitude / location.longitude), not top-level
 * numbers, and human_address is an empty JSON string that carries nothing.
 * Beware the sibling resource adw6-9hsg ("GEBIED REGELING"), which returns 200
 * but has NO coordinates at all, so it is not a substitute for this one.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

#define RDW_GARAGES "https://opendata.rdw.nl/resource/t5pc-eb34.json?$limit=1000"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, RDW_GARAGES, 30000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[netherlands-rdw-parking-garages] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *g;
  cJSON_ArrayForEach(g, doc) {
    const char *aid = jo_sv(g, "areaid");
    const char *desc = jo_sv(g, "areadesc");
    if (!aid && !desc) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "RDW (Netherlands)");
    trn_put_str(pr, "area_id", aid);
    trn_put_str(pr, "area_description", desc);
    trn_put_str(pr, "area_manager_id", jo_sv(g, "areamanagerid"));
    trn_put_str(pr, "usage_id", jo_sv(g, "usageid"));
    trn_put_str(pr, "start_data_area", jo_sv(g, "startdataarea"));
    trn_put_str(pr, "end_data_area", jo_sv(g, "enddataarea"));
    char *pj = cJSON_PrintUnformatted(pr);

    intel_item it = {0};
    it.remote_key      = aid ? aid : desc;
    it.title           = desc ? desc : aid;
    it.summary         = jo_sv(g, "usageid");
    it.lang            = "nl";
    it.record_type     = "parking-garage";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"parking\",\"netherlands\"]";
    /* coordinates are STRINGS nested under `location` */
    const cJSON *loc = cJSON_GetObjectItem(g, "location");
    double la, lo;
    if (loc && trn_num(loc, "latitude", &la) &&
        trn_num(loc, "longitude", &lo) && trn_geo_ok(la, lo)) {
      it.has_geo = 1; it.lat = la; it.lon = lo;
    }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[netherlands-rdw-parking-garages] emitted %d\n", n);
  return 0;
}

static const source_def trn_netherlands_rdw_parking_garages_def = {
  .id = "netherlands-rdw-parking-garages", .collector = "transport",
  .name = "Netherlands RDW — parking garages with coordinates",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset",
  .url = RDW_GARAGES,
  .description = "Dutch national parking-garage registry with per-garage coordinates and the managing authority id.",
  .license = "CC0 / Dutch open government data (RDW via Socrata).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_netherlands_rdw_parking_garages_def)
