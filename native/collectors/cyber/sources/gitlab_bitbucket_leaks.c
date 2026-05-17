/* collectors/cyber/sources/gitlab_bitbucket_leaks.c
 * Port of server/src/collectors/gitlabBitbucketLeaks.js — fetchHead probe
 * over 2 hosts (key-gated per host) → ONE intel item per host
 * (uid gitlab-bitbucket-leaks|<op>). OTHER portal-status family (lib/probe.h). */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

/* PROBES = [[op,url],...] — verbatim JS order */
static const char *PROBES[][2] = {
  { "gitlab",    "https://gitlab.com/" },
  { "bitbucket", "https://bitbucket.org/" },
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *gt = getenv("GITLAB_TOKEN");
  const char *bt = getenv("BITBUCKET_TOKEN");
  int has_gitlab    = gt && *gt;               /* !!envFor('GITLAB_TOKEN') */
  int has_bitbucket = bt && *bt;               /* !!envFor('BITBUCKET_TOKEN') */
  char now[32]; probe_iso_now(now, sizeof now);
  int err = 0;

  for (size_t i = 0; i < sizeof(PROBES) / sizeof(PROBES[0]); i++) {
    const char *op  = PROBES[i][0];
    const char *url = PROBES[i][1];
    int live = probe_head(ctx->http, url);
    int is_gitlab = (i == 0);
    int has_key   = is_gitlab ? has_gitlab : has_bitbucket;

    char title[64];
    snprintf(title, sizeof title, "%s public code search", op);

    /* tags: ['leak','code-search', op, live?'reachable':'unreachable'] */
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("leak"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("code-search"));
    cJSON_AddItemToArray(tags, cJSON_CreateString(op));
    cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
    char *tj = cJSON_PrintUnformatted(tags);

    /* properties: { reachable, requires_key, has_key } — EXACT JS key order */
    cJSON *p = cJSON_CreateObject();
    cJSON_AddBoolToObject(p, "reachable", live);
    cJSON_AddBoolToObject(p, "requires_key", 1);
    cJSON_AddBoolToObject(p, "has_key", has_key);
    char *pj = cJSON_PrintUnformatted(p);

    /* summary: configured ? 'Configured' : `Set <OP>_TOKEN to enable code search` */
    char summary[64];
    const char *sum;
    if (has_key) {
      sum = "Configured";
    } else {
      char up[16];
      size_t k = 0;
      for (const char *s = op; *s && k + 1 < sizeof up; s++) {
        char c = *s;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        up[k++] = c;
      }
      up[k] = '\0';
      snprintf(summary, sizeof summary, "Set %s_TOKEN to enable code search", up);
      sum = summary;
    }

    intel_item it = {0};
    it.remote_key   = op;                       /* → uid gitlab-bitbucket-leaks|<op> */
    it.title        = title;
    it.summary      = sum;
    it.link         = url;
    it.published_at = now;
    it.tags_json    = tj;
    it.properties_json = pj;
    if (sink->emit(sink, &it) < 0) err = 1;

    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(p);
    fprintf(stderr, "[gitlab-bitbucket-leaks] %s reachable=%d has_key=%d\n", op, live, has_key);
  }
  return err ? -1 : 0;
}

static const source_def gitlab_bitbucket_leaks_def = {
  .id = "gitlab-bitbucket-leaks", .collector = "cyber",
  .name = "GitLab + Bitbucket leak hunt", .name_ja = "GitLab/Bitbucket 漏洩",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(gitlab_bitbucket_leaks_def)
