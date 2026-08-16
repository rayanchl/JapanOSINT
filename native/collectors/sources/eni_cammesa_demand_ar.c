/* CAMMESA Argentina regional demand and temperature.
 * Endpoint: https://api.cammesa.com/demanda-svc/demanda/
 *           ObtieneDemandaYTemperaturaRegion?id_region=1002            (keyless)
 * id_region 1002 = the whole country. Flat array, one element per 5 minutes of
 * the current day; future minutes are simply ABSENT (not null-filled).
 * Emits the most recent 60 elements: demHoy (UNIT: MW — today's MEASURED
 * demand) with, when the element carries them, demPrevista (forecast, MW) and
 * tempHoy (UNIT: degC).
 *
 * COMPARATOR TRAP: demAyer and demSemanaAnt are YESTERDAY and LAST WEEK
 * comparators. They are recorded under explicit comparator field names and must
 * never be mistaken for a current value.
 * ABSENCE TRAP: demPrevista and tempHoy exist only on the hourly elements —
 * most 5-minute elements omit them entirely, so every lookup tolerates absence
 * rather than defaulting to 0.
 * Licence: CAMMESA public demand service, keyless, no stated restriction. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "cammesa-demand-ar"
#define KEEP 60

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = "https://api.cammesa.com/demanda-svc/demanda/"
                    "ObtieneDemandaYTemperaturaRegion?id_region=1002";
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] unexpected shape\n"); return -1; }

  int total = cJSON_GetArraySize(doc);
  int start = total > KEEP ? total - KEEP : 0;
  int n = 0;
  for (int i = start; i < total; i++) {
    cJSON *e = cJSON_GetArrayItem(doc, i);
    const char *when = jo_sv(e, "fecha");
    cJSON *hoy = cJSON_GetObjectItem(e, "demHoy");
    if (!when || !cJSON_IsNumber(hoy)) continue;   /* no measurement -> no row */

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "region_id", "1002");
    cJSON_AddStringToObject(p, "region", "Argentina (national)");
    cJSON_AddNumberToObject(p, "demand_mw", hoy->valuedouble);
    cJSON_AddStringToObject(p, "unit", "MW");
    cJSON_AddStringToObject(p, "fecha", when);
    /* comparators, explicitly named so they cannot be read as "now" */
    cJSON *ay = cJSON_GetObjectItem(e, "demAyer");
    if (cJSON_IsNumber(ay))
      cJSON_AddNumberToObject(p, "comparator_yesterday_mw", ay->valuedouble);
    cJSON *sa = cJSON_GetObjectItem(e, "demSemanaAnt");
    if (cJSON_IsNumber(sa))
      cJSON_AddNumberToObject(p, "comparator_last_week_mw", sa->valuedouble);
    /* only present on the hourly elements — absence is not zero */
    cJSON *pv = cJSON_GetObjectItem(e, "demPrevista");
    if (cJSON_IsNumber(pv))
      cJSON_AddNumberToObject(p, "forecast_mw", pv->valuedouble);
    cJSON *tp = cJSON_GetObjectItem(e, "tempHoy");
    if (cJSON_IsNumber(tp)) {
      cJSON_AddNumberToObject(p, "temperature_c", tp->valuedouble);
      cJSON_AddStringToObject(p, "temperature_unit", "degC");
    }
    cJSON_AddStringToObject(p, "country", "AR");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[192];
    snprintf(title, sizeof title, "Argentina demand %.0f MW at %s",
             hoy->valuedouble, when);

    intel_item row = {0};
    row.remote_key      = when;
    row.title           = title;
    row.summary         = title;
    row.published_at    = when;
    row.lang            = "es";
    row.link            = "https://cammesaweb.cammesa.com/demanda/";
    row.record_type     = "grid-demand";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"demand\",\"argentina\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_cammesa_demand_ar_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "CAMMESA Argentina regional demand and temperature",
  .update_interval_sec = 300, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://api.cammesa.com/demanda-svc/demanda/ObtieneDemandaYTemperaturaRegion?id_region=1002",
  .description = "Argentine wholesale market operator demand (MW) at 5-minute resolution with the matching temperature, comparators kept explicitly separate from the current value.",
  .license = "CAMMESA public demand service, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_cammesa_demand_ar_def)
