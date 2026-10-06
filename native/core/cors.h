/* core/cors.h — cross-origin access, by explicit allow-list only.
 *
 * WHY. The web client can be served from a different origin than the API
 * (client/src/utils/apiUrl.js: VITE_API_HOST, e.g. a CDN-hosted SPA talking
 * to api.example.com). Every API call it makes carries `Authorization` and
 * `X-Tenant-Id`, so the browser sends an OPTIONS preflight first — and this
 * server had no OPTIONS handling: the preflight reached the auth gate, got a
 * 401, and the browser blocked every authenticated request. Meanwhile every
 * JSON reply carried `Access-Control-Allow-Origin: *`, which let any page on
 * the web read the unauthenticated routes and helped no real deployment.
 *
 * CONFIG. JO_CORS_ORIGINS is a comma-separated list of exact origins:
 *     JO_CORS_ORIGINS=https://app.example.com,https://staging.example.com
 * An entry is scheme://host[:port] — no path, no wildcard. `*` and `null`
 * are refused at load (logged): a wildcard cannot express "this client and no
 * other", and `null` is what sandboxed iframes and file:// pages send.
 *
 * BEHAVIOUR.
 *   unset/empty   no CORS headers on any response, OPTIONS is not special —
 *                 the same-origin deployment (the server serves the SPA, or
 *                 Vite's dev proxy) needs none.
 *   set           a request whose Origin is on the list gets that origin
 *                 reflected in Access-Control-Allow-Origin; every response
 *                 says `Vary: Origin` so a shared cache never hands one
 *                 origin's answer to another. OPTIONS is answered before the
 *                 auth gate: 204 with the allowed methods/headers for a listed
 *                 origin, 403 for an unlisted one.
 *
 * No Access-Control-Allow-Credentials, ever. The client authenticates with a
 * bearer header, not cookies, and this server reads no cookie; credentialed
 * CORS would only widen what a listed origin could do. */
#ifndef JO_CORS_H
#define JO_CORS_H
#include <stddef.h>

#define CORS_MAX_ORIGINS 16
#define CORS_ORIGIN_MAX  256
/* Upper bound of cors_headers()' output, for callers composing header blocks. */
#define CORS_HDR_MAX     (CORS_ORIGIN_MAX + 256)

/* (Re)read JO_CORS_ORIGINS. Called once by httpd before it listens; safe to
 * call again (tests do). Returns the number of origins accepted. */
int cors_init(void);

/* 1 when at least one origin is configured. */
int cors_enabled(void);

/* The verdict for one request's Origin header (`origin` may be NULL):
 *   0      CORS is off — send nothing
 *   1      CORS is on, this request's origin is absent or not listed
 *   2 + k  listed origin k
 * Fits in one byte, so httpd keeps it in the connection's scratch and a reply
 * delivered later (a worker's wakeup) still knows which origin asked. */
unsigned char cors_verdict(const char *origin, size_t len);

/* Header lines ("Name: value\r\n"…) every response for this verdict carries:
 * "" for 0, "Vary: Origin\r\n" for 1, and the reflected origin plus Vary and
 * Access-Control-Expose-Headers for a listed origin. Never NULL. */
const char *cors_headers(unsigned char verdict);

/* The complete response to an OPTIONS request with this verdict (status line,
 * headers, body): 204 + the allowed methods/headers for a listed origin, 403
 * for an unlisted one, and a plain 204 + Allow when the request carried no
 * Origin (not a browser preflight). Only meaningful when cors_enabled().
 * Never NULL. */
const char *cors_preflight_response(unsigned char verdict, int has_origin,
                                    int *status);

#endif
