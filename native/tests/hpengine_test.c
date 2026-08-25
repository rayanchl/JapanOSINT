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

  /* A hypermedia API whose link array is NOT ordered with `next` first. Selecting
   * it positionally is what breaks when a server reorders its links. */
  { .id = "T_PAGE_REL", .name = "next selected by rel", .url = "https://x.test/pr?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "id",
    .next_path = "links.rel=next.href",
    .record_type = "t-page", .free_tier = 1, .description = "d" },

  /* XML: the shape the primary sanctions lists actually ship in. `target` is
   * the record; each carries leaf fields and one nested block. */
  { .id = "T_XML", .name = "xml records", .url = "https://x.test/x?q={q}",
    .mode = HP_XML, .array_path = "target",
    .title_keys = "name", .id_keys = "ref",
    .record_type = "t-xml", .free_tier = 1, .description = "d" },

  /* XML with no array_path: the engine must find the repeated element itself. */
  { .id = "T_XML_AUTO", .name = "xml autodetect", .url = "https://x.test/xa?q={q}",
    .mode = HP_XML, .title_keys = "name",
    .record_type = "t-xml", .free_tier = 1, .description = "d" },

  /* A 0-based page-numbered API. Without page_zero_based the engine coerces the
   * unset page_start to 1 and computes the first extra page as 2, skipping 1. */
  { .id = "T_PAGE_ZERO", .name = "zero-based paging", .url = "https://x.test/pz?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "id",
    .page_param = "page", .page_zero_based = 1,
    .record_type = "t-page", .free_tier = 1, .description = "d" },

  { .id = "T_CAPPED", .name = "declared cap", .url = "https://x.test/cap?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "id", .max_items = 2,
    .record_type = "t-cap", .free_tier = 1, .description = "d" },

  { .id = "T_DEEP_ALL", .name = "deepen every record", .url = "https://x.test/da?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "num",
    .detail_url = "https://x.test/d2/{v}", .detail_key = "num",
    .record_type = "t-deepall", .free_tier = 1, .description = "d" },

  { .id = "T_ERR", .name = "upstream error", .url = "https://x.test/err?q={q}",
    .record_type = "t-err", .free_tier = 1, .description = "d" },

  /* A headerless CSV that declares NO title_keys/id_keys — the DataPlane shape.
   * Columns parse as col0..colN, which match no fallback list, so before the
   * first-scalar rescue every record was dropped and the run reported
   * "emitted 0 of 36166". */
  /* .interval is not decoration: a static URL with no interval references no
   * entity token and is never scheduled, so hp_run returns 0 without fetching
   * (rule 3). A bulk file row must declare a cadence to exist at all. */
  { .id = "T_CSV_BARE", .name = "headerless csv, no keys declared",
    .url = "https://x.test/bare.csv", .mode = HP_CSV, .csv_no_header = 1,
    .interval = 3600,
    .record_type = "t-bare", .free_tier = 1, .description = "d" },

  /* Conventional key names holding JSON NUMBERS (SEC's cik, RIPEstat's number,
   * BrandMeister's id). cJSON_IsString alone was blind to every one of them. */
  { .id = "T_NUM_ID", .name = "numeric identifier", .url = "https://x.test/n?q={q}",
    .array_path = "items", .record_type = "t-num",
    .free_tier = 1, .description = "d" },

  /* Genuinely empty records must STILL be dropped — the rescue widens what
   * counts as content, it does not remove the noise floor. */
  { .id = "T_EMPTY_REC", .name = "no content at all", .url = "https://x.test/e?q={q}",
    .array_path = "items", .record_type = "t-empty",
    .free_tier = 1, .description = "d" },

  /* A row that DECLARES where its records live. Batch 20 found ArcGIS
   * answering an over-quota query with HTTP 200 and {"error":{"code":429}}:
   * `results` did not resolve, so the engine fell through to "root IS the
   * record" and filed a finding titled `<record_type> 429`. */
  { .id = "T_ERRDOC", .name = "declared shape", .url = "https://x.test/ed?q={q}",
    .array_path = "results", .title_keys = "name", .id_keys = "id",
    .record_type = "t-errdoc", .free_tier = 1, .description = "d" },

  /* SDMX structural metadata: the identity is in ATTRIBUTES (id=, agencyID=),
   * not in child elements. hp_xml_flatten walked child ELEMENTS only, so the
   * whole ILO/OECD/ABS/Istat/ECB/Eurostat/IMF family parsed into records with
   * a human-readable name and nothing to key it on. */
  { .id = "T_XML_ATTR", .name = "xml attributes", .url = "https://x.test/xat?q={q}",
    .mode = HP_XML, .array_path = "Codelist",
    .title_keys = "Name", .id_keys = "id",
    .record_type = "t-xmlattr", .free_tier = 1, .description = "d" },

  /* The same thing against the real bytes, namespace prefixes and all — the
   * fixture below is a trimmed copy of what data-api.ecb.europa.eu actually
   * returns for /service/codelist/ECB/CL_FREQ. */
  { .id = "T_SDMX", .name = "sdmx codelist", .url = "https://x.test/sdmx?q={q}",
    .mode = HP_XML, .array_path = "str:Code",
    .title_keys = "com:Name", .id_keys = "id",
    .record_type = "t-sdmx", .free_tier = 1, .description = "d" },
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

  /* 9d-bis. XML records reach the sink with their fields intact.
   * Before HP_XML existed, hp_run's switch fell through to hp_run_json, cJSON
   * refused the body, and the row emitted nothing while still registering — so
   * the UK, EU and Swiss consolidated sanctions lists, which are published as
   * XML and only as XML, were unreachable. */
  fx_reset();
  fx_add("/x?q=", 200,
    "<?xml version=\"1.0\"?><list>"
    "<target><ref>7001</ref><name>ACME &amp; CO</name>"
    "<addr><country>CH</country><city>Zug</city></addr></target>"
    "<target><ref>7002</ref><name>BETA LTD</name>"
    "<addr><country>GB</country><city>London</city></addr></target>"
    "</list>");
  rc = run_source("T_XML", "x");
  ok(rc == 0 && g_ncap == 2, "XML mode emits one record per repeated element");
  ok(g_ncap >= 1 && strstr(g_cap[0].title, "ACME & CO") != NULL,
     "XML entities are decoded in the emitted title");
  ok(g_ncap >= 1 && strstr(g_cap[0].props, "\"addr.country\"") != NULL,
     "nested XML elements flatten to dotted keys like JSON does");

  /* 9d-ter. with no array_path the most-repeated element is the record. */
  fx_reset();
  fx_add("/xa?q=", 200,
    "<feed><entry><name>one</name></entry><entry><name>two</name></entry>"
    "<entry><name>three</name></entry></feed>");
  rc = run_source("T_XML_AUTO", "x");
  ok(rc == 0 && g_ncap == 3, "XML record element is auto-detected when unset");

  /* 9e-bis. the next link is found by rel, not by position.
   * `self` is deliberately first here. A positional `links.0.href` would follow
   * it, refetch page 1 and keep doing so until the page ceiling — losing every
   * later page while still looking like a successful run. */
  fx_reset();
  fx_add("/pr?q=", 200,
    "{\"items\":[{\"name\":\"r1\",\"id\":\"1\"}],"
    "\"links\":[{\"rel\":\"self\",\"href\":\"https://x.test/pr?q=x\"},"
    "{\"rel\":\"next\",\"href\":\"https://x.test/pr2\"}]}");
  fx_add("/pr2", 200,
    "{\"items\":[{\"name\":\"r2\",\"id\":\"2\"}],"
    "\"links\":[{\"rel\":\"self\",\"href\":\"https://x.test/pr2\"}]}");
  rc = run_source("T_PAGE_REL", "x");
  ok(rc == 0 && g_ncap == 2 && g_ncalls == 2,
     "next_path selects the link by rel, not by array position");

  /* 9e-ter. a 0-based page-numbered API fetches page 1, not page 2.
   * page_start's unset value is 0, which used to be coerced to 1 for any
   * non-offset param — so the first extra page came out as 2 and page 1 was
   * silently never fetched. */
  fx_reset();
  fx_add("page=1", 200, "{\"items\":[{\"name\":\"z2\",\"id\":\"2\"}]}");
  fx_add("page=2", 200, "{\"items\":[]}");
  fx_add("/pz?q=", 200, "{\"items\":[{\"name\":\"z1\",\"id\":\"1\"}]}");
  rc = run_source("T_PAGE_ZERO", "x");
  /* base page (0) + page 1 + the empty page 2 that stops the walk. Two records
   * means page 1 was actually read; the pre-fix engine jumped straight to 2 and
   * emitted only the base page's single record. */
  ok(rc == 0 && g_ncap == 2 && g_ncalls == 3,
     "page_zero_based fetches page 1 rather than skipping to page 2");

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

  /* 11. a record is dropped only when it carries NO content — not merely when
   *     its fields are named unconventionally. */

  /* 11a. headerless CSV with nothing declared: col0..colN match no fallback */
  fx_reset();
  fx_add("/bare.csv", 200, "1.2.3.4,ssh,2026-08-01\n5.6.7.8,telnet,2026-08-02\n");
  rc = run_source("T_CSV_BARE", "");
  ok(rc == 0 && g_ncap == 2, "headerless CSV with no declared keys still emits");
  ok(strstr(g_cap[0].key, "1.2.3.4") != NULL,
     "headerless CSV record is keyed on its first column");
  ok(strstr(g_cap[0].props, "\"col1\":\"ssh\"") != NULL,
     "every column is preserved, not just the keying one");

  /* 11b. a conventional key holding a number */
  fx_reset();
  fx_add("/n?q=", 200, "{\"items\":[{\"cik\":320193,\"form\":\"10-K\"}]}");
  rc = run_source("T_NUM_ID", "x");
  ok(rc == 0 && g_ncap == 1, "a numeric identifier is an identifier");
  ok(strstr(g_cap[0].key, "320193") != NULL,
     "the numeric id is keyed as its decimal text, not dropped");

  /* 11c. the noise floor still holds */
  fx_reset();
  fx_add("/e?q=", 200, "{\"items\":[{\"a\":\"\",\"b\":null,\"c\":{},\"d\":[]}]}");
  rc = run_source("T_EMPTY_REC", "x");
  ok(rc == 0 && g_ncap == 0, "a record with no content at all is still dropped");

  /* 11d. ...and an empty slot is not reported as a shortfall. A trailing
   *      newline made every such CSV say "emitted 197 of 198" forever; 87 rows
   *      of batch 19 carried that phantom -1. Two real records plus one empty
   *      slot must emit 2 and disclose nothing — a truncation notice here would
   *      be a false alarm, and false alarms are why real ones get ignored. */
  fx_reset();
  fx_add("/e?q=", 200,
    "{\"items\":[{\"name\":\"A\"},{\"a\":\"\",\"b\":null},{\"name\":\"B\"}]}");
  rc = run_source("T_EMPTY_REC", "x");
  ok(rc == 0 && g_ncap == 2, "the empty slot is skipped, both real records emit");
  int notice = 0;
  for (int i = 0; i < g_ncap; i++)
    if (!strcmp(g_cap[i].rtype, "collector-truncation-notice")) notice = 1;
  ok(!notice, "an empty slot raises no truncation notice");

  /* 12. the uid collision guard.
   *
   *     remote_key is the sink's upsert key, so two records that produce the
   *     same string store as ONE row while the run line still reports both.
   *     `name` is a title fallback but not an id fallback, so these records are
   *     keyed on their title — and three sharing a title collapsed onto one.
   *     Measured across a 1,197-source sweep: 46 rows losing 114,795 records a
   *     pass, invisible to `records=N` because emit() really was called. */
  fx_reset();
  fx_add("/e?q=", 200,
    "{\"items\":[{\"name\":\"Widget\",\"lot\":\"1\"},"
    "{\"name\":\"Widget\",\"lot\":\"2\"},{\"name\":\"Other\"}]}");
  rc = run_source("T_EMPTY_REC", "x");
  ok(rc == 0 && g_ncap == 3, "three title-keyed records all emit");
  ok(strcmp(g_cap[0].key, g_cap[1].key) != 0,
     "records sharing a title get distinct remote_keys");
  ok(strstr(g_cap[2].key, "Other") != NULL &&
     strstr(g_cap[2].key, "|") != NULL &&
     !strchr(strstr(g_cap[2].key, "Other"), '|'),
     "a record whose title was already unique keeps its old key unchanged");

  /* 12b. a shared ID is covered too, because the id is usually OURS: `id_keys`
   *      is a manifest declaration and a wrong one is an ordinary mistake.
   *      ECDC_RESPIRATORY declared id_keys=country_code on a weekly time
   *      series, so 12,648 observations keyed onto 438 rows. Records sharing an
   *      id but DIFFERING must both survive. */
  fx_reset();
  fx_add("/e?q=", 200,
    "{\"items\":[{\"id\":\"X\",\"v\":\"1\"},{\"id\":\"X\",\"v\":\"2\"}]}");
  rc = run_source("T_EMPTY_REC", "x");
  ok(rc == 0 && g_ncap == 2, "both id-carrying records emit");
  ok(strcmp(g_cap[0].key, g_cap[1].key) != 0,
     "same id + different content = two records behind a bad id declaration");

  /* 12d. THE SHAPE THE FIRST VERSION OF THE GUARD COULD NOT SEE.
   *
   *      A row declaring neither `id_keys` nor `title_keys`, whose fields match
   *      neither fallback list either, is keyed by hp_emit_record() on its
   *      FIRST NON-EMPTY SCALAR. The collision map originally computed only
   *      `rkey ? rkey : title` and skipped the record when both were NULL — so
   *      this one shape was unguarded, and every record whose first scalar was
   *      a dimension constant collapsed onto one uid.
   *
   *      Measured over the full registry: 1,818 hp rows are in this shape.
   *      WHO_XMART_NCD_MORTALITY emitted 10,001 and stored 2. `indicator` is in
   *      neither TITLE_FALLBACK nor ID_FALLBACK, so it reproduces exactly. */
  fx_reset();
  fx_add("/e?q=", 200,
    "{\"items\":[{\"indicator\":\"NCD\",\"country\":\"FR\",\"v\":1},"
    "{\"indicator\":\"NCD\",\"country\":\"DE\",\"v\":2},"
    "{\"indicator\":\"NCD\",\"country\":\"IT\",\"v\":3}]}");
  rc = run_source("T_EMPTY_REC", "x");
  ok(rc == 0 && g_ncap == 3, "first-scalar-keyed records all emit");
  ok(strcmp(g_cap[0].key, g_cap[1].key) != 0 &&
     strcmp(g_cap[1].key, g_cap[2].key) != 0 &&
     strcmp(g_cap[0].key, g_cap[2].key) != 0,
     "a row declaring NEITHER key is still guarded — the map mirrors the emitter");

  /* 12c. ...but the disambiguator is a CONTENT hash, so genuinely identical
   *      records still collapse. That is real deduplication, and it is what
   *      stops 12b from fabricating a distinction the data does not contain. */
  fx_reset();
  fx_add("/e?q=", 200,
    "{\"items\":[{\"id\":\"X\",\"v\":\"1\"},{\"id\":\"X\",\"v\":\"1\"}]}");
  rc = run_source("T_EMPTY_REC", "x");
  ok(rc == 0 && g_ncap == 2, "both byte-identical records are emitted");
  ok(!strcmp(g_cap[0].key, g_cap[1].key),
     "byte-identical records still key onto one row — real dedupe survives");

  /* 13. an HTTP 200 whose BODY is an error report is not a finding.
   *
   *     Observed live in batch 20: ArcGIS answers an over-quota query with
   *     status 200 and {"error":{"code":429,"message":"..."}}. `results` did
   *     not resolve, hp_find_array() found no array of objects, and the
   *     root-record fallback flattened the envelope — `code` matched
   *     ID_FALLBACK through the last-segment rule, so a number lifted out of
   *     an error message was stored and served as a finding titled
   *     `airway-record 429`. tools/probe_hp_batch.py rejects that shape; the
   *     engine did not. Nothing may be stored, and the run must report what
   *     the equivalent HTTP status would have reported. */
  fx_reset();
  fx_add("/ed?q=", 200, "{\"error\":{\"code\":429,\"message\":\"quota exceeded\"}}");
  rc = run_source("T_ERRDOC", "x");
  ok(g_ncap == 0, "an HTTP-200 error document stores nothing");
  ok(rc == 0, "a 4xx/429-class error code is an honest empty, like an HTTP 404");

  fx_reset();
  fx_add("/ed?q=", 200, "{\"error\":{\"code\":500,\"message\":\"boom\"}}");
  rc = run_source("T_ERRDOC", "x");
  ok(rc == -1 && g_ncap == 0, "a 5xx-class error document errors like an HTTP 500");

  /* No code at all: unclassifiable, so it is reported as an error rather than
   * as a successful empty run — "found nothing" and "was never checked" must
   * not look the same. */
  fx_reset();
  fx_add("/ed?q=", 200, "{\"error\":\"rate limited\"}");
  rc = run_source("T_ERRDOC", "x");
  ok(rc == -1 && g_ncap == 0, "an error document with no code is an errored run, not an empty one");

  /* The same envelope reaching a row that declares NO array_path — the
   * root-record fallback is exactly the path that fabricated the record.
   *
   * These are the REAL bytes: services.arcgis.com answered a bad query with
   * exactly this, under HTTP 200, on 2026-08-24. `details` is why the
   * densest-array heuristic did not save us either — it is an array of
   * STRINGS, so hp_find_array() (which requires objects) skips it and the
   * root-record path takes over. */
  fx_reset();
  fx_add("/auto?q=", 200,
    "{\"error\":{\"code\":400,\"message\":\"Cannot perform query. Invalid query "
    "parameters.\",\"details\":[\"'Invalid field: BOGUS_FIELD' parameter is invalid\"]}}");
  rc = run_source("T_AUTO", "x");
  ok(g_ncap == 0, "the root-record fallback no longer files an error envelope as a finding");

  /* A declared array_path that does not resolve: the response is not the shape
   * the row expects, and guessing at it is what produced the garbage record. */
  fx_reset();
  fx_add("/ed?q=", 200, "{\"payload\":{\"name\":\"Widget\",\"id\":\"1\"}}");
  rc = run_source("T_ERRDOC", "x");
  ok(rc == 0 && g_ncap == 0,
     "a declared array_path that does not resolve emits nothing rather than guessing");

  /* ...but a row that declares NO array_path never told us where its records
   * live, so the single-object response is still the record. This is the case
   * the narrowing must not break. */
  fx_reset();
  fx_add("/auto?q=", 200, "{\"name\":\"Solo Record\",\"id\":\"s1\"}");
  rc = run_source("T_AUTO", "x");
  ok(rc == 0 && g_ncap == 1 && !strcmp(g_cap[0].title, "Solo Record"),
     "root IS the record still works for a row with no array_path");

  /* ...and an `error` member sitting ALONGSIDE real records must not delete
   * them. Plenty of APIs ship a permanently-present error slot; the check only
   * runs once the response has been found to carry no records at all. */
  fx_reset();
  fx_add("/ed?q=", 200,
    "{\"error\":{\"code\":0,\"message\":\"\"},\"results\":[{\"name\":\"A\",\"id\":\"1\"}]}");
  rc = run_source("T_ERRDOC", "x");
  ok(rc == 0 && g_ncap == 1 && !strcmp(g_cap[0].title, "A"),
     "an error member alongside real records does not suppress them");

  /* 14. XML ATTRIBUTES are records too.
   *
   *     hp_xml_flatten walked child ELEMENTS only, so every attribute was
   *     dropped. In SDMX structural metadata the identity is the attribute —
   *     <Codelist id="CL_FREQ" agencyID="ILO"> — which made the whole
   *     ILO/OECD/ABS/Istat/ECB/Eurostat/IMF family unusable and cost two live
   *     WITS endpoints their place in batch 20. Attributes now flatten into
   *     the same dotted keyspace under an `@` last segment, which cannot
   *     collide with a child element of the same name. */
  fx_reset();
  fx_add("/xat?q=", 200,
    "<?xml version=\"1.0\"?><Structures><Codelists>"
    "<Codelist id=\"CL_FREQ\" agencyID=\"ILO\" version=\"1.0\">"
    "<Name xml:lang=\"en\">Frequency</Name><Ref id=\"R1\" agencyID=\"X\"/></Codelist>"
    "<Codelist id=\"CL_AREA\" agencyID=\"ILO\" version=\"1.0\">"
    "<Name xml:lang=\"en\">Reference area</Name></Codelist>"
    "</Codelists></Structures>");
  rc = run_source("T_XML_ATTR", "x");
  ok(rc == 0 && g_ncap == 2, "XML attribute records emit (the `<Codelists>` wrapper is not one)");
  ok(g_ncap >= 1 && strstr(g_cap[0].key, "CL_FREQ") != NULL,
     "id_keys=id resolves to the @id ATTRIBUTE, so the record is keyed on its identity");
  ok(g_ncap >= 1 && !strcmp(g_cap[0].title, "Frequency"),
     "an element-valued title still wins over the attributes beside it");
  ok(g_ncap >= 1 && strstr(g_cap[0].props, "\"@agencyID\":\"ILO\"") != NULL,
     "record-element attributes flatten as @name");
  ok(g_ncap >= 1 && strstr(g_cap[0].props, "\"Name.@xml:lang\":\"en\"") != NULL,
     "child-element attributes flatten as <path>.@name");
  ok(g_ncap >= 1 && strstr(g_cap[0].props, "\"Ref.@id\":\"R1\"") != NULL,
     "a self-closing child is attribute payload, not an empty element");
  ok(g_ncap >= 2 && strstr(g_cap[1].key, "CL_AREA") != NULL,
     "the second record keys on its own @id, not the first one's");

  /* An attribute must never shadow a child element of the same name: `@id` and
   * `id` are different keys, and an exact-key request resolves to the one it
   * named. */
  fx_reset();
  fx_add("/xat?q=", 200,
    "<Codelists><Codelist id=\"ATTR\"><id>ELEM</id><Name>N1</Name></Codelist>"
    "<Codelist id=\"ATTR2\"><id>ELEM2</id><Name>N2</Name></Codelist></Codelists>");
  rc = run_source("T_XML_ATTR", "x");
  ok(rc == 0 && g_ncap == 2, "attribute + same-named element both survive");
  ok(g_ncap >= 1 && strstr(g_cap[0].props, "\"@id\":\"ATTR\"") != NULL &&
                    strstr(g_cap[0].props, "\"id\":\"ELEM\"") != NULL,
     "@id and id are distinct keys — neither overwrites the other");
  ok(g_ncap >= 1 && strstr(g_cap[0].key, "ELEM") != NULL,
     "id_keys=id prefers the exact key `id` over the @id attribute");

  /* 14b. the same fix against the REAL bytes. This is a trimmed copy of what
   *      https://data-api.ecb.europa.eu/service/codelist/ECB/CL_FREQ returned
   *      on 2026-08-24 — namespace-prefixed element names, a `urn` attribute
   *      and an `xml:lang` on the name. Before attributes were flattened,
   *      every one of these records carried a label and no identity, which is
   *      what made the SDMX structural family unusable. */
  fx_reset();
  fx_add("/sdmx?q=", 200,
    "<?xml version='1.0' encoding='UTF-8'?><mes:Structure "
    "xmlns:mes=\"http://www.sdmx.org/resources/sdmxml/schemas/v2_1/message\" "
    "xmlns:str=\"http://www.sdmx.org/resources/sdmxml/schemas/v2_1/structure\" "
    "xmlns:com=\"http://www.sdmx.org/resources/sdmxml/schemas/v2_1/common\">"
    "<mes:Structures><str:Codelists>"
    "<str:Codelist urn=\"urn:sdmx:org.sdmx.infomodel.codelist.Codelist=ECB:CL_FREQ(1.0)\" "
    "isExternalReference=\"false\" agencyID=\"ECB\" id=\"CL_FREQ\" isFinal=\"false\" version=\"1.0\">"
    "<com:Name xml:lang=\"en\">Frequency code list</com:Name>"
    "<str:Code urn=\"urn:sdmx:org.sdmx.infomodel.codelist.Code=ECB:CL_FREQ(1.0).A\" id=\"A\">"
    "<com:Name xml:lang=\"en\">Annual</com:Name></str:Code>"
    "<str:Code urn=\"urn:sdmx:org.sdmx.infomodel.codelist.Code=ECB:CL_FREQ(1.0).D\" id=\"D\">"
    "<com:Name xml:lang=\"en\">Daily</com:Name></str:Code>"
    "</str:Codelist></str:Codelists></mes:Structures></mes:Structure>");
  rc = run_source("T_SDMX", "x");
  ok(rc == 0 && g_ncap == 2, "a real SDMX codelist yields one record per <str:Code>");
  ok(g_ncap >= 2 && strstr(g_cap[0].key, "|A") != NULL &&
                    strstr(g_cap[1].key, "|D") != NULL,
     "each SDMX code is keyed on its own id= attribute");
  ok(g_ncap >= 1 && !strcmp(g_cap[0].title, "Annual"),
     "the SDMX label still comes from <com:Name>");
  ok(g_ncap >= 1 && strstr(g_cap[0].props, "urn:sdmx:org.sdmx.infomodel.codelist.Code") != NULL,
     "the urn attribute is kept, not discarded");

  printf(g_fail ? "\n%d FAILURES\n" : "\nall passed\n", g_fail);
  return g_fail ? 1 : 0;
}
