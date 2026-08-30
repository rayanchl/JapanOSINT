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
  /* array_path CROSSING an array — the JMA district-forecast shape: a
   * top-level array of blocks, each with timeSeries[], each with areas[]. */
  { .id = "T_THRUARR", .name = "path through arrays",
    .url = "https://x.test/thru?q={q}",
    .mode = HP_JSON, .array_path = "timeSeries.areas",
    .title_keys = "area.name", .id_keys = "area.code",
    .record_type = "t-thru", .free_tier = 1, .description = "d" },
  /* OAI-PMH: pages by an opaque cursor, not a URL. */
  { .id = "T_OAI", .name = "oai resumption", .url = "https://x.test/oai?verb=ListRecords",
    .mode = HP_XML, .array_path = "record",
    .title_keys = "dc:title", .id_keys = "identifier",
    .next_path = "resumptionToken",
    .next_tmpl = "https://x.test/oai?verb=ListRecords&resumptionToken={v}",
    .record_type = "t-oai", .free_tier = 1, .description = "d" },
  /* a .jp host serving Shift_JIS with no charset header */
  { .id = "T_SJIS", .name = "shift-jis body",
    .url = "https://example.co.jp/s?q={q}",
    .mode = HP_JSON, .array_path = "items",
    .title_keys = "name", .id_keys = "id",
    .record_type = "t-sjis", .free_tier = 1, .description = "d" },
  /* the same body from a NON-jp host must be left alone */
  { .id = "T_NOTJP", .name = "latin-1 body, not jp",
    .url = "https://example.com/s?q={q}",
    .mode = HP_JSON, .array_path = "items",
    .title_keys = "name", .id_keys = "id",
    .record_type = "t-notjp", .free_tier = 1, .description = "d" },

  /* ── schema-drift notices (tests 18a–18d) ──
   * A row that declares where its records / title / identity live, so that a
   * response which no longer matches the declaration can be told apart from
   * one that does. */
  { .id = "T_SHAPE_DECL", .name = "declared shape, drifting upstream",
    .url = "https://x.test/shape?q={q}",
    .mode = HP_JSON, .array_path = "results",
    .title_keys = "legalName", .id_keys = "orgno",
    .record_type = "t-shape", .free_tier = 1, .description = "d" },
  /* No array_path: the densest-array guess, on a document that offers a choice. */
  { .id = "T_SHAPE_AUTO", .name = "undeclared shape, several arrays",
    .url = "https://x.test/shauto?q={q}",
    .mode = HP_JSON, .record_type = "t-shauto", .free_tier = 1, .description = "d" },
  /* A short page ceiling on DISTINCT pages, so the ceiling disclosure is
   * tested on a walk that really was cut short (test 10b). */
  { .id = "T_PAGE_CEIL", .name = "page ceiling", .url = "https://x.test/pc?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "id",
    .page_param = "offset", .page_size = 1, .page_max = 3,
    .record_type = "t-page", .free_tier = 1, .description = "d" },

  /* ── batch 25's four manifest gaps (tests 20–23) ── */
  /* A fixed-width text table: three title lines, a ruler, the header, a
   * ruler, then whitespace-aligned rows — JPNIC's as-numbers.txt. */
  { .id = "T_CSV_WS", .name = "whitespace table", .url = "https://x.test/as.txt",
    .mode = HP_CSV, .csv_delim = "ws", .csv_skip_lines = 3, .interval = 3600,
    .title_keys = "ASname", .id_keys = "ASN",
    .record_type = "t-ws", .free_tier = 1, .description = "d" },
  /* 2ch's subject.txt: `<dat><>title (n)`, a two-character literal separator. */
  { .id = "T_CSV_LIT", .name = "literal multi-char delimiter", .url = "https://x.test/subject.txt",
    .mode = HP_CSV, .csv_delim = "lit:<>", .csv_no_header = 1, .interval = 3600,
    .title_keys = "col1", .id_keys = "col0",
    .record_type = "t-lit", .free_tier = 1, .description = "d" },
  /* A title line above a header whose cells contain quoted line breaks. */
  { .id = "T_CSV_SKIP", .name = "title line above the header", .url = "https://x.test/skip.csv",
    .mode = HP_CSV, .csv_skip_lines = 1, .interval = 3600,
    .title_keys = "name", .id_keys = "code",
    .record_type = "t-skip", .free_tier = 1, .description = "d" },
  /* Every relative-reference form, resolved against the page URL. */
  { .id = "T_HTML_REL", .name = "relative hrefs", .url = "https://x.test/a/b/list.htm?q={q}",
    .mode = HP_HTML, .record_type = "t-rel", .free_tier = 1, .description = "d" },
  /* ...against the page's own <base href> when it declares one... */
  { .id = "T_HTML_BASETAG", .name = "base href tag", .url = "https://x.test/bt?q={q}",
    .mode = HP_HTML, .record_type = "t-bt", .free_tier = 1, .description = "d" },
  /* ...and the row's `base` overrides both. */
  { .id = "T_HTML_OVERRIDE", .name = "base override", .url = "https://x.test/bt?q={q}",
    .mode = HP_HTML, .base = "https://ov.test/x/",
    .record_type = "t-ov", .free_tier = 1, .description = "d" },
  /* Path-segment paging: the page number lives in the path. */
  { .id = "T_PAGE_PATH", .name = "path paging", .url = "https://x.test/tosan/p/{page}?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "id",
    .record_type = "t-ppath", .free_tier = 1, .description = "d" },
  { .id = "T_PAGE_PATH0", .name = "path paging, 0-based", .url = "https://x.test/pz0/p/{page}?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "id", .page_zero_based = 1,
    .record_type = "t-ppath0", .free_tier = 1, .description = "d" },
  /* The uid collision guard on the two non-JSON record paths. The shape is
   * JPCERT's phishurl-list: a header, a URL that is the only key, and a
   * re-confirmed URL appearing as a second row with a different date. */
  { .id = "T_CSV_COLL", .name = "csv sharing id_keys", .url = "https://x.test/coll.csv",
    .mode = HP_CSV, .interval = 3600, .title_keys = "URL", .id_keys = "URL",
    .record_type = "t-csvcoll", .free_tier = 1, .description = "d" },
  /* XML text decoding: NCRs to UTF-8, CDATA unwrapped, attributes decoded.
   * 14 NDL feeds write every title as `&#x6b74;…` and IPA/NICT/MHLW wrap
   * theirs in CDATA; both shapes stored unreadable titles with every gate
   * green, because stored == emitted says nothing about the bytes. */
  { .id = "T_XML_TEXT", .name = "xml text decoding", .url = "https://x.test/xt?q={q}",
    .mode = HP_XML, .array_path = "item", .title_keys = "title", .id_keys = "guid,link",
    .record_type = "t-xmltext", .free_tier = 1, .description = "d" },
  { .id = "T_XML_COLL", .name = "xml sharing id_keys", .url = "https://x.test/coll.xml",
    .mode = HP_XML, .array_path = "hit", .interval = 3600,
    .title_keys = "url", .id_keys = "url",
    .record_type = "t-xmlcoll", .free_tier = 1, .description = "d" },
};
HP_REGISTER_TABLE(T)

