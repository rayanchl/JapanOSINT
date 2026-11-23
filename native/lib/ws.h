/* lib/ws.h — bounded-window WebSocket collector over libcurl's WS API
 * (curl 7.86+; system libcurl has it). The bluesky-jetstream-jp /
 * certstream-jp collectors open a wss socket, read for a short window
 * (default ~8 s) or until N messages, then close — a perfect fit for the
 * run-once scheduler (no long-lived connection). Reassembles fragmented
 * text frames; one callback per complete UTF-8 text message. */
#ifndef JO_WS_H
#define JO_WS_H
#include <stddef.h>

/* Called once per complete text message (`data`,`len`; NUL-terminated).
 * Return non-zero to stop the collect early (e.g. max reached). */
typedef int (*ws_on_msg)(const char *data, size_t len, void *ud);

/* Connect `url` (wss:// or ws://), optional NULL-terminated extra request
 * headers, then recv until `window_ms` elapses, `max_msgs` complete text
 * messages were delivered, the peer closes, or the callback returns
 * non-zero. Returns #messages delivered (>=0), or -1 if the connection /
 * handshake failed. */
int ws_collect(const char *url, const char *const *headers,
               int window_ms, int max_msgs, ws_on_msg cb, void *ud);

/* Messages the LAST ws_collect() on this thread dropped for exceeding
 * $JO_WS_MAX_MSG_BYTES (default 16 MB). They were never delivered, so a
 * caller that wants to disclose a shortfall (rule 2) reads this after the
 * call. */
int ws_last_oversize_dropped(void);
/* ... and the bytes of those messages (what was buffered plus what followed
 * until the message ended or the window did). Rule 2: the notice a collector
 * files says how much was dropped, not only that something was. */
size_t ws_last_oversize_dropped_bytes(void);
/* The per-message ceiling in force ($JO_WS_MAX_MSG_BYTES, default 16 MB). */
size_t ws_message_ceiling(void);

/* The reassembly step ws_collect() runs per received chunk, exposed so the
 * bound and its accounting can be tested without a socket. */
typedef struct {
  char *msg; size_t mlen, mcap, max;
  int discarding;
  int dropped;                 /* messages dropped for exceeding `max`      */
  size_t dropped_bytes;        /* their bytes, as far as they were received */
} ws_asm;
void ws_asm_init(ws_asm *a, size_t max_msg);
void ws_asm_free(ws_asm *a);
int  ws_asm_feed(ws_asm *a, const char *data, size_t got, int complete,
                 ws_on_msg cb, void *ud, int *delivered);

#endif
