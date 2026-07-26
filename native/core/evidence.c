/* core/evidence.c — see evidence.h for the design, the DDL and the wiring. */
#include "evidence.h"
#include "audit.h"
#include "operatorgate.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef JO_REPO_ROOT
#define JO_REPO_ROOT "/Users/rayan/JapanOSINT"
#endif

#define EV_DEFAULT_MAX_BYTES  2147483648LL   /* 2 GiB of blob bytes total   */
#define EV_DEFAULT_MAX_BLOB   8388608LL      /* 8 MiB per captured response */
#define EV_GC_DEFAULT_SEC     3600
#define EV_CACHE_SLOTS        128
#define EV_CACHE_TTL          60             /* seconds an opt-in is cached */
#define EV_GC_SCAN_LIMIT      200000         /* distinct blobs per GC pass  */
#define EV_RAW_MAX            (256LL * 1024 * 1024)

/* g_chain guards the read-tail/insert critical section AND g_bytes; both are
 * process-wide state that several collector threads hit concurrently. It is
 * held only across two prepared statements, never across a blob write. */
static pthread_mutex_t g_chain = PTHREAD_MUTEX_INITIALIZER;
static long long       g_bytes = -1;         /* -1 = not yet computed       */

/* ── environment ──────────────────────────────────────────────────────────
 * Read once and cached: these sit on the per-fetch hot path and getenv() is
 * not free. The races on lazy init are benign (same value, either order). */

static const char *root_dir(void) {
  const char *e = getenv("JO_EVIDENCE_DIR");
  return (e && *e) ? e : JO_REPO_ROOT "/data/evidence";
}
static long long env_ll(const char *name, long long dflt, long long lo) {
  const char *e = getenv(name);
  if (!e || !*e) return dflt;
  long long v = strtoll(e, NULL, 10);
  return v >= lo ? v : dflt;
}
static long long max_bytes(void) {
  static long long v = -1;
  if (v < 0) v = env_ll("JO_EVIDENCE_MAX_BYTES", EV_DEFAULT_MAX_BYTES, 1024 * 1024);
  return v;
}
static long long max_blob(void) {
  static long long v = -1;
  if (v < 0) v = env_ll("JO_EVIDENCE_MAX_BLOB_BYTES", EV_DEFAULT_MAX_BLOB, 1024);
  return v;
}
static int ev_disabled(void) {
  static int v = -1;
  if (v < 0) v = getenv("JO_NO_EVIDENCE") ? 1 : 0;
  return v;
}

/* ── small shared helpers (same shapes as alertsapi.c) ────────────────────── */

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
static char *ev_err(int *st, int code, const char *msg) {
  if (st) *st = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", msg);
  char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}
static cJSON *safe_json(const char *s, int as_array) {
  if (s && *s) { cJSON *j = cJSON_Parse(s); if (j) return j; }
  return as_array ? cJSON_CreateArray() : cJSON_CreateObject();
}
/* datetime('now') format, produced in C so the value we hash is byte-identical
 * to the value we store (a DEFAULT would be applied after we hashed). */
static void now_utc(char out[20]) {
  time_t t = time(NULL);
  struct tm g;
  gmtime_r(&t, &g);
  strftime(out, 20, "%Y-%m-%d %H:%M:%S", &g);
}
static int table_exists(db_handle *db, const char *name) {
  sqlite3_stmt *s; int ok = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1 LIMIT 1",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, name, -1, SQLITE_TRANSIENT);
    ok = (sqlite3_step(s) == SQLITE_ROW);
    sqlite3_finalize(s);
  }
  return ok;
}

/* ── hashing ──────────────────────────────────────────────────────────────
 * Identical construction to tenantapi.c's row_hash_of(): a cJSON object built
 * in lexicographic key order, PrintUnformatted (== JSON.stringify), SHA-256
 * hex. Reusing the scheme rather than inventing one means an auditor learns
 * the verification procedure once and it holds for both chains. */

