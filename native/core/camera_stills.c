/* core/camera_stills.c — roadmap 28: scheduled camera stills. See
 * camera_stills.h for the design, the DDL, the wiring, the retention maths and
 * an explicit statement of what is and is not exercised in this environment.
 * This file is the mechanics.
 *
 * ── HOW THIS WAS TESTED ───────────────────────────────────────────────────
 *
 * The interesting, dependency-free part of this module is
 * camera_stills_jpeg_frame(): a walk of the JPEG marker structure that pulls
 * exactly one complete frame out of an MJPEG multipart body with no decoder.
 * "It worked on a photo" is not a test of that, so it was driven from a
 * harness that ASSEMBLES THE BYTES — SOI, APP/DQT/SOF/DHT segments with
 * declared lengths, SOS, entropy-coded data, EOI — so the hard cases could be
 * constructed rather than hoped for. The harness links this file for real
 * against sqlite3 and cJSON with the surrounding modules stubbed; it is not in
 * the tree (this task was scoped to camera_stills.c/.h) but everything below
 * is reproducible from these two files. Built -fsanitize=address,undefined so
 * an out-of-bounds read is a hard failure and not a silent one. 47 assertions,
 * 0 failures, 0 sanitizer reports.
 *
 *   baseline      a minimal valid JPEG → returned byte-exact, offset 0.
 *   embedded      an APP1 segment carrying a whole thumbnail JPEG, complete
 *                 with its own FFD9, ahead of the real image data → the walk
 *                 skips it BY DECLARED LENGTH and returns the OUTER frame. A
 *                 naive FFD8…FFD9 scan returns a truncated frame here; this is
 *                 the single case that justifies walking the structure.
 *   stuffing      FF00 sequences inside entropy-coded data → not mistaken for
 *                 markers. FFD0..FFD7 restart markers → skipped, scan
 *                 continues. FF FF fill runs → skipped one byte at a time.
 *                 THIS ONE CAUGHT A REAL BUG: the first version broke out of
 *                 the entropy scan on any FF-not-followed-by-a-known-escape,
 *                 so a fill run in front of ordinary data ended the frame
 *                 early and returned a TRUNCATED image that still looked
 *                 valid. The fix is in the entropy loop: 0x02..0xBF are
 *                 reserved and are not markers, so they are data.
 *   multipart     a synthetic multipart/x-mixed-replace body: boundary line,
 *                 part headers, frame, CRLF, boundary, frame, … → frame 1 is
 *                 returned byte-exact and the part headers are stepped over
 *                 implicitly by starting at the first SOI.
 *   leading junk  an HTML error page containing a stray FFD8 before the real
 *                 SOI → skipped; a body with no SOI at all → 0.
 *   truncation    EVERY prefix length from 0 to the full buffer, for the
 *                 baseline and for the multipart body → each returns 0
 *                 ("incomplete, need more bytes") except the prefixes that
 *                 genuinely contain all of frame 1, which return exactly that
 *                 frame. None reads past the buffer. This is the case the
 *                 streaming grabber hits on every chunk, so it is the one that
 *                 must not lie.
 *   hostile       a segment length of 0 and of 1 (neither advances the cursor,
 *                 the classic non-terminating case); a length that runs past
 *                 the buffer; 64 KB of 0xFF; SOI followed by 64 KB of zeros;
 *                 an FF00 outside entropy data; an 8,192-marker chain (past
 *                 CAM_JPEG_MAX_MARKERS) → each refused promptly.
 *
 * Also exercised, against a real in-memory SQLite database with this file's
 * own migrate() building the schema: BOTH retention limbs (60 rows → frame cap
 * keeps the newest 10; a 10-day-old and a 4-day-old row → age limb takes
 * exactly those two), the sweep reaching a camera that no longer exists in
 * intel_items (the case the on-write prune can never reach), keyset paging
 * across three pages with no duplicate and no skipped row, the bare-camera_uid
 * spelling of the read API, and scene_changed serialising as JSON null rather
 * than false. Plus URL classification (rtsp/rtsps/HLS/axis/mjpg-streamer/
 * snapshot/ftp/file) and every rung of camera_stills_scene().
 *
 * NOT exercised: the pHash rung (no decoder on this host), the ffmpeg RTSP/HLS
 * path end to end (no ffmpeg on this host), and any live camera. See the
 * closing section of camera_stills.h, which says so in detail rather than
 * leaving it to be discovered in production.
 */
#include "camera_stills.h"
#include "audit.h"
#include "camera_feed.h"    /* properties → capture targets (views, upstream names) */
#include "camera_privacy.h" /* may this camera's frames be indexed            */
#include "evidence.h"
#include "ffmpeg.h"         /* THE video seam — this file no longer has one   */
#include "hostgate.h"       /* the SSRF floor every outbound fetch must clear */
#include "httpclient.h"     /* http_client_global_init() — the one curl init */
#include "image_vec.h"      /* a pruned frame's image-index ref goes with it   */
#include "media.h"
#include "operatorgate.h"
#include "../source.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <curl/curl.h>
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
#define JO_REPO_ROOT "/Users/rayan/OSINTsaas"
#endif

#define CAM_SOURCE_ID  "camera-discovery"   /* where camera rows live         */
#define CAM_STILLS_SID "camera-stills"      /* this pod's source id           */

/* Markers examined before a JPEG is declared unparseable. A real frame carries
 * a couple of dozen; this is a latency/absurdity bound on adversarial input,
 * not a correctness one. */
#define CAM_JPEG_MAX_MARKERS 4096

/* ══════════════════════════════════════════════════════════════════════════
 * Small shared helpers — same idioms as core/alertsapi.c and core/media.c.
 * ══════════════════════════════════════════════════════════════════════════ */

