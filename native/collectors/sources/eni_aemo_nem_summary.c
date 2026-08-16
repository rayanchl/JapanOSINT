/* AEMO National Electricity Market summary (Australia).
 * Endpoint: https://aemo.com.au/aemo/apps/api/report/ELEC_NEM_SUMMARY (keyless)
 * Emits one row per NEM region (NSW1, QLD1, SA1, TAS1, VIC1):
 *   price (UNIT: AUD/MWh), total_demand_mw / scheduled_generation_mw /
 *   semi_scheduled_generation_mw / net_interchange_mw (UNIT: MW), plus the
 *   parsed interconnector flows.
 *
 * DOUBLE-ENCODED FIELD: INTERCONNECTORFLOWS is a JSON *string* containing an
 * escaped array — it must be parsed a SECOND time, which is done below; a
 * failure there drops the flows, never fabricates them.
 * PRICE NOTE: PRICE is AUD/MWh and can be negative or spike to the market cap
 * (~$16,600/MWh) — both are real and neither is clamped.
 * TIME NOTE: SETTLEMENTDATE carries no timezone marker; it is AEST (UTC+10),
 * recorded as such in the properties.
 * Licence: AEMO market data, published for public use with attribution. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "aemo-nem-summary"

static void addnum(cJSON *p, const cJSON *o, const char *k, const char *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (cJSON_IsNumber(v)) cJSON_AddNumberToObject(p, out, v->valuedouble);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = "https://aemo.com.au/aemo/apps/api/report/ELEC_NEM_SUMMARY";
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *arr = cJSON_GetObjectItem(doc, "ELEC_NEM_SUMMARY");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no ELEC_NEM_SUMMARY[]\n"); return -1; }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    const char *region = jo_sv(r, "REGIONID");
    const char *when = jo_sv(r, "SETTLEMENTDATE");
    if (!region) continue;
    cJSON *price = cJSON_GetObjectItem(r, "PRICE");
    cJSON *dem = cJSON_GetObjectItem(r, "TOTALDEMAND");
    if (!cJSON_IsNumber(price) && !cJSON_IsNumber(dem)) continue;  /* no data */

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "region", region);
    if (when) {
      cJSON_AddStringToObject(p, "settlement_date_local", when);
      cJSON_AddStringToObject(p, "timezone", "AEST (UTC+10), no offset in feed");
    }
    if (cJSON_IsNumber(price)) {
      cJSON_AddNumberToObject(p, "price", price->valuedouble);
      cJSON_AddStringToObject(p, "price_unit", "AUD/MWh");
    }
    addnum(p, r, "TOTALDEMAND", "total_demand_mw");
    addnum(p, r, "NETINTERCHANGE", "net_interchange_mw");
    addnum(p, r, "SCHEDULEDGENERATION", "scheduled_generation_mw");
    addnum(p, r, "SEMISCHEDULEDGENERATION", "semi_scheduled_generation_mw");
    cJSON_AddStringToObject(p, "power_unit", "MW");

    /* INTERCONNECTORFLOWS is a JSON STRING — parse it a second time. */
    const char *icf = jo_sv(r, "INTERCONNECTORFLOWS");
    if (icf) {
      cJSON *flows = cJSON_Parse(icf);
      if (flows) cJSON_AddItemToObject(p, "interconnector_flows", flows);
      /* on parse failure: no flows key at all, never a placeholder */
    }
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[96], title[224];
    snprintf(key, sizeof key, "%s|%s", region, when ? when : "");
    if (cJSON_IsNumber(price) && cJSON_IsNumber(dem))
      snprintf(title, sizeof title, "NEM %s: %.2f AUD/MWh, demand %.0f MW",
               region, price->valuedouble, dem->valuedouble);
    else
      snprintf(title, sizeof title, "NEM %s dispatch summary", region);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = url;
    row.record_type     = "grid-dispatch";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"market\",\"australia\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_aemo_nem_summary_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "AEMO National Electricity Market summary (Australia)",
  .update_interval_sec = 300, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://aemo.com.au/aemo/apps/api/report/ELEC_NEM_SUMMARY",
  .description = "Live 5-minute dispatch price (AUD/MWh), demand, generation and interconnector flows (MW) for each Australian NEM region.",
  .license = "AEMO market data, published for public use with attribution; no key.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_aemo_nem_summary_def)
