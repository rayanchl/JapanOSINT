/* Fraunhofer Energy-Charts public net electricity generation.
 * Endpoint: https://api.energy-charts.info/public_power?country=<cc>  (keyless)
 * Emits one row per production type per country: megawatts (unit MW) at the
 * newest quarter-hour that actually carries a number, plus its epoch timestamp.
 *
 * COLUMN-ALIGNMENT TRAP (quoted from the source survey): "unix_seconds[] is the
 * time axis (epoch seconds, 900 s apart); each production_types[i].data[] is a
 * PARALLEL array of the same length. Index i in data[] belongs to
 * unix_seconds[i] - never zip by position across different production_types."
 * We therefore walk each series' OWN data[] backwards and pair index i with
 * unix_seconds[i] of the SAME index. nulls mean "not yet reported" and are
 * skipped, never coerced to 0.
 * Licence: CC BY 4.0 from Bundesnetzagentur | SMARD.de (response license_info). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "energy-charts-public-power"

static const char *COUNTRIES[] = { "de", "fr", "es", "pl", "it", NULL };

static int collect_country(const source_ctx *ctx, intel_sink *sink,
                           const char *cc, int *fetched) {
  char url[192];
  snprintf(url, sizeof url,
           "https://api.energy-charts.info/public_power?country=%s", cc);
  cJSON *doc = feed_get_json(ctx->http, url, 25000);
  if (!doc) return 0;
  *fetched = 1;

  cJSON *ts = cJSON_GetObjectItem(doc, "unix_seconds");
  cJSON *pts = cJSON_GetObjectItem(doc, "production_types");
  if (!cJSON_IsArray(ts) || !cJSON_IsArray(pts)) { cJSON_Delete(doc); return 0; }
  int nts = cJSON_GetArraySize(ts);

  int n = 0;
  cJSON *series;
  cJSON_ArrayForEach(series, pts) {
    cJSON *nm = cJSON_GetObjectItem(series, "name");
    cJSON *data = cJSON_GetObjectItem(series, "data");
    if (!cJSON_IsString(nm) || !cJSON_IsArray(data)) continue;
    int nd = cJSON_GetArraySize(data);
    int lim = nd < nts ? nd : nts;       /* pair strictly by shared index */

    /* newest index of THIS series that carries a real number */
    int i = lim - 1;
    for (; i >= 0; i--) {
      cJSON *v = cJSON_GetArrayItem(data, i);
      if (cJSON_IsNumber(v)) break;
    }
    if (i < 0) continue;                             /* nothing reported yet */
    cJSON *v = cJSON_GetArrayItem(data, i);
    cJSON *t = cJSON_GetArrayItem(ts, i);
    if (!cJSON_IsNumber(t)) continue;
    long long epoch = (long long)t->valuedouble;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "country", cc);
    cJSON_AddStringToObject(p, "production_type", nm->valuestring);
    cJSON_AddNumberToObject(p, "power_mw", v->valuedouble);
    cJSON_AddStringToObject(p, "unit", "MW");
    cJSON_AddNumberToObject(p, "unix_seconds", (double)epoch);
    cJSON_AddStringToObject(p, "resolution", "15min");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[192], title[220];
    snprintf(key, sizeof key, "%s|%s|%lld", cc, nm->valuestring, epoch);
    snprintf(title, sizeof title, "%s net generation: %s = %.1f MW",
             cc, nm->valuestring, v->valuedouble);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = url;
    row.record_type     = "grid-generation";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"generation\",\"europe\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, fetched = 0;
  for (int i = 0; COUNTRIES[i]; i++)
    total += collect_country(ctx, sink, COUNTRIES[i], &fetched);
  if (!fetched) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  fprintf(stderr, "[" SRC "] emitted %d\n", total);
  return 0;
}

static const source_def eni_energy_charts_power_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "Fraunhofer Energy-Charts public net electricity generation",
  .update_interval_sec = 900, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://api.energy-charts.info/public_power?country=de",
  .description = "Quarter-hourly net electricity generation by production type for European countries (DE, FR, ES, PL, IT).",
  .license = "CC BY 4.0 from Bundesnetzagentur | SMARD.de (Fraunhofer ISE Energy-Charts).",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_energy_charts_power_def)
