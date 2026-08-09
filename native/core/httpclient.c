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
#include <pthread.h>

typedef struct { char *host; int requests; int ok; } host_log;

struct http_client { CURLSH *share; host_log *hosts; int n_hosts, cap_hosts; };

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

static int do_once(http_client *c, const char *method, const char *url,
                   const char *const *headers, const char *body,
                   size_t body_len, int timeout_ms, http_response *out) {
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
  for (const char *const *h = headers; h && *h; ++h) hl = curl_slist_append(hl, *h);

  curl_easy_setopt(e, CURLOPT_URL, url);
  curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, on_data);
  curl_easy_setopt(e, CURLOPT_WRITEDATA, &b);
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
  curl_easy_setopt(e, CURLOPT_USERAGENT, JO_USER_AGENT);
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
  if (hl) curl_slist_free_all(hl);
  curl_easy_cleanup(e);

  if (rc != CURLE_OK) {
    if (b.over || rc == CURLE_FILESIZE_EXCEEDED)
      fprintf(stderr, "[http] %s: response exceeded %zu byte ceiling\n",
              url, b.cap);
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
  const char *stripped[24];
  if (eff != url && eff && url && strcmp(eff, url) != 0) {
    fprintf(stderr, "[url-override] rewrite %s -> %s\n", url, eff);
    if (!hostgate_same_host(url, eff) && headers && *headers) {
      int n = 0, dropped = 0;
      for (const char *const *h = headers; *h && n < 23; ++h) {
        if (header_is_credential(*h)) { dropped++; continue; }
        stripped[n++] = *h;
      }
      stripped[n] = NULL;
      if (dropped) {
        fprintf(stderr, "[url-override] dropped %d auth header(s): host "
                        "changed\n", dropped);
        eff_headers = stripped;
      }
    }
    url = eff;
  }
  headers = eff_headers;
  int attempt = 0;
  for (;;) {
    http_response r = {0};
    int hard = do_once(c, method, url, headers, body, body_len, timeout_ms, &r);
    int retryable = hard || (r.status >= 500 && r.status <= 599) || r.status == 429;
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
      return hard && attempt >= retries ? 1 : 0;
    }
    http_response_free(&r);
    /* exponential backoff: 250ms, 500ms, 1s, ... capped at 4s */
    int ms = 250 << attempt;
    if (ms > 4000) ms = 4000;
    usleep((useconds_t)ms * 1000);
    attempt++;
  }
}

void http_response_free(http_response *r) {
  if (r && r->body) { free(r->body); r->body = NULL; r->body_len = 0; r->status = 0; }
}
