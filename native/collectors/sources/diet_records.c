/* collectors/government/sources/diet_records.c
 * Diet legislative record — NDL 国会会議録検索 API (kokkai.ndl.go.jp/api/speech).
 * Free, no auth, JSON. One intel_item per speech in a rolling recent window,
 * keyed by speechID. Surveillance value: who (member + party) said what, in
 * which committee, on what date — the full speech text feeds FTS + the entity
 * enricher (person/company/place extraction). No speeches / fetch failure →
 * emits nothing (honest empty — no fabricated records).
 *
 * uid = diet-records|<speechID>. The masthead pseudo-record (speaker
 * "会議録情報", speechOrder 0) is skipped — it carries no speaker. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WINDOW_DAYS 7
#define PAGE_SIZE   100      /* speech API max per request */
#define MAX_RECORDS 300      /* cap a busy window (idempotent upsert backfills) */

static void add_str(cJSON *o, const char *k, const char *v) {
  cJSON_AddItemToObject(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL), from = now - WINDOW_DAYS * 86400;
  struct tm gt, gf; gmtime_r(&now, &gt); gmtime_r(&from, &gf);
  char to_s[11], from_s[11];
  snprintf(to_s,   sizeof to_s,   "%04d-%02d-%02d", gt.tm_year+1900, gt.tm_mon+1, gt.tm_mday);
  snprintf(from_s, sizeof from_s, "%04d-%02d-%02d", gf.tm_year+1900, gf.tm_mon+1, gf.tm_mday);

  int emitted = 0, start = 1;
  while (emitted < MAX_RECORDS) {
    char url[512];
    snprintf(url, sizeof url,
      "https://kokkai.ndl.go.jp/api/speech?from=%s&until=%s"
      "&maximumRecords=%d&startRecord=%d&recordPacking=json",
      from_s, to_s, PAGE_SIZE, start);

    cJSON *json = feed_get_json(ctx->http, url, 25000);
    if (!json) break;

    cJSON *recs = cJSON_GetObjectItem(json, "speechRecord");
    if (!cJSON_IsArray(recs) || cJSON_GetArraySize(recs) == 0) {
      cJSON_Delete(json); break;
    }

    cJSON *r;
    cJSON_ArrayForEach(r, recs) {
      if (emitted >= MAX_RECORDS) break;
      const char *sid = jo_sv(r, "speechID");
      const char *speaker = jo_sv(r, "speaker");
      if (!sid) continue;
      if (speaker && strcmp(speaker, "会議録情報") == 0) continue; /* masthead */

      const char *house   = jo_sv(r, "nameOfHouse");
      const char *meeting = jo_sv(r, "nameOfMeeting");
      const char *issue   = jo_sv(r, "issue");
      const char *date    = jo_sv(r, "date");
      const char *speech  = jo_sv(r, "speech");
      const char *surl    = jo_sv(r, "speechURL");
      const char *murl    = jo_sv(r, "meetingURL");

      /* title = speaker (the actor); summary = house · meeting · issue (date) */
      char summary[256]; summary[0] = 0; int w = 0;
      const char *parts[3] = { house, meeting, issue };
      for (int k = 0; k < 3; k++) if (parts[k] && parts[k][0]) {
        if (w) strncat(summary, " \xc2\xb7 ", sizeof summary - strlen(summary) - 1);
        strncat(summary, parts[k], sizeof summary - strlen(summary) - 1);
        w = 1;
      }
      if (date) { strncat(summary, " (", sizeof summary - strlen(summary) - 1);
        strncat(summary, date, sizeof summary - strlen(summary) - 1);
        strncat(summary, ")", sizeof summary - strlen(summary) - 1); }

      cJSON *p = cJSON_CreateObject();
      cJSON_AddItemToObject(p, "session",
        cJSON_GetObjectItem(r,"session") && cJSON_IsNumber(cJSON_GetObjectItem(r,"session"))
          ? cJSON_CreateNumber(cJSON_GetObjectItem(r,"session")->valuedouble)
          : cJSON_CreateNull());
      add_str(p, "house", house);
      add_str(p, "meeting", meeting);
      add_str(p, "issue", issue);
      add_str(p, "speaker", speaker);
      add_str(p, "speaker_yomi", jo_sv(r, "speakerYomi"));
      add_str(p, "speaker_group", jo_sv(r, "speakerGroup"));
      add_str(p, "speaker_position", jo_sv(r, "speakerPosition"));
      add_str(p, "issue_id", jo_sv(r, "issueID"));
      add_str(p, "meeting_url", murl);
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("diet"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("kokkai"));
      if (house) cJSON_AddItemToArray(tags, cJSON_CreateString(house));
      char *tj = cJSON_PrintUnformatted(tags);
      cJSON_Delete(tags);

      char tbuf[96];
      const char *title = speaker;
      if (!title) { snprintf(tbuf, sizeof tbuf, "%s %s",
        house ? house : "", meeting ? meeting : ""); title = tbuf; }

      intel_item it = {0};
      it.remote_key      = sid;
      it.title           = title;
      it.summary         = w ? summary : NULL;
      it.body            = speech;          /* full text → FTS + entity enrich */
      it.author          = speaker;
      it.lang            = "ja";
      it.published_at    = date;
      it.link            = surl ? surl : murl;
      it.record_type     = "diet-speech";
      it.properties_json = pj;
      it.tags_json       = tj;
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(pj); free(tj);
    }

    int got = cJSON_GetArraySize(recs);
    cJSON *nrp = cJSON_GetObjectItem(json, "nextRecordPosition");
    int next = (nrp && cJSON_IsNumber(nrp)) ? (int)nrp->valuedouble : 0;
    cJSON_Delete(json);
    if (got < PAGE_SIZE || next <= 0) break;   /* last page */
    start = next;
  }

  fprintf(stderr, "[diet-records] emitted %d\n", emitted);
  /* run() is a STATUS code, not a row count: fetch/parse failures already
   * returned -1 above, so reaching here with zero rows is an honest empty.
   * Returning -1 here had scheduler.c quarantine the source for working. */
  return 0;
}

static const source_def diet_records_def = {
  .id = "diet-records", .collector = "government",
  .name = "Diet Records (Kokkai)", .name_ja = "国会会議録",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "api",
  .url = "https://kokkai.ndl.go.jp/api/speech",
  .description = "National Diet Library Kokkai Gijiroku — member speeches by committee/date",
  .license = "NDL API (free)", .free_tier = 1,
};
REGISTER_SOURCE(diet_records_def)
