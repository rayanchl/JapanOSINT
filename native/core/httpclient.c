#include "httpclient.h"
#include "url_override.h"
#include "evidence.h"
#include "content_change.h"
#include "hostgate.h"
#include <stdio.h>
#include <curl/curl.h>
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
static void max_body_probe(void) {
  const char *e = getenv("JO_HTTP_MAX_BODY");
  long long v = (e && *e) ? atoll(e) : -1;
  g_max_body = (v >= 0) ? (size_t)v : (size_t)HTTP_MAX_BODY_DEFAULT;
}

static size_t max_body_bytes(void) {
  static pthread_once_t once = PTHREAD_ONCE_INIT;
  pthread_once(&once, max_body_probe);
  return g_max_body;
}

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
  (void)ud; (void)conn_local_ip; (void)conn_primary_port; (void)conn_local_port;
  /* The FLOOR as configured, not a hardcoded 0: with JO_HTTP_BLOCK_PRIVATE=1
   * the operator asked for RFC1918/loopback to be refused, and a hostname is
   * only ever caught here (url_check() sees a name, not an address). */
  if (hostgate_addr_check_floor(conn_primary_ip) != HG_URL_OK) {
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
}

/* usleep() takes a 32-bit useconds_t and POSIX allows EINVAL past one second;
 * a 30 s Retry-After is 3e10 µs. */
static void sleep_ms(long ms) {
  if (ms <= 0) return;
  struct timespec ts = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
  while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
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

int http_browser_host(const char *url) {
  int i = host_agent_index(url);
  return i >= 0 && HOST_AGENTS[i].kind == HA_BROWSER;
}

const char *http_ua_override(const char *url) {
  int i = host_agent_index(url);
  return i >= 0 ? HOST_AGENT_STRING[HOST_AGENTS[i].kind] : NULL;
}

void http_host_agents_dump(FILE *out) {
  fprintf(out, "host\tkind\tuser_agent\textra_headers\n");
  for (size_t i = 0; i < sizeof HOST_AGENTS / sizeof *HOST_AGENTS; i++) {
    host_agent_kind k = HOST_AGENTS[i].kind;
    fprintf(out, "%s\t%s\t%s\t", HOST_AGENTS[i].host, HOST_AGENT_NAME[k], HOST_AGENT_STRING[k]);
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

static int do_once(http_client *c, const char *method, const char *url,
                   const char *const *headers, const char *body,
                   size_t body_len, int timeout_ms, http_response *out,
                   long *retry_after_ms) {
  *retry_after_ms = -1;
  g_last_te = HTTP_TE_OTHER;            /* every early refusal below is "other" */
  /* Defensive: a NULL client/method/url must never segfault the process.
   * On-demand runs (e.g. dataapi_layer) may invoke an HTTP-backed collector
   * without a client wired up; treat that as a hard failure (rc=1 → caller
   * degrades to empty) rather than dereferencing NULL. */
  if (!c || !method || !url) { out->status = 0; out->body = NULL; out->body_len = 0; return 1; }
  /* Scheme + literal-host SSRF floor. Refusing here (rather than letting curl
   * dial) is what stops file:/gopher: and the cloud metadata range from ever
   * being reachable through a collector URL, an LLM-proposed override, or a
   * redirect target we were handed. */
  { int gk = hostgate_url_check(url);
    if (gk != HG_URL_OK) {
      fprintf(stderr, "[http] refused %s: %s\n", url, hostgate_url_reason(gk));
      out->status = 0; out->body = NULL; out->body_len = 0; return 1;
    } }
  CURL *e = curl_easy_init();
  if (!e) return 1;
  sbuf b = {0};
  b.cap = max_body_bytes();
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
  http_easy_harden(e);
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
  curl_easy_setopt(e, CURLOPT_USERAGENT, ha >= 0 ? HOST_AGENT_STRING[HOST_AGENTS[ha].kind] : JO_USER_AGENT);
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
  CURLcode rc = curl_easy_perform(e);
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
  }
  if (hl) curl_slist_free_all(hl);
  curl_easy_cleanup(e);
  *retry_after_ms = hs.retry_after_ms;

  if (rc != CURLE_OK) {
    if (b.over || rc == CURLE_FILESIZE_EXCEEDED)
      fprintf(stderr, "[http] %s: response exceeded %zu byte ceiling\n",
              url, b.cap);
    else  /* callers log only "transport failure"; say which one */
      fprintf(stderr, "[http] %s: %s\n", url, curl_easy_strerror(rc));
    free(b.buf); out->status = 0; out->body = NULL; out->body_len = 0; return 1;
  }
  out->status = code;
  out->body = b.buf ? b.buf : calloc(1, 1);
  out->body_len = b.len;
  return 0;
}

int http_request(http_client *c, const char *method, const char *url,
                  const char *const *headers, const char *body, size_t body_len,
                  int timeout_ms, int retries, http_response *out) {
  out->status = 0; out->body = NULL; out->body_len = 0;
  /* Single choke point for the maintenance pod's verified URL-swap repairs:
   * a stored override transparently rewrites this outbound URL for every
   * collector with no per-collector edits. No-op unless an override matches. */
  const char *eff = url_override_apply(url);
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
    int hard = do_once(c, method, url, headers, body, body_len, timeout_ms, &r,
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

void http_response_free(http_response *r) {
  if (r && r->body) { free(r->body); r->body = NULL; r->body_len = 0; r->status = 0; }
}

unsigned long long http_client_bytes_in(const http_client *c) { return c ? c->bytes_in : 0; }
int http_client_request_count(const http_client *c) { return c ? c->requests : 0; }
