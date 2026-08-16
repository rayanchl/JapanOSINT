#include "feedlib.h"
#include "csv.h"           /* csv_is_utf8 / csv_decode_sjis */
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The two joined-SHA-1 shapes this tree needs, in one place.
 *
 * `sep_between` picks between them: 0 appends the separator AFTER every part
 * (intelHashKey's shape, so "a|b|" — a trailing pipe is part of the hashed
 * input and removing it would change every uid ever minted), 1 puts it only
 * BETWEEN parts ("a|b", which is what station_clusterer's cluster_uid hashes).
 * `hex_chars` is how much of the digest to emit; the caller's buffer must hold
 * hex_chars + 1.
 *
 * EVP rather than SHA1_Init/Update/Final: those are deprecated in OpenSSL 3
 * and were the single largest source of build warnings in the tree (26 of 88),
 * repeated across five hand-rolled copies of this same loop. SHA-1 is used
 * here only to derive stable record keys — never as a security primitive —
 * so the algorithm choice is a compatibility constraint, not a weakness: it
 * has to keep matching the uids already in the database. */
static void hash_join(char *out, const char *const *parts, int n,
                      int sep_between, int hex_chars) {
  unsigned char d[EVP_MAX_MD_SIZE];
  unsigned int dlen = 0;
  EVP_MD_CTX *c = EVP_MD_CTX_new();
  if (c && EVP_DigestInit_ex(c, EVP_sha1(), NULL) == 1) {
    for (int i = 0, emitted = 0; i < n; i++) {
      if (!parts[i]) continue;
      if (sep_between && emitted) EVP_DigestUpdate(c, "|", 1);
      EVP_DigestUpdate(c, parts[i], strlen(parts[i]));
      if (!sep_between) EVP_DigestUpdate(c, "|", 1);
      emitted++;
    }
    EVP_DigestFinal_ex(c, d, &dlen);
  }
  EVP_MD_CTX_free(c);
  /* An allocation failure inside OpenSSL leaves dlen 0; zero-fill rather than
   * emit uninitialised stack as a record key. */
  if (dlen == 0) memset(d, 0, sizeof d);
  for (int i = 0; i < hex_chars / 2; i++) sprintf(out + i*2, "%02x", d[i]);
  out[hex_chars] = 0;
}

void feed_hash_key(char *out21, const char *const *parts, int n) {
  hash_join(out21, parts, n, 0, 20);
}

void feed_hash_join(char *out41, const char *const *parts, int n) {
  hash_join(out41, parts, n, 1, 40);
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
