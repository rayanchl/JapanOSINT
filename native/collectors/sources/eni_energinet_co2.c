/* Energinet (DK) realtime grid CO2 emission intensity.
 * Endpoint: https://api.energidataservice.dk/dataset/CO2Emis
 *           ?limit=10&sort=Minutes5UTC%20DESC                        (keyless)
 * Emits, for the newest 5-minute stamp of each price area (DK1, DK2):
 *   co2_g_per_kwh (UNIT: grams CO2 per kWh — the dataset's own unit, NOT
 *   tonnes and NOT g/MWh) + the UTC timestamp.
 * KEYING TRAP: two rows (DK1, DK2) share every 5-minute stamp — key by area.
 * Licence: Energi Data Service open data, no key. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "energinet-co2-emission"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http,
    "https://api.energidataservice.dk/dataset/CO2Emis"
    "?limit=10&sort=Minutes5UTC%20DESC", 20000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *recs = cJSON_GetObjectItem(doc, "records");
  if (!cJSON_IsArray(recs)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no records[]\n"); return -1; }

  char seen[8][16]; int nseen = 0;
  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, recs) {
    const char *area = jo_sv(r, "PriceArea");
    const char *tstamp = jo_sv(r, "Minutes5UTC");
    cJSON *v = cJSON_GetObjectItem(r, "CO2Emission");
    if (!area || !tstamp || !cJSON_IsNumber(v)) continue;
    int dup = 0;
    for (int i = 0; i < nseen; i++) if (strcmp(seen[i], area) == 0) { dup = 1; break; }
    if (dup) continue;
    if (nseen < 8) { snprintf(seen[nseen], sizeof seen[0], "%s", area); nseen++; }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "price_area", area);
    cJSON_AddNumberToObject(p, "co2_g_per_kwh", v->valuedouble);
    cJSON_AddStringToObject(p, "unit", "gCO2/kWh");
    cJSON_AddStringToObject(p, "timestamp_utc", tstamp);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[96], title[176];
    snprintf(key, sizeof key, "%s|%s", area, tstamp);
    snprintf(title, sizeof title, "Denmark %s grid CO2 %.0f gCO2/kWh",
             area, v->valuedouble);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = tstamp;
    row.lang            = "en";
    row.link            = "https://api.energidataservice.dk/dataset/CO2Emis";
    row.record_type     = "grid-carbon-intensity";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"carbon\",\"denmark\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_energinet_co2_def = {
  .id = SRC, .collector = "environment",
  .name = "Energinet realtime grid CO2 emission intensity (DK)",
  .update_interval_sec = 300, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.energidataservice.dk/dataset/CO2Emis",
  .description = "Declared CO2 emission per kWh of Danish grid electricity, 5-minute resolution, per price area.",
  .license = "Energi Data Service open data, no key.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_energinet_co2_def)
