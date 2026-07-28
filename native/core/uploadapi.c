/* core/uploadapi.c — see uploadapi.h for the design, the DDL and the wiring.
 *
 * The short version of why this file looks the way it does: mongoose buffers a
 * whole request body before MG_EV_HTTP_MSG and caps it at MG_MAX_RECV_SIZE
 * (3 MiB, per connection), so the file is split by the CLIENT and reassembled
 * here from scratch files. Nothing in this module is a blob store — the
 * assembled bytes are handed straight to evidence_capture(), which already
 * owns content addressing, dedup, the byte budget and the custody chain.
 *
 * Two invariants hold everywhere below and are worth stating once:
 *   · a request body is BINARY. (body, body_len) travels together; strlen(),
 *     strdup(), cJSON_Parse() and bind_text(...,-1,...) are never applied to it.
 *   · a filesystem path is built ONLY from a validated uuid and a range-checked
 *     integer seq. `filename` is data, never a path component. */
#include "uploadapi.h"
#include "audit.h"
#include "evidence.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef JO_REPO_ROOT
#define JO_REPO_ROOT "/Users/rayan/JapanOSINT"
#endif

#define UP_DEF_MAX_BYTES   67108864LL   /* 64 MiB per file                    */
#define UP_DEF_PART_MAX    1048576LL    /* 1 MiB per part (3 MiB recv cap)     */
#define UP_DEF_TTL_SEC     86400LL      /* 24 h idle before an open upload dies */
#define UP_DEF_ROW_TTL_SEC 604800LL     /* 7 d before a terminal row is dropped */
#define UP_GC_BATCH        500          /* uploads reaped per pass, per stage   */
#define UP_FILENAME_CAP    256          /* 255 bytes + NUL                      */
#define UP_CT_CAP          160
#define UP_AUDIT_NAME_MAX  120          /* filename bytes allowed in audit JSON */
#define UP_STATUS_SEQ_CAP  2048         /* seqs echoed by GET /:id              */

/* The source_id every custody row from this module carries. It must exist in
 * `sources` with capture_evidence=1 or evidence.c fails closed and the bytes
 * are dropped; uploadapi_migrate() is what guarantees that. */
#define UP_SOURCE_ID "client-upload"

/* ── environment ─────────────────────────────────────────────────────────────
 * Read once and cached, same shape as evidence.c. Every value is CLAMPED, not
 * merely defaulted: a typo in JO_UPLOAD_MAX_BYTES must not become a 4 GB
 * malloc() at commit time. */

static const char *up_root(void) {
  const char *e = getenv("JO_UPLOAD_DIR");
  return (e && *e) ? e : JO_REPO_ROOT "/data/uploads";
}
static long long env_ll(const char *name, long long dflt, long long lo,
                        long long hi) {
  const char *e = getenv(name);
  if (!e || !*e) return dflt;
  long long v = strtoll(e, NULL, 10);
  return (v >= lo && v <= hi) ? v : dflt;
}
static long long up_max_bytes(void) {
  static long long v = -1;
  if (v < 0) v = env_ll("JO_UPLOAD_MAX_BYTES", UP_DEF_MAX_BYTES,
                        65536LL, 268435456LL);
  return v;
}
static long long up_part_max(void) {
  static long long v = -1;
  /* Upper clamp 2 MiB, not 3: the request line, the headers and mongoose's own
   * IO buffer growth share MG_MAX_RECV_SIZE with the body. */
  if (v < 0) v = env_ll("JO_UPLOAD_PART_MAX_BYTES", UP_DEF_PART_MAX,
                        65536LL, 2097152LL);
  return v;
}
static long long up_ttl_sec(void) {
  static long long v = -1;
  if (v < 0) v = env_ll("JO_UPLOAD_TTL_SEC", UP_DEF_TTL_SEC, 60LL, 2592000LL);
  return v;
}
static long long up_row_ttl_sec(void) {
  static long long v = -1;
  if (v < 0) v = env_ll("JO_UPLOAD_ROW_TTL_SEC", UP_DEF_ROW_TTL_SEC,
                        3600LL, 31536000LL);
  return v;
}

/* ── small shared helpers (same shapes as alertsapi.c / evidence.c) ────────── */

static void uuid4(char out[37]) {
  unsigned char b[16]; RAND_bytes(b, 16);
  b[6] = (b[6] & 0x0F) | 0x40; b[8] = (b[8] & 0x3F) | 0x80;
  snprintf(out, 37,
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
    b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
}
static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static char *err(int *st, int code, const char *msg) {
  if (st) *st = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", msg);
  char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}
/* Same envelope with machine-readable context attached (which seq is missing,
 * what the ceiling actually is). Takes ownership of `meta`. A client that has
 * to parse a prose message to resume an upload will not resume it. */
static char *err_meta(int *st, int code, const char *msg, cJSON *meta) {
  if (st) *st = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", msg);
  if (meta) cJSON_AddItemToObject(o, "meta", meta);
  char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}
static char *wrap(cJSON *data, int *st, int code) {
  if (st) *st = code;
  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "data", data);
  char *j = cJSON_PrintUnformatted(root); cJSON_Delete(root); return j;
}
/* owner > admin > analyst > viewer; an unrecognised role ranks 0 and can do
 * nothing, so a future role name fails closed instead of inheriting write. */
static int role_rank(const char *r) {
  if (!r) return 0;
  if (!strcmp(r, "owner"))   return 4;
  if (!strcmp(r, "admin"))   return 3;
  if (!strcmp(r, "analyst")) return 2;
  if (!strcmp(r, "viewer"))  return 1;
  return 0;
}
static void sha256_hex_bytes(const void *b, size_t n, char out[65]) {
  unsigned char d[SHA256_DIGEST_LENGTH];
  SHA256((const unsigned char *)b, n, d);
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    snprintf(out + i * 2, 3, "%02x", d[i]);
}

/* Percent/plus-decoding lookup over the raw query string. Local copy of
 * timelineapi.c's qs_get shape — this module reads exactly one parameter and
 * pulling in that file's whole surface for it would be worse. */
static int hexv(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
static int qs_get(const char *qs, const char *key, char *out, size_t n) {
  if (!out || n == 0) return 0;
  out[0] = 0;
  if (!qs || !*qs) return 0;
  size_t kl = strlen(key);
  const char *p = qs;
  for (;;) {
    const char *amp = strchr(p, '&');
    const char *end = amp ? amp : p + strlen(p);
    const char *eqp = memchr(p, '=', (size_t)(end - p));
    if (eqp && (size_t)(eqp - p) == kl && strncmp(p, key, kl) == 0) {
      size_t o = 0;
      for (const char *v = eqp + 1; v < end && o + 1 < n; v++) {
        if (*v == '+') { out[o++] = ' '; continue; }
        if (*v == '%' && v + 2 < end) {
          int h = hexv((unsigned char)v[1]), l = hexv((unsigned char)v[2]);
          if (h >= 0 && l >= 0) { out[o++] = (char)(h * 16 + l); v += 2; continue; }
        }
        out[o++] = *v;
      }
      out[o] = 0;
      return 1;
    }
    if (!amp) return 0;
    p = amp + 1;
  }
}

/* ── input validation ─────────────────────────────────────────────────────── */

/* THE path-safety gate. Every upload id that reaches a snprintf() of a path has
 * passed through here first: exactly 36 bytes, lowercase hex, '-' only at
 * 8/13/18/23. "..", "/", "\" and NUL cannot survive it, so traversal is
 * impossible by construction rather than by sanitising a hostile string. */
static int id_is_uuid(const char *s) {
  if (!s) return 0;
  if (strlen(s) != 36) return 0;
  for (int i = 0; i < 36; i++) {
    char c = s[i];
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (c != '-') return 0;
      continue;
    }
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
  }
  return 1;
}

