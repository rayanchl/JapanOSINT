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
#include "source.h"
#include "lib/feedlib.h"
#include "lib/jocore.h"   /* jo_truncation_notice_ex() — the ONE builder */
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WINDOW_DAYS 7
#define PAGE_SIZE   100      /* speech API max per request */
#define MAX_PAGES   200      /* exhaustive-ok: runaway guard on the page walk */

/* string-truthy field, else NULL (JSON null / empty → NULL). */
static const char *sv(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}

static void add_str(cJSON *o, const char *k, const char *v) {
  cJSON_AddItemToObject(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL), from = now - WINDOW_DAYS * 86400;
  /* strftime rather than snprintf("%04d-%02d-%02d", tm_year + 1900, …): tm_year
   * is an int the compiler cannot bound, so that form can emit up to 33 bytes
   * into these 11-byte buffers and -Wformat-truncation says so. strftime is
   * bounded by construction — it writes nothing and returns 0 rather than
   * cutting a date in half. The rendering is identical for every year this can
   * see. gmtime_r's NULL return is checked too; it was not before, and reading
   * an unset `struct tm` would have put a garbage window on the query. */
  struct tm gt, gf;
  char to_s[11], from_s[11];
  if (!gmtime_r(&now, &gt) || !gmtime_r(&from, &gf) ||
      !strftime(to_s,   sizeof to_s,   "%Y-%m-%d", &gt) ||
      !strftime(from_s, sizeof from_s, "%Y-%m-%d", &gf)) {
    fprintf(stderr, "[diet-records] cannot render the query window as a date\n");
    return -1;
  }

  /* Walk every page of the window. The old MAX_RECORDS=300 stopped mid-window,
   * so a busy sitting week lost its later speeches even though the API would
   * have served them (docs/SOURCE_EXHAUSTIVENESS.md). */
  /* Three ways this walk can end, and they are NOT the same outcome:
   *   - the window is genuinely exhausted            → honest empty / complete
   *   - the FIRST fetch failed, so nothing was read  → a real error (rc -1)
   *   - a LATER fetch failed, or MAX_PAGES stopped a walk the API would have
   *     continued                                     → we have rows but the
   *     window is incomplete; that is truncation, and house rule 2 says it is
   *     reported as a record, not swallowed by a `break`.
   * The old `return emitted > 0 ? 0 : -1` collapsed all three into "did we get
   * anything", so a recess week (the Diet does not sit for months at a time)
   * quarantined a source that had just correctly reported no speeches. */
  int emitted = 0, start = 1, pages_read = 0;
  int hard_error = 0;
  int cut = 0;                 /* 0 none · 1 fetch failed mid-walk · 2 page cap */
  for (int page = 0; page < MAX_PAGES; page++) {
    char url[512];
    snprintf(url, sizeof url,
      "https://kokkai.ndl.go.jp/api/speech?from=%s&until=%s"
      "&maximumRecords=%d&startRecord=%d&recordPacking=json",
      from_s, to_s, PAGE_SIZE, start);

    cJSON *json = feed_get_json(ctx->http, url, 25000);
    if (!json) {
      if (page == 0) hard_error = 1;   /* nothing read at all — a real failure */
      else           cut = 1;          /* partial window — reported below */
      break;
    }

    cJSON *recs = cJSON_GetObjectItem(json, "speechRecord");
    if (!cJSON_IsArray(recs) || cJSON_GetArraySize(recs) == 0) {
      cJSON_Delete(json); break;
    }

    cJSON *r;
    cJSON_ArrayForEach(r, recs) {
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
      const char *sid = sv(r, "speechID");
      const char *speaker = sv(r, "speaker");
      if (!sid) continue;
      if (speaker && strcmp(speaker, "会議録情報") == 0) continue; /* masthead */

      const char *house   = sv(r, "nameOfHouse");
      const char *meeting = sv(r, "nameOfMeeting");
      const char *issue   = sv(r, "issue");
      const char *date    = sv(r, "date");
      const char *speech  = sv(r, "speech");
      const char *surl    = sv(r, "speechURL");
      const char *murl    = sv(r, "meetingURL");

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
      add_str(p, "speaker_yomi", sv(r, "speakerYomi"));
      add_str(p, "speaker_group", sv(r, "speakerGroup"));
      add_str(p, "speaker_position", sv(r, "speakerPosition"));
      add_str(p, "issue_id", sv(r, "issueID"));
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
    pages_read++;
    if (got < PAGE_SIZE || next <= 0) break;   /* last page */
    start = next;
    /* The API still has more and only the runaway guard is stopping us. */
    if (page + 1 >= MAX_PAGES) cut = 2;
  }

  /* Rule 2: what we did not take is reported as data, not as a log line
   * nobody reads (docs/SOURCE_EXHAUSTIVENESS.md). Built by the ONE notice
   * builder (lib/jocore.h) so this record is byte-shaped like every other
   * collector-truncation-notice in the tree; the window and the walk's own
   * counters ride along as extras.
   *
   * The `query` is the window, which reproduces this collector's historical
   * uid exactly: "diet-records|truncation:<from>..<until>". One row per window.
   *
   * records_available is -1 and MUST be: the API states nextRecordPosition,
   * not a total for the window, so how many speeches remain is genuinely
   * unknown and any number here would be invented. */
  if (cut) {
    char window[32];
    snprintf(window, sizeof window, "%s..%s", from_s, to_s);
    cJSON *extra = cJSON_CreateObject();
    if (extra) {
      cJSON_AddStringToObject(extra, "window_from", from_s);
      cJSON_AddStringToObject(extra, "window_until", to_s);
      cJSON_AddNumberToObject(extra, "pages_read", pages_read);
      cJSON_AddNumberToObject(extra, "next_record_position", start);
      cJSON_AddBoolToObject(extra, "more_pages_pending", 1);
      cJSON_AddNumberToObject(extra, "declared_max_pages", MAX_PAGES);
    }
    jo_truncation_notice_ex(sink, "diet-records", window, emitted, -1,
      cut == 2 ? "the MAX_PAGES ceiling stopped a walk the API would have continued"
               : "a page fetch failed part-way through the window",
      cut == 2 ? "raise MAX_PAGES in collectors/sources/diet_records.c — see "
                 "docs/SOURCE_EXHAUSTIVENESS.md"
               : "transient upstream failure; the next scheduled run re-walks "
                 "the whole window",
      extra);
  }

  fprintf(stderr, "[diet-records] emitted %d over %d page(s)%s\n",
          emitted, pages_read,
          hard_error ? " (first fetch failed)" : (cut ? " (truncated)" : ""));
  /* Zero speeches after a clean fetch is the Diet not sitting, not a fault. */
  return hard_error ? -1 : 0;      /* honest empty is not an error */
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
