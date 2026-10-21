/* core/hls_check.c — see hls_check.h. */
#include "hls_check.h"
#include "hostgate.h"
#include <curl/curl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* A playlist is text and small: a long VOD media playlist of 2-second segments
 * is ~100 bytes a segment, so 4 MiB is about 40,000 segments. Bigger is
 * refused (fail closed), not judged by its first part. */
#define HLS_MAX_BYTES   (4u * 1024u * 1024u)
#define HLS_TIMEOUT_MS  10000
#define HLS_MAX_DEPTH   3      /* master -> media -> (a nested master) */
#define HLS_MAX_FETCHES 64     /* playlists read for one input            */
#define HLS_HOST_MEMO   64     /* hosts already judged within one input   */

typedef struct {
  int fetches;
  int nhosts;
  char hosts[HLS_HOST_MEMO][256];
} hls_walk;

static void set_why(char *why, size_t cap, const char *fmt, const char *a) {
  if (why && cap) snprintf(why, cap, fmt, a ? a : "");
}

/* ── resolution ─────────────────────────────────────────────────────────── */

int hls_resolve(const char *base, const char *ref, char *out, size_t cap) {
  if (!base || !ref || !out || !cap) return -1;
  int n;
  if (strstr(ref, "://")) {                                /* absolute */
    n = snprintf(out, cap, "%s", ref);
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
  }
  const char *sep = strstr(base, "://");
  if (!sep) return -1;
  size_t scheme_len = (size_t)(sep - base);
  const char *auth = sep + 3;
  size_t auth_len = strcspn(auth, "/?#");
  if (ref[0] == '/' && ref[1] == '/') {                    /* //host/path */
    n = snprintf(out, cap, "%.*s:%s", (int)scheme_len, base, ref);
  } else if (ref[0] == '/') {                              /* /path */
    n = snprintf(out, cap, "%.*s%s", (int)(auth - base + auth_len), base, ref);
  } else {                                                 /* relative */
    const char *path = auth + auth_len;
    size_t plen = strcspn(path, "?#");
    size_t dir = plen;
    while (dir > 0 && path[dir - 1] != '/') dir--;
    if (dir == 0)
      n = snprintf(out, cap, "%.*s/%s", (int)(auth - base + auth_len), base, ref);
    else
      n = snprintf(out, cap, "%.*s%s", (int)(path - base + dir), base, ref);
  }
  return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

/* ── fetch ──────────────────────────────────────────────────────────────── */

typedef struct {
  char *buf; size_t len, cap;
  int not_playlist;   /* the first bytes were not #EXTM3U: stop reading */
  int too_large;
} hls_buf;

/* Is the head of the body an M3U playlist? Skips a UTF-8 BOM and whitespace,
 * as ffmpeg's hls probe does. -1 = not enough bytes yet to tell. */
static int looks_m3u(const char *b, size_t n) {
  size_t i = 0;
  if (n >= 3 && (unsigned char)b[0] == 0xEF && (unsigned char)b[1] == 0xBB &&
      (unsigned char)b[2] == 0xBF) i = 3;
  while (i < n && isspace((unsigned char)b[i])) i++;
  if (n - i < 7) return n > 512 ? 0 : -1;
  return strncmp(b + i, "#EXTM3U", 7) == 0;
}

static size_t hls_write(char *p, size_t sz, size_t nm, void *ud) {
  hls_buf *h = ud;
  size_t n = sz * nm;
  if (h->len + n > HLS_MAX_BYTES) { h->too_large = 1; return 0; }
  if (h->len + n + 1 > h->cap) {
    size_t nc = h->cap ? h->cap : 16384;
    while (nc < h->len + n + 1) nc *= 2;
    char *nb = realloc(h->buf, nc);
    if (!nb) return 0;
    h->buf = nb; h->cap = nc;
  }
  memcpy(h->buf + h->len, p, n);
  h->len += n;
  h->buf[h->len] = 0;
  int m = looks_m3u(h->buf, h->len);
  if (m == 0) { h->not_playlist = 1; return 0; }   /* stop: not a playlist */
  return n;
}

#if LIBCURL_VERSION_NUM >= 0x075000            /* 7.80.0: CURLOPT_PREREQFUNCTION */
static int hls_prereq(void *ud, char *primary_ip, char *local_ip,
                      int primary_port, int local_port) {
  (void)ud; (void)local_ip; (void)primary_port; (void)local_port;
  return hostgate_camera_addr_check(primary_ip) == HG_URL_OK
         ? CURL_PREREQFUNC_OK : CURL_PREREQFUNC_ABORT;
}
#endif

/* 1 = a playlist, read whole into *text; 0 = not a playlist; <0 = HLS_* code. */
static int fetch_playlist(const char *url, char **text, char *why, size_t cap) {
  *text = NULL;
  CURL *e = curl_easy_init();
  if (!e) { set_why(why, cap, "could not start a request for %s", url); return HLS_UNREADABLE; }
  hls_buf h = {0};
  curl_easy_setopt(e, CURLOPT_URL, url);
  curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, hls_write);
  curl_easy_setopt(e, CURLOPT_WRITEDATA, &h);
#if LIBCURL_VERSION_NUM >= 0x075500            /* 7.85.0 */
  curl_easy_setopt(e, CURLOPT_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(e, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
  curl_easy_setopt(e, CURLOPT_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
  curl_easy_setopt(e, CURLOPT_REDIR_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
#if LIBCURL_VERSION_NUM >= 0x075000
  /* Every hop's address is judged, as the camera proxy does. */
  curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(e, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(e, CURLOPT_PREREQFUNCTION, hls_prereq);
#else
  curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 0L);   /* no per-hop hook */
#endif
  curl_easy_setopt(e, CURLOPT_TIMEOUT_MS, (long)HLS_TIMEOUT_MS);
  curl_easy_setopt(e, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
  curl_easy_setopt(e, CURLOPT_USERAGENT, "JapanOSINT/1.0 (+native)");
  curl_easy_setopt(e, CURLOPT_NOSIGNAL, 1L);
  CURLcode rc = curl_easy_perform(e);
  long code = 0;
  curl_easy_getinfo(e, CURLINFO_RESPONSE_CODE, &code);
  curl_easy_cleanup(e);

  /* Status first: a 404 page is not a playlist, but a variant that 404s to
   * us may not 404 to ffmpeg, so an unanswered URL is refused, not passed. */
  if (code < 200 || code >= 300) {
    free(h.buf);
    set_why(why, cap, "could not read %s to check it", url);
    return HLS_UNREADABLE;
  }
  if (h.not_playlist) { free(h.buf); return 0; }
  if (h.too_large) {
    free(h.buf);
    set_why(why, cap, "playlist %s is larger than the 4 MiB the check reads", url);
    return HLS_UNREADABLE;
  }
  if (rc != CURLE_OK) {
    free(h.buf);
    set_why(why, cap, "could not read %s to check it", url);
    return HLS_UNREADABLE;
  }
  if (!h.buf || looks_m3u(h.buf, h.len) != 1) { free(h.buf); return 0; }
  *text = h.buf;
  return 1;
}

/* ── judging ────────────────────────────────────────────────────────────── */

static int judge_url(hls_walk *w, const char *url, char *why, size_t cap) {
  const char *u = !strncasecmp(url, "hls+", 4) ? url + 4 : url;
  if (strncasecmp(u, "http://", 7) && strncasecmp(u, "https://", 8)) {
    set_why(why, cap, "playlist names a non-http URI: %s", url);
    return HLS_BLOCKED;
  }
  char host[256];
  if (!hostgate_url_host(u, host, sizeof host)) {
    set_why(why, cap, "playlist names a URI with no host: %s", url);
    return HLS_BLOCKED;
  }
  for (int i = 0; i < w->nhosts; i++)
    if (!strcmp(w->hosts[i], host)) return HLS_OK;   /* already judged */
  int hg = hostgate_camera_url_check(u, 1);
  if (hg != HG_URL_OK) {
    if (why && cap) snprintf(why, cap, "playlist names %s: %s", url, hostgate_url_reason(hg));
    return HLS_BLOCKED;
  }
  if (w->nhosts < HLS_HOST_MEMO)
    snprintf(w->hosts[w->nhosts++], sizeof w->hosts[0], "%s", host);
  return HLS_OK;
}

static int check_text(hls_walk *w, const char *base, const char *text,
                      int depth, int follow, char *why, size_t cap);

static int check_url(hls_walk *w, const char *url, int depth, int follow,
                     int fetch, char *why, size_t cap) {
  int rc = judge_url(w, url, why, cap);
  if (rc != HLS_OK || !fetch || !follow) return rc;
  if (depth >= HLS_MAX_DEPTH) {
    set_why(why, cap, "playlists nest deeper than the check follows at %s", url);
    return HLS_UNREADABLE;
  }
  if (++w->fetches > HLS_MAX_FETCHES) {
    set_why(why, cap, "more playlists than the check reads, at %s", url);
    return HLS_UNREADABLE;
  }
  char *text = NULL;
  const char *u = !strncasecmp(url, "hls+", 4) ? url + 4 : url;
  rc = fetch_playlist(u, &text, why, cap);
  if (rc <= 0) return rc;            /* not a playlist (0) or a refusal */
  rc = check_text(w, u, text, depth + 1, follow, why, cap);
  free(text);
  return rc;
}

/* Every URI a playlist names: plain lines, and URI="…" on the tags that carry
 * one (EXT-X-KEY, -MAP, -MEDIA, -I-FRAME-STREAM-INF, -SESSION-KEY,
 * -PRELOAD-HINT, -RENDITION-REPORT, -SESSION-DATA …). Playlists among them —
 * the line after EXT-X-STREAM-INF, the URI of EXT-X-MEDIA and
 * EXT-X-I-FRAME-STREAM-INF, anything whose path ends .m3u8/.m3u — are read
 * and judged in turn. */
static int check_text(hls_walk *w, const char *base, const char *text,
                      int depth, int follow, char *why, size_t cap) {
  int next_is_playlist = 0, rc = HLS_OK;
  const char *p = text;
  char line[4096], abs[4096];
  while (*p && rc == HLS_OK) {
    size_t n = strcspn(p, "\r\n");
    size_t m = n < sizeof line - 1 ? n : sizeof line - 1;
    memcpy(line, p, m); line[m] = 0;
    p += n;
    while (*p == '\r' || *p == '\n') p++;
    char *s = line;
    while (isspace((unsigned char)*s)) s++;
    size_t sl = strlen(s);
    while (sl && isspace((unsigned char)s[sl - 1])) s[--sl] = 0;
    if (!*s) continue;
    if (n >= sizeof line - 1) {
      set_why(why, cap, "playlist line longer than the check reads in %s", base);
      return HLS_UNREADABLE;
    }
    if (*s == '#') {
      if (!strncmp(s, "#EXT-X-STREAM-INF", 17)) next_is_playlist = 1;
      int tag_playlist = !strncmp(s, "#EXT-X-MEDIA", 12) ||
                         !strncmp(s, "#EXT-X-I-FRAME-STREAM-INF", 25);
      for (char *q = strstr(s, "URI=\""); q && rc == HLS_OK; q = strstr(q, "URI=\"")) {
        q += 5;
        char *end = strchr(q, '"');
        if (!end) break;
        *end = 0;
        if (hls_resolve(base, q, abs, sizeof abs) != 0) {
          set_why(why, cap, "unresolvable URI in %s", base);
          return HLS_UNREADABLE;
        }
        size_t al = strcspn(abs, "?#");
        int is_pl = tag_playlist ||
                    (al > 5 && !strncasecmp(abs + al - 5, ".m3u8", 5));
        rc = check_url(w, abs, depth, follow, is_pl, why, cap);
        q = end + 1;
      }
      continue;
    }
    if (hls_resolve(base, s, abs, sizeof abs) != 0) {
      set_why(why, cap, "unresolvable URI in %s", base);
      return HLS_UNREADABLE;
    }
    size_t al = strcspn(abs, "?#");
    int is_pl = next_is_playlist ||
                (al > 5 && !strncasecmp(abs + al - 5, ".m3u8", 5)) ||
                (al > 4 && !strncasecmp(abs + al - 4, ".m3u", 4));
    next_is_playlist = 0;
    rc = check_url(w, abs, depth, follow, is_pl, why, cap);
  }
  return rc;
}

int hls_check_text(const char *base_url, const char *text, int follow,
                   char *why, size_t why_cap) {
  if (!base_url || !text) return HLS_UNREADABLE;
  hls_walk w;
  memset(&w, 0, sizeof w);
  return check_text(&w, base_url, text, 0, follow, why, why_cap);
}

int hls_check_input(const char *url, char *why, size_t why_cap) {
  if (!url) return HLS_UNREADABLE;
  const char *u = !strncasecmp(url, "hls+", 4) ? url + 4 : url;
  if (strncasecmp(u, "http://", 7) && strncasecmp(u, "https://", 8))
    return HLS_OK;                              /* not an http input */
  char *text = NULL;
  int rc = fetch_playlist(u, &text, why, why_cap);
  if (rc <= 0) return rc;                        /* not a playlist, or refused */
  hls_walk w;
  memset(&w, 0, sizeof w);
  w.fetches = 1;
  rc = check_text(&w, u, text, 0, 1, why, why_cap);
  free(text);
  return rc;
}
