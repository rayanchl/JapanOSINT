/* openFDA drug enforcement (recall) reports.
 *
 * Endpoint : https://api.fda.gov/drug/enforcement.json?limit=100
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
 * Notes    : the URL is used EXACTLY as verified. meta.results.total supports skip/limit paging
 *            (skip caps at 25,000; walk further with date-range filters) and
 *            device/enforcement.json and food/enforcement.json share this schema if the fleet
 *            wants them later.
 */
#include "sanc_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = sanc_http_json(ctx, "https://api.fda.gov/drug/enforcement.json?limit=100",
                              NULL, 40000, "openfda-enforcement");
  if (!doc) return -1;

  const cJSON *meta = cJSON_GetObjectItem(
      cJSON_GetObjectItem(doc, "meta"), "results");
  const cJSON *total = cJSON_GetObjectItem(meta, "total");

  cJSON *results = cJSON_GetObjectItem(doc, "results");
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
  cJSON_Delete(doc);
  fprintf(stderr, "[openfda-enforcement] emitted %d\n", n);
  return 0;
}

static const source_def sanc_openfda_enforcement_def = {
  .id = "openfda-enforcement-reports", .collector = "sanctions",
  .name = "openFDA drug enforcement (recall) reports",
  .update_interval_sec = 86400, .run = run,
  .category = "safety", .type = "api",
  .url = "https://api.fda.gov/drug/enforcement.json?limit=100",
  .description = "FDA regulatory enforcement reports naming the firm, the hazard classification and the reason for the recall — a corporate-conduct signal alongside financial-regulator actions.",
  .license = "openFDA: public domain data, no warranty. Do not rely on openFDA to make decisions regarding medical care.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sanc_openfda_enforcement_def)
