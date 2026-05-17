#include "httpclient.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct http_client { CURLSH *share; };

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

http_client *http_client_new(void) {
  static int inited = 0;
  if (!inited) { curl_global_init(CURL_GLOBAL_DEFAULT); inited = 1; }
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
  free(c);
}

static int do_once(http_client *c, const char *method, const char *url,
                   const char *const *headers, const char *body,
                   size_t body_len, int timeout_ms, http_response *out) {
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
  int attempt = 0;
  for (;;) {
    http_response r = {0};
    int hard = do_once(c, method, url, headers, body, body_len, timeout_ms, &r);
    int retryable = hard || (r.status >= 500 && r.status <= 599) || r.status == 429;
    if (!retryable || attempt >= retries) { *out = r; return hard && attempt >= retries ? 1 : 0; }
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