/* Number of captured rows of a given record_type, and the first of them. */
static int cap_count(const char *rtype, const cap **first) {
  int n = 0;
  if (first) *first = NULL;
  for (int i = 0; i < g_ncap; i++)
    if (!strcmp(g_cap[i].rtype, rtype)) { if (first && !*first) *first = &g_cap[i]; n++; }
  return n;
}
#define NOTICE "collector-shape-notice"

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

  /* 9d-quater. XML TEXT is decoded: numeric character references to UTF-8,
   * CDATA unwrapped, attributes decoded, undecodable references left literal.
   * Item 7 has no <title>; its @label attribute proves attribute decoding.
   * Item 2's description carries HTML inside CDATA — `<p>` twice per item
   * would out-count `<item>` in auto-detect if CDATA payload were tallied. */
  fx_reset();
  fx_add("/xt?q=", 200,
    "<?xml version=\"1.0\"?><rdf:RDF><channel><title>&#x6b74;</title></channel>"
    "<item><guid>1</guid><title>&#x6b74;&#21490; &#x1F600;</title></item>"
    "<item><guid>2</guid><title>&amp;&lt;&gt;&quot;&apos;</title>"
    "<description><![CDATA[<p>a</p><p>b</p><br></description>]]></description></item>"
    "<item><guid>3</guid><title>\n  <![CDATA[IPA &amp; raw]]>\n</title></item>"
    "<item><guid>4</guid><title>pre <![CDATA[a<b]]> mid &amp; <![CDATA[c]]> post</title></item>"
    "<item><guid>5</guid><title>&#0; &#xD800; &#x110000; &bogus; &#zz; &#12</title></item>"
    "<item><guid>6</guid><title>Plain \xe6\x9d\xb1\xe4\xba\xac title</title></item>"
    "<item label=\"&#x6771;&amp;T\"><guid>7</guid><link>https://x.test/7</link></item>"
    "</rdf:RDF>");
  rc = run_source("T_XML_TEXT", "x");
  ok(rc == 0 && g_ncap == 7, "XML text fixture emits all seven items");
  ok(g_ncap >= 1 && !strcmp(g_cap[0].title, "\xe6\xad\xb4\xe5\x8f\xb2 \xf0\x9f\x98\x80"),
     "hex + decimal NCRs decode to UTF-8 (kanji and a 4-byte emoji)");
  ok(g_ncap >= 2 && !strcmp(g_cap[1].title, "&<>\"'"),
     "the five predefined entities decode");
  ok(g_ncap >= 2 && strstr(g_cap[1].props, "\"description\":\"<p>a</p><p>b</p><br></description>\"") != NULL,
     "markup inside CDATA is text, not child elements, and does not end the element");
  ok(g_ncap >= 3 && !strcmp(g_cap[2].title, "IPA &amp; raw"),
     "a CDATA-only title is unwrapped, payload verbatim, padding trimmed");
  ok(g_ncap >= 4 && !strcmp(g_cap[3].title, "pre a<b mid & c post"),
     "several CDATA sections mixed with entity-bearing text");
  ok(g_ncap >= 5 && !strcmp(g_cap[4].title, "&#0; &#xD800; &#x110000; &bogus; &#zz; &#12"),
     "NUL, surrogate, out-of-range, unknown and malformed references stay literal");
  ok(g_ncap >= 6 && !strcmp(g_cap[5].title, "Plain \xe6\x9d\xb1\xe4\xba\xac title"),
     "text with no reference is byte-identical");
  ok(g_ncap >= 7 && strstr(g_cap[6].props, "\"@label\":\"\xe6\x9d\xb1&T\"") != NULL,
     "attribute values get the same decoding");
  ok(g_ncap >= 1 && strstr(g_cap[0].key, "T_XML_TEXT|1") != NULL,
     "id_keys=guid,link keys on guid first");
  {
    char buf[64];
    snprintf(buf, sizeof buf, "%s", "x&#x0;y");
    hp_xml_decode(buf);
    ok(!strcmp(buf, "x&#x0;y"), "hp_xml_decode never writes a NUL");
    snprintf(buf, sizeof buf, "%s", "<![CDATA[unterminated");
    hp_xml_decode(buf);
    ok(!strcmp(buf, "unterminated"), "an unterminated CDATA keeps its payload");
  }

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
  /* The row declares start_index paging, and this fixture answers every page
   * with the SAME bytes — i.e. it models a server that ignores start_index.
   * That used to run to the 10-page ceiling, re-emit the one record ten times
   * and file a truncation notice claiming pages were pending; since test 18d
   * the walk stops on the first repeated page and discloses THAT instead: one
   * record plus one page-param-ignored shape notice, two requests. */
  ok(rc == 0 && g_ncap == 2 && g_ncalls == 2,
     "UK_CH_PSC paginated, and stopped on the first repeated page");
  ok(!strcmp(g_cap[1].rtype, "collector-shape-notice") &&
     strstr(g_cap[1].key, "page-param-ignored") != NULL,
     "an ignored start_index is disclosed as a record");
  ok(strstr(g_last_url, "/company/00445790/persons-with-significant-control") != NULL,
     "UK_CH_PSC built the documented CH path");
  ok(strstr(g_cap[0].props, "natures_of_control.0") != NULL,
     "PSC control bands preserved in properties");

  /* 10b. the page CEILING, on pages that differ: three distinct pages against
   *      page_max=3 is a walk cut short, and that is disclosed as a truncation
   *      notice — 3 records plus the notice, and no shape notice, because the
   *      page parameter was honoured. */
  fx_reset();
  fx_add("offset=2", 200, "{\"items\":[{\"name\":\"C\",\"id\":\"3\"}]}");
  fx_add("offset=1", 200, "{\"items\":[{\"name\":\"B\",\"id\":\"2\"}]}");
  fx_add("/pc?q=",   200, "{\"items\":[{\"name\":\"A\",\"id\":\"1\"}]}");
  rc = run_source("T_PAGE_CEIL", "x");
  ok(rc == 0 && g_ncap == 4 && g_ncalls == 3, "T_PAGE_CEIL read 3 distinct pages to the ceiling");
  ok(!strcmp(g_cap[3].rtype, "collector-truncation-notice"),
     "page-ceiling stop is disclosed as a record");
  ok(strstr(g_cap[3].props, "\"pages_read\":3") != NULL,
     "the truncation notice states how many pages were read");
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
  /* No record — and since test 18a, the refusal to guess is itself disclosed
   * as one `collector-shape-notice` (array-path-missing), which is not a
   * record of the row's type. */
  ok(rc == 0 && g_ncap == 1 && !strcmp(g_cap[0].rtype, "collector-shape-notice"),
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

  /* 16. array_path THROUGH an array.
   *
   *     hp_path() walks with cJSON_GetObjectItem, which returns NULL on an
   *     array, so `timeSeries.areas` on a top-level array of blocks resolved
   *     to nothing and the row emitted nothing. The only expressible
   *     alternative was a positional index, which reaches one block and
   *     silently discards the rest — the discard house rule 2 forbids. This
   *     is the JMA district forecast, and it cost 56 verified offices.
   *
   *     The descending walk must take EVERY node at the path: 2 blocks x
   *     2 timeSeries x 2 areas = 8 records, not 2 and not 4. */
  fx_reset();
  fx_add("/thru?q=", 200,
    "[{\"reportDatetime\":\"T1\",\"timeSeries\":["
       "{\"areas\":[{\"area\":{\"name\":\"A1\",\"code\":\"1\"},\"v\":\"a\"},"
                   "{\"area\":{\"name\":\"A2\",\"code\":\"2\"},\"v\":\"b\"}]},"
       "{\"areas\":[{\"area\":{\"name\":\"A3\",\"code\":\"3\"},\"v\":\"c\"},"
                   "{\"area\":{\"name\":\"A4\",\"code\":\"4\"},\"v\":\"d\"}]}]},"
     "{\"reportDatetime\":\"T2\",\"timeSeries\":["
       "{\"areas\":[{\"area\":{\"name\":\"B1\",\"code\":\"5\"},\"v\":\"e\"},"
                   "{\"area\":{\"name\":\"B2\",\"code\":\"6\"},\"v\":\"f\"}]},"
       "{\"areas\":[{\"area\":{\"name\":\"B3\",\"code\":\"7\"},\"v\":\"g\"},"
                   "{\"area\":{\"name\":\"B4\",\"code\":\"8\"},\"v\":\"h\"}]}]}]");
  rc = run_source("T_THRUARR", "x");
  ok(rc == 0 && g_ncap == 8,
     "array_path descends through arrays and takes every node, not the first block");
  ok(g_ncap == 8 && !strcmp(g_cap[0].title, "A1") && !strcmp(g_cap[7].title, "B4"),
     "the first and last record of the LAST block both survive the descent");

  /* ...and the narrowing this must not break: a declared array_path that
   * genuinely is not in the document still emits nothing rather than letting
   * the descending walk mine something else. */
  fx_reset();
  fx_add("/thru?q=", 200, "[{\"reportDatetime\":\"T1\",\"other\":[{\"x\":1}]}]");
  rc = run_source("T_THRUARR", "x");
  ok(rc == 0 && g_ncap == 1 && !strcmp(g_cap[0].rtype, "collector-shape-notice"),
     "a genuinely absent array_path still emits nothing after the descending walk");

  /* 17. Shift_JIS on a .jp host.
   *
   *     lib/feedlib.c transcodes these; hpengine never did, so every hp row on
   *     a legacy .jp host stored mojibake. The 2ch-family boards are all
   *     Shift_JIS and could not be registered because of it.
   *
   *     "\x93\xfa\x96\x7b" is Shift_JIS for 日本. Under the bug the title is
   *     those raw bytes; fixed, it is the UTF-8 encoding e6 97 a5 e6 9c ac. */
  fx_reset();
  fx_add("/s?q=", 200,
    "{\"items\":[{\"name\":\"\x93\xfa\x96\x7b\",\"id\":\"1\"}]}");
  rc = run_source("T_SJIS", "x");
  ok(rc == 0 && g_ncap == 1 && !strcmp(g_cap[0].title, "\xe6\x97\xa5\xe6\x9c\xac"),
     "a Shift_JIS body from a .jp host is transcoded to UTF-8 before the parse");

  /* ...and the gate that makes it safe: Latin-1 is also invalid UTF-8, and
   *    "\xfc\x72" in "Zürich" is valid Shift_JIS, so a blanket transcode turns
   *    European feeds into kanji. Same bytes, non-jp host, left alone. */
  fx_reset();
  fx_add("/s?q=", 200, "{\"items\":[{\"name\":\"Z\xfcrich\",\"id\":\"1\"}]}");
  rc = run_source("T_NOTJP", "x");
  ok(rc == 0 && g_ncap == 1 && strstr(g_cap[0].title, "\xfc") != NULL,
     "the same bytes from a non-jp host are NOT transcoded");

  /* ...and the NEC extension rows. glibc's "SHIFT_JIS" is strict JIS X 0208
   *    and rejects 0x81A1 (■), which fails the transcode CLOSED and stores the
   *    whole document as mojibake — one decorative character in one thread
   *    title was enough to lose an entire board. "\x81\xa1" is ■ in CP932;
   *    correct output is UTF-8 e2 96 a0. This is the difference between the
   *    two 2ch-family boards behaving identically and only one of them
   *    working. */
  fx_reset();
  fx_add("/s?q=", 200,
    "{\"items\":[{\"name\":\"\x81\xa1\x93\xfa\x96\x7b\",\"id\":\"1\"}]}");
  rc = run_source("T_SJIS", "x");
  ok(rc == 0 && g_ncap == 1 && !strcmp(g_cap[0].title, "\xe2\x96\xa0\xe6\x97\xa5\xe6\x9c\xac"),
     "an NEC-extension character decodes instead of failing the whole body closed");

  /* ...and a .jp host serving perfectly good UTF-8 is untouched, which is the
   *    overwhelmingly common case and the one a regression would be silent in. */
  fx_reset();
  fx_add("/s?q=", 200, "{\"items\":[{\"name\":\"\xe6\x97\xa5\xe6\x9c\xac\",\"id\":\"1\"}]}");
  rc = run_source("T_SJIS", "x");
  ok(rc == 0 && g_ncap == 1 && !strcmp(g_cap[0].title, "\xe6\x97\xa5\xe6\x9c\xac"),
     "a .jp host already serving UTF-8 passes through unchanged");

  /* 18. Schema drift is reported as DATA — one `collector-shape-notice` per
   *     run per condition, in addition to the records, never instead of them,
   *     and never when the condition did not occur. */
  const cap *nt = NULL;

  /* 18a. a declared array_path that resolves to a non-array. */
  fx_reset();
  fx_add("/shape?q=", 200,
    "{\"results\":\"moved\",\"data\":[{\"legalName\":\"Acme\",\"orgno\":\"1\"}]}");
  rc = run_source("T_SHAPE_DECL", "x");
  ok(rc == 0 && cap_count("t-shape", NULL) == 0,
     "18a: a declared array_path that resolves to a string still emits no record");
  ok(cap_count(NOTICE, &nt) == 1 && nt && strstr(nt->key, "shape:array-path-missing:") != NULL,
     "18a: exactly one array-path-missing notice, keyed on condition + day");
  ok(nt && strstr(nt->title, "\"results\" resolved to a string on 1 of 1 page(s)") != NULL,
     "18a: the notice states what the path resolved to and on how many pages");
  ok(nt && strstr(nt->props, "\"declared_array_path\":\"results\"") != NULL &&
           strstr(nt->props, "\"records_emitted\":0") != NULL,
     "18a: the numbers are in the properties too");

  /* ...and the control: the declared shape, intact, produces records and NO notice. */
  fx_reset();
  fx_add("/shape?q=", 200,
    "{\"results\":[{\"legalName\":\"Acme\",\"orgno\":\"1\"},{\"legalName\":\"Bee\",\"orgno\":\"2\"}]}");
  rc = run_source("T_SHAPE_DECL", "x");
  ok(rc == 0 && cap_count("t-shape", NULL) == 2 && cap_count(NOTICE, NULL) == 0,
     "18: a response matching every declaration emits records and no notice");

  /* ...and JO_SHAPE_NOTICES=0 silences the notice without touching the records. */
  fx_reset();
  fx_add("/shape?q=", 200, "{\"results\":\"moved\"}");
  setenv("JO_SHAPE_NOTICES", "0", 1);
  rc = run_source("T_SHAPE_DECL", "x");
  unsetenv("JO_SHAPE_NOTICES");
  ok(rc == 0 && g_ncap == 0, "18: JO_SHAPE_NOTICES=0 silences the notice");

  /* 18b. no array_path, and the document offers more than one array of
   *      objects: the densest-array guess is disclosed with its runner-up. */
  fx_reset();
  fx_add("/shauto?q=", 200,
    "{\"main\":[{\"name\":\"A\",\"id\":\"1\"},{\"name\":\"B\",\"id\":\"2\"}],"
    "\"related\":[{\"name\":\"R\",\"id\":\"9\"}]}");
  rc = run_source("T_SHAPE_AUTO", "x");
  ok(rc == 0 && cap_count("t-shauto", NULL) == 2,
     "18b: the densest array is still mined — the records are not withheld");
  ok(cap_count(NOTICE, &nt) == 1 && nt && strstr(nt->key, "shape:densest-array-fallback:") != NULL,
     "18b: one densest-array-fallback notice");
  ok(nt && strstr(nt->props, "\"mined_array_path\":\"main\"") != NULL &&
           strstr(nt->props, "\"candidate_arrays\":2") != NULL &&
           strstr(nt->props, "\"runner_up_path\":\"related\"") != NULL &&
           strstr(nt->props, "\"runner_up_size\":1") != NULL,
     "18b: the notice names the array mined, the candidate count and the runner-up");
  /* ...one candidate is not a guess: no notice. */
  fx_reset();
  fx_add("/shauto?q=", 200, "{\"meta\":{\"n\":1},\"main\":[{\"name\":\"A\",\"id\":\"1\"}]}");
  rc = run_source("T_SHAPE_AUTO", "x");
  ok(rc == 0 && cap_count("t-shauto", NULL) == 1 && cap_count(NOTICE, NULL) == 0,
     "18b: a document with a single array of objects produces no notice");

  /* 18c. title_keys / id_keys declared, and NO record on the page carries
   *      them: the records are still emitted (fallback list), and each dead
   *      declaration gets one notice stating 0 of N. */
  fx_reset();
  fx_add("/shape?q=", 200,
    "{\"results\":[{\"name\":\"Acme\",\"id\":\"1\"},{\"name\":\"Bee\",\"id\":\"2\"},"
    "{\"name\":\"Cee\",\"id\":\"3\"}]}");
  rc = run_source("T_SHAPE_DECL", "x");
  ok(rc == 0 && cap_count("t-shape", NULL) == 3 && !strcmp(g_cap[0].title, "Acme"),
     "18c: records whose declared keys are gone are still emitted under the fallback list");
  ok(cap_count(NOTICE, NULL) == 2, "18c: one notice per dead declaration (title_keys, id_keys)");
  int seen_t = 0, seen_i = 0;
  for (int i = 0; i < g_ncap; i++) {
    if (strcmp(g_cap[i].rtype, NOTICE)) continue;
    if (strstr(g_cap[i].key, "shape:title-keys-unmatched:") &&
        strstr(g_cap[i].title, "title_keys \"legalName\" matched 0 of 3 record(s) on 1 of 1 page(s)"))
      seen_t = 1;
    if (strstr(g_cap[i].key, "shape:id-keys-unmatched:") &&
        strstr(g_cap[i].title, "id_keys \"orgno\" matched 0 of 3 record(s)"))
      seen_i = 1;
  }
  ok(seen_t && seen_i, "18c: both notices state the declared keys and 0 of N");
  /* ...a page where ONE record carries the key is a partial match, not drift. */
  fx_reset();
  fx_add("/shape?q=", 200,
    "{\"results\":[{\"legalName\":\"Acme\",\"orgno\":\"1\"},{\"name\":\"Bee\",\"id\":\"2\"}]}");
  rc = run_source("T_SHAPE_DECL", "x");
  ok(rc == 0 && cap_count("t-shape", NULL) == 2 && cap_count(NOTICE, NULL) == 0,
     "18c: declared keys matching at least one record on the page produce no notice");

  /* 18d. the page parameter is ignored: page 2 comes back byte-identical to
   *      page 1. The walk stops there (every further page is the same bytes)
   *      and says how many pages it did not request. T_PAGE_PARAM has
   *      page_size=2, so page 1's two full records used to trigger a walk to
   *      the 10-page ceiling, re-emitting the same two records ten times. */
  fx_reset();
  fx_add("/po?q=", 200, "{\"items\":[{\"name\":\"P1\",\"id\":\"1\"},{\"name\":\"P2\",\"id\":\"2\"}]}");
  rc = run_source("T_PAGE_PARAM", "x");
  ok(rc == 0 && g_ncalls == 2,
     "18d: the walk stops on the first repeated page instead of running to the ceiling");
  ok(cap_count("t-page", NULL) == 2,
     "18d: the repeated page's records are not re-emitted");
  ok(cap_count(NOTICE, &nt) == 1 && nt && strstr(nt->key, "shape:page-param-ignored:") != NULL,
     "18d: one page-param-ignored notice");
  ok(nt && strstr(nt->title, "page 2 was byte-identical to page 1") != NULL &&
           strstr(nt->props, "\"pages_skipped\":8") != NULL &&
           strstr(nt->props, "\"page_ceiling\":10") != NULL &&
           strstr(nt->props, "\"page_param\":\"offset\"") != NULL,
     "18d: the notice states the repeated page, the pages skipped and the ceiling");
  /* ...and a walk whose pages differ is untouched: no notice. */
  fx_reset();
  fx_add("offset=4", 404, NULL);      /* the upstream's end of the collection */
  fx_add("offset=2", 200, "{\"items\":[{\"name\":\"P3\",\"id\":\"3\"}]}");
  fx_add("/po?q=", 200, "{\"items\":[{\"name\":\"P1\",\"id\":\"1\"},{\"name\":\"P2\",\"id\":\"2\"}]}");
  rc = run_source("T_PAGE_PARAM", "x");
  ok(rc == 0 && cap_count("t-page", NULL) == 3 && cap_count(NOTICE, NULL) == 0,
     "18d: a walk whose pages differ emits every page and no notice");

  /* 19. OAI-PMH pages by an opaque resumptionToken, which is NOT a URL.
   *
   *     next_path alone assumes the upstream hands back an absolute URL, and a
   *     bare token used as one simply fails — so every OAI-PMH row read its
   *     first page and stopped. That is 100 records of a repository holding
   *     69,738, silently, which is exactly what house rule 2 forbids.
   *     next_tmpl builds the continuation request from the token.
   *
   *     Compounding it, the XML path never resolved next_path at all (only
   *     hp_run_json did), so an XML row that declared one paged not at all
   *     regardless of what the upstream returned. */
  fx_reset();
  fx_add("resumptionToken=tok1", 200,
    "<?xml version=\"1.0\"?><OAI-PMH><ListRecords>"
    "<record><identifier>b1</identifier><dc:title>B1</dc:title></record>"
    "<record><identifier>b2</identifier><dc:title>B2</dc:title></record>"
    "<resumptionToken/></ListRecords></OAI-PMH>");
  fx_add("verb=ListRecords", 200,
    "<?xml version=\"1.0\"?><OAI-PMH><ListRecords>"
    "<record><identifier>a1</identifier><dc:title>A1</dc:title></record>"
    "<record><identifier>a2</identifier><dc:title>A2</dc:title></record>"
    "<resumptionToken>tok1</resumptionToken></ListRecords></OAI-PMH>");
  rc = run_source("T_OAI", "x");
  ok(rc == 0 && g_ncap == 4,
     "an OAI resumptionToken is followed, so page 2 is read instead of discarded");
  ok(g_ncap == 4 && !strcmp(g_cap[3].title, "B2"),
     "the second page's records are the ones the cursor pointed at");
  ok(strstr(g_last_url, "resumptionToken=tok1") != NULL,
     "the continuation URL is built from next_tmpl, not from the bare token");
  /* ...and an EMPTY <resumptionToken/> is the protocol saying "last page".
   *    Treating it as a cursor would refetch page 1 up to the page ceiling. */
  ok(g_ncalls == 2, "an empty resumptionToken ends the walk instead of looping");

  /* 20. Text tables that are not CSV. */
  fx_reset();
  fx_add("/as.txt", 200,
    "                      AS number list (2026/08/29)\n"
    "                *: not assigned\n"
    "                   second title line\n"
    "-------------------------------------------\n"
    "ASN          ASname        contact\n"
    "-------------------------------------------\n"
    "2497      IIJ          JP00006327\n"
    "2498*\n"
    "2500      WIDE-BB      JM002JP\n");
  rc = run_source("T_CSV_WS", NULL);
  ok(rc == 0 && cap_count("t-ws", NULL) == 3,
     "20a: csv_delim=ws + csv_skip_lines: three data rows, no title/ruler/header junk");
  ok(g_ncap >= 1 && strstr(g_cap[0].props, "\"ASname\":\"IIJ\"") != NULL &&
     strstr(g_cap[0].props, "\"contact\":\"JP00006327\"") != NULL,
     "20a: a run of blanks is one separator and the header names the columns");
  ok(g_ncap >= 2 && !strcmp(g_cap[1].title, "t-ws 2498*") &&
     strstr(g_cap[1].props, "\"ASname\"") == NULL,
     "20a: a short row keeps what it has and invents nothing for the missing cells");
  ok(g_ncap >= 3 && strstr(g_cap[2].props, "\"ASname\":\"WIDE-BB\"") != NULL,
     "20a: leading alignment blanks are not a cell");
  fx_reset();
  fx_add("/subject.txt", 200,
    "1756400000.dat<>Thread one (12)\n1756400001.dat<>Thread <two> (3)\n");
  rc = run_source("T_CSV_LIT", NULL);
  ok(rc == 0 && cap_count("t-lit", NULL) == 2 && !strcmp(g_cap[0].title, "Thread one (12)") &&
     !strcmp(g_cap[1].title, "Thread <two> (3)"),
     "20b: csv_delim=lit:<> splits on the literal token and only on it");
  ok(g_ncap >= 1 && strstr(g_cap[0].props, "\"col0\":\"1756400000.dat\"") != NULL,
     "20b: the id column is the dat name");
  fx_reset();
  fx_add("/skip.csv", 200,
    "School code list,,,updated:,2026/5/20\n"
    "code,\"set\nkind\",name\n"
    "1,\"a\nb\",Alpha\n"
    "2,c,Beta\n");
  rc = run_source("T_CSV_SKIP", NULL);
  ok(rc == 0 && cap_count("t-skip", NULL) == 2,
     "20c: csv_skip_lines=1 drops the title line and reads the real header");
  ok(g_ncap >= 2 && !strcmp(g_cap[0].title, "Alpha") && !strcmp(g_cap[1].title, "Beta") &&
     strstr(g_cap[0].key, "T_CSV_SKIP|1") != NULL,
     "20c: records are titled and keyed by the header names, not col0..colN");
  ok(g_ncap >= 1 && strstr(g_cap[0].props, "\"set\\nkind\":\"a\\nb\"") != NULL,
     "20c: quoted line breaks in the header and the data survive the skip");
  { int junk = 0; for (int i = 0; i < g_ncap; i++) if (strstr(g_cap[i].title, "School code")) junk = 1;
    ok(!junk, "20c: the title line is not emitted as a record"); }

  /* 21. Relative href resolution in HTML mode. */
  fx_reset();
  fx_add("/a/b/list.htm", 200,
    "<html><body>"
    "<a href=\"../profile/x.htm\">Profile X</a>"
    "<a href=\"./meisai/y.htm\">Meisai Y</a>"
    "<a href=\"y.htm\">Bare Y</a>"
    "<a href=\"//other.test/z\">Other Z</a>"
    "<a href=\"?q=1\">Query one</a>"
    "<a href=\"#frag\">Fragment</a>"
    "<a href=\"/root.htm\">Root</a>"
    "<a href=\"https://abs.test/p\">Absolute</a>"
    "</body></html>");
  rc = run_source("T_HTML_REL", "acme");
  ok(rc == 0 && cap_count("t-rel", NULL) == 8, "21a: every anchor form yields a record");
  ok(g_ncap >= 8 && !strcmp(g_cap[0].link, "https://x.test/a/profile/x.htm"),
     "21a: ../ climbs one directory");
  ok(g_ncap >= 8 && !strcmp(g_cap[1].link, "https://x.test/a/b/meisai/y.htm"),
     "21a: ./ is the page's directory");
  ok(g_ncap >= 8 && !strcmp(g_cap[2].link, "https://x.test/a/b/y.htm"),
     "21a: a bare name is relative to the page's directory");
  ok(g_ncap >= 8 && !strcmp(g_cap[3].link, "https://other.test/z"),
     "21a: //host is scheme-relative");
  ok(g_ncap >= 8 && !strcmp(g_cap[4].link, "https://x.test/a/b/list.htm?q=1"),
     "21a: ?query keeps the page path");
  ok(g_ncap >= 8 && !strcmp(g_cap[5].link, "https://x.test/a/b/list.htm?q=acme#frag"),
     "21a: #frag keeps the page URL");
  ok(g_ncap >= 8 && !strcmp(g_cap[6].link, "https://x.test/root.htm"),
     "21a: /root is root-relative");
  ok(g_ncap >= 8 && !strcmp(g_cap[7].link, "https://abs.test/p"),
     "21a: an absolute href is untouched");
  fx_reset();
  fx_add("/bt?q=", 200,
    "<html><head><BASE HREF=\"https://cdn.test/dir/sub.htm\"></head><body>"
    "<a href=\"y.htm\">Bare Y</a><a href=\"../up.htm\">Up one</a></body></html>");
  rc = run_source("T_HTML_BASETAG", "x");
  ok(rc == 0 && cap_count("t-bt", NULL) == 2 &&
     !strcmp(g_cap[0].link, "https://cdn.test/dir/y.htm") &&
     !strcmp(g_cap[1].link, "https://cdn.test/up.htm"),
     "21b: the page's own <base href> is honoured over the fetched URL");
  rc = run_source("T_HTML_OVERRIDE", "x");
  ok(rc == 0 && cap_count("t-ov", NULL) == 2 &&
     !strcmp(g_cap[0].link, "https://ov.test/x/y.htm") &&
     !strcmp(g_cap[1].link, "https://ov.test/up.htm"),
     "21c: the row's base= overrides both the page URL and its <base href>");

  /* 22. Path-segment pagination through a {page} token. */
  fx_reset();
  fx_add("/p/3?", 404, NULL);
  fx_add("/p/2?", 200, "{\"items\":[{\"name\":\"C\",\"id\":\"3\"}]}");
  fx_add("/p/1?", 200, "{\"items\":[{\"name\":\"A\",\"id\":\"1\"},{\"name\":\"B\",\"id\":\"2\"}]}");
  rc = run_source("T_PAGE_PATH", "x");
  ok(rc == 0 && cap_count("t-ppath", NULL) == 3,
     "22a: {page} walks /p/1, /p/2 and takes every record");
  ok(g_ncalls == 3 && strstr(g_last_url, "/tosan/p/3?q=x") != NULL,
     "22a: the walk ends at the first page the upstream does not serve");
  ok(g_ncap >= 3 && strstr(g_cap[2].props, "\"_page\":2") != NULL,
     "22a: records carry the page they came from");
  fx_reset();
  fx_add("/pz0/p/1?", 404, NULL);
  fx_add("/pz0/p/0?", 200, "{\"items\":[{\"name\":\"Z\",\"id\":\"0\"}]}");
  rc = run_source("T_PAGE_PATH0", "x");
  ok(rc == 0 && cap_count("t-ppath0", NULL) == 1 && g_ncalls == 2 &&
     strstr(g_last_url, "/pz0/p/1?") != NULL,
     "22b: page_zero_based=1 starts a {page} walk at /p/0");

  /* 23. OAI-PMH continuation is test 19 above: next_path=resumptionToken with
   *     next_tmpl carrying {v}. Nothing new to add — the manifest already
   *     expresses it, which is what batch 25's KURENAI row now declares. */

  /* 24. The uid collision guard on the CSV and XML paths.
   *
   *     hp_collision_map() was called from hp_run_json only. JPCERT's monthly
   *     phishing-URL CSVs list a re-confirmed URL as a legitimate second row,
   *     the file has no row id, so the two rows keyed onto one uid and the sink
   *     kept one: 202401 emitted 5,772, stored 5,646. Same defect, second and
   *     third copy — CLAUDE.md §4b. One implementation now serves all three. */
  fx_reset();
  fx_add("/coll.csv", 200,
    "date,URL,description\n"
    "2024-01-05,http://phish.example/a,bank A\n"
    "2024-01-19,http://phish.example/a,bank A\n"
    "2024-01-07,http://phish.example/b,bank B\n");
  rc = run_source("T_CSV_COLL", NULL);
  ok(rc == 0 && cap_count("t-csvcoll", NULL) == 3, "24a: csv — all three rows emit");
  ok(g_ncap == 3 && strcmp(g_cap[0].key, g_cap[1].key) != 0,
     "24a: csv rows sharing id_keys but differing get distinct remote_keys");
  ok(g_ncap == 3 && !strcmp(g_cap[2].key, "T_CSV_COLL|http://phish.example/b"),
     "24a: the csv row whose key was already unique keeps its old key unchanged");

  fx_reset();
  fx_add("/coll.csv", 200,
    "date,URL,description\n"
    "2024-01-05,http://phish.example/a,bank A\n"
    "2024-01-05,http://phish.example/a,bank A\n");
  rc = run_source("T_CSV_COLL", NULL);
  ok(rc == 0 && g_ncap == 2 && !strcmp(g_cap[0].key, g_cap[1].key),
     "24b: byte-identical csv rows share one key (real dedupe, not fabricated distinction)");

  /* 24c. No collisions: the uids are exactly what the pre-change derivation
   *      produced, `<source id>|<id_keys value>`. A changed uid on an unchanged
   *      record would re-insert every stored row. */
  fx_reset();
  fx_add("/coll.csv", 200,
    "date,URL,description\n"
    "2024-01-05,http://phish.example/a,bank A\n"
    "2024-01-07,http://phish.example/b,bank B\n");
  rc = run_source("T_CSV_COLL", NULL);
  ok(rc == 0 && g_ncap == 2 &&
     !strcmp(g_cap[0].key, "T_CSV_COLL|http://phish.example/a") &&
     !strcmp(g_cap[1].key, "T_CSV_COLL|http://phish.example/b"),
     "24c: a csv with no collisions keeps byte-identical uids to before the guard");
  fx_reset();
  fx_add("/f.csv", 200, "1001,\"ACME TRADING LTD\",-0- \n1002,\"OTHER CORP\",-0- \n");
  rc = run_source("T_CSV", "ACME");
  ok(rc == 0 && g_ncap == 1 && !strcmp(g_cap[0].key, "T_CSV|1001"),
     "24c: headerless csv keys are unchanged too");

  /* 24d. XML equivalent. */
  fx_reset();
  fx_add("/coll.xml", 200,
    "<?xml version=\"1.0\"?><hits>"
    "<hit><url>http://phish.example/a</url><date>2024-01-05</date></hit>"
    "<hit><url>http://phish.example/a</url><date>2024-01-19</date></hit>"
    "<hit><url>http://phish.example/b</url><date>2024-01-07</date></hit>"
    "</hits>");
  rc = run_source("T_XML_COLL", NULL);
  ok(rc == 0 && g_ncap == 3 && strcmp(g_cap[0].key, g_cap[1].key) != 0,
     "24d: xml records sharing id_keys but differing get distinct remote_keys");
  ok(g_ncap == 3 && !strcmp(g_cap[2].key, "T_XML_COLL|http://phish.example/b"),
     "24d: the unique xml record keeps its old key unchanged");
  fx_reset();
  fx_add("/coll.xml", 200,
    "<?xml version=\"1.0\"?><hits>"
    "<hit><url>http://phish.example/a</url><date>2024-01-05</date></hit>"
    "<hit><url>http://phish.example/a</url><date>2024-01-05</date></hit>"
    "</hits>");
  rc = run_source("T_XML_COLL", NULL);
  ok(rc == 0 && g_ncap == 2 && !strcmp(g_cap[0].key, g_cap[1].key),
     "24e: byte-identical xml records share one key");

  /* 25. Card-style anchors: the label sits in nested children behind a
   *     leading <img>, or only in the image's alt. The parser's own text
   *     glues line breaks and keeps blank runs, so these failed `text_len < 3`
   *     and the records vanished. Anchors with direct text are unchanged. */
  fx_reset();
  fx_add("/h?q=", 200,
    "<html>"
    "<a href=\"/rec/88\"><img src=\"x.png\"><b>\nA\n</b><i>\nB\n</i></a>"
    "<a href=\"/rec/89\"><img src=\"y.png\" alt=\"Alt Label\"></a>"
    "<a href=\"/rec/90\"><img src=\"z.png\"></a>"
    "<a href=\"/rec/91\">Plain  text</a>"
    "</html>");
  rc = run_source("T_HTML", "x");
  ok(rc == 0 && g_ncap == 3, "25: card anchors with nested text or alt emit; a bare image does not");
  ok(g_ncap >= 1 && !strcmp(g_cap[0].title, "A B"),
     "25a: text the parser glued to \"AB\" is re-read as descendant text, collapsed");
  ok(g_ncap >= 2 && !strcmp(g_cap[1].title, "Alt Label"),
     "25b: an image-only anchor falls back to the img alt");
  ok(g_ncap >= 3 && !strcmp(g_cap[2].title, "Plain  text"),
     "25c: an anchor with direct text keeps exactly the text it had");

  printf(g_fail ? "\n%d FAILURES\n" : "\nall passed\n", g_fail);
  return g_fail ? 1 : 0;
}