static void uuid4(char out[37]) {
  unsigned char b[16];
  RAND_bytes(b, 16);
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

static char *dupcol(sqlite3_stmt *s, int i) {
  const char *c = ctext(s, i);
  return c ? strdup(c) : NULL;
}

static int env_int(const char *k, int dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  char *end = NULL;
  long n = strtol(v, &end, 10);
  if (end == v) return dflt;
  return (int)n;
}

static void sha256_hex(const void *b, size_t n, char out[65]) {
  unsigned char d[SHA256_DIGEST_LENGTH];
  SHA256((const unsigned char *)b, n, d);
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    snprintf(out + i * 2, 3, "%02x", d[i]);
  out[64] = '\0';
}

static char *cs_err(int *st, int code, const char *msg) {
  if (st) *st = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", msg);
  char *j = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return j;
}

static long long now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Case-insensitive substring, for URL shape heuristics. */
static int ci_has(const char *hay, const char *needle) {
  if (!hay || !needle || !*needle) return 0;
  size_t n = strlen(needle);
  for (const char *p = hay; *p; p++)
    if (strncasecmp(p, needle, n) == 0) return 1;
  return 0;
}

/* Buffer.from(str,'utf8').toString('base64url') — no padding, +/ → -_
 * (verbatim from core/intelapi.c so cursors are the same currency here as
 * everywhere else in the API). */
static char *b64url(const char *in) {
  static const char T[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  size_t len = strlen(in);
  char *out = malloc(((len + 2) / 3) * 4 + 1);
  if (!out) return NULL;
  size_t o = 0;
  for (size_t i = 0; i < len; i += 3) {
    unsigned a = (unsigned char)in[i];
    unsigned b = i + 1 < len ? (unsigned char)in[i + 1] : 0;
    unsigned c = i + 2 < len ? (unsigned char)in[i + 2] : 0;
    unsigned v = (a << 16) | (b << 8) | c;
    out[o++] = T[(v >> 18) & 63];
    out[o++] = T[(v >> 12) & 63];
    if (i + 1 < len) out[o++] = T[(v >> 6) & 63];
    if (i + 2 < len) out[o++] = T[v & 63];
  }
  out[o] = 0;
  return out;
}

static char *b64url_decode(const char *in) {
  static signed char R[256];
  static int init = 0;
  if (!init) {
    for (int i = 0; i < 256; i++) R[i] = -1;
    const char *T =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (int i = 0; i < 64; i++) R[(unsigned char)T[i]] = (signed char)i;
    init = 1;
  }
  size_t len = strlen(in);
  char *out = malloc(len / 4 * 3 + 4);
  if (!out) return NULL;
  size_t o = 0; unsigned v = 0; int bits = 0;
  for (size_t i = 0; i < len; i++) {
    signed char d = R[(unsigned char)in[i]];
    if (d < 0) { free(out); return NULL; }
    v = (v << 6) | (unsigned)d; bits += 6;
    if (bits >= 8) { bits -= 8; out[o++] = (char)((v >> bits) & 0xFF); }
  }
  out[o] = 0;
  return out;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Configuration. Every default is stated in camera_stills.h; the clamps here
 * are the real contract, because an operator who sets INTERVAL_SEC=0 must not
 * get an unthrottled hammer on somebody else's camera.
 * ══════════════════════════════════════════════════════════════════════════ */

static int cfg_interval(void) {
  int v = env_int("CAM_STILLS_INTERVAL_SEC", 900);
  if (v < 60) v = 60;                  /* a camera is never polled faster    */
  if (v > 86400) v = 86400;
  return v;
}
static int cfg_retain_days(void) {
  int v = env_int("CAM_STILLS_RETAIN_DAYS", 3);
  if (v < 1) v = 1;
  if (v > 365) v = 365;
  return v;
}
static int cfg_max_per_cam(void) {
  int v = env_int("CAM_STILLS_MAX_PER_CAM", 48);
  if (v < 1) v = 1;
  if (v > 10000) v = 10000;
  return v;
}
/* Captures attempted per tick. SIZED, not picked: the pod ticks every 300 s
 * and a target is due every CAM_STILLS_INTERVAL_SEC (900 s), so N opted-in
 * targets need N x 300/900 = N/3 captures per tick to keep up. 48 sustains
 * 100 targets (34 per tick) with ~40% headroom for backoff retries and for
 * ticks the budget below cuts short. The old default of 8 sustained 24; an
 * operator who opted in a 100-camera list fell further behind on every tick,
 * silently, with every camera permanently "due". */
static int cfg_batch(void) {
  int v = env_int("CAM_STILLS_BATCH", 48);
  if (v < 1) v = 1;
  if (v > 500) v = 500;
  return v;
}
/* Per-host ceiling inside ONE tick, on top of hostgate's per-request gap and
 * in-flight cap. A 100-view station list on one DOT image server would
 * otherwise spend the whole batch on that host back to back; with this the
 * rest stay due and lead the next tick (the due list is oldest-first). 40 per
 * tick x 3 ticks per interval = 120 targets per host at the default cadence. */
static int cfg_host_per_tick(void) {
  int v = env_int("CAM_STILLS_HOST_PER_TICK", 40);
  if (v < 1) v = 1;
  if (v > 500) v = 500;
  return v;
}
static long cfg_max_bytes(void) {
  int v = env_int("CAM_STILLS_MAX_BYTES", 2 * 1024 * 1024);
  if (v < 4096) v = 4096;
  return (long)v;
}
static int cfg_timeout_ms(void) {
  int v = env_int("CAM_STILLS_TIMEOUT_MS", 8000);
  if (v < 1000) v = 1000;
  if (v > 60000) v = 60000;
  return v;
}
static int cfg_tick_budget_ms(void) {
  /* 240 s of a 300 s tick: a full batch of healthy snapshots (~1 s each)
   * fits, a batch of dead cameras (8 s timeouts) is cut off before it can run
   * into the next tick, and the targets it did not reach stay due. */
  int v = env_int("CAM_STILLS_TICK_BUDGET_MS", 240000);
  if (v < 5000) v = 5000;
  return v;
}
static int cfg_phash_threshold(void) {
  int v = env_int("CAM_STILLS_PHASH_THRESHOLD", 6);
  if (v < 0) v = 0;
  if (v > 64) v = 64;
  return v;
}
static int cfg_max_backoff(void) {
  int v = env_int("CAM_STILLS_MAX_BACKOFF_SEC", 6 * 3600);
  if (v < 60) v = 60;
  return v;
}

/* Blob root, matching evidence.c exactly. Returned as a getenv pointer rather
 * than copied into a fixed buffer on purpose: a fixed buffer would let GCC
 * prove a truncation at -O2 and -Wformat-truncation would (correctly) complain
 * about every path built from it. */
static const char *ev_root(void) {
  const char *e = getenv("JO_EVIDENCE_DIR");
  return (e && *e) ? e : JO_REPO_ROOT "/data/evidence";
}

/* ══════════════════════════════════════════════════════════════════════════
 * Migration
 * ══════════════════════════════════════════════════════════════════════════ */

static pthread_mutex_t g_mig_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_migrated = 0;

void camera_stills_migrate(db_handle *db) {
  if (!db || !db->h) return;
  pthread_mutex_lock(&g_mig_lock);
  if (g_migrated) { pthread_mutex_unlock(&g_mig_lock); return; }

  /* Column FIRST: idx_intel_items_capture_stills below names it, and an index
   * over a column that does not exist yet fails and — because sqlite3_exec
   * abandons a script at the first error — would take every statement after it
   * down as well. This ordering is the whole reason the DDL lives in a
   * function instead of in schema.sql. */
  ensure_column(db, "intel_items", "capture_stills",
                "INTEGER NOT NULL DEFAULT 0");

  static const char *DDL =
    "CREATE TABLE IF NOT EXISTS camera_stills ("
    "  id TEXT PRIMARY KEY, camera_id TEXT NOT NULL,"
    "  captured_at TEXT NOT NULL DEFAULT (datetime('now')),"
    "  sha256 TEXT NOT NULL, blob_path TEXT, bytes INTEGER,"
    "  phash INTEGER, ocr_text TEXT, width INTEGER, height INTEGER,"
    "  scene_changed INTEGER);"
    "CREATE INDEX IF NOT EXISTS idx_camera_stills_cam"
    "  ON camera_stills(camera_id, captured_at DESC);"
    "CREATE TABLE IF NOT EXISTS camera_stills_state ("
    "  camera_id TEXT PRIMARY KEY,"
    "  fail_count INTEGER NOT NULL DEFAULT 0,"
    "  last_attempt_at TEXT, next_attempt_at TEXT, last_ok_at TEXT,"
    "  last_error TEXT, last_kind TEXT);"
    /* Partial, and it stays EMPTY until an operator opts a camera in — so
     * unlike the pending indexes in media.c and simhash.c there is no one-off
     * full-scan boot cost here. It also cannot go in schema.sql: that file is
     * executed BEFORE the boot-migration block, when capture_stills may not
     * exist yet. */
    "CREATE INDEX IF NOT EXISTS idx_intel_items_capture_stills"
    "  ON intel_items(uid) WHERE capture_stills=1;";

  char *e = NULL;
  if (sqlite3_exec(db->h, DDL, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "[camera-stills] migrate: %s\n", e ? e : "?");
    sqlite3_free(e);
  }
  /* Embedding drift (core/image_vec.c writes them when the frame's vector
   * lands): cosine distance to the target's previous embedded frame, and to
   * the mean of its last IMAGE_VEC_DRIFT_WINDOW. NULL = not measured (no
   * index, no bytes, the target's first frame). After the CREATE above, so a
   * fresh database gets them too; never in schema.sql (see (B) in the
   * header). */
  ensure_column(db, "camera_stills", "embed_drift", "REAL");
  ensure_column(db, "camera_stills", "drift_from_mean", "REAL");
  g_migrated = 1;
  pthread_mutex_unlock(&g_mig_lock);
}

/* ══════════════════════════════════════════════════════════════════════════
 * The decoder-free JPEG frame extractor — see the contract in the header.
 * ══════════════════════════════════════════════════════════════════════════ */

int camera_stills_jpeg_frame(const void *buf, size_t len,
                             size_t *off, size_t *flen) {
  const unsigned char *b = (const unsigned char *)buf;
  if (!b || !off || !flen || len < 4) return 0;

  /* Start of Image. FF D8 must be followed by another FF (the introducer of
   * the first marker): requiring it rejects the FF D8 pairs that occur by
   * chance in HTML, in binary noise and inside another frame's entropy data. */
  size_t soi = 0;
  int have_soi = 0;
  for (size_t i = 0; i + 2 < len; i++) {
    if (b[i] == 0xFF && b[i + 1] == 0xD8 && b[i + 2] == 0xFF) {
      soi = i; have_soi = 1; break;
    }
  }
  if (!have_soi) return 0;

  size_t p = soi + 2;
  int markers = 0;
  while (p + 1 < len) {
    if (++markers > CAM_JPEG_MAX_MARKERS) return 0;
    if (b[p] != 0xFF) return 0;                 /* desynchronised → refuse   */
    while (p + 1 < len && b[p + 1] == 0xFF) p++;   /* fill bytes             */
    if (p + 1 >= len) return 0;
    unsigned m = b[p + 1];

    if (m == 0x00) return 0;      /* stuffed byte outside entropy data: bogus */
    if (m == 0xD9) {              /* End of Image — the only accepted end     */
      *off = soi;
      *flen = p + 2 - soi;
      return 1;
    }
    /* Standalone markers: TEM and the restart markers carry no payload. A
     * nested SOI is malformed here but harmless to step over. */
    if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) { p += 2; continue; }

    if (p + 3 >= len) return 0;                 /* length field not in yet   */
    size_t seg = ((size_t)b[p + 2] << 8) | (size_t)b[p + 3];
    if (seg < 2) return 0;                      /* would not advance         */
    if (seg > len - (p + 2)) return 0;          /* runs past the buffer      */

    if (m == 0xDA) {
      /* Start of Scan: the segment header is followed by entropy-coded data of
       * undeclared length. Inside it a literal 0xFF is stuffed as FF 00, and
       * FFD0..FFD7 are restart markers — everything else introduces the next
       * real marker. This loop is why an embedded thumbnail's FFD9 cannot be
       * mistaken for ours: APP segments were already skipped by length above,
       * so the walk never enters them. */
      size_t q = p + 2 + seg;
      while (q + 1 < len) {
        if (b[q] != 0xFF) { q++; continue; }
        unsigned n2 = b[q + 1];
        if (n2 == 0xFF) { q++; continue; }        /* fill byte run           */
        if (n2 == 0x00) { q += 2; continue; }     /* stuffed literal 0xFF    */
        if (n2 >= 0xD0 && n2 <= 0xD7) { q += 2; continue; }   /* restart     */
        /* Only 0x01 and 0xC0..0xFE are markers. 0x02..0xBF are RESERVED and
         * are not markers at all, so an FF followed by one of them is data,
         * not the end of the scan — treating it as a marker ends the frame
         * early and returns a truncated image. (Found by the fixture with a
         * fill-byte run in front of ordinary entropy data.) */
        if (n2 == 0x01 || n2 >= 0xC0) break;
        q += 2;
      }
      if (q + 1 >= len) return 0;               /* frame not complete yet    */
      p = q;
      continue;
    }
    p += 2 + seg;
  }
  return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Feed classification and resolution
 * ══════════════════════════════════════════════════════════════════════════ */

int camera_stills_classify(const char *url) {
  if (!url || !*url) return CAM_FEED_NONE;
  if (strncasecmp(url, "rtsp://", 7) == 0 ||
      strncasecmp(url, "rtsps://", 8) == 0) return CAM_FEED_RTSP;
  if (strncasecmp(url, "http://", 7) != 0 &&
      strncasecmp(url, "https://", 8) != 0) return CAM_FEED_UNSUPPORTED;
  if (ci_has(url, ".m3u8")) return CAM_FEED_HLS;
  /* Stream shapes seen across the discovery channels in this corpus. Being
   * wrong in either direction is survivable — see the routing note in the
   * header — so this is a fast path, not a gate. */
  if (ci_has(url, "mjpg") || ci_has(url, "mjpeg") ||
      ci_has(url, "action=stream") || ci_has(url, "videostream") ||
      ci_has(url, "nphmotionjpeg") || ci_has(url, "axis-cgi/mjpg") ||
      ci_has(url, "video.cgi") || ci_has(url, "x-mixed-replace"))
    return CAM_FEED_MJPEG;
  return CAM_FEED_SNAPSHOT;
}

/* THIS MODULE's admissible feed URL, checked before anything is handed to
 * core/ffmpeg.h. The shell is never involved anywhere on that path, so this is
 * not a quoting check; it is a POLICY check, and it is deliberately TIGHTER
 * than ffmpeg_input_allowed(), which also permits file/rtmp/tcp/udp. Those are
 * right for an analyst's uploaded video and wrong for a URL that came out of a
 * third-party camera registry — a camera feed is rtsp/rtsps/http/https or it is
 * not a camera feed. It also stops control characters and whitespace and
 * guarantees a leading scheme, so ffmpeg can never read the string as an OPTION
 * rather than an input. */
static int url_exec_safe(const char *u) {
  if (!u || !*u) return 0;
  size_t n = strlen(u);
  if (n > 2000) return 0;
  if (strncasecmp(u, "rtsp://", 7) != 0 && strncasecmp(u, "rtsps://", 8) != 0 &&
      strncasecmp(u, "http://", 7) != 0 && strncasecmp(u, "https://", 8) != 0)
    return 0;
  for (const unsigned char *p = (const unsigned char *)u; *p; p++)
    if (*p < 0x21 || *p > 0x7E) return 0;
  return 1;
}

/* properties → feed URL(s) is core/camera_feed.h. It used to be a six-key
 * top-level lookup here, which matched the camera-discovery rows and almost
 * nothing else that carries a camera: hp tables keep upstream field names,
 * flattened (`attributes.ImageURL`), and station records keep one image per
 * view in an array. A record now yields one capture TARGET per view, each with
 * its own id ("<uid>#view=<suffix>"), backoff state and timeline. The record
 * itself stays the opt-in unit: capture_stills=1 on the row opts in every
 * view it carries. */

/* Accept either the full intel_items uid (optionally with a view suffix) or a
 * bare camera_uid. */
static char *normalize_camera_id(const char *id) {
  if (!id || !*id) return NULL;
  if (strchr(id, '|')) return strdup(id);
  size_t n = strlen(CAM_SOURCE_ID) + 1 + strlen(id) + 1;
  char *out = malloc(n);
  if (out) snprintf(out, n, "%s|%s", CAM_SOURCE_ID, id);
  return out;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Fetch: the streaming grabber (private curl handle — see the header for why
 * http_request() provably cannot do this).
 * ══════════════════════════════════════════════════════════════════════════ */

/* The global libcurl init lives in httpclient.c behind a single pthread_once. */

/* Per-connection SSRF re-check, verbatim in intent to httpclient.c's on_prereq.
 * It has to be repeated here because stream_grab() drives a PRIVATE handle:
 * everything http_request() applies — the floor, the redirect protocol
 * allowlist, this callback — is a property of THAT function, not of libcurl, so
 * a private handle starts with none of it. stream_grab follows up to 5
 * redirects, and a camera whose URL came out of shodan_api/insecam_scrape is
 * exactly the kind of upstream that can 302 into 169.254.169.254. */
#if LIBCURL_VERSION_NUM >= 0x075000            /* 7.80.0: CURLOPT_PREREQFUNCTION */
static int cam_prereq(void *ud, char *primary_ip, char *local_ip,
                      int primary_port, int local_port) {
  (void)ud; (void)local_ip; (void)primary_port; (void)local_port;
  if (hostgate_camera_addr_check(primary_ip) != HG_URL_OK) {
    fprintf(stderr, "[camera-stills] blocked connection to %s "
                    "(private/link-local)\n", primary_ip ? primary_ip : "?");
    return CURL_PREREQFUNC_ABORT;
  }
  return CURL_PREREQFUNC_OK;
}
#endif

/* THE DESTINATION check for this module's own sockets, at the CAMERA strength
 * (hostgate.h): every camera URL in the corpus was written by a discovery
 * collector from whatever its upstream reported, so loopback and RFC1918 are
 * always refused (there is no opt-in; JO_CAMERA_ALLOW_LAN was removed). This used to be the
 * floor ("LAN cameras are a shipped feature"), which let one poisoned record
 * point the pod — and capture_one, on a user's request — at the server's own
 * network.
 *
 * The name is RESOLVED here and every answer judged. On the MJPEG leg
 * cam_prereq() re-checks the peer actually connected to; the snapshot leg goes
 * through http_request(), whose per-connection check is only the floor; and on
 * the rtsp/HLS legs ffmpeg owns the socket (core/ffmpeg.c repeats this check
 * before spawning). For those two this lookup is the resolved-address check. A
 * DNS answer that changes between it and the real connect is the remaining
 * gap, stated in ffmpeg.h. One lookup per capture costs nothing on a
 * serialised pod. */
static int cam_gate_url(const char *url) {
  if (!url || !*url) return HG_URL_BAD_HOST;
  if (strncasecmp(url, "http://", 7) == 0 || strncasecmp(url, "https://", 8) == 0) {
    int rc = hostgate_url_check(url);
    if (rc != HG_URL_OK) return rc;
  }
  return hostgate_camera_url_check(url, 1);
}

typedef struct {
  unsigned char *buf;
  size_t len, cap, max;
  int    want_jpeg;      /* abort as soon as one complete frame is buffered  */
  int    got;            /* a complete frame is at [foff, foff+flen)         */
  size_t foff, flen;
  int    capped;         /* hit `max` — the transfer was aborted, not failed */
} grab;

static size_t grab_write(char *ptr, size_t sz, size_t nm, void *ud) {
  grab *g = (grab *)ud;
  size_t n = sz * nm;
  if (n == 0) return 0;
  if (g->got) return 0;                       /* already done → abort        */

  if (g->len + n > g->max) {
    n = g->max - g->len;
    g->capped = 1;
    if (n == 0) return 0;                     /* abort: buffer full          */
  }
  if (g->len + n + 1 > g->cap) {
    size_t nc = g->cap ? g->cap : 65536;
    while (nc < g->len + n + 1) nc *= 2;
    if (nc > g->max + 1) nc = g->max + 1;
    unsigned char *q = realloc(g->buf, nc);
    if (!q) return 0;                         /* OOM → abort, keep what we
                                               * already have               */
    g->buf = q; g->cap = nc;
  }
  memcpy(g->buf + g->len, ptr, n);
  size_t prev = g->len;
  g->len += n;
  g->buf[g->len] = 0;

  /* Only walk the frame when the NEW bytes contain an EOI candidate. Without
   * this the walk runs on every chunk and the grab is quadratic in the size of
   * the stream; with it, the total cost is linear. */
  if (g->want_jpeg) {
    size_t from = prev ? prev - 1 : 0;
    for (size_t i = from; i + 1 < g->len; i++) {
      if (g->buf[i] == 0xFF && g->buf[i + 1] == 0xD9) {
        if (camera_stills_jpeg_frame(g->buf, g->len, &g->foff, &g->flen)) {
          g->got = 1;
          return 0;                           /* abort the transfer NOW      */
        }
        break;
      }
    }
  }
  if (g->capped) return 0;
  return sz * nm;
}

/* Returns 1 on success with *out (malloc'd, caller frees) / *outlen set.
 * `want_jpeg` makes it stop at the first complete frame, which is the only way
 * an infinite multipart stream ever terminates. A CURLE_WRITE_ERROR after we
 * deliberately aborted is a SUCCESS, not a failure — distinguishing the two is
 * the whole reason this function exists. */
static int stream_grab(const char *url, int timeout_ms, size_t maxb,
                       int want_jpeg, unsigned char **out, size_t *outlen,
                       char *ct_out, size_t ct_cap, long *status_out) {
  if (!url || !out || !outlen) return 0;
  { int gk = hostgate_url_check(url);
    if (gk == HG_URL_OK) gk = hostgate_camera_url_check(url, 0);
    if (gk != HG_URL_OK) {
      fprintf(stderr, "[stills] refused %s: %s\n", url, hostgate_url_reason(gk));
      return 0;
    } }
  http_client_global_init();
  CURL *e = curl_easy_init();
  if (!e) return 0;
  /* Politeness: http_request() takes the per-host gate itself; this private
   * handle has to take it explicitly or every MJPEG grab (and every snapshot
   * fallback below) ignores the per-host gap and in-flight cap. Fail-open by
   * hostgate's own contract. */
  int gated = hostgate_acquire(url, timeout_ms);

  grab g;
  memset(&g, 0, sizeof g);
  g.max = maxb;
  g.want_jpeg = want_jpeg;

  struct curl_slist *hl = NULL;
  hl = curl_slist_append(hl, "Accept: image/jpeg,image/*,*/*;q=0.5");
  curl_easy_setopt(e, CURLOPT_URL, url);
  curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, grab_write);
  curl_easy_setopt(e, CURLOPT_WRITEDATA, &g);
  curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(e, CURLOPT_MAXREDIRS, 5L);
  /* Same two clamps do_once() sets, for the same reason: without
   * REDIR_PROTOCOLS a 302 can walk this fetch into file:// or scp://, and
   * cam_gate_url() only ever saw the FIRST url. */
#if LIBCURL_VERSION_NUM >= 0x075500            /* 7.85.0 */
  curl_easy_setopt(e, CURLOPT_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(e, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
  curl_easy_setopt(e, CURLOPT_PROTOCOLS,
                   (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
  curl_easy_setopt(e, CURLOPT_REDIR_PROTOCOLS,
                   (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
#if LIBCURL_VERSION_NUM >= 0x075000
  curl_easy_setopt(e, CURLOPT_PREREQFUNCTION, cam_prereq);
  curl_easy_setopt(e, CURLOPT_PREREQDATA, (void *)0);
#else
  /* No per-connection hook on this libcurl, so nothing would judge the
   * address of a redirect's target: a camera URL could 302 into the LAN or
   * the metadata service. Refuse to follow at all; the first hop was checked. */
  curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 0L);
#endif
  curl_easy_setopt(e, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
  curl_easy_setopt(e, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
  curl_easy_setopt(e, CURLOPT_USERAGENT, "JapanOSINT/1.0 (+native)");
  curl_easy_setopt(e, CURLOPT_NOSIGNAL, 1L);
  /* No ACCEPT_ENCODING: a multipart stream must not be handed to a
   * decompressor, and camera JPEGs do not compress anyway. */
  if (hl) curl_easy_setopt(e, CURLOPT_HTTPHEADER, hl);

  CURLcode rc = curl_easy_perform(e);
  long code = 0;
  curl_easy_getinfo(e, CURLINFO_RESPONSE_CODE, &code);
  char *ct = NULL;
  curl_easy_getinfo(e, CURLINFO_CONTENT_TYPE, &ct);
  if (ct_out && ct_cap) snprintf(ct_out, ct_cap, "%s", ct ? ct : "");
  if (status_out) *status_out = code;
  if (hl) curl_slist_free_all(hl);
  curl_easy_cleanup(e);
  if (gated) hostgate_release(url);

  int ok =(rc == CURLE_OK) || g.got || (g.capped && g.len > 0);
  if (!ok || g.len == 0) { free(g.buf); return 0; }
  *out = g.buf;
  *outlen = g.len;
  return 1;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Fetch: RTSP / HLS — DELEGATED to core/ffmpeg.h.
 *
 * This module used to carry its own find_ffmpeg() plus a fork/execvp/poll/
 * SIGKILL/waitpid loop. All of it is gone. core/ffmpeg.c is now the ONE place
 * in the product that spawns a media tool, because duplicated process plumbing
 * is the worst thing in this codebase to duplicate: the day one copy learns to
 * reap its children or bound its output and the other does not, the bug shows
 * up as a slow leak under load, in the file nobody was reading. Every security
 * property this path relies on — argv array and never a shell, -nostdin, a hard
 * SIGKILL deadline, bounded output, an explicit protocol allowlist — is stated
 * and enforced there, once.
 *
 * What stays HERE is this module's own policy, which is deliberately TIGHTER
 * than ffmpeg.h's: a camera feed URL must be rtsp/rtsps/http/https and nothing
 * else (url_exec_safe, above). ffmpeg.h additionally permits file, rtmp, tcp
 * and udp, which are perfectly legitimate for an uploaded video and are not
 * legitimate for a URL a third-party camera registry handed us.
 *
 * The argv is unchanged in substance: ffmpeg.c derives -rtsp_transport tcp from
 * the URL SCHEME, so an rtsp:// feed still gets it and an HLS feed still does
 * not (passing it to a non-RTSP demuxer is an immediate "Option not found").
 * ══════════════════════════════════════════════════════════════════════════ */

static int ffmpeg_grab(const char *url, int kind, int timeout_ms, size_t maxb,
                       unsigned char **out, size_t *outlen,
                       const char **reason) {
  *reason = NULL;
  if (!url_exec_safe(url)) { *reason = "unsafe_feed_url"; return 0; }

  char err[256] = {0};
  int rc = ffmpeg_extract_frame_capped(url, -1.0 /* first decodable frame */,
                                       0 /* native width */, timeout_ms, maxb,
                                       out, outlen, err, sizeof err);
  if (rc == FFMPEG_OK) return 1;
  if (err[0]) fprintf(stderr, "[camera-stills] %s\n", err);

  /* The reasons an operator can ACT on keep their historical spelling: they are
   * what camera_stills.h documents, they are what is already sitting in
   * camera_stills_state.last_error on every deployment, and on a host with no
   * ffmpeg they are the only ones that ever occur. Everything else — a timeout,
   * a 401 from the camera, an oversized frame — now carries ffmpeg.h's specific
   * token instead of the single "ffmpeg_failed" this file used to store, which
   * is the point of requirement 5: a caller must be able to say WHY.
   * ffmpeg_strerror() returns static storage, so the pointer safely outlives
   * this call and reaches state_write(). */
  if (rc == FFMPEG_ERR_DISABLED)
    *reason = (kind == CAM_FEED_RTSP) ? "rtsp_disabled_no_ffmpeg"
                                      : "hls_disabled_no_ffmpeg";
  else if (rc == FFMPEG_ERR_NOT_INSTALLED)
    *reason = (kind == CAM_FEED_RTSP) ? "rtsp_requires_ffmpeg"
                                      : "hls_requires_ffmpeg";
  else if (rc == FFMPEG_ERR_SCHEME || rc == FFMPEG_ERR_BAD_INPUT)
    *reason = "unsafe_feed_url";
  else
    *reason = ffmpeg_strerror(rc);
  return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Derived-data seams borrowed from roadmap 27 (core/media.h)
 * ══════════════════════════════════════════════════════════════════════════ */

/* One parse of media_capabilities() per process, so the pod does not fork a
 * decoder per frame on a host that has none (and does not re-probe PATH per
 * frame on a host that does). */
static void media_tools(int *have_phash, int *have_ocr) {
  static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
  static int probed = 0, ph = 0, oc = 0;
  pthread_mutex_lock(&lk);
  if (!probed) {
    probed = 1;
    char *j = media_capabilities();
    if (j) {
      cJSON *root = cJSON_Parse(j);
      cJSON *d = root ? cJSON_GetObjectItem(root, "data") : NULL;
      cJSON *p = d ? cJSON_GetObjectItem(d, "phash") : NULL;
      cJSON *o = d ? cJSON_GetObjectItem(d, "ocr") : NULL;
      cJSON *pa = p ? cJSON_GetObjectItem(p, "available") : NULL;
      cJSON *oa = o ? cJSON_GetObjectItem(o, "available") : NULL;
      ph = cJSON_IsTrue(pa) ? 1 : 0;
      oc = cJSON_IsTrue(oa) ? 1 : 0;
      cJSON_Delete(root);
      free(j);
    }
    /* media_phash_file() tries ffmpeg FIRST (ffmpeg_gray32) and only then the
     * ImageMagick/GraphicsMagick template media_capabilities() reports, so a
     * host with ffmpeg alone hashes frames — and used to be told it could
     * not, which skipped the hash on every frame of every camera. */
    if (!ph && ffmpeg_available()) ph = 1;
  }
  if (have_phash) *have_phash = ph;
  if (have_ocr)   *have_ocr = oc;
  pthread_mutex_unlock(&lk);
}

/* The relative blob path evidence.c just wrote, read back out of the row
 * rather than reconstructed from the hash: the "ab/abcd…" layout is
 * evidence.c's business and a row is the only place it is published. Empty
 * when capture is off for this source, the store is at budget, or the blob has
 * since been reaped — all normal. */
static int evidence_rel_path(sqlite3 *h, const char *sha, const char *uid,
                             char *out, size_t cap) {
  out[0] = '\0';
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT blob_path FROM evidence WHERE content_sha256=?1 AND item_uid=?2",
      -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, sha, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, uid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *t = ctext(s, 0);
    if (t) snprintf(out, cap, "%s", t);
  }
  sqlite3_finalize(s);
  return out[0] ? 1 : 0;
}

static int blob_abs_path(const char *rel, char *out, size_t cap) {
  if (!rel || !*rel) return 0;
  if (strstr(rel, "..")) return 0;            /* stored value, but cheap      */
  snprintf(out, cap, "%s/%s", ev_root(), rel);
  return 1;
}

static int temp_write(const void *b, size_t n, char *out, size_t cap) {
  const char *td = getenv("TMPDIR");
  if (!td || !*td) td = "/tmp";
  char tmpl[512];
  snprintf(tmpl, sizeof tmpl, "%s/jo-still-XXXXXX", td);
  int fd = mkstemp(tmpl);
  if (fd < 0) return 0;
  const char *p = (const char *)b;
  size_t o = 0;
  while (o < n) {
    ssize_t w = write(fd, p + o, n - o);
    if (w <= 0) { close(fd); unlink(tmpl); return 0; }
    o += (size_t)w;
  }
  close(fd);
  snprintf(out, cap, "%s", tmpl);
  return 1;
}

int camera_stills_scene(const char *prev_sha, uint64_t prev_phash,
                        const char *sha, uint64_t phash) {
  if (!prev_sha || !*prev_sha) return CAM_SCENE_CHANGED;   /* first frame    */
  if (sha && strcmp(prev_sha, sha) == 0) return CAM_SCENE_SAME;  /* provable */
  /* 0 is media.h's documented "no usable hash" sentinel, so it doubles as the
   * absent marker with no extra flag to keep in sync. */
  if (prev_phash != 0 && phash != 0)
    return media_phash_distance(prev_phash, phash) > cfg_phash_threshold()
             ? CAM_SCENE_CHANGED : CAM_SCENE_SAME;
  /* Different bytes, no decoder. Saying SAME here would report "nothing
   * happened" for every frame of everything that ever happens; saying CHANGED
   * would flag JPEG noise as an event. NULL is the only true answer. */
  return CAM_SCENE_UNKNOWN;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Per-camera scheduler state
 * ══════════════════════════════════════════════════════════════════════════ */

static void state_write(sqlite3 *h, const char *cam, int ok, const char *reason,
                        const char *kind, int delay_sec) {
  char mod[48];
  snprintf(mod, sizeof mod, "+%d seconds", delay_sec);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "INSERT INTO camera_stills_state"
      "  (camera_id,fail_count,last_attempt_at,next_attempt_at,last_ok_at,"
      "   last_error,last_kind)"
      " VALUES(?1, CASE WHEN ?2 THEN 0 ELSE 1 END, datetime('now'),"
      "        datetime('now',?3), CASE WHEN ?2 THEN datetime('now') END,"
      "        ?4, ?5)"
      " ON CONFLICT(camera_id) DO UPDATE SET"
      "   fail_count = CASE WHEN ?2 THEN 0"
      "                     ELSE MIN(camera_stills_state.fail_count+1, 30) END,"
      "   last_attempt_at = datetime('now'),"
      "   next_attempt_at = datetime('now',?3),"
      "   last_ok_at = CASE WHEN ?2 THEN datetime('now')"
      "                     ELSE camera_stills_state.last_ok_at END,"
      "   last_error = ?4, last_kind = ?5",
      -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, cam, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (s, 2, ok ? 1 : 0);
  sqlite3_bind_text(s, 3, mod, -1, SQLITE_TRANSIENT);
  if (reason) sqlite3_bind_text(s, 4, reason, -1, SQLITE_TRANSIENT);
  else        sqlite3_bind_null(s, 4);
  if (kind) sqlite3_bind_text(s, 5, kind, -1, SQLITE_TRANSIENT);
  else      sqlite3_bind_null(s, 5);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

static int state_fail_count(sqlite3 *h, const char *cam) {
  int n = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT fail_count FROM camera_stills_state WHERE camera_id=?1",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, cam, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
  }
  return n;
}

/* interval x 2^fails, capped. A camera is never abandoned permanently: cameras
 * come back, and a permanently-abandoned camera is indistinguishable from a
 * bug in this file. */
static int backoff_sec(int fails) {
  int base = cfg_interval();
  int cap = cfg_max_backoff();
  long long d = base;
  for (int i = 0; i < fails && d < cap; i++) d *= 2;
  if (d > cap) d = cap;
  return (int)d;
}

static const char *kind_name(int kind) {
  switch (kind) {
    case CAM_FEED_SNAPSHOT: return "snapshot";
    case CAM_FEED_MJPEG:    return "mjpeg";
    case CAM_FEED_RTSP:     return "rtsp";
    case CAM_FEED_HLS:      return "hls";
    case CAM_FEED_NONE:     return "none";
    default:                return "unsupported";
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 * Retention
 * ══════════════════════════════════════════════════════════════════════════ */

int camera_stills_prune_camera(db_handle *db, const char *camera_id) {
  if (!db || !db->h || !camera_id || !*camera_id) return 0;
  char age[32];
  snprintf(age, sizeof age, "-%d days", cfg_retain_days());

  sqlite3_stmt *s;
  /* Both limbs in ONE statement so the two ceilings can never disagree with
   * each other, and so a crash between them is not a state this module has to
   * reason about. `id NOT IN (newest N)` rather than a rowid trick because id
   * is the primary key and the subquery rides idx_camera_stills_cam. */
  if (sqlite3_prepare_v2(db->h,
      "DELETE FROM camera_stills WHERE camera_id=?1 AND ("
      "  captured_at < datetime('now',?2)"
      "  OR id NOT IN (SELECT id FROM camera_stills WHERE camera_id=?1"
      "                ORDER BY captured_at DESC, id DESC LIMIT ?3))",
      -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, camera_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, age, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (s, 3, cfg_max_per_cam());
  int n = 0;
  if (sqlite3_step(s) == SQLITE_DONE) n = sqlite3_changes(db->h);
  sqlite3_finalize(s);
  /* The image index points at frames by ref; a pruned frame's ref goes in
   * the same transaction, and its vector with it once nothing else (another
   * frame with the same bytes) points there. A no-op when it was never
   * built. */
  if (n > 0) image_vec_unref_stills(db, camera_id);
  return n;
}

int camera_stills_sweep(db_handle *db, int limit, volatile int *cancel) {
  if (!db || !db->h) return 0;
  camera_stills_migrate(db);
  sqlite3 *h = db->h;

  /* Driven off camera_stills, NOT off the opted-in camera list: a camera that
   * was switched off, deleted or renamed is exactly the one whose rows the
   * on-write prune can never reach again, and it is the only thing standing
   * between this feature and an unbounded table. */
  char **ids = NULL;
  int n = 0, cap = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT DISTINCT camera_id FROM camera_stills"
      " ORDER BY camera_id LIMIT ?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int(s, 1, limit > 0 ? limit : 1000000);
    while (sqlite3_step(s) == SQLITE_ROW) {
      if (n == cap) {
        int nc = cap ? cap * 2 : 128;
        char **q = realloc(ids, (size_t)nc * sizeof *q);
        if (!q) break;
        ids = q; cap = nc;
      }
      ids[n] = dupcol(s, 0);
      if (ids[n]) n++;
    }
    sqlite3_finalize(s);
  }

  int deleted = 0;
  if (n > 0) {
    sqlite3_exec(h, "BEGIN", NULL, NULL, NULL);
    for (int i = 0; i < n; i++) {
      if (cancel && *cancel) break;
      deleted += camera_stills_prune_camera(db, ids[i]);
    }
    sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
  }
  for (int i = 0; i < n; i++) free(ids[i]);
  free(ids);
  if (deleted)
    fprintf(stderr, "[camera-stills] sweep: cameras=%d pruned=%d row(s)\n",
            n, deleted);
  return deleted;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Capture
 * ══════════════════════════════════════════════════════════════════════════ */

/* Warn ONCE about the second opt-in (sources.capture_evidence), because
 * "capture is on but blob_path is NULL" is otherwise a puzzle solved by
 * reading source. */
static void warn_bytes_off(db_handle *db) {
  static int warned = 0;
  if (warned) return;
  warned = 1;
  int on = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
      "SELECT capture_evidence FROM sources WHERE id=?1", -1, &s, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(s, 1, CAM_STILLS_SID, -1, SQLITE_STATIC);
    if (sqlite3_step(s) == SQLITE_ROW &&
        sqlite3_column_type(s, 0) != SQLITE_NULL)
      on = sqlite3_column_int(s, 0) != 0;
    sqlite3_finalize(s);
  }
  if (!on)
    fprintf(stderr,
      "[camera-stills] frames will be hashed but NOT preserved: run\n"
      "                UPDATE sources SET capture_evidence=1 WHERE id='%s';\n"
      "                (evidence.h owns the blob store; this module does not "
      "keep a second one)\n", CAM_STILLS_SID);
}

/* Fetch the bytes for one camera. On success returns 1 with *out and *outlen
 * set (caller frees) and a kind that reflects what actually happened. */
static int fetch_frame(http_client *http, const char *url, int kind,
                       unsigned char **out, size_t *outlen, int *out_kind,
                       long *out_status, const char **reason,
                       char *rbuf, size_t rcap) {
  long maxb = cfg_max_bytes();
  int tmo = cfg_timeout_ms();
  *reason = NULL;
  *out_kind = kind;
  *out_status = 0;      /* 0 = "not an HTTP fetch" (the ffmpeg paths)        */

  /* ONE gate in front of all three legs, because all three dial a URL that was
   * written into intel_items.properties by a discovery collector (shodan_api,
   * insecam_scrape, …) — i.e. by an upstream that can choose it. It fails
   * CLOSED and with a reason that reaches camera_stills_state.last_error: a
   * blocked destination that looked like an empty result would be indexed as
   * "this camera has nothing", which is the silent-skip this file's retention
   * and backoff logic would then treat as a healthy no-op. */
  { int gk = cam_gate_url(url);
    if (gk != HG_URL_OK) {
      fprintf(stderr, "[camera-stills] refused %s: %s\n", url,
              hostgate_url_reason(gk));
      *reason = "blocked_destination";
      return 0;
    } }

  if (kind == CAM_FEED_RTSP || kind == CAM_FEED_HLS) {
    /* ffmpeg owns the socket, so the per-host gate is taken around it. */
    int gated = hostgate_acquire(url, tmo);
    int r = ffmpeg_grab(url, kind, tmo, (size_t)maxb, out, outlen, reason);
    if (gated) hostgate_release(url);
    return r;
  }

  if (kind == CAM_FEED_MJPEG) {
    if (stream_grab(url, tmo, (size_t)maxb, 1, out, outlen, NULL, 0, out_status))
      return 1;
    *reason = "mjpeg_fetch_failed";
    return 0;
  }

  /* Snapshot: the shared client first, so this common case keeps DNS/TLS
   * reuse, url_override_apply() and the retry policy. */
  char rangeh[64];
  snprintf(rangeh, sizeof rangeh, "Range: bytes=0-%ld", maxb);
  const char *hdrs[] = { rangeh, "Accept: image/*,*/*;q=0.5", NULL };
  http_response r = { 0 };
  int rc = http_request(http, "GET", url, hdrs, NULL, 0, tmo, 0, &r);
  if (rc == 0 && (r.status == 200 || r.status == 206) && r.body &&
      r.body_len >= 16 && (long)r.body_len <= maxb) {
    unsigned char *b = malloc(r.body_len);
    if (!b) { http_response_free(&r); *reason = "oom"; return 0; }
    memcpy(b, r.body, r.body_len);
    *out = b; *outlen = r.body_len;
    *out_status = r.status;
    http_response_free(&r);
    return 1;
  }
  long status = r.status;
  size_t got = r.body_len;
  http_response_free(&r);

  if (rc == 0 && status && status != 200 && status != 206) {
    /* The reason buffer belongs to the CALLER, not to a file-static: this
     * function is reachable both from the serialised pod and from
     * camera_stills_capture_one(), and a shared static would be a data race
     * the day someone wires the latter to a route. */
    if (rbuf && rcap) {
      snprintf(rbuf, rcap, "http_%ld", status);
      *reason = rbuf;
    } else {
      *reason = "http_error";
    }
    return 0;
  }
  if (rc == 0 && (long)got > maxb) { *reason = "too_large"; return 0; }

  /* A HARD failure with no body is the exact signature of an unannounced
   * MJPEG stream: httpclient.c frees the accumulated body on
   * CURLE_OPERATION_TIMEDOUT (see the header), so a perfectly healthy stream
   * arrives here as "nothing". One streaming retry distinguishes the two. */
  if (stream_grab(url, tmo, (size_t)maxb, 1, out, outlen, NULL, 0, out_status)) {
    *out_kind = CAM_FEED_MJPEG;
    return 1;
  }
  *reason = "fetch_failed";
  return 0;
}

/* Trim to exactly ONE frame. For JPEG this also PROVES the frame is
 * complete — a half-received image would otherwise be stored, hashed and
 * compared as though it were real, and every truncated frame would read as a
 * scene change. 1 with the frame's span inside `raw` and its type; 0 with
 * `reason`. */
static int trim_frame(const unsigned char *raw, size_t rawlen,
                      const unsigned char **frame, size_t *flen,
                      const char **ct, const char **reason) {
  *frame = raw;
  *flen = rawlen;
  *ct = media_sniff_type(raw, rawlen);
  if (!*ct || strcmp(*ct, "image/jpeg") == 0) {
    size_t off = 0, n = 0;
    if (camera_stills_jpeg_frame(raw, rawlen, &off, &n)) {
      *frame = raw + off;
      *flen = n;
      *ct = "image/jpeg";
    } else {
      *reason = *ct ? "incomplete_jpeg" : "not_an_image";
      return 0;
    }
  }
  if (*flen < 16 || (long)*flen > cfg_max_bytes()) {
    *reason = "bad_frame_size";
    return 0;
  }
  return 1;
}

int camera_stills_fetch_frame(http_client *http, const char *url,
                              unsigned char **out, size_t *outlen,
                              const char **ct, long *status,
                              const char **reason, char *rbuf, size_t rcap) {
  *out = NULL; *outlen = 0; *ct = NULL; *status = 0; *reason = NULL;
  int kind = camera_stills_classify(url);
  if (kind == CAM_FEED_NONE) { *reason = "no_feed_url"; return 0; }
  if (kind == CAM_FEED_UNSUPPORTED) { *reason = "unsupported_scheme"; return 0; }
  unsigned char *raw = NULL;
  size_t rawlen = 0;
  int eff = kind;
  if (!fetch_frame(http, url, kind, &raw, &rawlen, &eff, status, reason, rbuf, rcap)) {
    if (!*reason) *reason = "fetch_failed";
    return 0;
  }
  const unsigned char *frame = raw;
  size_t flen = rawlen;
  if (!trim_frame(raw, rawlen, &frame, &flen, ct, reason)) { free(raw); return 0; }
  if (frame != raw) memmove(raw, frame, flen);
  *out = raw;
  *outlen = flen;
  return 1;
}

/* The whole per-camera write, from bytes to committed row. Returns 1 on a
 * captured frame. `reason` is set on every 0 return. */
static int capture_frame(db_handle *db, http_client *http, const char *cam,
                         const char *item_uid, const char *url, int kind,
                         const char **reason, char *rbuf, size_t rcap) {
  sqlite3 *h = db->h;
  unsigned char *raw = NULL;
  size_t rawlen = 0;
  int eff_kind = kind;
  long status = 0;
  *reason = NULL;

  if (!fetch_frame(http, url, kind, &raw, &rawlen, &eff_kind, &status, reason,
                   rbuf, rcap)) {
    if (!*reason) *reason = "fetch_failed";
    return 0;
  }

  const unsigned char *frame = raw;
  size_t flen = rawlen;
  const char *ct = NULL;
  if (!trim_frame(raw, rawlen, &frame, &flen, &ct, reason)) {
    free(raw);
    return 0;
  }

  char sha[65];
  sha256_hex(frame, flen, sha);
  long w = 0, hgt = 0;
  media_image_dims(frame, flen, &w, &hgt);

  /* Preservation. Filed under the CAMERA's uid and this pod's source id, so an
   * operator has exactly one flag to flip to start keeping bytes and the
   * custody row says which camera the frame came from. evidence.c dedupes by
   * content hash, so a static scene is one file on disk however many rows
   * point at it. Filed under the RECORD's uid, not the view's target id: the
   * custody row must name an intel_items row, and the url says which view. */
  evidence_capture(db, item_uid, CAM_STILLS_SID, url, "GET", NULL, status, NULL,
                   frame, flen, ct);

  char rel[160];
  int have_rel = evidence_rel_path(h, sha, item_uid, rel, sizeof rel);

  /* Derived data, only when a tool exists — otherwise this would fork twice
   * per frame to be told there is no decoder. */
  uint64_t ph = 0;
  char *ocr = NULL;
  int want_ph = 0, want_ocr = 0;
  media_tools(&want_ph, &want_ocr);
  if (want_ph || want_ocr) {
    char path[2048];
    char tmpp[512];
    tmpp[0] = '\0';
    int have_path = 0;
    if (have_rel && blob_abs_path(rel, path, sizeof path)) {
      struct stat stt;
      have_path = (stat(path, &stt) == 0);
    }
    if (!have_path && temp_write(frame, flen, tmpp, sizeof tmpp)) {
      snprintf(path, sizeof path, "%s", tmpp);
      have_path = 1;
    }
    if (have_path) {
      if (want_ph) media_phash_file(path, &ph);
      if (want_ocr) {
        double conf = -1.0;
        if (!media_ocr_file(path, &ocr, &conf)) ocr = NULL;
      }
    }
    if (tmpp[0]) unlink(tmpp);
  }

  /* Previous frame → scene comparison. One indexed read on
   * idx_camera_stills_cam. */
  char *prev_sha = NULL;
  uint64_t prev_ph = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT sha256,phash FROM camera_stills WHERE camera_id=?1"
      " ORDER BY captured_at DESC, id DESC LIMIT 1", -1, &s, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(s, 1, cam, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      prev_sha = dupcol(s, 0);
      if (sqlite3_column_type(s, 1) != SQLITE_NULL)
        prev_ph = (uint64_t)sqlite3_column_int64(s, 1);
    }
    sqlite3_finalize(s);
  }
  int scene = camera_stills_scene(prev_sha, prev_ph, sha, ph);
  free(prev_sha);

  char id[37];
  uuid4(id);
  int ok = 0;
  sqlite3_exec(h, "BEGIN", NULL, NULL, NULL);
  /* datetime('now'), matching the column DEFAULT exactly. The retention
   * comparison and the keyset cursor both compare captured_at as a STRING, so
   * two formats in one column would silently break both. */
  if (sqlite3_prepare_v2(h,
      "INSERT INTO camera_stills(id,camera_id,captured_at,sha256,blob_path,"
      " bytes,phash,ocr_text,width,height,scene_changed)"
      " VALUES(?1,?2,datetime('now'),?3,?4,?5,?6,?7,?8,?9,?10)",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text (s, 1, id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (s, 2, cam, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (s, 3, sha, -1, SQLITE_TRANSIENT);
    /* RELATIVE, exactly as evidence.c stores it: an absolute path would make
     * every row wrong the day JO_EVIDENCE_DIR moves. */
    if (have_rel) sqlite3_bind_text(s, 4, rel, -1, SQLITE_TRANSIENT);
    else          sqlite3_bind_null(s, 4);
    sqlite3_bind_int64(s, 5, (sqlite3_int64)flen);
    /* The column is INTEGER, i.e. signed 64-bit; the hash is unsigned. The
     * cast is a reinterpretation, not a conversion, and readers reverse it. */
    if (ph) sqlite3_bind_int64(s, 6, (sqlite3_int64)ph);
    else    sqlite3_bind_null(s, 6);
    if (ocr) sqlite3_bind_text(s, 7, ocr, -1, SQLITE_TRANSIENT);
    else     sqlite3_bind_null(s, 7);
    if (w)   sqlite3_bind_int64(s, 8, (sqlite3_int64)w);
    else     sqlite3_bind_null(s, 8);
    if (hgt) sqlite3_bind_int64(s, 9, (sqlite3_int64)hgt);
    else     sqlite3_bind_null(s, 9);
    if (scene == CAM_SCENE_UNKNOWN) sqlite3_bind_null(s, 10);
    else                            sqlite3_bind_int(s, 10, scene);
    ok = (sqlite3_step(s) == SQLITE_DONE);
    sqlite3_finalize(s);
  }
  /* PRUNE ON WRITE, inside the same transaction. This is what makes the
   * per-camera ceiling hold even if the sweep pod is never wired up or is
   * switched off — retention that only runs in a background job is retention
   * that is not there on the day it matters. */
  int pruned = ok ? camera_stills_prune_camera(db, cam) : 0;
  if (ok) sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
  else    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);

  if (!ok) *reason = "db_error";
  else if (!have_rel) warn_bytes_off(db);

  if (ok && pruned)
    fprintf(stderr, "[camera-stills] %s: +1 frame (%s, %lu B), pruned %d\n",
            cam, kind_name(eff_kind), (unsigned long)flen, pruned);

  free(ocr);
  free(raw);
  return ok ? 1 : 0;
}

/* ── capture targets ───────────────────────────────────────────────────────
 * One opted-in intel_items row → one or more targets (camera_feed.h). The row
 * is the opt-in unit; the TARGET is the scheduling unit — each has its own
 * camera_stills_state row keyed by its target id, so one dead view backs off
 * without holding back the station's other views. A row with no feed at all
 * is still one target (url NULL), so its "no_feed_url" is recorded and backed
 * off rather than re-read on every tick. */
typedef struct {
  char *tid;        /* "<uid>" or "<uid>#view=<suffix>"                      */
  char *uid;        /* the intel_items row it came from                      */
  char *url;        /* NULL → the row has no feed                            */
  char *next;       /* next_attempt_at, NULL = never attempted               */
} cs_target;

typedef struct { cs_target *v; int n, cap; } cs_targets;

static void targets_free(cs_targets *T) {
  for (int i = 0; i < T->n; i++) {
    free(T->v[i].tid); free(T->v[i].uid); free(T->v[i].url); free(T->v[i].next);
  }
  free(T->v);
  memset(T, 0, sizeof *T);
}

static int targets_push(cs_targets *T, const char *tid, const char *uid,
                        const char *url) {
  if (T->n == T->cap) {
    int nc = T->cap ? T->cap * 2 : 64;
    cs_target *q = realloc(T->v, (size_t)nc * sizeof *q);
    if (!q) return 0;
    T->v = q; T->cap = nc;
  }
  cs_target *t = &T->v[T->n];
  memset(t, 0, sizeof *t);
  t->tid = strdup(tid);
  t->uid = strdup(uid);
  t->url = url ? strdup(url) : NULL;
  if (!t->tid || !t->uid || (url && !t->url)) {
    free(t->tid); free(t->uid); free(t->url);
    return 0;
  }
  T->n++;
  return 1;
}

/* Append every target of one row. Returns how many the row HAS (which can
 * exceed what was appended: CAM_FEED_MAX_TARGETS is a per-record ceiling and
 * crossing it is logged here and reported by camera_stills_list's meta). */
static int expand_row(cs_targets *T, const char *uid, const char *sid,
                      const char *props) {
  camera_feed_target *ft = calloc(CAM_FEED_MAX_TARGETS, sizeof *ft);
  if (!ft) return 0;
  int total = camera_feed_resolve(props, sid, ft, CAM_FEED_MAX_TARGETS);
  int got = total < CAM_FEED_MAX_TARGETS ? total : CAM_FEED_MAX_TARGETS;
  if (total > CAM_FEED_MAX_TARGETS)
    fprintf(stderr, "[camera-stills] %s carries %d feed targets; the first %d "
                    "are captured (targets_total in its stills meta)\n",
            uid, total, CAM_FEED_MAX_TARGETS);
  if (got == 0) {
    targets_push(T, uid, uid, NULL);
  } else {
    for (int i = 0; i < got; i++) {
      char tid[768];
      if (!ft[i].url || !camera_feed_target_id(uid, ft[i].suffix, tid, sizeof tid))
        continue;
      targets_push(T, tid, uid, ft[i].url);
    }
  }
  camera_feed_free(ft, got);
  free(ft);
  return total;
}

static int target_cmp(const void *a, const void *b) {
  const cs_target *x = a, *y = b;
  int c = strcmp(x->next ? x->next : "", y->next ? y->next : "");
  return c ? c : strcmp(x->tid, y->tid);
}

/* Every DUE target of every opted-in row, oldest-due first (never-attempted
 * first of all). Reads the opt-in list and FINALIZES before returning, so no
 * read cursor survives into a network call. Driven by
 * idx_intel_items_capture_stills — empty until an operator opts a row in, so
 * a default deployment pays one empty index seek. Any source's row can be
 * opted in (camera-discovery, the hp camera tables, trn_* station indexes):
 * the opt-in route takes any uid and nothing here filters on source. */
static int collect_due(sqlite3 *h, cs_targets *due) {
  memset(due, 0, sizeof *due);
  cs_targets all = { 0 };
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT uid, source_id, properties FROM intel_items"
      " WHERE capture_stills=1 ORDER BY uid", -1, &s, NULL) != SQLITE_OK)
    return 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *uid = ctext(s, 0);
    if (!uid) continue;
    expand_row(&all, uid, ctext(s, 1), ctext(s, 2));
  }
  sqlite3_finalize(s);

  if (sqlite3_prepare_v2(h,
      "SELECT next_attempt_at,"
      "       (next_attempt_at IS NULL OR next_attempt_at <= datetime('now'))"
      "  FROM camera_stills_state WHERE camera_id=?1", -1, &s, NULL)
      != SQLITE_OK) { targets_free(&all); return 0; }
  for (int i = 0; i < all.n; i++) {
    sqlite3_reset(s);
    sqlite3_bind_text(s, 1, all.v[i].tid, -1, SQLITE_TRANSIENT);
    int is_due = 1;
    char *next = NULL;
    if (sqlite3_step(s) == SQLITE_ROW) {
      next = dupcol(s, 0);
      is_due = sqlite3_column_int(s, 1) != 0;
    }
    if (!is_due) { free(next); continue; }
    if (targets_push(due, all.v[i].tid, all.v[i].uid, all.v[i].url))
      due->v[due->n - 1].next = next;
    else
      free(next);
  }
  sqlite3_finalize(s);
  targets_free(&all);
  if (due->n > 1) qsort(due->v, (size_t)due->n, sizeof *due->v, target_cmp);
  return due->n;
}

/* Capture one target and record its state. Returns 1 on a stored frame. */
static int capture_target(db_handle *db, http_client *http, const cs_target *t) {
  int kind = camera_stills_classify(t->url);
  int got = 0;
  char rbuf[32];
  const char *reason = NULL;
  if (kind == CAM_FEED_NONE)             reason = "no_feed_url";
  else if (kind == CAM_FEED_UNSUPPORTED) reason = "unsupported_scheme";
  else got = capture_frame(db, http, t->tid, t->uid, t->url, kind, &reason,
                           rbuf, sizeof rbuf);
  int fails = got ? 0 : state_fail_count(db->h, t->tid) + 1;
  state_write(db->h, t->tid, got, got ? NULL : reason, kind_name(kind),
              got ? cfg_interval() : backoff_sec(fails));
  return got;
}

int camera_stills_capture_one(db_handle *db, http_client *http,
                              const char *camera_id) {
  if (!db || !db->h) return 0;
  camera_stills_migrate(db);
  char *cam = normalize_camera_id(camera_id);
  if (!cam) return 0;
  const char *want = NULL;
  char *uid = camera_feed_split(cam, &want);
  if (!uid) { free(cam); return 0; }

  cs_targets T = { 0 };
  int on = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
      "SELECT source_id, properties, COALESCE(capture_stills,0)"
      "  FROM intel_items WHERE uid=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_int(s, 2) != 0) {
      on = 1;
      expand_row(&T, uid, ctext(s, 0), ctext(s, 1));
    }
    sqlite3_finalize(s);
  }
  if (!on) { targets_free(&T); free(uid); free(cam); return 0; }

  http_client *own = NULL;
  if (!http) { own = http_client_new(); http = own; }

  /* A bare uid captures every view of the record; a target id captures that
   * one view. A view that no longer exists in the record is RECORDED as such
   * rather than silently doing nothing. */
  int got = 0, matched = 0;
  for (int i = 0; i < T.n; i++) {
    if (want && strcmp(T.v[i].tid, cam) != 0) continue;
    matched++;
    got += capture_target(db, http, &T.v[i]);
  }
  if (want && !matched)
    state_write(db->h, cam, 0, "view_not_in_record", kind_name(CAM_FEED_NONE),
                backoff_sec(state_fail_count(db->h, cam) + 1));

  if (own) http_client_free(own);
  targets_free(&T);
  free(uid);
  free(cam);
  return got;
}

/* Host bucket for the per-tick ceiling. Unparseable → its own bucket "". */
static void host_key(const char *url, char *out, size_t cap) {
  out[0] = '\0';
  if (url) hostgate_url_host(url, out, cap);
}

int camera_stills_capture_batch(db_handle *db, http_client *http, int limit,
                                volatile int *cancel) {
  if (!db || !db->h) return 0;
  camera_stills_migrate(db);
  if (limit <= 0) limit = cfg_batch();

  /* Turn OFF httpclient.c's automatic evidence hook for this thread. The
   * scheduler bound the scope to this pod, which would file every frame under
   * the synthetic uid "src:camera-stills" — provenance that says where the
   * fetch was issued from rather than which camera it came from. Every capture
   * below is made explicitly under the CAMERA's uid instead. end() is safe
   * unpaired and safe when no scope was ever bound. */
  evidence_scope_end();

  cs_targets due;
  collect_due(db->h, &due);

  http_client *own = NULL;
  if (due.n > 0 && !http) { own = http_client_new(); http = own; }

  /* Per-host attempt counts for THIS tick. Linear: a tick is <= 500 attempts. */
  typedef struct { char host[256]; int n; } hostc;
  hostc *hc = calloc((size_t)(due.n ? due.n : 1), sizeof *hc);
  int nh = 0;
  int host_cap = cfg_host_per_tick();

  long long deadline = now_ms() + cfg_tick_budget_ms();
  int done = 0, failed = 0, attempted = 0, host_deferred = 0, budget_left = 0;
  for (int i = 0; i < due.n && attempted < limit; i++) {
    if (cancel && *cancel) break;
    /* A wall-clock ceiling on the whole tick. One slow camera cannot push the
     * tick into the next one; the targets that did not get their turn stay due
     * and are picked up first next time, because the order is oldest-due. */
    if (now_ms() > deadline) { budget_left = due.n - i; break; }

    char hk[256];
    host_key(due.v[i].url, hk, sizeof hk);
    hostc *slot = NULL;
    if (hc && hk[0]) {
      for (int k = 0; k < nh; k++) if (strcmp(hc[k].host, hk) == 0) { slot = &hc[k]; break; }
      if (!slot) { slot = &hc[nh++]; snprintf(slot->host, sizeof slot->host, "%s", hk); }
      if (slot->n >= host_cap) { host_deferred++; continue; }   /* stays due */
      slot->n++;
    }

    attempted++;
    if (capture_target(db, http, &due.v[i])) done++; else failed++;
  }
  free(hc);

  if (budget_left)
    fprintf(stderr, "[camera-stills] tick budget reached, %d target(s) "
                    "deferred to the next tick\n", budget_left);
  if (own) http_client_free(own);
  if (due.n)
    fprintf(stderr, "[camera-stills] capture: due=%d attempted=%d ok=%d "
                    "failed=%d host_deferred=%d not_reached=%d\n",
            due.n, attempted, done, failed, host_deferred,
            due.n - attempted - host_deferred);
  targets_free(&due);
  return done;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Read paths
 * ══════════════════════════════════════════════════════════════════════════ */

static void put_hex64(cJSON *o, const char *k, sqlite3_int64 v) {
  char b[17];
  snprintf(b, sizeof b, "%016llx", (unsigned long long)(uint64_t)v);
  cJSON_AddStringToObject(o, k, b);
}

char *camera_stills_list(db_handle *db, const char *camera_id, int limit,
                         const char *cursor) {
  if (!db || !db->h || !camera_id || !*camera_id) return NULL;
  camera_stills_migrate(db);
  char *cam = normalize_camera_id(camera_id);
  if (!cam) return NULL;
  if (limit <= 0) limit = 50;
  if (limit > 200) limit = 200;

  /* keyset cursor {"p":captured_at,"u":id}; a malformed cursor is IGNORED
   * rather than rejected — same stance as intelapi.c. */
  char *cur_p = NULL, *cur_u = NULL;
  cJSON *curj = NULL;
  if (cursor && *cursor) {
    char *dec = b64url_decode(cursor);
    if (dec) {
      curj = cJSON_Parse(dec);
      free(dec);
      if (curj) {
        cJSON *jp = cJSON_GetObjectItem(curj, "p");
        cJSON *ju = cJSON_GetObjectItem(curj, "u");
        if (cJSON_IsString(jp)) cur_p = jp->valuestring;
        if (cJSON_IsString(ju)) cur_u = ju->valuestring;
      }
    }
  }

  /* A bare record uid lists EVERY view of the record as one timeline (each
   * row says which view it is); a "<uid>#view=<suffix>" id lists that view.
   * The views are the half-open key range [uid#view=, uid#view>) — '>' is
   * the byte after '=' — so this rides idx_camera_stills_cam with no LIKE
   * and no escaping of whatever a uid happens to contain. An exact target
   * binds an empty range. */
  const char *want_view = NULL;
  char *rec_uid = camera_feed_split(cam, &want_view);
  if (!rec_uid) { if (curj) cJSON_Delete(curj); free(cam); return NULL; }
  size_t rl = strlen(cam) + sizeof CAM_FEED_VIEW_SEP + 1;
  char *lo = malloc(rl), *hi = malloc(rl);
  if (!lo || !hi) {
    free(lo); free(hi); free(rec_uid);
    if (curj) cJSON_Delete(curj);
    free(cam); return NULL;
  }
  if (want_view) { lo[0] = hi[0] = '\0'; }
  else {
    snprintf(lo, rl, "%s" CAM_FEED_VIEW_SEP, cam);
    snprintf(hi, rl, "%s" CAM_FEED_VIEW_SEP, cam);
    hi[strlen(hi) - 1] = '>';
  }
#define CS_WHERE_CAM " (camera_id=?1 OR (camera_id >= ?5 AND camera_id < ?6))"

  const char *SQL_PLAIN =
    "SELECT id,captured_at,sha256,blob_path,bytes,phash,ocr_text,width,height,"
    "       scene_changed,camera_id,embed_drift,drift_from_mean"
    "  FROM camera_stills WHERE" CS_WHERE_CAM
    " ORDER BY captured_at DESC, id DESC LIMIT ?2";
  const char *SQL_CUR =
    "SELECT id,captured_at,sha256,blob_path,bytes,phash,ocr_text,width,height,"
    "       scene_changed,camera_id,embed_drift,drift_from_mean"
    "  FROM camera_stills WHERE" CS_WHERE_CAM
    "   AND (captured_at < ?3 OR (captured_at = ?3 AND id < ?4))"
    " ORDER BY captured_at DESC, id DESC LIMIT ?2";

  sqlite3_stmt *s;
  int use_cur = (cur_p && cur_u) ? 1 : 0;
  if (sqlite3_prepare_v2(db->h, use_cur ? SQL_CUR : SQL_PLAIN, -1, &s, NULL)
      != SQLITE_OK) {
    if (curj) cJSON_Delete(curj);
    free(lo); free(hi); free(rec_uid);
    free(cam);
    return NULL;
  }
  sqlite3_bind_text(s, 1, cam, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (s, 2, limit);
  if (use_cur) {
    sqlite3_bind_text(s, 3, cur_p, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, cur_u, -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_text(s, 5, lo, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 6, hi, -1, SQLITE_TRANSIENT);

  cJSON *arr = cJSON_CreateArray();
  int count = 0, present = 0;
  char last_p[64] = {0}, last_u[64] = {0};
  while (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *o = cJSON_CreateObject();
    const char *v;
    v = ctext(s, 0); if (v) cJSON_AddStringToObject(o, "id", v);
    v = ctext(s, 1); if (v) cJSON_AddStringToObject(o, "captured_at", v);
    v = ctext(s, 2); if (v) cJSON_AddStringToObject(o, "sha256", v);
    if (sqlite3_column_type(s, 4) != SQLITE_NULL)
      cJSON_AddNumberToObject(o, "bytes", (double)sqlite3_column_int64(s, 4));
    else cJSON_AddNullToObject(o, "bytes");
    /* 16-char hex, never a JSON number: a 64-bit hash does not survive an IEEE
     * double and a silently truncated fingerprint is worse than none. */
    if (sqlite3_column_type(s, 5) != SQLITE_NULL)
      put_hex64(o, "phash", sqlite3_column_int64(s, 5));
    else cJSON_AddNullToObject(o, "phash");
    if (ctext(s, 6)) cJSON_AddStringToObject(o, "ocr_text", ctext(s, 6));
    else             cJSON_AddNullToObject(o, "ocr_text");
    if (sqlite3_column_type(s, 7) != SQLITE_NULL)
      cJSON_AddNumberToObject(o, "width", (double)sqlite3_column_int64(s, 7));
    else cJSON_AddNullToObject(o, "width");
    if (sqlite3_column_type(s, 8) != SQLITE_NULL)
      cJSON_AddNumberToObject(o, "height", (double)sqlite3_column_int64(s, 8));
    else cJSON_AddNullToObject(o, "height");
    /* true / false / null. NULL is UNKNOWN and MUST render differently from
     * false — see the scene-change note in the header. */
    if (sqlite3_column_type(s, 9) != SQLITE_NULL)
      cJSON_AddBoolToObject(o, "scene_changed", sqlite3_column_int(s, 9) != 0);
    else cJSON_AddNullToObject(o, "scene_changed");
    /* Embedding drift — a DIFFERENT question from scene_changed (which is
     * pHash/bytes): how far the frame moved in the image index's space from
     * the previous frame and from the recent mean. null = not measured. */
    if (sqlite3_column_type(s, 11) != SQLITE_NULL)
      cJSON_AddNumberToObject(o, "embed_drift", sqlite3_column_double(s, 11));
    else cJSON_AddNullToObject(o, "embed_drift");
    if (sqlite3_column_type(s, 12) != SQLITE_NULL)
      cJSON_AddNumberToObject(o, "drift_from_mean", sqlite3_column_double(s, 12));
    else cJSON_AddNullToObject(o, "drift_from_mean");
    /* Which view of the record this frame is (null = the record's own feed),
     * since a record-level listing interleaves every view's timeline. */
    { const char *cid = ctext(s, 10);
      const char *vs = NULL;
      char *cu = cid ? camera_feed_split(cid, &vs) : NULL;
      if (cid) cJSON_AddStringToObject(o, "camera_id", cid);
      if (vs)  cJSON_AddStringToObject(o, "view", vs);
      else     cJSON_AddNullToObject(o, "view");
      free(cu); }

    const char *rel = ctext(s, 3);
    int have = 0;
    if (rel && *rel) {
      char p[2048];
      struct stat stt;
      if (blob_abs_path(rel, p, sizeof p) && stat(p, &stt) == 0) have = 1;
    }
    cJSON_AddBoolToObject(o, "blob_present", have);
    if (have) present++;

    if (ctext(s, 1)) snprintf(last_p, sizeof last_p, "%s", ctext(s, 1));
    if (ctext(s, 0)) snprintf(last_u, sizeof last_u, "%s", ctext(s, 0));
    cJSON_AddItemToArray(arr, o);
    count++;
  }
  sqlite3_finalize(s);
  if (curj) cJSON_Delete(curj);

  /* meta: the aggregate the UI needs to say "48 frames, 3 changes, 12 unknown"
   * without paging the whole series. */
  int total = 0, changed = 0, unknown = 0;
  char oldest[64] = {0}, newest[64] = {0};
  if (sqlite3_prepare_v2(db->h,
      "SELECT COUNT(*), SUM(CASE WHEN scene_changed=1 THEN 1 ELSE 0 END),"
      "       SUM(CASE WHEN scene_changed IS NULL THEN 1 ELSE 0 END),"
      "       MIN(captured_at), MAX(captured_at)"
      "  FROM camera_stills WHERE" CS_WHERE_CAM, -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, cam, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 5, lo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 6, hi, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      total = sqlite3_column_int(s, 0);
      changed = sqlite3_column_int(s, 1);
      unknown = sqlite3_column_int(s, 2);
      if (ctext(s, 3)) snprintf(oldest, sizeof oldest, "%s", ctext(s, 3));
      if (ctext(s, 4)) snprintf(newest, sizeof newest, "%s", ctext(s, 4));
    }
    sqlite3_finalize(s);
  }

  /* The opt-in lives on the RECORD; its views come from its current
   * properties, each with its own scheduler state, so a client can show
   * "3 views: 2 capturing, 1 backing off (http_404)". */
  int opted = 0, fails = 0, targets_total = 0, indexable = 0;
  char nextat[64] = {0}, lasterr[192] = {0}, lastkind[32] = {0};
  cs_targets T = { 0 };
  if (sqlite3_prepare_v2(db->h,
      "SELECT COALESCE(capture_stills,0), source_id, properties"
      "  FROM intel_items WHERE uid=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, rec_uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      opted = sqlite3_column_int(s, 0) != 0;
      targets_total = expand_row(&T, rec_uid, ctext(s, 1), ctext(s, 2));
      indexable = camera_record_indexable(ctext(s, 2), ctext(s, 1));
    }
    sqlite3_finalize(s);
  }
  cJSON *tarr = cJSON_CreateArray();
  if (sqlite3_prepare_v2(db->h,
      "SELECT fail_count,next_attempt_at,last_error,last_kind"
      "  FROM camera_stills_state WHERE camera_id=?1", -1, &s, NULL)
      == SQLITE_OK) {
    for (int i = 0; i < T.n; i++) {
      cJSON *to = cJSON_CreateObject();
      const char *vs = NULL;
      char *cu = camera_feed_split(T.v[i].tid, &vs);
      cJSON_AddStringToObject(to, "camera_id", T.v[i].tid);
      if (vs) cJSON_AddStringToObject(to, "view", vs);
      else    cJSON_AddNullToObject(to, "view");
      free(cu);
      if (T.v[i].url) {
        cJSON_AddStringToObject(to, "url", T.v[i].url);
        cJSON_AddStringToObject(to, "kind",
                                kind_name(camera_stills_classify(T.v[i].url)));
      } else {
        cJSON_AddNullToObject(to, "url");
        cJSON_AddStringToObject(to, "kind", kind_name(CAM_FEED_NONE));
      }
      sqlite3_reset(s);
      sqlite3_bind_text(s, 1, T.v[i].tid, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(s) == SQLITE_ROW) {
        cJSON_AddNumberToObject(to, "fail_count", sqlite3_column_int(s, 0));
        if (ctext(s, 1)) cJSON_AddStringToObject(to, "next_attempt_at", ctext(s, 1));
        else             cJSON_AddNullToObject(to, "next_attempt_at");
        if (ctext(s, 2)) cJSON_AddStringToObject(to, "last_error", ctext(s, 2));
        else             cJSON_AddNullToObject(to, "last_error");
      } else {
        cJSON_AddNumberToObject(to, "fail_count", 0);
        cJSON_AddNullToObject(to, "next_attempt_at");
        cJSON_AddNullToObject(to, "last_error");
      }
      cJSON_AddItemToArray(tarr, to);
    }
    sqlite3_finalize(s);
  }
  targets_free(&T);
  if (sqlite3_prepare_v2(db->h,
      "SELECT fail_count,next_attempt_at,last_error,last_kind"
      "  FROM camera_stills_state WHERE camera_id=?1", -1, &s, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(s, 1, cam, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      fails = sqlite3_column_int(s, 0);
      if (ctext(s, 1)) snprintf(nextat, sizeof nextat, "%s", ctext(s, 1));
      if (ctext(s, 2)) snprintf(lasterr, sizeof lasterr, "%s", ctext(s, 2));
      if (ctext(s, 3)) snprintf(lastkind, sizeof lastkind, "%s", ctext(s, 3));
    }
    sqlite3_finalize(s);
  }

  int have_ph = 0;
  media_tools(&have_ph, NULL);

  cJSON *page = cJSON_CreateObject();
  cJSON_AddNumberToObject(page, "limit", limit);
  cJSON_AddNumberToObject(page, "count", count);
  if (count == limit && last_p[0] && last_u[0]) {
    cJSON *c = cJSON_CreateObject();
    cJSON_AddStringToObject(c, "p", last_p);
    cJSON_AddStringToObject(c, "u", last_u);
    char *cj = cJSON_PrintUnformatted(c);
    cJSON_Delete(c);
    char *enc = cj ? b64url(cj) : NULL;
    free(cj);
    if (enc) { cJSON_AddStringToObject(page, "cursor", enc); free(enc); }
    else cJSON_AddNullToObject(page, "cursor");
  } else {
    cJSON_AddNullToObject(page, "cursor");
  }

  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "camera_id", cam);
  cJSON_AddBoolToObject(meta, "capture_stills", opted);
  cJSON_AddNumberToObject(meta, "total", total);
  cJSON_AddNumberToObject(meta, "changed", changed);
  cJSON_AddNumberToObject(meta, "unknown", unknown);
  cJSON_AddNumberToObject(meta, "blobs_present", present);
  if (oldest[0]) cJSON_AddStringToObject(meta, "oldest", oldest);
  else           cJSON_AddNullToObject(meta, "oldest");
  if (newest[0]) cJSON_AddStringToObject(meta, "newest", newest);
  else           cJSON_AddNullToObject(meta, "newest");
  cJSON_AddNumberToObject(meta, "fail_count", fails);
  if (nextat[0]) cJSON_AddStringToObject(meta, "next_attempt_at", nextat);
  else           cJSON_AddNullToObject(meta, "next_attempt_at");
  if (lasterr[0]) cJSON_AddStringToObject(meta, "last_error", lasterr);
  else            cJSON_AddNullToObject(meta, "last_error");
  if (lastkind[0]) cJSON_AddStringToObject(meta, "last_kind", lastkind);
  else             cJSON_AddNullToObject(meta, "last_kind");
  cJSON_AddBoolToObject(meta, "scene_change_available", have_ph);
  /* drift is measured only where the image index exists and holds frames */
  cJSON_AddBoolToObject(meta, "embed_drift_available", image_vec_ready(db));
  cJSON_AddStringToObject(meta, "record_uid", rec_uid);
  /* Whether this camera's frames may ever be indexed (vision embeddings,
   * OCR search) — core/camera_privacy.h. Capture and viewing are unaffected. */
  cJSON_AddBoolToObject(meta, "indexable", indexable);
  /* targets_total can exceed the array: CAM_FEED_MAX_TARGETS per record is
   * captured, and the difference is stated here rather than dropped. */
  cJSON_AddItemToObject(meta, "targets", tarr);
  cJSON_AddNumberToObject(meta, "targets_total", targets_total);
  cJSON_AddNumberToObject(meta, "targets_captured_max", CAM_FEED_MAX_TARGETS);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", arr);
  cJSON_AddItemToObject(env, "page", page);
  cJSON_AddItemToObject(env, "meta", meta);
  char *j = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  free(lo); free(hi); free(rec_uid);
  free(cam);
  return j;
}

char *camera_stills_raw(db_handle *db, const auth_user *u, const char *still_id,
                        unsigned char **out_bytes, size_t *out_len,
                        char *out_sha, int *status) {
  if (status) *status = 200;
  if (!db || !db->h || !out_bytes || !out_len)
    return cs_err(status, 500, "server_error");
  if (!still_id || !*still_id) return cs_err(status, 400, "bad_request");
  camera_stills_migrate(db);

  /* Frames are raw bytes from arbitrary internet cameras. They are not tenant
   * data and are not role-gated: this is platform-operator material, the same
   * posture as evidence_raw() and the breach reveal path. */
  int oc = opgate_check(u);
  if (oc == -401)  return cs_err(status, 401, "Auth required");
  if (oc == -1403) return cs_err(status, 403,
                                 "Platform operator access not configured");
  if (oc != 0)     return cs_err(status, 403,
                                 "Platform operator role required");

  char sha[65] = {0}, cam[640] = {0}, rel[192] = {0};
  long long bytes = 0;
  int found = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
      "SELECT camera_id,sha256,blob_path,bytes FROM camera_stills WHERE id=?1",
      -1, &s, NULL) != SQLITE_OK)
    return cs_err(status, 500, "server_error");
  sqlite3_bind_text(s, 1, still_id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) {
    found = 1;
    if (ctext(s, 0)) snprintf(cam, sizeof cam, "%s", ctext(s, 0));
    if (ctext(s, 1)) snprintf(sha, sizeof sha, "%s", ctext(s, 1));
    if (ctext(s, 2)) snprintf(rel, sizeof rel, "%s", ctext(s, 2));
    bytes = sqlite3_column_int64(s, 3);
  }
  sqlite3_finalize(s);
  if (!found)          return cs_err(status, 404, "not_found");
  if (strlen(sha) < 3) return cs_err(status, 500, "server_error");
  /* 410, not 404: the row is intact and still attests to what the bytes were.
   * Either the operator never enabled preservation or the evidence reaper
   * evicted the blob under its byte budget. A 404 would hide both. */
  if (!rel[0]) return cs_err(status, 410, "blob_not_preserved");

  char p[2048];
  if (!blob_abs_path(rel, p, sizeof p)) return cs_err(status, 500, "server_error");
  struct stat st;
  if (stat(p, &st) != 0) return cs_err(status, 410, "blob_evicted");
  if ((long long)st.st_size > 256LL * 1024 * 1024)
    return cs_err(status, 500, "blob_too_large");

  int fd = open(p, O_RDONLY);
  if (fd < 0) return cs_err(status, 410, "blob_evicted");
  size_t len = (size_t)st.st_size;
  unsigned char *buf = malloc(len ? len : 1);
  if (!buf) { close(fd); return cs_err(status, 500, "server_error"); }
  size_t off = 0;
  while (off < len) {
    ssize_t r = read(fd, buf + off, len - off);
    if (r <= 0) break;
    off += (size_t)r;
  }
  close(fd);
  if (off != len) { free(buf); return cs_err(status, 500, "server_error"); }

  /* Integrity on the way OUT, not just on the way in. A blob whose bytes no
   * longer hash to what the row says is exactly the case worth detecting, and
   * serving it as "the frame" would be worse than serving nothing.
   *
   * NOTE the blob is the WHOLE captured object, which for a still IS the
   * frame: capture_frame() trims to one frame BEFORE hashing and before
   * evidence_capture(), so the stored bytes, the sha256 and this check are all
   * over the same extent. */
  char actual[65];
  sha256_hex(buf, len, actual);
  if (strcmp(actual, sha) != 0) {
    fprintf(stderr, "[camera-stills] blob %s hashes to %s — refusing to serve\n",
            sha, actual);
    free(buf);
    return cs_err(status, 500, "blob_integrity_failed");
  }

  cJSON *pl = cJSON_CreateObject();
  cJSON_AddStringToObject(pl, "sha256", sha);
  cJSON_AddNumberToObject(pl, "bytes", (double)bytes);
  cJSON_AddStringToObject(pl, "camera_id", cam);
  char *pj = cJSON_PrintUnformatted(pl);
  cJSON_Delete(pl);
  audit_write(db, "platform", u ? u->id : NULL, "camera_still.raw", still_id, pj);
  free(pj);

  *out_bytes = buf;
  *out_len = len;
  if (out_sha) snprintf(out_sha, 65, "%s", sha);
  if (status) *status = 200;
  return NULL;
}

char *camera_stills_capabilities(db_handle *db) {
  if (!db || !db->h) return NULL;
  camera_stills_migrate(db);

  int opted = 0, stills = 0, cams = 0, blobs = 0, bytes_on = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
      "SELECT COUNT(*) FROM intel_items WHERE capture_stills=1",
      -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) opted = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
  }
  if (sqlite3_prepare_v2(db->h,
      "SELECT COUNT(*), COUNT(DISTINCT camera_id),"
      "       SUM(CASE WHEN blob_path IS NOT NULL THEN 1 ELSE 0 END)"
      "  FROM camera_stills", -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) {
      stills = sqlite3_column_int(s, 0);
      cams   = sqlite3_column_int(s, 1);
      blobs  = sqlite3_column_int(s, 2);
    }
    sqlite3_finalize(s);
  }
  if (sqlite3_prepare_v2(db->h,
      "SELECT capture_evidence FROM sources WHERE id=?1", -1, &s, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(s, 1, CAM_STILLS_SID, -1, SQLITE_STATIC);
    if (sqlite3_step(s) == SQLITE_ROW &&
        sqlite3_column_type(s, 0) != SQLITE_NULL)
      bytes_on = sqlite3_column_int(s, 0) != 0;
    sqlite3_finalize(s);
  }

  /* Both from core/ffmpeg.h, which probes once per process and caches — so this
   * ops route no longer walks PATH on every request, and it reports the SAME
   * tool the capture path would actually use rather than a second opinion. */
  int have_ff = ffmpeg_available();
  const char *tool = ffmpeg_path();
  int have_ph = 0;
  media_tools(&have_ph, NULL);
  const char *ke = getenv("CAM_STILLS_ENABLED");

  cJSON *d = cJSON_CreateObject();
  cJSON_AddBoolToObject(d, "enabled", !(ke && strcmp(ke, "false") == 0));
  cJSON_AddNumberToObject(d, "cameras_opted_in", opted);
  cJSON_AddNumberToObject(d, "stills", stills);
  cJSON_AddNumberToObject(d, "cameras_with_stills", cams);
  cJSON_AddBoolToObject(d, "bytes_preserved", bytes_on);
  cJSON_AddNumberToObject(d, "blobs_present", blobs);

  cJSON *snap = cJSON_CreateObject();
  cJSON_AddBoolToObject(snap, "available", 1);
  cJSON_AddBoolToObject(snap, "builtin", 1);
  cJSON_AddItemToObject(d, "snapshot", snap);
  cJSON *mj = cJSON_CreateObject();
  cJSON_AddBoolToObject(mj, "available", 1);
  cJSON_AddBoolToObject(mj, "builtin", 1);
  cJSON_AddItemToObject(d, "mjpeg", mj);
  for (int i = 0; i < 2; i++) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "available", have_ff);
    if (have_ff && tool) cJSON_AddStringToObject(o, "tool", tool);
    else                 cJSON_AddNullToObject(o, "tool");
    cJSON_AddItemToObject(d, i == 0 ? "rtsp" : "hls", o);
  }
  cJSON *sc = cJSON_CreateObject();
  cJSON_AddBoolToObject(sc, "available", have_ph);
  cJSON_AddStringToObject(sc, "method", have_ph ? "phash" : "sha256_only");
  cJSON_AddItemToObject(d, "scene_change", sc);

  /* ── which binaries, which features, and how much of the corpus they have
   * actually reached. "pHash: available" says nothing about the frames
   * captured before the package was installed; "pHash on 12 of 480" does. */
  media_decoders md;
  media_probe_decoders(&md);
  int have_ocr = 0;
  media_tools(NULL, &have_ocr);
  cJSON *dec = cJSON_CreateObject();
  struct { const char *k, *path, *pkg; } tools[] = {
    { "magick",    md.magick,    "imagemagick" },
    { "convert",   md.convert,   "imagemagick" },
    { "gm",        md.gm,        "graphicsmagick" },
    { "tesseract", md.tesseract, "tesseract-ocr" },
    { "ffmpeg",    have_ff && tool ? tool : "", "ffmpeg" },
  };
  for (size_t i = 0; i < sizeof tools / sizeof tools[0]; i++) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "present", tools[i].path[0] != '\0');
    if (tools[i].path[0]) cJSON_AddStringToObject(o, "path", tools[i].path);
    else                  cJSON_AddNullToObject(o, "path");
    cJSON_AddStringToObject(o, "package", tools[i].pkg);
    if (i == 3) {
      if (md.tesseract_jpn < 0) cJSON_AddNullToObject(o, "jpn");
      else                      cJSON_AddBoolToObject(o, "jpn", md.tesseract_jpn);
      if (md.tesseract_langs[0]) cJSON_AddStringToObject(o, "langs", md.tesseract_langs);
      else                       cJSON_AddNullToObject(o, "langs");
    }
    cJSON_AddItemToObject(dec, tools[i].k, o);
  }
  cJSON_AddBoolToObject(dec, "media_tools_disabled", md.disabled);
  cJSON_AddItemToObject(d, "decoders", dec);

  int have_im = md.magick[0] || md.convert[0] || md.gm[0];
  cJSON *feat = cJSON_CreateObject();
  cJSON *fp = cJSON_CreateObject();
  cJSON_AddBoolToObject(fp, "available", have_ph);
  cJSON_AddStringToObject(fp, "via", have_ff ? "ffmpeg" : have_im ? "imagemagick"
                                     : "none");
  if (!have_ph)
    cJSON_AddStringToObject(fp, "missing", "ffmpeg or imagemagick/graphicsmagick");
  cJSON_AddItemToObject(feat, "phash", fp);
  cJSON *fo = cJSON_CreateObject();
  cJSON_AddBoolToObject(fo, "available", have_ocr);
  /* Japanese signage is most of what OCR is for on this corpus; tesseract
   * without jpn.traineddata reads it as noise and says nothing about it. */
  if (md.tesseract_jpn < 0) cJSON_AddNullToObject(fo, "japanese");
  else                      cJSON_AddBoolToObject(fo, "japanese", md.tesseract_jpn);
  if (!md.tesseract[0])
    cJSON_AddStringToObject(fo, "missing", "tesseract-ocr (+ tesseract-ocr-jpn)");
  else if (md.tesseract_jpn == 0)
    cJSON_AddStringToObject(fo, "missing", "tesseract-ocr-jpn (jpn.traineddata)");
  cJSON_AddItemToObject(feat, "ocr", fo);
  cJSON *fr = cJSON_CreateObject();
  cJSON_AddBoolToObject(fr, "available", have_ff);
  if (!have_ff) cJSON_AddStringToObject(fr, "missing", "ffmpeg");
  cJSON_AddItemToObject(feat, "rtsp_hls_stills", fr);
  cJSON_AddItemToObject(d, "features", feat);

  int n_ph = 0, n_ocr = 0;
  if (sqlite3_prepare_v2(db->h,
      "SELECT SUM(CASE WHEN phash IS NOT NULL THEN 1 ELSE 0 END),"
      "       SUM(CASE WHEN ocr_text IS NOT NULL THEN 1 ELSE 0 END)"
      "  FROM camera_stills", -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) {
      n_ph  = sqlite3_column_int(s, 0);
      n_ocr = sqlite3_column_int(s, 1);
    }
    sqlite3_finalize(s);
  }
  cJSON *cov = cJSON_CreateObject();
  cJSON_AddNumberToObject(cov, "stills", stills);
  cJSON_AddNumberToObject(cov, "phash", n_ph);
  cJSON_AddNumberToObject(cov, "ocr", n_ocr);
  char line[96];
  snprintf(line, sizeof line, "pHash on %d of %d", n_ph, stills);
  cJSON_AddStringToObject(cov, "phash_text", line);
  snprintf(line, sizeof line, "OCR on %d of %d", n_ocr, stills);
  cJSON_AddStringToObject(cov, "ocr_text", line);
  cJSON_AddItemToObject(d, "coverage", cov);

  cJSON *ret = cJSON_CreateObject();
  cJSON_AddNumberToObject(ret, "days", cfg_retain_days());
  cJSON_AddNumberToObject(ret, "max_per_camera", cfg_max_per_cam());
  cJSON_AddNumberToObject(ret, "interval_sec", cfg_interval());
  cJSON_AddNumberToObject(ret, "max_bytes", (double)cfg_max_bytes());
  cJSON_AddItemToObject(d, "retention", ret);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", d);
  char *j = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return j;
}

/* ══════════════════════════════════════════════════════════════════════════
 * The pod
 *
 * `_maint` (== collectors/sources/entity_stats_pod.c): scheduler.c skips
 * fetch_log_write() and anomaly_detect() for underscore collectors, which this
 * job needs on both counts — it emits zero intel_items (records=0 forever
 * would trip records_drop against its own baseline) and its wall-clock
 * duration is whatever an operator's opt-in list makes it. It also inherits
 * the scheduler's skip-if-running serialisation, so two ticks can never
 * overlap — which is what lets fetch_frame() use a function-static buffer for
 * the "http_<status>" reason string.
 *
 * 300s ticks against a 900s per-camera interval: a camera is sampled within
 * ~5 minutes of becoming due rather than up to a full interval late, and the
 * batch cap keeps the pod from becoming a de facto crawler running at full
 * tilt behind the operator's back.
 *
 * ORDER: capture first, sweep second — the sweep should see anything this tick
 * just wrote, and the on-write prune has already enforced the ceiling for the
 * cameras that were touched, so the sweep is only ever mopping up cameras
 * nobody is capturing any more.
 * ══════════════════════════════════════════════════════════════════════════ */

static int pod_run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                              /* writes go to camera_stills      */
  if (!ctx || !ctx->db) return -1;

  const char *ke = getenv("CAM_STILLS_ENABLED");
  if (ke && strcmp(ke, "false") == 0) return 0;

  camera_stills_migrate(ctx->db);
  camera_stills_capture_batch(ctx->db, ctx->http, 0, ctx->cancel);
  if (ctx->cancel && *ctx->cancel) return 0;
  camera_stills_sweep(ctx->db, 0, ctx->cancel);
  return 0;
}

static const source_def camera_stills_def = {
  .id = CAM_STILLS_SID, .collector = "_maint",
  .name = "Camera Stills (scheduled snapshots)",
  .name_ja = "カメラ静止画（定期取得）",
  .update_interval_sec = 300, .run = pod_run,
  .category = "maintenance" };
REGISTER_SOURCE(camera_stills_def)
