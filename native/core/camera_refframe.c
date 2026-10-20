/* core/camera_refframe.c — see camera_refframe.h. */
#include "camera_refframe.h"
#include "camera_feed.h"
#include "camera_privacy.h"
#include "camera_stills.h"
#include "evidence.h"
#include "hostgate.h"
#include "image_vec.h"
#include "../lib/vecindex.h"
#include "../source.h"
#include "../third_party/sqlite3.h"
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RF_HOSTS_MAX 64              /* distinct hosts tracked per tick      */

static long env_long(const char *k, long dflt) {
  const char *e = getenv(k);
  if (!e || !*e) return dflt;
  char *end = NULL;
  long v = strtol(e, &end, 10);
  return (end && end != e && v > 0) ? v : dflt;
}

static char *dupcol(sqlite3_stmt *s, int i) {
  const char *t = (const char *)sqlite3_column_text(s, i);
  return t ? strdup(t) : NULL;
}

void camera_refframe_migrate(db_handle *db) {
  if (!db || !db->h) return;
  sqlite3_exec(db->h,
    "CREATE TABLE IF NOT EXISTS camera_refframes ("
    "  item_uid TEXT PRIMARY KEY, camera_uid TEXT NOT NULL, url TEXT NOT NULL,"
    "  sha256 TEXT, blob_path TEXT, evidence_id TEXT, bytes INTEGER,"
    "  content_type TEXT, fetched_at TEXT, attempted_at TEXT NOT NULL,"
    "  last_error TEXT, fail_count INTEGER NOT NULL DEFAULT 0);"
    "CREATE INDEX IF NOT EXISTS idx_camera_refframes_cam"
    "  ON camera_refframes(camera_uid);", NULL, NULL, NULL);
  /* evidence.c fails CLOSED on sources.capture_evidence, so the pseudo-source
   * is created opted IN — on the insert only, so an operator who later turns
   * it off is not overridden on the next boot (uploadapi_migrate's rule). Not
   * the pod's own id: db_seed_sources() inserts that row at boot with the
   * flag off, before this could tell an insert from an existing row. */
  ensure_column(db, "sources", "capture_evidence", "INTEGER NOT NULL DEFAULT 0");
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT OR IGNORE INTO sources(id,name,type,category,url,status) "
        "VALUES('" CAM_REFFRAME_EV_SID "','Camera Reference Frames','api',"
        "'maintenance','internal://camera-refframe','online')",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_step(s);
    sqlite3_finalize(s);
    if (sqlite3_changes(db->h) > 0)
      sqlite3_exec(db->h, "UPDATE sources SET capture_evidence=1 WHERE id='"
                   CAM_REFFRAME_EV_SID "'", NULL, NULL, NULL);
  }
}

typedef struct { char *uid, *sid, *props; long long rowid; int due; } rf_row;

typedef struct {
  long walked, due, not_indexable, no_feed, host_deferred, fetched, failed,
       not_preserved;
} rf_stats;

/* The page after the cursor: camera rows and rows opted into stills, each
 * with whether it is due. Read and FINALIZED before the first fetch. */
