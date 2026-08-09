/* Federal Register — OFAC sanctions actions (agency-filtered chronological feed).
 *
 * Endpoint : https://www.federalregister.gov/api/v1/documents.json
 *              ?conditions[agencies][]=foreign-assets-control-office&per_page=20&order=newest
 *            (query string %-encoded exactly as verified).
 * Format   : JSON, {"count","total_pages","next_page_url","results":[…]}.
 * Verified : HTTP 200, count 2,598 documents; newest "Notice of OFAC Sanctions Action"
 *            (Notice, 2026-07-30, document 2026-15356) whose abstract names the persons added
 *            to the SDN List.
 * Keyless  : yes — the Federal Register API asks for no key and publishes no quota.
 * Emits    : document title, type (Notice/Rule/Proposed Rule), the abstract verbatim (it is
 *            what names the designations), publication date, Federal Register document number,
 *            the html_url and pdf_url, and the publishing agencies.
 * Geometry : NONE (R2).
 * Licence  : US Government work, public domain.
 *
 * Notes    : this is the legal instrument behind each OFAC designation and de-listing,
 *            published the day it takes effect — the authoritative timeline of who was added
 *            or removed and under which programme. Same API host as an existing free-text
 *            pivot collector but a DIFFERENT query shape: an agency-filtered chronological
 *            feed, not a name lookup. Swapping the agency slug (securities-and-exchange-
 *            commission, industry-and-security-bureau) widens coverage from one collector.
 */
#include "sanc_common.inc"

#define FEDREG_URL \
  "https://www.federalregister.gov/api/v1/documents.json" \
  "?conditions%5Bagencies%5D%5B%5D=foreign-assets-control-office&per_page=20&order=newest"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = sanc_http_json(ctx, FEDREG_URL, NULL, 30000, "fedreg-ofac");
  if (!doc) return -1;

  cJSON *results = cJSON_GetObjectItem(doc, "results");
  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, results) {
    const char *title = jo_sv(r, "title");
    const char *docnum = jo_sv(r, "document_number");
    if (!title) continue;

    const char *type = jo_sv(r, "type");
    const char *abstract = jo_sv(r, "abstract");
    if (!abstract) abstract = jo_sv(r, "excerpts");
    const char *pubdate = jo_sv(r, "publication_date");
    const char *html = jo_sv(r, "html_url");
    const char *pdf = jo_sv(r, "pdf_url");

    cJSON *agencies = cJSON_CreateArray();
    const cJSON *a;
    cJSON_ArrayForEach(a, cJSON_GetObjectItem(r, "agencies")) {
      const char *an = jo_sv(a, "name");
      if (!an) an = jo_sv(a, "raw_name");
      if (an) cJSON_AddItemToArray(agencies, cJSON_CreateString(an));
      if (cJSON_GetArraySize(agencies) >= 6) break;
    }

    char summary[512];
    sanc_trunc(summary, sizeof summary, abstract ? abstract : "");
    sanc_collapse_ws(summary);

    cJSON *body = cJSON_CreateObject();
    sanc_add(body, "title", title);
    sanc_add(body, "type", type);
    sanc_add(body, "abstract", abstract);
    sanc_add(body, "publication_date", pubdate);
    sanc_add(body, "document_number", docnum);
    sanc_add(body, "pdf_url", pdf);
    cJSON_AddItemToObject(body, "agencies", cJSON_Duplicate(agencies, 1));
    char *bj = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    cJSON *props = cJSON_CreateObject();
    sanc_add(props, "document_number", docnum);
    sanc_add(props, "document_type", type);
    sanc_add(props, "publication_date", pubdate);
    sanc_add(props, "pdf_url", pdf);
    cJSON_AddItemToObject(props, "agencies", agencies);   /* transferred */
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key = docnum ? docnum : title;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.body = bj;
    it.link = html;
    it.lang = "en";
    it.published_at = pubdate;
    it.record_type = "fedreg-ofac-notice";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"sanctions\",\"ofac\",\"federal-register\",\"us\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj);
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[fedreg-ofac] emitted %d\n", n);
  return 0;
}

static const source_def sanc_fedreg_ofac_def = {
  .id = "fedreg-ofac-notices", .collector = "sanctions",
  .name = "Federal Register — OFAC sanctions actions",
  .update_interval_sec = 21600, .run = run,
  .category = "government", .type = "api",
  .url = FEDREG_URL,
  .description = "The legal instrument behind each OFAC designation and de-listing, published the day it takes effect — the authoritative timeline of who was added or removed and under which programme.",
  .license = "US Government work, public domain.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_fedreg_ofac_def)