/* filename → storable, JSON-safe DATA. It is never a path component; this runs
 * anyway, because (a) a value that is only data today becomes a path the day
 * someone adds a download route, and (b) invalid UTF-8 would make cJSON emit
 * invalid JSON, turning a hostile filename into a response-corruption bug.
 *
 * Dropped: NUL and control bytes, DEL, '/', '\', a leading '.', trailing dots
 * and spaces, and any byte sequence that is not well-formed UTF-8 (overlongs,
 * lone continuation bytes, surrogates, > U+10FFFF). KEPT: every valid
 * multi-byte sequence — Japanese filenames are the normal case for this
 * product, and mangling them into "____.pdf" would be a real regression.
 * Truncation never splits a sequence. An empty result becomes "upload.bin". */
static void sanitize_filename(const char *in, char *out, size_t cap) {
  size_t o = 0;
  if (in && cap > 16) {
    const unsigned char *p = (const unsigned char *)in;
    while (*p && o + 1 < cap) {
      unsigned char c = *p;
      if (c < 0x80) {
        if (c >= 0x20 && c != 0x7f && c != '/' && c != '\\' &&
            !(o == 0 && c == '.'))
          out[o++] = (char)c;
        p++;
        continue;
      }
      int n = 0;
      unsigned cp = 0;
      if      ((c & 0xE0) == 0xC0) { n = 2; cp = c & 0x1Fu; }
      else if ((c & 0xF0) == 0xE0) { n = 3; cp = c & 0x0Fu; }
      else if ((c & 0xF8) == 0xF0) { n = 4; cp = c & 0x07u; }
      else { p++; continue; }                 /* lone continuation / 0xF8+ */
      int ok = 1;
      for (int i = 1; i < n; i++) {           /* p[i] is the NUL at worst   */
        if ((p[i] & 0xC0) != 0x80) { ok = 0; break; }
        cp = (cp << 6) | (unsigned)(p[i] & 0x3F);
      }
      if (ok) {
        if      (n == 2 && cp < 0x80)    ok = 0;
        else if (n == 3 && cp < 0x800)   ok = 0;
        else if (n == 4 && cp < 0x10000) ok = 0;
        else if (cp >= 0xD800 && cp <= 0xDFFF) ok = 0;
        else if (cp > 0x10FFFF) ok = 0;
      }
      if (!ok) { p++; continue; }
      if (o + (size_t)n + 1 > cap) break;     /* never split a sequence */
      for (int i = 0; i < n; i++) out[o++] = (char)p[i];
      p += n;
    }
  }
  while (o && (out[o - 1] == '.' || out[o - 1] == ' ')) o--;
  out[o] = 0;
  if (o == 0) snprintf(out, cap, "upload.bin");
}

/* Truncate to `max` bytes without leaving a dangling UTF-8 continuation byte.
 * Used for the audit payload: filename is attacker-controlled and audit_events
 * is read and exported by operators, so an unbounded name would be a
 * log-flooding primitive. */
static void utf8_trunc(const char *in, size_t max, char *out, size_t cap) {
  size_t n = in ? strlen(in) : 0;
  if (n > max) n = max;
  if (cap && n + 1 > cap) n = cap - 1;
  while (n > 0 && ((unsigned char)in[n] & 0xC0) == 0x80) n--;
  if (n) memcpy(out, in, n);
  out[n] = 0;
}

/* `type/subtype` with RFC-7231 token characters only, one slash, <=127 bytes.
 * A client does not get to write arbitrary text into a custody record; a value
 * that fails this is passed to evidence as NULL and its own sniff decides. */
static int ct_valid(const char *s, char *out, size_t cap) {
  if (!s || !*s) return 0;
  size_t n = strlen(s);
  if (n > 127 || n + 1 > cap) return 0;
  int slash = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '/') {
      if (slash || i == 0) return 0;
      slash = 1;
      continue;
    }
    if (isalnum(c) || c == '-' || c == '+' || c == '.' || c == '_') continue;
    return 0;
  }
  if (!slash || s[n - 1] == '/') return 0;
  memcpy(out, s, n);
  out[n] = 0;
  return 1;
}

/* ── scratch filesystem ───────────────────────────────────────────────────────
 * $JO_UPLOAD_DIR/<id[0:2]>/<id>/<seq>.part, 0700 dirs and 0600 files. The
 * two-character shard keeps the root from becoming one directory with tens of
 * thousands of entries. Every path here is built from an id that has already
 * passed id_is_uuid() and a seq that has already been range-checked. */

static void up_dir(const char *id, char *out, size_t cap) {
  snprintf(out, cap, "%s/%c%c/%s", up_root(), id[0], id[1], id);
}
static void up_part_path(const char *id, long seq, char *out, size_t cap) {
  snprintf(out, cap, "%s/%c%c/%s/%ld.part", up_root(), id[0], id[1], id, seq);
}
static int ensure_dir(const char *p) {
  struct stat st;
  if (stat(p, &st) == 0) return 0;
  if (mkdir(p, 0700) == 0) return 0;
  return (stat(p, &st) == 0) ? 0 : -1;        /* lost a race → fine */
}
static int up_mkdirs(const char *id) {
  char p[1024];
  if (ensure_dir(up_root()) != 0) return -1;
  snprintf(p, sizeof p, "%s/%c%c", up_root(), id[0], id[1]);
  if (ensure_dir(p) != 0) return -1;
  up_dir(id, p, sizeof p);
  return ensure_dir(p);
}

/* Write the part bytes to a temp file in the upload's own directory and fsync
 * them, WITHOUT publishing it under <seq>.part. The caller renames only after
 * the metadata transaction commits, so a rolled-back part can never overwrite
 * a previously accepted one. Returns 0 and fills `tmp`. */
static int part_stage(const char *id, const void *body, size_t len,
                      char *tmp, size_t tmpcap) {
  if (up_mkdirs(id) != 0) return -1;
  unsigned char r[4]; RAND_bytes(r, 4);
  snprintf(tmp, tmpcap, "%s/%c%c/%s/.tmp-%d-%02x%02x%02x%02x",
           up_root(), id[0], id[1], id, (int)getpid(), r[0], r[1], r[2], r[3]);
  int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0) return -1;
  const char *b = (const char *)body;
  size_t off = 0;
  while (off < len) {
    ssize_t w = write(fd, b + off, len - off);
    if (w <= 0) { close(fd); unlink(tmp); return -1; }
    off += (size_t)w;
  }
  if (fsync(fd) != 0) { close(fd); unlink(tmp); return -1; }
  close(fd);
  return 0;
}

