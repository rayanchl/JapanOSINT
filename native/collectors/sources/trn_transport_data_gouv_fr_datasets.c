/* transport.data.gouv.fr — French national mobility data registry.
 * Endpoint: https://transport.data.gouv.fr/api/datasets
 * Emits: one intel row per registered dataset — datagouv_id, title, type,
 * publisher, licence, created_at, the covered area (type, name and INSEE code)
 * and every published resource with its format, title, URL and update time.
 * Keyless.
 * Licence: transport.data.gouv.fr (Ministère de la Transition écologique /
 * Etalab) — Licence Ouverte v2.0; per-dataset licences are carried in the
 * payload and re-emitted in properties.
 *
 * Parse notes: 2.5 MB top-level array. covered_area gives commune/EPCI/region
 * names with INSEE codes but NO geometry in this endpoint, so every row is
 * emitted with has_geo = 0 — manufacturing a coordinate from the area name is
 * exactly what R2 forbids. resources[] is where the actual downloadable feed
 * URLs live, which is what makes this a discovery layer for dozens of
 * downstream GTFS / GTFS-RT / GBFS / SIRI feeds.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

#define TDG_URL "https://transport.data.gouv.fr/api/datasets"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, TDG_URL, 60000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[transport-data-gouv-fr-datasets] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *d;
  cJSON_ArrayForEach(d, doc) {
    const char *id = jo_sv(d, "datagouv_id");
    const char *title = jo_sv(d, "title");
    if (!id && !title) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "registry", "transport.data.gouv.fr");
    trn_put_str(pr, "datagouv_id", id);
    trn_put_str(pr, "title", title);
    trn_put_str(pr, "type", jo_sv(d, "type"));
    trn_put_str(pr, "publisher", jo_sv(d, "publisher"));
    trn_put_str(pr, "licence", jo_sv(d, "licence"));
    trn_put_str(pr, "created_at", jo_sv(d, "created_at"));
    trn_put_str(pr, "page_url", jo_sv(d, "page_url"));
    trn_put_str(pr, "slug", jo_sv(d, "slug"));
    {
      const cJSON *ca = cJSON_GetObjectItem(d, "covered_area");
      if (cJSON_IsObject(ca))
        cJSON_AddItemToObject(pr, "covered_area", cJSON_Duplicate(ca, 1));
      const cJSON *res = cJSON_GetObjectItem(d, "resources");
      if (cJSON_IsArray(res)) {
        cJSON_AddItemToObject(pr, "resources", cJSON_Duplicate(res, 1));
        cJSON_AddNumberToObject(pr, "resource_count", cJSON_GetArraySize(res));
      }
    }
    char *pj = cJSON_PrintUnformatted(pr);

    char summary[320];
    const char *ty = jo_sv(d, "type"), *pub = jo_sv(d, "publisher");
    if (ty && pub) snprintf(summary, sizeof summary, "%s · %s", ty, pub);
    else if (ty)   snprintf(summary, sizeof summary, "%s", ty);
    else if (pub)  snprintf(summary, sizeof summary, "%s", pub);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = id ? id : title;
    it.title           = title ? title : id;
    it.summary         = summary[0] ? summary : NULL;
    it.link            = jo_sv(d, "page_url");
    it.lang            = "fr";
    it.published_at    = jo_sv(d, "created_at");
    it.record_type     = "mobility-dataset";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"france\",\"open-data\",\"catalogue\"]";
    /* covered_area carries no geometry → no geo (R2) */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[transport-data-gouv-fr-datasets] emitted %d\n", n);
  return 0;
}

static const source_def trn_transport_data_gouv_fr_def = {
  .id = "transport-data-gouv-fr-datasets", .collector = "transport",
  .name = "transport.data.gouv.fr — French national mobility data registry",
  .update_interval_sec = 86400, .run = run,
  .category = "transport", .type = "api",
  .url = TDG_URL,
  .description = "France's Point d'Acces National to mobility data: every registered GTFS, GTFS-RT, GBFS, SIRI, carpooling and EV-charging dataset with publisher, covered area, licence and direct download URLs.",
  .license = "Licence Ouverte v2.0 (transport.data.gouv.fr / Etalab); per-dataset licences carried in the payload.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_transport_data_gouv_fr_def)
