/* collectors/infrastructure/sources/regional_grid_outages.c
 * Port of server/src/collectors/regionalGridOutages.js — per-operator fetchHead
 * reachability probe → ONE intel item per regional grid operator
 * (uid = regional-grid-outages|<op> == intelUid(SOURCE_ID, op)).
 * 3rd tag is op.toLowerCase(). OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

static const struct { const char *op; const char *lop; const char *url; } OPS[] = {
  { "KEPCO",   "kepco",   "https://www.kansai-td.co.jp/teiden/area_jishin/" },
  { "Chubu",   "chubu",   "https://teiden.chuden.jp/p/index.html" },
  { "Tohoku",  "tohoku",  "https://teiden.nw.tohoku-epco.co.jp/" },
  { "Kyuden",  "kyuden",  "https://www.kyuden.co.jp/td_teiden_index.html" },
  { "HEPCO",   "hepco",   "https://www.hepco.co.jp/network/teiden_info/" },
  { "Rikuden", "rikuden", "https://teiden.rikuden.co.jp/" },
  { "Chugoku", "chugoku", "https://teiden.energia.co.jp/" },
  { "Yonden",  "yonden",  "https://www.yonden.co.jp/cnt_teiden/" },
  { "Okiden",  "okiden",  "https://www.okiden.co.jp/dispatch/" },
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  char now[32]; probe_iso_now(now, sizeof now);
  int ok = 0;
  for (size_t i = 0; i < sizeof OPS / sizeof OPS[0]; i++) {
    const char *op = OPS[i].op, *lop = OPS[i].lop, *url = OPS[i].url;
    int live = probe_head(ctx->http, url);

    /* tags: ['power','outage', op.toLowerCase(), live?'reachable':'unreachable'] */
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("power"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("outage"));
    cJSON_AddItemToArray(tags, cJSON_CreateString(lop));
    cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
    char *tj = cJSON_PrintUnformatted(tags);

    /* properties: { operator, reachable } — EXACT JS key order */
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "operator", op);
    cJSON_AddBoolToObject(p, "reachable", live);
    char *pj = cJSON_PrintUnformatted(p);

    char title[128];
    snprintf(title, sizeof title, "%s outage portal", op);

    intel_item it = {0};
    it.remote_key   = op;                       /* → uid regional-grid-outages|<op> */
    it.title        = title;
    it.summary      = "Per-municipality outage feed";
    it.link         = url;
    it.lang         = "ja";
    it.published_at = now;
    it.tags_json    = tj;
    it.properties_json = pj;
    int rc = sink->emit(sink, &it);

    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(p);
    if (rc < 0) ok = -1;
    fprintf(stderr, "[regional-grid-outages] %s reachable=%d\n", op, live);
  }
  return ok;
}

static const source_def regional_grid_outages_def = {
  .id = "regional-grid-outages", .collector = "infrastructure",
  .name = "9 regional outage portals", .name_ja = "9地域 停電ポータル",
   .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(regional_grid_outages_def)
