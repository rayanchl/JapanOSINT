/* Poland dane.gov.pl national open-data API.
 * Endpoint: https://api.dane.gov.pl/1.4/datasets?page=1&per_page=20
 * The only major EU national portal that is neither CKAN nor OpenDataSoft:
 * this is JSON:API, so the payload is data[] with {type,id,attributes,
 * relationships} and every real field lives under attributes.
 * Emits, per dataset: title, notes, slug, created/modified, view and download
 * counts, licence conditions, the owning institution id and the related
 * resources collection URL - all read out of the response. Keyless.
 * Licence: Otwarte Dane; per-dataset licences in attributes.license_*. */
#include "od_shared.inc"
#include "../../lib/pagewalk.h"   /* pw_walk() — house rule 2 paging */

#define SID "opendata-pl-dane"
static const char *URL =
  "https://api.dane.gov.pl/1.4/datasets?page=1&per_page=20";  /* exhaustive-ok: page=1 is where a pw_walk() STARTS — run() follows links.next / page=2,3,… and discloses the remainder */

/* One page of data[]. Returns #emitted and reports #records the page CONTAINED
 * through `seen` — pagewalk drives "did this page come back full" off the
 * latter, see lib/pagewalk.h. */
static int emit_page(const source_ctx *ctx, intel_sink *sink, const char *id,
                     cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)id; (void)ud;
  const cJSON *arr = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(arr)) return 0;
  *seen = cJSON_GetArraySize(arr);      /* records this page CONTAINED */

  /* The portal states the catalogue size in meta.count, which is not one of
   * the names lib/pagewalk.c recognises. Republish the upstream's OWN number
   * under a name it does read, so a truncation notice can carry a real
   * records_available. The value is copied, never computed (house rule 1). */
  const cJSON *meta = cJSON_GetObjectItem(doc, "meta");
  const cJSON *cnt  = cJSON_IsObject(meta) ? cJSON_GetObjectItem(meta, "count")
                                           : NULL;
  if (cJSON_IsNumber(cnt) && !cJSON_GetObjectItem(doc, "totalCount"))
    cJSON_AddNumberToObject(doc, "totalCount", cnt->valuedouble);

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
  return n;
}

/* House rule 2: dane.gov.pl is JSON:API, so the envelope carries links.next and
 * the URL already carries page=/per_page=. pw_walk follows the server's own
 * next link (falling back to advancing the author's page= parameter) to the
 * JO_PAGE_MAX ceiling, and discloses whatever is still behind it as a
 * collector-truncation-notice. per_page=20 is the author's page size and is
 * left exactly as it was. */
static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = pw_walk(ctx, sink, SID, URL, pw_fetch_json, emit_page, NULL);
  return od_rc(SID, n);
}

static const source_def od_pl_dane_def = {
  .id = SID, .collector = "government",
  .name = "Poland dane.gov.pl open-data API",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://api.dane.gov.pl/1.4/datasets?page=1&per_page=20",  /* exhaustive-ok: source_def metadata, not a fetch; run() walks it with pw_walk() */
  .description = "Polish national open-data portal (JSON:API): dataset title, notes, institution and resource collection per record",
  .license = "Otwarte Dane; per-dataset licences in attributes.license_*",
  .free_tier = 1,
};
REGISTER_SOURCE(od_pl_dane_def)
