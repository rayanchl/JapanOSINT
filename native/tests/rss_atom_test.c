/* tests/rss_atom_test.c — lib/rss_atom.c's guarantees, offline.
 *
 * WHAT IS BEING PROTECTED, and why this file exists at all.
 *
 * `rss_collect()` returns the number of items it emitted, and the scheduler
 * reads a non-negative return as success. So a feed URL that has stopped being
 * a feed reports `rc=0 records=0 stored=0` — byte for byte what a newspaper
 * that published nothing today looks like. Measured 2026-10-23 from the
 * registry sweep's EMITS_NOTHING column: `fijitimes.com.fj/feed/` answers HTTP
 * 200 with `text/html` and 128,800 bytes of the site's HOMEPAGE, and
 * nation-thailand, daily-mirror-lk and cyb-cybereason do the same with 107 KB,
 * 70 KB and 106 KB. Four national press feeds, dead, and nothing anywhere said
 * so — house rule 1's invisible nothing, after the zero-byte body and the
 * refused fetch.
 *
 * The rule added for it is narrow on purpose and these fixtures are what pin
 * that: it fires ONLY when the parse found no items, and a body carrying
 * `<item` or `<entry` is a feed whatever its root is called, so no run that
 * emitted a record can be affected and no feed this parser handles is failed.
 *
 * It is a standalone target (`make rsstest`) rather than a tests/unit/ file for
 * the reason hpengine_test.c is: `tests/unit/run.sh` links every object, so a
 * test there cannot stub http_request without a duplicate symbol.
 */
#include "../lib/rss_atom.h"
#include "../core/httpclient.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
static void ok(int cond, const char *what) {
  printf("  %s  %s\n", cond ? "ok  " : "FAIL", what);
  if (!cond) g_fail++;
}

/* ── stub HTTP: one fixture body, served to every request ───────────────── */
static const char *g_body = NULL;
static long        g_status = 200;

int http_request(http_client *c, const char *method, const char *url,
                 const char *const *headers, const char *body, size_t body_len,
                 int timeout_ms, int retries, http_response *out) {
  (void)c; (void)method; (void)url; (void)headers; (void)body; (void)body_len;
  (void)timeout_ms; (void)retries;
  out->status = g_status;
  if (!g_body) { out->body = NULL; out->body_len = 0; return 0; }
  out->body_len = strlen(g_body);
  out->body = malloc(out->body_len + 1);
  memcpy(out->body, g_body, out->body_len + 1);
  return 0;
}
void http_response_free(http_response *r) { if (r) { free(r->body); r->body = NULL; } }
int http_last_transport_error(void) { return HTTP_TE_NONE; }
int http_last_tls_cause(void) { return HTTP_TLS_NONE; }
const char *http_tls_cause_label(int cause) { (void)cause; return ""; }
const char *http_tls_cause_remedy(int cause) { (void)cause; return ""; }
/* lib/feedlib.c and lib/hpengine.c are on the link line (rss_atom.c calls
 * feed_url_host_is_jp and hp_xml_decode), and they reach the rest of the HTTP
 * client and the registry. None of that is exercised here — every fetch in this
 * test goes through the http_request above — so they are stubbed rather than
 * dragged in, exactly as tests/hpengine_test.c stubs the same two. */
int http_request_max(http_client *c, const char *method, const char *url,
                     const char *const *headers, const char *body, size_t body_len,
                     int timeout_ms, int retries, size_t max_body,
                     http_response *out) {
  (void)max_body;
  return http_request(c, method, url, headers, body, body_len, timeout_ms,
                      retries, out);
}
http_client *http_client_new(void) { return NULL; }
void http_client_free(http_client *c) { (void)c; }
const char *url_override_apply(const char *url) { return url; }
/* The per-host agent table (core/host_agents.def) is a property of the real
 * client; what goes on the wire is not what this test is about. */
const char *http_ua_override(const char *url) { (void)url; return NULL; }
void registry_add(const source_def *d) { (void)d; }

/* ── a sink that counts ─────────────────────────────────────────────────── */
static int g_emitted = 0;
static char g_last_rtype[64];
static int emit_count(intel_sink *s, const intel_item *it) {
  (void)s;
  g_emitted++;
  snprintf(g_last_rtype, sizeof g_last_rtype, "%s",
           it->record_type ? it->record_type : "");
  return 0;
}

