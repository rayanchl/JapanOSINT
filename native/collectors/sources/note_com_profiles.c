/* collectors/social/sources/note_com_profiles.c
 * Port of server/src/collectors/noteComProfiles.js (note-com-profiles).
 * Key-free public note.com v2 search API; queries a fixed JP-relevant term
 * set, emits author profiles as non-spatial intel. Dedup by urlname across
 * queries. honest empty on failure. uid = note-com-profiles|<urlname>. */
#include "../../lib/jocore.h"
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
  int n = 0, fetched = 0;

  for (int qi = 0; qi < NQ; qi++) {
    char url[256];
    /* v2/searches has been retired — it returns note.com's HTML 404 page, so
     * every query parsed to nothing and the source emitted 0 forever. v3 is
     * live and keeps the same data.users.contents[] shape. */
    snprintf(url, sizeof url,
      "https://note.com/api/v3/searches?context=user&q=%s&size=20",
      QUERIES[qi]);
    cJSON *data = feed_get_json_h(ctx->http, url, hdrs, 12000);
    if (!data) continue;
    fetched++;

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
        const char *urlname = jo_sv(u, "urlname");
        if (!urlname) urlname = jo_sv(u, "id");
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

        const char *nick = jo_sv(u, "nickname");
        const char *name = jo_sv(u, "name");
        const char *prof = jo_sv(u, "profile");
        const char *created = jo_sv(u, "created_at");

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
        /* v3 names these follower_count/following_count (the camelCase
         * followerCount/noteCount of the old v2 shape never existed here, so
         * both numbers persisted as JSON null); there is no note count in the
         * v3 user object, so we do not invent one. */
        cJSON *fc = cJSON_GetObjectItem(u, "follower_count");
        if (fc && cJSON_IsNumber(fc))
          cJSON_AddNumberToObject(props, "followers", fc->valuedouble);
        cJSON *gc = cJSON_GetObjectItem(u, "following_count");
        if (gc && cJSON_IsNumber(gc))
          cJSON_AddNumberToObject(props, "following", gc->valuedouble);
        cJSON *off = cJSON_GetObjectItem(u, "is_official");
        if (off && cJSON_IsBool(off))
          cJSON_AddBoolToObject(props, "is_official", cJSON_IsTrue(off));
        const char *img = jo_sv(u, "profile_image_url");
        if (img) cJSON_AddStringToObject(props, "profile_image_url", img);
        const char *cd = jo_sv(u, "custom_domain");
        if (cd) cJSON_AddStringToObject(props, "custom_domain", cd);
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
  fprintf(stderr, "[note-com-profiles] emitted %d (%d/%d queries fetched)\n",
          n, fetched, NQ);
  /* rc is a STATUS, not a count. A search that legitimately matches no
   * profiles must not be reported as an errored run — core/scheduler.c feeds
   * any non-zero rc to anomaly_detect() and quarantines the source. Only a
   * total fetch failure (no query returned JSON at all) is an error. */
  return fetched > 0 ? 0 : -1;
}

static const source_def note_com_profiles_def = {
  .id = "note-com-profiles", .collector = "social",
  .name = "note.com profile search", .name_ja = "note.com プロフィール検索",
  .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(note_com_profiles_def)
