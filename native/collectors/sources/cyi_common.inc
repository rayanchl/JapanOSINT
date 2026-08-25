/* cyi_common.inc — shared helpers for the cyi_* cyber-infrastructure
 * collectors. Not a translation unit: it is #included by the cyi_*.c sources
 * (the build globs *.c, so this file is never compiled on its own), the same
 * arrangement as geoeo_common.inc / av_common.inc / trn_common.inc.
 *
 * WHY IT EXISTS. Twelve cyi_* files each carried a byte-for-byte identical
 * `static char *http_get(const source_ctx *, const char *, long *)` — a plain
 * GET that hands back the body and the HTTP status so the caller can
 * distinguish "the upstream said no such object" (4xx → 0 rows, R3) from "the
 * fetch broke" (→ -1). The bodies differed in exactly three ways, none of them
 * behavioural: the timeout (15s/20s/25s/30s), whether an
 * `Accept: application/json` header went out, and whether the caller passed
 * its own header list. Twelve copies is twelve places for the next fix to
 * land in one of.
 *
 * `cyi_get()` is the superset: headers and timeout are parameters, so every
 * former call site keeps its own values and no request on the wire changes.
 *
 * Everything here is `static inline` on purpose: a file that uses only one of
 * these still compiles warning-free under -Wall -Wextra (an unused
 * `static inline` is not diagnosed, an unused plain `static` is).
 *
 * The includer must already have source.h and core/httpclient.h.
 */
#ifndef CYI_COMMON_INC
#define CYI_COMMON_INC

#include <stdlib.h>

/* GET `url` and return the malloc'd body on HTTP 200 (caller frees), else
 * NULL. `*status` always receives the HTTP status — 0 when the request never
 * reached a response, which is what lets a caller apply R3 (a 4xx is "no such
 * object", 0 rows; anything else is a real failure, -1). `hdrs` may be NULL.
 *
 * Ownership note, because all twelve copies got this right and it is easy to
 * get wrong when re-deriving: the body is STOLEN out of the response
 * (`hr.body = NULL`) before http_response_free(), so freeing the response does
 * not free the buffer we are returning. */
static inline char *cyi_get(const source_ctx *ctx, const char *url,
                            const char *const *hdrs, int timeout_ms,
                            long *status) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, timeout_ms, 1, &hr);
  if (status) *status = hr.status;
  if (rc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  char *b = hr.body; hr.body = NULL; http_response_free(&hr);
  return b;
}

/* The commonest shape: no caller headers, 20-second timeout. */
static inline char *cyi_get_plain(const source_ctx *ctx, const char *url,
                                  long *status) {
  return cyi_get(ctx, url, NULL, 20000, status);
}

/* Same, announcing JSON. Several of these endpoints content-negotiate. */
static inline char *cyi_get_json(const source_ctx *ctx, const char *url,
                                 int timeout_ms, long *status) {
  static const char *const hdrs[] = { "Accept: application/json", NULL };
  return cyi_get(ctx, url, hdrs, timeout_ms, status);
}

#endif /* CYI_COMMON_INC */
