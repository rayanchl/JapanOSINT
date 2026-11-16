/* libFuzzer harness: lib/rss_atom.c's parser, through its only entry point,
 * rss_collect(). The network is stubbed exactly as tests/rss_atom_test.c stubs
 * it: every request is answered with the fuzz input as a 200 body. */
#include "../../lib/rss_atom.h"
#include "../../core/httpclient.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t *g_data; static size_t g_size;

int http_request(http_client *c, const char *method, const char *url,
                 const char *const *headers, const char *body, size_t body_len,
                 int timeout_ms, int retries, http_response *out) {
  (void)c; (void)method; (void)url; (void)headers; (void)body; (void)body_len;
  (void)timeout_ms; (void)retries;
  out->status = 200;
  out->body_len = g_size;
  out->body = malloc(g_size + 1);
  if (!out->body) { out->body_len = 0; return -1; }
  memcpy(out->body, g_data, g_size);
  out->body[g_size] = 0;
  return 0;
}
void http_response_free(http_response *r) { if (r) { free(r->body); r->body = NULL; } }
int http_last_transport_error(void) { return HTTP_TE_NONE; }
int http_last_tls_cause(void) { return HTTP_TLS_NONE; }
const char *http_credential_refusal(void) { return NULL; }
void http_credential_refusal_reset(void) {}
const char *http_tls_cause_label(int cause) { (void)cause; return ""; }
const char *http_tls_cause_remedy(int cause) { (void)cause; return ""; }
int http_request_max(http_client *c, const char *method, const char *url,
                     const char *const *headers, const char *body, size_t body_len,
                     int timeout_ms, int retries, size_t max_body,
                     http_response *out) {
  (void)max_body;
  return http_request(c, method, url, headers, body, body_len, timeout_ms, retries, out);
}
http_client *http_client_new(void) { return NULL; }
void http_client_free(http_client *c) { (void)c; }
const char *url_override_apply(const char *url) { return url; }
const char *http_ua_override(const char *url) { (void)url; return NULL; }
void registry_add(const source_def *d) { (void)d; }

static int emit_nop(intel_sink *s, const intel_item *it) { (void)s; (void)it; return 0; }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  g_data = data; g_size = size;
  intel_sink sink = { .ctx = NULL, .emit = emit_nop };
  source_ctx ctx = { 0 };
  ctx.source_id = "FUZZ_RSS";
  (void)rss_collect(&ctx, &sink, "https://feed.example/rss", "en", "[]");
  return 0;
}
