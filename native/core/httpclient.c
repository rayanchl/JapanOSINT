#include "httpclient.h"
#include "catrust.h"
#include "url_override.h"
#include "evidence.h"
#include "content_change.h"
#include "hostgate.h"
#include "../lib/jocore.h"   /* jo_strcasestr: strcasestr needs _GNU_SOURCE */
#include <stdio.h>
#include <curl/curl.h>
/* For the X509_V_ERR_* codes only — the verify result is how a TLS failure is
 * told apart from the three others it is routinely confused with (see
 * http_last_tls_cause in httpclient.h). OpenSSL's headers are always present
 * here; core/catrust.c says why. */
#include <openssl/x509.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

typedef struct { char *host; int requests; int ok; } host_log;

struct http_client { CURLSH *share; host_log *hosts; int n_hosts, cap_hosts;
                     unsigned long long bytes_in; int requests; };

/* Record one request's host (parsed from the effective URL) + outcome. */
static void log_host(http_client *c, const char *url, long status, int hard) {
  if (!c || !url) return;
  const char *p = strstr(url, "://");
  p = p ? p + 3 : url;
  size_t n = 0;
  while (p[n] && p[n] != '/' && p[n] != ':' && p[n] != '?' && p[n] != '#') n++;
  if (n == 0 || n >= 256) return;
  char host[256];
  for (size_t i = 0; i < n; i++) host[i] = (char)tolower((unsigned char)p[i]);
  host[n] = 0;
  int ok = (!hard && status >= 200 && status < 400) ? 1 : 0;
  for (int i = 0; i < c->n_hosts; i++) {
    if (strcmp(c->hosts[i].host, host) == 0) {
      c->hosts[i].requests++;
      if (ok) c->hosts[i].ok = 1;
      return;
    }
  }
  if (c->n_hosts >= c->cap_hosts) {
    int nc = c->cap_hosts ? c->cap_hosts * 2 : 4;
    host_log *q = realloc(c->hosts, (size_t)nc * sizeof *q);
    if (!q) return;
    c->hosts = q; c->cap_hosts = nc;
  }
  c->hosts[c->n_hosts].host = strdup(host);
  c->hosts[c->n_hosts].requests = 1;
  c->hosts[c->n_hosts].ok = ok;
  c->n_hosts++;
}

void http_client_test_log_host(http_client *c, const char *url, long status) {
  log_host(c, url, status, 0);
}

int http_client_host_count(http_client *c) { return c ? c->n_hosts : 0; }

const char *http_client_host_at(http_client *c, int i,
                                int *out_requests, int *out_ok) {
  if (!c || i < 0 || i >= c->n_hosts) return NULL;
  if (out_requests) *out_requests = c->hosts[i].requests;
  if (out_ok)       *out_ok       = c->hosts[i].ok;
  return c->hosts[i].host;
}

/* Hard ceiling on a single response body. CURLOPT_ACCEPT_ENCODING "" asks for
 * compression and libcurl INFLATES before on_data ever sees a byte, so a 1 MB
 * gzip bomb expands to tens of GB inside this buffer — an upstream we do not
 * control could OOM the process at will. curl's own CURLOPT_MAXFILESIZE only
 * looks at the advertised Content-Length, which a bomb does not advertise, so
 * the ceiling has to be counted here on the decompressed stream.
 * JO_HTTP_MAX_BODY overrides (bytes); 0 disables. */
#define HTTP_MAX_BODY_DEFAULT (64u * 1024u * 1024u)

/* pthread_once, not the `static int done` flag this used to carry with the
 * comment "racy-but-idempotent: same value either way".
 *
 * It is not idempotent, and ThreadSanitizer says so: eight scheduler workers
 * calling http_request() at once race on both `done` (4-byte) and `cached`
 * (8-byte) — the reads at the return are unordered against another thread's
 * write, which is UB, and a reader that observes done==1 before the store to
 * `cached` is visible gets a ceiling of 0. Zero is the DISABLE value for
 * on_data()'s check (`if (b->cap && ...)`), so the one thing this cap exists
 * to stop — an unbounded decompressed body — would be let through. Same
 * idiom as curl_boot() below; the probe is one getenv, so the once costs
 * nothing after the first call. */
static size_t g_max_body;
/* Whether JO_HTTP_MAX_BODY was SET, as opposed to defaulted. An operator who
 * names a ceiling has stated a policy, and a per-call override must not walk
 * through it — see body_ceiling(). */
static int g_max_body_explicit;
static void max_body_probe(void) {
  const char *e = getenv("JO_HTTP_MAX_BODY");
  long long v = (e && *e) ? atoll(e) : -1;
  g_max_body_explicit = (v >= 0);
  g_max_body = (v >= 0) ? (size_t)v : (size_t)HTTP_MAX_BODY_DEFAULT;
}

static size_t max_body_bytes(void) {
  static pthread_once_t once = PTHREAD_ONCE_INIT;
  pthread_once(&once, max_body_probe);
  return g_max_body;
}

/* A ceiling for ONE request on THIS thread, set by http_request_max() and
 * cleared when that call returns. 0 means "use the default above".
 *
 * Why a per-call ceiling exists at all: the 64 MB default is a security bound
 * on an upstream we do not control, and a handful of rows legitimately need
 * more. `ods-public-geonames` is the measured case — GeoNames cities with
 * population >= 1,000 is 171,176 records, the ODS records API refuses
 * offset+limit past 10,000, and the only complete answer is the ~111 MB
 * /exports/jsonl file. Raising the DEFAULT to reach it would lift the bomb
 * guard for all 48,343 sources to serve one; this raises it for that one row,
 * for that one fetch.
 *
 * It is thread-local for the same reason g_last_te is: eight scheduler workers
 * are inside http_request() at once, and a process-wide variable would let one
 * row's generous ceiling apply to another row's fetch — exactly the bound this
 * cap exists to hold. The ceiling still applies on the DECOMPRESSED stream in
 * on_data(), so a row that asks for 192 MB still cannot be fed tens of GB. */
static __thread size_t g_body_ceiling_once;

/* An operator's EXPLICIT $JO_HTTP_MAX_BODY is a hard cap, and a per-call
 * override is clamped to it rather than silently exceeding it. Without this,
 * an operator who lowers the ceiling to 16 MB to bound a small deployment's
 * memory would still have one collector pull 192 MB, with nothing anywhere
 * saying so — the setting would read as honoured and not be. The override
 * raises the DEFAULT, which is what it is for; it does not overrule a policy
 * somebody set on purpose. */
static size_t body_ceiling(void) {
  size_t base = max_body_bytes();
  if (!g_body_ceiling_once) return base;
  if (g_max_body_explicit) {
    if (base == 0) return g_body_ceiling_once;  /* 0 = the operator disabled it */
    return g_body_ceiling_once < base ? g_body_ceiling_once : base;
  }
  return g_body_ceiling_once;
}

