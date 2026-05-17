/* collectors/transport/sources/jr_west_delay.c
 * Port of server/src/collectors/jrWestDelay.js — fetchHead reachability +
 * fetchText stripped body snippet → ONE intel item
 * (uid = jr-west-delay|jr-west-portal). OTHER portal-status family
 * (lib/probe.h). snippet = html_strip(body).slice(0,240); summary =
 * snippet || default (empty == falsy). Snippet is display-only (not in
 * uid/props) so a UTF-8-boundary byte cut is correctness-neutral. */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/htmlparse.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORTAL_URL "https://trafficinfo.westjr.co.jp/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PORTAL_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  char *snippet = NULL;
  char *html = feed_get_text(ctx->http, PORTAL_URL, 10000);
  if (html) {
    char *stripped = html_strip(html);
    if (stripped) {
      size_t cut = strlen(stripped);
      if (cut > 240) {
        cut = 240;
        while (cut > 0 && ((unsigned char)stripped[cut] & 0xC0) == 0x80)
          cut--;
        stripped[cut] = 0;
      }
      snippet = stripped;
    }
    free(html);
  }
  const char *summary = (snippet && snippet[0])
    ? snippet
    : "JR West line-by-line operating status and delay information";

  /* tags: ['transit','rail','delay','jr-west', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("transit"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("rail"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("delay"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("jr-west"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "JR West");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "jr-west-portal";
  it.title        = "JR West 列車運行情報 (train status)";
  it.summary      = summary;
  it.body         = (snippet && snippet[0]) ? snippet : NULL;
  it.link         = PORTAL_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  free(snippet);
  fprintf(stderr, "[jr-west-delay] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def jr_west_delay_def = {
  .id = "jr-west-delay", .collector = "transport",
  .name = "JR West Delay Info", .name_ja = "JR西日本 運行情報",
   .update_interval_sec = 60, .run = run };
REGISTER_SOURCE(jr_west_delay_def)
