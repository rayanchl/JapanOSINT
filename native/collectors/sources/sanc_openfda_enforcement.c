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
 * Notes    : meta.results.total supports skip/limit paging — which this collector now DOES,
 *            see the comment on OPENFDA_URL (skip caps at 25,000; past that the walk needs
 *            date-range filters, and the row would say so in its truncation notice) — and
 *            device/enforcement.json and food/enforcement.json share this schema if the fleet
 *            wants them later.
 */
#include "sanc_common.inc"
#include "lib/pagewalk.h"

/* PAGED, and the Notes above are what made it obvious: this row already KNEW
 * meta.results.total was 17,832 and that skip/limit paging existed, and still
 * stored the first 100. Measured 2026-10-18: the total is now 18,010, `skip` is
 * honoured (skip=100 shares no record with skip=0) and `limit=1000` is
 * openFDA's documented maximum and is served (1.6 MB a page). 18,010 / 1,000 =
 * 19 pages, inside the 20-page ceiling, so one daily run now takes the whole
 * table instead of 0.55% of it. `skip=0` is written into the URL deliberately:
 * pw_walk advances a parameter the author wrote and never invents one. If the
 * table grows past 20,000 the ceiling bites and the walk files a truncation
 * notice naming it, which is the honest outcome rather than a silent 20,000. */
#define OPENFDA_URL "https://api.fda.gov/drug/enforcement.json?limit=1000&skip=0"

static cJSON *fda_fetch(const source_ctx *c, const char *url, void *ud) {
  (void)ud;
  return sanc_http_json(c, url, NULL, 40000, "openfda-enforcement");
}

/* pw_emit_fn. `seen` is the page's record count, not the emitted count: a
 * record with neither firm nor product description is skipped below, and
 * reporting the emitted count would make a full page look short — stopping the
 * walk and suppressing its own truncation notice (lib/pagewalk.h). */
static int fda_emit_page(const source_ctx *ctx, intel_sink *sink,
                         const char *sid, cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)sid; (void)ud;
  const cJSON *meta = cJSON_GetObjectItem(
      cJSON_GetObjectItem(doc, "meta"), "results");
  const cJSON *total = cJSON_GetObjectItem(meta, "total");

  cJSON *results = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(results)) { *seen = 0; return 0; }
  *seen = cJSON_GetArraySize(results);

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
  int n = pw_walk(ctx, sink, "openfda-enforcement", OPENFDA_URL,
                  fda_fetch, fda_emit_page, NULL);
  return n < 0 ? -1 : 0;
}

static const source_def sanc_openfda_enforcement_def = {
  .id = "openfda-enforcement-reports", .collector = "sanctions",
  .name = "openFDA drug enforcement (recall) reports",
  .update_interval_sec = 86400, .run = run,
  .category = "safety", .type = "api",
  /* The scanner reads a `skip=` in any URL literal as a fetch site. This one is
   * source_def.url — the endpoint this row documents — and the request is made
   * by run() above, which walks it with pw_walk. Measured 2026-10-18: 18,010
   * reports across 19 pages, = meta.results.total. */
  .url = "https://api.fda.gov/drug/enforcement.json?limit=1000&skip=0",  /* exhaustive-ok: metadata field, not a fetch; run() walks it with pw_walk */
  .description = "FDA regulatory enforcement reports naming the firm, the hazard classification and the reason for the recall — a corporate-conduct signal alongside financial-regulator actions.",
  .license = "openFDA: public domain data, no warranty. Do not rely on openFDA to make decisions regarding medical care.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_openfda_enforcement_def)
