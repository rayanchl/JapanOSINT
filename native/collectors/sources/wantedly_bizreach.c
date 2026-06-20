/* collectors/social/sources/wantedly_bizreach.c
 * Port of server/src/collectors/wantedlyBizreach.js — fetchHead reachability
 * probe over 2 portals → ONE intel item each.
 * OTHER portal-status family (lib/probe.h). SEED/_meta envelope not ported. */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

struct probe { const char *op, *op_lc, *url; };
static const struct probe PROBES[] = {
  { "Wantedly", "wantedly", "https://www.wantedly.com/" },
  { "Bizreach", "bizreach", "https://www.bizreach.jp/" },
};
#define NP ((int)(sizeof(PROBES)/sizeof(PROBES[0])))

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0;
  char now[32]; probe_iso_now(now, sizeof now);

  for (int i = 0; i < NP; i++) {
    const struct probe *pr = &PROBES[i];
    int live = probe_head(ctx->http, pr->url);

    /* uid: intelUid(SOURCE_ID, op) → "<source>|<op>" (sink derives) */
    char rk[64]; snprintf(rk, sizeof rk, "%s", pr->op);

    char title[64];
    snprintf(title, sizeof title, "%s \xe2\x80\x94 public profiles", pr->op);

    /* tags: ['profile','corporate', op.toLowerCase(),
     *        live?'reachable':'unreachable','tos-caveat'] */
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("profile"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("corporate"));
    cJSON_AddItemToArray(tags, cJSON_CreateString(pr->op_lc));
    cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("tos-caveat"));
    char *tj = cJSON_PrintUnformatted(tags);

    /* properties: { operator, reachable, tos_caveat } — EXACT JS key order */
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "operator", pr->op);
    cJSON_AddBoolToObject(p, "reachable", live);
    cJSON_AddBoolToObject(p, "tos_caveat", 1);
    char *pj = cJSON_PrintUnformatted(p);

    intel_item it = {0};
    it.remote_key     = rk;
    it.title          = title;
    it.summary        = "Employer history + skills (ToS-rate-limited scraping)";
    it.link           = pr->url;
    it.lang           = "ja";
    it.published_at   = now;
    it.tags_json      = tj;
    it.properties_json = pj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(p);
  }
  fprintf(stderr, "[wantedly-bizreach] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def wantedly_bizreach_def = {
  .id = "wantedly-bizreach", .collector = "social",
  .name = "Wantedly + Bizreach profiles",
  .name_ja = "Wantedly + Bizreach \xe3\x83\x97\xe3\x83\xad\xe3\x83\x95\xe3\x82\xa3\xe3\x83\xbc\xe3\x83\xab",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(wantedly_bizreach_def)
