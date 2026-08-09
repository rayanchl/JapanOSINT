/* Fraunhofer Energy-Charts day-ahead wholesale electricity price.
 * Endpoint: https://api.energy-charts.info/price?bzn=<zone>   (keyless)
 * Emits one row per quarter-hour step that carries a price: price
 * (unit taken from the response's own `unit` field, EUR/MWh) + bidding zone
 * + epoch timestamp.
 *
 * COLUMN-ALIGNMENT TRAP: unix_seconds[] and price[] are PARALLEL arrays and
 * must be indexed together — index i of price[] belongs to unix_seconds[i].
 * NEGATIVE PRICES ARE REAL and common; they are emitted, not filtered.
 * Licence: CC BY 4.0 (license_info: "... from Bundesnetzagentur | SMARD.de"). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "energy-charts-price"

static const char *ZONES[] = { "DE-LU", "FR", "NL", "PL", NULL };

static int collect_zone(const source_ctx *ctx, intel_sink *sink,
                        const char *bzn, int *fetched) {
  char url[192];
  snprintf(url, sizeof url, "https://api.energy-charts.info/price?bzn=%s", bzn);
  cJSON *doc = feed_get_json(ctx->http, url, 25000);
  if (!doc) return 0;
  *fetched = 1;

  cJSON *ts = cJSON_GetObjectItem(doc, "unix_seconds");
  cJSON *pr = cJSON_GetObjectItem(doc, "price");
  cJSON *un = cJSON_GetObjectItem(doc, "unit");
  if (!cJSON_IsArray(ts) || !cJSON_IsArray(pr)) { cJSON_Delete(doc); return 0; }
  const char *unit = (cJSON_IsString(un) && un->valuestring[0]) ? un->valuestring
                                                                : "EUR/MWh";
  int nts = cJSON_GetArraySize(ts), npr = cJSON_GetArraySize(pr);
  int lim = nts < npr ? nts : npr;

  int n = 0;
  for (int i = 0; i < lim; i++) {
    cJSON *v = cJSON_GetArrayItem(pr, i);
    cJSON *t = cJSON_GetArrayItem(ts, i);
    if (!cJSON_IsNumber(v) || !cJSON_IsNumber(t)) continue;   /* skip nulls */
    long long epoch = (long long)t->valuedouble;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "bidding_zone", bzn);
    cJSON_AddNumberToObject(p, "price", v->valuedouble);
    cJSON_AddStringToObject(p, "unit", unit);
    cJSON_AddNumberToObject(p, "unix_seconds", (double)epoch);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[128], title[192];
    snprintf(key, sizeof key, "%s|%lld", bzn, epoch);
    snprintf(title, sizeof title, "%s day-ahead price %.2f %s", bzn,
             v->valuedouble, unit);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = url;
    row.record_type     = "electricity-price";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"price\",\"market\",\"europe\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, fetched = 0;
  for (int i = 0; ZONES[i]; i++)
    total += collect_zone(ctx, sink, ZONES[i], &fetched);
  if (!fetched) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  fprintf(stderr, "[" SRC "] emitted %d\n", total);
  return 0;
}

static const source_def eni_energy_charts_price_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "Energy-Charts day-ahead electricity price",
  .update_interval_sec = 3600, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://api.energy-charts.info/price?bzn=DE-LU",
  .description = "Day-ahead wholesale electricity price per European bidding zone; price spikes lead grid stress and plant outages.",
  .license = "CC BY 4.0 from Bundesnetzagentur | SMARD.de (Fraunhofer ISE Energy-Charts).",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_energy_charts_price_def)
