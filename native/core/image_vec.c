/* core/image_vec.c — see image_vec.h. */
#include "image_vec.h"
#include "camera_feed.h"
#include "camera_privacy.h"
#include "camera_stills.h"
#include "httpclient.h"
#include "../source.h"
#include "../third_party/cJSON.h"
#include "../third_party/mongoose.h"   /* mg_base64_encode */
#include "../third_party/sqlite3.h"
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define IVEC_SID "_image_embed"
/* Images per POST, and raw bytes per POST before base64 (×4/3 on the wire).
 * The sidecar refuses more than VISION_EMBED_MAX_ITEMS (64) whole, so 16 is
 * well inside it; the byte bound keeps one request's JSON body to ~8 MB. */
#define IVEC_BATCH_MAX   16
#define IVEC_BATCH_BYTES (6L << 20)
/* The sidecar's own per-image cap (VISION_EMBED_MAX_BYTES). A larger frame is
 * not sent — it would come back "image_too_large" — and is recorded as such. */
#define IVEC_MAX_IMAGE_BYTES (8L << 20)
/* Orphan vectors removed per gc pass. */
#define IVEC_GC_PER_TICK 1000

static const vecindex_col IVEC_COLS[] = {
  { "ts",   VI_INT,  0 },           /* epoch s of the newest ref's capture    */
  { "kind", VI_TEXT, 0 },           /* still | media | refframe               */
};
const vecindex_spec image_vec_spec = {
  .name = "image", .table = "image_vec", .key_col = "sha256",
  .meta_table = "image_vec_meta", .done_table = "image_vec_done",
  .fail_table = "image_vec_failed", .cols = IVEC_COLS, .ncols = 2,
  .rebuild_hint = " (rebuild: DROP TABLE image_vec, image_vec_done, "
                  "image_vec_failed, image_vec_meta, image_vec_ref; the pod "
                  "re-embeds from the evidence store)" };
VECINDEX_REGISTER(image_vec_spec)

const char *image_vec_base_url(void) {
  const char *e = getenv("JO_VISION_EMBED_URL");
  return (e && *e) ? e : NULL;
}

static long env_long(const char *k, long dflt) {
  const char *e = getenv(k);
  if (!e || !*e) return dflt;
  char *end = NULL;
  long v = strtol(e, &end, 10);
  return (end && end != e && v > 0) ? v : dflt;
}

static void iso_now(char *buf, size_t n) {
  time_t t = time(NULL);
  struct tm tm;
  gmtime_r(&t, &tm);
  strftime(buf, n, "%Y-%m-%dT%H:%M:%S.000Z", &tm);
}

/* Blob root, matching evidence.c exactly. */
static const char *ev_root(void) {
  const char *e = getenv("JO_EVIDENCE_DIR");
  return (e && *e) ? e : JO_REPO_ROOT "/data/evidence";
}

static char *dupcol(sqlite3_stmt *s, int i) {
  const char *t = (const char *)sqlite3_column_text(s, i);
  return t ? strdup(t) : NULL;
}

static int exec_ok(db_handle *db, const char *sql) {
  return sqlite3_exec(db->h, sql, NULL, NULL, NULL) == SQLITE_OK;
}

void image_vec_migrate(db_handle *db) {
  if (!db || !db->h) return;
  const vecindex_spec *s = &image_vec_spec;
  vecindex_ensure_meta(db, s);
  vecindex_ensure_side_tables(db, s);
  exec_ok(db,
    "CREATE TABLE IF NOT EXISTS image_vec_ref ("
    "  kind TEXT NOT NULL, ref_id TEXT NOT NULL, sha256 TEXT NOT NULL,"
    "  camera_uid TEXT, item_uid TEXT, captured_at TEXT,"
    "  added_at TEXT NOT NULL, PRIMARY KEY(kind, ref_id));"
    "CREATE INDEX IF NOT EXISTS idx_image_vec_ref_sha ON image_vec_ref(sha256);"
    "CREATE INDEX IF NOT EXISTS idx_image_vec_ref_cam"
    "  ON image_vec_ref(camera_uid, captured_at DESC);");
}

int image_vec_ready(db_handle *db) {
  const vecindex_spec *s = &image_vec_spec;
  if (!db || !db->h || !vecindex_exists(db, s)) return 0;
  if (vecindex_refused(db, s, NULL, 0)) return 0;
  return vecindex_meta_long(db, s, "dim") > 0;
}

/* ---- refs and vectors ---------------------------------------------------- */

