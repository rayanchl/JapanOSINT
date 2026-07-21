/* collectors/transport/sources/shinkansen_status.c
 * Port of server/src/collectors/shinkansenStatus.js — aggregates 5 official
 * Shinkansen operating-status portals (one JR operator each). Each is
 * best-effort probed (fetchHead) + fetchText snippet → ONE intel item per
 * operator (uid = shinkansen-status|<op>). OTHER portal-status family
 * (lib/probe.h). Honest unreachable flag; no fabricated status. */
#include "../../source.h"
#include "../../lib/probe.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct line { const char *op, *opslug, *line, *url; };
static const struct line LINES[] = {
  { "JR East",     "jr-east",     "Tohoku/Joetsu/Hokuriku Shinkansen",
    "https://traininfo.jreast.co.jp/shinkansen/" },
  { "JR Central",  "jr-central",  "Tokaido Shinkansen",
    "https://global.jr-central.co.jp/en/info/status/" },
  { "JR West",     "jr-west",     "Sanyo/Hokuriku Shinkansen",
    "https://www.westjr.co.jp/global/en/traffic/" },
  { "JR Kyushu",   "jr-kyushu",   "Kyushu Shinkansen",
    "https://www.jrkyushu.co.jp/unkou/" },
  { "JR Hokkaido", "jr-hokkaido", "Hokkaido Shinkansen",
    "https://www3.jrhokkaido.co.jp/traininfo/" },
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  int emitted = 0;
  for (size_t i = 0; i < sizeof LINES / sizeof LINES[0]; i++) {
    const struct line *l = &LINES[i];
    int live = probe_head(ctx->http, l->url);
    char now[32]; probe_iso_now(now, sizeof now);

    char *snippet = NULL;
    char *html = feed_get_text(ctx->http, l->url, 10000);
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

    char title[160], dfltsum[160];
    snprintf(title, sizeof title, "%s status (%s)", l->line, l->op);
    snprintf(dfltsum, sizeof dfltsum, "%s %s operating-status portal",
             l->op, l->line);
    const char *summary = (snippet && snippet[0]) ? snippet : dfltsum;

    /* tags: ['transit','shinkansen','rail',<opslug>, reach|unreach] */
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("transit"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("shinkansen"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("rail"));
    cJSON_AddItemToArray(tags, cJSON_CreateString(l->opslug));
    cJSON_AddItemToArray(tags,
      cJSON_CreateString(live ? "reachable" : "unreachable"));
    char *tj = cJSON_PrintUnformatted(tags);

    /* properties: { operator, line, reachable } — EXACT JS key order */
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "operator", l->op);
    cJSON_AddStringToObject(p, "line", l->line);
    cJSON_AddBoolToObject(p, "reachable", live);
    char *pj = cJSON_PrintUnformatted(p);

    intel_item it = {0};
    it.remote_key   = l->op;          /* → uid shinkansen-status|JR East … */
    it.title        = title;
    it.summary      = summary;
    it.body         = (snippet && snippet[0]) ? snippet : NULL;
    it.link         = l->url;
    it.lang         = "ja";
    it.published_at = now;
    it.tags_json    = tj;
    it.properties_json = pj;
    if (sink->emit(sink, &it) >= 0) emitted++;

    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(p);
    free(snippet);
  }
  fprintf(stderr, "[shinkansen-status] emitted %d\n", emitted);
  return emitted > 0 ? 0 : -1;
}

static const source_def shinkansen_status_def = {
  .id = "shinkansen-status", .collector = "transport",
  .name = "Shinkansen Line Status", .name_ja = "新幹線 運行状況",
   .update_interval_sec = 60, .run = run };
REGISTER_SOURCE(shinkansen_status_def)
