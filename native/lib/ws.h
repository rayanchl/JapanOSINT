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

#endif