static void sha256_hex_bytes(const void *b, size_t n, char out[65]) {
  unsigned char d[SHA256_DIGEST_LENGTH];
  SHA256((const unsigned char *)b, n, d);
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    snprintf(out + i * 2, 3, "%02x", d[i]);
}
static void put(cJSON *o, const char *k, const char *v) {
  cJSON_AddItemToObject(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}
/* keys sorted: blob_path,captured_at,chain_seq,content_bytes,content_sha256,
 * content_type,id,item_uid,prev_hash,request_headers,request_method,
 * request_url,response_headers,response_status,source_id */
static void row_hash_of(const char *blob_path, const char *captured_at,
    long long chain_seq, long long content_bytes, const char *content_sha256,
    const char *content_type, const char *id, const char *item_uid,
    const char *prev_hash, const char *request_headers,
    const char *request_method, const char *request_url,
    const char *response_headers, long long response_status,
    const char *source_id, char out[65]) {
  cJSON *o = cJSON_CreateObject();
  put(o, "blob_path", blob_path);
  put(o, "captured_at", captured_at);
  cJSON_AddNumberToObject(o, "chain_seq", (double)chain_seq);
  cJSON_AddNumberToObject(o, "content_bytes", (double)content_bytes);
  put(o, "content_sha256", content_sha256);
  put(o, "content_type", content_type);
  put(o, "id", id);
  put(o, "item_uid", item_uid);
  put(o, "prev_hash", prev_hash);
  put(o, "request_headers", request_headers);
  put(o, "request_method", request_method);
  put(o, "request_url", request_url);
  put(o, "response_headers", response_headers);
  cJSON_AddNumberToObject(o, "response_status", (double)response_status);
  put(o, "source_id", source_id);
  char *cj = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  if (cj) { sha256_hex_bytes(cj, strlen(cj), out); free(cj); }
  else    { snprintf(out, 65, "%s", ""); }
}

/* ── redaction ────────────────────────────────────────────────────────────
 * A name-substring rule rather than an allowlist of known header names: the
 * cost of missing one (a stored credential) is far higher than the cost of
 * over-redacting (an opaque header value in a custody record), and 415 sources
 * invent header names we have never seen. */

static int name_is_secret(const char *name, size_t len) {
  char b[128];
  size_t n = len < sizeof b - 1 ? len : sizeof b - 1;
  for (size_t i = 0; i < n; i++) b[i] = (char)tolower((unsigned char)name[i]);
  b[n] = 0;
  return (strstr(b, "auth") || strstr(b, "key") || strstr(b, "token") ||
          strstr(b, "secret") || strstr(b, "password") || strstr(b, "passwd") ||
          strstr(b, "cookie") || strstr(b, "session") ||
          strstr(b, "credential") || strstr(b, "signature")) ? 1 : 0;
}

/* "Name: value" → "Name: [redacted]" when the name looks like a credential.
 * Returned string is owned by the caller. */
static char *redact_header_line(const char *line) {
  const char *colon = strchr(line, ':');
  if (colon && name_is_secret(line, (size_t)(colon - line))) {
    size_t nl = (size_t)(colon - line);
    char *o = malloc(nl + 16);
    if (!o) return NULL;
    memcpy(o, line, nl);
    memcpy(o + nl, ": [redacted]", 13);
    return o;
  }
  return strdup(line);
}

/* NULL-terminated "Name: value" array → JSON array text (NULL if none). */
static char *headers_json(const char *const *hdrs) {
  if (!hdrs || !*hdrs) return NULL;
  cJSON *a = cJSON_CreateArray();
  for (const char *const *h = hdrs; *h; ++h) {
    char *r = redact_header_line(*h);
    cJSON_AddItemToArray(a, cJSON_CreateString(r ? r : ""));
    free(r);
  }
  char *js = cJSON_PrintUnformatted(a);
  cJSON_Delete(a);
  return js;
}

/* Raw response header block → JSON array text, one entry per line. */
static char *header_block_json(const char *block) {
  if (!block || !*block) return NULL;
  cJSON *a = cJSON_CreateArray();
  const char *p = block;
  while (*p) {
    const char *nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    while (len && (p[len - 1] == '\r')) len--;
    if (len) {
      char *line = malloc(len + 1);
      if (line) {
        memcpy(line, p, len); line[len] = 0;
        char *r = redact_header_line(line);
        cJSON_AddItemToArray(a, cJSON_CreateString(r ? r : ""));
        free(r); free(line);
      }
    }
    if (!nl) break;
    p = nl + 1;
  }
  char *js = cJSON_PrintUnformatted(a);
  cJSON_Delete(a);
  return js;
}

/* Strip userinfo and redact credential-looking query parameters. Many Japanese
 * open-data APIs pass the key in the URL, so redacting headers alone would
 * leak every one of them into the custody record. */
static char *redact_url(const char *url) {
  if (!url) return NULL;
  size_t ul = strlen(url);
  char *out = malloc(ul * 3 + 64);
  if (!out) return NULL;
  size_t o = 0;

  const char *p = url;
  const char *scheme = strstr(url, "://");
  if (scheme) {
    const char *hstart = scheme + 3;
    const char *slash = strchr(hstart, '/');
    const char *at = strchr(hstart, '@');
    if (at && (!slash || at < slash)) {
      size_t pre = (size_t)(hstart - url);
      memcpy(out + o, url, pre); o += pre;
      memcpy(out + o, "[redacted]@", 11); o += 11;
      p = at + 1;
    }
  }
  const char *q = strchr(p, '?');
  size_t head = q ? (size_t)(q - p) : strlen(p);
  memcpy(out + o, p, head); o += head;
  if (q) {
    out[o++] = '?';
    const char *k = q + 1;
    while (*k) {
      const char *amp = strchr(k, '&');
      size_t plen = amp ? (size_t)(amp - k) : strlen(k);
      const char *eq = memchr(k, '=', plen);
      size_t nlen = eq ? (size_t)(eq - k) : plen;
      if (eq && name_is_secret(k, nlen)) {
        memcpy(out + o, k, nlen); o += nlen;
        memcpy(out + o, "=[redacted]", 11); o += 11;
      } else {
        memcpy(out + o, k, plen); o += plen;
      }
      if (!amp) break;
      out[o++] = '&';
      k = amp + 1;
    }
  }
  out[o] = 0;
  return out;
}

/* Only used when the caller has no Content-Type (httpclient.c does not read
 * response headers today). Deliberately coarse: this is a hint recorded
 * alongside the bytes, never a reason to render them. */
static const char *sniff_ct(const void *body, size_t n) {
  const unsigned char *p = (const unsigned char *)body;
  size_t i = 0;
  while (i < n && isspace(p[i])) i++;
  if (i >= n) return "application/octet-stream";
  if (p[i] == '{' || p[i] == '[') return "application/json";
  if (p[i] == '<') {
    if (n - i >= 5 && strncasecmp((const char *)p + i, "<?xml", 5) == 0)
      return "application/xml";
    return "text/html";
  }
  return "application/octet-stream";
}

/* ── blob store ───────────────────────────────────────────────────────────
 * $JO_EVIDENCE_DIR/<sha[0:2]>/<sha>. 0700 dirs and 0600 files: raw upstream
 * responses are the most sensitive bytes this process writes to disk. */

static void blob_rel(const char *sha, char *out, size_t cap) {
  snprintf(out, cap, "%c%c/%s", sha[0], sha[1], sha);
}
static void blob_abs(const char *sha, char *out, size_t cap) {
  snprintf(out, cap, "%s/%c%c/%s", root_dir(), sha[0], sha[1], sha);
}
static int ensure_dir(const char *p) {
  struct stat st;
  if (stat(p, &st) == 0) return 0;
  if (mkdir(p, 0700) == 0) return 0;
  return (stat(p, &st) == 0) ? 0 : -1;   /* lost a race → fine */
}
static int blob_exists(const char *sha) {
  char p[1200]; blob_abs(sha, p, sizeof p);
  struct stat st;
  return stat(p, &st) == 0;
}

/* Write the blob if absent. *created is 1 only when THIS call put new bytes on
 * disk. Atomic by construction: the bytes are written and fsync'd to a temp
 * name in the same directory and only then rename()d onto the hash name, so a
 * file at <sha> always contains <sha>'s preimage. A crash leaves a .tmp-*
 * orphan, which is inert and is never read. */
static int blob_write(const char *sha, const void *body, size_t len,
                      int *created) {
  *created = 0;
  char dir[1024], dst[1200], tmp[1300];
  snprintf(dir, sizeof dir, "%s/%c%c", root_dir(), sha[0], sha[1]);
  snprintf(dst, sizeof dst, "%s/%s", dir, sha);
  struct stat st;
  if (stat(dst, &st) == 0) return 0;               /* dedup: never rewrite */
  if (ensure_dir(root_dir()) != 0 || ensure_dir(dir) != 0) return -1;

  unsigned char r[4]; RAND_bytes(r, 4);
  snprintf(tmp, sizeof tmp, "%s/.tmp-%d-%02x%02x%02x%02x", dir, (int)getpid(),
           r[0], r[1], r[2], r[3]);
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
  if (rename(tmp, dst) != 0) { unlink(tmp); return -1; }
  *created = 1;
  return 0;
}

/* ── per-source opt-in (sources.capture_evidence) ─────────────────────────
 * Cached for EV_CACHE_TTL seconds in a fixed 128-slot direct-mapped table: the
 * check runs on every fetch and must not add a SQLite round trip (and its
 * write-lock contention) to the collector hot path. Cost of the cache: an
 * operator flipping the flag waits up to a minute. Fail-closed everywhere —
 * a missing column (ensure_column not yet run) prepares as an error and
 * reports OFF, so a half-migrated deployment captures nothing rather than
 * capturing everything. */

typedef struct { char id[80]; int on; time_t ts; } evc_ent;
static evc_ent         g_evc[EV_CACHE_SLOTS];
static pthread_mutex_t g_evc_lock = PTHREAD_MUTEX_INITIALIZER;

static unsigned evc_hash(const char *s) {
  unsigned h = 2166136261u;
  while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
  return h;
}
static int source_capture_on(db_handle *db, const char *sid) {
  time_t now = time(NULL);
  unsigned slot = evc_hash(sid) % EV_CACHE_SLOTS;

  pthread_mutex_lock(&g_evc_lock);
  if (strcmp(g_evc[slot].id, sid) == 0 && now - g_evc[slot].ts < EV_CACHE_TTL) {
    int on = g_evc[slot].on;
    pthread_mutex_unlock(&g_evc_lock);
    return on;
  }
  pthread_mutex_unlock(&g_evc_lock);

  int on = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "SELECT capture_evidence FROM sources WHERE id=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, sid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) != SQLITE_NULL)
      on = sqlite3_column_int(s, 0) != 0;
    sqlite3_finalize(s);
  }

  pthread_mutex_lock(&g_evc_lock);
  snprintf(g_evc[slot].id, sizeof g_evc[slot].id, "%s", sid);
  g_evc[slot].on = on;
  g_evc[slot].ts = now;
  pthread_mutex_unlock(&g_evc_lock);
  return on;
}

