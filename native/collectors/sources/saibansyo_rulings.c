/* collectors/government/sources/saibansyo_rulings.c
 * Port of server/src/collectors/saibansyoRulings.js — fetchHead reachability
 * probe → ONE intel item (uid = saibansyo-rulings|portal).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.courts.go.jp/app/hanrei_jp/list1"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['legal','courts','jurisprudence', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("legal"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("courts"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("jurisprudence"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "裁判所");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid saibansyo-rulings|portal */
  it.title        = "Saibansyo (Courts of Japan) ruling search";
  it.summary      = "Public ruling index — Supreme Court + High / District / Family / Summary courts";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[saibansyo-rulings] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def saibansyo_rulings_def = {
  .id = "saibansyo-rulings", .collector = "government",
  .name = "Saibansyo court rulings", .name_ja = "裁判所 判例検索",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(saibansyo_rulings_def)
