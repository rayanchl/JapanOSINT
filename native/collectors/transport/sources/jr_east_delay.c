/* collectors/transport/sources/jr_east_delay.c
 * Port of server/src/collectors/jreastDelay.js — fetchHead reachability +
 * fetchText body snippet → ONE intel item
 * (uid = jr-east-delay|jr-east-portal). OTHER portal-status family
 * (lib/probe.h). JS: live=fetchHead(PORTAL); then a SEPARATE fetchText whose
 * body is stripped (html_strip == replace(/<[^>]+>/g,' ').replace(/\s+/g,' ')
 * .trim()) and .slice(0,240); summary = snippet || default (empty == falsy).
 * snippet is display-only (NOT in uid/props) so byte- vs UTF-16-slice is
 * correctness-neutral; we cut <=240 bytes on a UTF-8 boundary. */
#include "../../../source.h"
#include "../../../lib/probe.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/htmlparse.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORTAL_URL "https://traininfo.jreast.co.jp/delay_certificate/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int live = probe_head(ctx->http, PORTAL_URL);
  char now[32]; probe_iso_now(now, sizeof now);

  /* bodySnippet = stripped body, .slice(0,240); NULL if fetch failed,
     "" if body empty (both -> summary default). */
  char *snippet = NULL;
  char *html = feed_get_text(ctx->http, PORTAL_URL, 10000);
  if (html) {
    char *stripped = html_strip(html);          /* JS .replace.replace.trim */
    if (stripped) {
      size_t cut = strlen(stripped);
      if (cut > 240) {                          /* .slice(0,240) */
        cut = 240;
        while (cut > 0 && ((unsigned char)stripped[cut] & 0xC0) == 0x80)
          cut--;                                /* don't split a UTF-8 char */
        stripped[cut] = 0;
      }
      snippet = stripped;                       /* may be "" */
    }
    free(html);
  }
  /* summary = bodySnippet || 'Daily line-level delay certificates...' */
  const char *summary = (snippet && snippet[0])
    ? snippet : "Daily line-level delay certificates with cause text";

  /* tags: ['transit','rail','delay','jr-east', live?'reachable':'unreachable'] */
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("transit"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("rail"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("delay"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("jr-east"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(live ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);

  /* properties: { operator, reachable } — EXACT JS key order */
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "operator", "JR East");
  cJSON_AddBoolToObject(p, "reachable", live);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key   = "jr-east-portal";
  it.title        = "JR East delay certificates (遅延証明書)";
  it.summary      = summary;
  it.link         = PORTAL_URL;
  it.lang         = "ja";
  it.published_at = now;
  it.tags_json    = tj;
  it.properties_json = pj;
  int rc = sink->emit(sink, &it);

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
  free(snippet);
  fprintf(stderr, "[jr-east-delay] probe reachable=%d\n", live);
  return rc >= 0 ? 0 : -1;
}

static const source_def jr_east_delay_def = {
  .id = "jr-east-delay", .collector = "transport",
  .name = "JR East delay certificates", .name_ja = "JR東日本 遅延証明書",
   .update_interval_sec = 1800, .run = run };
REGISTER_SOURCE(jr_east_delay_def)
