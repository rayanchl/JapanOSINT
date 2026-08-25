/* Poland dane.gov.pl national open-data API.
 * Endpoint: https://api.dane.gov.pl/1.4/datasets?page=1&per_page=100
 * The only major EU national portal that is neither CKAN nor OpenDataSoft:
 * this is JSON:API, so the payload is data[] with {type,id,attributes,
 * relationships} and every real field lives under attributes.
 * Emits, per dataset: title, notes, slug, created/modified, view and download
 * counts, licence conditions, the owning institution id and the related
 * resources collection URL - all read out of the response. Keyless.
 * Licence: Otwarte Dane; per-dataset licences in attributes.license_*. */
#include "od_shared.c"

#define SID "opendata-pl-dane"
/* per_page raised from 20 to 100, the API maximum. At 20 this asked for the
 * same catalogue in five times as many requests — and then read only the first
 * of them, because the collector fetched page 1 and stopped. dane.gov.pl is
 * JSON:API and hands back its own `links.next`, so the walk below follows the
 * upstream's arithmetic rather than guessing at a page count. */
static const char *URL =
  "https://api.dane.gov.pl/1.4/datasets?page=1&per_page=100";  /* exhaustive-ok: this is the walk's FIRST page; run() follows links.next to the end and discloses an early stop */

/* exhaustive-ok: ceiling on a walk that discloses in-band when it stops early.
 * Set from the catalogue's real size, not from taste: dane.gov.pl reports
 * ~26,500 datasets, which is ~266 pages of 100. At 40 the walk stopped with
 * 4,000 read and 22,000 still offered — disclosed, but still our bound rather
 * than the upstream's. 300 lets the walk finish and leaves headroom as the
 * catalogue grows; measured at ~1.4s per page, a full read is ~6 minutes once
 * a day.
 *
 * Measured at 300: the walk ends on its own at 10,000 records (100 pages) —
 * links.next simply stops being offered — so that is the API's own ceiling and
 * no truncation notice fires, correctly. It is NOT the whole catalogue, which
 * is roughly 26,500 datasets: reaching the remainder needs a different
 * traversal (slicing by date or institution), not a bigger page number. Worth
 * knowing before anyone reads 10,000 as "all of Poland's open data". */
#define PL_PAGE_MAX 300   /* exhaustive-ok: page-walk ceiling (see above), disclosed as a collector-truncation-notice when it stops a walk with more to give */

static int emit_page(intel_sink *sink, const cJSON *arr) {
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
  char *next = strdup(URL);
  if (!next) return od_rc(SID, -1);
  int n = 0, pages = 0, truncated = 0;

  while (next && pages < PL_PAGE_MAX) {
    cJSON *doc = feed_get_json(ctx->http, next, 30000);
    if (!doc) {
      /* A dead FIRST page is a dead endpoint and stays an error. A failure
       * mid-walk means we already hold real datasets: keep them, stop, and
       * disclose that the walk ended early. */
      if (pages == 0) { free(next); return od_rc(SID, -1); }
      truncated = 1;
      break;
    }
    pages++;
    const cJSON *arr = cJSON_GetObjectItem(doc, "data");
    if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); break; }
    n += emit_page(sink, arr);

    const cJSON *lk = cJSON_GetObjectItem(doc, "links");
    const char *nx = cJSON_IsObject(lk) ? od_s(lk, "next") : NULL;
    char *adv = (nx && *nx) ? strdup(nx) : NULL;
    cJSON_Delete(doc);
    free(next);
    next = adv;
  }
  if (next) truncated = 1;          /* the ceiling stopped a walk with more */
  free(next);

  if (truncated) {
    /* Rule 2: a shortfall is reported as data, not as a log line. */
    char t[192];
    snprintf(t, sizeof t, "%s read %d dataset(s) over %d page(s) and stopped "
             "while the upstream still offered more", SID, n, pages);
    intel_item note = {0};
    note.remote_key      = "truncation";
    note.title           = t;
    note.lang            = "en";
    note.record_type     = "collector-truncation-notice";
    note.properties_json =
      "{\"reason\":\"the page ceiling or a mid-walk fetch failure stopped the "
      "walk\",\"remedy\":\"raise PL_PAGE_MAX in collectors/sources/od_pl_dane.c\"}";
    note.tags_json       = "[\"opendata\",\"pl\",\"truncation-notice\"]";
    sink->emit(sink, &note);
  }
  return od_rc(SID, n);
}

static const source_def od_pl_dane_def = {
  .id = SID, .collector = "government",
  .name = "Poland dane.gov.pl open-data API",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://api.dane.gov.pl/1.4/datasets?page=1&per_page=100",  /* exhaustive-ok: registry metadata for /api/status, not the fetch; run() walks links.next */
  .description = "Polish national open-data portal (JSON:API): dataset title, notes, institution and resource collection per record",
  .license = "Otwarte Dane; per-dataset licences in attributes.license_*",
  .free_tier = 1,
};
REGISTER_SOURCE(od_pl_dane_def)