static int run(const char *body, long status) {
  g_body = body; g_status = status; g_emitted = 0; g_last_rtype[0] = 0;
  intel_sink sink = { .ctx = NULL, .emit = emit_count };
  source_ctx ctx = { 0 };
  ctx.source_id = "RSS_TEST";
  return rss_collect(&ctx, &sink, "https://feed.example/rss", "en", "[]");
}

#define RSS_TWO \
  "<?xml version=\"1.0\"?><rss version=\"2.0\"><channel>" \
  "<title>Example</title>" \
  "<item><title>One</title><link>https://e.example/1</link>" \
  "<guid>g1</guid><pubDate>Tue, 21 Oct 2025 07:28:00 GMT</pubDate></item>" \
  "<item><title>Two</title><link>https://e.example/2</link>" \
  "<guid>g2</guid><pubDate>Tue, 21 Oct 2025 08:28:00 GMT</pubDate></item>" \
  "</channel></rss>"

/* The real thing, trimmed: what fijitimes.com.fj/feed/ served on 2026-10-23. */
#define HTML_HOMEPAGE \
  "<!DOCTYPE html>\n<html lang=\"en-US\">\n<head>\n\t<meta charset=\"UTF-8\" />\n" \
  "<title>The Fiji Times</title></head><body><div class=\"site\">" \
  "<a href=\"/\">Home</a><a href=\"/news/\">News</a></body></html>"

int main(void) {
  int rc;

  /* 1. The control: a feed with items is unaffected by any of this. */
  rc = run(RSS_TWO, 200);
  ok(rc == 2 && g_emitted == 2, "1: a feed with two items emits two");

  /* 2. A VALID feed that happens to be empty is an HONEST EMPTY and must stay
   *    one. This is the fixture that keeps the new rule narrow: without it the
   *    rule could be "zero items is a failure", which would put a notice on
   *    every quiet feed in the fleet. */
  rc = run("<?xml version=\"1.0\"?><rss version=\"2.0\"><channel>"
           "<title>Quiet</title></channel></rss>", 200);
  ok(rc == 0 && g_emitted == 0, "2: a valid feed with no items is an empty, not a failure");

  /* 2b. …and the same for Atom and RSS 1.0, whose roots are different words. */
  rc = run("<?xml version=\"1.0\"?><feed xmlns=\"http://www.w3.org/2005/Atom\">"
           "<title>Quiet</title></feed>", 200);
  ok(rc == 0, "2b: an empty Atom feed is an empty, not a failure");
  rc = run("<?xml version=\"1.0\"?><rdf:RDF xmlns:rdf=\"x\"><channel/></rdf:RDF>", 200);
  ok(rc == 0, "2b: an empty RSS 1.0 document is an empty, not a failure");

  /* 3. THE FINDING. A 200 carrying the site's HTML homepage is a dead feed
   *    path, not a quiet publisher, and it must not read as success. */
  rc = run(HTML_HOMEPAGE, 200);
  ok(rc == -1, "3: a 200 serving an HTML page is a failed fetch, not an empty feed");

  /* 3b. And a 200 carrying something that is neither — JSON, a plain-text
   *     error — is also not a feed this parser can read. */
  rc = run("{\"error\":\"feed retired\",\"see\":\"/api/v2\"}", 200);
  ok(rc == -1, "3b: a 200 serving JSON is a failed fetch too");

  /* 4. THE ESCAPE HATCH. A document whose ROOT this parser does not recognise
   *    but which carries entries IS a feed, and must not be failed — the
   *    parser's own vocabulary decides, not a list of root names. A namespaced
   *    root is the case that matters, since `<atom:feed` still contains
   *    "<feed"; this uses a root that shares nothing with the list. */
  rc = run("<?xml version=\"1.0\"?><nitf:channel xmlns:nitf=\"x\">"
           "<item><title>Still an item</title><guid>g9</guid></item>"
           "</nitf:channel>", 200);
  ok(rc >= 0, "4: a document carrying <item> is a feed whatever its root is called");

  /* 5. The two cases that already failed before this change, re-pinned so the
   *    new branch cannot have swallowed them. */
  rc = run(NULL, 202);
  ok(rc == -1, "5: a 2xx with a zero-byte body is a failed fetch (europarl's WAF)");
  rc = run(RSS_TWO, 404);
  ok(rc == -1, "5: a non-2xx is a failed fetch");

  printf(g_fail ? "\n%d FAILURES\n" : "\nall passed\n", g_fail);
  return g_fail ? 1 : 0;
}