/* ── byte budget ──────────────────────────────────────────────────────────── */

/* Sum of distinct blob sizes as the DATABASE believes them. Used once, lazily,
 * to seed g_bytes before the first GC pass has run; it over-counts blobs that
 * were reaped in a previous process (their rows survive by design), which is
 * the fail-safe direction — capture pauses early and the first GC pass, which
 * stat()s every blob, replaces the estimate with the truth. */
static long long store_bytes_db(db_handle *db) {
  sqlite3_stmt *s; long long total = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT COALESCE(SUM(b),0) FROM "
        "(SELECT MAX(content_bytes) b FROM evidence GROUP BY content_sha256)",
        -1, &s, NULL) != SQLITE_OK) return 0;
  if (sqlite3_step(s) == SQLITE_ROW) total = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return total;
}
static void warn_budget(long long used, long long budget) {
  static time_t last = 0;
  time_t now = time(NULL);
  if (now - last < 300) return;
  last = now;
  fprintf(stderr, "[evidence] at budget (%lld/%lld bytes) — capture paused "
                  "until the reaper frees space\n", used, budget);
}

/* ── chain + capture ──────────────────────────────────────────────────────── */

static void chain_tail(db_handle *db, char prev[65], long long *seq) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT row_hash,chain_seq FROM evidence WHERE row_hash IS NOT NULL "
        "ORDER BY chain_seq DESC LIMIT 1", -1, &s, NULL) != SQLITE_OK) return;
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *rh = ctext(s, 0);
    if (rh) snprintf(prev, 65, "%s", rh);
    *seq = sqlite3_column_int64(s, 1) + 1;
  }
  sqlite3_finalize(s);
}

