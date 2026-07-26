/* core/breach_adapter.c — see breach_adapter.h. */
#include "breach_adapter.h"
#include "breach_index.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void add_sn(cJSON *o, const char *k, const char *v) {
  if (v) cJSON_AddStringToObject(o, k, v);
  else   cJSON_AddNullToObject(o, k);
}

static void iso_now(char *out, size_t n) {
  time_t t = time(NULL);
  struct tm g;
#if defined(_WIN32)
  gmtime_s(&g, &t);
#else
  gmtime_r(&t, &g);
#endif
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &g);
}

/* Wrap a user term as a single quoted FTS5 phrase (doubles embedded quotes) so
 * it is never parsed as query syntax. Mirrors breach_store.c's fts_phrase. */
static void fts_phrase(const char *q, char *out, size_t cap) {
  size_t o = 0;
  if (cap < 3) { if (cap) out[0] = 0; return; }
  out[o++] = '"';
  for (const char *p = q; *p && o + 2 < cap - 1; p++) {
    if (*p == '"') out[o++] = '"';
    out[o++] = *p;
  }
  out[o++] = '"';
  out[o] = 0;
}

/* Build the intel-item object in EXACT row_to_item(full) key order for a breach
 * record. Strings are copied, so caller may free the source buffers after. */
static cJSON *build_breach_item(const char *keyid, const char *type,
                                const char *value, const char *source_id,
                                int has_secret, long long count,
                                const char *first_seen, const char *breach_name) {
  char uid[160];
  snprintf(uid, sizeof uid, "breach:%s", keyid);

  const char *bname = (breach_name && *breach_name) ? breach_name
                    : (source_id ? source_id : "breach");
  char title[288];
  snprintf(title, sizeof title, "%s exposed in %s", type[0] ? type : "identifier", bname);
  const char *summary = has_secret
    ? "Identifier exposed with a leaked secret (hidden). Reveal requires operator access."
    : "Identifier exposed in this breach.";

  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("breach"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(type[0] ? type : "identifier"));

  cJSON *pv = cJSON_CreateObject();
  add_sn(pv, "source_id", source_id);
  cJSON_AddStringToObject(pv, "source_name", bname);
  add_sn(pv, "source_name_ja", NULL);
  cJSON_AddStringToObject(pv, "category", "breach");
  cJSON_AddStringToObject(pv, "collection_method", "breach-corpus");
  add_sn(pv, "source_url", NULL);
  add_sn(pv, "item_url", NULL);
  add_sn(pv, "sub_source_id", NULL);
  add_sn(pv, "fetched_at", first_seen);
  add_sn(pv, "published_at", NULL);
  add_sn(pv, "license", NULL);
  cJSON_AddNullToObject(pv, "confidence");

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "type", type[0] ? type : "identifier");
  if (value) cJSON_AddStringToObject(props, "value", value);   /* identifier, not a secret */
  else       cJSON_AddNullToObject(props, "value");            /* passwords are hash-only */
  cJSON_AddBoolToObject(props, "has_secret", has_secret != 0);
  cJSON_AddNumberToObject(props, "count", (double)count);
  cJSON_AddStringToObject(props, "keyid", keyid);

  cJSON *out = cJSON_CreateObject();
  cJSON_AddStringToObject(out, "uid", uid);
  add_sn(out, "source_id", source_id);
  cJSON_AddStringToObject(out, "title", title);
  cJSON_AddStringToObject(out, "summary", summary);
  add_sn(out, "link", NULL);
  add_sn(out, "author", NULL);
  add_sn(out, "language", NULL);
  add_sn(out, "published_at", NULL);
  add_sn(out, "fetched_at", first_seen);
  cJSON_AddItemToObject(out, "tags", tags);
  cJSON_AddNullToObject(out, "lat");
  cJSON_AddNullToObject(out, "lon");
  add_sn(out, "geom_source", NULL);
  add_sn(out, "geom_at", NULL);
  cJSON_AddStringToObject(out, "record_type", "breach_record");
  add_sn(out, "sub_source_id", NULL);
  cJSON_AddItemToObject(out, "provenance", pv);
  cJSON_AddItemToObject(out, "properties", props);
  return out;
}

char *breach_adapter_item_by_uid(db_handle *db, const char *uid) {
  if (!db || !db->h || !uid || strncmp(uid, "breach:", 7) != 0) return NULL;
  const char *keyid = uid + 7;
  static const char *Q =
    "SELECT b.keyid,b.type,b.value,b.source_id,b.has_secret,b.count,b.first_seen,"
    "COALESCE(m.name,m.title,'') "
    "FROM breach_items b LEFT JOIN breach_meta m ON m.breach_id=b.source_id "
    "WHERE b.keyid=?1";
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h, Q, -1, &st, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(st, 1, keyid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st); return NULL; }

  const char *kid = (const char *)sqlite3_column_text(st, 0);
  const char *ty  = (const char *)sqlite3_column_text(st, 1);
  const char *val = (const char *)sqlite3_column_text(st, 2);   /* NULL for passwords */
  const char *src = (const char *)sqlite3_column_text(st, 3);
  int hs          = sqlite3_column_int(st, 4);
  long long cnt   = sqlite3_column_int64(st, 5);
  const char *fs  = (const char *)sqlite3_column_text(st, 6);
  const char *nm  = (const char *)sqlite3_column_text(st, 7);
  cJSON *item = build_breach_item(kid ? kid : keyid, ty ? ty : "", val, src, hs, cnt, fs, nm);
  sqlite3_finalize(st);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", item);
  char *js = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return js;
}

