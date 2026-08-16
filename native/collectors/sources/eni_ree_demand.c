/* Red Electrica de Espana (REE) realtime national electricity demand.
 * Endpoint: https://apidatos.ree.es/en/datos/demanda/demanda-tiempo-real
 *           ?start_date=<today>T00:00&end_date=<today>T23:59&time_trunc=hour
 * Keyless. start_date/end_date are MANDATORY; an out-of-range window returns
 * included[] with EMPTY values[] rather than an HTTP error, so an empty result
 * is an honest 0, not a failure.
 * Emits, per series ("Real" / "Forecasted" / "Scheduled"), the newest point
 * actually returned: demand_mw (UNIT: MW) + its datetime.
 * SHAPE TRAP: the measurements live in included[].attributes.values[] — NOT in
 * data.attributes, which only carries the title/metadata.
 * Licence: REE apidatos public API, keyless; REE asks for attribution. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SRC "ree-demand-realtime"

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL);
  struct tm tmv;
  gmtime_r(&now, &tmv);
  char day[16];
  strftime(day, sizeof day, "%Y-%m-%d", &tmv);

  char url[320];
  snprintf(url, sizeof url,
    "https://apidatos.ree.es/en/datos/demanda/demanda-tiempo-real"
    "?start_date=%sT00:00&end_date=%sT23:59&time_trunc=hour", day, day);

  cJSON *doc = feed_get_json(ctx->http, url, 25000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *inc = cJSON_GetObjectItem(doc, "included");
  if (!cJSON_IsArray(inc)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no included[]\n"); return -1; }

  int n = 0;
  cJSON *series;
  cJSON_ArrayForEach(series, inc) {
    const char *stype = jo_sv(series, "type");
    cJSON *attr = cJSON_GetObjectItem(series, "attributes");
    if (!attr) continue;
    if (!stype) stype = jo_sv(attr, "title");
    if (!stype) continue;
    cJSON *vals = cJSON_GetObjectItem(attr, "values");
    if (!cJSON_IsArray(vals)) continue;

    /* newest point that actually carries a number */
    cJSON *best = NULL;
    cJSON *v;
    cJSON_ArrayForEach(v, vals) {
      cJSON *num = cJSON_GetObjectItem(v, "value");
      if (cJSON_IsNumber(num) && jo_sv(v, "datetime")) best = v;
    }
    if (!best) continue;                    /* series present but no values */
    double mw = cJSON_GetObjectItem(best, "value")->valuedouble;
    const char *dt = jo_sv(best, "datetime");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "series", stype);
    cJSON_AddNumberToObject(p, "demand_mw", mw);
    cJSON_AddStringToObject(p, "unit", "MW");
    cJSON_AddStringToObject(p, "datetime", dt);
    cJSON_AddStringToObject(p, "country", "ES");
    cJSON *pct = cJSON_GetObjectItem(best, "percentage");
    if (cJSON_IsNumber(pct)) cJSON_AddNumberToObject(p, "percentage", pct->valuedouble);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[160], title[224];
    snprintf(key, sizeof key, "%s|%s", stype, dt);
    snprintf(title, sizeof title, "Spain demand (%s) %.0f MW at %s", stype, mw, dt);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = dt;
    row.lang            = "en";
    row.link            = url;
    row.record_type     = "grid-demand";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"demand\",\"spain\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_ree_demand_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "Red Electrica de Espana realtime national demand",
  .update_interval_sec = 900, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://apidatos.ree.es/en/datos/demanda/demanda-tiempo-real",
  .description = "Spanish TSO realtime national electricity demand (real, forecast, scheduled) in MW.",
  .license = "REE apidatos public API, keyless; attribution to Red Electrica requested.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_ree_demand_def)
