/* lib/ws.c — see header. libcurl CONNECT_ONLY=2 WebSocket client. */
#include "ws.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <poll.h>

static long now_ms(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return (long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int ws_collect(const char *url, const char *const *headers,
                int window_ms, int max_msgs, ws_on_msg cb, void *ud) {
  if (!url || !cb) return -1;
  if (window_ms <= 0) window_ms = 8000;

  CURL *c = curl_easy_init();
  if (!c) return -1;
  struct curl_slist *hl = NULL;
  for (int i = 0; headers && headers[i]; i++) hl = curl_slist_append(hl, headers[i]);

  curl_easy_setopt(c, CURLOPT_URL, url);
  curl_easy_setopt(c, CURLOPT_CONNECT_ONLY, 2L);          /* WebSocket mode */
  curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, (long)(window_ms > 15000 ? 15000 : window_ms));
  curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
  if (hl) curl_easy_setopt(c, CURLOPT_HTTPHEADER, hl);

  CURLcode rc = curl_easy_perform(c);                     /* WS handshake */
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

    if (got) {                                             /* accumulate */
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
    if (complete && mlen) {
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
