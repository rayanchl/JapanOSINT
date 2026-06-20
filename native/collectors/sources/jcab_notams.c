/* collectors/transport/sources/jcab_notams.c
 * Port of server/src/collectors/jcabNotams.js — fetchHead reachability
 * probe → ONE intel item (uid jcab-notams|notam-portal). OTHER
 * portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_URL "https://aim-naviserv.mlit.go.jp/aimjp/web/notam.html"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PROBE_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* tags: ['aviation','notam','tfr','mlit', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("aviation"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("notam"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("tfr"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("mlit"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "MLIT JCAB AIM Naviservice");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "notam-portal";           /* → uid jcab-notams|notam-portal */
  it.title        = "JCAB NOTAMs (notices to airmen)";
  it.summary      = "Japan Civil Aviation Bureau NOTAM portal — temporary flight restrictions, exercise areas, VIP transit corridors";
  it.link         = PROBE_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  fprintf(stderr, "[jcab-notams] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def jcab_notams_def = {
  .id = "jcab-notams", .collector = "transport",
  .name = "JCAB NOTAMs", .name_ja = "JCAB NOTAM",
   .update_interval_sec = 1800, .run = run };
REGISTER_SOURCE(jcab_notams_def)
