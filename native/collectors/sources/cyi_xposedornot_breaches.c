/* collectors/sources/cyi_xposedornot_breaches.c
 * XposedOrNot breach index — which organisation, which domain, when, how many
 * records and which data classes were exposed. Breach METADATA only.
 * Endpoint: https://api.xposedornot.com/v1/breaches                   (keyless)
 * parse_notes: "Rows in exposedBreaches[] ... Use ONLY this catalogue endpoint
 * — the per-email lookup paths are out of scope for this platform." This
 * collector therefore touches nothing but the catalogue: no credential or
 * per-account lookup is performed, and no credential content is stored.
 * Emits: breachID, domain, industry, breachedDate, addedDate, exposedRecords,
 * exposedData[], breachType, verified, exposureDescription.
 * No coordinates -> has_geo 0 (R2).
 * Licence: XposedOrNot free API; breach metadata only.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://api.xposedornot.com/v1/breaches"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 40000);
  if (!doc) { fprintf(stderr, "[xposedornot-breaches] fetch/parse failed\n"); return -1; }

  const cJSON *rows = cJSON_GetObjectItem(doc, "exposedBreaches");
  if (!cJSON_IsArray(rows) && cJSON_IsArray(doc)) rows = doc;

  int n = 0;
  const cJSON *b;
  cJSON_ArrayForEach(b, rows) {
    const char *id = jo_sv(b, "breachID");
    if (!id) continue;
    const char *domain = jo_sv(b, "domain");
    const char *industry = jo_sv(b, "industry");
    const char *breached = jo_sv(b, "breachedDate");
    const char *added = jo_sv(b, "addedDate");
    const char *btype = jo_sv(b, "breachType");
    const char *desc = jo_sv(b, "exposureDescription");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "breach_id", id);
    if (domain)   cJSON_AddStringToObject(p, "domain", domain);
    if (industry) cJSON_AddStringToObject(p, "industry", industry);
    if (breached) cJSON_AddStringToObject(p, "breached_date", breached);
    if (added)    cJSON_AddStringToObject(p, "added_date", added);
    if (btype)    cJSON_AddStringToObject(p, "breach_type", btype);
    double recs = 0;
    if (jo_numf(b, "exposedRecords", &recs))
      cJSON_AddNumberToObject(p, "exposed_records", recs);
    const cJSON *ed = cJSON_GetObjectItem(b, "exposedData");
    if (cJSON_IsArray(ed) && cJSON_GetArraySize(ed) > 0)
      cJSON_AddItemToObject(p, "exposed_data_classes", cJSON_Duplicate(ed, 1));
    const cJSON *ver = cJSON_GetObjectItem(b, "verified");
    if (cJSON_IsBool(ver)) cJSON_AddBoolToObject(p, "verified", cJSON_IsTrue(ver));
    else { const char *vs = jo_sv(b, "verified"); if (vs) cJSON_AddStringToObject(p, "verified", vs); }
    cJSON_AddStringToObject(p, "scope", "breach-metadata-only");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char summary[384];
    snprintf(summary, sizeof summary, "%s%s%s%.0f records exposed%s%s",
             domain ? domain : "", domain ? " · " : "",
             industry ? industry : "", recs,
             breached ? " · breached " : "", breached ? breached : "");

    char link[256];
    snprintf(link, sizeof link, "https://xposedornot.com/xposed");

    intel_item it = {0};
    it.remote_key      = id;
    it.title           = id;
    it.summary         = summary;
    it.body            = desc;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = breached;
    it.record_type     = "data-breach";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"breach\",\"xposedornot\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[xposedornot-breaches] emitted %d\n", n);
  return 0;
}

static const source_def cyi_xposedornot_breaches_def = {
  .id = "xposedornot-breaches", .collector = "cyber",
  .name = "XposedOrNot breach index",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "api", .url = CYI_URL,
  .description = "Keyless breach-notification index: organisation, domain, date, record count and exposed data classes — breach metadata only, no credential content.",
  .license = "XposedOrNot free API; breach metadata only.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_xposedornot_breaches_def)
