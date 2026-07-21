#include "feedlib.h"
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void feed_hash_key(char *out21, const char *const *parts, int n) {
  SHA_CTX c; SHA1_Init(&c);
  for (int i = 0; i < n; i++) {
    if (!parts[i]) continue;
    SHA1_Update(&c, parts[i], strlen(parts[i]));
    SHA1_Update(&c, "|", 1);
  }
  unsigned char d[20]; SHA1_Final(d, &c);
  for (int i = 0; i < 10; i++) sprintf(out21 + i*2, "%02x", d[i]);
  out21[20] = 0;
}

char *feed_get_text(http_client *http, const char *url, int timeout_ms) {
  int own = 0;
  if (!http) { http = http_client_new(); own = 1; }
  http_response r = {0};
  const char *h[] = { "User-Agent: JapanOSINT/1.0 (+https://github.com)", NULL };
  int rc = http_request(http, "GET", url, h, NULL, 0,
                        timeout_ms > 0 ? timeout_ms : 15000, 2, &r);
  char *body = NULL;
  if (rc == 0 && r.status >= 200 && r.status < 300 && r.body)
    body = strdup(r.body);
  http_response_free(&r);
  if (own) http_client_free(http);
  return body;
}

cJSON *feed_get_json_h(http_client *http, const char *url,
                       const char *const *headers, int timeout_ms) {
  int own = 0;
  if (!http) { http = http_client_new(); own = 1; }
  http_response r = {0};
  int rc = http_request(http, "GET", url, headers, NULL, 0,
                        timeout_ms > 0 ? timeout_ms : 20000, 2, &r);
  cJSON *j = NULL;
  if (rc == 0 && r.status >= 200 && r.status < 300 && r.body)
    j = cJSON_Parse(r.body);
  http_response_free(&r);
  if (own) http_client_free(http);
  return j;
}

cJSON *feed_get_json(http_client *http, const char *url, int timeout_ms) {
  return feed_get_json_h(http, url, NULL, timeout_ms);
}

cJSON *feed_post_json(http_client *http, const char *url, const char *body,
                      const char *const *headers, int timeout_ms) {
  int own = 0;
  if (!http) { http = http_client_new(); own = 1; }
  http_response r = {0};
  int rc = http_request(http, "POST", url, headers,
                        body, body ? strlen(body) : 0,
                        timeout_ms > 0 ? timeout_ms : 20000, 2, &r);
  cJSON *j = NULL;
  if (rc == 0 && r.status >= 200 && r.status < 300 && r.body)
    j = cJSON_Parse(r.body);
  http_response_free(&r);
  if (own) http_client_free(http);
  return j;
}
