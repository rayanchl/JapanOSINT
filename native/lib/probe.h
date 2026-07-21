/* lib/probe.h — port of the OTHER "portal-status" collectors: a `fetchHead`
 * reachability probe that emits ONE intel item (NOT zero rows — they surface
 * a reachable/unreachable status row in the Sources/intel views). ~45 of the
 * 62 OTHER ids follow this exact shape. Each becomes a tiny source.c that
 * probes a URL, builds its specific tags/properties, and emits one item. */
#ifndef JO_PROBE_H
#define JO_PROBE_H
#include "../core/httpclient.h"
#include <stddef.h>

/* fetchHead(url): HEAD request; 1 if the host answered 2xx or 3xx, 0 for any
 * error/timeout/4xx/5xx (exact _liveHelpers.fetchHead semantics). */
int  probe_head(http_client *http, const char *url);

/* new Date().toISOString() → "YYYY-MM-DDTHH:MM:SS.sssZ" into out (>=25). */
void probe_iso_now(char *out, size_t n);

#endif
