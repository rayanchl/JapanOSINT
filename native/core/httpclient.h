/* core/httpclient.h — libcurl wrapper (subset port of OSINTsaas http_client.c).
 * P1: GET/POST, headers, timeout, exponential-backoff retry on transport
 * errors / 5xx. Response cache + per-domain rate-limit land in P4/P5. */
#ifndef JO_HTTPCLIENT_H
#define JO_HTTPCLIENT_H

#include <stddef.h>

typedef struct {
  long  status;       /* HTTP status, 0 on transport failure */
  char *body;         /* malloc'd, NUL-terminated; caller frees */
  size_t body_len;
} http_response;

typedef struct http_client http_client; /* opaque (shared curl share/handle) */

http_client *http_client_new(void);
void         http_client_free(http_client *);

/* method: "GET"/"POST"/... ; headers: NULL-terminated array or NULL;
 * body/body_len for POST (may be NULL). timeout_ms<=0 → 30000. retries: extra
 * attempts on transport error or 5xx (exp backoff). Returns 0 on a completed
 * HTTP exchange (any status), non-zero only on hard failure. */
int http_request(http_client *c, const char *method, const char *url,
                  const char *const *headers, const char *body, size_t body_len,
                  int timeout_ms, int retries, http_response *out);

void http_response_free(http_response *r);

#endif
