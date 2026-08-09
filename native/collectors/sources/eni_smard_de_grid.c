/* SMARD.de — the German regulator's own grid dataset (Bundesnetzagentur).
 * Endpoints (keyless), TWO-STEP because a made-up timestamp yields HTTP 404:
 *   1 https://www.smard.de/app/chart_data/{filter}/DE/index_quarterhour.json
 *       -> {"timestamps":[...]} of WEEK-START epoch-ms values; take the last
 *   2 https://www.smard.de/app/chart_data/{filter}/DE/{filter}_DE_quarterhour_{ts}.json
 *       -> {"series":[[epoch_ms, value], ...]}, null for unpublished steps
 * Emits one row per filter (410 total load, 1223 lignite, 1224 nuclear,
 * 4067 onshore wind, 4068 solar, 4069 hard coal, 4071 natural gas) at the
 * newest published quarter-hour.
 *
 * UNIT TRAP, quoted from the source survey: "the values are MWh PER
 * QUARTER-HOUR, not MW - multiply by 4 to get average power (10210.02
 * MWh/15min = 40.8 GW, which is the sanity check that the column is right)."
 * Both figures are emitted explicitly: energy_mwh_per_quarter_hour (raw) and
 * average_power_mw (= raw * 4), each with its own unit string.
 * Licence: CC BY 4.0, Bundesnetzagentur | SMARD.de. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "smard-de-grid"

static const struct { const char *filter, *label; } FILTERS[] = {
  { "410",  "total load"     },
  { "1223", "lignite"        },
  { "1224", "nuclear"        },
  { "4067", "onshore wind"   },
  { "4068", "solar"          },
  { "4069", "hard coal"      },
  { "4071", "natural gas"    },
  { NULL, NULL }
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* step 1: the index gives valid week-start timestamps; reuse it for every
   * filter — the week boundaries are the same across filters. */
  cJSON *idx = feed_get_json(ctx->http,
    "https://www.smard.de/app/chart_data/410/DE/index_quarterhour.json", 25000);
  if (!idx) { fprintf(stderr, "[" SRC "] index fetch failed\n"); return -1; }
  cJSON *ts = cJSON_GetObjectItem(idx, "timestamps");
  int nts = cJSON_IsArray(ts) ? cJSON_GetArraySize(ts) : 0;
  if (nts == 0) { cJSON_Delete(idx); fprintf(stderr, "[" SRC "] empty index\n"); return -1; }
  cJSON *lastts = cJSON_GetArrayItem(ts, nts - 1);
  if (!cJSON_IsNumber(lastts)) { cJSON_Delete(idx); fprintf(stderr, "[" SRC "] bad index\n"); return -1; }
  long long week = (long long)lastts->valuedouble;
  cJSON_Delete(idx);

  int n = 0;
  for (int i = 0; FILTERS[i].filter; i++) {
    char url[256];
    snprintf(url, sizeof url,
      "https://www.smard.de/app/chart_data/%s/DE/%s_DE_quarterhour_%lld.json",
      FILTERS[i].filter, FILTERS[i].filter, week);
    cJSON *doc = feed_get_json(ctx->http, url, 30000);
    if (!doc) continue;
    cJSON *series = cJSON_GetObjectItem(doc, "series");
    if (!cJSON_IsArray(series)) { cJSON_Delete(doc); continue; }

    /* newest [epoch_ms, value] pair whose value is not null */
    cJSON *best = NULL;
    int m = cJSON_GetArraySize(series);
    for (int k = m - 1; k >= 0; k--) {
      cJSON *pair = cJSON_GetArrayItem(series, k);
      if (!cJSON_IsArray(pair) || cJSON_GetArraySize(pair) < 2) continue;
      if (cJSON_IsNumber(cJSON_GetArrayItem(pair, 1))) { best = pair; break; }
    }
    if (!best) { cJSON_Delete(doc); continue; }
    long long ems = (long long)cJSON_GetArrayItem(best, 0)->valuedouble;
    double mwh_qh = cJSON_GetArrayItem(best, 1)->valuedouble;
    /* MWh per quarter-hour -> average MW over that quarter-hour */
    double mw = mwh_qh * 4.0;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "filter_id", FILTERS[i].filter);
    cJSON_AddStringToObject(p, "series", FILTERS[i].label);
    cJSON_AddStringToObject(p, "region", "DE");
    cJSON_AddNumberToObject(p, "energy_mwh_per_quarter_hour", mwh_qh);
    cJSON_AddStringToObject(p, "energy_unit", "MWh per 15 min");
    cJSON_AddNumberToObject(p, "average_power_mw", mw);
    cJSON_AddStringToObject(p, "power_unit", "MW");
    cJSON_AddStringToObject(p, "conversion", "average_power_mw = MWh_per_quarter_hour * 4");
    cJSON_AddNumberToObject(p, "epoch_ms", (double)ems);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[96], title[224];
    snprintf(key, sizeof key, "%s|%lld", FILTERS[i].filter, ems);
    snprintf(title, sizeof title, "SMARD DE %s: %.1f MW (%.2f MWh/15min)",
             FILTERS[i].label, mw, mwh_qh);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = url;
    row.record_type     = "grid-generation";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"germany\",\"smard\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
    cJSON_Delete(doc);
  }
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_smard_de_grid_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "SMARD.de official German grid data (Bundesnetzagentur)",
  .update_interval_sec = 3600, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://www.smard.de/app/chart_data/410/DE/index_quarterhour.json",
  .description = "German regulator grid dataset: load and generation per fuel at quarter-hour resolution, emitted both as MWh per quarter-hour and as average MW.",
  .license = "CC BY 4.0, Bundesnetzagentur | SMARD.de.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_smard_de_grid_def)
