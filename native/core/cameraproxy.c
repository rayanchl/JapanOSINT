/* core/cameraproxy.c — see cameraproxy.h. */
#include "cameraproxy.h"
#include "httpclient.h"        /* http_client_global_init() — the one curl init */
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define CAM_SOURCE_ID       "camera-discovery"
#define CAM_PROXY_TIMEOUT_MS 5000
#define CAM_PROXY_MAX_BYTES  (5 * 1024 * 1024)
#define CAM_PROXY_TTL_SEC    30
/* Entry count, not bytes, is what has to be bounded here: each entry can hold
 * up to CAM_PROXY_MAX_BYTES, and there are ~2k proxied cameras — an unbounded
 * uid-keyed map is a 10 GB ceiling. 16 entries × 5 MB is a hard 80 MB, and the
 * access pattern this serves (a handful of open feeds refreshing on a timer)
 * fits in it. */
#define CAM_PROXY_CACHE_MAX  16

/* ── tiny TTL cache ───────────────────────────────────────────────────────── */

typedef struct {
  char           uid[192];
  unsigned char *bytes;
  size_t         len;
  char           ct[64];
  time_t         ts;
} proxy_entry;

static pthread_mutex_t g_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static proxy_entry     g_cache[CAM_PROXY_CACHE_MAX];

/* Returns a COPY of the cached bytes (caller frees) or NULL. */
static unsigned char *cache_get(const char *uid, size_t *len,
                                char *ct, size_t ct_cap) {
  unsigned char *out = NULL;
  time_t now = time(NULL);
  pthread_mutex_lock(&g_cache_lock);
  for (int i = 0; i < CAM_PROXY_CACHE_MAX; i++) {
    if (!g_cache[i].bytes || strcmp(g_cache[i].uid, uid) != 0) continue;
    if (now - g_cache[i].ts >= CAM_PROXY_TTL_SEC) break;   /* stale → refetch */
    out = malloc(g_cache[i].len);
    if (out) {
      memcpy(out, g_cache[i].bytes, g_cache[i].len);
      *len = g_cache[i].len;
      snprintf(ct, ct_cap, "%s", g_cache[i].ct);
    }
    break;
  }
  pthread_mutex_unlock(&g_cache_lock);
  return out;
}

/* Takes ownership of nothing — copies. Reuses this uid's slot if present, else
 * the empty or oldest one. */
static void cache_put(const char *uid, const unsigned char *bytes, size_t len,
                      const char *ct) {
  if (len == 0 || len > CAM_PROXY_MAX_BYTES) return;
  unsigned char *copy = malloc(len);
  if (!copy) return;
  memcpy(copy, bytes, len);

  pthread_mutex_lock(&g_cache_lock);
  int slot = -1, oldest = 0;
  for (int i = 0; i < CAM_PROXY_CACHE_MAX; i++) {
    if (g_cache[i].bytes && strcmp(g_cache[i].uid, uid) == 0) { slot = i; break; }
    if (!g_cache[i].bytes) { slot = i; break; }
    if (g_cache[i].ts < g_cache[oldest].ts) oldest = i;
  }
  if (slot < 0) slot = oldest;
  free(g_cache[slot].bytes);
  g_cache[slot].bytes = copy;
  g_cache[slot].len   = len;
  g_cache[slot].ts    = time(NULL);
  snprintf(g_cache[slot].uid, sizeof g_cache[slot].uid, "%s", uid);
  snprintf(g_cache[slot].ct,  sizeof g_cache[slot].ct,  "%s",
           (ct && *ct) ? ct : "image/jpeg");
  pthread_mutex_unlock(&g_cache_lock);
}

/* ── upstream fetch ───────────────────────────────────────────────────────── */

typedef struct { unsigned char *buf; size_t len, cap; int capped; } sink;

static size_t sink_write(char *ptr, size_t sz, size_t nm, void *ud) {
  sink *s = ud;
  size_t n = sz * nm;
  if (n == 0) return 0;
  if (s->len + n > CAM_PROXY_MAX_BYTES) { s->capped = 1; return 0; }
  if (s->len + n > s->cap) {
    size_t nc = s->cap ? s->cap : 65536;
    while (nc < s->len + n) nc *= 2;
    unsigned char *q = realloc(s->buf, nc);
    if (!q) return 0;
    s->buf = q; s->cap = nc;
  }
  memcpy(s->buf + s->len, ptr, n);
  s->len += n;
  return n;
}

static char *errj(const char *msg) {
  size_t n = strlen(msg) + 32;
  char *s = malloc(n);
  if (s) snprintf(s, n, "{\"error\":\"%s\"}", msg);
  return s;
}

static char *dupcol(sqlite3_stmt *s, int i) {
  const unsigned char *t = sqlite3_column_text(s, i);
  return t ? strdup((const char *)t) : NULL;
}

/* Prefer the camera's IMAGE url over its page url: for aggregators `url` is
 * HTML and can never be served as an image (== the Node route's comment). */
static char *upstream_url_of(const char *props_json) {
  if (!props_json) return NULL;
  cJSON *p = cJSON_Parse(props_json);
  if (!p) return NULL;
  static const char *KEYS[] = { "thumbnail_url", "snapshot_url", "image",
                                "url", NULL };
  char *out = NULL;
  for (int i = 0; KEYS[i] && !out; i++) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(p, KEYS[i]);
    if (v && cJSON_IsString(v) && v->valuestring[0])
      out = strdup(v->valuestring);
  }
  cJSON_Delete(p);
  return out;
}

