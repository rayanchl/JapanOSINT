/* core/storyapi.c — GET /api/stories/:id, the members of one story
 * (core/story.h: items that say the same thing within ±72 h, joined by the
 * embedding pod's post-batch hook).
 *
 * Members are read through the intel tenancy rule — this tenant + the shared
 * 'legacy' corpus — and a story never spans tenants, so a tenant sees either
 * the whole story or (for another tenant's story) a 404. Paged with
 * limit/offset and a measured total (house rule 2), oldest first by the row's
 * date (EMBED_ROW_DATE, the same rule the index's ts column holds). meta
 * states how membership was decided, so a reader can tell a story from a
 * simhash cluster. */
#include "storyapi.h"
#include "story.h"
#include "embed_pod.h"
#include "../third_party/cJSON.h"
#include "../third_party/mongoose.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void add_text(cJSON *o, const char *k, sqlite3_stmt *s, int i) {
  if (sqlite3_column_type(s, i) == SQLITE_NULL) cJSON_AddNullToObject(o, k);
  else cJSON_AddStringToObject(o, k, (const char *)sqlite3_column_text(s, i));
}

char *storyapi_get(db_handle *db, const char *tenant, const char *id,
                   const char *qs, int is_operator, int *status) {
  (void)is_operator;
  char lv[16] = "", ov[16] = "";
  struct mg_str q = mg_str(qs ? qs : "");
  mg_http_get_var(&q, "limit", lv, sizeof lv);
  mg_http_get_var(&q, "offset", ov, sizeof ov);
  int lim = atoi(lv), off = atoi(ov);
  if (lim <= 0) lim = 100;
  if (lim > 500) lim = 500;
  if (off < 0) off = 0;
  const char *ten = (tenant && *tenant) ? tenant : "legacy";
  if (!id || strncmp(id, "st-", 3) != 0 || strlen(id) > 40) {
    *status = 400;
    return strdup("{\"error\":\"bad_story_id\"}");
  }

  long total = -1;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "SELECT count(*) FROM intel_items WHERE story_id=?1"
        " AND (tenant_id='legacy' OR tenant_id=?2)", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, ten, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) total = (long)sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  if (total == 0) { *status = 404; return strdup("{\"error\":\"not_found\"}"); }

  if (sqlite3_prepare_v2(db->h,
        "SELECT uid,source_id,title,link,published_at,fetched_at,novelty,"
        " record_type, " EMBED_ROW_DATE " AS d FROM intel_items"
        " WHERE story_id=?1 AND (tenant_id='legacy' OR tenant_id=?2)"
        " ORDER BY d ASC, uid ASC LIMIT ?3 OFFSET ?4", -1, &s, NULL) != SQLITE_OK) {
    *status = 500;
    return strdup("{\"error\":\"server_error\"}");
  }
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, ten, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 3, lim);
  sqlite3_bind_int(s, 4, off);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "story_id", id);
  cJSON *arr = cJSON_AddArrayToObject(o, "members");
  int rc;
  while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
    cJSON *m = cJSON_CreateObject();
    add_text(m, "uid", s, 0);
    add_text(m, "source_id", s, 1);
    add_text(m, "title", s, 2);
    add_text(m, "link", s, 3);
    add_text(m, "published_at", s, 4);
    add_text(m, "fetched_at", s, 5);
    if (sqlite3_column_type(s, 6) == SQLITE_NULL) cJSON_AddNullToObject(m, "novelty");
    else cJSON_AddNumberToObject(m, "novelty", sqlite3_column_double(s, 6));
    add_text(m, "record_type", s, 7);
    cJSON_AddItemToArray(arr, m);
  }
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) { cJSON_Delete(o); *status = 500; return strdup("{\"error\":\"server_error\"}"); }

  cJSON *page = cJSON_AddObjectToObject(o, "page");
  cJSON_AddNumberToObject(page, "limit", lim);
  cJSON_AddNumberToObject(page, "offset", off);
  if (total < 0) cJSON_AddNullToObject(page, "total");
  else cJSON_AddNumberToObject(page, "total", (double)total);
  cJSON *meta = cJSON_AddObjectToObject(o, "meta");
  char ts[32];
  time_t now = time(NULL);
  struct tm tm;
  gmtime_r(&now, &tm);
  strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", &tm);
  cJSON_AddStringToObject(meta, "fetched_at", ts);
  cJSON_AddNumberToObject(meta, "tau", story_tau());
  cJSON_AddNumberToObject(meta, "window_hours", STORY_WINDOW_SEC / 3600);
  cJSON_AddNumberToObject(meta, "max_members", story_max_members());
  cJSON_AddBoolToObject(meta, "members_cap_reached",
                        total >= 0 && total >= story_max_members());
  cJSON_AddStringToObject(meta, "membership",
      "an item joins when its embedding has cosine >= tau to a member within "
      "window_hours of it, in the same tenant; stories are never merged with "
      "each other, and only rows the embedding pod has embedded can be members");
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  *status = 200;
  return js;
}
