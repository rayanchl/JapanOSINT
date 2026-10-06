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
#include "lib/jocore.h"
#include "lib/keyqual.h"
#include "trn_common.inc"

#define RDW_GARAGES "https://opendata.rdw.nl/resource/t5pc-eb34.json?$limit=1000&$order=:id"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, RDW_GARAGES, 30000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[netherlands-rdw-parking-garages] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0, folded = 0;
  /* IDENTITY. areaid is unique only within an area manager: live 2026-10-06
   * the 237 garages carry 236 areaids (599_HART is published by managers 2448
   * and 2459) but 237 areamanagerid+areaid pairs. Pass 1 counts every key;
   * pass 2 qualifies EVERY garage of a colliding areaid by its manager id
   * ("599_HART|2448"), falling back to a hash of the row. It used to be
   * first-come-plain — the plain areaid went to whichever garage `$order=:id`
   * listed first, and :id is Socrata's internal row id, which RDW's re-uploads
   * renumber, so the two garages traded uids. See lib/keyqual.h. */
  keyqual kq = {0};
  cJSON *g;
  cJSON_ArrayForEach(g, doc) {
    const char *k = jo_sv(g, "areaid");
    if (!k) k = jo_sv(g, "areadesc");
    if (k) keyqual_add(&kq, k, jo_sv(g, "areamanagerid"));
  }
  keyqual_seal(&kq);

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
    char keybuf[512];
    const char *rk = aid ? aid : desc;
    if (keyqual_count(&kq, rk) > 1) {
      const char *base = rk;
      char *raw = cJSON_PrintUnformatted(g);
      rk = keyqual_uid(&kq, base, jo_sv(g, "areamanagerid"), raw ? raw : "",
                       keybuf, sizeof keybuf);
      free(raw);
      if (!keyqual_claim(&kq, base, rk)) {
        folded++; free(pj); cJSON_Delete(pr); continue;
      }
    }
    it.remote_key      = rk;
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
  keyqual_free(&kq);
  cJSON_Delete(doc);
  fprintf(stderr, "[netherlands-rdw-parking-garages] emitted %d (%d byte-identical "
          "repeats folded)\n", n, folded);
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
