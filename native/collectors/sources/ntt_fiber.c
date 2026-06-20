/* collectors/infrastructure/sources/ntt_fiber.c
 * Port of server/src/collectors/nttFiber.js — fetchHead reachability probe
 * over the NTT East + NTT West FLET'S fault portals → TWO intel items
 * (uid = ntt-fiber|<op> == intelUid(SOURCE_ID, p.op)). No public coverage/
 * outage JSON API; 0 data rows is correct (RULE 8). REFERENCE boj_stats.c
 * (lib/probe.h). */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct portal { const char *op, *url, *tag; };
static const struct portal PORTALS[] = {
  { "NTT East", "https://www.ntt-east.co.jp/info-st/", "ntt-east" },
  { "NTT West", "https://www.ntt-west.co.jp/info_st/", "ntt-west" },
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  int emitted = 0;
  for (size_t i = 0; i < sizeof PORTALS / sizeof PORTALS[0]; i++) {
    const struct portal *pt = &PORTALS[i];
    int live = probe_head(ctx->http, pt->url);
    char now[32]; probe_iso_now(now, sizeof now);

    /* tags: ['telecom','fiber','ntt', <op-slug>, reachable|unreachable] */
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("telecom"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("fiber"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("ntt"));
    cJSON_AddItemToArray(tags, cJSON_CreateString(pt->tag));
    cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
    char *tj = cJSON_PrintUnformatted(tags);

    /* properties: { operator, reachable, public_feed } — EXACT JS order */
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "operator", pt->op);
    cJSON_AddBoolToObject(p, "reachable", live);
    cJSON_AddBoolToObject(p, "public_feed", 0);
    char *pj = cJSON_PrintUnformatted(p);

    char title[64];
    snprintf(title, sizeof title, "%s FLET'S fault / service information",
             pt->op);

    intel_item it = {0};
    it.remote_key   = pt->op;             /* → uid ntt-fiber|NTT East/West */
    it.title        = title;
    it.summary      = "FLET'S fiber service-area and fault information (interactive lookup; no public coverage/outage JSON API)";
    it.link         = pt->url;
    it.lang         = "ja";
    it.published_at = now;
    it.tags_json    = tj;
    it.properties_json = pj;
    if (sink->emit(sink, &it) >= 0) emitted++;

    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(p);
    fprintf(stderr, "[ntt-fiber] %s reachable=%d\n", pt->op, live);
  }
  return emitted > 0 ? 0 : -1;
}

static const source_def ntt_fiber_def = {
  .id = "ntt-fiber", .collector = "infrastructure",
  .name = "NTT Fiber Coverage", .name_ja = "NTT光回線 エリア",
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(ntt_fiber_def)
