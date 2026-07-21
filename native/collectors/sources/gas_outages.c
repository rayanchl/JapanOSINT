/* collectors/infrastructure/sources/gas_outages.c
 * Port of server/src/collectors/gasOutages.js — fetchHead reachability
 * probe over 4 gas operators → ONE intel item per operator
 * (uid gas-outages|<op>). OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* OPS = [[op,url],...] — verbatim JS order */
static const char *OPS[][2] = {
  { "Tokyo-Gas", "https://www.tokyo-gas.co.jp/saigai/" },
  { "Osaka-Gas", "https://home.osakagas.co.jp/disaster/" },
  { "Toho-Gas",  "https://www.tohogas.co.jp/anshin/saigai/" },
  { "Saibu-Gas", "https://www.saibugas.co.jp/info/saigai/" },
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  char now[32]; probe_iso_now(now, sizeof now);
  int err = 0;

  for (size_t i = 0; i < sizeof(OPS) / sizeof(OPS[0]); i++) {
    const char *op  = OPS[i][0];
    const char *url = OPS[i][1];
    int live = probe_head(ctx->http, url);

    /* op.toLowerCase().replace('-','_') — single replace, ASCII names */
    char optag[32];
    size_t k = 0;
    for (const char *s = op; *s && k + 1 < sizeof optag; s++) {
      char c = *s;
      if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
      if (c == '-') c = '_';
      optag[k++] = c;
    }
    optag[k] = '\0';

    char title[96];
    snprintf(title, sizeof title, "%s disaster / outage portal", op);

    /* tags: ['gas','outage', optag, live?'reachable':'unreachable'] */
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("gas"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("outage"));
    cJSON_AddItemToArray(tags, cJSON_CreateString(optag));
    cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
    char *tj = cJSON_PrintUnformatted(tags);

    /* properties: { operator, reachable } — EXACT JS key order */
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "operator", op);
    cJSON_AddBoolToObject(p, "reachable", live);
    char *pj = cJSON_PrintUnformatted(p);

    intel_item it = {0};
    it.remote_key   = op;                       /* → uid gas-outages|<op> */
    it.title        = title;
    it.summary      = "Gas-utility disaster / outage info page";
    it.link         = url;
    it.lang         = "ja";
    it.published_at = now;
    it.tags_json    = tj;
    it.properties_json = pj;
    if (sink->emit(sink, &it) < 0) err = 1;

    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(p);
    fprintf(stderr, "[gas-outages] %s reachable=%d\n", op, live);
  }
  return err ? -1 : 0;
}

static const source_def gas_outages_def = {
  .id = "gas-outages", .collector = "infrastructure",
  .name = "Gas operator outage portals", .name_ja = "ガス事業者 停止ポータル",
   .update_interval_sec = 1800, .run = run };
REGISTER_SOURCE(gas_outages_def)
