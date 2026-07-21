/* collectors/social/sources/note_com_profiles.c
 * Port of server/src/collectors/noteComProfiles.js (note-com-profiles).
 * Key-free public note.com v2 search API; queries a fixed JP-relevant term
 * set, emits author profiles as non-spatial intel. Dedup by urlname across
 * queries. honest empty on failure. uid = note-com-profiles|<urlname>. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOURCE_ID "note-com-profiles"

static const char *QUERIES[] = {
  "%E6%9D%B1%E4%BA%AC",                         /* 東京 */
  "OSINT",
  "%E9%98%B2%E7%81%BD",                         /* 防災 */
  "%E3%82%B9%E3%82%BF%E3%83%BC%E3%83%88%E3%82%A2%E3%83%83%E3%83%97", /* スタートアップ */
  "%E3%82%BB%E3%82%AD%E3%83%A5%E3%83%AA%E3%83%86%E3%82%A3",          /* セキュリティ */
};
static const char *QUERY_RAW[] = {
  "東京", "OSINT", "防災", "スタートアップ", "セキュリティ",
};
#define NQ ((int)(sizeof(QUERIES)/sizeof(QUERIES[0])))

static const char *sv(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

/* seen-set: simple dynamic string array. */
static int seen_has(char **arr, int n, const char *s) {
  for (int i = 0; i < n; i++) if (strcmp(arr[i], s) == 0) return 1;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = {
    "user-agent: JapanOSINT/1.0",
    "accept: application/json",
    NULL,
  };
  char **seen = NULL;
  int sn = 0, scap = 0;
  int n = 0;

  for (int qi = 0; qi < NQ; qi++) {
    char url[256];
    snprintf(url, sizeof url,
      "https://note.com/api/v2/searches?context=user&q=%s&size=20",
      QUERIES[qi]);
    cJSON *data = feed_get_json_h(ctx->http, url, hdrs, 12000);
    if (!data) continue;

    cJSON *d = cJSON_GetObjectItem(data, "data");
    cJSON *users = NULL;
    if (d) {
      cJSON *u = cJSON_GetObjectItem(d, "users");
      if (u) users = cJSON_GetObjectItem(u, "contents");
      if (!users) users = cJSON_GetObjectItem(d, "contents");
    }

    if (cJSON_IsArray(users)) {
      cJSON *u;
      cJSON_ArrayForEach(u, users) {
        const char *urlname = sv(u, "urlname");
        if (!urlname) urlname = sv(u, "id");
        if (!urlname) {
          const cJSON *idv = cJSON_GetObjectItem(u, "id");
          if (idv && cJSON_IsNumber(idv)) {
            static char idb[32];
            snprintf(idb, sizeof idb, "%g", idv->valuedouble);
            urlname = idb;
          }
        }
        if (!urlname || !urlname[0]) continue;
        if (seen_has(seen, sn, urlname)) continue;
        if (sn == scap) {
          scap = scap ? scap * 2 : 32;
          seen = realloc(seen, scap * sizeof(char *));
        }
        seen[sn++] = strdup(urlname);

        const char *nick = sv(u, "nickname");
        const char *name = sv(u, "name");
        const char *prof = sv(u, "profile");
        const char *created = sv(u, "created_at");

        char uidb[256];
        snprintf(uidb, sizeof uidb, "%s|%s", SOURCE_ID, urlname);
        char link[256];
        snprintf(link, sizeof link, "https://note.com/%s", urlname);

        cJSON *tags = cJSON_CreateArray();
        cJSON_AddItemToArray(tags, cJSON_CreateString("note.com"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("profile"));
        char qtag[64];
        snprintf(qtag, sizeof qtag, "q:%s", QUERY_RAW[qi]);
        cJSON_AddItemToArray(tags, cJSON_CreateString(qtag));
        char *tj = cJSON_PrintUnformatted(tags);

        cJSON *props = cJSON_CreateObject();
        cJSON_AddStringToObject(props, "urlname", urlname);
        cJSON *fc = cJSON_GetObjectItem(u, "followerCount");
        if (fc && cJSON_IsNumber(fc))
          cJSON_AddNumberToObject(props, "followers", fc->valuedouble);
        else cJSON_AddNullToObject(props, "followers");
        cJSON *nc = cJSON_GetObjectItem(u, "noteCount");
        if (nc && cJSON_IsNumber(nc))
          cJSON_AddNumberToObject(props, "notes", nc->valuedouble);
        else cJSON_AddNullToObject(props, "notes");
        cJSON_AddStringToObject(props, "matched_query", QUERY_RAW[qi]);
        cJSON_AddStringToObject(props, "source", "note_api");
        char *pj = cJSON_PrintUnformatted(props);

        intel_item it = {0};
        it.uid = uidb;
        it.title = nick ? nick : (name ? name : urlname);
        it.summary = prof;
        it.body = prof;
        it.link = link;
        it.author = nick ? nick : urlname;
        it.lang = "ja";
        it.published_at = created;
        it.record_type = SOURCE_ID;
        it.tags_json = tj;
        it.properties_json = pj;
        if (sink->emit(sink, &it) >= 0) n++;

        free(tj); free(pj);
        cJSON_Delete(tags); cJSON_Delete(props);
      }
    }
    cJSON_Delete(data);
  }

  for (int i = 0; i < sn; i++) free(seen[i]);
  free(seen);
  fprintf(stderr, "[note-com-profiles] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def note_com_profiles_def = {
  .id = "note-com-profiles", .collector = "social",
  .name = "note.com profile search", .name_ja = "note.com プロフィール検索",
  .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(note_com_profiles_def)
