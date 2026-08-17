/* tests/hpengine_test.c — offline test driver for lib/hpengine.c.
 *
 *   make hptest && ./bin/hpengine_test
 *
 * The engine's whole job is: build a URL from an entity, fetch it, and turn the
 * response into intel_items without inventing anything. This driver replaces the
 * HTTP layer with a fixture table so all of that is checked deterministically
 * with no network — which is also the only way to check it in a sandbox whose
 * egress is policy-blocked.
 *
 * It links the real lib/hpengine.c and the real collectors/sources/hp_uk_deep.c
 * table, so the ABI, the registration macro and one live row's URL construction
 * are covered too. */
#include "../lib/hpengine.h"
#include "../core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── stub registry (the real one lives in registry.c) ────────────────────── */
static const source_def *g_defs[2048];
static int g_ndefs = 0;
void registry_add(const source_def *d) { if (g_ndefs < 2048) g_defs[g_ndefs++] = d; }
static const source_def *find_def(const char *id) {
  for (int i = 0; i < g_ndefs; i++) if (!strcmp(g_defs[i]->id, id)) return g_defs[i];
  return NULL;
}

/* ── stub HTTP: fixtures keyed by URL substring ──────────────────────────── */
typedef struct { const char *match, *body; long status; } fixture;
static fixture g_fx[16];
static int g_nfx = 0;
static char g_last_url[2048];
static char g_last_body[2048];
static char g_last_hdrs[1024];
static int  g_ncalls = 0;

static void fx_reset(void) { g_nfx = 0; g_ncalls = 0; g_last_url[0] = 0;
                             g_last_body[0] = 0; g_last_hdrs[0] = 0; }
static void fx_add(const char *match, long status, const char *body) {
  if (g_nfx < 16) g_fx[g_nfx++] = (fixture){ match, body, status };
}

int http_request(http_client *c, const char *method, const char *url,
                 const char *const *headers, const char *body, size_t body_len,
                 int timeout_ms, int retries, http_response *out) {
  (void)c; (void)method; (void)timeout_ms; (void)retries;
  g_ncalls++;
  snprintf(g_last_url, sizeof g_last_url, "%s", url);
  snprintf(g_last_body, sizeof g_last_body, "%.*s", (int)body_len, body ? body : "");
  g_last_hdrs[0] = 0;
  for (int i = 0; headers && headers[i]; i++) {
    strncat(g_last_hdrs, headers[i], sizeof g_last_hdrs - strlen(g_last_hdrs) - 2);
    strncat(g_last_hdrs, "\n", sizeof g_last_hdrs - strlen(g_last_hdrs) - 1);
  }
  out->status = 404; out->body = NULL; out->body_len = 0;
  for (int i = 0; i < g_nfx; i++) {
    if (strstr(url, g_fx[i].match)) {
      out->status = g_fx[i].status;
      out->body = g_fx[i].body ? strdup(g_fx[i].body) : NULL;
      out->body_len = out->body ? strlen(out->body) : 0;
      return 0;
    }
  }
  return 0;                     /* completed exchange, 404 */
}
void http_response_free(http_response *r) { if (r) { free(r->body); r->body = NULL; } }

/* ── capturing sink ──────────────────────────────────────────────────────── */
#define MAXCAP 64
typedef struct { char title[256], key[256], props[8192], link[512], rtype[64]; } cap;
static cap g_cap[MAXCAP];
static int g_ncap = 0;

static int cap_emit(struct intel_sink *s, const intel_item *it) {
  (void)s;
  if (g_ncap >= MAXCAP) return -1;
  cap *c = &g_cap[g_ncap++];
  snprintf(c->title, sizeof c->title, "%s", it->title ? it->title : "");
  snprintf(c->key,   sizeof c->key,   "%s", it->remote_key ? it->remote_key : "");
  snprintf(c->props, sizeof c->props, "%s", it->properties_json ? it->properties_json : "");
  snprintf(c->link,  sizeof c->link,  "%s", it->link ? it->link : "");
  snprintf(c->rtype, sizeof c->rtype, "%s", it->record_type ? it->record_type : "");
  return 1;
}

