/* GB generation mix by fuel (NESO carbon intensity API).
 * Endpoint: https://api.carbonintensity.org.uk/generation   (keyless)
 * Emits one row per fuel actually returned: percent_of_generation
 * (unit "%", 0-100, one decimal) + the half-hour window from/to.
 * UNIT TRAP: `perc` is a PERCENTAGE SHARE, NOT megawatts. Nine fuels are
 * always present and a 0 is a real reading (coal frequently 0), so zeroes
 * are emitted, not dropped.
 * SHAPE TRAP: `data` is an OBJECT here, unlike /intensity where it is an array.
 * Licence: CC-BY 4.0 (NESO carbon intensity API). */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "uk-generation-mix"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http,
                             "https://api.carbonintensity.org.uk/generation", 20000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  /* data is an OBJECT (not an array as on /intensity) */
  cJSON *data = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsObject(data)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no data{}\n"); return -1; }

  const char *from = jo_sv(data, "from"), *to = jo_sv(data, "to");
  cJSON *mix = cJSON_GetObjectItem(data, "generationmix");
  if (!cJSON_IsArray(mix) || !from) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no generationmix\n"); return 0; }

  int n = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, mix) {
    const char *fuel = jo_sv(e, "fuel");
    cJSON *pc = cJSON_GetObjectItem(e, "perc");
    if (!fuel || !cJSON_IsNumber(pc)) continue;      /* no measurement -> no row */

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "fuel", fuel);
    cJSON_AddNumberToObject(p, "percent_of_generation", pc->valuedouble);
    cJSON_AddStringToObject(p, "unit", "%");
    cJSON_AddStringToObject(p, "period_from", from);
    if (to) cJSON_AddStringToObject(p, "period_to", to);
    cJSON_AddStringToObject(p, "region", "GB");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[128], title[160];
    snprintf(key, sizeof key, "%s|%s", from, fuel);
    snprintf(title, sizeof title, "GB generation mix: %s %.1f%%", fuel, pc->valuedouble);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = from;
    row.lang            = "en";
    row.link            = "https://api.carbonintensity.org.uk/generation";
    row.record_type     = "grid-generation-mix";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"generation\",\"gb\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_uk_generation_mix_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "GB generation mix by fuel (NESO)",
  .update_interval_sec = 1800, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://api.carbonintensity.org.uk/generation",
  .description = "Share of GB electricity generation by fuel type (biomass, coal, imports, gas, nuclear, hydro, solar, wind, other) for the current half-hour.",
  .license = "CC-BY 4.0, NESO carbon intensity API.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_uk_generation_mix_def)
