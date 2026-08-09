/* GB grid carbon intensity (NESO / National Grid ESO).
 * Endpoint: https://api.carbonintensity.org.uk/intensity   (keyless)
 * Emits, per half-hour settlement period actually returned:
 *   gco2_per_kwh (INTEGER, unit gCO2/kWh), basis ("actual"|"forecast"),
 *   index bucket string, from/to timestamps.
 * UNIT NOTE: intensity.actual/forecast are gCO2 per kWh, NOT tonnes and NOT
 * a percentage. intensity.index is a BUCKET STRING ("low"/"moderate"/...),
 * never a number — it is emitted as a string property only.
 * Licence: CC-BY 4.0 (NESO carbon intensity API). */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "uk-carbon-intensity"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http,
                             "https://api.carbonintensity.org.uk/intensity", 20000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *data = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(data)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no data[]\n"); return -1; }

  int n = 0;
  cJSON *it;
  cJSON_ArrayForEach(it, data) {
    const char *from = jo_sv(it, "from"), *to = jo_sv(it, "to");
    cJSON *iy = cJSON_GetObjectItem(it, "intensity");
    if (!from || !cJSON_IsObject(iy)) continue;

    cJSON *act = cJSON_GetObjectItem(iy, "actual");
    cJSON *fc  = cJSON_GetObjectItem(iy, "forecast");
    /* actual is null until the period settles; fall back to forecast but
     * LABEL it so a forecast is never presented as a measurement (R1). */
    double v; const char *basis;
    if (cJSON_IsNumber(act))      { v = act->valuedouble; basis = "actual"; }
    else if (cJSON_IsNumber(fc))  { v = fc->valuedouble;  basis = "forecast"; }
    else continue;                                   /* nothing measured (R1) */

    const char *idx = jo_sv(iy, "index");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "gco2_per_kwh", v);
    cJSON_AddStringToObject(p, "unit", "gCO2/kWh");
    cJSON_AddStringToObject(p, "basis", basis);
    if (idx) cJSON_AddStringToObject(p, "intensity_index", idx);
    cJSON_AddStringToObject(p, "period_from", from);
    if (to) cJSON_AddStringToObject(p, "period_to", to);
    cJSON_AddStringToObject(p, "region", "GB");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[160], summ[200];
    snprintf(title, sizeof title, "GB carbon intensity %.0f gCO2/kWh (%s)", v, basis);
    snprintf(summ, sizeof summ, "%s → %s: %.0f gCO2/kWh%s%s",
             from, to ? to : "?", v, idx ? ", index " : "", idx ? idx : "");

    intel_item row = {0};
    row.remote_key      = from;
    row.title           = title;
    row.summary         = summ;
    row.published_at    = from;
    row.lang            = "en";
    row.link            = "https://api.carbonintensity.org.uk/intensity";
    row.record_type     = "grid-carbon-intensity";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"carbon\",\"gb\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;                                   /* fetched fine (R3) */
}

static const source_def eni_uk_carbon_intensity_def = {
  .id = SRC, .collector = "environment",
  .name = "UK National Grid carbon intensity (NESO)",
  .update_interval_sec = 1800, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.carbonintensity.org.uk/intensity",
  .description = "Live and forecast carbon intensity of GB electricity in gCO2/kWh, half-hourly settlement periods.",
  .license = "CC-BY 4.0 (National Grid ESO/NESO carbon intensity API, explicitly open, no key).",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_uk_carbon_intensity_def)
