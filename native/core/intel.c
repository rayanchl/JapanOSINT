#include "intel.h"
#include "fts.h"
#include "alert_eval.h"
#include "simhash.h"
#include "../third_party/sqlite3.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

typedef struct { db_handle *db; char source_id[128]; char tenant_id[64]; } sink_state;

static void iso_now(char *b, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  snprintf(b, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec/1000));
}

/* Exact Node upsert: INSERT … ON CONFLICT(uid) DO UPDATE, preserving
 * DERIVED geometry across re-runs.
 *
 * 'exif' joined 'llm' here with roadmap 27. Both are coordinates this system
 * worked out for itself — the LLM geocoder from the text, media.c from an
 * image's EXIF GPS block — for items whose collector supplies no geometry of
 * its own. Those collectors emit lat=NULL every run, so without this the very
 * next scheduled refresh would fall through to `ELSE excluded.lat` and erase
 * the coordinate, and the item would silently drop off the map again. A
 * collector that DOES supply geometry still wins: excluded.lat is checked
 * first, because a real upstream coordinate outranks an inferred one. */
static const char *SQL_UPSERT =
 "INSERT INTO intel_items"
 " (uid,source_id,title,body,summary,link,author,language,published_at,"
 "  fetched_at,tags,properties,lat,lon,geom_source,geom_at,record_type,"
 "  sub_source_id,geometry,tenant_id)"
 " VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20)"
 " ON CONFLICT(uid) DO UPDATE SET"
 "  title=excluded.title, body=excluded.body, summary=excluded.summary,"
 "  link=excluded.link, author=excluded.author, language=excluded.language,"
 "  published_at=excluded.published_at, fetched_at=excluded.fetched_at,"
 "  tags=excluded.tags, properties=excluded.properties,"
 "  lat=CASE WHEN excluded.lat IS NOT NULL THEN excluded.lat"
 "          WHEN intel_items.geom_source IN ('llm','exif') THEN intel_items.lat"
 "          ELSE excluded.lat END,"
 "  lon=CASE WHEN excluded.lon IS NOT NULL THEN excluded.lon"
 "          WHEN intel_items.geom_source IN ('llm','exif') THEN intel_items.lon"
 "          ELSE excluded.lon END,"
 "  geom_source=CASE WHEN excluded.geom_source IS NOT NULL THEN excluded.geom_source"
 "          WHEN intel_items.geom_source IN ('llm','exif') THEN intel_items.geom_source"
 "          ELSE excluded.geom_source END,"
 "  geom_at=CASE WHEN excluded.geom_source IS NOT NULL THEN excluded.geom_at"
 "          WHEN intel_items.geom_source IN ('llm','exif') THEN intel_items.geom_at"
 "          ELSE excluded.geom_at END,"
 "  geometry=COALESCE(excluded.geometry,intel_items.geometry),"
 "  record_type=COALESCE(excluded.record_type,intel_items.record_type),"
 "  sub_source_id=COALESCE(excluded.sub_source_id,intel_items.sub_source_id);";

static void bind_txt(sqlite3_stmt *s, int i, const char *v) {
  if (v) sqlite3_bind_text(s, i, v, -1, SQLITE_TRANSIENT);
  else sqlite3_bind_null(s, i);
}

/* ftsMirror.writeOne: DELETE old fts row via uid_map, INSERT segmented,
 * upsert uid_map(uid,rowid). keywords stays '' (enricher fills later). */
