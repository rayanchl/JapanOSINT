/* collectors/social/sources/comiket_events.c
 * Port of server/src/collectors/comiketEvents.js — fetchHead reachability
 * probe → ONE intel item (uid comiket-events|portal). OTHER portal-status
 * family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://www.comiket.co.jp/info-a/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['event','doujin','comiket', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("event"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("doujin"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("comiket"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "Comiket Preparatory Committee");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "portal";                 /* → uid comiket-events|portal */
  it.title        = "Comiket — doujin event calendar";
  it.summary      = "Comiket + Comic City schedule — Tokyo Big Sight / Makuhari Messe occupancy spikes";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[comiket-events] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def comiket_events_def = {
  .id = "comiket-events", .collector = "social",
  .name = "Comiket / Comic City calendar", .name_ja = "コミケ / コミックシティ",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(comiket_events_def)