static int is_http_url(const char *u) {
  return u && (strncasecmp(u, "http://", 7) == 0 ||
               strncasecmp(u, "https://", 8) == 0);
}

char *camera_proxy_fetch(db_handle *db, const char *camera_uid,
                         unsigned char **out_bytes, size_t *out_len,
                         char *ct, size_t ct_cap, int *status) {
  if (status) *status = 400;
  if (!camera_uid || !*camera_uid || !out_bytes || !out_len || !ct)
    return errj("camera_uid query param required");
  if (!db || !db->h) { if (status) *status = 502; return errj("db unavailable"); }

  /* Served from cache before anything else, including the DB read. */
  size_t clen = 0;
  unsigned char *cached = cache_get(camera_uid, &clen, ct, ct_cap);
  if (cached) {
    *out_bytes = cached; *out_len = clen;
    if (status) *status = 200;
    return NULL;
  }

  /* Accept either the full intel_items uid or a bare camera_uid, same as the
   * stills module. */
  char *cam = NULL;
  if (strchr(camera_uid, '|')) {
    cam = strdup(camera_uid);
  } else {
    size_t n = strlen(CAM_SOURCE_ID) + 1 + strlen(camera_uid) + 1;
    cam = malloc(n);
    if (cam) snprintf(cam, n, "%s|%s", CAM_SOURCE_ID, camera_uid);
  }
  if (!cam) { if (status) *status = 502; return errj("out of memory"); }

  sqlite3_stmt *s = NULL;
  char *props = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT properties FROM intel_items WHERE uid=?1", -1, &s, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(s, 1, cam, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) props = dupcol(s, 0);
    sqlite3_finalize(s);
  }
  free(cam);
  if (!props) { if (status) *status = 404; return errj("camera not found"); }

  char *url = upstream_url_of(props);
  free(props);
  if (!url) return errj("camera has no url");
  /* The uid → URL indirection is what makes this not an SSRF: the caller never
   * supplies a URL, only the id of a camera this server discovered. The scheme
   * check keeps a file:// or gopher:// value in the property bag from becoming
   * a curl target. */
  if (!is_http_url(url)) { free(url); return errj("only http/https allowed"); }

  http_client_global_init();
  CURL *e = curl_easy_init();
  if (!e) { free(url); if (status) *status = 502; return errj("curl init failed"); }

  sink sk; memset(&sk, 0, sizeof sk);
  struct curl_slist *hl = NULL;
  hl = curl_slist_append(hl, "Accept: image/*");
  curl_easy_setopt(e, CURLOPT_URL, url);
  curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, sink_write);
  curl_easy_setopt(e, CURLOPT_WRITEDATA, &sk);
  curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(e, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(e, CURLOPT_TIMEOUT_MS, (long)CAM_PROXY_TIMEOUT_MS);
  curl_easy_setopt(e, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
  curl_easy_setopt(e, CURLOPT_USERAGENT, "JapanOsintApp/1.0 (camera-proxy)");
  curl_easy_setopt(e, CURLOPT_NOSIGNAL, 1L);
  if (hl) curl_easy_setopt(e, CURLOPT_HTTPHEADER, hl);

  CURLcode rc = curl_easy_perform(e);
  long code = 0;
  char upstream_ct[64] = {0};
  curl_easy_getinfo(e, CURLINFO_RESPONSE_CODE, &code);
  { char *cti = NULL;
    if (curl_easy_getinfo(e, CURLINFO_CONTENT_TYPE, &cti) == CURLE_OK && cti) {
      /* strip ";charset=…" */
      size_t n = strcspn(cti, ";");
      if (n >= sizeof upstream_ct) n = sizeof upstream_ct - 1;
      memcpy(upstream_ct, cti, n);
      upstream_ct[n] = 0;
      while (n && (upstream_ct[n-1] == ' ' || upstream_ct[n-1] == '\t'))
        upstream_ct[--n] = 0;
    } }
  if (hl) curl_slist_free_all(hl);
  curl_easy_cleanup(e);
  free(url);

  /* An MJPEG camera streams forever; hitting the cap is how that transfer
   * ends, so `capped` with bytes in hand is a SUCCESS, not a failure — the
   * partial buffer still starts with a complete JPEG the client can draw. */
  int aborted_at_cap = (rc == CURLE_WRITE_ERROR && sk.capped && sk.len > 0);
  if (rc != CURLE_OK && !aborted_at_cap) {
    free(sk.buf);
    if (status) *status = 502;
    return errj("upstream fetch failed");
  }
  if (code < 200 || code >= 300) {
    free(sk.buf);
    if (status) *status = 502;
    char m[64];
    snprintf(m, sizeof m, "upstream %ld", code);
    return errj(m);
  }
  if (sk.len == 0) {
    free(sk.buf);
    if (status) *status = 502;
    return errj("upstream returned no bytes");
  }
  if (strncasecmp(upstream_ct, "image/", 6) != 0) {
    free(sk.buf);
    if (status) *status = 502;
    return errj("upstream did not return an image");
  }

  snprintf(ct, ct_cap, "%s", upstream_ct);
  cache_put(camera_uid, sk.buf, sk.len, upstream_ct);
  *out_bytes = sk.buf;
  *out_len   = sk.len;
  if (status) *status = 200;
  return NULL;
}
