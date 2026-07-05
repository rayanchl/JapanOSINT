/* collectors/sources/global_ransomlook.c
 * OSINT service — RANSOMLOOK. ransomlook.io tracks ransomware/extortion group
 * leak-site victims. Two real, keyless modes over the public JSON API:
 *   - scheduled feed  : GET https://www.ransomlook.io/api/recent  → recent victims
 *   - entity pivot    : GET https://www.ransomlook.io/api/search/<entity> when
 *                       ctx->entity is set (company/org/domain).
 * Emits one intel_item per victim (org, group, discovered date). Real fetch or
 * honest empty — nothing fabricated. Keyless LIVE. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* one victim object → one intel_item. Returns 1 if emitted.
 * ransomlook victim records commonly carry: "post_title"/"victim"/"name" (org),
 * "group_name"/"group" (actor), "discovered"/"published"/"date". We read all the
 * likely field spellings and emit only what is really present. */
static int rl_emit(intel_sink *sink, const cJSON *v) {
  if (!v || !cJSON_IsObject(v)) return 0;
  const char *org = jo_sv(v, "post_title");
  if (!org) org = jo_sv(v, "victim");
  if (!org) org = jo_sv(v, "name");
  if (!org) org = jo_sv(v, "title");
  const char *group = jo_sv(v, "group_name");
  if (!group) group = jo_sv(v, "group");
  const char *date = jo_sv(v, "discovered");
  if (!date) date = jo_sv(v, "published");
  if (!date) date = jo_sv(v, "date");
  const char *desc = jo_sv(v, "description");
  const char *link = jo_sv(v, "website");
  if (!link) link = jo_sv(v, "link");
  if (!org && !group) return 0;

  cJSON *data = cJSON_CreateObject();
  if (org)   cJSON_AddStringToObject(data, "victim", org);
  if (group) cJSON_AddStringToObject(data, "group", group);
  if (date)  cJSON_AddStringToObject(data, "discovered", date);
  if (desc)  cJSON_AddStringToObject(data, "description", desc);
  cJSON_AddStringToObject(data, "source", "ransomlook.io");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "RANSOMLOOK");
  cJSON_AddStringToObject(props, "source", "ransomlook.io");
  if (group) cJSON_AddStringToObject(props, "group", group);
  if (date)  cJSON_AddStringToObject(props, "discovered", date);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char summary[512];
  snprintf(summary, sizeof summary, "Ransomware victim%s%s%s%s",
           group ? " — group: " : "", group ? group : "",
           date ? " — " : "", date ? date : "");

  intel_item it = {0};
  it.remote_key      = org ? org : group;
  it.title           = org ? org : group;
  it.summary         = summary;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = date;
  it.link            = link;
  it.record_type     = "ransomware-victim";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"RANSOMLOOK\",\"ransomware\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Walk a parsed JSON payload, emitting every victim object we find. Tolerates a
 * top-level array of victims, or an object whose values are arrays of victims. */
static int rl_walk(intel_sink *sink, const cJSON *root) {
  int emitted = 0;
  if (cJSON_IsArray(root)) {
    const cJSON *v;
    cJSON_ArrayForEach(v, root) emitted += rl_emit(sink, v);
  } else if (cJSON_IsObject(root)) {
    const cJSON *child;
    cJSON_ArrayForEach(child, root) {
      if (cJSON_IsArray(child)) {
        const cJSON *v;
        cJSON_ArrayForEach(v, child) emitted += rl_emit(sink, v);
      } else if (cJSON_IsObject(child)) {
        emitted += rl_emit(sink, child);
      }
    }
  }
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char url[1024];
  const char *q = ctx->entity;
  if (q && *q) {
    char *enc = jo_urlencode(q);
    if (!enc) return -1;
    snprintf(url, sizeof url, "https://www.ransomlook.io/api/search/%s", enc);
    free(enc);
  } else {
    snprintf(url, sizeof url, "https://www.ransomlook.io/api/recent");
  }

  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "ransomlook");
  if (!body) return 0;   /* honest empty on fetch failure */

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int emitted = rl_walk(sink, root);
  cJSON_Delete(root);
  fprintf(stderr, "[ransomlook] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def ransomlook_def = {
  .id = "RANSOMLOOK", .collector = "osint",
  .name = "RansomLook Victim Tracker", .name_ja = "RansomLook 被害組織追跡",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://www.ransomlook.io/api/recent",
  .description = "ransomlook.io — ransomware/extortion leak-site victim tracker (recent feed + entity search)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(ransomlook_def)
