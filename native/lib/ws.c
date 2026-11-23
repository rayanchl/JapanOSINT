/* lib/ws.c — see header. libcurl CONNECT_ONLY=2 WebSocket client. */
#include "ws.h"
#include "core/hostgate.h"      /* hostgate_url_check / acquire: the SSRF floor and pacing */
#include "core/httpclient.h"    /* http_easy_harden, http_ua_override: the one transport policy */
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <poll.h>
#include <strings.h>
#include "joenv.h"

/* One reassembled message may not grow past this (LIB-18). The buffer used to
 * grow without limit until the window closed, so a peer streaming one endless
 * fragmented message could take the process's memory. An over-size message is
 * dropped WHOLE and counted — never delivered truncated, which would hand the
 * callback half a JSON document. */
static size_t ws_max_msg(void) {
  return (size_t)jo_env_ll_strict("JO_WS_MAX_MSG_BYTES", 16LL << 20, 1024, LLONG_MAX);
}

static __thread int g_oversize_dropped;
int ws_last_oversize_dropped(void) { return g_oversize_dropped; }

/* "wss://h/p" -> "https://h/p": hostgate and the agent table judge http(s)
 * URLs, and a WebSocket upgrade is an HTTP request to the same host. */
static int ws_http_form(const char *url, char *out, size_t cap) {
  const char *rest;
  const char *scheme;
  if (!strncasecmp(url, "wss://", 6))     { scheme = "https://"; rest = url + 6; }
  else if (!strncasecmp(url, "ws://", 5)) { scheme = "http://";  rest = url + 5; }
  else if (!strncasecmp(url, "https://", 8) || !strncasecmp(url, "http://", 7)) {
    return snprintf(out, cap, "%s", url) < (int)cap ? 0 : -1;
  } else return -1;
  return snprintf(out, cap, "%s%s", scheme, rest) < (int)cap ? 0 : -1;
}

static long now_ms(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return (long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int ws_collect(const char *url, const char *const *headers,
                int window_ms, int max_msgs, ws_on_msg cb, void *ud) {
  if (!url || !cb) return -1;
  if (window_ms <= 0) window_ms = 8000;
  g_oversize_dropped = 0;

  /* The same destination floor every http_request() clears: a blocked or
   * unresolvable target is an explicit failure, never a connect. (Before this
   * the WebSocket path had its own curl handle with none of it.) */
  char hurl[2048];
  if (ws_http_form(url, hurl, sizeof hurl) != 0) {
    fprintf(stderr, "[ws] refused: not a ws/wss/http/https URL, or too long\n");
    return -1;
  }
  int hg = hostgate_url_check(hurl);
  if (hg != HG_URL_OK) {
    fprintf(stderr, "[ws] blocked: %s\n", hostgate_url_reason(hg));
    return -1;
  }

  CURL *c = curl_easy_init();
  if (!c) return -1;
  struct curl_slist *hl = NULL;
  for (int i = 0; headers && headers[i]; i++) hl = curl_slist_append(hl, headers[i]);

  curl_easy_setopt(c, CURLOPT_URL, url);
  curl_easy_setopt(c, CURLOPT_CONNECT_ONLY, 2L);          /* WebSocket mode */
  curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, (long)(window_ms > 15000 ? 15000 : window_ms));
  /* The shared transport policy: redirect cap, per-connection SSRF re-check,
   * the extra TLS intermediates. It pins the protocol set to http,https, which
   * would refuse a ws:// URL, so the set is widened to the WebSocket schemes
   * AFTER it (a later setopt wins) and nothing else. */
  http_easy_harden(c);
#if LIBCURL_VERSION_NUM >= 0x075500
  curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, "http,https,ws,wss");
  curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS_STR, "http,https,ws,wss");
#endif
  { const char *ua = http_ua_override(hurl);
    curl_easy_setopt(c, CURLOPT_USERAGENT, ua ? ua : JO_USER_AGENT); }
  if (hl) curl_easy_setopt(c, CURLOPT_HTTPHEADER, hl);

  int gated = hostgate_acquire(hurl, window_ms < 30000 ? window_ms : 30000);
  CURLcode rc = curl_easy_perform(c);                     /* WS handshake */
  if (gated) hostgate_release(hurl);
  if (rc != CURLE_OK) {
    if (hl) curl_slist_free_all(hl);
    curl_easy_cleanup(c);
    return -1;
  }

  curl_socket_t sock = CURL_SOCKET_BAD;
  curl_easy_getinfo(c, CURLINFO_ACTIVESOCKET, &sock);

  long deadline = now_ms() + window_ms;
  int delivered = 0;
  char *msg = NULL; size_t mlen = 0, mcap = 0;            /* reassembly buf */
  char buf[16384];
  const size_t max_msg = ws_max_msg();
  int discarding = 0;                                     /* inside an over-size message */

  while (delivered < (max_msgs > 0 ? max_msgs : 0x7fffffff)) {
    long remain = deadline - now_ms();
    if (remain <= 0) break;

    size_t got = 0;
    const struct curl_ws_frame *meta = NULL;
    CURLcode r = curl_ws_recv(c, buf, sizeof buf, &got, &meta);
    if (r == CURLE_AGAIN) {                                /* wait for data */
      if (sock == CURL_SOCKET_BAD) break;
      struct pollfd p = { sock, POLLIN, 0 };
      poll(&p, 1, (int)(remain > 1000 ? 1000 : remain));
      continue;
    }
    if (r != CURLE_OK) break;                              /* closed / error */
    if (meta && (meta->flags & CURLWS_CLOSE)) break;

    if (got && !discarding && mlen + got > max_msg) {
      discarding = 1;                                      /* drop it whole, count it */
      g_oversize_dropped++;
      mlen = 0;
      fprintf(stderr, "[ws] message exceeds JO_WS_MAX_MSG_BYTES (%zu) — dropped\n", max_msg);
    }
    if (got && !discarding) {                              /* accumulate */
      if (mlen + got + 1 > mcap) {
        size_t nc = (mlen + got + 1) * 2;
        char *nm = realloc(msg, nc);
        if (!nm) break;
        msg = nm; mcap = nc;
      }
      memcpy(msg + mlen, buf, got);
      mlen += got;
    }
    /* message complete: nothing more pending in this frame and not a
     * fragmented-continuation that expects further frames. */
    int complete = meta && meta->bytesleft == 0 &&
                   !(meta->flags & CURLWS_CONT);
    if (complete && discarding) {
      discarding = 0; mlen = 0;
    } else if (complete && mlen) {
      msg[mlen] = 0;
      int stop = cb(msg, mlen, ud);
      delivered++;
      mlen = 0;
      if (stop) break;
    } else if (complete) {
      mlen = 0;                                            /* empty frame */
    }
  }

  free(msg);
  if (hl) curl_slist_free_all(hl);
  curl_easy_cleanup(c);
  return delivered;
}
