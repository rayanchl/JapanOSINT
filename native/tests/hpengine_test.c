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
#include "../lib/jsonlist.h"
#include "../lib/feedlib.h"
#include "../lib/csv.h"
#include "../core/httpclient.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__GLIBC__)
#include <malloc.h>
#endif

/* Bytes the allocator holds for this process, or -1 where it cannot be read.
 * Used ONLY by the leak test (30): a per-page leak of an inflated ZIP entry is
 * megabytes per run, which no other observable in this harness shows. Under a
 * sanitizer the figure includes its quarantine of FREED blocks, so the test
 * reads -1 there and is skipped (the sanitizer's own leak check covers it). */
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define HPT_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define HPT_ASAN 1
#endif
static long heap_in_use(void) {
#if defined(HPT_ASAN)
  return -1;
#elif defined(__APPLE__)
  malloc_statistics_t st;
  malloc_zone_statistics(NULL, &st);
  return (long)st.size_in_use;
#elif defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 33)
  struct mallinfo2 mi = mallinfo2();
  return (long)mi.uordblks;
#else
  return -1;
#endif
#else
  return -1;
#endif
}

/* ── stub registry (the real one lives in registry.c) ────────────────────── */
static const source_def *g_defs[2048];
static int g_ndefs = 0;
void registry_add(const source_def *d) { if (g_ndefs < 2048) g_defs[g_ndefs++] = d; }
static const source_def *find_def(const char *id) {
  for (int i = 0; i < g_ndefs; i++) if (!strcmp(g_defs[i]->id, id)) return g_defs[i];
  return NULL;
}

/* ── stub HTTP: fixtures keyed by URL substring ──────────────────────────── */
/* `len` is 0 for a text body (strlen) and the byte count for a binary one —
 * a ZIP holds NULs, which strdup would cut at the first. */
typedef struct { const char *match, *body; long status; size_t len; } fixture;
static fixture g_fx[16];
static int g_nfx = 0;
static char g_last_url[2048];
static char g_last_body[2048];
static char g_last_hdrs[1024];
static int  g_ncalls = 0;
/* The headers of EACH call, in order — a later page's request must be checked
 * on its own, and g_last_hdrs only ever holds the final one. */
static char g_call_hdrs[16][512];
/* A fixture status that answers 304 Not Modified (no body) unless the request
 * carries `Cache-Control: no-cache`, and then 200 with the fixture's body: a
 * cache on the path answering for a page it never fetched, and the one retry
 * that asks it to go back to the origin. */
#define FX_304_UNLESS_NOCACHE 1304

static void fx_reset(void) { g_nfx = 0; g_ncalls = 0; g_last_url[0] = 0;
                             g_last_body[0] = 0; g_last_hdrs[0] = 0;
                             memset(g_call_hdrs, 0, sizeof g_call_hdrs); }
static void fx_add(const char *match, long status, const char *body) {
  if (g_nfx < 16) g_fx[g_nfx++] = (fixture){ match, body, status, 0 };
}
static void fx_add_bin(const char *match, long status, const char *body, size_t len) {
  if (g_nfx < 16) g_fx[g_nfx++] = (fixture){ match, body, status, len };
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
  if (g_ncalls <= 16)
    snprintf(g_call_hdrs[g_ncalls - 1], sizeof g_call_hdrs[0], "%s", g_last_hdrs);
  out->status = 404; out->body = NULL; out->body_len = 0;
  for (int i = 0; i < g_nfx; i++) {
    if (strstr(url, g_fx[i].match)) {
      out->status = g_fx[i].status;
      if (out->status == FX_304_UNLESS_NOCACHE) {
        if (!strstr(g_last_hdrs, "Cache-Control: no-cache")) { out->status = 304; return 0; }
        out->status = 200;
      }
      if (g_fx[i].body && g_fx[i].len) {
        out->body = malloc(g_fx[i].len + 1);
        if (out->body) {
          memcpy(out->body, g_fx[i].body, g_fx[i].len);
          out->body[g_fx[i].len] = 0;
          out->body_len = g_fx[i].len;
        }
        return 0;
      }
      out->body = g_fx[i].body ? strdup(g_fx[i].body) : NULL;
      out->body_len = out->body ? strlen(out->body) : 0;
      return 0;
    }
  }
  return 0;                     /* completed exchange, 404 */
}
void http_response_free(http_response *r) { if (r) { free(r->body); r->body = NULL; } }
/* lib/jsonlist.c is linked for jsonlist_next_page(), the paging decision the
 * page_walk rows share with every VJSON collector (and lib/pagewalk.c because
 * jsonlist.c calls into it). The operator URL override and the feed key hash
 * are not exercised here, so they are stubbed; the VJSON fetcher reads the same
 * fixture table, so jsonlist_emit_paged() — the VJSON walk itself — is testable
 * here too (test 31). Every fetch in this test goes through http_request(). */
static long g_feed_status;
cJSON *feed_get_json(http_client *h, const char *url, int t) {
  http_response r = {0};
  http_request(h, "GET", url, NULL, NULL, 0, t, 0, &r);
  cJSON *doc = (r.status == 200 && r.body) ? cJSON_Parse(r.body) : NULL;
  g_feed_status = (r.status == 200 && r.body && !doc) ? FEED_ST_UNPARSED : r.status;
  http_response_free(&r);
  return doc;
}
/* lib/jsonlist.c and lib/pagewalk.c ask feedlib what a failed fetch met; this
 * stub's feed_get_json above records it the same way. */
long feed_last_json_status(void) { return g_feed_status; }
void feed_last_json_status_reset(void) { g_feed_status = FEED_ST_UNKNOWN; }
void feed_status_describe(long st, char *out, size_t cap) {
  if (st == FEED_ST_TRANSPORT)      snprintf(out, cap, "a transport failure");
  else if (st == FEED_ST_UNPARSED)  snprintf(out, cap, "an HTTP 2xx whose body was not JSON");
  else if (st > 0)                  snprintf(out, cap, "HTTP %ld", st);
  else                              snprintf(out, cap, "an unrecorded failure");
}
const char *url_override_apply(const char *url) { return url; }
void feed_hash_key(char *out21, const char *const *parts, int n) {
  (void)parts; (void)n; if (out21) out21[0] = 0;
}

/* ── capturing sink ──────────────────────────────────────────────────────── */
#define MAXCAP 64
typedef struct { char title[256], key[256], props[8192], link[512], rtype[64];
                 int has_geo; double lat, lon; } cap;
static cap g_cap[MAXCAP];
static int g_ncap = 0;
/* The WHOLE properties_json of each captured row — `props` above is bounded,
 * and a record carrying thousands of fields puts its `_fields_dropped` stamp
 * past the bound (test 36). */
static char *g_full[MAXCAP];

