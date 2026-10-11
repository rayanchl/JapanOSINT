/* core/hostgate.h — per-upstream-host politeness gate.
 *
 * WHY THIS EXISTS. The scheduler used to run one source at a time, so no two
 * collectors could ever hit the same upstream simultaneously — serialisation
 * was accidental politeness. Running a worker pool removes that guarantee:
 * ~90 sources issue nationwide Overpass queries and would now arrive at
 * overpass-api.de together, which is both rude and self-defeating (the mirror
 * throttles, every one of them times out, and the fleet looks broken).
 *
 * So the concurrency we gain has to be spent ACROSS hosts, not within one.
 * This gate caps in-flight requests per host and enforces a minimum gap
 * between consecutive request starts to the same host.
 *
 * WHY IT LIVES UNDER httpclient AND NOT IN THE SCHEDULER. A registry-metadata
 * bucket (keyed on source_def.url) would miss the cases that matter most:
 * lib/overpass.c rotates over four endpoints of its own, feedlib follows
 * redirects to other hosts, and ~150 sources carry a NULL or `internal://`
 * url. Gating at the point of the actual fetch keys on the host we are
 * REALLY talking to, covers every path (scheduler, on-demand /api/sources
 * triggers, and the OSINT search fan-out) and needs no per-source metadata.
 *
 * FAIL-OPEN, DELIBERATELY. acquire() returns 0 if it waited out its budget
 * without a slot, and the caller proceeds anyway. A politeness gate must
 * never turn into an availability bug: the alternative is synthesising a
 * fetch failure for a source whose upstream was fine, which would feed
 * fetch_log/anomaly_detect and quarantine a healthy source — the exact class
 * of defect the audit spent a session removing.
 *
 * Loopback is exempt: llama-server is reached through this same http_client
 * and a 2-in-flight cap on 127.0.0.1 would throttle the LLM.
 *
 * Tunables (env, read once):
 *   JO_HOST_MAX_CONC    in-flight requests per host   (default 2, 0 = off)
 *   JO_HOST_MIN_GAP_MS  min ms between request starts (default 150)
 *   JO_HOST_MIN_GAP_OVERRIDES  "host=ms,host=ms" per-host gaps that beat the
 *                       global one (built in: reddit.com=30000 — measured
 *                       per-IP floor). Matches the host and its subdomains.
 *                       An override host waits for its gap (up to 5 min)
 *                       instead of failing open: walking into a known 429
 *                       wall is not availability.
 */
#ifndef JO_HOSTGATE_H
#define JO_HOSTGATE_H

#include <stddef.h>

/* Blocks until this URL's host has a free slot and its minimum gap has
 * elapsed, or until max_wait_ms passes. Returns 1 if a slot was taken (the
 * caller MUST pair it with hostgate_release), 0 if it timed out or the host
 * is exempt/unparseable (the caller must NOT release). */
int  hostgate_acquire(const char *url, int max_wait_ms);

/* Releases the slot taken by a hostgate_acquire that returned 1. */
void hostgate_release(const char *url);

/* Observability for the scheduler heartbeat: how many acquisitions failed
 * open — waited out their budget OR found the host table full of live hosts —
 * and how many are in flight right now. */
void hostgate_counters(long *out_waits, long *out_timeouts, long *out_inflight);

/* The same, split by cause, plus table occupancy. */
typedef struct {
  long waits;        /* grants that had to wait                              */
  long timeouts;     /* fail-opens: wait budget exhausted                    */
  long table_full;   /* fail-opens: every slot held a live host              */
  long inflight;
  long hosts;        /* hosts holding a slot now                             */
  long capacity;     /* slots                                                */
  long evictions;    /* idle slots reclaimed for a new host                  */
  long penalties;    /* hostgate_penalize() calls that landed                */
} hostgate_stats_t;
void hostgate_stats(hostgate_stats_t *out);

/* The server told us to back off (Retry-After on a 429/503): no request to
 * `url`'s host starts for `ms` (capped at 60 s), across every caller. Waiters
 * wait it out rather than failing open, as for an override host. */
void hostgate_penalize(const char *url, long ms);

/* ── SSRF destination check ────────────────────────────────────────────────
 * This lives here, next to url_host(), because this file already owns "what
 * host is this URL really talking to" and a second copy of that parse is how
 * the two answers drift apart. Two strengths, deliberately:
 *
 *   hostgate_url_check()        the floor every outbound fetch must clear:
 *                               http(s) only, and never the cloud metadata
 *                               range (169.254.0.0/16, fe80::/10, and the
 *                               metadata hostnames). Loopback and RFC1918 are
 *                               ALLOWED here — llama-server is reached over
 *                               127.0.0.1 through this same http_client.
 *                               (Camera fetches are stricter — see the
 *                               camera policy below.) Set
 *                               JO_HTTP_BLOCK_PRIVATE=1 to raise this to the
 *                               strict check for every collector fetch.
 *
 *   hostgate_url_check_strict() for URLs an API CALLER supplied (alert webhook
 *                               targets, LLM-proposed collector URL
 *                               overrides). No loopback, no RFC1918, no
 *                               link-local, no CGNAT, no unique-local IPv6.
 *                               There is no legitimate reason for a tenant to
 *                               aim our request engine at our own network.
 *
 * Hostnames are resolved and EVERY returned address is checked, so
 * "evil.example.com IN A 127.0.0.1" is rejected too. Returns 0 when allowed,
 * else one of the HG_URL_* codes; hostgate_url_reason() maps that to a short
 * stable string suitable for an API error body. */