static int has_row(db_handle *db, const char *sql, const char *key) {
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(db->h, sql, -1, &q, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(q, 1, key, -1, SQLITE_TRANSIENT);
  int r = sqlite3_step(q) == SQLITE_ROW;
  sqlite3_finalize(q);
  return r;
}

/* Delete one vector and its done row (vec0 has no cascade). */
static int drop_vector(db_handle *db, const char *sha) {
  sqlite3_stmt *dv = NULL, *dd = NULL;
  int ok = 1;
  if (vecindex_exists(db, &image_vec_spec)) {
    ok = sqlite3_prepare_v2(db->h, "DELETE FROM image_vec WHERE sha256=?1",
                            -1, &dv, NULL) == SQLITE_OK;
    if (ok) {
      sqlite3_bind_text(dv, 1, sha, -1, SQLITE_TRANSIENT);
      ok = sqlite3_step(dv) == SQLITE_DONE;
    }
    sqlite3_finalize(dv);
  }
  if (ok && sqlite3_prepare_v2(db->h, "DELETE FROM image_vec_done WHERE sha256=?1",
                               -1, &dd, NULL) == SQLITE_OK) {
    sqlite3_bind_text(dd, 1, sha, -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(dd) == SQLITE_DONE;
  }
  sqlite3_finalize(dd);
  return ok ? 0 : -1;
}

static int sha_unreferenced(db_handle *db, const char *sha) {
  return !has_row(db, "SELECT 1 FROM image_vec_ref WHERE sha256=?1", sha);
}

int image_vec_unref_stills(db_handle *db, const char *camera_id) {
  if (!db || !db->h || !camera_id) return 0;
  if (!vecindex_table_exists(db, "image_vec_ref")) return 0;
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(db->h,
      "SELECT r.ref_id, r.sha256 FROM image_vec_ref r"
      " WHERE r.kind='" IMAGE_VEC_KIND_STILL "' AND r.camera_uid=?1"
      "   AND NOT EXISTS (SELECT 1 FROM camera_stills s WHERE s.id=r.ref_id)",
      -1, &q, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(q, 1, camera_id, -1, SQLITE_TRANSIENT);
  char **ids = NULL, **shas = NULL;
  int n = 0, cap = 0;
  while (sqlite3_step(q) == SQLITE_ROW) {
    if (n == cap) {
      int nc = cap ? cap * 2 : 64;
      char **a = realloc(ids, (size_t)nc * sizeof *a);
      if (!a) break;
      ids = a;
      char **b = realloc(shas, (size_t)nc * sizeof *b);
      if (!b) break;
      shas = b; cap = nc;
    }
    ids[n] = dupcol(q, 0); shas[n] = dupcol(q, 1);
    if (!ids[n] || !shas[n]) { free(ids[n]); free(shas[n]); break; }
    n++;
  }
  sqlite3_finalize(q);
  int removed = 0;
  sqlite3_stmt *d = NULL;
  if (n > 0 && sqlite3_prepare_v2(db->h,
      "DELETE FROM image_vec_ref WHERE kind='" IMAGE_VEC_KIND_STILL "' AND ref_id=?1",
      -1, &d, NULL) == SQLITE_OK) {
    for (int i = 0; i < n; i++) {
      sqlite3_reset(d);
      sqlite3_bind_text(d, 1, ids[i], -1, SQLITE_TRANSIENT);
      if (sqlite3_step(d) == SQLITE_DONE) removed++;
    }
    sqlite3_finalize(d);
    /* Content-addressed: another still (another camera, or a frame this one
     * captured again) may still point at the same bytes. */
    for (int i = 0; i < n; i++)
      if (sha_unreferenced(db, shas[i])) drop_vector(db, shas[i]);
  }
  for (int i = 0; i < n; i++) { free(ids[i]); free(shas[i]); }
  free(ids); free(shas);
  return removed;
}

int image_vec_gc(db_handle *db, int limit) {
  if (!db || !db->h || !vecindex_table_exists(db, "image_vec_ref")) return 0;
  if (limit <= 0) limit = IVEC_GC_PER_TICK;
  if (vecindex_begin(db) != 0) return 0;
  int ok = 1;
  /* Referents deleted by any route other than the stills prune (a media
   * asset re-analysed away, a stills table edited by hand). */
  if (vecindex_table_exists(db, "camera_stills"))
    ok = ok && exec_ok(db,
      "DELETE FROM image_vec_ref WHERE kind='" IMAGE_VEC_KIND_STILL "'"
      "  AND NOT EXISTS (SELECT 1 FROM camera_stills s WHERE s.id=image_vec_ref.ref_id)");
  if (vecindex_table_exists(db, "media_assets"))
    ok = ok && exec_ok(db,
      "DELETE FROM image_vec_ref WHERE kind='" IMAGE_VEC_KIND_MEDIA "'"
      "  AND NOT EXISTS (SELECT 1 FROM media_assets m WHERE m.id=image_vec_ref.ref_id"
      "                  AND m.sha256=image_vec_ref.sha256)");
  char **shas = calloc((size_t)limit, sizeof *shas);
  int n = 0;
  sqlite3_stmt *q;
  if (ok && shas && sqlite3_prepare_v2(db->h,
      "SELECT d.sha256 FROM image_vec_done d WHERE NOT EXISTS"
      " (SELECT 1 FROM image_vec_ref r WHERE r.sha256=d.sha256) LIMIT ?1",
      -1, &q, NULL) == SQLITE_OK) {
    sqlite3_bind_int(q, 1, limit);
    while (n < limit && sqlite3_step(q) == SQLITE_ROW)
      if ((shas[n] = dupcol(q, 0)) != NULL) n++;
    sqlite3_finalize(q);
  }
  for (int i = 0; ok && i < n; i++) ok = drop_vector(db, shas[i]) == 0;
  vecindex_end(db, ok);
  for (int i = 0; i < n; i++) free(shas[i]);
  free(shas);
  if (ok && n)
    fprintf(stderr, "[image-vec] gc: removed %d vector(s) no frame points at\n", n);
  return ok ? n : 0;
}

/* The metadata a sha's vector should carry, from its refs: the newest
 * capture, and "still" whenever any still points at it. */
static void ref_vals(db_handle *db, const char *sha, long long *ts,
                     const char **kind) {
  *ts = 0;
  *kind = IMAGE_VEC_KIND_STILL;
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(db->h,
      "SELECT MAX(CAST(strftime('%s', captured_at) AS INTEGER)),"
      "       MAX(kind='" IMAGE_VEC_KIND_STILL "'), MAX(kind='" IMAGE_VEC_KIND_MEDIA "')"
      "  FROM image_vec_ref WHERE sha256=?1", -1, &q, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(q, 1, sha, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(q) == SQLITE_ROW) {
    *ts = sqlite3_column_int64(q, 0);
    *kind = sqlite3_column_int(q, 1) ? IMAGE_VEC_KIND_STILL
          : sqlite3_column_int(q, 2) ? IMAGE_VEC_KIND_MEDIA
          : IMAGE_VEC_KIND_REFFRAME;
  }
  sqlite3_finalize(q);
}

/* ---- the sidecar --------------------------------------------------------- */

static void join_url(char *out, size_t cap, const char *base, const char *path) {
  size_t bl = strlen(base);
  snprintf(out, cap, "%.*s%s", (int)(bl && base[bl - 1] == '/' ? bl - 1 : bl),
           base, path);
}

/* Basename of a model id, the same rule vecindex_served_model() applies, so
 * the envelope's "google/siglip2-…" and /v1/models agree. */
static void model_base(const char *id, char *out, size_t cap) {
  const char *b = id ? strrchr(id, '/') : NULL;
  snprintf(out, cap, "%s", b ? b + 1 : (id ? id : ""));
}

/* POST `body` to <base><path>; the parsed envelope, or NULL with `err`. */
static cJSON *sidecar_post(http_client *http, const char *path, const char *body,
                           size_t blen, int timeout_ms, long *status,
                           char *err, size_t ecap) {
  const char *base = image_vec_base_url();
  *status = 0;
  if (!base) { snprintf(err, ecap, "JO_VISION_EMBED_URL is not set"); return NULL; }
  char url[1024];
  join_url(url, sizeof url, base, path);
  http_client *own = NULL;
  if (!http) http = own = http_client_new();
  if (!http) { snprintf(err, ecap, "no http client"); return NULL; }
  static const char *const H[] = { "Content-Type: application/json", NULL };
  http_response r = {0};
  cJSON *j = NULL;
  if (http_request(http, "POST", url, H, body, blen, timeout_ms, 0, &r) != 0 ||
      r.status == 0)
    snprintf(err, ecap, "POST %s: no answer from the vision sidecar", path);
  else if (r.status < 200 || r.status >= 300 || !r.body)
    snprintf(err, ecap, "POST %s: HTTP %ld %.160s", path, (long)r.status,
             r.body ? r.body : "");
  else if (!(j = cJSON_Parse(r.body)))
    snprintf(err, ecap, "POST %s: the answer is not JSON", path);
  *status = r.status;
  http_response_free(&r);
  if (own) http_client_free(own);
  return j;
}

/* data[i].embedding → floats; NULL when it is not an array of numbers. */
static float *vec_of(cJSON *emb, int *dim) {
  int n = cJSON_IsArray(emb) ? cJSON_GetArraySize(emb) : 0;
  if (n <= 0) return NULL;
  float *v = malloc((size_t)n * sizeof *v);
  if (!v) return NULL;
  int i = 0;
  cJSON *x;
  cJSON_ArrayForEach(x, emb) {
    if (!cJSON_IsNumber(x)) { free(v); return NULL; }
    v[i++] = (float)x->valuedouble;
  }
  *dim = n;
  return v;
}

int image_vec_embed_text(http_client *http, const char *text, float **out,
                         int *dim, char *model, size_t mcap, char *err,
                         size_t ecap) {
  *out = NULL; *dim = 0;
  if (model && mcap) model[0] = 0;
  if (!text || !*text) { snprintf(err, ecap, "empty query"); return -1; }
  char *clean = vecindex_bound_text(text, 2048);
  cJSON *req = cJSON_CreateObject();
  cJSON *in = cJSON_AddArrayToObject(req, "input");
  cJSON_AddItemToArray(in, cJSON_CreateString(clean ? clean : ""));
  free(clean);
  char *body = cJSON_PrintUnformatted(req);
  cJSON_Delete(req);
  if (!body) { snprintf(err, ecap, "out of memory"); return -1; }
  long st = 0;
  cJSON *j = sidecar_post(http, "/embed/text", body, strlen(body),
                          (int)env_long("JO_VISION_QUERY_TIMEOUT_MS", 15000),
                          &st, err, ecap);
  free(body);
  if (!j) return -1;
  cJSON *d0 = cJSON_GetArrayItem(cJSON_GetObjectItem(j, "data"), 0); /* exhaustive-ok: one input sent, one vector back */
  float *v = d0 ? vec_of(cJSON_GetObjectItem(d0, "embedding"), dim) : NULL;
  cJSON *m = cJSON_GetObjectItem(j, "model");
  if (model && mcap) model_base(cJSON_IsString(m) ? m->valuestring : "", model, mcap);
  cJSON_Delete(j);
  if (!v) { snprintf(err, ecap, "POST /embed/text: no vector in the answer"); return -1; }
  *out = v;
  return 0;
}

/* ---- the walk ------------------------------------------------------------ */

typedef struct {
  const char *kind;                 /* IMAGE_VEC_KIND_*                       */
  int media;                        /* kind is media (needs proof of a camera) */
  char *ref_id, *sha, *cam, *item, *cap, *rel;
  int state;                        /* see C_* below                          */
} ivc;

enum { C_NEW = 0, C_SKIP, C_REF, C_EMBED, C_DONE };

typedef struct { char *uid; int ok; } icache;

typedef struct {
  long walked, not_indexable, no_bytes, failed_before, reused, embedded,
       failed, refs;
} ivstats;

static void ivc_free(ivc *c) {
  free(c->ref_id); free(c->sha); free(c->cam); free(c->item); free(c->cap);
  free(c->rel);
}

/* May this item's frames be indexed? camera_privacy's allowlist, plus — for a
 * media asset — proof that the item is a camera at all (it has a feed). */
static int item_indexable(db_handle *db, const char *uid, int need_feed,
                          icache **cache, int *nc) {
  for (int i = 0; i < *nc; i++)
    if (!strcmp((*cache)[i].uid, uid)) return (*cache)[i].ok;
  int ok = 0;
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(db->h, "SELECT properties, source_id FROM intel_items"
                         " WHERE uid=?1", -1, &q, NULL) == SQLITE_OK) {
    sqlite3_bind_text(q, 1, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(q) == SQLITE_ROW) {
      const char *p = (const char *)sqlite3_column_text(q, 0);
      const char *sid = (const char *)sqlite3_column_text(q, 1);
      ok = camera_record_indexable(p, sid) &&
           (!need_feed || camera_feed_resolve(p, sid, NULL, 0) > 0);
    }
    sqlite3_finalize(q);
  }
  icache *nc2 = realloc(*cache, (size_t)(*nc + 1) * sizeof **cache);
  if (nc2) {
    *cache = nc2;
    if (((*cache)[*nc].uid = strdup(uid)) != NULL) { (*cache)[*nc].ok = ok; (*nc)++; }
  }
  return ok;
}

/* Rows of one kind after the walk cursor that have no ref yet. Returns how
 * many rows the page held (the cursor's business), appending candidates. */
static int walk_kind(db_handle *db, const char *kind, long long after, int limit,
                     ivc **v, int *n, int *cap, long long *last) {
  const char *sql = !strcmp(kind, IMAGE_VEC_KIND_STILL)
    ? "SELECT s.rowid, s.id, s.sha256, s.camera_id, s.captured_at, s.blob_path"
      "  FROM camera_stills s WHERE s.rowid > ?1 AND NOT EXISTS"
      "  (SELECT 1 FROM image_vec_ref r WHERE r.kind='" IMAGE_VEC_KIND_STILL "'"
      "   AND r.ref_id=s.id) ORDER BY s.rowid LIMIT ?2"
    : "SELECT m.rowid, m.id, m.sha256, NULL,"
      "       COALESCE(datetime(m.analyzed_at), datetime('now')), NULL"
      "  FROM media_assets m WHERE m.rowid > ?1 AND m.sha256 IS NOT NULL"
      "   AND m.content_type LIKE 'image/%' AND NOT EXISTS"
      "  (SELECT 1 FROM image_vec_ref r WHERE r.kind='" IMAGE_VEC_KIND_MEDIA "'"
      "   AND r.ref_id=m.id) ORDER BY m.rowid LIMIT ?2";
  sqlite3_stmt *q;
  *last = after;
  if (sqlite3_prepare_v2(db->h, sql, -1, &q, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_int64(q, 1, after);
  sqlite3_bind_int(q, 2, limit);
  int rows = 0;
  while (sqlite3_step(q) == SQLITE_ROW) {
    rows++;
    *last = sqlite3_column_int64(q, 0);
    if (*n == *cap) {
      int nc = *cap ? *cap * 2 : 128;
      ivc *a = realloc(*v, (size_t)nc * sizeof *a);
      if (!a) break;
      *v = a; *cap = nc;
    }
    ivc *c = &(*v)[*n];
    memset(c, 0, sizeof *c);
    c->media = strcmp(kind, IMAGE_VEC_KIND_STILL) != 0;
    c->kind = c->media ? IMAGE_VEC_KIND_MEDIA : IMAGE_VEC_KIND_STILL;
    c->ref_id = dupcol(q, 1);
    c->sha = dupcol(q, 2);
    c->cap = dupcol(q, 4);
    c->rel = dupcol(q, 5);
    if (!c->media) {
      c->cam = dupcol(q, 3);
      const char *sfx = NULL;
      c->item = c->cam ? camera_feed_split(c->cam, &sfx) : NULL;
    }
    if (!c->ref_id || !c->sha || strlen(c->sha) != 64) { ivc_free(c); continue; }
    (*n)++;
  }
  sqlite3_finalize(q);
  return rows;
}

/* The media asset's item uid (kept out of the walk query so the same SELECT
 * shape serves both kinds). */
static void media_item(db_handle *db, ivc *c) {
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(db->h, "SELECT item_uid FROM media_assets WHERE id=?1",
                         -1, &q, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(q, 1, c->ref_id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(q) == SQLITE_ROW) c->item = dupcol(q, 0);
  sqlite3_finalize(q);
}

/* Read a frame's bytes from the evidence store and prove they are the bytes
 * the row attests to. NULL when there are none (capture_evidence off, the
 * blob reaped, a mismatch) — never a guess. */
static unsigned char *read_blob(db_handle *db, const ivc *c, size_t *len,
                                int *too_large) {
  *len = 0; *too_large = 0;
  char rel[256] = "";
  if (c->rel && *c->rel) snprintf(rel, sizeof rel, "%s", c->rel);
  else {
    sqlite3_stmt *q;
    if (sqlite3_prepare_v2(db->h, "SELECT blob_path FROM evidence WHERE"
                           " content_sha256=?1 LIMIT 1", -1, &q, NULL) == SQLITE_OK) {
      sqlite3_bind_text(q, 1, c->sha, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(q) == SQLITE_ROW && sqlite3_column_text(q, 0))
        snprintf(rel, sizeof rel, "%s", (const char *)sqlite3_column_text(q, 0));
      sqlite3_finalize(q);
    }
  }
  if (!rel[0] || strstr(rel, "..")) return NULL;
  char path[2048];
  snprintf(path, sizeof path, "%s/%s", ev_root(), rel);
  struct stat st;
  if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) return NULL;
  if (st.st_size > IVEC_MAX_IMAGE_BYTES) { *too_large = 1; return NULL; }
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  unsigned char *b = malloc((size_t)st.st_size);
  size_t got = b ? fread(b, 1, (size_t)st.st_size, f) : 0;
  fclose(f);
  if (!b || got != (size_t)st.st_size) { free(b); return NULL; }
  unsigned char d[SHA256_DIGEST_LENGTH];
  SHA256(b, got, d);
  char hex[65];
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) snprintf(hex + i * 2, 3, "%02x", d[i]);
  if (strcmp(hex, c->sha) != 0) { free(b); return NULL; }
  *len = got;
  return b;
}

static int put_ref(db_handle *db, const ivc *c, const char *now) {
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(db->h,
      "INSERT OR REPLACE INTO image_vec_ref(kind,ref_id,sha256,camera_uid,"
      " item_uid,captured_at,added_at) VALUES(?1,?2,?3,?4,?5,?6,?7)",
      -1, &q, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(q, 1, c->kind, -1, SQLITE_STATIC);
  sqlite3_bind_text(q, 2, c->ref_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(q, 3, c->sha, -1, SQLITE_TRANSIENT);
  if (c->cam)  sqlite3_bind_text(q, 4, c->cam, -1, SQLITE_TRANSIENT);
  else         sqlite3_bind_null(q, 4);
  if (c->item) sqlite3_bind_text(q, 5, c->item, -1, SQLITE_TRANSIENT);
  else         sqlite3_bind_null(q, 5);
  if (c->cap)  sqlite3_bind_text(q, 6, c->cap, -1, SQLITE_TRANSIENT);
  else         sqlite3_bind_null(q, 6);
  sqlite3_bind_text(q, 7, now, -1, SQLITE_TRANSIENT);
  int ok = sqlite3_step(q) == SQLITE_DONE;
  sqlite3_finalize(q);
  return ok ? 0 : -1;
}

/* Write (or rewrite) one sha's vector with the metadata its refs imply. A
 * NULL `vec` means "already stored": reload it, and rewrite only when the
 * metadata moved (vec0 has no in-place update a caller can rely on). */
static int put_vector(db_handle *db, vecindex_writer *w, const char *sha,
                      const float *vec, int dim, const char *now) {
  const vecindex_spec *s = &image_vec_spec;
  long long ts; const char *kind;
  ref_vals(db, sha, &ts, &kind);
  vecindex_val vals[2] = { VI_I(ts), VI_S(kind) };
  unsigned long long mh = vecindex_vals_hash(s, vals);
  float *own = NULL;
  if (!vec) {
    sqlite3_stmt *q;
    long long cur = 0; int have = 0;
    if (sqlite3_prepare_v2(db->h, "SELECT meta_hash FROM image_vec_done"
                           " WHERE sha256=?1", -1, &q, NULL) == SQLITE_OK) {
      sqlite3_bind_text(q, 1, sha, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(q) == SQLITE_ROW) { have = 1; cur = sqlite3_column_int64(q, 0); }
      sqlite3_finalize(q);
    }
    if (have && (unsigned long long)cur == mh) return 0;
    if (vecindex_load(db, s, sha, &own, &dim) != 0) return -1;
    vec = own;
  }
  int rc = vecindex_writer_put(w, sha, vec, dim, vals, vecindex_hash(sha), mh, now);
  free(own);
  return rc;
}

static void record_failure(db_handle *db, const char *sha, const char *reason,
                           const char *now) {
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(db->h,
      "INSERT OR REPLACE INTO image_vec_failed(sha256,text_hash,failed_at,reason)"
      " VALUES(?1,?2,?3,?4)", -1, &q, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(q, 1, sha, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(q, 2, (sqlite3_int64)vecindex_hash(sha));
  sqlite3_bind_text(q, 3, now, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(q, 4, reason, -1, SQLITE_TRANSIENT);
  sqlite3_step(q);
  sqlite3_finalize(q);
}

/* Refs for every candidate sharing `sha`, in the caller's transaction. */
static int refs_for(db_handle *db, ivc *v, int n, const char *sha,
                    const char *now, ivstats *st) {
  for (int i = 0; i < n; i++) {
    if ((v[i].state != C_REF && v[i].state != C_EMBED) || strcmp(v[i].sha, sha))
      continue;
    if (put_ref(db, &v[i], now) != 0) return -1;
    v[i].state = C_DONE;
    st->refs++;
  }
  return 0;
}

/* Embed pending[0..m) (their bytes in imgs/lens), write vectors + refs in one
 * transaction. 0 ok; -1 sidecar/db failure (recorded); -2 refused. */
static int embed_batch(db_handle *db, http_client *http, const char *model,
                       ivc *v, int n, ivc **pend, unsigned char **imgs,
                       size_t *lens, int m, const char *now, ivstats *st) {
  const vecindex_spec *s = &image_vec_spec;
  /* {"images":["<b64>",…]} assembled by hand: cJSON would copy each multi-MB
   * string twice more. */
  size_t cap = 32;
  for (int i = 0; i < m; i++) cap += 4 * ((lens[i] + 2) / 3) + 8;
  char *body = malloc(cap);
  if (!body) return -1;
  size_t len = 0;
  /* cap was sized for every part above, so these copies cannot overrun */
  memcpy(body, "{\"images\":[", 11);
  len = 11;
  for (int i = 0; i < m; i++) {
    if (i) body[len++] = ',';
    body[len++] = '"';
    len += mg_base64_encode(imgs[i], lens[i], body + len, cap - len);
    body[len++] = '"';
  }
  memcpy(body + len, "]}", 3);                 /* with its NUL             */
  len += 2;
  long status = 0;
  char err[400];
  cJSON *j = sidecar_post(http, "/embed/image", body, len,
                          (int)env_long("JO_VISION_EMBED_TIMEOUT_MS", 120000),
                          &status, err, sizeof err);
  free(body);
  if (!j) {
    char msg[480];
    snprintf(msg, sizeof msg, "%s: %s", now, err);
    vecindex_meta_set(db, s, "last_error", msg);
    fprintf(stderr, "[image-vec] %s\n", msg);
    return -1;
  }
  char emodel[256];
  cJSON *jm = cJSON_GetObjectItem(j, "model");
  model_base(cJSON_IsString(jm) ? jm->valuestring : model, emodel, sizeof emodel);
  float **vecs = calloc((size_t)m, sizeof *vecs);
  const char **why = calloc((size_t)m, sizeof *why);
  int dim = 0, bad = !vecs || !why;
  cJSON *x;
  cJSON_ArrayForEach(x, cJSON_GetObjectItem(j, "data")) {
    cJSON *ix = cJSON_GetObjectItem(x, "index");
    int i = cJSON_IsNumber(ix) ? ix->valueint : -1, d = 0;
    if (bad || i < 0 || i >= m || vecs[i]) { bad = 1; continue; }
    vecs[i] = vec_of(cJSON_GetObjectItem(x, "embedding"), &d);
    if (!vecs[i] || (dim && d != dim)) { bad = 1; continue; }
    dim = d;
  }
  cJSON_ArrayForEach(x, cJSON_GetObjectItem(j, "errors")) {
    cJSON *ix = cJSON_GetObjectItem(x, "index"), *e = cJSON_GetObjectItem(x, "error");
    int i = cJSON_IsNumber(ix) ? ix->valueint : -1;
    if (!bad && i >= 0 && i < m && !vecs[i])
      why[i] = cJSON_IsString(e) ? e->valuestring : "embed_error";
  }
  for (int i = 0; !bad && i < m; i++) if (!vecs[i] && !why[i]) bad = 1;
  int rc = 0;
  char idwhy[600];
  if (bad) {
    char msg[200];
    snprintf(msg, sizeof msg, "%s: /embed/image answered with a malformed envelope", now);
    vecindex_meta_set(db, s, "last_error", msg);
    rc = -1;
  } else if (dim > 0 && vecindex_check_identity(db, s, emodel, dim, idwhy, sizeof idwhy)
             > VI_ID_EMPTY) {
    vecindex_refuse(db, s, idwhy);
    rc = -2;
  } else if (vecindex_begin(db) != 0) {
    vecindex_sql_error(db, s, now, "BEGIN");
    rc = -1;
  } else {
    int ok = 1;
    if (dim > 0 && !vecindex_exists(db, s))
      ok = vecindex_create(db, s, dim) == 0 &&
           vecindex_record_identity(db, s, emodel, dim) == 0;
    vecindex_writer w;
    int wopen = ok && dim > 0 && vecindex_writer_open(&w, db, s) == 0;
    if (dim > 0 && !wopen) ok = 0;
    for (int i = 0; ok && i < m; i++) {
      if (vecs[i]) {
        ok = refs_for(db, v, n, pend[i]->sha, now, st) == 0 &&
             put_vector(db, &w, pend[i]->sha, vecs[i], dim, now) == 0;
        if (ok) st->embedded++;
      } else {
        /* Content-addressed: the same bytes will fail the same way, so the
         * failure is permanent for this sha and the walk skips it. */
        record_failure(db, pend[i]->sha, why[i], now);
        for (int k = 0; k < n; k++)
          if (v[k].state == C_EMBED && !strcmp(v[k].sha, pend[i]->sha))
            v[k].state = C_SKIP;
        st->failed++;
      }
    }
    if (wopen) vecindex_writer_close(&w);
    if (!ok) vecindex_sql_error(db, s, now, "writing image vectors");
    if (vecindex_end(db, ok) != 0) rc = -1;
  }
  for (int i = 0; vecs && i < m; i++) free(vecs[i]);
  free(vecs); free(why);
  cJSON_Delete(j);
  return rc;
}

typedef struct {
  ivc *pend[IVEC_BATCH_MAX];
  unsigned char *imgs[IVEC_BATCH_MAX];
  size_t lens[IVEC_BATCH_MAX];
  int m;
  long bytes;
} ivbatch;

static int flush_batch(db_handle *db, http_client *http, const char *model,
                       ivc *v, int n, ivbatch *b, const char *now, ivstats *st) {
  int rc = embed_batch(db, http, model, v, n, b->pend, b->imgs, b->lens, b->m,
                       now, st);
  for (int k = 0; k < b->m; k++) free(b->imgs[k]);
  b->m = 0; b->bytes = 0;
  return rc;
}

int image_vec_tick(db_handle *db, http_client *http, int max_walk) {
  const vecindex_spec *s = &image_vec_spec;
  if (!image_vec_base_url()) return 0;
  if (max_walk <= 0) max_walk = (int)env_long("JO_IMAGE_EMBED_MAX_WALK", 512);
  camera_stills_migrate(db);
  image_vec_migrate(db);
  char now[40];
  iso_now(now, sizeof now);
  char why[600];
  if (vecindex_refused(db, s, why, sizeof why)) {
    fprintf(stderr, "[image-vec] refused: %s\n", why);
    return -1;
  }
  char model[256];
  if (vecindex_served_model(http, image_vec_base_url(), model, sizeof model) != 0) {
    char msg[400];
    snprintf(msg, sizeof msg, "%s: cannot identify the vision model (%.200s); "
             "tick skipped", now, model);
    vecindex_meta_set(db, s, "last_error", msg);
    fprintf(stderr, "[image-vec] %s\n", msg);
    return -1;
  }
  if (vecindex_check_identity(db, s, model, 0, why, sizeof why) > VI_ID_EMPTY) {
    vecindex_refuse(db, s, why);
    return -1;
  }

  /* 1. the walk: stills, then media, each from its own cursor */
  ivc *v = NULL;
  int n = 0, cap = 0;
  long long s_after = vecindex_meta_long(db, s, "still_after");
  long long m_after = vecindex_meta_long(db, s, "media_after");
  long long s_last = s_after, m_last = m_after;
  int s_rows = walk_kind(db, IMAGE_VEC_KIND_STILL, s_after, max_walk, &v, &n, &cap, &s_last);
  int n_still = n;
  int m_rows = walk_kind(db, IMAGE_VEC_KIND_MEDIA, m_after, max_walk, &v, &n, &cap, &m_last);
  for (int i = n_still; i < n; i++) media_item(db, &v[i]);

  ivstats st = {0};
  st.walked = n;
  icache *cache = NULL;
  int ncache = 0;
  for (int i = 0; i < n; i++) {
    ivc *c = &v[i];
    if (!c->item || !item_indexable(db, c->item, c->media,
                                    &cache, &ncache)) {
      c->state = C_SKIP; st.not_indexable++; continue;
    }
    if (has_row(db, "SELECT 1 FROM image_vec_failed WHERE sha256=?1", c->sha)) {
      c->state = C_SKIP; st.failed_before++; continue;
    }
    c->state = has_row(db, "SELECT 1 FROM image_vec_done WHERE sha256=?1", c->sha)
             ? C_REF : C_EMBED;
  }
  for (int i = 0; i < ncache; i++) free(cache[i].uid);
  free(cache);

  int rc = 0;
  /* 2. frames whose bytes are already embedded: a ref, no request */
  int any_ref = 0;
  for (int i = 0; i < n; i++) if (v[i].state == C_REF) any_ref = 1;
  if (any_ref && vecindex_exists(db, s) && vecindex_begin(db) == 0) {
    vecindex_writer w;
    int ok = vecindex_writer_open(&w, db, s) == 0;
    for (int i = 0; ok && i < n; i++) {
      if (v[i].state != C_REF) continue;
      char *sha = strdup(v[i].sha);
      ok = sha && refs_for(db, v, n, sha, now, &st) == 0 &&
           put_vector(db, &w, sha, NULL, 0, now) == 0;
      if (ok) st.reused++;
      free(sha);
    }
    if (ok) vecindex_writer_close(&w);
    if (!ok) vecindex_sql_error(db, s, now, "adding refs");
    if (vecindex_end(db, ok) != 0) rc = -1;
  }

  /* 3. new bytes: one request per batch, identical frames sent once */
  ivbatch b = { .m = 0 };
  for (int i = 0; rc == 0 && i < n; i++) {
    ivc *c = &v[i];
    if (c->state != C_EMBED) continue;
    int dup = 0;
    for (int k = 0; k < b.m; k++) if (!strcmp(b.pend[k]->sha, c->sha)) dup = 1;
    if (dup) continue;
    size_t bl = 0; int big = 0;
    unsigned char *img = read_blob(db, c, &bl, &big);
    if (!img) {
      /* No bytes behind THIS row (its blob_path is its own); another row
       * with the same sha may still have them, so only a size refusal — a
       * property of the bytes — settles every row of the sha. */
      c->state = C_SKIP;
      if (!big) { st.no_bytes++; continue; }
      record_failure(db, c->sha, "image_too_large", now);
      st.failed++;
      for (int k = i; k < n; k++)
        if (v[k].state == C_EMBED && !strcmp(v[k].sha, c->sha)) v[k].state = C_SKIP;
      continue;
    }
    if (b.m > 0 && b.bytes + (long)bl > IVEC_BATCH_BYTES)
      rc = flush_batch(db, http, model, v, n, &b, now, &st);
    if (rc != 0) { free(img); break; }
    b.pend[b.m] = c; b.imgs[b.m] = img; b.lens[b.m] = bl; b.m++;
    b.bytes += (long)bl;
    if (b.m == IVEC_BATCH_MAX) rc = flush_batch(db, http, model, v, n, &b, now, &st);
  }
  if (rc == 0 && b.m > 0) rc = flush_batch(db, http, model, v, n, &b, now, &st);
  for (int k = 0; k < b.m; k++) free(b.imgs[k]);

  /* 4. cursors move only past a page that was fully settled; a short page is
   * the end of the lap, so the next tick starts over (which is what picks up
   * a frame whose bytes were preserved late, or a camera newly allowed). */
  if (rc == 0) {
    vecindex_meta_set_long(db, s, "still_after", s_rows < max_walk ? 0 : (long)s_last);
    vecindex_meta_set_long(db, s, "media_after", m_rows < max_walk ? 0 : (long)m_last);
    image_vec_gc(db, IVEC_GC_PER_TICK);
  }
  for (int i = 0; i < n; i++) ivc_free(&v[i]);
  free(v);

  vecindex_meta_set_long(db, s, "embedded_count",
                         vecindex_count(db, s->done_table));
  vecindex_meta_set(db, s, "last_run_at", now);
  vecindex_meta_set(db, s, "state", rc == 0 ? "ok" : rc == -2 ? "refused" : "error");
  char tick[400];
  snprintf(tick, sizeof tick,
           "{\"walked\":%ld,\"embedded\":%ld,\"reused\":%ld,\"refs\":%ld,"
           "\"not_indexable\":%ld,\"no_bytes\":%ld,\"failed\":%ld,"
           "\"failed_before\":%ld}", st.walked, st.embedded, st.reused, st.refs,
           st.not_indexable, st.no_bytes, st.failed, st.failed_before);
  vecindex_meta_set(db, s, "last_tick", tick);
  fprintf(stderr, "[image-vec] %s\n", tick);
  return rc == 0 ? (int)st.embedded : -1;
}

/* ---- the pod ------------------------------------------------------------- */

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                         /* the index is the output, not intel */
  if (!image_vec_base_url()) {
    fprintf(stderr, "[image-vec] disabled: JO_VISION_EMBED_URL is unset; camera "
                    "visual search answers 503\n");
    return 0;
  }
  if (!ctx || !ctx->db) return -1;
  db_handle own;
  db_handle *db = db_worker_open(&own, ctx->db);
  int n = image_vec_tick(db, ctx->http, 0);
  db_worker_close(&own);
  return n < 0 ? -1 : 0;
}

static const source_def image_vec_pod_def = {
  .id = IVEC_SID, .collector = "_maint",
  .name = "Camera Image Index",
  .name_ja = "カメラ画像ベクトル索引",
  .description = "Embeds preserved camera stills and camera media frames with "
                 "the vision sidecar so cameras can be searched by a sentence. "
                 "Public-infrastructure cameras only (camera_privacy.h); whole "
                 "frames, no person features. Inert unless JO_VISION_EMBED_URL "
                 "is set.",
  .url = "internal://image-index",
  .update_interval_sec = 600, .run = run,
  .category = "maintenance" };
REGISTER_SOURCE(image_vec_pod_def)
