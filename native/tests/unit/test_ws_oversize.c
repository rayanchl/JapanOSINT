/* test_ws_oversize.c — ARC-01: a dropped over-size WebSocket message is reported
 * as data, with its count and bytes (rule 2). libcurl's WebSocket API is not
 * available in every build (Ubuntu 24.04's 8.5.0 has none), so this tests the
 * reassembly step ws_collect() runs per chunk (ws_asm_feed) and the notice the
 * two ws collectors file (jo_ws_oversize_notice_n), neither of which needs a
 * socket. */
#include "../../lib/ws.c"
#include "../../lib/jocore.h"

#include <assert.h>
#include <stdio.h>

typedef struct { int n; size_t last; } rx_t;
static int on_msg(const char *d, size_t n, void *ud) {
  rx_t *r = ud; (void)d; r->n++; r->last = n; return 0;
}

static int stop_cb(const char *d, size_t n, void *u) { (void)d; (void)n; (void)u; return 1; }

typedef struct { intel_sink base; int n; char key[320]; char type[64]; char props[1024]; } cap_sink;
static int cap_emit(intel_sink *s, const intel_item *it) {
  cap_sink *c = (cap_sink *)s;
  c->n++;
  snprintf(c->key, sizeof c->key, "%s", it->remote_key ? it->remote_key : "");
  snprintf(c->type, sizeof c->type, "%s", it->record_type ? it->record_type : "");
  snprintf(c->props, sizeof c->props, "%s", it->properties_json ? it->properties_json : "");
  return 0;
}

int main(void) {
  char chunk[1024]; memset(chunk, 'x', sizeof chunk);
  ws_asm a; rx_t rx = {0}; int delivered = 0;

  /* Ceiling 2048: a 1 KiB message in two chunks is delivered whole. */
  ws_asm_init(&a, 2048);
  assert(ws_asm_feed(&a, chunk, 512, 0, on_msg, &rx, &delivered) == 0);
  assert(ws_asm_feed(&a, chunk, 512, 1, on_msg, &rx, &delivered) == 0);
  assert(delivered == 1 && rx.last == 1024 && a.dropped == 0 && a.dropped_bytes == 0);

  /* A 4-chunk (4096 B) message: dropped whole, never delivered truncated;
   * 1024+1024 were buffered when the third chunk crossed the ceiling, and the
   * third and fourth are discarded: all 4096 bytes are accounted for. */
  rx.n = 0; delivered = 0;
  for (int i = 0; i < 4; i++)
    assert(ws_asm_feed(&a, chunk, 1024, i == 3, on_msg, &rx, &delivered) == 0);
  assert(rx.n == 0 && delivered == 0);
  assert(a.dropped == 1 && a.dropped_bytes == 4096);

  /* The next, small message is unaffected by the drop. */
  assert(ws_asm_feed(&a, chunk, 100, 1, on_msg, &rx, &delivered) == 0);
  assert(delivered == 1 && rx.last == 100);

  /* A second oversize message accumulates into the same counters. */
  for (int i = 0; i < 3; i++)
    assert(ws_asm_feed(&a, chunk, 1024, i == 2, on_msg, &rx, &delivered) == 0);
  assert(a.dropped == 2 && a.dropped_bytes == 4096 + 3072);

  /* An empty frame is not a message; a callback stop is reported. */
  assert(ws_asm_feed(&a, "", 0, 1, on_msg, &rx, &delivered) == 0);
  int d0 = delivered;
  assert(ws_asm_feed(&a, chunk, 10, 1, stop_cb, &rx, &delivered) == 1);
  assert(delivered == d0 + 1);
  ws_asm_free(&a);

  /* The notice: nothing when nothing was dropped, one record when something was. */
  cap_sink cs; memset(&cs, 0, sizeof cs); cs.base.emit = cap_emit;
  jo_ws_oversize_notice_n(&cs.base, "certstream-jp", "wss://example.invalid/", 40, 0, 0, 1u << 24);
  assert(cs.n == 0);
  jo_ws_oversize_notice_n(&cs.base, "certstream-jp", "wss://example.invalid/", 40, 2,
                          7168, 1u << 24);
  assert(cs.n == 1);
  assert(!strcmp(cs.type, "collector-truncation-notice"));
  assert(strstr(cs.key, "certstream-jp|truncation:"));
  assert(strstr(cs.props, "\"messages_dropped\":2"));
  assert(strstr(cs.props, "\"bytes_dropped\":7168"));
  assert(strstr(cs.props, "\"max_message_bytes\":16777216"));
  assert(strstr(cs.props, "\"records_used\":40"));
  assert(strstr(cs.props, "\"records_available\":null"));

  /* ws_collect() clears the counters on entry, so a refused connection after a
   * dropped run cannot leave a stale count for the next collector. */
  g_oversize_dropped = 5; g_oversize_bytes = 99;
  assert(ws_collect("ftp://x/", NULL, 100, 1, on_msg, &rx) == -1);
  assert(ws_last_oversize_dropped() == 0 && ws_last_oversize_dropped_bytes() == 0);

  printf("test_ws_oversize: all passed\n");
  return 0;
}