static int g_fail = 0;
static void ok(int cond, const char *what) {
  printf("%s  %s\n", cond ? "  ok  " : "FAIL  ", what);
  if (!cond) g_fail++;
}

static int run_source(const char *id, const char *entity) {
  g_ncap = 0;
  const source_def *d = find_def(id);
  if (!d) { printf("FAIL  no such source %s\n", id); g_fail++; return -99; }
  intel_sink sink = { .ctx = NULL, .emit = cap_emit };
  source_ctx ctx = { .source_id = id, .entity = entity };
  return d->run(&ctx, &sink);
}

/* ── the table under test ────────────────────────────────────────────────── */
static const hp_source T[] = {
  { .id = "T_JSON", .name = "json list", .url = "https://x.test/s?q={q}&d={qd}",
    .array_path = "results", .title_keys = "legalName", .id_keys = "orgno",
    .record_type = "t-company", .link_tmpl = "https://x.test/c/{v}", .link_keys = "orgno",
    .free_tier = 1, .description = "d" },

  { .id = "T_DEEP", .name = "json list + detail", .url = "https://x.test/list?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "num",
    .detail_url = "https://x.test/detail/{v}", .detail_key = "num", .detail_max = 2,
    .record_type = "t-deep", .free_tier = 1, .description = "d" },

  { .id = "T_AUTO", .name = "auto array discovery", .url = "https://x.test/auto?q={q}",
    .record_type = "t-auto", .free_tier = 1, .description = "d" },

  { .id = "T_CSV", .name = "headerless csv", .url = "https://x.test/f.csv",
    .mode = HP_CSV, .csv_no_header = 1, .filter_query = 1,
    .title_keys = "col1", .id_keys = "col0", .record_type = "t-csv",
    .free_tier = 1, .description = "d" },

  { .id = "T_HTML", .name = "html anchors", .url = "https://x.test/h?q={q}",
    .mode = HP_HTML, .href_must = "/rec/", .base = "https://x.test",
    .record_type = "t-html", .free_tier = 1, .description = "d" },

  { .id = "T_KEYED", .name = "key gated", .url = "https://x.test/k?q={q}",
    .key_env = "HP_TEST_KEY", .headers = { "Authorization: Basic {keyb64}", NULL },
    .record_type = "t-keyed", .free_tier = 1, .description = "d" },

  { .id = "T_DOMAIN_ONLY", .name = "domain gated", .url = "https://x.test/d?h={qh}",
    .want = HP_DOMAIN, .record_type = "t-dom", .free_tier = 1, .description = "d" },

  { .id = "T_POST", .name = "post body", .url = "https://x.test/p",
    .post_body = "{\"q\":\"{Q}\"}", .array_path = "hits",
    .record_type = "t-post", .free_tier = 1, .description = "d" },

  { .id = "T_ICAO", .name = "icao24 gated", .url = "https://x.test/i?h={ql}",
    .want = HP_ICAO24, .array_path = "ac", .title_keys = "r", .id_keys = "hex",
    .record_type = "t-icao", .free_tier = 1, .description = "d" },

  { .id = "T_PAGE_NEXT", .name = "next-link pagination", .url = "https://x.test/pn?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "id",
    .next_path = "next", .record_type = "t-page", .free_tier = 1, .description = "d" },

  { .id = "T_PAGE_PARAM", .name = "offset pagination", .url = "https://x.test/po?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "id",
    .page_param = "offset", .page_size = 2,
    .record_type = "t-page", .free_tier = 1, .description = "d" },

  { .id = "T_CAPPED", .name = "declared cap", .url = "https://x.test/cap?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "id", .max_items = 2,
    .record_type = "t-cap", .free_tier = 1, .description = "d" },

  { .id = "T_DEEP_ALL", .name = "deepen every record", .url = "https://x.test/da?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "num",
    .detail_url = "https://x.test/d2/{v}", .detail_key = "num",
    .record_type = "t-deepall", .free_tier = 1, .description = "d" },

  /* The three rows below declare NO paging at all. That used to mean "one
   * request, discard the rest"; it now means "walk on the upstream's own
   * evidence" (lib/pager.c), which is what the generated jsonlist collectors
   * have always done. */
  { .id = "T_PAGE_AUTO", .name = "undeclared paging, server next link",
    .url = "https://x.test/pa?q={q}", .array_path = "items",
    .title_keys = "name", .id_keys = "id",
    .record_type = "t-auto-page", .free_tier = 1, .description = "d" },

  { .id = "T_PAGE_CURSOR", .name = "undeclared paging, declared page size",
    .url = "https://x.test/pc?limit=2", .array_path = "items",
    .title_keys = "name", .id_keys = "id", .interval = 3600,
    .record_type = "t-auto-cursor", .free_tier = 1, .description = "d" },

  { .id = "T_PAGE_SHORT", .name = "undeclared paging, short first page",
    .url = "https://x.test/ps?limit=5", .array_path = "items",
    .title_keys = "name", .id_keys = "id", .interval = 3600,
    .record_type = "t-auto-short", .free_tier = 1, .description = "d" },

  { .id = "T_PAGE_SET", .name = "page_param replaces, never appends",
    .url = "https://x.test/pset?page=1&per=2", .array_path = "items",
    .title_keys = "name", .id_keys = "id", .page_param = "page", .interval = 3600,
    .record_type = "t-page-set", .free_tier = 1, .description = "d" },

  { .id = "T_TOTAL", .name = "upstream declares its own total",
    .url = "https://x.test/tot?q={q}", .array_path = "items",
    .title_keys = "name", .id_keys = "id", .max_items = 2,
    .record_type = "t-total", .free_tier = 1, .description = "d" },

  { .id = "T_ERR", .name = "upstream error", .url = "https://x.test/err?q={q}",
    .record_type = "t-err", .free_tier = 1, .description = "d" },
};
HP_REGISTER_TABLE(T)

