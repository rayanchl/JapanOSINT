/* Elia (Belgian TSO) measured and forecast total grid load.
 * Endpoint: https://opendata.elia.be/api/explore/v2.1/catalog/datasets/ods001/
 *   records?limit=100&order_by=datetime%20desc&where=totalload%20is%20not%20null
 * Keyless (Opendatasoft Explore v2.1).
 * Emits one row per 15-minute step returned: total_load_mw (UNIT: MW, PT15M),
 * plus most-recent and day-ahead forecasts when present.
 *
 * FORECAST-AS-MEASUREMENT TRAP: without `where=totalload is not null` the
 * newest rows are FUTURE forecast rows whose totalload is null — a naive
 * "take the first record" then reads a FORECAST as a MEASUREMENT. The filter
 * and order_by are both baked into the URL, and totalload is re-checked here.
 * Licence: Elia Open Data (Opendatasoft), CC BY 4.0 equivalent, keyless. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "elia-total-load"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = "https://opendata.elia.be/api/explore/v2.1/catalog/datasets/"
                    "ods001/records?limit=100&order_by=datetime%20desc"
                    "&where=totalload%20is%20not%20null";
  cJSON *doc = feed_get_json(ctx->http, url, 25000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *res = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(res)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no results[]\n"); return -1; }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, res) {
    const char *dt = jo_sv(r, "datetime");
    cJSON *load = cJSON_GetObjectItem(r, "totalload");
    /* re-assert the filter: a null totalload is a forecast row, never a load */
    if (!dt || !cJSON_IsNumber(load)) continue;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "datetime", dt);
    cJSON_AddNumberToObject(p, "total_load_mw", load->valuedouble);
    cJSON_AddStringToObject(p, "unit", "MW");
    cJSON_AddStringToObject(p, "measurement", "measured");
    const char *rc = jo_sv(r, "resolutioncode");
    cJSON_AddStringToObject(p, "resolution", rc ? rc : "PT15M");
    cJSON *mf = cJSON_GetObjectItem(r, "mostrecentforecast");
    cJSON *df = cJSON_GetObjectItem(r, "dayaheadforecast");
    if (cJSON_IsNumber(mf)) cJSON_AddNumberToObject(p, "most_recent_forecast_mw", mf->valuedouble);
    if (cJSON_IsNumber(df)) cJSON_AddNumberToObject(p, "day_ahead_forecast_mw", df->valuedouble);
    cJSON_AddStringToObject(p, "country", "BE");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[192];
    snprintf(title, sizeof title, "Belgium total load %.2f MW at %s",
             load->valuedouble, dt);

    intel_item row = {0};
    row.remote_key      = dt;
    row.title           = title;
    row.summary         = title;
    row.published_at    = dt;
    row.lang            = "en";
    row.link            = "https://opendata.elia.be/explore/dataset/ods001/";
    row.record_type     = "grid-load";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"load\",\"belgium\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_elia_load_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "Elia Belgium measured and forecast total load",
  .update_interval_sec = 900, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://opendata.elia.be/api/explore/v2.1/catalog/datasets/ods001/records",
  .description = "Belgian TSO 15-minute measured grid load in MW with day-ahead and most-recent forecasts.",
  .license = "Elia Open Data (Opendatasoft) open data licence, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_elia_load_def)
