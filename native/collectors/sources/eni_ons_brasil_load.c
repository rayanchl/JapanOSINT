/* ONS Brazil — national interconnected system (SIN) load.
 * Endpoint: https://integra.ons.org.br/api/energiaagora/Get/Carga_SIN_json
 * Keyless. Flat array of {instante, carga}, one sample per minute for the
 * current day (~1,440 rows); `instante` carries an explicit -03:00 offset.
 * Emits the most recent 60 samples: load (UNIT: MW — carga is megawatts, not
 * MWh and not GW) with its timestamp.
 * Sibling paths on the same host: Get/Geracao_SIN_json, Get/Carga_SE_json.
 * Licence: ONS "Energia Agora" public dashboard API, keyless; the archival
 * series is CC-BY on dados.ons.org.br. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "ons-brasil-load"
#define KEEP 60

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = "https://integra.ons.org.br/api/energiaagora/Get/Carga_SIN_json";
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] unexpected shape\n"); return -1; }

  int total = cJSON_GetArraySize(doc);
  int start = total > KEEP ? total - KEEP : 0;
  int n = 0;
  for (int i = start; i < total; i++) {
    cJSON *e = cJSON_GetArrayItem(doc, i);
    const char *when = jo_sv(e, "instante");
    cJSON *carga = cJSON_GetObjectItem(e, "carga");
    if (!when || !cJSON_IsNumber(carga)) continue;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "system", "SIN");
    cJSON_AddNumberToObject(p, "load_mw", carga->valuedouble);
    cJSON_AddStringToObject(p, "unit", "MW");
    cJSON_AddStringToObject(p, "instante", when);
    cJSON_AddStringToObject(p, "resolution", "1min");
    cJSON_AddStringToObject(p, "country", "BR");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[192];
    snprintf(title, sizeof title, "Brazil SIN load %.1f MW at %s",
             carga->valuedouble, when);

    intel_item row = {0};
    row.remote_key      = when;
    row.title           = title;
    row.summary         = title;
    row.published_at    = when;
    row.lang            = "pt";
    row.link            = url;
    row.record_type     = "grid-load";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"load\",\"brazil\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_ons_brasil_load_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "ONS Brazil national interconnected system load",
  .update_interval_sec = 300, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://integra.ons.org.br/api/energiaagora/Get/Carga_SIN_json",
  .description = "Minute-by-minute electricity load (MW) of the Brazilian National Interconnected System from the system operator's live dashboard.",
  .license = "ONS Energia Agora public dashboard API, keyless; archival series CC-BY on dados.ons.org.br.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_ons_brasil_load_def)