int evidence_capture(db_handle *db, const char *item_uid, const char *source_id,
                     const char *url, const char *method,
                     const char *const *req_headers,
                     long status, const char *resp_headers,
                     const void *body, size_t body_len,
                     const char *content_type) {
  if (ev_disabled()) return EV_SKIP_DISABLED;
  if (!db || !db->h || !source_id || !*source_id) return EV_SKIP_DISABLED;
  if (!body) { body = ""; body_len = 0; }        /* a 204 is still evidence */
  if ((long long)body_len > max_blob()) return EV_SKIP_TOO_BIG;
  if (!source_capture_on(db, source_id)) return EV_SKIP_DISABLED;

  /* No specific item in flight → file under the source. With
   * UNIQUE(content_sha256,item_uid) that collapses a 60s poll of unchanged
   * bytes to a single custody row instead of 1,440 a day. */
  char uid[288];
  if (item_uid && *item_uid) snprintf(uid, sizeof uid, "%s", item_uid);
  else                       snprintf(uid, sizeof uid, "src:%s", source_id);

  char sha[65];
  sha256_hex_bytes(body, body_len, sha);

  /* An existing blob costs no new bytes, so it is admitted even at budget —
   * refusing it would only cost us the custody row, not disk. */
  if (!blob_exists(sha)) {
    pthread_mutex_lock(&g_chain);
    if (g_bytes < 0) g_bytes = store_bytes_db(db);
    long long used = g_bytes, budget = max_bytes();
    pthread_mutex_unlock(&g_chain);
    if (used >= budget) { warn_budget(used, budget); return EV_SKIP_BUDGET; }
  }

  int created = 0;
  if (blob_write(sha, body, body_len, &created) != 0) {
    fprintf(stderr, "[evidence] blob write failed for %s (%s)\n", sha, source_id);
    return EV_ERR;                               /* never fails the fetch */
  }

  char rel[80];  blob_rel(sha, rel, sizeof rel);
  char ts[20];   now_utc(ts);
  char id[37];   uuid4(id);
  char *rurl = redact_url(url);
  char *rhj  = headers_json(req_headers);
  char *shj  = header_block_json(resp_headers);
  const char *ct = (content_type && *content_type)
                     ? content_type : sniff_ct(body, body_len);

  int rc = EV_CAPTURED;
  pthread_mutex_lock(&g_chain);
  char prev[65] = "GENESIS";
  long long seq = 1;
  chain_tail(db, prev, &seq);

  char rh[65];
  row_hash_of(rel, ts, seq, (long long)body_len, sha, ct, id, uid, prev,
              rhj, method, rurl, shj, (long long)status, source_id, rh);

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO evidence(id,item_uid,source_id,captured_at,request_url,"
        "request_method,request_headers,response_status,response_headers,"
        "content_sha256,content_bytes,content_type,blob_path,prev_hash,"
        "row_hash,chain_seq) VALUES"
        "(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16) "
        "ON CONFLICT(content_sha256,item_uid) DO NOTHING",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text (s,  1, id,        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (s,  2, uid,       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (s,  3, source_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (s,  4, ts,        -1, SQLITE_TRANSIENT);
    if (rurl) sqlite3_bind_text(s, 5, rurl, -1, SQLITE_TRANSIENT);
    else      sqlite3_bind_null(s, 5);
    if (method) sqlite3_bind_text(s, 6, method, -1, SQLITE_TRANSIENT);
    else        sqlite3_bind_null(s, 6);
    if (rhj) sqlite3_bind_text(s, 7, rhj, -1, SQLITE_TRANSIENT);
    else     sqlite3_bind_null(s, 7);
    sqlite3_bind_int64(s, 8, (sqlite3_int64)status);
    if (shj) sqlite3_bind_text(s, 9, shj, -1, SQLITE_TRANSIENT);
    else     sqlite3_bind_null(s, 9);
    sqlite3_bind_text (s, 10, sha, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 11, (sqlite3_int64)body_len);
    sqlite3_bind_text (s, 12, ct,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (s, 13, rel, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (s, 14, prev, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (s, 15, rh,   -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 16, (sqlite3_int64)seq);
    if (sqlite3_step(s) != SQLITE_DONE) {
      fprintf(stderr, "[evidence] insert: %s\n", sqlite3_errmsg(db->h));
      rc = EV_ERR;
    } else if (sqlite3_changes(db->h) == 0) {
      rc = EV_SKIP_DUP;      /* same bytes already recorded for this uid */
    }
    sqlite3_finalize(s);
  } else {
    fprintf(stderr, "[evidence] prepare insert: %s\n", sqlite3_errmsg(db->h));
    rc = EV_ERR;
  }
  /* The blob occupies disk whether or not the row was new (a re-capture after
   * an eviction legitimately restores the bytes under the surviving row). */
  if (created && g_bytes >= 0) g_bytes += (long long)body_len;
  pthread_mutex_unlock(&g_chain);

  free(rurl); free(rhj); free(shj);
  return rc;
}

/* ── thread-local capture scope + the httpclient hook ─────────────────────── */

typedef struct {
  db_handle *db;
  char source_id[96];
  char item_uid[288];
  int  active;
} ev_scope;
static __thread ev_scope g_scope;

void evidence_scope_begin(db_handle *db, const char *source_id,
                          const char *item_uid) {
  g_scope.active = 0;
  if (!db || !db->h || !source_id || !*source_id) return;
  snprintf(g_scope.source_id, sizeof g_scope.source_id, "%s", source_id);
  if (item_uid && *item_uid)
    snprintf(g_scope.item_uid, sizeof g_scope.item_uid, "%s", item_uid);
  else
    g_scope.item_uid[0] = 0;
  g_scope.db = db;
  g_scope.active = 1;
}

void evidence_scope_end(void) {
  g_scope.active = 0;
  g_scope.db = NULL;
  g_scope.source_id[0] = 0;
  g_scope.item_uid[0] = 0;
}

int evidence_http_hook(const char *method, const char *url,
                       const char *const *req_headers,
                       long status, const void *body, size_t body_len) {
  if (!g_scope.active || !g_scope.db) return EV_SKIP_DISABLED;
  return evidence_capture(g_scope.db,
                          g_scope.item_uid[0] ? g_scope.item_uid : NULL,
                          g_scope.source_id, url, method, req_headers,
                          status, NULL, body, body_len, NULL);
}

/* ── read paths ───────────────────────────────────────────────────────────── */

char *evidence_list_for_item(db_handle *db, const char *item_uid, int limit) {
  if (!db || !db->h || !item_uid || !*item_uid) return NULL;
  int lim = limit > 0 ? limit : 50;
  if (lim > 200) lim = 200;

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT id,captured_at,source_id,request_url,request_method,"
        "request_headers,response_status,response_headers,content_sha256,"
        "content_bytes,content_type,blob_path,prev_hash,row_hash,chain_seq "
        "FROM evidence WHERE item_uid=?1 "
        "ORDER BY captured_at DESC, chain_seq DESC LIMIT ?2",
        -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, item_uid, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (s, 2, lim);

  cJSON *arr = cJSON_CreateArray();
  int count = 0, present = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "id", ctext(s, 0) ? ctext(s, 0) : "");
    cJSON_AddStringToObject(o, "captured_at", ctext(s, 1) ? ctext(s, 1) : "");
    cJSON_AddStringToObject(o, "source_id", ctext(s, 2) ? ctext(s, 2) : "");
    const char *u = ctext(s, 3);
    cJSON_AddItemToObject(o, "request_url",
                          u ? cJSON_CreateString(u) : cJSON_CreateNull());
    const char *m = ctext(s, 4);
    cJSON_AddItemToObject(o, "request_method",
                          m ? cJSON_CreateString(m) : cJSON_CreateNull());
    cJSON_AddItemToObject(o, "request_headers", safe_json(ctext(s, 5), 1));
    cJSON_AddNumberToObject(o, "response_status",
                            (double)sqlite3_column_int64(s, 6));
    cJSON_AddItemToObject(o, "response_headers", safe_json(ctext(s, 7), 1));
    const char *sha = ctext(s, 8) ? ctext(s, 8) : "";
    cJSON_AddStringToObject(o, "content_sha256", sha);
    cJSON_AddNumberToObject(o, "content_bytes",
                            (double)sqlite3_column_int64(s, 9));
    const char *ct = ctext(s, 10);
    cJSON_AddItemToObject(o, "content_type",
                          ct ? cJSON_CreateString(ct) : cJSON_CreateNull());
    cJSON_AddStringToObject(o, "blob_path", ctext(s, 11) ? ctext(s, 11) : "");
    const char *ph = ctext(s, 12);
    cJSON_AddItemToObject(o, "prev_hash",
                          ph ? cJSON_CreateString(ph) : cJSON_CreateNull());
    const char *rh = ctext(s, 13);
    cJSON_AddItemToObject(o, "row_hash",
                          rh ? cJSON_CreateString(rh) : cJSON_CreateNull());
    cJSON_AddNumberToObject(o, "chain_seq",
                            (double)sqlite3_column_int64(s, 14));
    /* The row is the custody record; the blob may have been reaped under the
     * byte budget. Say so plainly rather than 404ing later. */
    int have = (strlen(sha) > 2) && blob_exists(sha);
    cJSON_AddBoolToObject(o, "blob_present", have);
    if (have) present++;
    cJSON_AddItemToArray(arr, o);
    count++;
  }
  sqlite3_finalize(s);

  cJSON *page = cJSON_CreateObject();
  cJSON_AddNumberToObject(page, "limit", lim);
  cJSON_AddNumberToObject(page, "count", count);
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "item_uid", item_uid);
  cJSON_AddNumberToObject(meta, "present", present);
  cJSON_AddNumberToObject(meta, "evicted", count - present);
  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "data", arr);
  cJSON_AddItemToObject(root, "page", page);
  cJSON_AddItemToObject(root, "meta", meta);
  char *js = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return js;
}

char *evidence_verify(db_handle *db) {
  if (!db || !db->h) return NULL;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT id,item_uid,source_id,captured_at,request_url,request_method,"
        "request_headers,response_status,response_headers,content_sha256,"
        "content_bytes,content_type,blob_path,prev_hash,row_hash,chain_seq "
        "FROM evidence WHERE row_hash IS NOT NULL "
        "ORDER BY chain_seq ASC LIMIT 100000", -1, &s, NULL) != SQLITE_OK)
    return NULL;

  typedef struct {
    char *id,*uid,*src,*ts,*url,*mth,*rhj,*shj,*sha,*ct,*bp,*ph,*rh;
    long long st, bytes, seq;
  } row_t;
  row_t *R = NULL; int n = 0, cap = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    if (n == cap) {
      int nc = cap ? cap * 2 : 128;
      row_t *q = realloc(R, (size_t)nc * sizeof *q);
      if (!q) break;
      R = q; cap = nc;
    }
    row_t *r = &R[n++];
    #define DUP(i) (ctext(s,i) ? strdup(ctext(s,i)) : NULL)
    r->id=DUP(0); r->uid=DUP(1); r->src=DUP(2); r->ts=DUP(3); r->url=DUP(4);
    r->mth=DUP(5); r->rhj=DUP(6); r->st=sqlite3_column_int64(s,7);
    r->shj=DUP(8); r->sha=DUP(9); r->bytes=sqlite3_column_int64(s,10);
    r->ct=DUP(11); r->bp=DUP(12); r->ph=DUP(13); r->rh=DUP(14);
    r->seq=sqlite3_column_int64(s,15);
    #undef DUP
  }
  sqlite3_finalize(s);

  cJSON *o = cJSON_CreateObject();
  const char *expected_prev = "GENESIS";
  long long expected_seq = 1;
  int broken = -1; const char *reason = NULL; char rbuf[96];
  for (int i = 0; i < n; i++) {
    row_t *r = &R[i];
    if (r->seq != expected_seq) {
      snprintf(rbuf, sizeof rbuf, "chain_seq gap (expected %lld, got %lld)",
               expected_seq, r->seq);
      reason = rbuf; broken = i; break;
    }
    if (strcmp(r->ph ? r->ph : "", expected_prev) != 0) {
      reason = "prev_hash linkage mismatch (row inserted/removed)";
      broken = i; break;
    }
    char rh[65];
    row_hash_of(r->bp, r->ts, r->seq, r->bytes, r->sha, r->ct, r->id, r->uid,
                r->ph, r->rhj, r->mth, r->url, r->shj, r->st, r->src, rh);
    if (strcmp(rh, r->rh ? r->rh : "") != 0) {
      reason = "row_hash mismatch (row altered)"; broken = i; break;
    }
    expected_prev = r->rh;
    expected_seq += 1;
  }

  if (broken >= 0) {
    cJSON_AddBoolToObject(o, "ok", 0);
    cJSON_AddNumberToObject(o, "count", n);
    cJSON_AddNumberToObject(o, "brokenAt", broken);
    cJSON_AddItemToObject(o, "brokenId", R[broken].id
      ? cJSON_CreateString(R[broken].id) : cJSON_CreateNull());
    cJSON_AddStringToObject(o, "reason", reason);
  } else {
    cJSON_AddBoolToObject(o, "ok", 1);
    cJSON_AddNumberToObject(o, "count", n);
    cJSON_AddItemToObject(o, "tip",
      strcmp(expected_prev, "GENESIS") == 0 ? cJSON_CreateNull()
                                            : cJSON_CreateString(expected_prev));
  }
  for (int i = 0; i < n; i++) {
    free(R[i].id); free(R[i].uid); free(R[i].src); free(R[i].ts);
    free(R[i].url); free(R[i].mth); free(R[i].rhj); free(R[i].shj);
    free(R[i].sha); free(R[i].ct); free(R[i].bp); free(R[i].ph); free(R[i].rh);
  }
  free(R);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

