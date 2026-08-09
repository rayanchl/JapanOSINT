#include "feedlib.h"
#include "csv.h"           /* csv_is_utf8 / csv_decode_sjis */
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

/* Is `url` on a Japanese host? Used to decide whether a body that is not valid
 * UTF-8 should be read as Shift_JIS.
 *
 * The host test is the whole point. jo_get() transcodes ANY invalid-UTF-8 body,
 * which is right for the JP-government scrapers it was written for and wrong
 * everywhere else: Latin-1 is also invalid UTF-8, and a Latin-1 byte pair like
 * `FC 72` in "Zürich" is a perfectly valid Shift_JIS sequence, so a blanket
 * transcode turns European feeds into kanji. Gating on the host fixes the real
 * defect (customs.go.jp / soumu.go.jp serve Shift_JIS with no charset header,
 * and those bytes were persisted verbatim into TEXT columns and their FTS
 * mirror) without inventing a new one for the ~700 non-JP feeds. */
int feed_url_host_is_jp(const char *url) {
  if (!url) return 0;
  const char *h = strstr(url, "://");
  h = h ? h + 3 : url;
  /* userinfo: "user.jp@evil.com" must resolve to evil.com, not to .jp */
  const char *scan = h, *at = NULL;
  while (*scan && *scan != '/' && *scan != '?' && *scan != '#') {
    if (*scan == '@') at = scan;
    scan++;
  }
  if (at) h = at + 1;
  const char *end = h;
  while (*end && *end != '/' && *end != '?' && *end != '#' && *end != ':') end++;
  size_t n = (size_t)(end - h);
  /* Case-insensitive: a host is case-insensitive by definition, and "…GO.JP"
   * appears in hand-written collector URLs. A byte compare here would fail
   * open on exactly the hosts this gate exists for. */
  return n >= 3 && end[-3] == '.' &&
         (end[-2] == 'j' || end[-2] == 'J') &&
         (end[-1] == 'p' || end[-1] == 'P');
}

char *feed_get_text(http_client *http, const char *url, int timeout_ms) {
  int own = 0;
  if (!http) { http = http_client_new(); own = 1; }
  http_response r = {0};
  /* No explicit UA header: httpclient sets JO_USER_AGENT on every request, so
   * overriding here only made this path differ from feed_get_json's — and the
   * string it used, "(+https://github.com)", was a placeholder that points at
   * nothing, which is worse than the shared one for any host that checks. */
  int rc = http_request(http, "GET", url, NULL, NULL, 0,
                        timeout_ms > 0 ? timeout_ms : 15000, 2, &r);
  char *body = NULL;
  if (rc == 0 && r.status >= 200 && r.status < 300 && r.body) {
    size_t n = strlen(r.body);
    /* Only when the host is Japanese AND the bytes are not already UTF-8.
     * csv_decode_sjis fails closed (returns a verbatim copy) if the body is
     * not decodable Shift_JIS, so a JP host serving something else is safe. */
    if (n && feed_url_host_is_jp(url) && !csv_is_utf8(r.body, n))
      body = csv_decode_sjis(r.body, n);
    if (!body) body = strdup(r.body);
  }
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
