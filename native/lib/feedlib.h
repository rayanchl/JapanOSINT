/* lib/feedlib.h — shared source toolkit. P4: HTTP-GET-JSON. RSS/Atom, GTFS-RT
 * (protobuf-c), HTML scrape, CSV are added here as P5 waves need them — each
 * source.c stays tiny by leaning on this. */
#ifndef JO_FEEDLIB_H
#define JO_FEEDLIB_H
#include "../third_party/cJSON.h"
#include "../core/httpclient.h"

/* GET url, parse JSON. Returns cJSON* (caller cJSON_Delete) or NULL. */
cJSON *feed_get_json(http_client *http, const char *url, int timeout_ms);

/* Like feed_get_json but with extra request headers (NULL-terminated array of
 * "Key: value" strings, or NULL). For API-key'd JSON endpoints. */
cJSON *feed_get_json_h(http_client *http, const char *url,
                       const char *const *headers, int timeout_ms);

/* What this thread's most recent feed_get_json / feed_get_json_h /
 * feed_post_json call met, for a caller that got NULL back and must say WHY:
 * the HTTP status, FEED_ST_TRANSPORT when the exchange never completed, or
 * FEED_ST_UNPARSED for a 2xx whose body was not JSON. FEED_ST_UNKNOWN after
 * feed_last_json_status_reset() until one of those calls records something,
 * so a page walker whose fetch callback does not go through them can tell
 * "unknown" from a real status. Per thread: collectors run concurrently. */
#define FEED_ST_UNKNOWN   (-1L)
#define FEED_ST_TRANSPORT   0L
#define FEED_ST_UNPARSED  (-2L)
/* The upstream answered HTTP 404 with its OWN "nothing matched" wording — an
 * honest empty, not a refusal (see feed_status_body_is_declared_empty below).
 * A 204 is NOT folded into this code: it is unambiguous by itself, and an
 * existing caller (collectors/sources/geohazard_extra.c) already keys on the
 * raw 204, so it is still reported as 204. Ask feed_status_declared_empty()
 * rather than comparing against either value. Added 2026-11-07. */
#define FEED_ST_DECLARED_EMPTY (-3L)
long feed_last_json_status(void);
void feed_last_json_status_reset(void);
/* "HTTP 304", "a transport failure", "an HTTP 200 whose body was not JSON",
 * "an unrecorded failure" — the words a truncation notice uses for `st`. */
void feed_status_describe(long st, char *out, size_t cap);

/* THE UPSTREAM'S OWN "NOTHING MATCHED", as one rule for every engine.
 *
 * FDSN's `nodata` parameter names the status a service must use for an empty
 * result, and every FDSN row in this tree passes `nodata=404`: those rows ASK
 * to be told "nothing" with a 404. lib/hpengine.c has read that as an honest
 * empty since 2026-10-20 (hp_status_is_declared_empty, hptest 46-46d), but
 * feed_get_json_h() returned NULL for it, so jsonlist_emit_paged(),
 * geojson_emit_paged() and pw_walk() returned -1 and a quiet FDSN window on a
 * VJSON/VGEO row filed a fetch-failure — the very confusion that notice exists
 * to prevent, in the engines that never got the fix. Found 2026-11-07 by an
 * audit that read every copy of the decision.
 *
 * The rule is hpengine's, byte for byte, and so are its controls: a 204 is
 * empty by definition; a 404 is empty ONLY when its BODY carries the
 * service's no-data wording ("no matching", "no data available",
 * case-insensitive). `ws.icgc.cat` answers its empty event query that way and
 * answers the identical query with 204 when `nodata` is not forced.
 * `eida.koeri.boun.edu.tr` answers the SAME 404 to the same row with "The
 * requested resource does not exist on this server" — a node that does not run
 * the service — and that must stay a failure, which is why the status alone
 * and the row's URL are never the discriminator.
 *
 * static inline, in the header, so lib/hpengine.c can share it without a link
 * dependency on this file (hptest links hpengine.c without feedlib.c). */
static inline int feed_ci_contains(const char *hay, const char *needle) {
  if (!hay || !needle || !*needle) return 0;
  for (; *hay; hay++) {
    const char *h = hay, *n = needle;
    while (*h && *n) {
      char a = *h, b = *n;
      if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
      if (a != b) break;
      h++; n++;
    }
    if (!*n) return 1;
  }
  return 0;
}
static inline int feed_status_body_is_declared_empty(long status, const char *body) {
  if (status == 204) return 1;                  /* no content, by definition */
  if (status != 404 || !body) return 0;
  return feed_ci_contains(body, "no matching") ||
         feed_ci_contains(body, "no data available");
}
/* Is a status recorded by feed_last_json_status() an honest empty? */
static inline int feed_status_declared_empty(long st) {
  return st == 204 || st == FEED_ST_DECLARED_EMPTY;
}

/* GET url, return raw body (malloc'd, caller frees) or NULL. */
char *feed_get_text(http_client *http, const char *url, int timeout_ms);

/* feed_get_text with a response-body ceiling for this one fetch (0 = the
 * default). For a row whose complete answer is one file larger than the 64 MB
 * security default — see http_request_max() in core/httpclient.h. */
char *feed_get_text_max(http_client *http, const char *url, int timeout_ms,
                        size_t max_body);

/* POST `body` to url with extra headers (NULL-terminated "K: v" array, set
 * Content-Type there), parse JSON reply. cJSON* (caller cJSON_Delete) or
 * NULL. Backs the POST+JSON-body collectors (osv-dev, quake360-jp, …). */
cJSON *feed_post_json(http_client *http, const char *url, const char *body,
                      const char *const *headers, int timeout_ms);

/* Port of intelHelpers.intelHashKey: sha1( for each non-null part: part,"|" )
 * → first 20 hex chars into out (>=21 bytes). NULL parts skipped. */
void feed_hash_key(char *out21, const char *const *parts, int n);

/* The other joined-SHA-1 shape: separator only BETWEEN parts, full 40 hex
 * chars into out (>=41 bytes). station_clusterer's cluster_uid hashes
 * member_uids.join('|'), which is a different input string from
 * feed_hash_key's trailing-pipe form — the two are not interchangeable. */
void feed_hash_join(char *out41, const char *const *parts, int n);

/* Is `url`'s host under .jp? The one gate on "read a non-UTF-8 body as
 * Shift_JIS". Exported (rather than static in feedlib.c) because jocore.h's
 * jo_get() has the identical decision to make for 133 collectors, and two
 * copies of a host test is how the two answers drift. Port, userinfo, path and
 * query are all handled; `something.jp.evil.com` is NOT a match. */
int feed_url_host_is_jp(const char *url);

#endif
