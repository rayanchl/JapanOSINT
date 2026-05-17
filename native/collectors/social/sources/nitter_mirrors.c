/* collectors/social/sources/nitter_mirrors.c
 * Port of server/src/collectors/nitterMirrors.js — per-instance fetchHead
 * reachability probe → ONE intel item per mirror
 * (uid = nitter-mirrors|<url> == intelUid(SOURCE_ID, url)).
 * title/host use new URL(url).hostname. OTHER portal-status family. */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

static const struct { const char *url; const char *host; } INSTANCES[] = {
  { "https://nitter.net/",              "nitter.net" },
  { "https://nitter.tiekoetter.com/",   "nitter.tiekoetter.com" },
  { "https://nitter.privacydev.net/",   "nitter.privacydev.net" },
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  char now[32]; probe_iso_now(now, sizeof now);
  int ok = 0;
  for (size_t i = 0; i < sizeof INSTANCES / sizeof INSTANCES[0]; i++) {
    const char *url = INSTANCES[i].url, *host = INSTANCES[i].host;
    int live = probe_head(ctx->http, url);

    /* tags: ['twitter','nitter','mirror', live?'reachable':'unreachable'] */
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("twitter"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("nitter"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("mirror"));
    cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
    char *tj = cJSON_PrintUnformatted(tags);

    /* properties: { reachable, host } — EXACT JS key order */
    cJSON *p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "reachable", live);
    cJSON_AddStringToObject(p, "host", host);
    char *pj = cJSON_PrintUnformatted(p);

    char title[128];
    snprintf(title, sizeof title, "Nitter mirror %s", host);

    intel_item it = {0};
    it.remote_key   = url;                      /* → uid nitter-mirrors|<url> */
    it.title        = title;
    it.summary      = "X/Twitter RSS proxy — no API key required";
    it.link         = url;
    it.lang         = "en";
    it.published_at = now;
    it.tags_json    = tj;
    it.properties_json = pj;
    int rc = sink->emit(sink, &it);

    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(p);
    if (rc < 0) ok = -1;
    fprintf(stderr, "[nitter-mirrors] %s reachable=%d\n", host, live);
  }
  return ok;
}

static const source_def nitter_mirrors_def = {
  .id = "nitter-mirrors", .collector = "social",
  .name = "Nitter mirror network", .name_ja = "Nitter ミラー",
   .update_interval_sec = 1800, .run = run };
REGISTER_SOURCE(nitter_mirrors_def)