#define HG_URL_OK          0
#define HG_URL_BAD_SCHEME (-1)   /* not http:// or https://                 */
#define HG_URL_BAD_HOST   (-2)   /* unparseable, over-long, or unresolvable */
#define HG_URL_PRIVATE    (-3)   /* resolves into a blocked address range   */

int         hostgate_url_check(const char *url);
int         hostgate_url_check_strict(const char *url);

/* The strict ruleset MINUS the DNS lookup: scheme, literal address and
 * metadata hostnames only. For a URL used as a match key rather than as a
 * destination (url_override's `old_url`), where "does not resolve" is the
 * normal state and must not be a rejection. */
int         hostgate_url_check_strict_nodns(const char *url);
const char *hostgate_url_reason(int rc);

/* Classifies one already-resolved address in its textual form ("93.184.216.34",
 * "::1"). `strict` selects the same two strengths as above. This is what the
 * per-connection callback in httpclient.c calls, so a redirect or a rebound DNS
 * answer is judged on the address we actually connected to rather than on the
 * name we started from. Unparseable text is allowed (fail open: the floor check
 * has already run on the URL, and a curl-reported peer we cannot parse is not
 * evidence of an attack). */
int hostgate_addr_check(const char *ip_text, int strict);

/* hostgate_addr_check() at the CURRENT floor, i.e. honouring
 * JO_HTTP_BLOCK_PRIVATE. Use this — not hostgate_addr_check(ip, 0) — anywhere
 * on the outbound fetch path: url_check() can only judge a literal address, so
 * this is the only place a hostname that resolves into a private range is
 * seen, and hardcoding the strength there silently disabled the env switch. */
int hostgate_addr_check_floor(const char *ip_text);

/* Resolve a BARE HOSTNAME (no URL, no scheme) and judge every address it
 * answers with, plus the metadata hostnames. Returns HG_URL_OK or an HG_URL_*
 * code, same as the url_check family.
 *
 * This exists for the collectors that open RAW SOCKETS rather than going
 * through core/httpclient.c — port_scanner, ssl_analyzer and email_validator
 * call socket()/connect() directly, so none of the protection inside
 * http_request() reaches them. They take ctx->entity, which on the
 * /api/search pivot path is caller-supplied text, so `strict` is the right
 * strength there: a tenant has no legitimate reason to aim our socket layer at
 * our own network. Call it AFTER resolution is needed and BEFORE connect(). */
int hostgate_host_check(const char *host, int strict);

/* ── camera / media destinations ───────────────────────────────────────────
 * Camera URLs are NOT ours: shodan_api, insecam_scrape and friends write
 * whatever their upstream reported into intel_items.properties, and the camera
 * proxy, the stills pod and ffmpeg then dial it on behalf of any signed-in
 * user. At the floor strength that let one poisoned record aim the server at
 * 127.0.0.1 or the operator's LAN, with the proxy's "upstream <status>" error
 * text acting as a port/status oracle. So these paths use the STRICT ruleset
 * unless the operator opts in with JO_CAMERA_ALLOW_LAN=1 (LAN cameras are a
 * real deployment, just not a default one). Metadata and link-local stay
 * refused either way; JO_HTTP_BLOCK_PRIVATE=1 forces strict regardless.
 *
 * hostgate_camera_url_check() takes any "scheme://host" (rtsp, rtmp, tcp …) —
 * the CALLER pins the scheme set. `resolve` = 1 also resolves the name and
 * judges every answer: for ffmpeg, which owns its socket, that is the only
 * resolved-address check there is. curl paths pass 0 and rely on
 * hostgate_camera_addr_check() in their CURLOPT_PREREQFUNCTION. */
int hostgate_camera_lan_allowed(void);
int hostgate_camera_url_check(const char *url, int resolve);
int hostgate_camera_addr_check(const char *ip_text);

/* Textual (lowercased, port/userinfo/brackets stripped) host of `url` into
 * out[], including loopback — url_host()'s politeness exemption is NOT applied.
 * Returns 1 on success, 0 if there is no parseable host. */
int hostgate_url_host(const char *url, char *out, size_t cap);

/* True when `a` and `b` have the same (lowercased) host. Used to decide
 * whether a URL rewrite may keep carrying the original request's credentials.
 * A NULL or unparseable side is never "same". */
int hostgate_same_host(const char *a, const char *b);

#endif