/* Strict mode for ONE request on THIS thread, set by http_request_strict() and
 * restored when it returns — same shape and same thread-local reasoning as
 * g_body_ceiling_once. It is for a destination somebody OTHER than the
 * collector author chose (an alert webhook, an image URL scraped from an
 * upstream item): the strict address ruleset applies to the URL and to the
 * connected address, and redirects are not followed at all, so a 302 cannot
 * walk the request somewhere the caller never judged. */
static __thread int g_strict_once;
/* With g_strict_once: 1 = redirects ARE followed (each hop re-judged by the
 * strict per-connection check), 0 = none followed. */
static __thread int g_strict_follow;

/* Where the last do_once() on this thread was told to go next, when it was
 * asked not to follow redirects (CURLINFO_REDIRECT_URL, always absolute). */
static __thread char g_redirect_url[2048];

typedef struct { char *buf; size_t len, cap; int over; } sbuf;

static size_t on_data(void *ptr, size_t sz, size_t nm, void *ud) {
  size_t n = sz * nm;
  sbuf *b = (sbuf *)ud;
  if (b->cap && b->len + n > b->cap) {
    /* Returning short aborts the transfer with CURLE_WRITE_ERROR, which
     * do_once() already treats as a hard failure. Flagged so the log says
     * "too big" rather than blaming the network. */
    b->over = 1;
    return 0;
  }
  char *p = realloc(b->buf, b->len + n + 1);
  if (!p) return 0;
  b->buf = p;
  memcpy(b->buf + b->len, ptr, n);
  b->len += n;
  b->buf[b->len] = '\0';
  return n;
}

/* Per-connection SSRF re-check. libcurl calls this after the socket is
 * connected and before the request goes out — once per hop, so a 302 into
 * 169.254.169.254 (or a DNS answer that rebinds between our check and curl's
 * own resolve) is caught here rather than being followed. */
#if LIBCURL_VERSION_NUM >= 0x075000            /* 7.80.0: CURLOPT_PREREQFUNCTION */
static int on_prereq(void *ud, char *conn_primary_ip, char *conn_local_ip,
                     int conn_primary_port, int conn_local_port) {
  (void)conn_local_ip; (void)conn_primary_port; (void)conn_local_port;
  /* The FLOOR as configured, not a hardcoded 0: with JO_HTTP_BLOCK_PRIVATE=1
   * the operator asked for RFC1918/loopback to be refused, and a hostname is
   * only ever caught here (url_check() sees a name, not an address).
   * A non-NULL `ud` is http_request_strict(): the strict ruleset whatever the
   * floor is set to, so a name that rebinds to loopback/RFC1918 after the
   * caller's own check is refused here. */
  int bad = ud ? hostgate_addr_check(conn_primary_ip, 1)
               : hostgate_addr_check_floor(conn_primary_ip);
  if (bad != HG_URL_OK) {
    fprintf(stderr, "[http] blocked connection to %s (private/link-local)\n",
            conn_primary_ip ? conn_primary_ip : "?");
    return CURL_PREREQFUNC_ABORT;
  }
  return CURL_PREREQFUNC_OK;
}
#endif

