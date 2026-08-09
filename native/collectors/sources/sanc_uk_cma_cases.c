/* UK CMA competition and merger cases (GOV.UK Search API).
 *
 * Endpoint : https://www.gov.uk/api/search.json
 *              ?filter_content_store_document_type=cma_case&count=100&order=-public_timestamp
 * Format   : JSON, {"results":[…], "total":…}.
 * Verified : HTTP 200, total 2,572 cases; newest "Healthcare Ireland / Hutchinson Homes merger
 *            inquiry" (format cma_case, link /cma-cases/healthcare-ireland-slash-hutchinson-
 *            homes-merger-inquiry, public_timestamp 2026-07-31T11:00:46Z) whose description
 *            names both companies.
 * Keyless  : yes. The GOV.UK Search API is public and asks only for a descriptive
 *            User-Agent, which sanc_http_get sends.
 * Emits    : case title, the CMA's own description (this is where the companies are named),
 *            the case URL on GOV.UK, the publication timestamp and the publishing organisation.
 * Geometry : NONE (R2).
 * Licence  : Open Government Licence v3.0.
 *
 * Notes    : the generic filter_organisations= filter returns staff-pay spreadsheets and other
 *            noise; filter_content_store_document_type=cma_case is what isolates actual cases.
 *            The same API with document types such as utaac_decision or
 *            employment_tribunal_decision opens further legal registers from one collector.
 *            `link` is site-relative and is expanded to an absolute www.gov.uk URL.
 */
#include "sanc_common.inc"

#define CMA_URL "https://www.gov.uk/api/search.json" \
                "?filter_content_store_document_type=cma_case&count=100&order=-public_timestamp"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = sanc_http_json(ctx, CMA_URL, NULL, 40000, "uk-cma-cases");
  if (!doc) return -1;

  cJSON *results = cJSON_GetObjectItem(doc, "results");
  const cJSON *total = cJSON_GetObjectItem(doc, "total");
  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, results) {
    const char *title = jo_sv(r, "title");
    const char *path = jo_sv(r, "link");
    if (!title) continue;

    const char *desc = jo_sv(r, "description");
    const char *ts = jo_sv(r, "public_timestamp");
    const char *fmt = jo_sv(r, "format");

    char orgs[256];
    orgs[0] = 0;
    {
      size_t k = 0;
      const cJSON *o;
      cJSON_ArrayForEach(o, cJSON_GetObjectItem(r, "organisations")) {
        const char *on = jo_sv(o, "title");
        if (!on) continue;
        size_t need = strlen(on) + (k ? 2 : 0);
        if (k + need >= sizeof orgs - 1) break;
        if (k) { strcpy(orgs + k, ", "); k += 2; }
        strcpy(orgs + k, on);
        k += strlen(on);
      }
    }

    char link[512] = {0};
    if (path)
      snprintf(link, sizeof link, "%s%s",
               strncmp(path, "http", 4) == 0 ? "" : "https://www.gov.uk", path);

    char summary[512];
    sanc_trunc(summary, sizeof summary, desc ? desc : "");
    sanc_collapse_ws(summary);

    cJSON *body = cJSON_CreateObject();
    sanc_add(body, "case", title);
    sanc_add(body, "description", desc);
    sanc_add(body, "document_type", fmt);
    sanc_add(body, "published", ts);
    if (orgs[0]) sanc_add(body, "organisations", orgs);
    sanc_add(body, "url", link[0] ? link : NULL);
    char *bj = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    cJSON *props = cJSON_CreateObject();
    sanc_add(props, "document_type", fmt);
    sanc_add(props, "published", ts);
    if (orgs[0]) sanc_add(props, "organisations", orgs);
    sanc_add(props, "gov_uk_path", path);
    if (cJSON_IsNumber(total))
      cJSON_AddNumberToObject(props, "upstream_total", total->valuedouble);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key = path ? path : title;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.body = bj;
    it.link = link[0] ? link : NULL;
    it.lang = "en";
    it.published_at = ts;
    it.record_type = "uk-cma-case";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"enforcement\",\"competition\",\"merger\",\"uk\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj);
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[uk-cma-cases] emitted %d\n", n);
  return 0;
}

static const source_def sanc_uk_cma_cases_def = {
  .id = "uk-cma-cases", .collector = "sanctions",
  .name = "UK CMA competition and merger cases",
  .update_interval_sec = 21600, .run = run,
  .category = "government", .type = "api",
  .url = CMA_URL,
  .description = "Live register of UK competition enforcement: merger inquiries, cartel investigations and market studies, with the companies named in the case description.",
  .license = "Open Government Licence v3.0.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_uk_cma_cases_def)