/* Read exactly `want` bytes of one part into dst. Returns 0 on success. A file
 * that is SHORTER or LONGER than the parts table claims is a failure, not a
 * silent short read: the declared-length check at commit is only meaningful if
 * the bytes actually behind it are the ones that were counted. */
static int part_read_into(const char *id, long seq, unsigned char *dst,
                          size_t want) {
  char p[1200];
  up_part_path(id, seq, p, sizeof p);
  int fd = open(p, O_RDONLY);
  if (fd < 0) return -1;
  size_t off = 0;
  while (off < want) {
    ssize_t r = read(fd, dst + off, want - off);
    if (r <= 0) break;
    off += (size_t)r;
  }
  unsigned char extra;
  ssize_t more = read(fd, &extra, 1);
  close(fd);
  return (off == want && more == 0) ? 0 : -1;
}

/* Tear down one upload's scratch directory. DELIBERATELY NOT a recursive
 * delete: only "<digits>.part" and ".tmp-*" are unlinked, anything else is left
 * alone and the rmdir then fails harmlessly. If JO_UPLOAD_DIR is ever pointed
 * at something precious by a bad deploy, this loses nothing. Returns the number
 * of files removed. */
static int part_dir_purge(const char *id) {
  char dir[1024];
  up_dir(id, dir, sizeof dir);
  DIR *d = opendir(dir);
  if (!d) return 0;
  struct dirent *e;
  int removed = 0;
  while ((e = readdir(d)) != NULL) {
    const char *n = e->d_name;
    int ok = 0;
    if (strncmp(n, ".tmp-", 5) == 0) {
      ok = 1;
    } else {
      const char *dot = strrchr(n, '.');
      if (dot && dot != n && strcmp(dot, ".part") == 0) {
        ok = 1;
        for (const char *q = n; q < dot; q++)
          if (*q < '0' || *q > '9') { ok = 0; break; }
      }
    }
    if (!ok) continue;
    char p[1400];
    snprintf(p, sizeof p, "%s/%s", dir, n);
    if (unlink(p) == 0) removed++;
  }
  closedir(d);
  rmdir(dir);            /* fails harmlessly if something unexpected remains */
  return removed;
}

/* ── row access ───────────────────────────────────────────────────────────────
 * EVERY statement in this module that touches `uploads` carries tenant_id=?.
 * upload_parts has no tenant column and needs none: it is reachable only
 * through an upload id that has already been resolved together with the
 * tenant by up_load(), the single entry point below. There is no code path
 * here that touches a part row with an unresolved id. */

typedef struct {
  char      id[40];
  char      user[80];
  char      filename[UP_FILENAME_CAP];
  char      ct[UP_CT_CAP];
  int       has_ct;
  long long total, received, parts;
  char      sha[72];
  char      status[16];
  char      created[32], committed[32];
  int       has_committed;
} up_row;

static int up_load(db_handle *db, const char *tid, const char *id, up_row *r) {
  memset(r, 0, sizeof *r);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT id,user_id,filename,content_type,total_bytes,received_bytes,"
        "part_count,sha256,status,created_at,committed_at FROM uploads "
        "WHERE id=?1 AND tenant_id=?2", -1, &s, NULL) != SQLITE_OK)
    return -1;
  sqlite3_bind_text(s, 1, id,  -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tid, -1, SQLITE_TRANSIENT);
  int found = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    found = 1;
    const char *v;
    if ((v = ctext(s, 0))) snprintf(r->id, sizeof r->id, "%s", v);
    if ((v = ctext(s, 1))) snprintf(r->user, sizeof r->user, "%s", v);
    if ((v = ctext(s, 2))) snprintf(r->filename, sizeof r->filename, "%s", v);
    if ((v = ctext(s, 3))) { snprintf(r->ct, sizeof r->ct, "%s", v); r->has_ct = 1; }
    r->total    = sqlite3_column_int64(s, 4);
    r->received = sqlite3_column_int64(s, 5);
    r->parts    = sqlite3_column_int64(s, 6);
    if ((v = ctext(s, 7))) snprintf(r->sha, sizeof r->sha, "%s", v);
    if ((v = ctext(s, 8))) snprintf(r->status, sizeof r->status, "%s", v);
    if ((v = ctext(s, 9))) snprintf(r->created, sizeof r->created, "%s", v);
    if ((v = ctext(s,10))) {
      snprintf(r->committed, sizeof r->committed, "%s", v);
      r->has_committed = 1;
    }
  }
  sqlite3_finalize(s);
  return found;
}

/* Recompute received_bytes / part_count from upload_parts. ALWAYS a recompute,
 * never an increment: parts are retryable, and an incremented counter turns
 * one retried part into permanent drift that only shows up as a length
 * mismatch at commit, long after the retry that caused it. */
static int up_retotal(db_handle *db, const char *tid, const char *id) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE uploads SET "
        "received_bytes=(SELECT COALESCE(SUM(bytes),0) FROM upload_parts "
        "                WHERE upload_id=?1),"
        "part_count=(SELECT COUNT(*) FROM upload_parts WHERE upload_id=?1) "
        "WHERE id=?1 AND tenant_id=?2", -1, &s, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(s, 1, id,  -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tid, -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  return rc == SQLITE_DONE ? 0 : -1;
}

static void up_parts_delete(db_handle *db, const char *id) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "DELETE FROM upload_parts WHERE upload_id=?1",
                         -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

/* ── evidence bridge ──────────────────────────────────────────────────────── */

static const char *ev_result(int rc) {
  switch (rc) {
    case EV_CAPTURED:      return "captured";
    case EV_SKIP_DISABLED: return "skipped_disabled";
    case EV_SKIP_TOO_BIG:  return "skipped_too_big";
    case EV_SKIP_BUDGET:   return "skipped_budget";
    case EV_SKIP_DUP:      return "duplicate";
    default:               return "error";
  }
}

/* Ask the evidence store itself whether the bytes are on disk, rather than
 * stat()ing a path this module has no business knowing. evidence_list_for_item
 * already computes blob_present for exactly this reason. */
static int blob_present_for(db_handle *db, const char *item_uid) {
  char *js = evidence_list_for_item(db, item_uid, 1);
  if (!js) return 0;
  cJSON *root = cJSON_Parse(js);
  free(js);
  if (!root) return 0;
  int present = 0;
  cJSON *d = cJSON_GetObjectItem(root, "data");
  if (d && cJSON_IsArray(d)) {
    cJSON *e0 = cJSON_GetArrayItem(d, 0);
    cJSON *bp = e0 ? cJSON_GetObjectItem(e0, "blob_present") : NULL;
    present = (bp && cJSON_IsTrue(bp)) ? 1 : 0;
  }
  cJSON_Delete(root);
  return present;
}