static void fts_write(sqlite3 *h, const char *uid, const char *title,
                      const char *body, const char *summary) {
  sqlite3_stmt *s;
  sqlite3_int64 old = -1;
  if (sqlite3_prepare_v2(h, "SELECT rowid FROM intel_items_fts_uid_map WHERE uid=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) old = sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  if (old >= 0) {
    if (sqlite3_prepare_v2(h, "DELETE FROM intel_items_fts WHERE rowid=?1",
                           -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_int64(s, 1, old); sqlite3_step(s); sqlite3_finalize(s);
    }
  }
  char *st = fts_segment(title ? title : "");
  char *sb = fts_segment(body ? body : "");
  char *ss = fts_segment(summary ? summary : "");
  /* The rowid must come from THIS insert. Reading last_insert_rowid()
   * unconditionally meant that when the FTS insert failed or was never prepared,
   * the map recorded whatever rowid the preceding intel_items upsert produced —
   * and the delete at the top of the next write then removed a DIFFERENT item's
   * search-index row, evicting it from /api/intel/search with no error. */
  int inserted = 0;
  if (sqlite3_prepare_v2(h,
      "INSERT INTO intel_items_fts(uid,title,body,summary,keywords)"
      " VALUES(?1,?2,?3,?4,'')", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, st, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, sb, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, ss, -1, SQLITE_TRANSIENT);
    inserted = (sqlite3_step(s) == SQLITE_DONE);
    sqlite3_finalize(s);
  }
  free(st); free(sb); free(ss);
  if (!inserted) {
    fprintf(stderr, "[intel] fts insert failed for %s: %s\n", uid, sqlite3_errmsg(h));
    return;                       /* leave the existing map row alone */
  }
  sqlite3_int64 rid = sqlite3_last_insert_rowid(h);
  if (sqlite3_prepare_v2(h,
      "INSERT INTO intel_items_fts_uid_map(uid,rowid) VALUES(?1,?2)"
      " ON CONFLICT(uid) DO UPDATE SET rowid=excluded.rowid",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 2, rid);
    sqlite3_step(s); sqlite3_finalize(s);
  }
}

static int emit(struct intel_sink *self, const intel_item *it) {
  sink_state *st = self->ctx;
  sqlite3 *h = st->db->h;

  /* uid: explicit → "<source>|<remote_key>" → (hash fallback = P5 toolkit). */
  char uid[512];
  if (it->uid && *it->uid) snprintf(uid, sizeof uid, "%s", it->uid);
  else if (it->remote_key && *it->remote_key)
    snprintf(uid, sizeof uid, "%s|%s", st->source_id, it->remote_key);
  else { fprintf(stderr, "[intel] %s: item without uid/remote_key skipped\n",
                 st->source_id); return -1; }

  char fetched[40]; iso_now(fetched, sizeof fetched);
  int has_geo = it->has_geo;
  const char *geom_src = has_geo ? "native" : NULL;

  /* Is this genuinely a new row? source.h documents emit() as "1 if a NEW
   * row, 0 if updated", but that was never true: after
   * "INSERT ... ON CONFLICT(uid) DO UPDATE" sqlite3_changes() is 1 either
   * way, so emit() always returned 1. One indexed PK probe makes the
   * documented contract real.
   *
   * This matters now that alert_eval hangs off ingest: without it, a rule
   * with dedup_window_sec=0 would re-fire on every scheduled refresh of an
   * unchanged item — a collector on a 60s interval would emit an alert a
   * minute, forever. No existing caller branches on the return value
   * (checked across all 569 emit() call sites), so tightening it is safe. */
  int is_new = 1;
  {
    sqlite3_stmt *ex;
    if (sqlite3_prepare_v2(h, "SELECT 1 FROM intel_items WHERE uid=?1",
                           -1, &ex, NULL) == SQLITE_OK) {
      sqlite3_bind_text(ex, 1, uid, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(ex) == SQLITE_ROW) is_new = 0;
      sqlite3_finalize(ex);
    }
  }

  sqlite3_exec(h, "BEGIN", NULL, NULL, NULL);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, SQL_UPSERT, -1, &s, NULL) != SQLITE_OK) {
    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL); return -1;
  }
  bind_txt(s, 1, uid);
  bind_txt(s, 2, st->source_id);
  bind_txt(s, 3, it->title);
  bind_txt(s, 4, it->body);
  bind_txt(s, 5, it->summary);
  bind_txt(s, 6, it->link);
  bind_txt(s, 7, it->author);
  bind_txt(s, 8, it->lang);
  bind_txt(s, 9, it->published_at);
  bind_txt(s, 10, fetched);
  bind_txt(s, 11, it->tags_json ? it->tags_json : "[]");
  bind_txt(s, 12, it->properties_json ? it->properties_json : "{}");
  if (has_geo) { sqlite3_bind_double(s, 13, it->lat); sqlite3_bind_double(s, 14, it->lon); }
  else { sqlite3_bind_null(s, 13); sqlite3_bind_null(s, 14); }
  bind_txt(s, 15, geom_src);
  bind_txt(s, 16, has_geo ? fetched : NULL);
  bind_txt(s, 17, it->record_type);
  bind_txt(s, 18, it->sub_source_id);
  bind_txt(s, 19, it->geometry_geojson);
  bind_txt(s, 20, st->tenant_id[0] ? st->tenant_id : "legacy");
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) { sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL); return -1; }

  int changes = sqlite3_changes(h); /* 1 insert, or update */
  fts_write(h, uid, it->title, it->body, it->summary);
  sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);

  /* Alert matching (roadmap P0.1) — AFTER the commit, never inside it. An
   * alert write must not be able to roll back the ingest that produced it,
   * and a rule evaluation must see the row it is evaluating. Only new rows
   * are evaluated; re-ingest of an unchanged item is not news. */
  if (changes && is_new)
    alert_eval_on_item(st->db, st->tenant_id[0] ? st->tenant_id : "legacy", uid);

  /* Near-duplicate clustering (roadmap 25). Deliberately NOT gated on is_new:
   * an UPDATE can change title/body, and a stale fingerprint would cluster the
   * row by text it no longer has. */
  if (changes)
    simhash_on_item(st->db, st->tenant_id[0] ? st->tenant_id : "legacy",
                    uid, it->title, it->body);

  return (changes && is_new) ? 1 : 0;
}

intel_sink intel_sink_make(db_handle *db, const char *source_id,
                           const char *tenant_id) {
  sink_state *st = calloc(1, sizeof *st);
  st->db = db;
  snprintf(st->source_id, sizeof st->source_id, "%s", source_id ? source_id : "unknown");
  if (tenant_id) snprintf(st->tenant_id, sizeof st->tenant_id, "%s", tenant_id);
  intel_sink k; k.ctx = st; k.emit = emit;
  return k;
}
