/* core/httpclient.h — libcurl wrapper (subset port of OSINTsaas http_client.c).
 * P1: GET/POST, headers, timeout, exponential-backoff retry on transport
 * errors / 5xx. Response cache + per-domain rate-limit land in P4/P5. */
#ifndef JO_HTTPCLIENT_H
#define JO_HTTPCLIENT_H

/* THE User-Agent for every outbound fetch. One string, defined once.
 *
 * There were three: httpclient's "JapanOSINT/1.0 (+native)", feedlib's
 * "JapanOSINT/1.0 (+https://github.com)" (a placeholder URL that resolves to
 * nothing), and rss_atom's descriptive one. Only the last identifies the
 * client and offers a contact route, which is what the audit's defect #14 was
 * about: ReliefWeb blocklists a bare "japanosint" UA, and the documented
 * remedy is a descriptive UA with a contact address — NOT browser spoofing.
 * That fix reached rss_atom.c only, so every JSON and text fetch in the fleet
 * still went out with an anonymous UA. */
#define JO_USER_AGENT \
  "JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; " \
  "feed collector; contact via repo issues)"

#include <stddef.h>

typedef struct {
  long  status;       /* HTTP status, 0 on transport failure */
  char *body;         /* malloc'd, NUL-terminated; caller frees */
  size_t body_len;
} http_response;

typedef struct http_client http_client; /* opaque (shared curl share/handle) */

/* The one curl_global_init() in the process. Idempotent and thread-safe, so
 * callers that reach curl WITHOUT going through http_client_new() (e.g. the
 * camera-stills grabber, which drives a raw easy handle) can call it directly
 * instead of racing a second global init on a worker thread. */
void         http_client_global_init(void);

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

/* Per-client log of the distinct hosts contacted (every http_request goes
 * through one client). Lets the OSINT dispatcher attribute a service's output
 * to the real upstream providers it hit — automatic source attribution for
 * every HTTP collector, no per-collector code. The dispatcher creates a fresh
 * client per service call, so the log is naturally scoped to that call. */
int  http_client_host_count(http_client *c);
/* i-th distinct host (0-based). Writes request count + whether any response
 * was 2xx/3xx into the out params (either may be NULL). Returns NULL if i is
 * out of range. The pointer is owned by the client. */
const char *http_client_host_at(http_client *c, int i,
                                 int *out_requests, int *out_ok);

#endif