/* ── POST /api/uploads/begin ──────────────────────────────────────────────── */

static char *h_begin(db_handle *db, const tenant_ctx *t,
                     const char *body, size_t body_len, int *st) {
  /* The ONE place a body is turned into a C string, and only after its length
   * has been checked. cJSON_ParseWithLength() bounds the tokeniser but its
   * number path still hands the raw pointer to strtod(), which stops on a
   * non-numeric byte and does not know about the length — on a body that
   * ends in a digit and is not NUL-terminated (which hm->body.buf is not)
   * that is a read past the end. A begin body is a three-field object, so a
   * bounded copy costs nothing and removes the question entirely. */
  if (!body || body_len == 0 || body_len > 65536)
    return err(st, 400, "bad_request");
  char *jb = malloc(body_len + 1);
  if (!jb) return err(st, 500, "server_error");
  memcpy(jb, body, body_len);
  jb[body_len] = 0;
  cJSON *b = cJSON_ParseWithLength(jb, body_len + 1);
  free(jb);
  if (!b || !cJSON_IsObject(b)) {
    cJSON_Delete(b);
    return err(st, 400, "bad_request");
  }
  cJSON *jf = cJSON_GetObjectItem(b, "filename");
  cJSON *jc = cJSON_GetObjectItem(b, "content_type");
  cJSON *jt = cJSON_GetObjectItem(b, "total_bytes");

  long long total = 0;
  if (jt && cJSON_IsNumber(jt))      total = (long long)jt->valuedouble;
  else if (jt && cJSON_IsString(jt)) total = strtoll(jt->valuestring, NULL, 10);

  /* Refuse an oversized file HERE, before a single part is sent. Failing on
   * the last part after a phone has spent four minutes on cellular data is a
   * much worse experience than failing in the first round trip, and it is the
   * only point at which the client can still choose a different file. */
  if (total <= 0 || total > up_max_bytes()) {
    cJSON *m = cJSON_CreateObject();
    cJSON_AddNumberToObject(m, "total_bytes", (double)total);
    cJSON_AddNumberToObject(m, "max_bytes", (double)up_max_bytes());
    cJSON_Delete(b);
    return err_meta(st, total <= 0 ? 400 : 413,
                    total <= 0 ? "invalid_total_bytes" : "file_too_large", m);
  }

  char fn[UP_FILENAME_CAP];
  sanitize_filename((jf && cJSON_IsString(jf)) ? jf->valuestring : NULL,
                    fn, sizeof fn);
  char ct[UP_CT_CAP];
  int has_ct = ct_valid((jc && cJSON_IsString(jc)) ? jc->valuestring : NULL,
                        ct, sizeof ct);
  cJSON_Delete(b);

  char id[37];
  uuid4(id);
  if (up_mkdirs(id) != 0) {
    fprintf(stderr, "[upload] cannot create scratch dir under %s\n", up_root());
    return err(st, 500, "server_error");
  }

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO uploads(id,tenant_id,user_id,filename,content_type,"
        "total_bytes,received_bytes,part_count,status) "
        "VALUES(?1,?2,?3,?4,?5,?6,0,0,'open')", -1, &s, NULL) != SQLITE_OK) {
    part_dir_purge(id);
    return err(st, 500, "server_error");
  }
  sqlite3_bind_text (s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text (s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
  if (t->user_id[0]) sqlite3_bind_text(s, 3, t->user_id, -1, SQLITE_TRANSIENT);
  else               sqlite3_bind_null(s, 3);
  sqlite3_bind_text (s, 4, fn, -1, SQLITE_TRANSIENT);
  if (has_ct) sqlite3_bind_text(s, 5, ct, -1, SQLITE_TRANSIENT);
  else        sqlite3_bind_null(s, 5);
  sqlite3_bind_int64(s, 6, (sqlite3_int64)total);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "[upload] begin insert: %s\n", sqlite3_errmsg(db->h));
    part_dir_purge(id);
    return err(st, 500, "server_error");
  }

  long long pmax = up_part_max();
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "upload_id", id);
  cJSON_AddNumberToObject(d, "part_max_bytes", (double)pmax);
  cJSON_AddNumberToObject(d, "max_bytes", (double)up_max_bytes());
  cJSON_AddNumberToObject(d, "total_bytes", (double)total);
  cJSON_AddNumberToObject(d, "part_count_expected",
                          (double)((total + pmax - 1) / pmax));
  cJSON_AddNumberToObject(d, "expires_in_sec", (double)up_ttl_sec());
  cJSON_AddStringToObject(d, "filename", fn);
  cJSON_AddItemToObject(d, "content_type",
                        has_ct ? cJSON_CreateString(ct) : cJSON_CreateNull());
  return wrap(d, st, 201);
}

/* ── POST /api/uploads/:id/part?seq=N ─────────────────────────────────────── */

