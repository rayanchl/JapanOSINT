/* openFDA drug enforcement (recall) reports.
 *
 * Endpoint : https://api.fda.gov/drug/enforcement.json?limit=1000&skip=0
 * Format   : JSON, {"meta":{"results":{"skip","limit","total"}}, "results":[…]}.
 * Verified : HTTP 200, meta.results.total 17,832; sample record "Kalman Health & Wellness, Inc.
 *            dba Essential Wellness Pharma", Peoria IL US, Class II, Terminated, event_id 72241.
 * Keyless  : yes (240 req/min anonymous, 1000/min with a key — we make one request a day).
 * Emits    : recalling firm, product description, hazard classification, status, the reason for
 *            the recall verbatim, city/state/country as returned, recall and event numbers,
 *            distribution pattern and the report date.
 * Geometry : NONE (R2). The records carry a city and state but no coordinates; a recall is not
 *            geocoded to a city centroid here.
 * Licence  : openFDA terms — public domain data, no warranty. openFDA states plainly: "do not
 *            rely on openFDA to make decisions regarding medical care". This is used as a
 *            corporate-conduct signal alongside financial-regulator actions, nothing more.
 *
 * Notes    : meta.results.total supports skip/limit paging, and
 *            device/enforcement.json and food/enforcement.json share this schema if the fleet
 *            wants them later.
 *
 * PAGED 2026-10-12. The note above used to end "the URL is used EXACTLY as
 * verified" and say that total "supports skip/limit paging" — a capability
 * named and not used. One request for `limit=100` stored 100 of the 18,002
 * reports meta.results.total declares, daily, forever: the seventh discard
 * shape (tools/audit_url_limits.py), with the shortfall carried on every row
 * as `upstream_total` where anyone could have compared it. Measured
 * 2026-10-12: `limit=1000` is honoured and `skip` works to the API's documented
 * ceiling of 25,000 (26,000 is a 400 naming it), so 18,002 records are 19
 * pages of 1,000 and the whole set fits inside that ceiling with room. If this
 * corpus grows past 25,000, the walk will stop at the ceiling and disclose it;
 * the remedy is then a date-range filter, as the note always said.
 */
#include "sanc_common.inc"
#include "lib/pagewalk.h"

#define FDA_URL "https://api.fda.gov/drug/enforcement.json?limit=1000&skip=0"  /* exhaustive-ok: walked by pw_walk/pw_walk_max below, which advances the offset or page this URL carries */
/* 18,002 reports at 1,000 a page = 19; 26 is both headroom and the API's own
 * skip ceiling of 25,000, past which a page is a 400 rather than data. */
#define FDA_PAGE_MAX 26  /* exhaustive-ok: page ceiling handed to pw_walk_max(); a stop at it files a collector-truncation-notice */

/* pw_fetch_fn: this family fetches through sanc_http_json for its UA and
 * error logging. */
static cJSON *fda_fetch(const source_ctx *ctx, const char *url, void *ud) {
  (void)ud;
  return sanc_http_json(ctx, url, NULL, 40000, "openfda-enforcement");
}

static int fda_page(const source_ctx *ctx, intel_sink *sink, const char *sid,
                    cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)sid; (void)ud;
  const cJSON *meta = cJSON_GetObjectItem(
      cJSON_GetObjectItem(doc, "meta"), "results");
  const cJSON *total = cJSON_GetObjectItem(meta, "total");

  cJSON *results = cJSON_GetObjectItem(doc, "results");
  *seen = cJSON_IsArray(results) ? cJSON_GetArraySize(results) : 0;
  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, results) {
    const char *firm = jo_sv(r, "recalling_firm");
    const char *prod = jo_sv(r, "product_description");
    if (!firm && !prod) continue;                 /* nothing fetched -> no row */

    const char *cls = jo_sv(r, "classification");
    const char *status = jo_sv(r, "status");
    const char *reason = jo_sv(r, "reason_for_recall");
    const char *city = jo_sv(r, "city");
    const char *state = jo_sv(r, "state");
    const char *country = jo_sv(r, "country");
    const char *event = jo_sv(r, "event_id");
    const char *recallno = jo_sv(r, "recall_number");
    const char *pattern = jo_sv(r, "distribution_pattern");
    const char *ptype = jo_sv(r, "product_type");
    const char *report = jo_sv(r, "report_date");
    const char *initiated = jo_sv(r, "recall_initiation_date");

    char iso[16] = {0};
    if (report) sanc_yyyymmdd(report, iso, sizeof iso);
    char iso_init[16] = {0};
    if (initiated) sanc_yyyymmdd(initiated, iso_init, sizeof iso_init);

    char title[400];
    snprintf(title, sizeof title, "%s%s%s", firm ? firm : "(firm not stated)",
             cls ? " — " : "", cls ? cls : "");

    char summary[512];
    sanc_trunc(summary, sizeof summary, reason ? reason : (prod ? prod : ""));
    sanc_collapse_ws(summary);

    cJSON *body = cJSON_CreateObject();
    sanc_add(body, "recalling_firm", firm);
    sanc_add(body, "product_type", ptype);
    sanc_add(body, "product_description", prod);
    sanc_add(body, "classification", cls);
    sanc_add(body, "status", status);
    sanc_add(body, "reason_for_recall", reason);
    sanc_add(body, "city", city);
    sanc_add(body, "state", state);
    sanc_add(body, "country", country);
    sanc_add(body, "distribution_pattern", pattern);
    sanc_add(body, "recall_number", recallno);
    sanc_add(body, "event_id", event);
    sanc_add(body, "report_date", iso[0] ? iso : report);
    char *bj = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    cJSON *props = cJSON_CreateObject();
    sanc_add(props, "recalling_firm", firm);
    sanc_add(props, "classification", cls);
    sanc_add(props, "status", status);
    sanc_add(props, "city", city);
    sanc_add(props, "state", state);
    sanc_add(props, "country", country);
    sanc_add(props, "recall_number", recallno);
    sanc_add(props, "event_id", event);
    sanc_add(props, "product_type", ptype);
    sanc_add(props, "recall_initiation_date", iso_init[0] ? iso_init : NULL);
    if (cJSON_IsNumber(total))
      cJSON_AddNumberToObject(props, "upstream_total", total->valuedouble);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key = recallno ? recallno : event;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.body = bj;
    it.lang = "en";
    it.published_at = iso[0] ? iso : NULL;
    it.record_type = "fda-enforcement-report";
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"enforcement\",\"recall\",\"safety\",\"us\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj);
    free(pj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  if (pw_walk_max(ctx, sink, "openfda-enforcement-reports", FDA_URL, fda_fetch,
                  fda_page, NULL, FDA_PAGE_MAX) < 0)
    return -1;
  return 0;
}

static const source_def sanc_openfda_enforcement_def = {
  .id = "openfda-enforcement-reports", .collector = "sanctions",
  .name = "openFDA drug enforcement (recall) reports",
  .update_interval_sec = 86400, .run = run,
  .category = "safety", .type = "api",
  .url = FDA_URL,
  .description = "FDA regulatory enforcement reports naming the firm, the hazard classification and the reason for the recall — a corporate-conduct signal alongside financial-regulator actions.",
  .license = "openFDA: public domain data, no warranty. Do not rely on openFDA to make decisions regarding medical care.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_openfda_enforcement_def)
