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

/* Observability for /api/status: how many acquisitions timed out (i.e. how
 * often we exceeded a host's budget) and how many are in flight right now. */
void hostgate_counters(long *out_waits, long *out_timeouts, long *out_inflight);

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
 *                               127.0.0.1 through this same http_client, and
 *                               LAN cameras are a shipped feature. Set
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

/* Textual (lowercased, port/userinfo/brackets stripped) host of `url` into
 * out[], including loopback — url_host()'s politeness exemption is NOT applied.
 * Returns 1 on success, 0 if there is no parseable host. */
int hostgate_url_host(const char *url, char *out, size_t cap);

/* True when `a` and `b` have the same (lowercased) host. Used to decide
 * whether a URL rewrite may keep carrying the original request's credentials.
 * A NULL or unparseable side is never "same". */
int hostgate_same_host(const char *a, const char *b);

#endif
