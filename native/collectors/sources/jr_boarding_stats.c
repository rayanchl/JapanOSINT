/* collectors/transport/sources/jr_boarding_stats.c
 * Port of server/src/collectors/jrBoardingStats.js — per-source fetchHead
 * reachability probe → ONE intel item per operator
 * (uid = jr-boarding-stats|<op> == intelUid(SOURCE_ID, s.op)).
 * OTHER portal-status family (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

static const struct { const char *op; const char *url; } SOURCES[] = {
  { "JR-East",     "https://www.jreast.co.jp/passenger/" },
  { "Tokyo-Metro", "https://www.tokyometro.jp/corporate/enterprise/passenger_rail/" },
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  char now[32]; probe_iso_now(now, sizeof now);
  int ok = 0;
  for (size_t i = 0; i < sizeof SOURCES / sizeof SOURCES[0]; i++) {
    const char *op = SOURCES[i].op, *url = SOURCES[i].url;
    int live = probe_head(ctx->http, url);

    /* tags: ['transit','rail','boarding', live?'reachable':'unreachable'] */
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("transit"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("rail"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("boarding"));
    cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
    char *tj = cJSON_PrintUnformatted(tags);

    /* properties: { operator, reachable } — EXACT JS key order */
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "operator", op);
    cJSON_AddBoolToObject(p, "reachable", live);
    char *pj = cJSON_PrintUnformatted(p);

    char title[128];
    snprintf(title, sizeof title, "%s — per-station boarding stats", op);

    intel_item it = {0};
    it.remote_key   = op;                       /* → uid jr-boarding-stats|<op> */
    it.title        = title;
    it.summary      = "Annual passenger boardings (乗降客数) by station";
    it.link         = url;
    it.lang         = "ja";
    it.published_at = now;
    it.tags_json    = tj;
    it.properties_json = pj;
    int rc = sink->emit(sink, &it);

    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(p);
    if (rc < 0) ok = -1;
    fprintf(stderr, "[jr-boarding-stats] %s reachable=%d\n", op, live);
  }
  return ok;
}

static const source_def jr_boarding_stats_def = {
  .id = "jr-boarding-stats", .collector = "transport",
  .name = "JR / Tokyo Metro boarding stats", .name_ja = "JR / 東京メトロ 乗降客数",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(jr_boarding_stats_def)