static char *h_part(db_handle *db, const tenant_ctx *t, const char *id,
                    const char *qs, const char *body, size_t body_len,
                    int *st) {
  char sv[24] = {0};
  if (!qs_get(qs, "seq", sv, sizeof sv) || !sv[0]) return err(st, 400, "invalid_seq");
  for (const char *p = sv; *p; p++)
    if (*p < '0' || *p > '9') return err(st, 400, "invalid_seq");
  long seq = strtol(sv, NULL, 10);
  if (seq < 0 || seq >= UP_MAX_PARTS) {
    cJSON *m = cJSON_CreateObject();
    cJSON_AddNumberToObject(m, "max_seq", (double)(UP_MAX_PARTS - 1));
    return err_meta(st, 400, "invalid_seq", m);
  }
  if (body_len == 0) return err(st, 400, "empty_part");
  if ((long long)body_len > up_part_max()) {
    cJSON *m = cJSON_CreateObject();
    cJSON_AddNumberToObject(m, "bytes", (double)body_len);
    cJSON_AddNumberToObject(m, "part_max_bytes", (double)up_part_max());
    return err_meta(st, 413, "part_too_large", m);
  }

  up_row r;
  int found = up_load(db, t->tenant_id, id, &r);
  if (found < 0) return err(st, 500, "server_error");
  if (found == 0) return err(st, 404, "not_found");   /* other tenant → 404 */
  if (strcmp(r.status, "open") != 0)
    return err(st, 409, strcmp(r.status, "aborted") == 0 ? "upload_aborted"
                                                         : "upload_not_open");

  /* Cheap pre-check so an obviously hostile client is refused before a
   * megabyte is written to disk. It is NOT the authority — the transaction
   * below re-checks the recomputed total, and commit checks again. */
  long long old = 0;
  {
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h,
          "SELECT bytes FROM upload_parts WHERE upload_id=?1 AND seq=?2",
          -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text (s, 1, id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(s, 2, (sqlite3_int64)seq);
      if (sqlite3_step(s) == SQLITE_ROW) old = sqlite3_column_int64(s, 0);
      sqlite3_finalize(s);
    }
  }
  if (r.total > 0 && r.received - old + (long long)body_len > r.total) {
    cJSON *m = cJSON_CreateObject();
    cJSON_AddNumberToObject(m, "received_bytes", (double)r.received);
    cJSON_AddNumberToObject(m, "total_bytes", (double)r.total);
    return err_meta(st, 409, "declared_length_exceeded", m);
  }

  char tmp[1300];
  if (part_stage(id, body, body_len, tmp, sizeof tmp) != 0) {
    fprintf(stderr, "[upload] part write failed for %s seq %ld\n", id, seq);
    return err(st, 500, "server_error");
  }

  /* Metadata first, bytes published second. The rename happens only after the
   * transaction commits, so a rolled-back part can never clobber a previously
   * accepted one — the failure mode of the reverse order is a part whose file
   * and row disagree, which surfaces as a corrupt file instead of an error. */
  sqlite3_exec(db->h, "BEGIN IMMEDIATE", NULL, NULL, NULL);
  int ok = 1;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO upload_parts(upload_id,seq,bytes,received_at) "
        "VALUES(?1,?2,?3,datetime('now')) "
        "ON CONFLICT(upload_id,seq) DO UPDATE SET "
        "bytes=excluded.bytes, received_at=excluded.received_at",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text (s, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 2, (sqlite3_int64)seq);
    sqlite3_bind_int64(s, 3, (sqlite3_int64)body_len);
    if (sqlite3_step(s) != SQLITE_DONE) ok = 0;
    sqlite3_finalize(s);
  } else {
    ok = 0;
  }
  if (ok && up_retotal(db, t->tenant_id, id) != 0) ok = 0;

  up_row after;
  memset(&after, 0, sizeof after);
  if (ok) {
    int f2 = up_load(db, t->tenant_id, id, &after);
    if (f2 != 1) ok = 0;
    else if (after.total > 0 && after.received > after.total) {
      sqlite3_exec(db->h, "ROLLBACK", NULL, NULL, NULL);
      unlink(tmp);
      cJSON *m = cJSON_CreateObject();
      cJSON_AddNumberToObject(m, "received_bytes", (double)after.received);
      cJSON_AddNumberToObject(m, "total_bytes", (double)after.total);
      return err_meta(st, 409, "declared_length_exceeded", m);
    }
  }
  if (!ok) {
    sqlite3_exec(db->h, "ROLLBACK", NULL, NULL, NULL);
    unlink(tmp);
    fprintf(stderr, "[upload] part txn: %s\n", sqlite3_errmsg(db->h));
    return err(st, 500, "server_error");
  }
  sqlite3_exec(db->h, "COMMIT", NULL, NULL, NULL);

  char dst[1200];
  up_part_path(id, seq, dst, sizeof dst);
  if (rename(tmp, dst) != 0) {
    /* The row now claims bytes that are not on disk. Undo the row rather than
     * leave commit to discover it: the client can simply re-send this seq. */
    unlink(tmp);
    sqlite3_stmt *dsx;
    if (sqlite3_prepare_v2(db->h,
          "DELETE FROM upload_parts WHERE upload_id=?1 AND seq=?2",
          -1, &dsx, NULL) == SQLITE_OK) {
      sqlite3_bind_text (dsx, 1, id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(dsx, 2, (sqlite3_int64)seq);
      sqlite3_step(dsx);
      sqlite3_finalize(dsx);
    }
    up_retotal(db, t->tenant_id, id);
    return err(st, 500, "server_error");
  }

  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "upload_id", id);
  cJSON_AddNumberToObject(d, "seq", (double)seq);
  cJSON_AddNumberToObject(d, "bytes", (double)body_len);
  cJSON_AddNumberToObject(d, "received_bytes", (double)after.received);
  cJSON_AddNumberToObject(d, "part_count", (double)after.parts);
  cJSON_AddNumberToObject(d, "total_bytes", (double)after.total);
  cJSON_AddBoolToObject(d, "complete",
                        after.total > 0 && after.received == after.total);
  return wrap(d, st, 200);
}

/* ── POST /api/uploads/:id/commit ─────────────────────────────────────────── */

/* The committed response, rebuilt from the row. Shared by the first commit and
 * by a retried one, so a client that loses the reply to its commit and asks
 * again gets the SAME answer instead of a 409 it cannot act on. */
static char *commit_response(db_handle *db, const up_row *r, const char *uid,
                             const char *ev, int *st, int code) {
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "upload_id", r->id);
  cJSON_AddStringToObject(d, "sha256", r->sha);
  cJSON_AddNumberToObject(d, "bytes", (double)r->received);
  cJSON_AddItemToObject(d, "content_type",
                        r->has_ct ? cJSON_CreateString(r->ct)
                                  : cJSON_CreateNull());
  cJSON_AddStringToObject(d, "filename", r->filename);
  cJSON_AddNumberToObject(d, "part_count", (double)r->parts);
  cJSON_AddStringToObject(d, "item_uid", uid);
  cJSON_AddBoolToObject(d, "blob_present", blob_present_for(db, uid));
  cJSON_AddStringToObject(d, "evidence", ev);
  cJSON_AddItemToObject(d, "committed_at",
                        r->has_committed ? cJSON_CreateString(r->committed)
                                         : cJSON_CreateNull());
  return wrap(d, st, code);
}