void http_easy_harden(void *curl_easy) {
  CURL *e = (CURL *)curl_easy;
  if (!e) return;
  curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(e, CURLOPT_MAXREDIRS, 5L);
  /* Pin the protocol set on BOTH the initial request and the redirect chain.
   * Without REDIR_PROTOCOLS a 302 can walk an http(s) fetch into file:// or
   * scp:// — libcurl's redirect default has historically been permissive. */
#if LIBCURL_VERSION_NUM >= 0x075500            /* 7.85.0 */
  curl_easy_setopt(e, CURLOPT_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(e, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
  curl_easy_setopt(e, CURLOPT_PROTOCOLS,
                   (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
  curl_easy_setopt(e, CURLOPT_REDIR_PROTOCOLS,
                   (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
#if LIBCURL_VERSION_NUM >= 0x075000
  curl_easy_setopt(e, CURLOPT_PREREQFUNCTION, on_prereq);
  curl_easy_setopt(e, CURLOPT_PREREQDATA, (void *)0);
#endif
  /* Supply the intermediates nine upstreams omit from their own handshake.
   * This ADDS links to the verification store; it cannot add a trust anchor
   * and does not touch VERIFYPEER or VERIFYHOST. See core/catrust.h. */
  catrust_install(e);
}

/* usleep() takes a 32-bit useconds_t and POSIX allows EINVAL past one second;
 * a 30 s Retry-After is 3e10 µs. */
static void sleep_ms(long ms) {
  if (ms <= 0) return;
  /* In slices, so a cancelled run does not sleep out a 30 s Retry-After. */
  while (ms > 0 && !hostgate_cancelled()) {
    long step = ms > 200 ? 200 : ms;
    struct timespec ts = { (time_t)(step / 1000), (long)(step % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
    ms -= step;
  }
}

/* Header names that must not follow a URL rewrite onto a different host. */
static int header_is_credential(const char *h) {
  static const char *N[] = { "authorization:", "proxy-authorization:",
                             "cookie:", "x-api-key:", "api-key:",
                             "x-auth-token:", "x-access-token:", NULL };
  for (int i = 0; N[i]; i++) {
    size_t l = strlen(N[i]);
    if (strncasecmp(h, N[i], l) == 0) return 1;
  }
  /* Anything shaped "<something>-key: " / "<something>-token: " too. */
  const char *colon = strchr(h, ':');
  if (!colon) return 0;
  char name[128];
  size_t nl = (size_t)(colon - h);
  if (nl >= sizeof name) return 0;
  for (size_t i = 0; i < nl; i++) name[i] = (char)tolower((unsigned char)h[i]);
  name[nl] = 0;
  return strstr(name, "token") || strstr(name, "secret") ||
         strstr(name, "apikey") || strstr(name, "auth") ? 1 : 0;
}

static void curl_boot(void) { curl_global_init(CURL_GLOBAL_DEFAULT); }

/* pthread_once rather than a plain static flag: clients are created from
 * collector worker threads, and two of them hitting an unguarded flag can both
 * run curl_global_init(), which is not re-entrant. */
void http_client_global_init(void) {
  static pthread_once_t once = PTHREAD_ONCE_INIT;
  pthread_once(&once, curl_boot);
}

http_client *http_client_new(void) {
  http_client_global_init();
  http_client *c = calloc(1, sizeof *c);
  if (!c) return NULL;
  c->share = curl_share_init();
  if (c->share) {
    curl_share_setopt(c->share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(c->share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    curl_share_setopt(c->share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
  }
  return c;
}

void http_client_free(http_client *c) {
  if (!c) return;
  if (c->share) curl_share_cleanup(c->share);
  for (int i = 0; i < c->n_hosts; i++) free(c->hosts[i].host);
  free(c->hosts);
  free(c);
}

/* PER-HOST USER-AGENT OVERRIDES.
 *
 * JO_USER_AGENT is deliberately descriptive and contactable, which is the
 * documented remedy for a host that blocklists anonymous clients (httpclient.h
 * records the ReliefWeb case). A few hosts have the opposite policy: their bot
 * filter matches on a SUBSTRING of the agent and refuses anything containing
 * it, regardless of how honest the rest of the string is.
 *
 * data.humdata.org (OCHA HDX) is one. Measured against
 * /api/3/action/package_search:
 *
 *     "JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; …)"  406
 *     "JapanOSINT/1.0"                                            406
 *     "OSINT/1.0"                                                 406
 *     "Japan"                                                     200
 *     "curl/8.5.0"                                                200
 *     no User-Agent at all                                        200
 *
 * The blocked token is "OSINT" — which our repo URL also contains, so even
 * renaming the product was not enough on its own. The response is
 * 406 {"error":"Blocked due to bot activity."}, so all 20 afr-hdx-* sources
 * fetched nothing on every scheduled run.
 *
 * The override is therefore NOT browser spoofing, which httpclient.h rules out
 * and which is not needed here anyway: the replacement still names the client,
 * still says what it is, and still offers a contact route. It only avoids the
 * one token their filter rejects.
 *
 * Scoped per host on purpose. Changing JO_USER_AGENT globally would silently
 * re-open every source verified under the current agent — batch 19's 1,692
 * rows were re-probed specifically "under the engine's own request conditions"
 * — so a host-specific problem gets a host-specific answer.
 *
 * Since 2026-10-21 every per-host agent lives in core/host_agents.def, and a
 * listed host's agent wins over a caller-supplied "User-Agent:" (the request
 * builder below drops the caller's), so a row's own header1= now applies to
 * hosts NOT in the table only. */
/* THE per-host agent table: core/host_agents.def, also read by
 * tools/probe_hp_batch.py. One X-macro file, three expansions. */
typedef enum {
#define HOST_AGENT_KIND(k, s) k,
#define HOST_AGENT(h, k)
#include "core/host_agents.def"
#undef HOST_AGENT
#undef HOST_AGENT_KIND
  HA__COUNT
} host_agent_kind;

static const char *const HOST_AGENT_STRING[HA__COUNT] = {
#define HOST_AGENT_KIND(k, s) [k] = s,
#define HOST_AGENT(h, k)
#include "core/host_agents.def"
#undef HOST_AGENT
#undef HOST_AGENT_KIND
};

static const char *const HOST_AGENT_NAME[HA__COUNT] = {
#define HOST_AGENT_KIND(k, s) [k] = #k,
#define HOST_AGENT(h, k)
#include "core/host_agents.def"
#undef HOST_AGENT
#undef HOST_AGENT_KIND
};

static const struct { const char *host; host_agent_kind kind; } HOST_AGENTS[] = {
#define HOST_AGENT_KIND(k, s)
#define HOST_AGENT(h, k) { h, k },
#include "core/host_agents.def"
#undef HOST_AGENT
#undef HOST_AGENT_KIND
};

/* Sent beside HA_BROWSER's agent: www.bls.gov needs the Sec-Fetch-* headers,
 * not just the agent. The caller's own Accept is kept — the NOAA hosts answer
 * an HTML Accept with a 202 challenge page. */
static const char *const BROWSER_EXTRA_HEADERS[] = {
  "Accept-Language: en-US,en;q=0.9",
  "Sec-Fetch-Dest: document",
  "Sec-Fetch-Mode: navigate",
  "Sec-Fetch-Site: none",
  "Upgrade-Insecure-Requests: 1",
  NULL };

/* The entry for `url`'s host, or -1. Exact host match on the authority
 * without :port: "data.humdata.org.evil.example" must not inherit an entry,
 * and neither may a path that merely contains a listed name. */
static int host_agent_index(const char *url) {
  if (!url) return -1;
  const char *h = strstr(url, "://");
  if (!h) return -1;
  h += 3;
  size_t n = strcspn(h, "/?#");
  const char *colon = memchr(h, ':', n);
  if (colon) n = (size_t)(colon - h);
  for (size_t i = 0; i < sizeof HOST_AGENTS / sizeof *HOST_AGENTS; i++)
    if (strlen(HOST_AGENTS[i].host) == n && strncasecmp(h, HOST_AGENTS[i].host, n) == 0)
      return (int)i;
  return -1;
}

/* ── SEC_CONTACT: a credential the User-Agent is built from ──────────────────
 * HA_SEC's table string carries a %CONTACT% placeholder. The value comes from
 * $SEC_CONTACT_UA (a full agent string, kept for compatibility, wins) else
 * $SEC_CONTACT — an address or a full agent string — which keysapi mirrors
 * from data/api-keys.json into the environment at boot and on every PUT, so
 * the Keys screen and the environment resolve through one getenv. */
int http_contact_check(const char *v, const char **why) {
  static const char *w;
  if (!why) why = &w;
  if (!v || !*v) { *why = "empty"; return -1; }
  size_t n = strlen(v);
  if (n > 256) { *why = "longer than 256 characters"; return -1; }
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)v[i];
    if (c < 0x20 || c == 0x7f) {
      *why = "contains a control character (CR/LF would inject a header)";
      return -1;
    }
  }
  for (size_t i = 0; i < n; i++) {
    if (v[i] != '@') continue;
    size_t l = i, r = i + 1;
    while (l > 0 && (isalnum((unsigned char)v[l - 1]) || strchr("._%+-", v[l - 1]))) l--;
    while (r < n && (isalnum((unsigned char)v[r]) || strchr(".-", v[r]))) r++;
    if (l == i) continue;                       /* no local part */
    const char *dom = v + i + 1;
    size_t dl = r - (i + 1);
    const char *dot = dl ? memchr(dom, '.', dl) : NULL;
    if (dl >= 3 && dot && dot != dom && dom[dl - 1] != '.') return 0;
  }
  *why = "must contain an e-mail address (user@example.org) - SEC refuses a "
         "User-Agent without one";
  return -1;
}

static const char *trim_span(const char *v, size_t *len) {
  while (*v && isspace((unsigned char)*v)) v++;
  size_t n = strlen(v);
  while (n && isspace((unsigned char)v[n - 1])) n--;
  *len = n;
  return v;
}

int http_contact_ua(char *buf, size_t cap, const char **why) {
  static const char *w;
  if (!why) why = &w;
  const char *full = getenv("SEC_CONTACT_UA"), *val = getenv("SEC_CONTACT");
  int from_full = full && *full;
  const char *src = from_full ? full : val;
  if (!src || !*src) { *why = "SEC_CONTACT is not configured"; return -1; }
  size_t n; const char *t = trim_span(src, &n);
  char tmp[300];
  if (n == 0 || n >= sizeof tmp) { *why = n ? "longer than 256 characters" : "empty"; return -1; }
  memcpy(tmp, t, n); tmp[n] = 0;
  if (http_contact_check(tmp, why) != 0) return -1;
  int bare = !from_full && !strpbrk(tmp, " \t()/");
  if (bare) snprintf(buf, cap, "JapanOSINT/1.0 research (%s)", tmp);
  else snprintf(buf, cap, "%s", tmp);
  return 0;
}

static __thread char g_ua_buf[400];
static __thread const char *g_cred_refusal;

/* The credential a kind needs before it may be used, or NULL. */
static const char *host_agent_credential(host_agent_kind k) {
  return k == HA_SEC ? "SEC_CONTACT" : NULL;
}

/* The agent a kind puts on the wire; NULL when the kind needs a credential
 * that is not (validly) configured. One function, because a kind whose string
 * an operator supplies must resolve the same way wherever it is read — the
 * table dump, the override accessor and the request builder. */
static const char *host_agent_ua(host_agent_kind k) {
  if (host_agent_credential(k)) {
    if (http_contact_ua(g_ua_buf, sizeof g_ua_buf, NULL) != 0) return NULL;
    return g_ua_buf;
  }
  return HOST_AGENT_STRING[k];
}

const char *http_host_needs_credential(const char *url) {
  int i = host_agent_index(url);
  if (i < 0) return NULL;
  host_agent_kind k = HOST_AGENTS[i].kind;
  const char *c = host_agent_credential(k);
  return (c && !host_agent_ua(k)) ? c : NULL;
}

const char *http_credential_refusal(void) { return g_cred_refusal; }
void http_credential_refusal_reset(void) { g_cred_refusal = NULL; }

int http_browser_host(const char *url) {
  int i = host_agent_index(url);
  return i >= 0 && HOST_AGENTS[i].kind == HA_BROWSER;
}

const char *http_ua_override(const char *url) {
  int i = host_agent_index(url);
  return i >= 0 ? host_agent_ua(HOST_AGENTS[i].kind) : NULL;
}

void http_host_agents_dump(FILE *out) {
  fprintf(out, "host\tkind\tuser_agent\textra_headers\n");
  for (size_t i = 0; i < sizeof HOST_AGENTS / sizeof *HOST_AGENTS; i++) {
    host_agent_kind k = HOST_AGENTS[i].kind;
    const char *ua = host_agent_ua(k);
    if (ua) fprintf(out, "%s\t%s\t%s\t", HOST_AGENTS[i].host, HOST_AGENT_NAME[k], ua);
    else    fprintf(out, "%s\t%s\t(unset: %s)\t", HOST_AGENTS[i].host,
                    HOST_AGENT_NAME[k], host_agent_credential(k));
    if (k == HA_BROWSER)
      for (const char *const *x = BROWSER_EXTRA_HEADERS; *x; x++)
        fprintf(out, "%s%s", x == BROWSER_EXTRA_HEADERS ? "" : " | ", *x);
    fputc('\n', out);
  }
}

/* ── Retry-After ────────────────────────────────────────────────────────────
 * The retry loop below used to answer every 429/503 with 250·2^n ms (capped at
 * 4 s) and never looked at Retry-After, so a server that said "come back in
 * 120 s" got up to `retries` more requests inside a few seconds — each one a
 * fresh 429, and each one lengthening the ban on the shared IP for every other
 * collector on that host.
 *
 * Now: the header is parsed in both of RFC 9110's forms (delta-seconds and an
 * HTTP-date — curl_getdate() reads all three date spellings). A delay within
 * HTTP_RETRY_AFTER_MAX_MS and within the caller's own timeout is slept before
 * the retry; a longer one is NOT retried, and the 429/503 is returned for the
 * caller to report. Either way the delay is recorded in the host gate
 * (hostgate_penalize) so every other fetch to that host backs off too. */
#define HTTP_RETRY_AFTER_MAX_MS 30000

/* Retry-After value -> milliseconds from `now`, or -1 when the value is
 * neither form. A date in the past is 0 ("now"). Bounded to a day so the
 * arithmetic cannot overflow on a hostile value. */
static long retry_after_parse_ms(const char *v, time_t now) {
  if (!v) return -1;
  while (*v == ' ' || *v == '\t') v++;
  size_t n = strlen(v);
  while (n && (v[n - 1] == ' ' || v[n - 1] == '\t' || v[n - 1] == '\r' ||
               v[n - 1] == '\n')) n--;
  if (n == 0 || n >= 128) return -1;
  char buf[128];
  memcpy(buf, v, n);
  buf[n] = 0;
  const long day_ms = 86400L * 1000L;
  int digits = 1;
  for (size_t i = 0; i < n; i++) if (!isdigit((unsigned char)buf[i])) { digits = 0; break; }
  if (digits) {
    if (n > 6) return day_ms;               /* > 999,999 s: clamp, no overflow */
    long s = strtol(buf, NULL, 10);
    return s * 1000L > day_ms ? day_ms : s * 1000L;
  }
  time_t t = curl_getdate(buf, NULL);
  if (t == (time_t)-1) return -1;
  if (t <= now) return 0;
  double d = difftime(t, now) * 1000.0;
  return d > (double)day_ms ? day_ms : (long)d;
}

/* What to do with a response's Retry-After: -2 not applicable (no header, or
 * a status it does not govern here), -1 do not retry (longer than we will
 * wait), else the milliseconds to sleep before retrying. */
static long retry_after_decision(long status, long ra_ms, int timeout_ms) {
  if (ra_ms < 0 || (status != 429 && status != 503)) return -2;
  long bound = HTTP_RETRY_AFTER_MAX_MS;
  if (timeout_ms > 0 && timeout_ms < bound) bound = timeout_ms;
  return ra_ms > bound ? -1 : ra_ms;
}

typedef struct { long retry_after_ms; } hdr_state;

/* Sees every header of every hop. A status line starts a new response, so a
 * Retry-After from a redirect hop cannot leak onto the final one. */
static size_t on_header(char *buf, size_t sz, size_t nm, void *ud) {
  size_t n = sz * nm;
  hdr_state *h = (hdr_state *)ud;
  if (n >= 5 && strncmp(buf, "HTTP/", 5) == 0) { h->retry_after_ms = -1; return n; }
  static const char K[] = "retry-after:";
  if (n > sizeof K - 1 && n < 256 && strncasecmp(buf, K, sizeof K - 1) == 0) {
    char v[256];
    memcpy(v, buf + sizeof K - 1, n - (sizeof K - 1));
    v[n - (sizeof K - 1)] = 0;
    h->retry_after_ms = retry_after_parse_ms(v, time(NULL));
  }
  return n;
}

/* What the last do_once() on THIS thread failed on — see
 * http_last_transport_error() in httpclient.h. Thread-local because the one
 * caller that reads it (the LLM worker) reads it on the thread that fetched. */
static __thread int g_last_te = HTTP_TE_NONE;

int http_last_transport_error(void) { return g_last_te; }

/* Which TLS failure, if the last attempt was one — see httpclient.h. Same
 * thread-local reasoning as g_last_te. */
static __thread int g_last_tls = HTTP_TLS_NONE;

int http_last_tls_cause(void) { return g_last_tls; }
/* Both are per-THREAD and a scheduler worker outlives the run it is on, so a
 * run that made no request used to read the previous source's cause back out
 * of them and print it in its own fetch-failure notice. The scheduler clears
 * them at the start of every run. */
void http_reset_last_error(void) { g_last_te = HTTP_TE_NONE; g_last_tls = HTTP_TLS_NONE; }

const char *http_tls_cause_label(int cause) {
  switch (cause) {
    case HTTP_TLS_NO_LOCAL_ISSUER:
      return "the certificate chain could not be completed — no client here "
             "could find the certificate that SIGNED this one, which usually "
             "means the server sent its leaf without the intermediate";
    case HTTP_TLS_EXPIRED:
      return "the server's certificate has expired";
    case HTTP_TLS_NOT_YET_VALID:
      return "the server's certificate is not valid yet, so either it was "
             "issued for a later date or this machine's clock is wrong";
    case HTTP_TLS_HOSTNAME_MISMATCH:
      return "the certificate is valid but is for a different hostname, so "
             "this row is asking the wrong name";
    case HTTP_TLS_SELF_SIGNED:
      return "the certificate is self-signed, so no public CA vouches for it";
    case HTTP_TLS_REVOKED:
      return "the issuing CA has revoked the certificate";
    case HTTP_TLS_UNRECOGNIZED_NAME:
      return "the server answered the TLS handshake with \"unrecognized "
             "name\": it does not serve this hostname at all, so no "
             "certificate was ever offered";
    case HTTP_TLS_HANDSHAKE:
      return "the TLS handshake was refused before any certificate was "
             "judged — a protocol version, cipher or security-level mismatch";
    case HTTP_TLS_OTHER:
      return "the certificate failed verification for a reason this build "
             "does not name separately";
    default:
      return "";
  }
}

const char *http_tls_cause_remedy(int cause) {
  switch (cause) {
    case HTTP_TLS_NO_LOCAL_ISSUER:
      /* The one cause that is usually OURS to fix, and the one that was
       * mislabelling 24 live hosts as untrustworthy until 2026-10-21. */
      return "this is the fixable one and it is not a trust problem: read the "
             "leaf's AIA CA-Issuers extension, fetch that intermediate, verify "
             "it against the SYSTEM root store, and add it to "
             "native/data/tls/extra-intermediates.pem with its provenance "
             "(core/catrust.c loads it for the engine and tools/jo_tls.py for "
             "the tools). An entry there can COMPLETE a chain and never ANCHOR "
             "one, which tests/unit/test_catrust.c enforces";
    case HTTP_TLS_EXPIRED:
    case HTTP_TLS_NOT_YET_VALID:
      return "nothing here can fix this: it is the upstream's certificate, or "
             "this machine's clock. Check the clock first, then re-run — an "
             "expiry is often fixed within hours";
    case HTTP_TLS_HOSTNAME_MISMATCH:
      return "re-point the row at the name the certificate carries; this is "
             "how www.ip.kyusan-u.ac.jp resolved to www.kyusan-u.ac.jp and "
             "matrix.kde.org to kde.modular.im. Do NOT stop checking the "
             "hostname — that is the one route to this host nobody should take";
    case HTTP_TLS_SELF_SIGNED:
      return "a self-signed certificate cannot be completed by an "
             "intermediate, so extra-intermediates.pem is the wrong lever: "
             "record the row as unreachable rather than widening trust";
    case HTTP_TLS_REVOKED:
      return "a revoked certificate is a decision by the CA; record the row as "
             "unreachable and do not work around it";
    case HTTP_TLS_UNRECOGNIZED_NAME:
      /* Named apart from HTTP_TLS_HANDSHAKE because that remedy points at a
       * cipher security level, and a knob that cannot move is worse than no
       * remedy at all. Measured 2026-10-23: dataverse.openscholar.org and
       * nesstar.statssa.gov.za — the two hosts docs/tls-intermediates-
       * 2026-10-21.md records as "genuinely unreachable, an SNI mismatch" —
       * both answer CURLE 35 `tlsv1 unrecognized name`, and no cipher or
       * trust change reaches that. */
      return "the host is not serving this name: find the name it does serve "
             "and re-point the row, or record it as unreachable. Neither a "
             "cipher level nor a trust change reaches this — there is no "
             "certificate in the exchange to fix";
    case HTTP_TLS_HANDSHAKE:
      return "no certificate was judged, so this is not about trust: compare "
             "against tools/jo_tls.py, which lowers OpenSSL's CIPHER SECURITY "
             "LEVEL (verification and hostname checking stay on) and reaches "
             "hosts the default context refuses — yamaguchi-opendata.jp is the "
             "measured case";
    case HTTP_TLS_OTHER:
      return "read the libcurl message on the [http] line above: \"no local "
             "issuer\", \"expired\", \"hostname mismatch\" and \"self-signed\" "
             "are four findings with four remedies";
    default:
      return "";
  }
}

/* The verify result is the discriminator; the message text is the fallback for
 * the cases it cannot express (see httpclient.h). `msg` may be empty. */
static int tls_cause_of(CURLcode rc, long verify, const char *msg) {
  switch (verify) {
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
      return HTTP_TLS_NO_LOCAL_ISSUER;
    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_CRL_HAS_EXPIRED:
      return HTTP_TLS_EXPIRED;
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_CRL_NOT_YET_VALID:
      return HTTP_TLS_NOT_YET_VALID;
    case X509_V_ERR_HOSTNAME_MISMATCH:
      return HTTP_TLS_HOSTNAME_MISMATCH;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
      return HTTP_TLS_SELF_SIGNED;
    case X509_V_ERR_CERT_REVOKED:
      return HTTP_TLS_REVOKED;
    default: break;
  }
  /* No verify result to read. A hostname mismatch arrives this way on some
   * backends, and `strcasestr` is jo_strcasestr in this tree for the reason
   * CLAUDE.md gives (glibc declares strcasestr only under _GNU_SOURCE). */
  if (msg && msg[0]) {
    /* The two spellings libcurl actually uses: "no alternative certificate
     * subject name matches target host name 'x'" and "certificate subject name
     * 'x' does not match target host name 'y'". A bare "hostname" is NOT in
     * this list on purpose — it appears in resolver and proxy messages that
     * have nothing to do with a certificate, and an over-broad alternative in
     * a classifier is how this tree has twice filed one finding as another. */
    if (jo_strcasestr(msg, "subject name") || jo_strcasestr(msg, "subjectAltName")
        || jo_strcasestr(msg, "host name"))
      return HTTP_TLS_HOSTNAME_MISMATCH;
    if (jo_strcasestr(msg, "local issuer")) return HTTP_TLS_NO_LOCAL_ISSUER;
    if (jo_strcasestr(msg, "expired"))      return HTTP_TLS_EXPIRED;
    if (jo_strcasestr(msg, "self signed") || jo_strcasestr(msg, "self-signed"))
      return HTTP_TLS_SELF_SIGNED;
    /* OpenSSL alert 112, which libcurl reports as "tlsv1 unrecognized name"
     * inside CURLE 35. It is not a certificate verdict and wants the opposite
     * remedy from a cipher mismatch — see http_tls_cause_remedy. */
    if (jo_strcasestr(msg, "unrecognized name")) return HTTP_TLS_UNRECOGNIZED_NAME;
  }
  /* A refusal before verification has no certificate verdict at all, so it is
   * not a trust finding and must not be reported as one. */
  if (rc == CURLE_SSL_CONNECT_ERROR || rc == CURLE_SSL_CIPHER)
    return HTTP_TLS_HANDSHAKE;
  return HTTP_TLS_OTHER;
}

static int do_once(http_client *c, const char *method, const char *url,
                   const char *const *headers, const char *body,
                   size_t body_len, int timeout_ms, http_response *out,
                   long *retry_after_ms, int nofollow) {
  *retry_after_ms = -1;
  g_redirect_url[0] = 0;
  g_last_te = HTTP_TE_OTHER;            /* every early refusal below is "other" */
  g_last_tls = HTTP_TLS_NONE;           /* nothing below this is a TLS verdict */
  /* Defensive: a NULL client/method/url must never segfault the process.
   * On-demand runs (e.g. dataapi_layer) may invoke an HTTP-backed collector
   * without a client wired up; treat that as a hard failure (rc=1 → caller
   * degrades to empty) rather than dereferencing NULL. */
  if (!c || !method || !url) { out->status = 0; out->body = NULL; out->body_len = 0; return 1; }
  /* Scheme + literal-host SSRF floor. Refusing here (rather than letting curl
   * dial) is what stops file:/gopher: and the cloud metadata range from ever
   * being reachable through a collector URL, an LLM-proposed override, or a
   * redirect target we were handed. */
  { int gk = g_strict_once ? hostgate_url_check_strict(url)
                           : hostgate_url_check(url);
    if (gk != HG_URL_OK) {
      fprintf(stderr, "[http] refused %s: %s\n", url, hostgate_url_reason(gk));
      out->status = 0; out->body = NULL; out->body_len = 0; return 1;
    } }
  CURL *e = curl_easy_init();
  if (!e) return 1;
  sbuf b = {0};
  b.cap = body_ceiling();
  struct curl_slist *hl = NULL;
  /* One precedence for every caller: a host in core/host_agents.def gets the
   * table's agent whatever User-Agent the caller set; any other host keeps a
   * caller's header, else JO_USER_AGENT. */
  int ha = host_agent_index(url);
  for (const char *const *h = headers; h && *h; ++h) {
    if (ha >= 0 && !strncasecmp(*h, "User-Agent:", 11)) continue;
    hl = curl_slist_append(hl, *h);
  }
  if (ha >= 0 && HOST_AGENTS[ha].kind == HA_BROWSER)
    for (const char *const *h = BROWSER_EXTRA_HEADERS; *h; ++h) hl = curl_slist_append(hl, *h);

  curl_easy_setopt(e, CURLOPT_URL, url);
  curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, on_data);
  curl_easy_setopt(e, CURLOPT_WRITEDATA, &b);
  hdr_state hs = { -1 };
  curl_easy_setopt(e, CURLOPT_HEADERFUNCTION, on_header);
  curl_easy_setopt(e, CURLOPT_HEADERDATA, &hs);
  /* curl_easy_strerror() gives the CLASS ("SSL peer certificate or SSH remote
   * key was not OK"); the error buffer gives the sentence that names which
   * ("unable to get local issuer certificate"). Both are logged, because the
   * first is stable and the second is what tells four TLS findings apart. */
  char ebuf[CURL_ERROR_SIZE];
  ebuf[0] = 0;
  curl_easy_setopt(e, CURLOPT_ERRORBUFFER, ebuf);
  http_easy_harden(e);
  if (nofollow) curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 0L);
#if LIBCURL_VERSION_NUM >= 0x075000
  if (g_strict_once) curl_easy_setopt(e, CURLOPT_PREREQDATA, (void *)1);
#endif
  /* Belt to the write-callback's braces: when the server DOES advertise a
   * length, refuse before a single byte is transferred. */
  if (b.cap) curl_easy_setopt(e, CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)b.cap);
  curl_easy_setopt(e, CURLOPT_TIMEOUT_MS, (long)(timeout_ms > 0 ? timeout_ms : 30000));
  /* For HTTPS the connect timeout covers the TLS handshake, so a flat 10 s made
   * slow-handshaking hosts unreachable no matter what budget the caller asked
   * for: api.gdeltproject.org handshakes in 9.6-10.8 s, so GDELT_TV died at
   * 10.8 s while requesting 45 s. Honour the caller, capped so a generous
   * per-request budget can't hang the scheduler slot on a black-holed host. */
  {
    long ct = (long)(timeout_ms > 0 ? timeout_ms : 30000);
    if (ct > 20000) ct = 20000;
    curl_easy_setopt(e, CURLOPT_CONNECTTIMEOUT_MS, ct);
  }
  curl_easy_setopt(e, CURLOPT_ACCEPT_ENCODING, "");
  {
    /* http_request() refuses a credential-less SEC host before getting here,
     * so a NULL agent cannot occur; the fallback is the engine's own. */
    const char *ua = ha >= 0 ? host_agent_ua(HOST_AGENTS[ha].kind) : NULL;
    curl_easy_setopt(e, CURLOPT_USERAGENT, ua ? ua : JO_USER_AGENT);
  }
  curl_easy_setopt(e, CURLOPT_NOSIGNAL, 1L);
  if (c->share) curl_easy_setopt(e, CURLOPT_SHARE, c->share);
  if (hl) curl_easy_setopt(e, CURLOPT_HTTPHEADER, hl);
  if (strcmp(method, "GET") != 0)
    curl_easy_setopt(e, CURLOPT_CUSTOMREQUEST, method);
  /* CUSTOMREQUEST alone still makes libcurl wait for a response body, which a
   * HEAD never sends — so every HEAD stalled until the timeout and the whole
   * probe family reported `reachable:false` for sites that answer `curl -I`
   * with a 200. NOBODY is what actually makes it a HEAD. */
  if (strcmp(method, "HEAD") == 0)
    curl_easy_setopt(e, CURLOPT_NOBODY, 1L);
  if (body) {
    curl_easy_setopt(e, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(e, CURLOPT_POSTFIELDSIZE, (long)body_len);
  }

  /* Per-host politeness (core/hostgate.h). The scheduler is a worker pool now,
   * so the accidental one-at-a-time serialisation that kept ~90 Overpass
   * collectors from arriving together is gone and has to be made explicit.
   * The wait budget is the caller's own timeout: a fetch already willing to
   * spend 30 s on the network can spend some of it queueing. Fails open, so a
   * saturated host degrades to "as rude as before", never to a fetch error. */
  int gated = hostgate_acquire(url, timeout_ms > 0 ? timeout_ms : 30000);
  /* A cancelled run makes no request: the gate returned without a slot. */
  CURLcode rc = hostgate_cancelled() ? CURLE_ABORTED_BY_CALLBACK : curl_easy_perform(e);
  if (gated) hostgate_release(url);
  long code = 0;
  curl_easy_getinfo(e, CURLINFO_RESPONSE_CODE, &code);
  {
    /* A timeout before the connection completed is a host that is not there
     * (black-holed), not a slow answer; the LLM worker retries the first and
     * never the second, so the two must not share a verdict. */
    double conn = 0;
    curl_easy_getinfo(e, CURLINFO_CONNECT_TIME, &conn);
    g_last_te = rc == CURLE_OK ? HTTP_TE_NONE
              : rc == CURLE_OPERATION_TIMEDOUT ? (conn > 0 ? HTTP_TE_TIMEOUT
                                                           : HTTP_TE_CONNECT)
              : (rc == CURLE_COULDNT_CONNECT || rc == CURLE_COULDNT_RESOLVE_HOST ||
                 rc == CURLE_COULDNT_RESOLVE_PROXY) ? HTTP_TE_CONNECT
              : HTTP_TE_OTHER;
    /* Which TLS failure (open item D1-bis). Read from the handle BEFORE
     * cleanup, and only for the CURLcodes that mean the TLS layer refused —
     * CURLINFO_SSL_VERIFYRESULT is 0 on a plain HTTP error too, and 0 is also
     * X509_V_OK, so asking unconditionally would read every connect refusal as
     * a certificate verdict. */
    if (rc == CURLE_PEER_FAILED_VERIFICATION || rc == CURLE_SSL_CONNECT_ERROR ||
        rc == CURLE_SSL_CERTPROBLEM || rc == CURLE_SSL_CIPHER ||
        rc == CURLE_SSL_ISSUER_ERROR || rc == CURLE_SSL_INVALIDCERTSTATUS) {
      long verify = 0;
      curl_easy_getinfo(e, CURLINFO_SSL_VERIFYRESULT, &verify);
      g_last_tls = tls_cause_of(rc, verify, ebuf);
    }
  }
  if (nofollow && rc == CURLE_OK) {
    char *loc = NULL;
    if (curl_easy_getinfo(e, CURLINFO_REDIRECT_URL, &loc) == CURLE_OK && loc &&
        strlen(loc) < sizeof g_redirect_url)
      strcpy(g_redirect_url, loc);
  }
  if (hl) curl_slist_free_all(hl);
  curl_easy_cleanup(e);
  *retry_after_ms = hs.retry_after_ms;

  if (rc != CURLE_OK) {
    if (b.over || rc == CURLE_FILESIZE_EXCEEDED)
      fprintf(stderr, "[http] %s: response exceeded %zu byte ceiling\n",
              url, b.cap);
    else if (ebuf[0])  /* the sentence that names WHICH failure, not the class */
      fprintf(stderr, "[http] %s: %s (%s)\n", url, ebuf, curl_easy_strerror(rc));
    else  /* callers log only "transport failure"; say which one */
      fprintf(stderr, "[http] %s: %s\n", url, curl_easy_strerror(rc));
    free(b.buf); out->status = 0; out->body = NULL; out->body_len = 0; return 1;
  }
  out->status = code;
  out->body = b.buf ? b.buf : calloc(1, 1);
  out->body_len = b.len;
  return 0;
}

/* One logical request with redirects handled HERE when the request must not
 * let libcurl follow them: a strict request follows none, and a request that
 * carries a credential header follows manually so the credential can be
 * dropped when a hop changes host. libcurl strips only `Authorization:` (and
 * Cookie) from a custom header list on a cross-host redirect, so an
 * `X-API-Key:` was sent to whatever host a 302 named. Each hop goes through
 * do_once(), so it gets the same URL check, per-host gate and connection
 * check as the first. Otherwise this is exactly do_once(). */
static int do_follow(http_client *c, const char *method, const char *url,
                     const char *const *headers, const char *body,
                     size_t body_len, int timeout_ms, http_response *out,
                     long *retry_after_ms) {
  int has_cred = 0;
  for (const char *const *h = headers; h && *h; ++h)
    if (header_is_credential(*h)) { has_cred = 1; break; }
  int strict_nf = g_strict_once && !g_strict_follow;
  int nofollow = strict_nf || has_cred;
  int hard = do_once(c, method, url, headers, body, body_len, timeout_ms, out,
                     retry_after_ms, nofollow);
  if (!nofollow || strict_nf || hard) return hard;

  const char *const *hdrs = headers;
  const char **stripped = NULL;
  char cur[2048];
  if (strlen(url) >= sizeof cur) return hard;
  strcpy(cur, url);
  const char *m = method;
  const char *b = body;
  size_t bl = body_len;
  for (int hop = 0;; hop++) {
    long st = out->status;
    if (!(st == 301 || st == 302 || st == 303 || st == 307 || st == 308) ||
        !g_redirect_url[0])
      break;                                    /* not a redirect: done */
    if (hop >= 5) {                             /* libcurl's MAXREDIRS, same */
      fprintf(stderr, "[http] %s: too many redirects\n", url);
      http_response_free(out);
      free(stripped);
      return 1;
    }
    char next[2048];
    strcpy(next, g_redirect_url);
    if (hdrs && *hdrs && !hostgate_same_host(cur, next)) {
      size_t nh = 0;
      while (hdrs[nh]) nh++;
      const char **ns = malloc((nh + 1) * sizeof *ns);
      if (!ns) {                                /* fail closed, as the override path */
        fprintf(stderr, "[http] refusing redirect to %s: cannot build a "
                        "credential-free header list\n", next);
        http_response_free(out);
        free(stripped);
        return 1;
      }
      size_t n = 0;
      int dropped = 0;
      for (size_t i = 0; i < nh; i++) {
        if (header_is_credential(hdrs[i])) { dropped++; continue; }
        ns[n++] = hdrs[i];
      }
      ns[n] = NULL;
      free(stripped);
      stripped = ns;
      hdrs = (const char *const *)stripped;
      if (dropped)
        fprintf(stderr, "[http] dropped %d auth header(s): redirect changed "
                        "host\n", dropped);
    }
    /* libcurl's own method rewrite for the three historic redirects. */
    if ((st == 301 || st == 302 || st == 303) && strcmp(m, "GET") != 0 &&
        strcmp(m, "HEAD") != 0) {
      m = "GET"; b = NULL; bl = 0;
    }
    if (c) c->bytes_in += (unsigned long long)out->body_len;
    http_response_free(out);
    hard = do_once(c, m, next, hdrs, b, bl, timeout_ms, out, retry_after_ms, 1);
    if (hard) { free(stripped); return hard; }
    strcpy(cur, next);
  }
  free(stripped);
  return hard;
}

int http_request(http_client *c, const char *method, const char *url,
                  const char *const *headers, const char *body, size_t body_len,
                  int timeout_ms, int retries, http_response *out) {
  out->status = 0; out->body = NULL; out->body_len = 0;
  /* Single choke point for the maintenance pod's verified URL-swap repairs:
   * a stored override transparently rewrites this outbound URL for every
   * collector with no per-collector edits. No-op unless an override matches. */
  const char *eff = url_override_apply(url);
  /* A host that needs a configured credential (SEC_CONTACT) is refused here,
   * before any network, with a distinct named reason the engine and the
   * scheduler disclose as "needs <credential>" - a request is a certain 403. */
  {
    const char *need = http_host_needs_credential(eff ? eff : url);
    if (need) {
      g_cred_refusal = need;
      g_last_te = HTTP_TE_OTHER;
      fprintf(stderr, "[http] refused %s: %s is not configured (SEC returns "
                      "403 to a User-Agent without a contact address)\n",
              eff ? eff : url, need);
      return 1;
    }
  }
  /* An override is proposed by the LLM maintenance pod, so the new host is not
   * necessarily the one the collector's credentials were issued to. Sending
   * this source's API key to a host somebody else nominated is credential
   * exfiltration with our own outbound connection; drop the credential headers
   * whenever the rewrite crosses hosts. Same-host repairs (a moved path, a
   * changed query) keep their auth and keep working. */
  const char *const *eff_headers = headers;
  const char **stripped = NULL;
  if (eff != url && eff && url && strcmp(eff, url) != 0) {
    fprintf(stderr, "[url-override] rewrite %s -> %s\n", url, eff);
    if (!hostgate_same_host(url, eff) && headers && *headers) {
      /* This scan MUST reach the end of the array. It used to stop at 23 KEPT
       * headers — a bound on the wrong counter and in the wrong direction: a
       * caller with 23 innocuous headers ahead of its `Authorization:` never
       * reached the credential, `dropped` stayed 0, `eff_headers` was left
       * pointing at the ORIGINAL array, and the key went to the host this
       * check exists to keep it away from. A security control whose overflow
       * behaviour is fail-OPEN is not a control.
       *
       * So the list is now sized to the input and the failure is closed. If
       * the copy cannot be allocated we do not fall back to sending the
       * original headers — we refuse the request, because the only thing we
       * know at that point is that we cannot prove the credential is gone. */
      size_t nh = 0;
      while (headers[nh]) nh++;
      int dropped = 0;
      for (size_t i = 0; i < nh; i++)
        if (header_is_credential(headers[i])) dropped++;
      if (dropped) {
        stripped = malloc((nh + 1) * sizeof *stripped);
        if (!stripped) {
          fprintf(stderr, "[url-override] refusing %s: cannot build a "
                          "credential-free header list\n", eff);
          return 1;                 /* out is already zeroed: status 0, no body */
        }
        size_t n = 0;
        for (size_t i = 0; i < nh; i++) {
          if (header_is_credential(headers[i])) continue;
          stripped[n++] = headers[i];
        }
        stripped[n] = NULL;
        fprintf(stderr, "[url-override] dropped %d auth header(s): host "
                        "changed\n", dropped);
        eff_headers = (const char *const *)stripped;
      }
    }
    url = eff;
  }
  headers = eff_headers;
  int attempt = 0;
  for (;;) {
    http_response r = {0};
    long ra_ms = -1;
    int hard = do_follow(c, method, url, headers, body, body_len, timeout_ms, &r,
                         &ra_ms);
    /* Every attempt's body counts, retried ones included: transfer is what
     * the upstream sent us, not what we kept. */
    if (c) { c->bytes_in += (unsigned long long)r.body_len; c->requests++; }   /* NULL client: do_once answered a hard failure */
    int retryable = hard || (r.status >= 500 && r.status <= 599) || r.status == 429;
    long ra_wait = hard ? -2 : retry_after_decision(r.status, ra_ms, timeout_ms);
    if (ra_wait != -2) {
      /* Every other fetch to this host honours the server's delay too. */
      hostgate_penalize(url, ra_ms);
      if (ra_wait == -1) {
        if (retryable && attempt < retries)
          fprintf(stderr, "[http] %s: %ld with Retry-After %ld s — longer than "
                          "we wait, not retrying\n", url, r.status, ra_ms / 1000);
        retryable = 0;
      }
    }
    if (!retryable || attempt >= retries) {
      *out = r;
      log_host(c, url, r.status, hard);   /* attribute to the real upstream host */
      /* Chain of custody (roadmap 17). No-op unless the calling thread has an
       * evidence scope bound AND the source opted in — this file has no
       * db_handle or source_id of its own, so the scope is thread-local and
       * fails closed. Never affects the fetch's result. Auth headers and
       * URL query/userinfo are redacted inside the hook: evidence must not
       * become a credential store. */
      evidence_http_hook(method, url, headers, r.status, r.body, r.body_len);
      /* Content change detection (roadmap 26). Also scope-gated and fails
       * closed. Rejects non-GET and non-200 internally: a 404 page is not
       * "content changed", and admitting it would fire on every outage AND
       * again on every recovery. */
      content_change_http_hook(method, url, r.status, r.body, r.body_len);
      free(stripped);
      return hard && attempt >= retries ? 1 : 0;
    }
    http_response_free(&r);
    /* exponential backoff: 250ms, 500ms, 1s, ... capped at 4s — or the
     * server's own Retry-After when it named one we are willing to wait. */
    long ms = attempt < 5 ? 250L << attempt : 4000L;
    if (ms > 4000) ms = 4000;
    if (ra_wait > ms) ms = ra_wait;
    sleep_ms(ms);
    attempt++;
  }
}

/* http_request() with a body ceiling for this one call. See
 * g_body_ceiling_once above for why this is per-call and thread-local rather
 * than a bigger default. max_body of 0 means "the default", so a caller that
 * does not care is identical to calling http_request() directly.
 *
 * The ceiling is restored on EVERY exit path, including the retry loop's and
 * every early refusal, because it is restored here rather than inside
 * http_request() — a leaked ceiling would silently widen the next fetch this
 * worker makes, which is the failure mode the thread-local exists to avoid. */
int http_request_max(http_client *c, const char *method, const char *url,
                     const char *const *headers, const char *body,
                     size_t body_len, int timeout_ms, int retries,
                     size_t max_body, http_response *out) {
  size_t prev = g_body_ceiling_once;
  g_body_ceiling_once = max_body;
  int rc = http_request(c, method, url, headers, body, body_len,
                        timeout_ms, retries, out);
  g_body_ceiling_once = prev;
  return rc;
}

/* Standing per-thread ceiling for every request until it is set back — the
 * scheduler wraps one source's whole run in it, so the operator's per-source
 * `http_body_ceiling_bytes` reaches the hpengine and VJSON rows that never
 * read ctx->body_ceiling. Same variable, same clamp by an explicit
 * $JO_HTTP_MAX_BODY as http_request_max(). Returns the previous value. */
size_t http_body_ceiling_set(size_t max_body) {
  size_t prev = g_body_ceiling_once;
  g_body_ceiling_once = max_body;
  return prev;
}

/* http_request() under the strict ruleset for this one call. See
 * g_strict_once / g_strict_follow. Restored on every exit path for the reason
 * http_request_max() restores its ceiling here. */
int http_request_strict(http_client *c, const char *method, const char *url,
                        const char *const *headers, const char *body,
                        size_t body_len, int timeout_ms, int retries,
                        int follow_redirects, http_response *out) {
  int prev = g_strict_once, prev_f = g_strict_follow;
  g_strict_once = 1;
  g_strict_follow = follow_redirects ? 1 : 0;
  int rc = http_request(c, method, url, headers, body, body_len,
                        timeout_ms, retries, out);
  g_strict_once = prev;
  g_strict_follow = prev_f;
  return rc;
}

void http_response_free(http_response *r) {
  if (r && r->body) { free(r->body); r->body = NULL; r->body_len = 0; r->status = 0; }
}

unsigned long long http_client_bytes_in(const http_client *c) { return c ? c->bytes_in : 0; }
int http_client_request_count(const http_client *c) { return c ? c->requests : 0; }