int main(void) {
  printf("hpengine test\n");

  /* 1. token expansion + JSON list + link template + every field flattened */
  fx_reset();
  fx_add("/s?q=", 200,
    "{\"results\":[{\"legalName\":\"Acme AS\",\"orgno\":\"912345678\","
    "\"address\":{\"city\":\"Oslo\",\"zip\":\"0150\"},"
    "\"nace\":[\"62.010\",\"70.100\"]}]}");
  int rc = run_source("T_JSON", "Acme AS 912345678");
  ok(rc == 0 && g_ncap == 1, "T_JSON emitted one record");
  ok(strstr(g_last_url, "q=Acme%20AS%20912345678") != NULL, "{q} url-encoded");
  ok(strstr(g_last_url, "d=912345678") != NULL, "{qd} digits-only expansion");
  ok(!strcmp(g_cap[0].title, "Acme AS"), "title from title_keys");
  ok(strstr(g_cap[0].key, "T_JSON|912345678") != NULL, "remote_key = id|record id");
  ok(strstr(g_cap[0].props, "\"address.city\":\"Oslo\"") != NULL, "nested object flattened");
  ok(strstr(g_cap[0].props, "\"nace.0\":\"62.010\"") != NULL, "array flattened by index");
  ok(strstr(g_cap[0].props, "\"real_fetch\":true") != NULL, "provenance stamped");
  ok(strstr(g_cap[0].props, "\"endpoint\":\"https://x.test/s?q=") != NULL, "endpoint recorded");
  ok(!strcmp(g_cap[0].link, "https://x.test/c/912345678"), "link_tmpl {v} substitution");
  ok(!strcmp(g_cap[0].rtype, "t-company"), "record_type stamped");

  /* 2. second hop merges the detail document under detail.* */
  fx_reset();
  fx_add("/list?q=", 200, "{\"items\":[{\"name\":\"A\",\"num\":\"1\"},{\"name\":\"B\",\"num\":\"2\"}]}");
  fx_add("/detail/1", 200, "{\"role\":\"chair\",\"person\":{\"name\":\"Nils\"}}");
  fx_add("/detail/2", 200, "{\"role\":\"ceo\",\"person\":{\"name\":\"Ida\"}}");
  rc = run_source("T_DEEP", "acme");
  ok(rc == 0 && g_ncap == 2, "T_DEEP emitted two records");
  ok(strstr(g_cap[0].props, "\"detail.person.name\":\"Nils\"") != NULL, "detail hop merged (1)");
  ok(strstr(g_cap[1].props, "\"detail.role\":\"ceo\"") != NULL, "detail hop merged (2)");
  ok(g_ncalls == 3, "one list call + one detail call per record");

  /* 3. array auto-discovery when no array_path is declared */
  fx_reset();
  fx_add("/auto?q=", 200,
    "{\"meta\":{\"n\":2},\"payload\":{\"rows\":[{\"name\":\"R1\",\"id\":\"a\"},"
    "{\"name\":\"R2\",\"id\":\"b\"}]}}");
  rc = run_source("T_AUTO", "x");
  ok(rc == 0 && g_ncap == 2, "T_AUTO found the record array unaided");

  /* 4. headerless CSV -> col0..colN, filtered to the query */
  fx_reset();
  fx_add("/f.csv", 200, "1001,\"ACME TRADING LTD\",-0- \n1002,\"OTHER CORP\",-0- \n");
  rc = run_source("T_CSV", "ACME");
  ok(rc == 0 && g_ncap == 1, "T_CSV filter_query kept only the matching row");
  ok(strstr(g_cap[0].props, "\"col1\":\"ACME TRADING LTD\"") != NULL, "positional columns");

  /* 5. HTML mode extracts real anchors and honours href_must */
  fx_reset();
  fx_add("/h?q=", 200,
    "<html><a href=\"/nav/home\">Home page</a>"
    "<a href=\"/rec/77\">ACME TRADING LTD</a>"
    "<a href=\"/rec/77\">ACME TRADING LTD</a></html>");
  rc = run_source("T_HTML", "ACME");
  ok(rc == 0 && g_ncap == 1, "T_HTML kept one anchor (href filter + dedupe)");
  ok(!strcmp(g_cap[0].link, "https://x.test/rec/77"), "relative href resolved against base");

  /* 6. credential gating: no key -> no request, no rows, not an error */
  fx_reset();
  unsetenv("HP_TEST_KEY");
  fx_add("/k?q=", 200, "{\"a\":[{\"name\":\"n\"}]}");
  rc = run_source("T_KEYED", "acme");
  ok(rc == 0 && g_ncap == 0 && g_ncalls == 0, "missing credential = honest empty, no call");
  setenv("HP_TEST_KEY", "secret", 1);
  rc = run_source("T_KEYED", "acme");
  ok(g_ncalls == 1, "credential present = request made");
  ok(strstr(g_last_hdrs, "Authorization: Basic c2VjcmV0Og==") != NULL,
     "{keyb64} = base64(\"key:\") basic auth");

  /* 7. entity-shape gate */
  fx_reset();
  fx_add("/d?h=", 200, "{\"x\":[{\"name\":\"n\"}]}");
  rc = run_source("T_DOMAIN_ONLY", "John Smith");
  ok(rc == 0 && g_ncalls == 0, "HP_DOMAIN row skips a person name without fetching");
  rc = run_source("T_DOMAIN_ONLY", "https://acme.example/x");
  ok(g_ncalls == 1 && strstr(g_last_url, "h=acme.example") != NULL,
     "{qh} reduces a URL to its host");

  /* 8. POST with a body template */
  fx_reset();
  fx_add("/p", 200, "{\"hits\":[{\"name\":\"P\",\"id\":\"7\"}]}");
  rc = run_source("T_POST", "acme corp");
  ok(rc == 0 && g_ncap == 1, "T_POST emitted");
  ok(!strcmp(g_last_body, "{\"q\":\"acme corp\"}"), "{Q} raw in POST body");

  /* 9. failure semantics: 404 = honest empty (rc 0), 5xx = errored (rc -1) */
  fx_reset();                        /* entity has digits so {qd} resolves and
                                      * the request is really made -> real 404 */
  rc = run_source("T_JSON", "ghost 999888");
  ok(rc == 0 && g_ncap == 0 && g_ncalls == 1, "404 is an honest empty, not an error");
  fx_reset();
  rc = run_source("T_JSON", "no digits here");
  ok(rc == 0 && g_ncalls == 0, "a template token the entity cannot fill = skip, no call");
  fx_reset();
  fx_add("/err?q=", 503, "upstream down");
  rc = run_source("T_ERR", "x");
  ok(rc == -1 && g_ncap == 0, "5xx surfaces as an errored source");
  fx_reset();
  rc = run_source("T_JSON", "");
  ok(rc == 0 && g_ncalls == 0, "no entity = no work (on-demand pivot)");

  /* 9b. ICAO24 gate: a Mode-S address is 6 hex chars, not a 32/40/64 hash */
  fx_reset();
  fx_add("/i?h=", 200, "{\"ac\":[{\"hex\":\"4ca1fd\",\"r\":\"EI-ABC\"}]}");
  rc = run_source("T_ICAO", "4ca1fd");
  ok(rc == 0 && g_ncap == 1 && g_ncalls == 1, "HP_ICAO24 accepts a 6-hex Mode-S address");
  rc = run_source("T_ICAO", "Acme Airways");
  ok(g_ncalls == 1, "HP_ICAO24 rejects a name without fetching");

  /* 9c. EXHAUSTIVE USE: no implicit cap — every record in the response is used */
  fx_reset();
  fx_add("/s?q=", 200,
    "{\"results\":[{\"legalName\":\"A\",\"orgno\":\"1\"},{\"legalName\":\"B\",\"orgno\":\"2\"},"
    "{\"legalName\":\"C\",\"orgno\":\"3\"},{\"legalName\":\"D\",\"orgno\":\"4\"},"
    "{\"legalName\":\"E\",\"orgno\":\"5\"},{\"legalName\":\"F\",\"orgno\":\"6\"},"
    "{\"legalName\":\"G\",\"orgno\":\"7\"},{\"legalName\":\"H\",\"orgno\":\"8\"},"
    "{\"legalName\":\"I\",\"orgno\":\"9\"},{\"legalName\":\"J\",\"orgno\":\"10\"},"
    "{\"legalName\":\"K\",\"orgno\":\"11\"},{\"legalName\":\"L\",\"orgno\":\"12\"},"
    "{\"legalName\":\"M\",\"orgno\":\"13\"},{\"legalName\":\"N\",\"orgno\":\"14\"},"
    "{\"legalName\":\"O\",\"orgno\":\"15\"},{\"legalName\":\"P\",\"orgno\":\"16\"},"
    "{\"legalName\":\"Q\",\"orgno\":\"17\"},{\"legalName\":\"R\",\"orgno\":\"18\"},"
    "{\"legalName\":\"S\",\"orgno\":\"19\"},{\"legalName\":\"T\",\"orgno\":\"20\"},"
    "{\"legalName\":\"U\",\"orgno\":\"21\"},{\"legalName\":\"V\",\"orgno\":\"22\"},"
    "{\"legalName\":\"W\",\"orgno\":\"23\"},{\"legalName\":\"X\",\"orgno\":\"24\"},"
    "{\"legalName\":\"Y\",\"orgno\":\"25\"},{\"legalName\":\"Z\",\"orgno\":\"26\"},"
    "{\"legalName\":\"AA\",\"orgno\":\"27\"},{\"legalName\":\"AB\",\"orgno\":\"28\"}]}");
  rc = run_source("T_JSON", "many 1234");
  ok(rc == 0 && g_ncap == 28, "no declared cap = all 28 records used (was 25)");

  /* 9d. a declared cap is honoured AND disclosed as a truncation notice */
  fx_reset();
  fx_add("/cap?q=", 200,
    "{\"items\":[{\"name\":\"A\",\"id\":\"1\"},{\"name\":\"B\",\"id\":\"2\"},"
    "{\"name\":\"C\",\"id\":\"3\"},{\"name\":\"D\",\"id\":\"4\"}]}");
  rc = run_source("T_CAPPED", "x");
  ok(rc == 0 && g_ncap == 3, "declared cap emits 2 records + 1 truncation notice");
  ok(!strcmp(g_cap[2].rtype, "collector-truncation-notice"),
     "the shortfall is disclosed as a record, not just a log line");
  ok(strstr(g_cap[2].props, "\"records_available\":4") != NULL &&
     strstr(g_cap[2].props, "\"records_used\":2") != NULL,
     "notice states records_used vs records_available");
  ok(strstr(g_cap[2].props, "\"remedy\"") != NULL,
     "notice names the remedy (raise the cap) rather than just complaining");

  /* 9e. next-link pagination walks to the end */
  fx_reset();
  fx_add("/pn?q=", 200,
    "{\"items\":[{\"name\":\"p1a\",\"id\":\"1\"},{\"name\":\"p1b\",\"id\":\"2\"}],"
    "\"next\":\"https://x.test/pn2\"}");
  fx_add("/pn2", 200,
    "{\"items\":[{\"name\":\"p2a\",\"id\":\"3\"}],\"next\":null}");
  rc = run_source("T_PAGE_NEXT", "x");
  ok(rc == 0 && g_ncap == 3 && g_ncalls == 2,
     "next_path pagination reads page 2 instead of discarding it");

  /* 9f. offset pagination stops when a page comes back empty */
  fx_reset();
  fx_add("offset=2", 200, "{\"items\":[{\"name\":\"q3\",\"id\":\"3\"}]}");
  fx_add("offset=4", 200, "{\"items\":[]}");
  fx_add("/po?q=", 200,
    "{\"items\":[{\"name\":\"q1\",\"id\":\"1\"},{\"name\":\"q2\",\"id\":\"2\"}]}");
  rc = run_source("T_PAGE_PARAM", "x");
  ok(rc == 0 && g_ncap == 3, "offset pagination collects every page");
  ok(strstr(g_last_url, "offset=4") != NULL, "walk ends on the first empty page");

  /* 9g. the second hop now deepens EVERY record, not the first three */
  fx_reset();
  fx_add("/da?q=", 200,
    "{\"items\":[{\"name\":\"A\",\"num\":\"1\"},{\"name\":\"B\",\"num\":\"2\"},"
    "{\"name\":\"C\",\"num\":\"3\"},{\"name\":\"D\",\"num\":\"4\"},"
    "{\"name\":\"E\",\"num\":\"5\"}]}");
  fx_add("/d2/", 200, "{\"role\":\"member\"}");
  rc = run_source("T_DEEP_ALL", "x");
  ok(rc == 0 && g_ncap == 5 && g_ncalls == 6, "every list record gets its detail hop");
  int deep_all = 1;
  for (int i = 0; i < 5; i++)
    if (!strstr(g_cap[i].props, "\"detail.role\":\"member\"")) deep_all = 0;
  ok(deep_all, "all five records carry their detail block");

  /* 9h. a row that declares NO paging still follows a next link the server
   * published. This is the regression that mattered: moving a verified source
   * onto this engine to wire its detail hop must not cost it its later pages. */
  fx_reset();
  fx_add("/pa?q=", 200,
    "{\"items\":[{\"name\":\"a1\",\"id\":\"1\"}],"
    "\"links\":{\"next\":\"https://x.test/pa-2\"}}");
  fx_add("/pa-2", 200, "{\"items\":[{\"name\":\"a2\",\"id\":\"2\"}],\"links\":{\"next\":null}}");
  rc = run_source("T_PAGE_AUTO", "x");
  ok(rc == 0 && g_ncap == 2 && g_ncalls == 2,
     "undeclared paging follows links.next instead of dropping page 2");

  /* 9i. and advances a cursor when the URL itself declared the page size and
   * the page came back exactly that full. */
  fx_reset();
  fx_add("offset=2", 200, "{\"items\":[{\"name\":\"c3\",\"id\":\"3\"}]}");
  fx_add("/pc?limit=2", 200,
    "{\"items\":[{\"name\":\"c1\",\"id\":\"1\"},{\"name\":\"c2\",\"id\":\"2\"}]}");
  rc = run_source("T_PAGE_CURSOR", "");
  ok(rc == 0 && g_ncap == 3 && g_ncalls == 2,
     "undeclared paging advances limit/offset while pages come back full");
  ok(strstr(g_last_url, "offset=2") != NULL, "cursor advanced by the declared page size");

  /* 9j. a SHORT first page is the upstream saying it is finished. Following it
   * would be inventing a page that was never offered. */
  fx_reset();
  fx_add("/ps?limit=5", 200, "{\"items\":[{\"name\":\"s1\",\"id\":\"1\"}]}");
  rc = run_source("T_PAGE_SHORT", "");
  ok(rc == 0 && g_ncap == 1 && g_ncalls == 1,
     "a short page stops the walk — no guessed second request");

  /* 9k. page_param SETS its parameter. Appending built page=1&page=2&page=3 and
   * left the winner to the server. */
  fx_reset();
  fx_add("page=2", 200, "{\"items\":[{\"name\":\"g2\",\"id\":\"2\"}]}");
  fx_add("page=3", 200, "{\"items\":[]}");
  fx_add("/pset?page=1", 200, "{\"items\":[{\"name\":\"g1\",\"id\":\"1\"}]}");
  rc = run_source("T_PAGE_SET", "");
  ok(rc == 0 && g_ncap == 2, "page_param walk collects both pages");
  ok(strstr(g_last_url, "page=3") != NULL && strstr(g_last_url, "page=1") == NULL,
     "page_param replaced the existing page= rather than appending a second one");

  /* 9l. when the upstream declares a total, the shortfall notice reports THAT,
   * not just the records we happened to count. */
  fx_reset();
  fx_add("/tot?q=", 200,
    "{\"total_count\":97,\"items\":[{\"name\":\"t1\",\"id\":\"1\"},"
    "{\"name\":\"t2\",\"id\":\"2\"},{\"name\":\"t3\",\"id\":\"3\"}]}");
  rc = run_source("T_TOTAL", "x");
  ok(rc == 0 && g_ncap == 3, "capped row emits 2 records + 1 notice");
  ok(strstr(g_cap[2].props, "\"records_available\":97") != NULL,
     "notice reports the upstream's declared total, not the page it saw");
  ok(strstr(g_cap[2].props, "upstream declared this total") != NULL,
     "notice states where that total came from");

  /* 10. a real shipped row: Companies House PSC (from hp_uk_deep.c) */
  fx_reset();
  setenv("COMPANIES_HOUSE_API_KEY", "chkey", 1);
  fx_add("/persons-with-significant-control", 200,
    "{\"items\":[{\"name\":\"J SMITH\",\"notified_on\":\"2019-04-01\","
    "\"natures_of_control\":[\"ownership-of-shares-75-to-100-percent\"]}]}");
  rc = run_source("UK_CH_PSC", "00445790");
  /* The row declares start_index paging, and this fixture answers every page,
   * so the walk runs to the page ceiling and then DISCLOSES it — 10 records
   * plus one truncation notice. Before the exhaustive-use work this row would
   * have read page 1 only and reported nothing about the rest. */
  ok(rc == 0 && g_ncap == 11, "UK_CH_PSC paginated and disclosed the page ceiling");
  ok(!strcmp(g_cap[10].rtype, "collector-truncation-notice"),
     "page-ceiling stop is disclosed as a record");
  ok(strstr(g_last_url, "/company/00445790/persons-with-significant-control") != NULL,
     "UK_CH_PSC built the documented CH path");
  ok(strstr(g_cap[0].props, "natures_of_control.0") != NULL,
     "PSC control bands preserved in properties");
  const source_def *psc = find_def("UK_CH_PSC");
  ok(psc && psc->update_interval_sec == 0 && psc->layer == NULL,
     "shipped rows are on-demand pivots and never map layers");

  printf(g_fail ? "\n%d FAILURES\n" : "\nall passed\n", g_fail);
  return g_fail ? 1 : 0;
}