static char *h_commit(db_handle *db, const tenant_ctx *t, const char *id,
                      int *st) {
  up_row r;
  int found = up_load(db, t->tenant_id, id, &r);
  if (found < 0) return err(st, 500, "server_error");
  if (found == 0) return err(st, 404, "not_found");

  char uid[64];
  snprintf(uid, sizeof uid, "upload:%s", id);

  if (strcmp(r.status, "committed") == 0)
    return commit_response(db, &r, uid, "already_committed", st, 200);
  if (strcmp(r.status, "open") != 0)
    return err(st, 409, "upload_aborted");

  /* Collect the parts in seq order. */
  long *seqs = NULL;
  long long *lens = NULL;
  int n = 0, cap = 0;
  long long assembled = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT seq,bytes FROM upload_parts WHERE upload_id=?1 ORDER BY seq ASC",
        -1, &s, NULL) != SQLITE_OK) return err(st, 500, "server_error");
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  while (sqlite3_step(s) == SQLITE_ROW) {
    if (n == cap) {
      int nc = cap ? cap * 2 : 64;
      long *q1 = realloc(seqs, (size_t)nc * sizeof *q1);
      long long *q2 = realloc(lens, (size_t)nc * sizeof *q2);
      if (q1) seqs = q1;
      if (q2) lens = q2;
      if (!q1 || !q2) break;
      cap = nc;
    }
    seqs[n] = (long)sqlite3_column_int64(s, 0);
    lens[n] = sqlite3_column_int64(s, 1);
    assembled += lens[n];
    n++;
  }
  sqlite3_finalize(s);

  if (n == 0) { free(seqs); free(lens); return err(st, 409, "no_parts"); }

  /* Contiguity: seqs must run 0..n-1. A gap means the client thinks it is done
   * and is not; telling it exactly which seq is missing is what makes a
   * resumed upload one more round trip instead of a restart. */
  for (int i = 0; i < n; i++) {
    if (seqs[i] != (long)i) {
      cJSON *m = cJSON_CreateObject();
      cJSON_AddNumberToObject(m, "expected_seq", (double)i);
      cJSON_AddNumberToObject(m, "part_count", (double)n);
      free(seqs); free(lens);
      return err_meta(st, 409, "missing_parts", m);
    }
  }

  /* REQUIREMENT: declared vs actual. A client that declares 1 MB and sends
   * 60 MB is broken or hostile; either way the assembled object is not the
   * object that was authorised at begin, so it is refused rather than stored. */
  if (r.total != assembled) {
    cJSON *m = cJSON_CreateObject();
    cJSON_AddNumberToObject(m, "declared_bytes", (double)r.total);
    cJSON_AddNumberToObject(m, "assembled_bytes", (double)assembled);
    free(seqs); free(lens);
    return err_meta(st, 409, "length_mismatch", m);
  }
  if (assembled <= 0 || assembled > up_max_bytes()) {
    free(seqs); free(lens);
    return err(st, 413, "file_too_large");
  }

  unsigned char *buf = malloc((size_t)assembled);
  if (!buf) { free(seqs); free(lens); return err(st, 500, "server_error"); }
  size_t off = 0;
  int bad = 0;
  for (int i = 0; i < n && !bad; i++) {
    if (part_read_into(id, seqs[i], buf + off, (size_t)lens[i]) != 0) bad = 1;
    else off += (size_t)lens[i];
  }
  free(seqs); free(lens);
  if (bad || off != (size_t)assembled) {
    free(buf);
    fprintf(stderr, "[upload] %s: scratch part missing or wrong size\n", id);
    return err(st, 500, "part_read_failed");
  }

  char sha[65];
  sha256_hex_bytes(buf, (size_t)assembled, sha);

  /* THE handoff. See uploadapi.h for why each of these arguments is what it
   * is — in particular why method is "UPLOAD", status is 0 and both header
   * arguments are NULL: there is no HTTP request behind a client upload, and
   * inventing one would put attacker-controlled text into fields whose
   * contract is "what we sent upstream". */
  char curl[64];
  snprintf(curl, sizeof curl, "jo:upload/%s", id);   /* non-HTTP scheme, and
                                   deliberately filename-free: the custody URL
                                   is the field most likely to be echoed. */
  int evrc = evidence_capture(db, uid, UP_SOURCE_ID,
                              curl, "UPLOAD", NULL, 0L, NULL,
                              buf, (size_t)assembled,
                              r.has_ct ? r.ct : NULL);
  free(buf);

  sqlite3_stmt *up;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE uploads SET status='committed', sha256=?1, received_bytes=?2,"
        "part_count=?3, committed_at=datetime('now') "
        "WHERE id=?4 AND tenant_id=?5 AND status='open'",
        -1, &up, NULL) != SQLITE_OK) return err(st, 500, "server_error");
  sqlite3_bind_text (up, 1, sha, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(up, 2, (sqlite3_int64)assembled);
  sqlite3_bind_int64(up, 3, (sqlite3_int64)n);
  sqlite3_bind_text (up, 4, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text (up, 5, t->tenant_id, -1, SQLITE_TRANSIENT);
  int urc = sqlite3_step(up);
  sqlite3_finalize(up);
  if (urc != SQLITE_DONE) {
    fprintf(stderr, "[upload] commit update: %s\n", sqlite3_errmsg(db->h));
    return err(st, 500, "server_error");
  }

  /* Ingress event. The filename is bounded to UP_AUDIT_NAME_MAX bytes on a
   * UTF-8 boundary — it is attacker-controlled and audit rows are read and
   * exported by operators, so an unbounded name is a log-flooding primitive. */
  char safe_name[UP_AUDIT_NAME_MAX + 8];
  utf8_trunc(r.filename, UP_AUDIT_NAME_MAX, safe_name, sizeof safe_name);
  cJSON *pl = cJSON_CreateObject();
  cJSON_AddStringToObject(pl, "sha256", sha);
  cJSON_AddNumberToObject(pl, "bytes", (double)assembled);
  cJSON_AddNumberToObject(pl, "part_count", (double)n);
  cJSON_AddStringToObject(pl, "filename", safe_name);
  cJSON_AddBoolToObject(pl, "filename_truncated",
                        strlen(r.filename) > strlen(safe_name));
  cJSON_AddItemToObject(pl, "content_type",
                        r.has_ct ? cJSON_CreateString(r.ct) : cJSON_CreateNull());
  cJSON_AddStringToObject(pl, "item_uid", uid);
  cJSON_AddStringToObject(pl, "evidence", ev_result(evrc));
  char *pj = cJSON_PrintUnformatted(pl);
  cJSON_Delete(pl);
  audit_write(db, t->tenant_id, t->user_id, "upload.commit", id, pj);
  free(pj);

  /* The bytes live in the content-addressed store now; the scratch copy is
   * pure duplication and is released immediately, not left to the reaper. */
  part_dir_purge(id);
  up_parts_delete(db, id);

  up_row done;
  if (up_load(db, t->tenant_id, id, &done) != 1)
    return err(st, 500, "server_error");
  return commit_response(db, &done, uid, ev_result(evrc), st, 200);
}

/* ── GET /api/uploads/:id ─────────────────────────────────────────────────── */

static char *h_status(db_handle *db, const tenant_ctx *t, const char *id,
                      int *st) {
  up_row r;
  int found = up_load(db, t->tenant_id, id, &r);
  if (found < 0) return err(st, 500, "server_error");
  if (found == 0) return err(st, 404, "not_found");

  cJSON *recv = cJSON_CreateArray();
  cJSON *miss = cJSON_CreateArray();
  int emitted = 0, truncated = 0;
  long expect = 0, maxseq = -1;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT seq FROM upload_parts WHERE upload_id=?1 ORDER BY seq ASC",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(s) == SQLITE_ROW) {
      long q = (long)sqlite3_column_int64(s, 0);
      /* Gaps below the highest seq received are what a resuming client owes
       * us; anything above it, it already knows it has not sent. */
      while (expect < q && cJSON_GetArraySize(miss) < UP_STATUS_SEQ_CAP)
        cJSON_AddItemToArray(miss, cJSON_CreateNumber((double)expect++));
      expect = q + 1;
      if (emitted < UP_STATUS_SEQ_CAP) {
        cJSON_AddItemToArray(recv, cJSON_CreateNumber((double)q));
        emitted++;
      } else {
        truncated = 1;
      }
      maxseq = q;
    }
    sqlite3_finalize(s);
  }

  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "upload_id", r.id);
  cJSON_AddStringToObject(d, "status", r.status);
  cJSON_AddStringToObject(d, "filename", r.filename);
  cJSON_AddItemToObject(d, "content_type",
                        r.has_ct ? cJSON_CreateString(r.ct) : cJSON_CreateNull());
  cJSON_AddNumberToObject(d, "total_bytes", (double)r.total);
  cJSON_AddNumberToObject(d, "received_bytes", (double)r.received);
  cJSON_AddNumberToObject(d, "part_count", (double)r.parts);
  cJSON_AddNumberToObject(d, "part_max_bytes", (double)up_part_max());
  cJSON_AddNumberToObject(d, "next_seq", (double)(maxseq + 1));
  cJSON_AddItemToObject(d, "received_seqs", recv);
  cJSON_AddItemToObject(d, "missing_seqs", miss);
  cJSON_AddBoolToObject(d, "seqs_truncated", truncated);
  cJSON_AddBoolToObject(d, "complete",
                        r.total > 0 && r.received == r.total &&
                        cJSON_GetArraySize(miss) == 0);
  cJSON_AddItemToObject(d, "sha256",
                        r.sha[0] ? cJSON_CreateString(r.sha) : cJSON_CreateNull());
  cJSON_AddStringToObject(d, "created_at", r.created);
  cJSON_AddItemToObject(d, "committed_at",
                        r.has_committed ? cJSON_CreateString(r.committed)
                                        : cJSON_CreateNull());
  cJSON_AddNumberToObject(d, "expires_in_sec", (double)up_ttl_sec());
  return wrap(d, st, 200);
}

