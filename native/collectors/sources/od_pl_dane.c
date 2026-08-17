/* Poland dane.gov.pl national open-data API.
 * Endpoint: https://api.dane.gov.pl/1.4/datasets?page=1&per_page=20, then the
 * server's own links.next until it stops offering one.
 * The only major EU national portal that is neither CKAN nor OpenDataSoft:
 * this is JSON:API, so the payload is data[] with {type,id,attributes,
 * relationships} and every real field lives under attributes.
 * Emits, per dataset: title, notes, slug, created/modified, view and download
 * counts, licence conditions, the owning institution id and the related
 * resources collection URL - all read out of the response. Keyless.
 * Licence: Otwarte Dane; per-dataset licences in attributes.license_*. */
#include "od_shared.c"
#include "lib/pager.h"
#include "lib/truncnotice.h"

#define SID "opendata-pl-dane"
/* per_page was 20 and page was pinned at 1, so Poland's national catalogue
 * arrived twenty datasets at a time, once a day, forever. Being JSON:API, the
 * response publishes its own `links.next` and a `meta.count`, so there is no
 * guessing to do: follow the link the server gives, stop when it stops giving
 * one, and say so if the budget runs out first.
 *
 * per_page STAYS AT 20, deliberately. Raising it to 100 to match the sibling row
 * in collectors/feed/generated/vsrc13_europe_data_1.c made this URL byte-identical
 * to that row's, i.e. two collectors fetching one endpoint — which is what
 * tools/lint_sources.py's dup-endpoint check exists to catch, and it caught it.
 * The page budget carries the coverage instead, so the walk reaches the same
 * number of datasets in more, smaller requests.
 *
 * REDUNDANCY FOR A HUMAN TO SETTLE: `eur-api-dane-pl` in that generated file is
 * now strictly dominated by this collector — same endpoint, same array, same
 * daily cadence, but page 1 only and without the JSON:API relationships (owning
 * institution, resource sub-collection) this one reads. Dropping it is an
 * inventory decision, not a refactor, so it is written down here rather than
 * taken unilaterally. */
static const char *URL =
  "https://api.dane.gov.pl/1.4/datasets?page=1&per_page=20";  /* exhaustive-ok: page 1 is where run()'s links.next walk STARTS */

#define PL_DANE_PAGES 100  /* per run; override with DANE_PL_PAGE_MAX */

static int emit_datasets(intel_sink *sink, const cJSON *arr) {
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

static int run(const source_ctx *ctx, intel_sink *sink) {
  int pages_max = PL_DANE_PAGES;
  const char *pm = getenv("DANE_PL_PAGE_MAX");
  if (pm && atoi(pm) > 0) pages_max = atoi(pm);

  char *page_url = strdup(URL);
  if (!page_url) return od_rc(SID, -1);

  int n = 0, pages = 0, truncated = 0;
  long available = -1;
  while (page_url && pages < pages_max) {
    cJSON *doc = feed_get_json(ctx->http, page_url, 30000);
    if (!doc) {
      /* Page 1 failing is a dead endpoint; failing mid-walk keeps the real
       * records already emitted and discloses the shortfall. */
      if (pages == 0) { free(page_url); return od_rc(SID, -1); }
      truncated = 1;
      break;
    }
    const cJSON *arr = cJSON_GetObjectItem(doc, "data");
    if (!cJSON_IsArray(arr)) {
      cJSON_Delete(doc);
      if (pages == 0) { free(page_url); return od_rc(SID, -1); }
      truncated = 1;
      break;
    }
    if (available < 0) available = pager_declared_total(doc);
    n += emit_datasets(sink, arr);

    char *next = pager_next_link(doc);      /* JSON:API links.next */
    cJSON_Delete(doc);
    free(page_url);
    page_url = next;
    pages++;
    if (page_url && pages >= pages_max) truncated = 1;
  }
  free(page_url);

  if (truncated)
    trunc_notice(sink, SID, URL, NULL, n, available,
                 "the per-run page budget stopped the walk while dane.gov.pl "
                 "was still publishing a next link",
                 "raise DANE_PL_PAGE_MAX");
  return od_rc(SID, n);
}

static const source_def od_pl_dane_def = {
  .id = SID, .collector = "government",
  .name = "Poland dane.gov.pl open-data API",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://api.dane.gov.pl/1.4/datasets?page=1&per_page=20",  /* exhaustive-ok: registry metadata, not the fetch */
  .description = "Polish national open-data portal (JSON:API): dataset title, notes, institution and resource collection per record",
  .license = "Otwarte Dane; per-dataset licences in attributes.license_*",
  .free_tier = 1,
};
REGISTER_SOURCE(od_pl_dane_def)