static int cap_emit(struct intel_sink *s, const intel_item *it) {
  (void)s;
  if (g_ncap >= MAXCAP) return -1;
  free(g_full[g_ncap]);
  g_full[g_ncap] = strdup(it->properties_json ? it->properties_json : "");
  cap *c = &g_cap[g_ncap++];
  snprintf(c->title, sizeof c->title, "%s", it->title ? it->title : "");
  snprintf(c->key,   sizeof c->key,   "%s", it->remote_key ? it->remote_key : "");
  snprintf(c->props, sizeof c->props, "%s", it->properties_json ? it->properties_json : "");
  snprintf(c->link,  sizeof c->link,  "%s", it->link ? it->link : "");
  snprintf(c->rtype, sizeof c->rtype, "%s", it->record_type ? it->record_type : "");
  c->has_geo = it->has_geo; c->lat = it->lat; c->lon = it->lon;
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
  /* One "lon,lat" field (CALIL's geocode), and the lat,lon order for the
   * other declaration. */
  { .id = "T_GEO_PAIR", .name = "paired coords", .url = "https://x.test/geo?q={q}",
    .array_path = "libs", .title_keys = "formal", .id_keys = "libid",
    .lonlat_key = "geocode", .record_type = "t-lib", .free_tier = 1, .description = "d" },
  { .id = "T_GEO_PAIR2", .name = "paired coords lat first", .url = "https://x.test/geo2?q={q}",
    .array_path = "libs", .title_keys = "formal", .id_keys = "libid",
    .latlon_key = "pos", .record_type = "t-lib", .free_tier = 1, .description = "d" },

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

  /* The data URL is PUBLISHED on an index page, not fixed (hpengine.h
   * index_url). Kawasaki city stamps its licence CSVs with the month and
   * offers no stable path, so a baked-in URL works until the next refresh and
   * then 404s — nine rows were dead that way, and one had been renamed rather
   * than re-dated, which no date arithmetic would have caught. The declared
   * .url stays as the endpoint of record; index_url overrides it at run time. */
  { .id = "T_INDEX", .name = "data url found on an index page",
    .url = "https://x.test/data/old202401.csv",
    .index_url = "https://x.test/portal.html", .index_href_must = "riyoujo",
    .mode = HP_CSV, .csv_no_header = 1, .interval = 3600,
    .title_keys = "col1", .id_keys = "col0",
    .record_type = "t-index", .free_tier = 1, .description = "d" },

  { .id = "T_INDEX_MISS", .name = "index page holds no matching link",
    .url = "https://x.test/data/old202401.csv",
    .index_url = "https://x.test/portal.html", .index_href_must = "nosuchfile",
    .mode = HP_CSV, .csv_no_header = 1, .interval = 3600,
    .record_type = "t-index-miss", .free_tier = 1, .description = "d" },

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
  /* The same walk with a row-declared conditional header: it belongs to the
   * first request only (test 9f-sexies). */
  { .id = "T_PAGE_COND", .name = "offset pagination, conditional first request",
    .url = "https://x.test/pc?q={q}",
    .headers = { "If-None-Match: \"v1\"" },
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

  /* A row whose URL ALREADY binds its page parameter — the shape of every API
   * that requires the parameter on the first request (PNCP answers 400 without
   * `pagina`). The engine used to APPEND, producing `…&pagina=1&pagina=2`, and
   * a server that reads the first occurrence then served page 1 for the whole
   * walk: N pages emitted, one page stored, run rc=0. 103 rows in the tree are
   * this shape. */
  { .id = "T_PAGE_INURL", .name = "page param already in the url",
    .url = "https://x.test/piu?q={q}&pagina=1",
    .array_path = "items", .title_keys = "name", .id_keys = "id",
    .page_param = "pagina", .page_start = 1,
    .record_type = "t-page", .free_tier = 1, .description = "d" },

  /* A 0-based page-numbered API. Without page_zero_based the engine coerces the
   * unset page_start to 1 and computes the first extra page as 2, skipping 1. */
  { .id = "T_PAGE_ZERO", .name = "zero-based paging", .url = "https://x.test/pz?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "id",
    .page_param = "page", .page_zero_based = 1,
    .record_type = "t-page", .free_tier = 1, .description = "d" },

  /* ArcGIS-shaped offset walk: the parameter is `resultOffset` (capital O) and
   * the URL binds resultOffset=0 on the first request. The engine used to
   * coerce page_start to 1 for any parameter without a lowercase "offset", so
   * it asked for 0, then step+1, 2·step+1 — one record lost per page boundary. */
  { .id = "T_PAGE_RESULTOFFSET", .name = "resultOffset walk",
    .url = "https://x.test/ro?q={q}&resultOffset=0",
    .array_path = "items", .title_keys = "name", .id_keys = "id",
    .page_param = "resultOffset", .page_size = 2,
    .record_type = "t-page", .free_tier = 1, .description = "d" },

  /* A 1-based offset API (OpenSearch `startIndex=1`): the walk must continue
   * from the URL's own 1, i.e. 1, 3, 5 — reading the URL is right both ways. */
  { .id = "T_PAGE_STARTINDEX1", .name = "1-based startIndex walk",
    .url = "https://x.test/si?q={q}&startIndex=1",
    .array_path = "items", .title_keys = "name", .id_keys = "id",
    .page_param = "startIndex", .page_size = 2,
    .record_type = "t-page", .free_tier = 1, .description = "d" },

  { .id = "T_CAPPED", .name = "declared cap", .url = "https://x.test/cap?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "id", .max_items = 2,
    .record_type = "t-cap", .free_tier = 1, .description = "d" },

  { .id = "T_DEEP_ALL", .name = "deepen every record", .url = "https://x.test/da?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "num",
    .detail_url = "https://x.test/d2/{v}", .detail_key = "num",
    .record_type = "t-deepall", .free_tier = 1, .description = "d" },

  /* The rows below declare NO paging of their own and opt into page_walk:
   * walk on the upstream's own evidence, by the decision every VJSON collector
   * makes (jsonlist_next_page). A row moved from VJSON onto this engine to walk
   * its detail hop must not lose its later pages. */
  { .id = "T_PAGE_AUTO", .name = "undeclared paging, server next link",
    .url = "https://x.test/pa?q={q}", .array_path = "items",
    .title_keys = "name", .id_keys = "id",
    .page_walk = 1,
    .record_type = "t-auto-page", .free_tier = 1, .description = "d" },

  { .id = "T_PAGE_CURSOR", .name = "undeclared paging, declared page size",
    .url = "https://x.test/pc?limit=2&offset=0", .array_path = "items",
    .title_keys = "name", .id_keys = "id", .interval = 3600,
    .page_walk = 1,
    .record_type = "t-auto-cursor", .free_tier = 1, .description = "d" },

  { .id = "T_PAGE_SHORT", .name = "undeclared paging, short first page",
    .url = "https://x.test/ps?limit=5", .array_path = "items",
    .title_keys = "name", .id_keys = "id", .interval = 3600,
    .page_walk = 1,
    .record_type = "t-auto-short", .free_tier = 1, .description = "d" },

  /* A full page, no next link, and a page size with no cursor sibling the
   * walk knows (`maxFeatures`): the walk stops — and says so in-band, because
   * a full page is evidence that more exists. */
  { .id = "T_PAGE_STUCK", .name = "full page, nothing to advance",
    .url = "https://x.test/pk?maxFeatures=2", .array_path = "items",
    .title_keys = "name", .id_keys = "id", .interval = 3600, .page_walk = 1,
    .record_type = "t-auto-stuck", .free_tier = 1, .description = "d" },

  /* `rp` is flexigrid's page size (taginfo), paired with a `page` cursor. */
  { .id = "T_PAGE_PROVEN", .name = "page size proven by the response",
    .url = "https://x.test/pp?page=1&rp=3", .array_path = "items",
    .title_keys = "name", .id_keys = "id", .interval = 3600,
    .page_walk = 1,
    .record_type = "t-proven", .free_tier = 1, .description = "d" },

  /* Same shape, but nothing in the URL is a page size and the upstream
   * declares no total, so there is no evidence of more and no page 2. */
  { .id = "T_PAGE_NOPROOF", .name = "no provable page size",
    .url = "https://x.test/np?page=1&order_by=name", .array_path = "items",
    .title_keys = "name", .id_keys = "id", .interval = 3600,
    .page_walk = 1,
    .record_type = "t-noproof", .free_tier = 1, .description = "d" },

  /* `?page=1` with no page size anywhere — ROR's and bio.tools' shape. Only the
   * upstream's declared total can say whether more remains. */
  { .id = "T_PAGE_TOTAL", .name = "cursor advanced on the declared total",
    .url = "https://x.test/pt?page=1", .array_path = "items",
    .title_keys = "name", .id_keys = "id", .interval = 3600,
    .page_walk = 1,
    .record_type = "t-total-page", .free_tier = 1, .description = "d" },

  { .id = "T_PAGE_SET", .name = "page_param replaces, never appends",
    .url = "https://x.test/pset?page=1&per=2", .array_path = "items",
    .title_keys = "name", .id_keys = "id", .page_param = "page", .interval = 3600,
    .record_type = "t-page-set", .free_tier = 1, .description = "d" },

  { .id = "T_TOTAL", .name = "upstream declares its own total",
    .url = "https://x.test/tot?q={q}", .array_path = "items",
    .title_keys = "name", .id_keys = "id", .max_items = 2,
    .record_type = "t-total", .free_tier = 1, .description = "d" },

  /* Composite record identity. 45 rows converted from VJSON_KEYED /
   * VJSON_IDKEYS carry their macro's id field over verbatim into .id_keys, and
   * _vjson_idkeys.inc promises that field means the same thing here ("`+`
   * composes, as in hpengine id_keys"). Nothing tested that promise. Getting it
   * wrong is silent — the records collapse onto one uid at the sink and the run
   * still reports success — which is why it is pinned here rather than trusted. */
  /* One day at a time: the date is in the URL, and the records live in three
   * sibling arrays of one response (SIDOF's morning / evening / extraordinary
   * editions). */
  { .id = "T_DATE_UNION", .name = "date token + array union",
    .url = "https://x.test/du/{date:%Y-%m-%d:-2}", .array_path = "Morning+Evening+Extra",
    .title_keys = "name", .id_keys = "id", .interval = 86400,
    .record_type = "t-date", .free_tier = 1, .description = "d" },

  { .id = "T_IDKEYS", .name = "composite id_keys", .url = "https://x.test/ik",
    .array_path = "rows", .title_keys = "name", .id_keys = "code+date",
    .interval = 3600, .record_type = "t-idk", .free_tier = 1, .description = "d" },

  /* The d-portal transaction rows key on an 8-part composite, because `aid`
   * alone is the ACTIVITY and would collapse every transaction under it. A
   * real ledger row leaves some dimensions null (no sector group, no ref), so
   * the realistic case is a long composite with holes in it. */
  /* A bare root array whose records each carry a LONGER nested array. The pair
   * below differ only in array_path, and that is the whole point: discovery
   * picks the densest array of objects, so the auto row mines the nested one
   * and the "." row takes the root. hex.pm's package list is this exact shape
   * and cost 3,643 phantom records before the root was declared. */
  { .id = "T_ROOTARR_AUTO", .name = "bare root array, discovery",
    .url = "https://x.test/rootarr", .title_keys = "name", .id_keys = "name",
    .interval = 3600, .record_type = "t-root", .free_tier = 1, .description = "d" },

  { .id = "T_ROOTARR_DOT", .name = "bare root array, declared",
    .url = "https://x.test/rootarr", .array_path = ".",
    .title_keys = "name", .id_keys = "name",
    .interval = 3600, .record_type = "t-root", .free_tier = 1, .description = "d" },

  /* NDJSON. The body is the shape index.golang.org and index.crates.io
   * publish: one complete JSON object per line, no array, no commas. Declared
   * HP_JSON it would die at line 2 and emit nothing forever. */
  { .id = "T_NDJSON", .name = "ndjson feed",
    .url = "https://x.test/ndjson", .mode = HP_NDJSON, .array_path = ".",
    .title_keys = "Path", .id_keys = "Path+Version",
    .interval = 3600, .record_type = "t-nd", .free_tier = 1, .description = "d" },

  /* A cursor that is the LAST RECORD's own field, not an envelope's: the Go
   * module index publishes no next link and pages by `since=<Timestamp of the
   * last record>`. `$last` is the only way to name it, because the array's
   * length is not known when the row is written. */
  { .id = "T_NDJSON_LASTCURSOR", .name = "ndjson paged by last record's field",
    .url = "https://x.test/ndpage", .mode = HP_NDJSON, .array_path = ".",
    .title_keys = "Path", .id_keys = "Path+Version",
    .next_path = "$last.Timestamp",
    .next_tmpl = "https://x.test/ndpage?since={v}",
    .page_max = 4,
    .interval = 3600, .record_type = "t-nd", .free_tier = 1, .description = "d" },

  /* {ago:N} — a `since` cursor relative to now. The fixture matches on the
   * YEAR only, because the rest of the timestamp is whatever the clock says
   * when the test runs; what is being pinned is that the token expands to a
   * real RFC 3339 instant in the URL rather than being left verbatim. */
  { .id = "T_AGO", .name = "relative since cursor",
    .url = "https://x.test/ago?since={ago:3600}", .mode = HP_NDJSON,
    .array_path = ".", .title_keys = "Path", .id_keys = "Path",
    .interval = 3600, .record_type = "t-ago", .free_tier = 1, .description = "d" },

  { .id = "T_IDKEYS_WIDE", .name = "wide composite with absent parts",
    .url = "https://x.test/wide", .array_path = "rows", .title_keys = "title",
    .id_keys = "aid+trans_id+trans_ref+trans_day+trans_value+trans_code+trans_sector+trans_country",
    .interval = 3600, .record_type = "t-wide", .free_tier = 1, .description = "d" },

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
  /* A dotted XML array_path, the way every other array_path in the engine is
   * written and the way the probe resolves it. */
  { .id = "T_XML_DOTTED", .name = "xml dotted path", .url = "https://x.test/rss.xml",
    .mode = HP_XML, .array_path = "channel.item", .interval = 3600,
    .title_keys = "title", .id_keys = "link",
    .record_type = "t-xmldot", .free_tier = 1, .description = "d" },
  /* hp_xml_flatten's bounds and repeats (test 36). */
  { .id = "T_XML_FLAT", .name = "xml flatten bounds", .url = "https://x.test/xf.xml",
    .mode = HP_XML, .array_path = "rec", .interval = 3600,
    .title_keys = "title", .id_keys = "id",
    .record_type = "t-xmlflat", .free_tier = 1, .description = "d" },
  { .id = "T_XML_ROWS", .name = "xml attribute-only records", .url = "https://x.test/rows.xml",
    .mode = HP_XML, .array_path = "row", .interval = 3600,
    .title_keys = "@name", .id_keys = "@id",
    .record_type = "t-xmlrow", .free_tier = 1, .description = "d" },
  { .id = "T_XML_STR", .name = "xml text-only records", .url = "https://x.test/str.xml",
    .mode = HP_XML, .array_path = "string", .interval = 3600,
    .record_type = "t-xmlstr", .free_tier = 1, .description = "d" },

  /* ── engine fixes of 2026-10-02 (tests 27-33) ── */
  /* A page-numbered page_walk row whose URL states a page size and NO page.
   * Spring Data (`size`+`page`) is 0-based; flexigrid-style `per_page` APIs
   * are 1-based; the URL cannot say which. */
  { .id = "T_PW_ZERO", .name = "page_walk, 0-based page numbers", .url = "https://x.test/wz?size=2",
    .array_path = "items", .title_keys = "name", .id_keys = "id", .interval = 3600,
    .page_walk = 1, .record_type = "t-pwz", .free_tier = 1, .description = "d" },
  { .id = "T_PW_ONE", .name = "page_walk, 1-based page numbers", .url = "https://x.test/wo?per_page=2",
    .array_path = "items", .title_keys = "name", .id_keys = "id", .interval = 3600,
    .page_walk = 1, .record_type = "t-pwo", .free_tier = 1, .description = "d" },
  /* An offset the server ignores, under an envelope that changes per request. */
  { .id = "T_PW_VOLATILE", .name = "page_walk, ignored cursor, volatile envelope",
    .url = "https://x.test/wv?limit=2", .array_path = "items",
    .title_keys = "name", .id_keys = "id", .interval = 3600,
    .page_walk = 1, .record_type = "t-pwv", .free_tier = 1, .description = "d" },
  /* page_walk + filter_query: a filtered-out record was still fetched. */
  { .id = "T_PW_FILTER", .name = "page_walk with filter_query", .url = "https://x.test/wf?q={q}",
    .array_path = "items", .title_keys = "name", .id_keys = "id", .filter_query = 1,
    .page_walk = 1, .record_type = "t-pwf", .free_tier = 1, .description = "d" },
  /* A record deep enough to trip the flatten depth guard, then XML/CSV rows. */
  { .id = "T_DEEPREC", .name = "record past the depth guard", .url = "https://x.test/dr",
    .array_path = "items", .title_keys = "name", .id_keys = "id", .interval = 3600,
    .record_type = "t-deeprec", .free_tier = 1, .description = "d" },
  /* A composite title longer than 64 bytes. */
  { .id = "T_TCOMP", .name = "long composite title", .url = "https://x.test/tc",
    .array_path = "features", .title_keys = "attributes.title+attributes.summary",
    .id_keys = "attributes.oid", .interval = 3600,
    .record_type = "t-tcomp", .free_tier = 1, .description = "d" },
  /* A ZIP-served CSV. */
  { .id = "T_ZIPCSV", .name = "zip-served csv", .url = "https://x.test/z.zip",
    .mode = HP_CSV, .title_keys = "name", .id_keys = "id", .interval = 3600,
    .record_type = "t-zip", .free_tier = 1, .description = "d" },
  /* An XML row with a detail hop and a budget of one. */
  { .id = "T_XML_DEEP", .name = "xml with a detail hop", .url = "https://x.test/xd",
    .mode = HP_XML, .array_path = "item", .title_keys = "name", .id_keys = "ref",
    .detail_url = "https://x.test/xdd/{v}", .detail_key = "ref", .detail_max = 1,
    .interval = 3600, .record_type = "t-xmldeep", .free_tier = 1, .description = "d" },
};
HP_REGISTER_TABLE(T)

/* A ZIP of `n` STORED entries (no compression, so the test needs no zlib and
 * the payload bytes appear in the archive verbatim), with a central directory
 * and end record. CRCs are 0: nothing in the engine checks them. Caller frees;
 * *len receives the size. */
static void put16(unsigned char *p, unsigned v) { p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; }
static void put32(unsigned char *p, unsigned long v) {
  p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; p[2] = (v >> 16) & 0xff; p[3] = (v >> 24) & 0xff;
}
static char *mk_zip(const char *const *names, const char *const *datas,
                    const size_t *lens, int n, size_t *len) {
  size_t cap = 22;
  for (int i = 0; i < n; i++) cap += 30 + 46 + 2 * strlen(names[i]) + lens[i];
  unsigned char *z = calloc(1, cap), *w = z;
  unsigned long offs[8];
  for (int i = 0; i < n && i < 8; i++) {
    size_t nl = strlen(names[i]);
    offs[i] = (unsigned long)(w - z);
    put32(w, 0x04034b50UL); put16(w + 4, 20); put16(w + 8, 0);
    put32(w + 18, (unsigned long)lens[i]); put32(w + 22, (unsigned long)lens[i]);
    put16(w + 26, (unsigned)nl); put16(w + 28, 0);
    memcpy(w + 30, names[i], nl); memcpy(w + 30 + nl, datas[i], lens[i]);
    w += 30 + nl + lens[i];
  }
  unsigned long cd = (unsigned long)(w - z);
  for (int i = 0; i < n && i < 8; i++) {
    size_t nl = strlen(names[i]);
    put32(w, 0x02014b50UL); put16(w + 4, 20); put16(w + 6, 20); put16(w + 10, 0);
    put32(w + 20, (unsigned long)lens[i]); put32(w + 24, (unsigned long)lens[i]);
    put16(w + 28, (unsigned)nl); put32(w + 42, offs[i]);
    memcpy(w + 46, names[i], nl);
    w += 46 + nl;
  }
  unsigned long cdsz = (unsigned long)(w - z) - cd;
  put32(w, 0x06054b50UL); put16(w + 8, (unsigned)n); put16(w + 10, (unsigned)n);
  put32(w + 12, cdsz); put32(w + 16, cd);
  w += 22;
  *len = (size_t)(w - z);
  return (char *)z;
}

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

  /* 6. credential gating: no key -> no request, not an error, and the empty is
   * DISCLOSED rather than silent.
   *
   * This used to assert g_ncap == 0. A bare zero is exactly the invisible
   * nothing CLAUDE.md rule 1 names: fetch_log status='ok' records=0, the run
   * line green, and no way for a consumer to tell "gated, never asked" from
   * "asked and the upstream had nothing". The twelve hand-written collectors
   * were fixed by _credential_notice.inc (audit #29); the engine's own gate is
   * the same defect in the other copy of the code, so it now emits that one
   * shape too. What must stay true is that the row is an ACCOUNTING record and
   * not a finding — constant remote_key, no observation in it — and that no
   * request is made, which is what the rest of this case pins. */
  fx_reset();
  unsetenv("HP_TEST_KEY");
  fx_add("/k?q=", 200, "{\"a\":[{\"name\":\"n\"}]}");
  rc = run_source("T_KEYED", "acme");
  ok(rc == 0 && g_ncalls == 0, "missing credential = no call, not an error");
  ok(g_ncap == 1, "missing credential is disclosed as one record, not silence");
  ok(g_ncap == 1 && strstr(g_cap[0].props, "\"status\":\"needs_credential\"") != NULL,
     "and that record is the status notice, carrying no observation");
  setenv("HP_TEST_KEY", "secret", 1);
  rc = run_source("T_KEYED", "acme");
  ok(g_ncalls == 1, "credential present = request made");
  ok(strstr(g_last_hdrs, "Authorization: Basic c2VjcmV0Og==") != NULL,
     "{keyb64} = base64(\"key:\") basic auth");

  /* 6b. the data URL is published on an index page, not fixed.
   *
   * Two things have to hold, and the second is the one that matters. The row
   * must fetch the link the index ACTUALLY carries today (a relative href,
   * resolved against the index URL) — and when no link matches, it must fail
   * honestly rather than fall back to the stale URL baked into .url. A
   * fallback would re-fetch last year's file and report success, which is the
   * exact silent staleness this opt exists to end. */
  fx_reset();
  fx_add("/portal.html", 200,
    "<html><ul>"
    "<li><a href=\"/data/02biyoujo202608.csv\">美容所</a></li>"
    "<li><a href=\"/data/01riyoujo202608.csv\">理容所</a></li>"
    "</ul></html>");
  fx_add("/data/01riyoujo202608.csv", 200, "A001,Shop One\nA002,Shop Two\n");
  fx_add("/data/old202401.csv", 200, "STALE,should never be fetched\n");
  rc = run_source("T_INDEX", NULL);
  ok(rc == 0 && g_ncap == 2, "T_INDEX read the CSV the index links to");
  ok(strstr(g_last_url, "01riyoujo202608.csv") != NULL,
     "the resolved href was fetched, not the declared .url");
  ok(strstr(g_last_url, "old202401") == NULL, "the stale declared url was not used");
  ok(g_ncalls == 2, "one index fetch + one data fetch");
  ok(g_ncap == 2 && !strcmp(g_cap[0].title, "Shop One"), "records come from the linked file");

  fx_reset();
  fx_add("/portal.html", 200, "<html><a href=\"/data/02biyoujo202608.csv\">x</a></html>");
  fx_add("/data/old202401.csv", 200, "STALE,should never be fetched\n");
  rc = run_source("T_INDEX_MISS", NULL);
  ok(rc == -1 && g_ncap == 0, "no matching href = honest failure, no records");
  ok(g_ncalls == 1, "and NO fallback fetch of the stale declared url");

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

  /* 9f-ter. an offset parameter not spelled "offset" starts at the URL's own
   * value. The pre-fix engine coerced page_start to 1 and asked for
   * resultOffset=3 — which no fixture below answers except the base page, so
   * the walk re-read page 1, stopped on the repeat, and stored 2 of 3 records. */
  fx_reset();
  fx_add("resultOffset=2", 200, "{\"items\":[{\"name\":\"o3\",\"id\":\"3\"}]}");
  fx_add("resultOffset=4", 200, "{\"items\":[]}");
  fx_add("/ro?q=", 200,
    "{\"items\":[{\"name\":\"o1\",\"id\":\"1\"},{\"name\":\"o2\",\"id\":\"2\"}]}");
  rc = run_source("T_PAGE_RESULTOFFSET", "x");
  ok(rc == 0 && g_ncap == 3, "resultOffset=0 walk advances to 2, not 3");
  ok(strstr(g_last_url, "resultOffset=4") != NULL,
     "resultOffset walk ends on the first empty page");

  /* 9f-quater. …and a 1-based offset keeps its 1: startIndex 1, 3, 5. */
  fx_reset();
  fx_add("startIndex=3", 200, "{\"items\":[{\"name\":\"s3\",\"id\":\"3\"}]}");
  fx_add("startIndex=5", 200, "{\"items\":[]}");
  fx_add("/si?q=", 200,
    "{\"items\":[{\"name\":\"s1\",\"id\":\"1\"},{\"name\":\"s2\",\"id\":\"2\"}]}");
  rc = run_source("T_PAGE_STARTINDEX1", "x");
  ok(rc == 0 && g_ncap == 3, "startIndex=1 walk advances to 3");
  ok(strstr(g_last_url, "startIndex=5") != NULL,
     "startIndex walk ends on the first empty page");

  /* 9f-quinquies. a later page refused with 429 is a walk cut short, and says
   * so. The pre-fix engine broke out of the loop in silence: two records, no
   * notice, rc=0 — indistinguishable from a complete two-record collection. */
  fx_reset();
  fx_add("offset=2", 429, "{\"error\":\"rate limited\"}");
  fx_add("/po?q=", 200,
    "{\"items\":[{\"name\":\"q1\",\"id\":\"1\"},{\"name\":\"q2\",\"id\":\"2\"}]}");
  rc = run_source("T_PAGE_PARAM", "x");
  {
    int notice = 0;
    for (int i = 0; i < g_ncap; i++)
      if (!strcmp(g_cap[i].rtype, "collector-truncation-notice")) notice = 1;
    ok(rc == 0 && notice, "a 429 on a later page emits a truncation notice");
  }

  /* 9f-sexies. A later page that does not deliver records: end of data, or a
   * walk cut short? (hp_later_page_cut). JO32_ARC_MLIT_SCHOOL stopped at
   * 36,000 of 56,807 on a 304 with rc=0 and no notice — every status other
   * than 429/5xx used to end a walk in silence. */
  {
#define TRUNC "collector-truncation-notice"
    const char *full2 = "{\"items\":[{\"name\":\"q1\",\"id\":\"1\"},{\"name\":\"q2\",\"id\":\"2\"}]}";
    const cap *tn = NULL;

    /* (a) a 304 the no-cache retry cannot clear: disclosed, with the page,
     *     the status and the URL, and pages_read counts the pages that
     *     delivered — not the request that failed. */
    fx_reset();
    fx_add("offset=2", 304, NULL);
    fx_add("/po?q=", 200, full2);
    rc = run_source("T_PAGE_PARAM", "x");
    ok(rc == 0 && cap_count("t-page", NULL) == 2 && cap_count(TRUNC, &tn) == 1,
       "9f-sexies: a 304 on a later page files a truncation notice");
    ok(tn && strstr(tn->props, "\"failed_page_status\":304") &&
             strstr(tn->props, "\"failed_page\":2") &&
             strstr(tn->props, "\"pages_read\":1") &&
             strstr(tn->props, "\"records_used\":2") &&
             strstr(tn->props, "\"failed_page_url\":\"https://x.test/po?q=x&offset=2\"") &&
             strstr(tn->props, "304 Not Modified") &&
             strstr(tn->title, "page 2 answered 304"),
       "9f-sexies: the notice states the failing page, its status, its URL and the pages read");
    ok(g_ncalls == 3 && strstr(g_call_hdrs[2], "Cache-Control: no-cache") &&
       !strstr(g_call_hdrs[1], "Cache-Control"),
       "9f-sexies: a 304 to an unconditional request is retried once with no-cache");

    /* (b) …and when the retry reaches the origin, the walk simply continues. */
    fx_reset();
    fx_add("offset=4", 200, "{\"items\":[]}");
    fx_add("offset=2", FX_304_UNLESS_NOCACHE, "{\"items\":[{\"name\":\"q3\",\"id\":\"3\"}]}");
    fx_add("/po?q=", 200, full2);
    rc = run_source("T_PAGE_PARAM", "x");
    ok(rc == 0 && cap_count("t-page", NULL) == 3 && cap_count(TRUNC, NULL) == 0,
       "9f-sexies: a 304 cleared by the no-cache retry loses nothing and files nothing");

    /* (c) a row-declared conditional header goes on the FIRST request only. */
    fx_reset();
    fx_add("offset=4", 200, "{\"items\":[]}");
    fx_add("offset=2", 200, "{\"items\":[{\"name\":\"c3\",\"id\":\"3\"}]}");
    fx_add("/pc?q=", 200, full2);
    rc = run_source("T_PAGE_COND", "x");
    ok(rc == 0 && cap_count("t-page", NULL) == 3 && g_ncalls == 3 &&
       strstr(g_call_hdrs[0], "If-None-Match: \"v1\"") &&
       !strstr(g_call_hdrs[1], "If-None-Match") && !strstr(g_call_hdrs[2], "If-None-Match"),
       "9f-sexies: a conditional header reaches page 1 and never a later page");

    /* (d) a 403 after a full page: refused, not finished. */
    fx_reset();
    fx_add("offset=2", 403, "forbidden");
    fx_add("/po?q=", 200, full2);
    rc = run_source("T_PAGE_PARAM", "x");
    ok(rc == 0 && cap_count("t-page", NULL) == 2 && cap_count(TRUNC, &tn) == 1 &&
       tn && strstr(tn->props, "\"failed_page_status\":403"),
       "9f-sexies: a 403 on a later page files a truncation notice");

    /* (e) a 404 after a FULL page of the declared size: the deep-paging
     *     signature, disclosed — but a 404 after a SHORT page is the end
     *     (18d below) and stays silent. */
    fx_reset();
    fx_add("offset=4", 404, NULL);
    fx_add("offset=2", 200, "{\"items\":[{\"name\":\"q3\",\"id\":\"3\"},{\"name\":\"q4\",\"id\":\"4\"}]}");
    fx_add("/po?q=", 200, full2);
    rc = run_source("T_PAGE_PARAM", "x");
    ok(rc == 0 && cap_count("t-page", NULL) == 4 && cap_count(TRUNC, &tn) == 1 &&
       tn && strstr(tn->props, "\"failed_page_status\":404") &&
       strstr(tn->props, "\"failed_page\":3") && strstr(tn->props, "\"pages_read\":2") &&
       strstr(tn->props, "deep-paging limit"),
       "9f-sexies: a 404 after a full page is a walk cut short, and says why it may be");
    fx_reset();
    fx_add("offset=4", 404, NULL);
    fx_add("offset=2", 200, "{\"items\":[{\"name\":\"q3\",\"id\":\"3\"}]}");
    fx_add("/po?q=", 200, full2);
    rc = run_source("T_PAGE_PARAM", "x");
    ok(rc == 0 && cap_count("t-page", NULL) == 3 && cap_count(TRUNC, NULL) == 0,
       "9f-sexies: a 404 after a short page is the end of the data, silently");
    /* …and so is a 500 after a short page (EPA Envirofacts answers a range
     *    past its last row that way). */
    fx_reset();
    fx_add("offset=4", 500, "boom");
    fx_add("offset=2", 200, "{\"items\":[{\"name\":\"q3\",\"id\":\"3\"}]}");
    fx_add("/po?q=", 200, full2);
    rc = run_source("T_PAGE_PARAM", "x");
    ok(rc == 0 && cap_count("t-page", NULL) == 3 && cap_count(TRUNC, NULL) == 0,
       "9f-sexies: a 500 after a short page is the end of the data, silently");

    /* (f) a later page that answers 200 with a body that is not JSON (a WAF
     *     or error page): disclosed, and records_used still counts the
     *     earlier pages — the early return used to rewrite it to 0. */
    fx_reset();
    fx_add("offset=2", 200, "<html><body>Access denied</body></html>");
    fx_add("/po?q=", 200, full2);
    rc = run_source("T_PAGE_PARAM", "x");
    ok(rc == 0 && cap_count("t-page", NULL) == 2 && cap_count(TRUNC, &tn) == 1 &&
       tn && strstr(tn->props, "\"failed_page_unreadable\":true") &&
       strstr(tn->props, "\"records_used\":2"),
       "9f-sexies: an unreadable 200 on a later page files a notice and keeps the count");

    /* (g) a 304 on the FIRST request of a row that sent no condition is not
     *     an honest empty: the source was never actually checked. */
    fx_reset();
    fx_add("/po?q=", 304, NULL);
    rc = run_source("T_PAGE_PARAM", "x");
    ok(rc == -1 && cap_count("t-page", NULL) == 0 && g_ncalls == 2,
       "9f-sexies: an unrequested 304 on page 1 is retried, then reported as an error");
#undef TRUNC
  }

  /* 9f-bis. a row whose URL already binds its page parameter.
   *
   * The engine appended, so page 2 was requested as `…&pagina=1&pagina=2`.
   * Servers that bind the FIRST occurrence (Spring, JAX-RS) then answer with
   * page 1 again, forever: the walk emits N pages of the same records, the
   * sink stores one page, and the run exits 0. BR_PNCP_CONTRATOS lost 4,499
   * of 5,000 records that way with every gate in this repo green.
   *
   * The fixtures below are keyed on the WHOLE pagina= assignment, so a URL
   * carrying two of them matches the page-1 fixture and this test fails the
   * way the defect failed — a duplicated parameter cannot pass silently. */
  fx_reset();
  fx_add("pagina=2", 200, "{\"items\":[{\"name\":\"b2\",\"id\":\"2\"}]}");
  fx_add("pagina=3", 200, "{\"items\":[]}");
  fx_add("pagina=1", 200, "{\"items\":[{\"name\":\"b1\",\"id\":\"1\"}]}");
  rc = run_source("T_PAGE_INURL", "x");
  ok(rc == 0 && g_ncap == 2 && g_ncalls == 3,
     "9f-bis: a page param already in the url is REPLACED, not appended");
  ok(strstr(g_last_url, "pagina=3") != NULL &&
     strstr(g_last_url, "pagina=1") == NULL,
     "9f-bis: the last request carries exactly one pagina=, and it is the last page");

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

  /* 9i. and advances the URL's own offset by its own declared page size while
   * pages come back exactly that full. */
  fx_reset();
  fx_add("offset=2", 200, "{\"items\":[{\"name\":\"c3\",\"id\":\"3\"}]}");
  fx_add("/pc?limit=2", 200,
    "{\"items\":[{\"name\":\"c1\",\"id\":\"1\"},{\"name\":\"c2\",\"id\":\"2\"}]}");
  rc = run_source("T_PAGE_CURSOR", "");
  ok(rc == 0 && g_ncap == 3 && g_ncalls == 2,
     "undeclared paging advances limit/offset while pages come back full");
  ok(strstr(g_last_url, "offset=2") != NULL, "cursor advanced by the declared page size");

  /* 9i2. a full page with nothing the walk can advance is not followed with a
   * guessed parameter; the shortfall is stated as a truncation-notice record. */
  fx_reset();
  fx_add("/pk?maxFeatures=2", 200,
    "{\"items\":[{\"name\":\"k1\",\"id\":\"1\"},{\"name\":\"k2\",\"id\":\"2\"}]}");
  rc = run_source("T_PAGE_STUCK", "");
  ok(rc == 0 && g_ncalls == 1 && g_ncap == 3,
     "full page with no cursor sibling: one request, no guessed cursor");
  ok(g_ncap == 3 && strcmp(g_cap[2].rtype, "collector-truncation-notice") == 0 &&
     strstr(g_cap[2].props, "came back full") != NULL,
     "…and the full last page is disclosed in-band");

  /* 9j. a SHORT first page is the upstream saying it is finished. Following it
   * would be inventing a page that was never offered. */
  fx_reset();
  fx_add("/ps?limit=5", 200, "{\"items\":[{\"name\":\"s1\",\"id\":\"1\"}]}");
  rc = run_source("T_PAGE_SHORT", "");
  ok(rc == 0 && g_ncap == 1 && g_ncalls == 1,
     "a short page stops the walk — no guessed second request");

  /* 9j2. `rp=3` is the page size and `page` its cursor: 3 back, page 2 is
   * asked for, and its 1 record is a short page that ends the walk. */
  fx_reset();
  fx_add("page=2", 200, "{\"items\":[{\"name\":\"r4\",\"id\":\"4\"}]}");
  fx_add("/pp?page=1&rp=3", 200,
    "{\"items\":[{\"name\":\"r1\",\"id\":\"1\"},{\"name\":\"r2\",\"id\":\"2\"},"
    "{\"name\":\"r3\",\"id\":\"3\"}]}");
  rc = run_source("T_PAGE_PROVEN", "");
  ok(rc == 0 && g_ncap == 4 && g_ncalls == 2,
     "a flexigrid rp/page walk advances while pages come back full");
  ok(strstr(g_last_url, "page=2") != NULL && strstr(g_last_url, "rp=3") != NULL,
     "the cursor advanced and the proven page size was left alone");

  /* 9j3. no page size and no declared total: no evidence of a page 2. */
  fx_reset();
  fx_add("/np?page=1", 200,
    "{\"items\":[{\"name\":\"n1\",\"id\":\"1\"},{\"name\":\"n2\",\"id\":\"2\"}]}");
  rc = run_source("T_PAGE_NOPROOF", "");
  ok(rc == 0 && g_ncap == 2 && g_ncalls == 1,
     "no page size and no total = one request, not a guessed page 2");

  /* 9j4. `?page=1` with no page size at all (ROR's shape): the upstream
   * declares 3 and hands over 2, so it has said itself that more remains. */
  fx_reset();
  fx_add("page=2", 200,
    "{\"number_of_results\":3,\"items\":[{\"name\":\"t3\",\"id\":\"3\"}]}");
  fx_add("/pt?page=1", 200,
    "{\"number_of_results\":3,\"items\":[{\"name\":\"t1\",\"id\":\"1\"},"
    "{\"name\":\"t2\",\"id\":\"2\"}]}");
  rc = run_source("T_PAGE_TOTAL", "");
  ok(rc == 0 && g_ncap == 3 && g_ncalls == 2,
     "a declared total moves a page cursor the URL gives no page size for");
  ok(strstr(g_last_url, "page=2") != NULL, "page cursor advanced to 2");

  /* 9j5. and once the declared total is reached the walk stops — the upstream
   * has handed over everything it said it had. */
  fx_reset();
  fx_add("/pt?page=1", 200,
    "{\"number_of_results\":2,\"items\":[{\"name\":\"u1\",\"id\":\"1\"},"
    "{\"name\":\"u2\",\"id\":\"2\"}]}");
  rc = run_source("T_PAGE_TOTAL", "");
  ok(rc == 0 && g_ncap == 2 && g_ncalls == 1,
     "walk stops when the declared total has been delivered");

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
  /* 24f. `array_path=channel.item` on an RSS row: the XML path is an element
   *     name, so the literal "channel.item" repeats zero times. 30 batch-30
   *     rows (bank and ward RSS feeds) probed PASS and emitted nothing. */
  fx_reset();
  fx_add("/rss.xml", 200,
    "<?xml version=\"1.0\"?><rss><channel><title>Bank</title>"
    "<item><title>Notice A</title><link>http://b.example/a</link></item>"
    "<item><title>Notice B</title><link>http://b.example/b</link></item>"
    "</channel></rss>");
  rc = run_source("T_XML_DOTTED", NULL);
  ok(rc == 0 && g_ncap == 2 && !strcmp(g_cap[1].title, "Notice B"),
     "24f: a dotted xml array_path falls back to its last segment as the element name");

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

  /* 25d-g. Camera-index layouts (batch 30, 2026-09-21): the anchor is an
   *     image or a map pin with no text and no alt; its name is an attribute
   *     of the anchor, or the text beside it — after it, in the next table
   *     cell, or before it. Each used to be dropped. An image anchor with NO
   *     neighbouring text is still dropped (rec/94), and a neighbour's text
   *     is never borrowed across another anchor (rec/95 does not take
   *     rec/96's label). */
  fx_reset();
  fx_add("/h?q=", 200,
    "<html><ul>"
    "<li><a href=\"/rec/c1\" title=\"Kamo Bridge\"><img src=\"1.jpg\"></a></li>"
    "<li><a href=\"/rec/c2\"><img src=\"2.jpg\"></a> Sakura Weir<br></li>"
    "</ul><table>"
    "<tr><td><a href=\"/rec/c3\"><img src=\"3.jpg\"></a></td><td>Route 8 Tunnel</td></tr>"
    "<tr><td>Harbour East</td><td><a href=\"/rec/c4\"><img src=\"4.jpg\"></a></td></tr>"
    "<tr><td><a href=\"/rec/c94\"><img src=\"5.jpg\"></a></td></tr>"
    "<tr><td><a href=\"/rec/c95\"><img src=\"6.jpg\"></a><a href=\"/rec/c96\">Named</a></td></tr>"
    "</table></html>");
  rc = run_source("T_HTML", "x");
  ok(rc == 0 && g_ncap == 5, "25d: attribute and sibling-labelled image anchors emit; a bare one does not");
  ok(g_ncap >= 1 && !strcmp(g_cap[0].title, "Kamo Bridge"),
     "25d: the anchor's own title attribute labels it");
  ok(g_ncap >= 2 && !strcmp(g_cap[1].title, "Sakura Weir"),
     "25e: text following </a> labels an image anchor");
  ok(g_ncap >= 3 && !strcmp(g_cap[2].title, "Route 8 Tunnel"),
     "25f: the next table cell labels an icon cell");
  ok(g_ncap >= 4 && !strcmp(g_cap[3].title, "Harbour East"),
     "25g: text preceding <a> in the same row labels it");
  ok(g_ncap >= 5 && !strcmp(g_cap[4].title, "Named"),
     "25g: a bare anchor followed by another anchor borrows nothing");

  /* 25h-i. Hokkaido keeps the camera name as data-title on the anchor's
   *     PARENT div (the anchor holds empty spans); Hamada writes its hrefs
   *     with a leading space inside the quotes. */
  fx_reset();
  fx_add("/h?q=", 200,
    "<html>"
    "<div class=\"cam\" data-title=\"Ishikari Weir\"><a href=\"/rec/c7\"><span></span></a></div>"
    "<div><a href=\" ./rec/c8 \"><img src=\"8.jpg\"></a> Trim Me</div>"
    "</html>");
  rc = run_source("T_HTML", "x");
  ok(rc == 0 && g_ncap == 2, "25h: parent-labelled and space-padded anchors both emit");
  ok(g_ncap >= 1 && !strcmp(g_cap[0].title, "Ishikari Weir"),
     "25h: data-title on the anchor's parent labels a first-child anchor");
  ok(g_ncap >= 2 && !strcmp(g_cap[1].link, "https://x.test/rec/c8") &&
     !strcmp(g_cap[1].title, "Trim Me"),
     "25i: whitespace inside the href quotes is stripped before resolution");

  /* 26. A single field carrying both coordinates. CALIL's library directory
   *     has `geocode: "139.69,35.68"` (lon first); lat_key/lon_key each name a
   *     whole field, so 7,606 geocoded branches landed without a position. */
  fx_reset();
  fx_add("/geo?q=", 200,
    "{\"libs\":[{\"libid\":\"1\",\"formal\":\"Central Lib\",\"geocode\":\"139.6917,35.6895\"},"
    "{\"libid\":\"2\",\"formal\":\"No Geo\",\"geocode\":\"\"},"
    "{\"libid\":\"3\",\"formal\":\"Spaced\",\"geocode\":\"135.5 34.7\"}]}");
  rc = run_source("T_GEO_PAIR", "x");
  ok(rc == 0 && g_ncap == 3, "26: all three records emit regardless of geocode");
  ok(g_ncap >= 1 && g_cap[0].has_geo && fabs(g_cap[0].lat - 35.6895) < 1e-6 &&
     fabs(g_cap[0].lon - 139.6917) < 1e-6, "26a: lonlat_key splits \"lon,lat\" into lon then lat");
  ok(g_ncap >= 2 && !g_cap[1].has_geo, "26b: an empty pair field carries no position");
  ok(g_ncap >= 3 && g_cap[2].has_geo && fabs(g_cap[2].lat - 34.7) < 1e-6,
     "26c: a space-separated pair is accepted too");
  fx_reset();
  fx_add("/geo2?q=", 200,
    "{\"libs\":[{\"libid\":\"9\",\"formal\":\"Lat First\",\"pos\":\"35.0,139.0\"}]}");
  rc = run_source("T_GEO_PAIR2", "x");
  ok(rc == 0 && g_ncap == 1 && g_cap[0].has_geo && fabs(g_cap[0].lat - 35.0) < 1e-6 &&
     fabs(g_cap[0].lon - 139.0) < 1e-6, "26d: latlon_key reads lat then lon");

  /* composite id_keys: two records sharing `code` must NOT collapse — the
   * tuple (code, date) is the identity, which is what most registers are. */
  fx_reset();
  fx_add("/ik", 200,
    "{\"rows\":[{\"name\":\"a\",\"code\":\"X1\",\"date\":\"2026-01\"},"
    "{\"name\":\"b\",\"code\":\"X1\",\"date\":\"2026-02\"}]}");
  rc = run_source("T_IDKEYS", "");
  ok(rc == 0 && g_ncap == 2, "composite id_keys emits both records");
  ok(strcmp(g_cap[0].key, g_cap[1].key) != 0,
     "`code+date` composes a distinct uid per record (a shared `code` must not collapse them)");
  ok(strstr(g_cap[0].key, "X1") != NULL && strstr(g_cap[0].key, "2026-01") != NULL,
     "both parts of the composite reach the uid");

  /* {date:FMT:-2} renders two days before today (UTC), and `a+b+c` emits the
   * records of every sibling array that is present — the absent one is not an
   * error. */
  {
    char want[32];
    time_t t2 = time(NULL) - 2 * 86400;
    struct tm g2;
    gmtime_r(&t2, &g2);
    strftime(want, sizeof want, "/du/%Y-%m-%d", &g2);
    fx_reset();
    fx_add("/du/", 200,
      "{\"Morning\":[{\"id\":\"m1\",\"name\":\"a\"},{\"id\":\"m2\",\"name\":\"b\"}],"
      "\"Evening\":[{\"id\":\"e1\",\"name\":\"c\"}]}");
    rc = run_source("T_DATE_UNION", "");
    ok(strstr(g_last_url, want) != NULL, "{date:%Y-%m-%d:-2} renders the date two days back");
    ok(strstr(g_last_url, "{date") == NULL, "no date token left in the requested url");
    ok(rc == 0 && g_ncap == 3, "array_path a+b+c emits every present array (2 + 1), the absent one is no error");
  }

  /* 27. page_walk on a 0-based page-numbered API whose URL states a size and
   *     no page (Spring Data; Diavgeia). The walk used to follow the implicit
   *     page 0 with page 2 — page 1 was never requested and its records were
   *     lost on every run, with nothing disclosed. It now asks for page 1, which
   *     on a 0-based API is new data. */
  fx_reset();
  fx_add("wz?size=2&page=1", 200, "{\"items\":[{\"name\":\"r2\",\"id\":\"2\"},{\"name\":\"r3\",\"id\":\"3\"}]}");
  fx_add("wz?size=2&page=2", 200, "{\"items\":[{\"name\":\"r4\",\"id\":\"4\"},{\"name\":\"r5\",\"id\":\"5\"}]}");
  fx_add("wz?size=2&page=3", 200, "{\"items\":[]}");
  fx_add("wz?size=2", 200, "{\"items\":[{\"name\":\"r0\",\"id\":\"0\"},{\"name\":\"r1\",\"id\":\"1\"}]}");
  rc = run_source("T_PW_ZERO", "");
  ok(rc == 0 && cap_count("t-pwz", NULL) == 6 && g_ncap == 6,
     "27: a 0-based page walk reads pages 0, 1, 2 — page 1 is no longer skipped");
  ok(g_ncalls == 4 && strstr(g_last_url, "page=3") != NULL,
     "27: four requests (implicit 0, 1, 2, then the empty 3)");

  /* 28. the same walk on a 1-based API: the page-1 request returns the page
   *     already read. That repeat is recognised as the probe it is, emits
   *     nothing, is not counted as a page, and the walk continues at page 2. */
  fx_reset();
  fx_add("wo?per_page=2&page=1", 200, "{\"items\":[{\"name\":\"a1\",\"id\":\"1\"},{\"name\":\"a2\",\"id\":\"2\"}]}");
  fx_add("wo?per_page=2&page=2", 200, "{\"items\":[{\"name\":\"a3\",\"id\":\"3\"},{\"name\":\"a4\",\"id\":\"4\"}]}");
  fx_add("wo?per_page=2&page=3", 200, "{\"items\":[{\"name\":\"a5\",\"id\":\"5\"}]}");
  fx_add("wo?per_page=2", 200, "{\"items\":[{\"name\":\"a1\",\"id\":\"1\"},{\"name\":\"a2\",\"id\":\"2\"}]}");
  rc = run_source("T_PW_ONE", "");
  ok(rc == 0 && cap_count("t-pwo", NULL) == 5 && g_ncap == 5,
     "28: a 1-based page walk emits a1..a5 once each, and no notice");
  ok(g_ncalls == 4 && strstr(g_last_url, "page=3") != NULL,
     "28: the page-1 probe costs one request; the walk continues at 2 and 3");
  {
    const cap *c3 = NULL;
    for (int i = 0; i < g_ncap; i++) if (!strcmp(g_cap[i].title, "a3")) c3 = &g_cap[i];
    ok(c3 && strstr(c3->props, "\"_page\":2") != NULL,
       "28: the probe is not counted as a page (page=2's records are page 2)");
  }

  /* 29. page_walk, the server ignores the cursor and stamps every response
   *     with a fresh `took`. The body hash never matched, so the same two
   *     records were re-emitted for 20 pages and a page-ceiling truncation
   *     notice claimed pages were pending. The records repeat; the walk stops
   *     before re-emitting them and says the cursor was ignored. */
  fx_reset();
  fx_add("wv?limit=2&offset=2", 200,
    "{\"took\":2,\"items\":[{\"name\":\"v1\",\"id\":\"1\"},{\"name\":\"v2\",\"id\":\"2\"}]}");
  fx_add("wv?limit=2", 200,
    "{\"took\":1,\"items\":[{\"name\":\"v1\",\"id\":\"1\"},{\"name\":\"v2\",\"id\":\"2\"}]}");
  rc = run_source("T_PW_VOLATILE", "");
  {
    const cap *nt = NULL;
    ok(rc == 0 && g_ncalls == 2 && cap_count("t-pwv", NULL) == 2,
       "29: an ignored cursor under a changing envelope stops after one repeat, nothing re-emitted");
    ok(cap_count("collector-truncation-notice", NULL) == 0,
       "29: and files no page-ceiling truncation notice");
    ok(cap_count(NOTICE, &nt) == 1 && nt && strstr(nt->key, "page-param-ignored") &&
       strstr(nt->title, "repeated the records of page 1") != NULL,
       "29: the repeat is disclosed as page-param-ignored, by its records");
  }

  /* 29b. page_walk + filter_query: the upstream declares 3, hands over 3, and
   *     the filter keeps 1. Comparing the declared total with what was EMITTED
   *     filed a truncation notice on a walk that had read everything. */
  fx_reset();
  fx_add("/wf?q=", 200,
    "{\"total\":3,\"items\":[{\"name\":\"acme corp\",\"id\":\"1\"},"
    "{\"name\":\"foo\",\"id\":\"2\"},{\"name\":\"bar\",\"id\":\"3\"}]}");
  rc = run_source("T_PW_FILTER", "acme");
  ok(rc == 0 && cap_count("t-pwf", NULL) == 1 && g_ncalls == 1 &&
     cap_count("collector-truncation-notice", NULL) == 0,
     "29b: filtered-out records were fetched — no false truncation notice");

  /* 30. A ZIP-served CSV. (a) one entry whose bytes contain "PK\3\4": the old
   *     signature scan read that as a second entry; the archive's own count
   *     says one. (b) two entries: disclosed. (c) the inflated entry is freed
   *     — it leaked once per page, up to 256 MB each. */
  {
    static const char csv1[] = "id,name\n1,alpha PK\x03\x04 beta\n2,gamma\n";
    const char *nm[2] = { "a.csv", "b.csv" };
    const char *dt[2] = { csv1, "id,name\n9,other\n" };
    size_t ln[2] = { sizeof csv1 - 1, strlen("id,name\n9,other\n") };
    size_t zl = 0;
    char *z1 = mk_zip(nm, dt, ln, 1, &zl);
    fx_reset();
    fx_add_bin("/z.zip", 200, z1, zl);
    rc = run_source("T_ZIPCSV", "");
    ok(rc == 0 && cap_count("t-zip", NULL) == 2 && cap_count(NOTICE, NULL) == 0,
       "30a: a one-entry ZIP whose data contains PK\\3\\4 reads 2 records and files no zip-extra notice");
    free(z1);
    size_t zl2 = 0;
    char *z2 = mk_zip(nm, dt, ln, 2, &zl2);
    fx_reset();
    fx_add_bin("/z.zip", 200, z2, zl2);
    rc = run_source("T_ZIPCSV", "");
    const cap *nt = NULL;
    ok(rc == 0 && cap_count("t-zip", NULL) == 2 && cap_count(NOTICE, &nt) == 1 && nt &&
       strstr(nt->key, "zip-extra-entries") != NULL,
       "30b: a two-entry ZIP reads the first and discloses the second");
    free(z2);

    /* (c) 2 MB entry, ten runs. Leaking it would hold 20 MB. */
    size_t big = 2u << 20;
    char *bd = malloc(big + 64);
    int hl = snprintf(bd, 64, "id,name\n1,");
    memset(bd + hl, 'x', big - (size_t)hl - 1);
    bd[big - 1] = '\n';
    const char *bn[1] = { "big.csv" };
    const char *bdt[1] = { bd };
    size_t bl[1] = { big };
    size_t zl3 = 0;
    char *z3 = mk_zip(bn, bdt, bl, 1, &zl3);
    free(bd);
    fx_reset();
    fx_add_bin("/z.zip", 200, z3, zl3);
    run_source("T_ZIPCSV", "");                 /* warm-up */
    long h0 = heap_in_use();
    for (int i = 0; i < 10; i++) run_source("T_ZIPCSV", "");
    long h1 = heap_in_use();
    if (h0 >= 0 && h1 >= 0)
      ok(h1 - h0 < (long)(4u << 20),
         "30c: ten runs of a 2 MB ZIP entry do not grow the heap (the entry is freed)");
    else
      printf("  skip  30c: heap statistics unavailable on this platform\n");
    free(z3);
  }

  /* 31. The VJSON walk itself (jsonlist_emit_paged), both bases. */
  {
    intel_sink vs = { .ctx = NULL, .emit = cap_emit };
    fx_reset();
    g_ncap = 0;
    fx_add("vz?size=2&page=1", 200, "{\"items\":[{\"name\":\"r2\",\"id\":\"2\"},{\"name\":\"r3\",\"id\":\"3\"}]}");
    fx_add("vz?size=2&page=2", 200, "{\"items\":[{\"name\":\"r4\",\"id\":\"4\"}]}");
    fx_add("vz?size=2", 200, "{\"items\":[{\"name\":\"r0\",\"id\":\"0\"},{\"name\":\"r1\",\"id\":\"1\"}]}");
    int n = jsonlist_emit_paged(&vs, "VJ_ZERO", NULL, "https://x.test/vz?size=2", 1000,
                                "items", "t-vj", "en", "[]");
    ok(n == 5 && g_ncalls == 3,
       "31a: VJSON 0-based walk reads implicit 0, then 1 and 2 (5 records, 3 requests)");
    fx_reset();
    g_ncap = 0;
    fx_add("vo?per_page=2&page=1", 200, "{\"items\":[{\"name\":\"a1\",\"id\":\"1\"},{\"name\":\"a2\",\"id\":\"2\"}]}");
    fx_add("vo?per_page=2&page=2", 200, "{\"items\":[{\"name\":\"a3\",\"id\":\"3\"}]}");
    fx_add("vo?per_page=2", 200, "{\"items\":[{\"name\":\"a1\",\"id\":\"1\"},{\"name\":\"a2\",\"id\":\"2\"}]}");
    n = jsonlist_emit_paged(&vs, "VJ_ONE", NULL, "https://x.test/vo?per_page=2", 1000,
                            "items", "t-vj", "en", "[]");
    ok(n == 3 && g_ncap == 3 && g_ncalls == 3,
       "31b: VJSON 1-based walk: the page-1 probe repeats page 1, nothing re-emitted, page 2 read");
    char *nx = jsonlist_page_one_retry("https://x.test/a?size=2&page=1",
                                       "https://x.test/a?size=2&page=2");
    ok(nx == NULL, "31c: a repeat that is not the page-1 probe is not retried");
    free(nx);

    /* 31d. A later VJSON page that fails is disclosed with its page, status
     *      and URL. It used to be filed as "the page ceiling stopped the walk"
     *      — a ceiling that a 2-page walk never reached. */
    fx_reset();
    g_ncap = 0;
    fx_add("vf?offset=2&limit=2", 304, NULL);
    fx_add("vf?offset=0&limit=2", 200, "{\"items\":[{\"name\":\"f1\",\"id\":\"1\"},{\"name\":\"f2\",\"id\":\"2\"}]}");
    n = jsonlist_emit_paged(&vs, "VJ_FAIL", NULL, "https://x.test/vf?offset=0&limit=2", 1000,
                            "items", "t-vj", "en", "[]");
    {
      const cap *tn = NULL;
      int nn = cap_count("collector-truncation-notice", &tn);
      ok(n == 2 && nn == 1 && tn &&
         strstr(tn->props, "\"failed_page\":2") &&
         strstr(tn->props, "\"failed_page_status\":304") &&
         strstr(tn->props, "\"failed_page_url\":\"https://x.test/vf?offset=2&limit=2\"") &&
         strstr(tn->props, "page 2 answered HTTP 304") &&
         !strstr(tn->props, "page ceiling stopped"),
         "31d: a failed later VJSON page names the page, the status and the URL");
    }
  }

  /* 32. Flatten accounting is per RECORD on every path. A JSON record past the
   *     depth guard set the thread's counters; the XML, headerless-CSV and
   *     HTML paths never reset them, so their next records were stamped
   *     `_fields_dropped` they never had. */
  {
    static const char deep[] =
      "{\"items\":[{\"id\":\"1\",\"name\":\"deep\",\"a\":{\"b\":{\"c\":{\"d\":{\"e\":"
      "{\"f\":{\"g\":{\"h\":{\"i\":{\"j\":{\"k\":1}}}}}}}}}}}]}";
    fx_reset();
    fx_add("/dr", 200, deep);
    rc = run_source("T_DEEPREC", "");
    ok(rc == 0 && g_ncap >= 1 && strstr(g_cap[0].props, "_fields_dropped") != NULL,
       "32: the deep JSON record itself is stamped");
    fx_reset();
    fx_add("/x?q=", 200,
      "<list><target><ref>A1</ref><name>Alpha</name></target>"
      "<target><ref>B2</ref><name>Beta</name></target></list>");
    rc = run_source("T_XML", "x");
    int stale = 0;
    for (int i = 0; i < g_ncap; i++) if (strstr(g_cap[i].props, "_fields_dropped")) stale++;
    ok(rc == 0 && g_ncap == 2 && !stale, "32a: the XML records after it are not stamped");
    fx_reset(); fx_add("/dr", 200, deep); run_source("T_DEEPREC", "");
    fx_reset();
    fx_add("/bare.csv", 200, "10.0.0.1,telnet\n10.0.0.2,ssh\n");
    rc = run_source("T_CSV_BARE", "");
    stale = 0;
    for (int i = 0; i < g_ncap; i++) if (strstr(g_cap[i].props, "_fields_dropped")) stale++;
    ok(rc == 0 && g_ncap == 2 && !stale, "32b: nor are headerless CSV records");
    fx_reset(); fx_add("/dr", 200, deep); run_source("T_DEEPREC", "");
    fx_reset();
    fx_add("/h?q=", 200, "<html><a href=\"/rec/9\">Ninth record</a></html>");
    rc = run_source("T_HTML", "x");
    stale = 0;
    for (int i = 0; i < g_ncap; i++) if (strstr(g_cap[i].props, "_fields_dropped")) stale++;
    ok(rc == 0 && g_ncap == 1 && !stale, "32c: nor are HTML anchors");
  }

  /* 33. A composite title longer than 64 bytes is SHOWN, not hashed. */
  fx_reset();
  fx_add("/tc", 200,
    "{\"features\":[{\"attributes\":{\"oid\":7,"
    "\"title\":\"\xE5\xB7\x9D\xE5\xB4\x8E\xE5\xB8\x82\xE4\xB8\xAD\xE5\x8E\x9F\xE5\x8C\xBA road occupancy permits\","
    "\"summary\":\"FY2026 Q3 list of occupancy permits for poles and buried plant\"}}]}");
  rc = run_source("T_TCOMP", "");
  ok(rc == 0 && g_ncap == 1 &&
     !strcmp(g_cap[0].title, "\xE5\xB7\x9D\xE5\xB4\x8E\xE5\xB8\x82\xE4\xB8\xAD\xE5\x8E\x9F\xE5\x8C\xBA"
                             " road occupancy permits|FY2026 Q3 list of occupancy permits for poles and buried plant"),
     "33: a composite title over 64 bytes is the joined text, not a hex hash");
  ok(g_ncap == 1 && !strcmp(g_cap[0].key, "T_TCOMP|7"), "33: and the uid is still the declared id");

  /* 34. An icon anchor whose caption PRECEDES it in its own paragraph. The
   *     scan after </a> did not stop at </p>, so each PDF was labelled with
   *     the NEXT paragraph's caption. Paragraph and division boundaries now
   *     stop both scans; the following text, finding none, yields to the
   *     caption before the icon. */
  fx_reset();
  fx_add("/h?q=", 200,
    "<html><div class=\"list\">"
    "<p>Minutes 2026-01 <a href=\"/rec/m1\"><img src=\"pdf.gif\"></a></p>"
    "<p>Minutes 2026-02 <a href=\"/rec/m2\"><img src=\"pdf.gif\"></a></p>"
    "</div><div><a href=\"/rec/m3\"><img src=\"pdf.gif\"></a> Caption After</div></html>");
  rc = run_source("T_HTML", "x");
  ok(rc == 0 && g_ncap == 3 && !strcmp(g_cap[0].title, "Minutes 2026-01") &&
     !strcmp(g_cap[1].title, "Minutes 2026-02"),
     "34: a caption before its icon in the same <p> labels it, not the next one");
  ok(g_ncap == 3 && !strcmp(g_cap[2].title, "Caption After"),
     "34: a caption after the icon is still read when nothing precedes it");
  /* 34b. The two live layouts the boundary set was measured against
   *     (2026-10-03): the name on the line UNDER the icon in a table cell
   *     (Kumamoto R57), and one name split over two lines (Yodogawa). A <br>
   *     boundary dropped the first and halved the second; preferring the
   *     preceding text gave each Kumamoto camera its left neighbour's name. */
  fx_reset();
  fx_add("/h?q=", 200,
    "<html><table><tr>"
    "<td><a href=\"/rec/k1\"><img src=\"1.jpg\" /></a><br />\n Udo Nagahama</td>"
    "<td>&nbsp;</td>"
    "<td><a href=\"/rec/k2\"><img src=\"2.jpg\" /></a><br />\n Ichinokawa</td>"
    "</tr></table><ul>"
    "<li><a href=\"/rec/y1\"><img src=\"3.jpg\" alt=\"\"></a>"
    " <span class=\"ttl\">18.2k<br>Arashiyama</span></li>"
    "</ul></html>");
  rc = run_source("T_HTML", "x");
  ok(rc == 0 && g_ncap == 3 && !strncmp(g_cap[0].title, "Udo Nagahama", 12) &&
     !strncmp(g_cap[1].title, "Ichinokawa", 10),
     "34b: a name under the icon (<br>) labels its own camera, not the next or previous one");
  ok(g_ncap == 3 && strstr(g_cap[2].title, "18.2k") && strstr(g_cap[2].title, "Arashiyama"),
     "34b: a two-line caption is read whole (a <br> is not a boundary)");
  /* 34c. Division layouts: a card whose icon and title sit in sibling divs
   *     (niigata-cci.or.jp, mbsd.jp) keeps the title that follows; a name in
   *     the div BEFORE the icon's div is found; and a caption-before-icon list
   *     written with divs is not labelled one item off. */
  fx_reset();
  fx_add("/h?q=", 200,
    "<html><ul>"
    "<li><div class=\"img\"><a href=\"/rec/d1\"><img src=\"1.png\" alt=\"\"></a></div>"
    "<div class=\"txt\"><h3>Card Title One</h3><p class=\"gaiyo\">summary text</p></div></li>"
    "<li><div class=\"name\">Kamo Bridge</div><div class=\"icon\"><a href=\"/rec/d2\"><img src=\"2.png\"></a></div></li>"
    "</ul>"
    "<div>Report 2026-01 <a href=\"/rec/d3\"><img src=\"p.gif\"></a></div>"
    "<div>Report 2026-02 <a href=\"/rec/d4\"><img src=\"p.gif\"></a></div>"
    "</html>");
  rc = run_source("T_HTML", "x");
  ok(rc == 0 && g_ncap == 4 && !strcmp(g_cap[0].title, "Card Title One"),
     "34c: a card's title in the sibling div labels its icon (and stops at the summary <p>)");
  ok(g_ncap == 4 && !strcmp(g_cap[1].title, "Kamo Bridge"),
     "34c: a name in the div before the icon's div labels it");
  ok(g_ncap == 4 && !strcmp(g_cap[2].title, "Report 2026-01") &&
     !strcmp(g_cap[3].title, "Report 2026-02"),
     "34c: a caption-before-icon list in divs labels each item with its own caption");
  /* 34d. A spacer `&nbsp;` between the icon and its caption is layout, not
   *     the caption (jsite.mhlw.go.jp/tokyo-roudoukyoku): counting it as text
   *     stopped the scan at the next <div> with a label of "&nbsp; &nbsp;". */
  fx_reset();
  fx_add("/h?q=", 200,
    "<html><a href=\"/rec/n1\"><img src=\"1.jpg\"></a><br />\n &nbsp;\n"
    "<h4>&nbsp;</h4><div><h4>Spacer Caption</h4></div></html>");
  rc = run_source("T_HTML", "x");
  ok(rc == 0 && g_ncap == 1 && strstr(g_cap[0].title, "Spacer Caption") != NULL,
     "34d: a no-break-space spacer does not end the label before the caption");

  /* 35. XML rows spend the detail budget per record, as JSON rows do. */
  fx_reset();
  fx_add("/xdd/", 200, "{\"role\":\"member\"}");
  fx_add("/xd", 200,
    "<r><item><ref>A1</ref><name>a</name></item><item><ref>B2</ref><name>b</name></item>"
    "<item><ref>C3</ref><name>c</name></item></r>");
  rc = run_source("T_XML_DEEP", "");
  {
    int pend = 0;
    for (int i = 0; i < g_ncap; i++) if (strstr(g_cap[i].props, "_detail_pending")) pend++;
    ok(rc == 0 && g_ncap == 3 && g_ncalls == 2 && pend == 2,
       "35: detail_max=1 on an XML row makes ONE detail request; the rest are marked pending");
  }

  /* Two transactions of the SAME activity, differing only in later parts of the
   * composite, and with two dimensions absent on each. They must not collapse:
   * `aid` is shared, so anything that keys on the first present part alone
   * stores one row and reports two. */
  fx_reset();
  fx_add("/wide", 200,
    "{\"rows\":[{\"title\":\"t1\",\"aid\":\"XM-DAC-1\",\"trans_id\":\"a\","
    "\"trans_day\":\"2026-01-04\",\"trans_value\":100,\"trans_code\":\"D\","
    "\"trans_country\":\"GH\"},"
    "{\"title\":\"t2\",\"aid\":\"XM-DAC-1\",\"trans_id\":\"b\","
    "\"trans_day\":\"2026-02-09\",\"trans_value\":250,\"trans_code\":\"D\","
    "\"trans_country\":\"GH\"}]}");
  rc = run_source("T_IDKEYS_WIDE", "");
  ok(rc == 0 && g_ncap == 2, "wide composite emits both transactions of one activity");
  ok(strcmp(g_cap[0].key, g_cap[1].key) != 0,
     "a shared `aid` does not collapse them — later composite parts separate the rows");
  ok(strstr(g_cap[0].key, "XM-DAC-1") != NULL && strstr(g_cap[0].key, "2026-01-04") != NULL,
     "absent parts do not truncate the key: parts after the holes still reach it");

  /* `array_path = "."` is the document root. Two packages at the root, one of
   * them carrying three releases, so the nested array (3) outnumbers the root
   * (2) and discovery prefers it. That is the hex.pm shape: on a 60-page walk
   * the engine mined `[33].releases` on 34 pages and emitted 9,643 records
   * where the pages held 6,000 (measured 2026-10-04). Both halves are pinned
   * — the hijack, so the reason the "." exists stays legible, and the fix. */
  fx_reset();
  fx_add("/rootarr", 200,
    "[{\"name\":\"alpha\",\"releases\":[{\"version\":\"1\"},{\"version\":\"2\"},"
    "{\"version\":\"3\"}]},"
    "{\"name\":\"beta\",\"releases\":[{\"version\":\"9\"}]}]");
  rc = run_source("T_ROOTARR_AUTO", "");
  int rootarr_recs = 0, rootarr_named = 0, rootarr_notice = 0;
  for (int i = 0; i < g_ncap; i++) {
    if (!strcmp(g_cap[i].rtype, "t-root")) {
      rootarr_recs++;
      if (strstr(g_cap[i].key, "alpha") || strstr(g_cap[i].key, "beta"))
        rootarr_named++;
    } else if (strstr(g_cap[i].key, "shape:densest-array-fallback")) {
      rootarr_notice++;
    }
  }
  ok(rc == 0 && rootarr_recs == 3 && rootarr_named == 0,
     "without array_path, discovery mines the longer NESTED array (3 releases, not 2 packages)");
  ok(rootarr_notice == 1,
     "and says so: the densest-array fallback is stamped as a shape notice, not silent");
  fx_reset();
  fx_add("/rootarr", 200,
    "[{\"name\":\"alpha\",\"releases\":[{\"version\":\"1\"},{\"version\":\"2\"},"
    "{\"version\":\"3\"}]},"
    "{\"name\":\"beta\",\"releases\":[{\"version\":\"9\"}]}]");
  rc = run_source("T_ROOTARR_DOT", "");
  ok(rc == 0 && g_ncap == 2,
     "array_path=\".\" takes the ROOT array — one record per package");
  ok(g_ncap == 2 && strstr(g_cap[0].key, "alpha") != NULL &&
     strstr(g_cap[1].key, "beta") != NULL,
     "and the root records key on their own fields, so id_keys resolves");

  /* NDJSON: three record lines, one blank line, one line that is not JSON and
   * one bare scalar. The three records must emit, the blank must not count at
   * all, and the unreadable line must be DISCLOSED rather than skipped —
   * silently dropping it is what would make a half-broken feed look whole. */
  fx_reset();
  fx_add("/ndjson", 200,
    "{\"Path\":\"golang.org/x/text\",\"Version\":\"v0.3.0\"}\n"
    "\n"
    "{\"Path\":\"golang.org/x/text\",\"Version\":\"v0.4.0\"}\n"
    "{\"Path\":\"github.com/a/b\",\"Version\":\"v1.0.0\"} oops not json\n"
    "42\n"
    "{\"Path\":\"github.com/a/b\",\"Version\":\"v1.0.0\"}\n");
  rc = run_source("T_NDJSON", "");
  int nd_recs = 0, nd_note = 0;
  for (int i = 0; i < g_ncap; i++) {
    if (!strcmp(g_cap[i].rtype, "t-nd")) nd_recs++;
    else if (strstr(g_cap[i].key, "truncation:")) nd_note++;
  }
  ok(rc == 0 && nd_recs == 3,
     "ndjson emits one record per JSON line (a blank line is not a record)");
  ok(g_ncap >= 3 && strstr(g_cap[0].key, "golang.org/x/text") != NULL &&
     strstr(g_cap[0].key, "v0.3.0") != NULL,
     "and the composite id_keys resolves against the per-line objects");
  ok(nd_recs == 3 && strcmp(g_cap[0].key, g_cap[1].key) != 0,
     "two versions of one module do not collapse onto each other");
  ok(nd_note == 1,
     "an unreadable line is disclosed as a truncation notice, not skipped");

  /* `$last` cursor. Page 1 ends at ts2; the engine must ask for since=ts2 and
   * then stop when the page it gets back holds nothing new. */
  fx_reset();
  /* Fixtures match by strstr in insertion order, so the specific cursors must
   * be registered BEFORE the bare path — "/ndpage" is a substring of every
   * one of these URLs and would otherwise answer all three pages with page 1,
   * which is what the engine's own repeated-page detector then reports. */
  fx_add("/ndpage?since=ts3", 200, "\n");
  fx_add("/ndpage?since=ts2", 200,
    "{\"Path\":\"m/three\",\"Version\":\"v1\",\"Timestamp\":\"ts3\"}\n");
  fx_add("/ndpage", 200,
    "{\"Path\":\"m/one\",\"Version\":\"v1\",\"Timestamp\":\"ts1\"}\n"
    "{\"Path\":\"m/two\",\"Version\":\"v1\",\"Timestamp\":\"ts2\"}\n");
  rc = run_source("T_NDJSON_LASTCURSOR", "");
  int ndp = 0;
  for (int i = 0; i < g_ncap; i++) if (!strcmp(g_cap[i].rtype, "t-nd")) ndp++;
  ok(rc == 0 && ndp == 3,
     "$last.Timestamp walks the feed: page 1's last record supplies page 2's cursor");

  /* {ago:N}. The URL must carry a real instant, not the literal token — an
   * unknown token is left verbatim by design, so "left verbatim" is exactly
   * the failure this pins. */
  fx_reset();
  fx_add("/ago?since=20", 200, "{\"Path\":\"m/ago\"}\n");
  rc = run_source("T_AGO", "");
  ok(rc == 0 && strstr(g_last_url, "{ago:") == NULL,
     "{ago:N} is expanded, not passed through as a literal token");
  ok(strstr(g_last_url, "since=20") != NULL && strstr(g_last_url, "Z") != NULL,
     "and expands to an RFC 3339 UTC instant");

  /* 36. hp_xml_flatten drops nothing without saying so.
   *
   *     It returned at depth 4 and at 400 fields with no stamp, dropped any
   *     value of 4 KB or more, dropped every REPEATED child (its key already
   *     existed — the second <dc:subject>, the second <author> and all its
   *     fields), and dropped an element's own text when it also had children.
   *     Self-closing records were skipped uncounted, and a text-only record
   *     flattened to nothing. */
  {
    /* (a) depth 7 is kept whole, and the record is not stamped. */
    fx_reset();
    fx_add("/xf.xml", 200,
      "<list><rec><id>D7</id><title>deep</title>"
      "<a><b><c><d><e><f><g>leaf</g></f></e></d></c></b></a></rec></list>");
    rc = run_source("T_XML_FLAT", "");
    ok(rc == 0 && g_ncap == 1 && strstr(g_full[0], "\"a.b.c.d.e.f.g\":\"leaf\"") &&
       !strstr(g_full[0], "_fields_dropped"),
       "36a: a field seven elements deep is flattened, not dropped");

    /* (b) past the recursion guard: stamped, with the count of elements kept out. */
    {
      char *x = malloc(4096);
      size_t w = 0;
      w += (size_t)snprintf(x + w, 4096 - w, "<list><rec><id>D40</id><title>deeper</title>");
      for (int i = 0; i < 40; i++) w += (size_t)snprintf(x + w, 4096 - w, "<n%d>", i);
      w += (size_t)snprintf(x + w, 4096 - w, "v");
      for (int i = 39; i >= 0; i--) w += (size_t)snprintf(x + w, 4096 - w, "</n%d>", i);
      snprintf(x + w, 4096 - w, "</rec></list>");
      fx_reset();
      fx_add("/xf.xml", 200, x);
      rc = run_source("T_XML_FLAT", "");
      /* depth 0 is n0, so n0..n32 are walked and n33..n39 (7 elements) are not */
      ok(rc == 0 && g_ncap == 1 && strstr(g_full[0], "\"_fields_dropped\":7"),
         "36b: past the recursion guard the record is stamped with what was kept out");
      free(x);
    }

    /* (c) 500 fields are all kept; 2,100 keep 2,048 and stamp the other 52. */
    for (int pass = 0; pass < 2; pass++) {
      int nf = pass ? 2098 : 498;            /* + id + title */
      size_t cap = (size_t)nf * 40 + 256;
      char *x = malloc(cap);
      size_t w = 0;
      w += (size_t)snprintf(x + w, cap - w, "<list><rec><id>W</id><title>wide</title>");
      for (int i = 0; i < nf; i++) w += (size_t)snprintf(x + w, cap - w, "<f%d>v%d</f%d>", i, i, i);
      /* The old bound was tested on ENTRY to each nested element, so a flat
       * run of leaves passed it and the first CONTAINER after field 400 —
       * with everything in it — was what vanished. */
      if (!pass) w += (size_t)snprintf(x + w, cap - w, "<z><y>tail</y></z>");
      snprintf(x + w, cap - w, "</rec></list>");
      fx_reset();
      fx_add("/xf.xml", 200, x);
      rc = run_source("T_XML_FLAT", "");
      if (!pass)
        ok(rc == 0 && g_ncap == 1 && strstr(g_full[0], "\"f497\":\"v497\"") &&
           strstr(g_full[0], "\"z.y\":\"tail\"") && !strstr(g_full[0], "_fields_dropped"),
           "36c: 500 fields and a container after them are all flattened (the old bound was 400, unstamped)");
      else
        ok(rc == 0 && g_ncap == 1 && strstr(g_full[0], "\"f2045\":\"v2045\"") &&
           !strstr(g_full[0], "\"f2046\"") && strstr(g_full[0], "\"_fields_dropped\":52"),
           "36c: past HP_MAX_PROPS the record keeps 2,048 fields and stamps the 52 it did not");
      free(x);
    }

    /* (d) repeats are indexed, not dropped; the first keeps the plain key. */
    fx_reset();
    fx_add("/xf.xml", 200,
      "<list><rec><id>R1</id><title>repeats</title>"
      "<subject>alpha</subject><subject>beta</subject><subject>gamma</subject>"
      "<author id=\"a1\"><name>Xu</name></author>"
      "<author id=\"a2\"><name>Yamada</name><aff>Kyoto</aff></author></rec></list>");
    rc = run_source("T_XML_FLAT", "");
    ok(rc == 0 && g_ncap == 1 &&
       strstr(g_full[0], "\"subject\":\"alpha\"") && strstr(g_full[0], "\"subject.1\":\"beta\"") &&
       strstr(g_full[0], "\"subject.2\":\"gamma\"") &&
       strstr(g_full[0], "\"author.name\":\"Xu\"") && strstr(g_full[0], "\"author.@id\":\"a1\"") &&
       strstr(g_full[0], "\"author.1.name\":\"Yamada\"") &&
       strstr(g_full[0], "\"author.1.aff\":\"Kyoto\"") && strstr(g_full[0], "\"author.1.@id\":\"a2\"") &&
       !strstr(g_full[0], "_fields_dropped"),
       "36d: a repeated child is indexed (name.1, name.2) and none of its fields is lost");

    /* (e) a value of 4 KB or more is kept whole. */
    {
      char *x = malloc(6000);
      size_t w = (size_t)snprintf(x, 6000, "<list><rec><id>L1</id><title>long</title><abstract>");
      for (int i = 0; i < 5000; i++) x[w++] = (char)('a' + i % 26);
      snprintf(x + w, 6000 - w, "</abstract></rec></list>");
      fx_reset();
      fx_add("/xf.xml", 200, x);
      rc = run_source("T_XML_FLAT", "");
      const char *ab = strstr(g_full[0] ? g_full[0] : "", "\"abstract\":\"");
      const char *q = ab ? strchr(ab + 12, '"') : NULL;
      ok(rc == 0 && g_ncap == 1 && ab && q && q - (ab + 12) == 5000,
         "36e: a 5,000-byte value is flattened whole (it was dropped at 4,096)");
      free(x);
    }

    /* (f) an element's own text beside its children is kept as <key>.#text. */
    fx_reset();
    fx_add("/xf.xml", 200,
      "<list><rec><id>M1</id><title>mixed</title>"
      "<desc>Hello <b>world</b> again</desc></rec></list>");
    rc = run_source("T_XML_FLAT", "");
    ok(rc == 0 && g_ncap == 1 && strstr(g_full[0], "\"desc.#text\":\"Hello again\"") &&
       strstr(g_full[0], "\"desc.b\":\"world\""),
       "36f: mixed content keeps the element's own text and its child's");

    /* (g) a self-closing record is a record when it carries attributes, an
     *     empty slot (counted) when it carries none. */
    fx_reset();
    fx_add("/rows.xml", 200,
      "<rows><row id=\"1\" name=\"one\"/><row id=\"2\" name=\"two\"/><row/></rows>");
    rc = run_source("T_XML_ROWS", "");
    ok(rc == 0 && cap_count("t-xmlrow", NULL) == 2 &&
       !strcmp(g_cap[0].title, "one") && !strcmp(g_cap[1].title, "two"),
       "36g: attribute-only self-closing records are emitted, not skipped");

    /* (h) a record element that holds only text is a record. */
    fx_reset();
    fx_add("/str.xml", 200,
      "<ArrayOfString><string>alpha</string><string>beta</string></ArrayOfString>");
    rc = run_source("T_XML_STR", "");
    ok(rc == 0 && cap_count("t-xmlstr", NULL) == 2 &&
       strstr(g_full[0], "\"#text\":\"alpha\"") && strstr(g_full[1], "\"#text\":\"beta\""),
       "36h: a text-only record element is emitted with its text as #text");
  }

  printf(g_fail ? "\n%d FAILURES\n" : "\nall passed\n", g_fail);
  return g_fail ? 1 : 0;
}
