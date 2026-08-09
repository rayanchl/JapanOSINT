/* US FRA — national highway-rail grade crossing inventory (Form 71).
 * Endpoint: https://data.transportation.gov/resource/m2f8-22s6.json?$limit=1000
 * Emits: one intel row per crossing — crossingid, operating railroad, state,
 * county and city, the crossing street, the railroad milepost, the nearest
 * timetable station, the inventory reason description and the revision date.
 * Keyless.
 * Licence: US Federal Railroad Administration via data.transportation.gov
 * (Socrata) — US Government work, public domain.
 *
 * Parse notes: Socrata SoDA JSON. The current-inventory resource m2f8-22s6 does
 * NOT expose latitude/longitude in the fields it returns, so every row here is
 * emitted with has_geo = 0. Synthesising a position from countyname/cityname
 * would be exactly the invented geometry R2 forbids; the companion "Grade
 * Crossings" resource nw2s-ygjq is the geospatial one for anybody who needs it.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

#define FRA_URL "https://data.transportation.gov/resource/m2f8-22s6.json?$limit=1000"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, FRA_URL, 30000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[fra-grade-crossing-inventory] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    const char *cid = jo_sv(r, "crossingid");
    if (!cid) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator",
                            "US Federal Railroad Administration");
    cJSON_AddStringToObject(pr, "crossing_id", cid);
    trn_put_str(pr, "railroad_name", jo_sv(r, "railroadname"));
    trn_put_str(pr, "state_name", jo_sv(r, "statename"));
    trn_put_str(pr, "county_name", jo_sv(r, "countyname"));
    trn_put_str(pr, "city_name", jo_sv(r, "cityname"));
    trn_put_str(pr, "street", jo_sv(r, "street"));
    trn_put_str(pr, "railroad_milepost", jo_sv(r, "railroadmilepostnumber"));
    trn_put_str(pr, "nearest_timetable_station",
                jo_sv(r, "nearesttimetablestation"));
    trn_put_str(pr, "reason_description", jo_sv(r, "reasondescription"));
    trn_put_str(pr, "revision_date", jo_sv(r, "revisiondate"));
    char *pj = cJSON_PrintUnformatted(pr);

    const char *street = jo_sv(r, "street");
    const char *rr = jo_sv(r, "railroadname");
    char title[320], summary[256];
    if (street && rr) snprintf(title, sizeof title, "%s at %s", rr, street);
    else snprintf(title, sizeof title, "Grade crossing %s", cid);
    const char *st = jo_sv(r, "statename"), *co = jo_sv(r, "countyname");
    if (st && co) snprintf(summary, sizeof summary, "%s / %s", st, co);
    else if (st)  snprintf(summary, sizeof summary, "%s", st);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = cid;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "en";
    it.published_at    = jo_sv(r, "revisiondate");
    it.record_type     = "rail-grade-crossing";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"rail\",\"us\",\"infrastructure\"]";
    /* resource carries no coordinates → no geo (R2) */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[fra-grade-crossing-inventory] emitted %d\n", n);
  return 0;
}

static const source_def trn_fra_grade_crossing_inventory_def = {
  .id = "fra-grade-crossing-inventory", .collector = "transport",
  .name = "US FRA highway-rail grade crossing inventory",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset",
  .url = FRA_URL,
  .description = "The FRA Form 71 national inventory of public and private highway-rail grade crossings — railroad, milepost, street and warning-device detail.",
  .license = "US Government work, public domain (FRA via data.transportation.gov).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_fra_grade_crossing_inventory_def)