/* ── DELETE /api/uploads/:id ──────────────────────────────────────────────── */

static char *h_abort(db_handle *db, const tenant_ctx *t, const char *id,
                     int *st) {
  up_row r;
  int found = up_load(db, t->tenant_id, id, &r);
  if (found < 0) return err(st, 500, "server_error");
  if (found == 0) return err(st, 404, "not_found");
  /* A committed upload's bytes belong to the evidence store and its custody
   * row is immutable by design; abandoning it is not this route's job. */
  if (strcmp(r.status, "committed") == 0) return err(st, 409, "upload_committed");

  int removed = part_dir_purge(id);
  up_parts_delete(db, id);
  /* The ROW survives, marked aborted. A part that was already in flight then
   * gets a coherent 409 instead of a 404 that reads like "your id is wrong". */
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE uploads SET status='aborted', received_bytes=0, part_count=0 "
        "WHERE id=?1 AND tenant_id=?2", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }

  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "upload_id", id);
  cJSON_AddStringToObject(d, "status", "aborted");
  cJSON_AddNumberToObject(d, "bytes_discarded", (double)r.received);
  cJSON_AddNumberToObject(d, "parts_discarded", (double)removed);
  return wrap(d, st, 200);
}

/* ── dispatcher ───────────────────────────────────────────────────────────── */

char *uploadapi(db_handle *db, const tenant_ctx *t, const char *method,
                const char *seg, const char *act, const char *qs,
                const char *body, size_t body_len, int *status) {
  if (!db || !db->h || !t) {
    if (status) *status = 500;
    return NULL;
  }
  if (!method) method = "";
  if (!seg)    seg = "";
  if (!act)    act = "";

  /* analyst+ for everything, reads included: an upload in flight is
   * unreleased evidence, and its status leaks the filename. */
  if (role_rank(t->role) < 2) return err(status, 403, "forbidden");

  int is_post = strcmp(method, "POST")   == 0;
  int is_get  = strcmp(method, "GET")    == 0;
  int is_del  = strcmp(method, "DELETE") == 0;

  if (is_post && !*act && (!*seg || strcmp(seg, "begin") == 0))
    return h_begin(db, t, body, body_len, status);
  if (!*seg) return err(status, 405, "method_not_allowed");

  /* Path safety and tenant safety meet here. A malformed id is 404, not 400,
   * for the same reason a foreign tenant's id is 404: neither answer may tell
   * a prober anything about which ids exist. */
  if (!id_is_uuid(seg)) return err(status, 404, "not_found");

  if (is_post && strcmp(act, "part")   == 0)
    return h_part(db, t, seg, qs, body, body_len, status);
  if (is_post && strcmp(act, "commit") == 0)
    return h_commit(db, t, seg, status);
  if (is_get && !*act)  return h_status(db, t, seg, status);
  if (is_del && !*act)  return h_abort(db, t, seg, status);
  return err(status, 405, "method_not_allowed");
}

/* ── boot migration ───────────────────────────────────────────────────────── */

void uploadapi_migrate(db_handle *db) {
  if (!db || !db->h) return;

  /* Idempotent, and deliberately duplicated from the DDL block in the header:
   * this module must work on a database whose schema.sql has not yet been
   * regenerated, and every statement is CREATE ... IF NOT EXISTS so it is a
   * no-op once it has been. */
  static const char *ddl =
    "CREATE TABLE IF NOT EXISTS uploads ("
    "  id TEXT PRIMARY KEY, tenant_id TEXT NOT NULL, user_id TEXT,"
    "  filename TEXT, content_type TEXT, total_bytes INTEGER,"
    "  received_bytes INTEGER NOT NULL DEFAULT 0,"
    "  part_count INTEGER NOT NULL DEFAULT 0,"
    "  sha256 TEXT, status TEXT NOT NULL DEFAULT 'open',"
    "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
    "  committed_at TEXT);"
    "CREATE TABLE IF NOT EXISTS upload_parts ("
    "  upload_id TEXT NOT NULL, seq INTEGER NOT NULL,"
    "  bytes INTEGER NOT NULL DEFAULT 0,"
    "  received_at TEXT NOT NULL DEFAULT (datetime('now')),"
    "  PRIMARY KEY (upload_id, seq));"
    "CREATE INDEX IF NOT EXISTS idx_uploads_tenant_status"
    "  ON uploads(tenant_id, status, created_at DESC);"
    "CREATE INDEX IF NOT EXISTS idx_uploads_status_created"
    "  ON uploads(status, created_at);"
    "CREATE INDEX IF NOT EXISTS idx_upload_parts_upload"
    "  ON upload_parts(upload_id, seq);";
  char *e = NULL;
  if (sqlite3_exec(db->h, ddl, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "[upload] schema: %s\n", e ? e : "?");
    sqlite3_free(e);
  }

  if (ensure_dir(up_root()) != 0)
    fprintf(stderr, "[upload] scratch root %s is not writable — uploads will "
                    "fail at begin\n", up_root());

  /* evidence.c fails CLOSED on sources.capture_evidence: an unknown source_id,
   * or a known one with the flag off, means evidence_capture() returns
   * EV_SKIP_DISABLED and the assembled bytes are dropped on the floor. So the
   * pseudo-source is created here and opted IN — but only on the insert. If an
   * operator later sets capture_evidence=0 for client-upload, that is a
   * deliberate choice and this must not silently undo it on the next boot. */
  ensure_column(db, "sources", "capture_evidence", "INTEGER NOT NULL DEFAULT 0");
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT OR IGNORE INTO sources(id,name,type,category,url,status) "
        "VALUES('" UP_SOURCE_ID "','Client File Upload','api','investigation',"
        "'internal://upload','online')", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_step(s);
    sqlite3_finalize(s);
    if (sqlite3_changes(db->h) > 0 &&
        sqlite3_prepare_v2(db->h,
          "UPDATE sources SET capture_evidence=1 WHERE id='" UP_SOURCE_ID "'",
          -1, &s, NULL) == SQLITE_OK) {
      sqlite3_step(s);
      sqlite3_finalize(s);
    }
  }

  /* An honest warning beats a mystery. evidence's per-blob ceiling defaults to
   * 8 MiB while ours defaults to 64 MiB; in that configuration a 20 MiB
   * attachment commits, gets a sha256 and an audit row, and stores NO BYTES. */
  long long evmax = env_ll("JO_EVIDENCE_MAX_BLOB_BYTES", 8388608LL, 1024LL,
                           1099511627776LL);
  if (up_max_bytes() > evmax)
    fprintf(stderr, "[upload] JO_UPLOAD_MAX_BYTES (%lld) exceeds "
                    "JO_EVIDENCE_MAX_BLOB_BYTES (%lld); commits above the "
                    "latter will record custody metadata with no bytes\n",
            up_max_bytes(), evmax);
}

