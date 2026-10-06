/* tests/unit/test_http_retry_after.c — Retry-After handling in
 * core/httpclient.c. No network.
 *
 * WHAT IS BEING PROTECTED. http_request() retried 429/5xx after 250·2^n ms
 * and never read Retry-After, so a server asking for two minutes got the
 * remaining retries within seconds. Now:
 *   1. both header forms parse (delta-seconds and HTTP-date), garbage is -1;
 *   2. the decision: within 30 s AND within the request's timeout → sleep that
 *      long and retry; longer → do not retry; other statuses → not governed;
 *   3. the header callback resets per response, so a redirect hop's
 *      Retry-After cannot attach to the final response;
 *   4. the delay reaches the host gate (hostgate_penalize).
 *
 * Includes httpclient.c to reach the static helpers. */

#include "../../core/httpclient.c"

#include <assert.h>

static void test_parse(void) {
  time_t now = curl_getdate("Wed, 21 Oct 2015 07:28:00 GMT", NULL);
  assert(now != (time_t)-1);
  assert(retry_after_parse_ms("120", now) == 120000);
  assert(retry_after_parse_ms(" 5 \r\n", now) == 5000);
  assert(retry_after_parse_ms("0", now) == 0);
  /* IMF-fixdate, RFC 850 and asctime — all three RFC 9110 spellings. */
  assert(retry_after_parse_ms("Wed, 21 Oct 2015 07:28:30 GMT", now) == 30000);
  assert(retry_after_parse_ms("Wednesday, 21-Oct-15 07:29:00 GMT", now) == 60000);
  assert(retry_after_parse_ms("Wed Oct 21 07:28:10 2015", now) == 10000);
  /* a date already past means "now", not a negative sleep */
  assert(retry_after_parse_ms("Wed, 21 Oct 2015 07:00:00 GMT", now) == 0);
  /* garbage and negative values are not a delay */
  assert(retry_after_parse_ms("soon", now) == -1);
  assert(retry_after_parse_ms("-5", now) == -1);
  assert(retry_after_parse_ms("", now) == -1);
  assert(retry_after_parse_ms(NULL, now) == -1);
  /* hostile huge values clamp to a day instead of overflowing */
  assert(retry_after_parse_ms("99999999999999999999", now) == 86400000L);
  assert(retry_after_parse_ms("172800", now) == 86400000L);
}

static void test_decision(void) {
  assert(retry_after_decision(429, 10000, 30000) == 10000);
  assert(retry_after_decision(503, 0, 30000) == 0);
  assert(retry_after_decision(429, 30000, 0) == 30000);   /* default timeout */
  assert(retry_after_decision(429, 45000, 0) == -1);      /* past 30 s       */
  assert(retry_after_decision(503, 20000, 15000) == -1);  /* past timeout    */
  assert(retry_after_decision(500, 1000, 30000) == -2);   /* not governed    */
  assert(retry_after_decision(200, 1000, 30000) == -2);
  assert(retry_after_decision(429, -1, 30000) == -2);     /* no header       */
}

static size_t feed(hdr_state *h, const char *line) {
  char buf[256];
  snprintf(buf, sizeof buf, "%s", line);
  return on_header(buf, 1, strlen(buf), h);
}

static void test_header_callback(void) {
  hdr_state h = { -1 };
  assert(feed(&h, "HTTP/1.1 429 Too Many Requests\r\n") == strlen("HTTP/1.1 429 Too Many Requests\r\n"));
  feed(&h, "Content-Type: text/plain\r\n");
  assert(h.retry_after_ms == -1);
  feed(&h, "retry-after: 7\r\n");                   /* case-insensitive name */
  assert(h.retry_after_ms == 7000);
  /* next hop: a new status line forgets the previous response's header */
  feed(&h, "HTTP/2 200\r\n");
  assert(h.retry_after_ms == -1);
  feed(&h, "Retry-After: nonsense\r\n");
  assert(h.retry_after_ms == -1);
  feed(&h, "Retry-After-Extra: 9\r\n");             /* different header       */
  assert(h.retry_after_ms == -1);
}

static void test_penalty_reaches_gate(void) {
  hostgate_stats_t a, b;
  hostgate_stats(&a);
  hostgate_penalize("https://ratelimited.example/api", 1500);
  hostgate_stats(&b);
  assert(b.penalties == a.penalties + 1);
}

int main(void) {
  test_parse();
  test_decision();
  test_header_callback();
  test_penalty_reaches_gate();
  printf("test_http_retry_after: OK\n");
  return 0;
}
