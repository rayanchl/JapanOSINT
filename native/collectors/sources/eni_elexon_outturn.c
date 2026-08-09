/* Elexon BMRS generation outturn by fuel type (GB).
 * Endpoint: https://data.elexon.co.uk/bmrs/api/v1/generation/outturn/summary
 *           ?resolveBmUnits=false                                    (keyless)
 * Emits one row per fuel type of the MOST RECENT settlement period returned:
 *   generation_mw (UNIT: MW, integer), fuel_type, settlement period, startTime.
 * INTERCONNECTOR NOTE: fuelType values beginning "INT" are interconnector
 * flows (positive = import into GB), not domestic generation. Each row carries
 * is_interconnector so they are never silently summed into GB generation.
 * SHAPE: top level is an ARRAY of settlement periods, each with a nested data[].
 * Licence: Elexon BMRS Insights API, open, no key required for this path. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "elexon-generation-outturn"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = "https://data.elexon.co.uk/bmrs/api/v1/generation/outturn/"
                    "summary?resolveBmUnits=false";
  cJSON *doc = feed_get_json(ctx->http, url, 25000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *arr = doc;
  if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] unexpected shape\n"); return -1; }

  /* pick the newest settlement period by startTime string (ISO-8601 sorts) */
  cJSON *newest = NULL; const char *newest_t = NULL;
  cJSON *per;
  cJSON_ArrayForEach(per, arr) {
    const char *t = jo_sv(per, "startTime");
    if (!t) continue;
    if (!newest_t || strcmp(t, newest_t) > 0) { newest_t = t; newest = per; }
  }
  if (!newest) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no periods\n"); return 0; }

  cJSON *sp = cJSON_GetObjectItem(newest, "settlementPeriod");
  int period = cJSON_IsNumber(sp) ? (int)sp->valuedouble : -1;
  cJSON *data = cJSON_GetObjectItem(newest, "data");
  if (!cJSON_IsArray(data)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no data[]\n"); return 0; }

  int n = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, data) {
    const char *fuel = jo_sv(e, "fuelType");
    cJSON *g = cJSON_GetObjectItem(e, "generation");
    if (!fuel || !cJSON_IsNumber(g)) continue;   /* 0 MW is real; null is not */
    int is_int = strncmp(fuel, "INT", 3) == 0;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "fuel_type", fuel);
    cJSON_AddNumberToObject(p, "generation_mw", g->valuedouble);
    cJSON_AddStringToObject(p, "unit", "MW");
    cJSON_AddBoolToObject(p, "is_interconnector", is_int);
    if (is_int) cJSON_AddStringToObject(p, "flow_convention",
                                        "positive = import into GB");
    if (period >= 0) cJSON_AddNumberToObject(p, "settlement_period", period);
    cJSON_AddStringToObject(p, "start_time", newest_t);
    cJSON_AddStringToObject(p, "region", "GB");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[128], title[192];
    snprintf(key, sizeof key, "%s|%s", newest_t, fuel);
    snprintf(title, sizeof title, "GB outturn %s = %.0f MW (period %d)",
             fuel, g->valuedouble, period);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = newest_t;
    row.lang            = "en";
    row.link            = url;
    row.record_type     = "grid-generation";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"generation\",\"gb\",\"elexon\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_elexon_outturn_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "Elexon BMRS generation outturn by fuel type (GB)",
  .update_interval_sec = 1800, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://data.elexon.co.uk/bmrs/api/v1/generation/outturn/summary",
  .description = "GB half-hourly metered generation by fuel type in MW, including interconnector flows, from the Balancing Mechanism Reporting Service.",
  .license = "Elexon BMRS Insights API, open, no key required.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_elexon_outturn_def)