char *evidence_raw(db_handle *db, const auth_user *u, const char *evidence_id,
                   unsigned char **out_bytes, size_t *out_len,
                   char *out_sha, int *status) {
  if (status) *status = 200;
  if (!db || !db->h || !out_bytes || !out_len)
    return ev_err(status, 500, "server_error");
  if (!evidence_id || !*evidence_id)
    return ev_err(status, 400, "bad_request");

  /* Raw upstream bytes are not tenant data and are not role-gated: they are
   * platform-operator material, same posture as the breach reveal path. */
  int oc = opgate_check(u);
  if (oc == -401)  return ev_err(status, 401, "Auth required");
  if (oc == -1403) return ev_err(status, 403, "Platform operator access not configured");
  if (oc != 0)     return ev_err(status, 403, "Platform operator role required");

  char sha[65] = {0}, uid[288] = {0}, src[96] = {0};
  long long bytes = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT content_sha256,content_bytes,item_uid,source_id FROM evidence "
        "WHERE id=?1", -1, &s, NULL) != SQLITE_OK)
    return ev_err(status, 500, "server_error");
  sqlite3_bind_text(s, 1, evidence_id, -1, SQLITE_TRANSIENT);
  int found = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    found = 1;
    if (ctext(s, 0)) snprintf(sha, sizeof sha, "%s", ctext(s, 0));
    bytes = sqlite3_column_int64(s, 1);
    if (ctext(s, 2)) snprintf(uid, sizeof uid, "%s", ctext(s, 2));
    if (ctext(s, 3)) snprintf(src, sizeof src, "%s", ctext(s, 3));
  }
  sqlite3_finalize(s);
  if (!found)          return ev_err(status, 404, "not_found");
  if (strlen(sha) < 3) return ev_err(status, 500, "server_error");

  char p[1200]; blob_abs(sha, p, sizeof p);
  struct stat st;
  if (stat(p, &st) != 0)
    return ev_err(status, 410, "blob_evicted");   /* row intact, bytes reaped */
  if ((long long)st.st_size > EV_RAW_MAX)
    return ev_err(status, 500, "blob_too_large");

  int fd = open(p, O_RDONLY);
  if (fd < 0) return ev_err(status, 410, "blob_evicted");
  size_t len = (size_t)st.st_size;
  unsigned char *buf = malloc(len ? len : 1);
  if (!buf) { close(fd); return ev_err(status, 500, "server_error"); }
  size_t off = 0;
  while (off < len) {
    ssize_t r = read(fd, buf + off, len - off);
    if (r <= 0) break;
    off += (size_t)r;
  }
  close(fd);
  if (off != len) { free(buf); return ev_err(status, 500, "server_error"); }

  /* Integrity is checked on the way out, not just on the way in: a blob whose
   * bytes no longer hash to its name is exactly the case this module exists
   * to detect, and serving it as evidence would be worse than serving nothing. */
  char actual[65];
  sha256_hex_bytes(buf, len, actual);
  if (strcmp(actual, sha) != 0) {
    fprintf(stderr, "[evidence] blob %s hashes to %s — refusing to serve\n",
            sha, actual);
    free(buf);
    return ev_err(status, 500, "blob_integrity_failed");
  }

  cJSON *pl = cJSON_CreateObject();
  cJSON_AddStringToObject(pl, "sha256", sha);
  cJSON_AddNumberToObject(pl, "bytes", (double)bytes);
  cJSON_AddStringToObject(pl, "item_uid", uid);
  cJSON_AddStringToObject(pl, "source_id", src);
  char *pj = cJSON_PrintUnformatted(pl);
  cJSON_Delete(pl);
  audit_write(db, "platform", u ? u->id : NULL, "evidence.raw", evidence_id, pj);
  free(pj);

  *out_bytes = buf;
  *out_len = len;
  if (out_sha) snprintf(out_sha, 65, "%s", sha);
  if (status) *status = 200;
  return NULL;
}

