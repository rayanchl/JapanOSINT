/* collectors/social/sources/misskey_timeline.c
 * Port of server/src/collectors/misskeyTimeline.js.
 * POST https://misskey.io/api/notes/global-timeline with body {limit:30}
 * (+ {i:<MISSKEY_TOKEN>} if set). Keep notes with visibility=public or no
 * visibility → intel rows. uid = misskey-timeline|<note.id>.
 * Disabled when MISSKEY_DISABLED=1 (→ 0 rows).
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MISSKEY_BASE "https://misskey.io"
#define API_URL MISSKEY_BASE "/api/notes/global-timeline"
#define TIMEOUT_MS 10000

/* (s || '').slice(0,max) → malloc'd byte-prefix (NULL if input NULL). */
static char *slice_str(const char *s, size_t max) {
  if (!s) return NULL;
  size_t L = strlen(s);
  if (L > max) L = max;
  char *o = malloc(L + 1);
  memcpy(o, s, L);
  o[L] = 0;
  return o;
}

static const char *str_or_null(const cJSON *o, const char *k) {
  if (!o) return NULL;
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *dis = getenv("MISSKEY_DISABLED");
  if (dis && !strcmp(dis, "1")) {
    fprintf(stderr, "[misskey-timeline] disabled (MISSKEY_DISABLED=1)\n");
    return 0;
  }

  const char *token = getenv("MISSKEY_TOKEN");
  cJSON *req = cJSON_CreateObject();
  cJSON_AddNumberToObject(req, "limit", 30);
  if (token && *token) cJSON_AddStringToObject(req, "i", token);
  char *body = cJSON_PrintUnformatted(req);
  cJSON_Delete(req);

  const char *hdrs[] = { "Content-Type: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  cJSON *notes = feed_post_json(ctx->http, API_URL, body, hdrs, TIMEOUT_MS);
  free(body);
  if (!notes || !cJSON_IsArray(notes)) {
    if (notes) cJSON_Delete(notes);
    return -1;
  }

  int n = 0;
  cJSON *note;
  cJSON_ArrayForEach(note, notes) {
    /* keep visibility === 'public' || !visibility */
    cJSON *vis = cJSON_GetObjectItem(note, "visibility");
    if (vis && cJSON_IsString(vis) && vis->valuestring[0]
        && strcmp(vis->valuestring, "public") != 0)
      continue;

    const char *nid = str_or_null(note, "id");
    const cJSON *user = cJSON_GetObjectItem(note, "user");
    const char *text = str_or_null(note, "text");
    const char *uname = str_or_null(user, "username");

    /* title = text.slice(0,120) || (username ? @username : 'note') */
    char *title = slice_str(text, 120);
    char unbuf[160];
    if (!title || !title[0]) {
      free(title);
      if (uname) { snprintf(unbuf, sizeof unbuf, "@%s", uname); title = strdup(unbuf); }
      else title = strdup("note");
    }
    char *summary = slice_str(text, 240);
    if (summary && !summary[0]) { free(summary); summary = NULL; }

    char linkbuf[256];
    snprintf(linkbuf, sizeof linkbuf, "%s/notes/%s", MISSKEY_BASE, nid ? nid : "");

    cJSON *pj = cJSON_CreateObject();            /* EXACT JS key order */
    const char *aname = str_or_null(user, "name");
    cJSON_AddItemToObject(pj, "author_name",
      aname ? cJSON_CreateString(aname) : cJSON_CreateNull());
    const char *aloc = str_or_null(user, "location");
    cJSON_AddItemToObject(pj, "author_location",
      aloc ? cJSON_CreateString(aloc) : cJSON_CreateNull());
    cJSON *rc = cJSON_GetObjectItem(note, "repliesCount");
    cJSON_AddNumberToObject(pj, "reply_count",
      (rc && cJSON_IsNumber(rc)) ? rc->valuedouble : 0);
    cJSON *nc = cJSON_GetObjectItem(note, "renoteCount");
    cJSON_AddNumberToObject(pj, "renote_count",
      (nc && cJSON_IsNumber(nc)) ? nc->valuedouble : 0);
    char *pjs = cJSON_PrintUnformatted(pj);

    intel_item it = {0};
    it.remote_key   = nid;                       /* uid = misskey-timeline|<id> */
    it.title        = title;
    it.body         = text;
    it.summary      = summary;
    it.link         = linkbuf;
    it.author       = uname;
    it.lang         = "ja";
    it.published_at = str_or_null(note, "createdAt");
    it.record_type  = "misskey-timeline";
    it.properties_json = pjs;
    it.tags_json    = "[\"fediverse\",\"misskey\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(title);
    free(summary);
    free(pjs);
    cJSON_Delete(pj);
  }
  cJSON_Delete(notes);
  fprintf(stderr, "[misskey-timeline] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def misskey_timeline_def = {
  .id = "misskey-timeline", .collector = "social",
  .name = "Misskey Global Timeline", .name_ja = "Misskey グローバルタイムライン",
   .update_interval_sec = 600, .run = run,
};
REGISTER_SOURCE(misskey_timeline_def)