static int read_page(db_handle *db, long long after, int limit, long max_age,
                     rf_row **out) {
  *out = NULL;
  char age[48];
  snprintf(age, sizeof age, "-%ld seconds", max_age);
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(db->h,
      "SELECT i.rowid, i.uid, i.source_id, i.properties,"
      "       NOT EXISTS (SELECT 1 FROM camera_refframes f WHERE f.item_uid=i.uid"
      "                   AND f.attempted_at > datetime('now', ?3))"
      "  FROM intel_items i WHERE i.rowid > ?1 AND i.rowid IN"
      "  (SELECT rowid FROM intel_items WHERE record_type IN ('camera','camera-discovery')"
      "   UNION ALL SELECT rowid FROM intel_items WHERE capture_stills=1)"
      " ORDER BY i.rowid LIMIT ?2", -1, &q, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_int64(q, 1, after);
  sqlite3_bind_int(q, 2, limit);
  sqlite3_bind_text(q, 3, age, -1, SQLITE_TRANSIENT);
  rf_row *v = calloc((size_t)limit, sizeof *v);
  int n = 0;
  while (v && n < limit && sqlite3_step(q) == SQLITE_ROW) {
    rf_row *r = &v[n];
    r->rowid = sqlite3_column_int64(q, 0);
    r->uid = dupcol(q, 1);
    r->sid = dupcol(q, 2);
    r->props = dupcol(q, 3);
    r->due = sqlite3_column_int(q, 4);
    if (!r->uid) { free(r->sid); free(r->props); continue; }
    n++;
  }
  sqlite3_finalize(q);
  if (!v) return -1;
  *out = v;
  return n;
}

/* The evidence row evidence_capture() wrote (or already had) for these bytes
 * under this camera: its id and relative blob path. 0 when the bytes were not
 * preserved (capture off for the pseudo-source, over the blob cap, budget). */
static int evidence_of(db_handle *db, const char *sha, const char *uid,
                       char *id, size_t icap, char *rel, size_t rcap) {
  sqlite3_stmt *q;
  int ok = 0;
  if (sqlite3_prepare_v2(db->h, "SELECT id, blob_path FROM evidence WHERE"
                         " content_sha256=?1 AND item_uid=?2 LIMIT 1",
                         -1, &q, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(q, 1, sha, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(q, 2, uid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(q) == SQLITE_ROW && sqlite3_column_text(q, 0) &&
      sqlite3_column_text(q, 1)) {
    snprintf(id, icap, "%s", (const char *)sqlite3_column_text(q, 0));
    snprintf(rel, rcap, "%s", (const char *)sqlite3_column_text(q, 1));
    ok = 1;
  }
  sqlite3_finalize(q);
  return ok;
}

/* Upsert the record's row. `sha` NULL = a failed attempt (`err` says why):
 * the last good frame, if any, is kept — it is still the newest the camera
 * gave us, and its fetched_at says how old. */
static void put_row(db_handle *db, const char *uid, const char *cam,
                    const char *url, const char *sha, const char *rel,
                    const char *evid, size_t bytes, const char *ct,
                    const char *err) {
  sqlite3_stmt *q;
  const char *sql = sha
    ? "INSERT INTO camera_refframes(item_uid,camera_uid,url,sha256,blob_path,"
      " evidence_id,bytes,content_type,fetched_at,attempted_at,last_error,fail_count)"
      " VALUES(?1,?2,?3,?4,?5,?6,?7,?8,datetime('now'),datetime('now'),NULL,0)"
      " ON CONFLICT(item_uid) DO UPDATE SET camera_uid=excluded.camera_uid,"
      "  url=excluded.url, sha256=excluded.sha256, blob_path=excluded.blob_path,"
      "  evidence_id=excluded.evidence_id, bytes=excluded.bytes,"
      "  content_type=excluded.content_type, fetched_at=excluded.fetched_at,"
      "  attempted_at=excluded.attempted_at, last_error=NULL, fail_count=0"
    : "INSERT INTO camera_refframes(item_uid,camera_uid,url,attempted_at,"
      " last_error,fail_count) VALUES(?1,?2,?3,datetime('now'),?9,1)"
      " ON CONFLICT(item_uid) DO UPDATE SET camera_uid=excluded.camera_uid,"
      "  url=excluded.url, attempted_at=excluded.attempted_at,"
      "  last_error=excluded.last_error, fail_count=camera_refframes.fail_count+1";
  if (sqlite3_prepare_v2(db->h, sql, -1, &q, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(q, 1, uid, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(q, 2, cam, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(q, 3, url, -1, SQLITE_TRANSIENT);
  if (sha) {
    sqlite3_bind_text(q, 4, sha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(q, 5, rel, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(q, 6, evid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(q, 7, (sqlite3_int64)bytes);
    sqlite3_bind_text(q, 8, ct ? ct : "", -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_text(q, 9, err ? err : "fetch_failed", -1, SQLITE_TRANSIENT);
  }
  sqlite3_step(q);
  sqlite3_finalize(q);
}

int camera_refframe_tick(db_handle *db, http_client *http, volatile int *cancel) {
  if (!image_vec_base_url()) return 0;
  if (!db || !db->h) return -1;
  const vecindex_spec *vs = &image_vec_spec;
  camera_stills_migrate(db);          /* intel_items.capture_stills          */
  image_vec_migrate(db);              /* image_vec_meta holds the cursor     */
  camera_refframe_migrate(db);
  int walk = (int)env_long("JO_REFFRAME_MAX_WALK", 2000);
  int per_tick = (int)env_long("JO_REFFRAME_PER_TICK", 20);
  int per_host = (int)env_long("JO_REFFRAME_HOST_PER_TICK", 2);
  long max_age = env_long("JO_REFFRAME_MAX_AGE_SEC", 604800);

  long long after = vecindex_meta_long(db, vs, "refframe_cursor");
  if (after < 0) after = 0;
  rf_row *rows = NULL;
  int n = read_page(db, after, walk, max_age, &rows);
  if (n < 0) return -1;

  http_client *own = NULL;
  if (!http) http = own = http_client_new();
  char hosts[RF_HOSTS_MAX][128];
  int hcount[RF_HOSTS_MAX], nh = 0;
  rf_stats st = {0};
  long long cursor = after;
  int stopped = 0;
  for (int i = 0; i < n; i++) {
    rf_row *r = &rows[i];
    if (stopped || (cancel && *cancel)) { stopped = 1; break; }
    st.walked++;
    if (!r->due) { cursor = r->rowid; continue; }
    if (!camera_record_indexable(r->props, r->sid)) {
      st.not_indexable++; cursor = r->rowid; continue; }
    camera_feed_target t[1];
    memset(t, 0, sizeof t);
    if (camera_feed_resolve(r->props, r->sid, t, 1) <= 0 || !t[0].url) {
      camera_feed_free(t, 1);
      st.no_feed++; cursor = r->rowid; continue;
    }
    st.due++;
    if (st.fetched + st.failed >= per_tick) {
      /* the budget is spent: this row and every one after it stay due, and
       * the cursor stops in front of them */
      camera_feed_free(t, 1);
      stopped = 1;
      break;
    }
    char host[128] = "";
    hostgate_url_host(t[0].url, host, sizeof host);
    int hi = -1;
    for (int k = 0; k < nh; k++) if (!strcmp(hosts[k], host)) hi = k;
    if (hi < 0 && nh < RF_HOSTS_MAX) {
      hi = nh++;
      snprintf(hosts[hi], sizeof hosts[hi], "%s", host);
      hcount[hi] = 0;
    }
    if (hi >= 0 && hcount[hi] >= per_host) {
      /* politeness: left due, picked up on a later lap */
      camera_feed_free(t, 1);
      st.host_deferred++; cursor = r->rowid; continue;
    }
    if (hi >= 0) hcount[hi]++;
    char cam[640];
    if (!camera_feed_target_id(r->uid, t[0].suffix, cam, sizeof cam))
      snprintf(cam, sizeof cam, "%s", r->uid);

    unsigned char *frame = NULL;
    size_t flen = 0;
    const char *ct = NULL, *why = NULL;
    long status = 0;
    char rbuf[32];
    if (camera_stills_fetch_frame(http, t[0].url, &frame, &flen, &ct, &status,
                                  &why, rbuf, sizeof rbuf)) {
      unsigned char d[SHA256_DIGEST_LENGTH];
      SHA256(frame, flen, d);
      char sha[65];
      for (int k = 0; k < SHA256_DIGEST_LENGTH; k++) snprintf(sha + k * 2, 3, "%02x", d[k]);
      evidence_capture(db, r->uid, CAM_REFFRAME_EV_SID, t[0].url, "GET", NULL,
                       status, NULL, frame, flen, ct);
      char evid[64], rel[256];
      if (evidence_of(db, sha, r->uid, evid, sizeof evid, rel, sizeof rel)) {
        put_row(db, r->uid, cam, t[0].url, sha, rel, evid, flen, ct, NULL);
        st.fetched++;
      } else {
        put_row(db, r->uid, cam, t[0].url, NULL, NULL, NULL, 0, NULL, "not_preserved");
        st.not_preserved++; st.failed++;
      }
      free(frame);
    } else {
      put_row(db, r->uid, cam, t[0].url, NULL, NULL, NULL, 0, NULL, why);
      st.failed++;
    }
    camera_feed_free(t, 1);
    cursor = r->rowid;
  }
  if (own) http_client_free(own);
  for (int i = 0; i < n; i++) { free(rows[i].uid); free(rows[i].sid); free(rows[i].props); }
  free(rows);

  /* A short page that was walked to its end closes the lap: the next tick
   * starts over, which is what reaches a camera added behind the cursor. */
  if (!stopped && n < walk) cursor = 0;
  vecindex_meta_set_long(db, vs, "refframe_cursor", (long)cursor);
  char tick[400], now[32];
  time_t tt = time(NULL);
  struct tm tm;
  gmtime_r(&tt, &tm);
  strftime(now, sizeof now, "%Y-%m-%dT%H:%M:%SZ", &tm);
  snprintf(tick, sizeof tick,
           "{\"at\":\"%s\",\"walked\":%ld,\"due\":%ld,\"fetched\":%ld,\"failed\":%ld,"
           "\"not_preserved\":%ld,\"not_indexable\":%ld,\"no_feed\":%ld,"
           "\"host_deferred\":%ld,\"budget_spent\":%s}", now, st.walked, st.due,
           st.fetched, st.failed, st.not_preserved, st.not_indexable, st.no_feed,
           st.host_deferred, stopped ? "true" : "false");
  vecindex_meta_set(db, vs, "refframe_last_tick", tick);
  fprintf(stderr, "[camera-refframe] %s\n", tick);
  return (int)st.fetched;
}

cJSON *camera_refframe_coverage(db_handle *db) {
  cJSON *o = cJSON_CreateObject();
  long with = 0, failing = 0;
  if (db && db->h && vecindex_table_exists(db, "camera_refframes")) {
    sqlite3_stmt *q;
    if (sqlite3_prepare_v2(db->h,
        "SELECT COALESCE(SUM(sha256 IS NOT NULL),0), COALESCE(SUM(last_error IS NOT NULL),0)"
        "  FROM camera_refframes", -1, &q, NULL) == SQLITE_OK) {
      if (sqlite3_step(q) == SQLITE_ROW) {
        with = (long)sqlite3_column_int64(q, 0);
        failing = (long)sqlite3_column_int64(q, 1);
      }
      sqlite3_finalize(q);
    }
  }
  cJSON_AddNumberToObject(o, "cameras_with_frame", (double)with);
  cJSON_AddNumberToObject(o, "failing", (double)failing);
  char buf[600];
  int have_meta = db && db->h && vecindex_table_exists(db, image_vec_spec.meta_table);
  if (have_meta && vecindex_meta_get(db, &image_vec_spec, "refframe_last_tick",
                                     buf, sizeof buf) && buf[0]) {
    cJSON *lt = cJSON_Parse(buf);
    cJSON_AddItemToObject(o, "last_tick", lt ? lt : cJSON_CreateNull());
  } else cJSON_AddNullToObject(o, "last_tick");
  cJSON_AddNumberToObject(o, "cursor", have_meta
    ? (double)vecindex_meta_long(db, &image_vec_spec, "refframe_cursor") : 0);
  return o;
}

/* ---- the pod ------------------------------------------------------------- */

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                         /* the frames are evidence, not intel  */
  if (!image_vec_base_url()) {
    fprintf(stderr, "[camera-refframe] disabled: JO_VISION_EMBED_URL is unset "
                    "(nothing would embed the frames)\n");
    return 0;
  }
  if (!ctx || !ctx->db) return -1;
  db_handle own;
  db_handle *db = db_worker_open(&own, ctx->db);
  int n = camera_refframe_tick(db, ctx->http, ctx->cancel);
  db_worker_close(&own);
  return n < 0 ? -1 : 0;
}

static const source_def camera_refframe_def = {
  .id = "_camera_refframe",           /* = CAM_REFFRAME_SID; a literal so
                                       * tools/lint_sources.py can count it */
  .collector = "_maint",
  .name = "Camera Reference Frames",
  .name_ja = "カメラ参照フレーム",
  .description = "Fetches ONE reference image per public-infrastructure camera "
                 "record (camera_privacy.h), at most weekly per camera and a "
                 "bounded number per tick and per host, through the camera "
                 "stills fetch path, and preserves it as evidence so the "
                 "_image_embed pod can index cameras nobody opted into stills. "
                 "Whole frames, no person features. Inert unless "
                 "JO_VISION_EMBED_URL is set.",
  .url = "internal://camera-refframe",
  .update_interval_sec = 3600, .run = run,
  .category = "maintenance" };
REGISTER_SOURCE(camera_refframe_def)