/* ── reaper ───────────────────────────────────────────────────────────────────
 * An upload begun and never committed must not leak scratch disk forever. The
 * three passes are described in uploadapi.h; all are bounded, all poll
 * `cancel`, and none of them touches an upload that is still being fed. */

static int cancelled(volatile int *cancel) { return cancel && *cancel; }

/* Collect up to UP_GC_BATCH ids matching one bounded query. Returned array is
 * caller-freed; *n is the count. Ids are collected before any write so no
 * statement is left open across a transaction. */
static char (*gc_collect(db_handle *db, const char *sql, const char *arg,
                         int *n))[40] {
  *n = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, arg, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (s, 2, UP_GC_BATCH);
  char (*ids)[40] = malloc((size_t)UP_GC_BATCH * sizeof *ids);
  if (!ids) { sqlite3_finalize(s); return NULL; }
  int c = 0;
  while (c < UP_GC_BATCH && sqlite3_step(s) == SQLITE_ROW) {
    const char *v = ctext(s, 0);
    if (v && id_is_uuid(v)) snprintf(ids[c++], 40, "%s", v);
  }
  sqlite3_finalize(s);
  *n = c;
  return ids;
}

static void gc_exec_id(db_handle *db, const char *sql, const char *id) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

/* Orphan scratch directories: what a crash between mkdir() and INSERT leaves
 * behind. Bounded by UP_GC_ORPHAN_SCAN so one pass over a pathological tree
 * cannot run forever on a scheduler thread.
 *
 * Orphans are COLLECTED first and unlinked only after both directory handles
 * are closed. Deleting entries out of a directory that is mid-readdir() is the
 * kind of thing that works on every machine it is tested on and silently skips
 * entries on the one it is not. */
static long gc_orphans(db_handle *db, volatile int *cancel) {
  sqlite3_stmt *probe = NULL;
  if (sqlite3_prepare_v2(db->h, "SELECT 1 FROM uploads WHERE id=?1 LIMIT 1",
                         -1, &probe, NULL) != SQLITE_OK) return 0;
  DIR *root = opendir(up_root());
  if (!root) { sqlite3_finalize(probe); return 0; }

  char (*orph)[40] = malloc((size_t)UP_GC_ORPHAN_SCAN * sizeof *orph);
  int no = 0, scanned = 0;
  struct dirent *shard;
  while (orph && (shard = readdir(root)) != NULL && !cancelled(cancel) &&
         scanned < UP_GC_ORPHAN_SCAN) {
    if (strlen(shard->d_name) != 2) continue;      /* only the hash shards */
    char sp[1100];
    snprintf(sp, sizeof sp, "%s/%s", up_root(), shard->d_name);
    DIR *sd = opendir(sp);
    if (!sd) continue;
    struct dirent *e;
    while ((e = readdir(sd)) != NULL && !cancelled(cancel) &&
           scanned < UP_GC_ORPHAN_SCAN) {
      if (!id_is_uuid(e->d_name)) continue;
      scanned++;
      sqlite3_bind_text(probe, 1, e->d_name, -1, SQLITE_TRANSIENT);
      int known = (sqlite3_step(probe) == SQLITE_ROW);
      sqlite3_reset(probe);
      sqlite3_clear_bindings(probe);
      if (!known) snprintf(orph[no++], 40, "%s", e->d_name);
    }
    closedir(sd);
  }
  closedir(root);
  sqlite3_finalize(probe);

  for (int i = 0; i < no; i++) part_dir_purge(orph[i]);
  free(orph);
  return (long)no;
}

long uploadapi_gc(db_handle *db, volatile int *cancel) {
  if (!db || !db->h) return -1;
  long reaped = 0;

  /* Pass 1 — open uploads that have gone quiet. The clock runs from the LAST
   * PART RECEIVED, not from created_at: an upload being resumed across a
   * working day must not be reaped out from under the client at the 24 h mark,
   * while one nobody has touched since yesterday is abandoned by any
   * reasonable definition. */
  char idle[48];
  snprintf(idle, sizeof idle, "-%lld seconds", up_ttl_sec());
  int n = 0;
  char (*ids)[40] = gc_collect(db,
      "SELECT id FROM uploads WHERE status='open' AND "
      "COALESCE((SELECT MAX(received_at) FROM upload_parts p "
      "          WHERE p.upload_id=uploads.id), created_at) "
      "  < datetime('now', ?1) "
      "ORDER BY created_at ASC LIMIT ?2", idle, &n);
  for (int i = 0; ids && i < n && !cancelled(cancel); i++) {
    part_dir_purge(ids[i]);
    gc_exec_id(db, "DELETE FROM upload_parts WHERE upload_id=?1", ids[i]);
    gc_exec_id(db, "UPDATE uploads SET status='aborted', received_bytes=0,"
                   "part_count=0 WHERE id=?1 AND status='open'", ids[i]);
    reaped++;
  }
  free(ids);

  /* Pass 2 — terminal rows past their retention. Until then the row is kept so
   * a client retrying commit gets its answer instead of a bare 404. */
  if (!cancelled(cancel)) {
    char old[48];
    snprintf(old, sizeof old, "-%lld seconds", up_row_ttl_sec());
    n = 0;
    ids = gc_collect(db,
        "SELECT id FROM uploads WHERE status IN ('committed','aborted') AND "
        "COALESCE(committed_at, created_at) < datetime('now', ?1) "
        "ORDER BY created_at ASC LIMIT ?2", old, &n);
    for (int i = 0; ids && i < n && !cancelled(cancel); i++) {
      part_dir_purge(ids[i]);
      gc_exec_id(db, "DELETE FROM upload_parts WHERE upload_id=?1", ids[i]);
      gc_exec_id(db, "DELETE FROM uploads WHERE id=?1", ids[i]);
      reaped++;
    }
    free(ids);
  }

  /* Pass 3 — scratch with no row at all. */
  if (!cancelled(cancel)) reaped += gc_orphans(db, cancel);

  if (reaped)
    fprintf(stderr, "[upload] gc released %ld abandoned upload(s) under %s\n",
            reaped, up_root());
  return reaped;
}
