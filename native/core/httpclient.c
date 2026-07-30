#include "httpclient.h"
#include "url_override.h"
#include "evidence.h"
#include "content_change.h"
#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
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

typedef struct { char *buf; size_t len; } sbuf;

static size_t on_data(void *ptr, size_t sz, size_t nm, void *ud) {
  size_t n = sz * nm;
  sbuf *b = (sbuf *)ud;
  char *p = realloc(b->buf, b->len + n + 1);
  if (!p) return 0;
  b->buf = p;
  memcpy(b->buf + b->len, ptr, n);
  b->len += n;
  b->buf[b->len] = '\0';
  return n;
}

/* curl_global_init() is documented as not thread-safe and must run before any
 * other libcurl call. http_client_new() is reached from the scheduler thread,
 * every detached /api/search/analyze pipeline thread, the breach fetch/ingest
 * job threads, the LLM worker and the mongoose event loop — so the old
 * `static int inited` test-and-set was a genuine startup race. This is the
 * single global init; alert_deliver.c and camera_stills.c call it too. */
static pthread_once_t g_curl_once = PTHREAD_ONCE_INIT;
static void curl_boot(void) { curl_global_init(CURL_GLOBAL_DEFAULT); }
void http_client_global_init(void) { pthread_once(&g_curl_once, curl_boot); }

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
  CURL *e = curl_easy_init();
  if (!e) return 1;
  sbuf b = {0};
  struct curl_slist *hl = NULL;
  for (const char *const *h = headers; h && *h; ++h) hl = curl_slist_append(hl, *h);

  curl_easy_setopt(e, CURLOPT_URL, url);
  curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, on_data);
  curl_easy_setopt(e, CURLOPT_WRITEDATA, &b);
  curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(e, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(e, CURLOPT_TIMEOUT_MS, (long)(timeout_ms > 0 ? timeout_ms : 30000));
  curl_easy_setopt(e, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
  curl_easy_setopt(e, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(e, CURLOPT_USERAGENT, "JapanOSINT/1.0 (+native)");
  curl_easy_setopt(e, CURLOPT_NOSIGNAL, 1L);
  if (c->share) curl_easy_setopt(e, CURLOPT_SHARE, c->share);
  if (hl) curl_easy_setopt(e, CURLOPT_HTTPHEADER, hl);
  if (strcmp(method, "GET") != 0)
    curl_easy_setopt(e, CURLOPT_CUSTOMREQUEST, method);
  if (body) {
    curl_easy_setopt(e, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(e, CURLOPT_POSTFIELDSIZE, (long)body_len);
  }

  CURLcode rc = curl_easy_perform(e);
  long code = 0;
  curl_easy_getinfo(e, CURLINFO_RESPONSE_CODE, &code);
  if (hl) curl_slist_free_all(hl);
  curl_easy_cleanup(e);

  if (rc != CURLE_OK) { free(b.buf); out->status = 0; out->body = NULL; out->body_len = 0; return 1; }
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
  if (eff != url && eff && url && strcmp(eff, url) != 0) {
    fprintf(stderr, "[url-override] rewrite %s -> %s\n", url, eff);
    url = eff;
  }
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
