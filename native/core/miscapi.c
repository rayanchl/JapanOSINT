#include "miscapi.h"
#include "source_registry.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

static void iso_now(char *buf, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000));
}

/* one SELECT * row → cJSON object, sqlite type → JSON type (mirrors
 * better-sqlite3 row objects + JSON.stringify). */
static cJSON *row_obj(sqlite3_stmt *s) {
  cJSON *o = cJSON_CreateObject();
  int n = sqlite3_column_count(s);
  for (int i = 0; i < n; i++) {
    const char *k = sqlite3_column_name(s, i);
    switch (sqlite3_column_type(s, i)) {
      case SQLITE_NULL:    cJSON_AddNullToObject(o, k); break;
      case SQLITE_INTEGER: cJSON_AddNumberToObject(o, k,
                              (double)sqlite3_column_int64(s, i)); break;
      case SQLITE_FLOAT:   cJSON_AddNumberToObject(o, k,
                              sqlite3_column_double(s, i)); break;
      default:             cJSON_AddStringToObject(o, k,
                              (const char *)sqlite3_column_text(s, i)); break;
    }
  }
  return o;
}

char *miscapi_source_by_id(db_handle *db, const char *id) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "SELECT * FROM sources WHERE id = ?1",
                         -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  char *js = NULL;
  if (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *o = row_obj(s);
    js = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
  }
  sqlite3_finalize(s);
  return js;                                    /* NULL → 404 */
}

char *miscapi_set_schedule(db_handle *db, const char *id, const char *body) {
  cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
  cJSON *jm = jb ? cJSON_GetObjectItem(jb, "mode") : NULL;
  const char *mode = (jm && cJSON_IsString(jm)) ? jm->valuestring : "";
  int ok = !strcmp(mode, "map_cron") || !strcmp(mode, "search_only");
  if (!ok) { if (jb) cJSON_Delete(jb); return strdup("\1bad"); }

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE sources SET schedule_mode=?1 WHERE id=?2", -1, &s, NULL)
      != SQLITE_OK) { if (jb) cJSON_Delete(jb); return NULL; }
  sqlite3_bind_text(s, 1, mode, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, id, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  int changed = sqlite3_changes(db->h);
  sqlite3_finalize(s);
  if (jb) cJSON_Delete(jb);
  if (changed == 0) return NULL;                 /* unknown id → 404 */
  return miscapi_source_by_id(db, id);           /* updated row */
}

char *miscapi_source_logs(db_handle *db, const char *id, int limit) {
  /* getSourceById guard first — unknown id is a 404, not an empty list. */
  sqlite3_stmt *chk;
  int exists = 0;
  if (sqlite3_prepare_v2(db->h, "SELECT 1 FROM sources WHERE id = ?1",
                         -1, &chk, NULL) == SQLITE_OK) {
    sqlite3_bind_text(chk, 1, id, -1, SQLITE_TRANSIENT);
    exists = sqlite3_step(chk) == SQLITE_ROW;
  }
  sqlite3_finalize(chk);
  if (!exists) return NULL;

  int lim = limit > 0 ? limit : 50;
  if (lim > 500) lim = 500;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT * FROM fetch_log WHERE source_id = ?1 "
        "ORDER BY timestamp DESC, id DESC LIMIT ?2", -1, &s, NULL) != SQLITE_OK)
    return NULL;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 2, lim);
  cJSON *arr = cJSON_CreateArray();
  while (sqlite3_step(s) == SQLITE_ROW)
    cJSON_AddItemToArray(arr, row_obj(s));
  sqlite3_finalize(s);
  char *js = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return js;
}

char *miscapi_layer_geojson(const char *layer_id) {
  /* getLayerDefinitions(): a layer exists iff some registry source declares
   * it. data_cache is gone, so the body is the documented empty FC whose
   * _meta.sources lists that layer's contributing source ids. */
  cJSON *srcs = cJSON_CreateArray();
  int n = src_meta_count();
  for (int i = 0; i < n; i++) {
    const src_meta *m = src_meta_at(i);
    if (m->layer && strcmp(m->layer, layer_id) == 0)
      cJSON_AddItemToArray(srcs, cJSON_CreateString(m->id));
  }
  if (cJSON_GetArraySize(srcs) == 0) { cJSON_Delete(srcs); return NULL; }

  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "layer", layer_id);
  cJSON_AddStringToObject(meta, "message",
                          "Use /api/data/:layerId for live collector output.");
  cJSON_AddItemToObject(meta, "sources", srcs);

  cJSON *fc = cJSON_CreateObject();
  cJSON_AddStringToObject(fc, "type", "FeatureCollection");
  cJSON_AddItemToObject(fc, "features", cJSON_CreateArray());
  cJSON_AddItemToObject(fc, "_meta", meta);
  char *js = cJSON_PrintUnformatted(fc);
  cJSON_Delete(fc);
  return js;
}

char *miscapi_follow_recent(int limit) {
  (void)limit;
  char ts[40]; iso_now(ts, sizeof ts);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddNumberToObject(o, "count", 0);
  cJSON_AddItemToObject(o, "events", cJSON_CreateArray());
  cJSON_AddStringToObject(o, "timestamp", ts);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}