/* ── reaper ───────────────────────────────────────────────────────────────
 * Evicts BLOBS ONLY, oldest-last-captured first, never a blob whose item is
 * pinned into a case. Rows are immutable: deleting one would break the chain
 * for every row after it and read as tampering, so the custody record of an
 * evicted capture survives its bytes. */

char *evidence_gc(db_handle *db) {
  if (!db || !db->h) return NULL;
  long long budget = max_bytes();
  long long low = budget - budget / 10;         /* reap to 90%, not to the line */
  int have_cases = table_exists(db, "case_items");

  typedef struct { char sha[65]; long long bytes; } cand;
  cand *C = NULL; int n = 0, cap = 0;
  long long total = 0;

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT content_sha256, MAX(captured_at) FROM evidence "
        "GROUP BY content_sha256 ORDER BY MAX(captured_at) ASC LIMIT ?1",
        -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_int(s, 1, EV_GC_SCAN_LIMIT);
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *sh = ctext(s, 0);
    if (!sh || strlen(sh) < 3) continue;
    char p[1200]; blob_abs(sh, p, sizeof p);
    struct stat st;
    if (stat(p, &st) != 0) continue;        /* already gone — not on the books */
    if (n == cap) {
      int nc = cap ? cap * 2 : 256;
      cand *q = realloc(C, (size_t)nc * sizeof *q);
      if (!q) break;
      C = q; cap = nc;
    }
    snprintf(C[n].sha, sizeof C[n].sha, "%s", sh);
    C[n].bytes = (long long)st.st_size;     /* on-disk truth, not the DB's claim */
    total += C[n].bytes;
    n++;
  }
  sqlite3_finalize(s);

  /* A blob is pinned when ANY custody row carrying it belongs to an intel item
   * someone pinned into a case. If case_items does not exist in this database
   * nothing can be pinned, so reaping proceeds unprotected — correct, not a
   * degradation. */
  sqlite3_stmt *pin = NULL;
  if (have_cases &&
      sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM evidence e JOIN case_items ci ON ci.ref_id=e.item_uid "
        "AND ci.ref_type='intel_item' WHERE e.content_sha256=?1 LIMIT 1",
        -1, &pin, NULL) != SQLITE_OK) pin = NULL;

  long long before = total, freed = 0;
  int evicted = 0, pinned = 0;
  for (int i = 0; i < n && total > low; i++) {
    if (pin) {
      sqlite3_bind_text(pin, 1, C[i].sha, -1, SQLITE_TRANSIENT);
      int is_pinned = (sqlite3_step(pin) == SQLITE_ROW);
      sqlite3_reset(pin);
      sqlite3_clear_bindings(pin);
      if (is_pinned) { pinned++; continue; }
    }
    char p[1200]; blob_abs(C[i].sha, p, sizeof p);
    if (unlink(p) == 0) {
      total -= C[i].bytes; freed += C[i].bytes; evicted++;
    }
  }
  if (pin) sqlite3_finalize(pin);
  free(C);

  pthread_mutex_lock(&g_chain);
  g_bytes = total;                 /* the stat() pass is authoritative */
  pthread_mutex_unlock(&g_chain);

  cJSON *d = cJSON_CreateObject();
  cJSON_AddNumberToObject(d, "blobs_scanned", n);
  cJSON_AddNumberToObject(d, "bytes_before", (double)before);
  cJSON_AddNumberToObject(d, "bytes_after", (double)total);
  cJSON_AddNumberToObject(d, "bytes_freed", (double)freed);
  cJSON_AddNumberToObject(d, "blobs_evicted", evicted);
  cJSON_AddNumberToObject(d, "blobs_pinned_skipped", pinned);
  cJSON_AddNumberToObject(d, "budget_bytes", (double)budget);
  cJSON_AddNumberToObject(d, "low_water_bytes", (double)low);
  cJSON_AddBoolToObject(d, "case_items_present", have_cases);
  cJSON_AddBoolToObject(d, "over_budget", total > budget);
  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "data", d);
  char *js = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  if (evicted)
    fprintf(stderr, "[evidence] gc evicted %d blob(s), freed %lld bytes "
                    "(%lld -> %lld of %lld)\n", evicted, freed, before, total, budget);
  return js;
}

