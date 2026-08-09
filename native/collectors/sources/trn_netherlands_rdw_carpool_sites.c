/* Netherlands RDW — carpool and park-and-ride sites.
 * Endpoint: https://opendata.rdw.nl/resource/9c54-cmfx.json?$limit=1000
 * Emits: one intel row per official carpool / P+R site — areaid, areadesc,
 * usageid, the parking-space count, the EV charge-point count and the maximum
 * vehicle entry height, pinned on the site's own coordinates. Keyless.
 * Licence: RDW open data via Socrata — CC0 / Dutch open government data.
 *
 * Parse notes: Socrata SoDA JSON. Coordinates are STRINGS nested under
 * `location`, and the numeric fields (aantal_parkeer_plaatsen,
 * aantal_laad_punten, maximale_inrij_hoogte) are STRINGS too — all are coerced
 * rather than read at the wrong type.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

#define RDW_CARPOOL "https://opendata.rdw.nl/resource/9c54-cmfx.json?$limit=1000"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, RDW_CARPOOL, 30000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[netherlands-rdw-carpool-sites] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, doc) {
    const char *aid = jo_sv(s, "areaid");
    const char *desc = jo_sv(s, "areadesc");
    if (!aid && !desc) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "RDW (Netherlands)");
    trn_put_str(pr, "area_id", aid);
    trn_put_str(pr, "area_description", desc);
    trn_put_str(pr, "usage_id", jo_sv(s, "usageid"));
    trn_put_num(pr, "parking_spaces", s, "aantal_parkeer_plaatsen");
    trn_put_num(pr, "charge_points", s, "aantal_laad_punten");
    trn_put_num(pr, "max_entry_height_m", s, "maximale_inrij_hoogte");
    char *pj = cJSON_PrintUnformatted(pr);

    char summary[160]; summary[0] = 0;
    double spaces;
    if (trn_num(s, "aantal_parkeer_plaatsen", &spaces))
      snprintf(summary, sizeof summary, "%.0f spaces", spaces);

    intel_item it = {0};
    it.remote_key      = aid ? aid : desc;
    it.title           = desc ? desc : aid;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "nl";
    it.record_type     = "park-and-ride";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"parking\",\"netherlands\",\"carpool\"]";
    const cJSON *loc = cJSON_GetObjectItem(s, "location");
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
  fprintf(stderr, "[netherlands-rdw-carpool-sites] emitted %d\n", n);
  return 0;
}

static const source_def trn_netherlands_rdw_carpool_sites_def = {
  .id = "netherlands-rdw-carpool-sites", .collector = "transport",
  .name = "Netherlands RDW — carpool / park-and-ride sites",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset",
  .url = RDW_CARPOOL,
  .description = "Every official Dutch carpool and park-and-ride site with coordinates, space count, EV charge-point count and maximum vehicle height.",
  .license = "CC0 / Dutch open government data (RDW via Socrata).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_netherlands_rdw_carpool_sites_def)