char *breach_adapter_list(db_handle *db, const char *source_id,
                          const char *q, const char *cursor, int limit) {
  if (!db || !db->h || !source_id || !*source_id) return NULL;
  int safe = limit > 0 ? limit : 50;
  if (safe < 1) safe = 1;
  if (safe > 200) safe = 200;
  long long cur_id = (cursor && *cursor) ? atoll(cursor) : 0;
  int has_q = (q && *q);

  char sql[640];
  if (has_q) {
    snprintf(sql, sizeof sql,
      "SELECT b.keyid,b.type,b.value,b.source_id,b.has_secret,b.count,b.first_seen,b.id,"
      "COALESCE(m.name,m.title,'') "
      "FROM breach_items b JOIN breach_fts ON breach_fts.rowid=b.id "
      "LEFT JOIN breach_meta m ON m.breach_id=b.source_id "
      "WHERE b.source_id=?1 AND breach_fts MATCH ?2 AND b.id>?3 "
      "ORDER BY b.id ASC LIMIT ?4");
  } else {
    snprintf(sql, sizeof sql,
      "SELECT b.keyid,b.type,b.value,b.source_id,b.has_secret,b.count,b.first_seen,b.id,"
      "COALESCE(m.name,m.title,'') "
      "FROM breach_items b "
      "LEFT JOIN breach_meta m ON m.breach_id=b.source_id "
      "WHERE b.source_id=?1 AND b.id>?2 "
      "ORDER BY b.id ASC LIMIT ?3");
  }
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK) return NULL;
  int bi = 1;
  sqlite3_bind_text(st, bi++, source_id, -1, SQLITE_TRANSIENT);
  char mq[600];
  if (has_q) { fts_phrase(q, mq, sizeof mq); sqlite3_bind_text(st, bi++, mq, -1, SQLITE_TRANSIENT); }
  sqlite3_bind_int64(st, bi++, cur_id);
  sqlite3_bind_int(st, bi++, safe);

  cJSON *data = cJSON_CreateArray();
  int n = 0;
  long long last_id = 0;
  while (sqlite3_step(st) == SQLITE_ROW) {
    const char *kid = (const char *)sqlite3_column_text(st, 0);
    const char *ty  = (const char *)sqlite3_column_text(st, 1);
    const char *val = (const char *)sqlite3_column_text(st, 2);
    const char *src = (const char *)sqlite3_column_text(st, 3);
    int hs          = sqlite3_column_int(st, 4);
    long long cnt   = sqlite3_column_int64(st, 5);
    const char *fs  = (const char *)sqlite3_column_text(st, 6);
    last_id         = sqlite3_column_int64(st, 7);
    const char *nm  = (const char *)sqlite3_column_text(st, 8);
    cJSON_AddItemToArray(data,
      build_breach_item(kid ? kid : "", ty ? ty : "", val, src, hs, cnt, fs, nm));
    n++;
  }
  sqlite3_finalize(st);

  cJSON *page = cJSON_CreateObject();
  if (n == safe) {
    char c[32]; snprintf(c, sizeof c, "%lld", last_id);
    cJSON_AddStringToObject(page, "next_cursor", c);
  } else {
    cJSON_AddNullToObject(page, "next_cursor");
  }
  cJSON_AddNumberToObject(page, "limit", safe);
  cJSON_AddNullToObject(page, "total");

  cJSON *filters = cJSON_CreateObject();
  cJSON_AddStringToObject(filters, "source", source_id);
  add_sn(filters, "q", has_q ? q : NULL);
  cJSON_AddNullToObject(filters, "q_alt");
  cJSON_AddNullToObject(filters, "lang");
  cJSON_AddNullToObject(filters, "record_type");
  cJSON_AddNullToObject(filters, "sub_source_id");
  cJSON_AddNullToObject(filters, "has_geom");

  char ts[40]; iso_now(ts, sizeof ts);
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "fetched_at", ts);
  cJSON_AddItemToObject(meta, "filters", filters);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", data);
  cJSON_AddItemToObject(env, "page", page);
  cJSON_AddItemToObject(env, "meta", meta);
  char *js = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return js;
}

char *breach_adapter_reveal_by_uid(db_handle *db, const char *uid) {
  if (!db || !db->h || !uid || strncmp(uid, "breach:", 7) != 0) return NULL;
  const char *keyid = uid + 7;

  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT type,value FROM breach_items WHERE keyid=?1", -1, &st, NULL)
      != SQLITE_OK) return NULL;
  sqlite3_bind_text(st, 1, keyid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st); return NULL; }
  char tybuf[16] = {0}, valbuf[512] = {0};
  const char *ty = (const char *)sqlite3_column_text(st, 0);
  const char *val = (const char *)sqlite3_column_text(st, 1);
  if (ty) snprintf(tybuf, sizeof tybuf, "%s", ty);
  if (val) snprintf(valbuf, sizeof valbuf, "%s", val);
  sqlite3_finalize(st);

  if (!valbuf[0]) {
    /* password records are hash-only — nothing to decrypt */
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "found", 1);
    cJSON_AddStringToObject(o, "type", tybuf);
    cJSON_AddStringToObject(o, "note", "password records are hash-only; no cleartext to reveal");
    char *js = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return js;
  }

  cJSON *out = NULL;
  breach_index_lookup(breach_type_parse(tybuf), valbuf, 1 /* reveal */, &out);
  if (!out) return NULL;
  char *js = cJSON_PrintUnformatted(out);
  cJSON_Delete(out);
  return js;
}
