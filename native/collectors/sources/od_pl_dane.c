/* Poland dane.gov.pl national open-data API.
 * Endpoint: https://api.dane.gov.pl/1.4/datasets?page=1&per_page=20
 * The only major EU national portal that is neither CKAN nor OpenDataSoft:
 * this is JSON:API, so the payload is data[] with {type,id,attributes,
 * relationships} and every real field lives under attributes.
 * Emits, per dataset: title, notes, slug, created/modified, view and download
 * counts, licence conditions, the owning institution id and the related
 * resources collection URL - all read out of the response. Keyless.
 * Licence: Otwarte Dane; per-dataset licences in attributes.license_*. */
#include "od_shared.c"

#define SID "opendata-pl-dane"
static const char *URL =
  "https://api.dane.gov.pl/1.4/datasets?page=1&per_page=20";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 30000);
  if (!doc) return od_rc(SID, -1);
  const cJSON *arr = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); return od_rc(SID, -1); }

  int n = 0;
  const cJSON *d;
  cJSON_ArrayForEach(d, arr) {
    if (!cJSON_IsObject(d)) continue;
    const cJSON *at = cJSON_GetObjectItem(d, "attributes");
    if (!cJSON_IsObject(at)) continue;
    const char *title = od_s(at, "title");
    const char *slug  = od_s(at, "slug");
    if (!title) title = slug;
    if (!title) continue;
    const char *dsid = od_s(d, "id");
    const char *mod  = od_s(at, "modified");
    if (!mod) mod = od_s(at, "created");

    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_copy_scalars(props, at);          /* every attribute upstream returned */
    od_put_s(props, "dataset_id", dsid);

    /* relationships: owning institution + the resources sub-collection URL */
    const cJSON *rel = cJSON_GetObjectItem(d, "relationships");
    const char *reslink = NULL;
    if (cJSON_IsObject(rel)) {
      const cJSON *inst = cJSON_GetObjectItem(rel, "institution");
      const cJSON *idat = cJSON_IsObject(inst)
                            ? cJSON_GetObjectItem(inst, "data") : NULL;
      if (cJSON_IsObject(idat)) od_put_s(props, "institution_id",
                                         od_s(idat, "id"));
      const cJSON *rsc = cJSON_GetObjectItem(rel, "resources");
      const cJSON *lnk = cJSON_IsObject(rsc)
                           ? cJSON_GetObjectItem(rsc, "links") : NULL;
      if (cJSON_IsObject(lnk)) {
        reslink = od_s(lnk, "related");
        od_put_s(props, "resources_url", reslink);
      }
    }
    const cJSON *links = cJSON_GetObjectItem(d, "links");
    const char *self = cJSON_IsObject(links) ? od_s(links, "self") : NULL;

    intel_item it = {0};
    it.title = title;
    it.remote_key = dsid;
    it.published_at = mod;
    it.link = self ? self : reslink;      /* upstream-supplied URLs only */
    it.lang = "pl";
    it.record_type = "opendata-dataset";
    it.tags_json = "[\"opendata\",\"pl\"]";
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  return od_rc(SID, n);
}

static const source_def od_pl_dane_def = {
  .id = SID, .collector = "government",
  .name = "Poland dane.gov.pl open-data API",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://api.dane.gov.pl/1.4/datasets?page=1&per_page=20",
  .description = "Polish national open-data portal (JSON:API): dataset title, notes, institution and resource collection per record",
  .license = "Otwarte Dane; per-dataset licences in attributes.license_*",
  .free_tier = 1,
};
REGISTER_SOURCE(od_pl_dane_def)