/* ── reaper pod ───────────────────────────────────────────────────────────── */

static pthread_t    g_gc_thread;
static volatile int g_gc_stop = 0;
static int          g_gc_running = 0;

static void *gc_thread(void *arg) {
  db_handle *db = (db_handle *)arg;
  int interval = (int)env_ll("JO_EVIDENCE_GC_INTERVAL_SEC", EV_GC_DEFAULT_SEC, 60);
  if (interval > 86400) interval = 86400;
  /* One pass shortly after boot: it replaces the DB-derived byte estimate with
   * a stat()ed total, so capture is not throttled by a stale over-count. */
  for (int i = 0; i < 30 && !g_gc_stop; i++) sleep(1);
  while (!g_gc_stop) {
    char *r = evidence_gc(db);
    free(r);
    /* Chunked sleep so shutdown is bounded by 1s, not by the GC interval. */
    for (int i = 0; i < interval && !g_gc_stop; i++) sleep(1);
  }
  return NULL;
}

void evidence_gc_start(db_handle *db) {
  if (g_gc_running) return;
  if (getenv("JO_NO_EVIDENCE_GC")) {
    fprintf(stderr, "[evidence] reaper disabled (JO_NO_EVIDENCE_GC) — "
                    "the byte budget will only pause capture, never shrink the store\n");
    return;
  }
  if (!db || !db->h) return;
  g_gc_stop = 0;
  if (pthread_create(&g_gc_thread, NULL, gc_thread, db) == 0) {
    g_gc_running = 1;
    fprintf(stderr, "[evidence] reaper started (budget %lld bytes, dir %s)\n",
            max_bytes(), root_dir());
  } else {
    fprintf(stderr, "[evidence] pthread_create failed; store will not be reaped\n");
  }
}

void evidence_gc_stop(void) {
  if (!g_gc_running) return;
  g_gc_stop = 1;
  pthread_join(g_gc_thread, NULL);
  g_gc_running = 0;
  fprintf(stderr, "[evidence] reaper stopped\n");
}
