/* cert_nvd_cpe.c — NVD CPE Product Dictionary 2.0.
 *
 * Endpoint: https://services.nvd.nist.gov/rest/json/cpes/2.0   (keyless)
 * Shape:    { totalResults, products: [ { cpe: { cpeName, cpeNameId,
 *            deprecated, created, lastModified, titles:[{title,lang}] } } ] }
 *
 * SCOPE: the dictionary is 1,792,918 entries. A run reads one QUERY to the
 * end — every page of it, walked on startIndex against the upstream's own
 * totalResults — never the whole dictionary:
 *   - Scheduled run: the recently-modified window —
 *     ?lastModStartDate=<now-7d>&lastModEndDate=<now>.
 *     If NVD rejects or empties that window, it falls back to the head of the
 *     dictionary (no filter), which is the one query the page ceiling below
 *     is expected to bite on — and says so in a truncation notice.
 *   - OSINT pivot (ctx->entity set): ?keywordSearch=<entity>, which is what
 *     turns a fingerprinted product string into the CPE used to query for its
 *     CVEs.
 *
 * This used to read ONE page of each: measured 2026-10-02, the 7-day window
 * held 4,826 CPEs of which the collector kept 200, and keywordSearch=fortinet
 * held 3,664 of which it kept 100 — silently, with no notice. Pages are now
 * NVD_CPE_PAGE records (the API allows up to 10,000), at most
 * JO_NVD_CPE_PAGE_MAX of them (default 25) per query; when that ceiling or a
 * failed later page stops a walk short of totalResults, the shortfall is
 * emitted as a collector-truncation-notice, scoped to the query.
 *
 * Emits: the localized product title, cpeName, cpeNameId (remote_key),
 * deprecation flag, created/lastModified timestamps, and the part / vendor /
 * product / version fields split out of the fetched cpe:2.3 string (local
 * computation over fetched bytes, not a lookup URL).
 *
 * NVD asks for reasonable request rates: keyless is limited to 5 requests per
 * 30 s, so a walk sleeps 6 s between pages (0.6 s with a key, whose limit is
 * 50 per 30 s). NVD_API_KEY is honoured when present purely to raise that
 * limit — the source works without it (R6).
 *
 * R2: no geometry — a product name has no location.
 * R3: -1 only when both the windowed and the fallback fetch fail.
 *
 * Licence: NIST, public domain. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "_timefmt.inc"
#include "third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *urlenc(const char *s) {
  static const char hx[] = "0123456789ABCDEF";
  if (!s) s = "";
  size_t n = strlen(s);
  char *o = (char *)malloc(n * 3 + 1);
  if (!o) return NULL;
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o[j++] = (char)c;
    else { o[j++] = '%'; o[j++] = hx[c >> 4]; o[j++] = hx[c & 15]; }
  }
  o[j] = 0;
  return o;
}

/* cpe:2.3:<part>:<vendor>:<product>:<version>:… → field #idx (1-based after
 * the "cpe" prefix). Returns 1 and fills out on success. */
static int cpe_field(const char *cpe, int idx, char *out, size_t n) {
  out[0] = 0;
  if (!cpe) return 0;
  const char *p = cpe;
  int seen = 0;
  while (*p) {
    if (*p == ':') {
      seen++;
      if (seen == idx) {
        const char *q = p + 1;
        size_t j = 0;
        while (*q && *q != ':' && j + 1 < n) out[j++] = *q++;
        out[j] = 0;
        return out[0] && strcmp(out, "*") && strcmp(out, "-");
      }
    }
    p++;
  }
  return 0;
}

static int emit_cpe(intel_sink *s, cJSON *prod) {
  cJSON *cpe = cJSON_GetObjectItem(prod, "cpe");
  if (!cpe) cpe = prod;                    /* tolerate a flattened shape */
  const char *cpe_name = jo_sv(cpe, "cpeName");
  const char *cpe_id   = jo_sv(cpe, "cpeNameId");
  if (!cpe_name) return 0;                 /* no fetched CPE -> no row (R1) */

  /* titles[] — prefer lang "en", else the first one that is really there */
  const char *label = NULL;
  cJSON *titles = cJSON_GetObjectItem(cpe, "titles");
  if (cJSON_IsArray(titles)) {
    cJSON *t;
    cJSON_ArrayForEach(t, titles) {
      const char *tv = jo_sv(t, "title");
      if (!tv) continue;
      const char *lg = jo_sv(t, "lang");
      if (!label) label = tv;
      if (lg && !strncasecmp(lg, "en", 2)) { label = tv; break; }
    }
  }

  char part[16], vendor[128], product[192], version[96];
  cpe_field(cpe_name, 2, part,    sizeof part);
  cpe_field(cpe_name, 3, vendor,  sizeof vendor);
  cpe_field(cpe_name, 4, product, sizeof product);
  cpe_field(cpe_name, 5, version, sizeof version);

  char title[512];
  if (label && version[0]) snprintf(title, sizeof title, "%s %s", label, version);
  else if (label)          snprintf(title, sizeof title, "%s", label);
  else                     snprintf(title, sizeof title, "%s", cpe_name);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "cpe_name", cpe_name);
  if (cpe_id)     cJSON_AddStringToObject(p, "cpe_name_id", cpe_id);
  if (part[0])    cJSON_AddStringToObject(p, "part", part);
  if (vendor[0])  cJSON_AddStringToObject(p, "vendor", vendor);
  if (product[0]) cJSON_AddStringToObject(p, "product", product);
  if (version[0]) cJSON_AddStringToObject(p, "version", version);
  const char *v;
  if ((v = jo_sv(cpe, "created")))      cJSON_AddStringToObject(p, "created", v);
  if ((v = jo_sv(cpe, "lastModified"))) cJSON_AddStringToObject(p, "last_modified", v);
  cJSON *dep = cJSON_GetObjectItem(cpe, "deprecated");
  if (dep && cJSON_IsBool(dep))
    cJSON_AddBoolToObject(p, "deprecated", cJSON_IsTrue(dep));
  if (cJSON_IsArray(titles))
    cJSON_AddItemToObject(p, "titles", cJSON_Duplicate(titles, 1));
  cJSON_AddStringToObject(p, "source", "nvd_cpe_dictionary");
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  intel_item it = {0};
  it.remote_key      = cpe_id ? cpe_id : cpe_name;
  it.title           = title;
  it.body            = cpe_name;
  it.summary         = label;
  it.lang            = "en";
  it.published_at    = jo_sv(cpe, "created");
  it.record_type     = "cpe-product";
  it.properties_json = pj ? pj : "{}";
  it.tags_json       = "[\"cyber\",\"cpe\",\"nvd\",\"product-dictionary\"]";
  int rc = s->emit(s, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Emit every product on one page. `*seen` is what the page HELD, which is
 * what advances startIndex — a product without a cpeName is not emitted (R1)
 * but it still occupied its slot in the upstream's numbering. */
static int walk(cJSON *doc, intel_sink *s, int *seen) {
  cJSON *prods = cJSON_GetObjectItem(doc, "products");
  int n = 0;
  *seen = 0;
  if (cJSON_IsArray(prods)) {
    *seen = cJSON_GetArraySize(prods);
    cJSON *e;
    cJSON_ArrayForEach(e, prods) n += emit_cpe(s, e);
  }
  return n;
}

#define NVD_CPE_PAGE 2000

static int nvd_page_max(void) {
  const char *e = getenv("JO_NVD_CPE_PAGE_MAX");
  int v = e ? atoi(e) : 0;
  return v > 0 ? v : 25;
}

static void nvd_pause(int keyed) {
  struct timespec ts = keyed ? (struct timespec){ 0, 600000000L }
                             : (struct timespec){ 6, 0 };
  nanosleep(&ts, NULL);
}

/* Walk one query to the end of its totalResults. `base` carries the filter
 * and no paging; resultsPerPage/startIndex are appended here. Returns records
 * emitted, or -1 when the FIRST page could not be fetched (R3). */
static int nvd_walk(const source_ctx *c, intel_sink *s, const char *const *hdrs,
                    int keyed, const char *base, const char *scope) {
  long start = 0, total = -1;
  int emitted = 0, pages = 0, stopped = 0;
  const int page_max = nvd_page_max();
  char url[640];
  for (;;) {
    snprintf(url, sizeof url, "%s%sresultsPerPage=%d&startIndex=%ld", base,
             strchr(base, '?') ? "&" : "?", NVD_CPE_PAGE, start);
    if (pages > 0) nvd_pause(keyed);
    cJSON *doc = feed_get_json_h(c->http, url, hdrs, 60000);
    if (!doc) {
      if (pages == 0) return -1;
      stopped = 1;                          /* a later page failed */
      break;
    }
    pages++;
    const cJSON *tr = cJSON_GetObjectItem(doc, "totalResults");
    if (cJSON_IsNumber(tr)) total = (long)tr->valuedouble;
    int seen = 0;
    emitted += walk(doc, s, &seen);
    cJSON_Delete(doc);
    start += seen;
    if (seen <= 0 || (total >= 0 && start >= total)) break;
    if (pages >= page_max) { stopped = 1; break; }
  }
  if (stopped && (total < 0 || start < total)) {
    char why[200];
    snprintf(why, sizeof why,
             "the walk stopped after %d page(s) of %d at startIndex %ld",
             pages, NVD_CPE_PAGE, start);
    jo_trunc_notice_scoped(s, "nvd-cpe-dictionary", scope, base, emitted, total,
                           why, "raise JO_NVD_CPE_PAGE_MAX (pages per query), "
                           "set NVD_API_KEY for the faster rate, or re-run");
  }
  fprintf(stderr, "[nvd-cpe-dictionary] %s: %d emitted across %d page(s), "
          "upstream totalResults %ld\n", scope, emitted, pages, total);
  return emitted;
}

static int run(const source_ctx *c, intel_sink *s) {
  const char *key = getenv("NVD_API_KEY");
  const char *h_key[] = { "accept: application/json", NULL, NULL };
  char keyhdr[128] = {0};
  int keyed = key && *key;
  if (keyed) {
    snprintf(keyhdr, sizeof keyhdr, "apiKey: %s", key);
    h_key[1] = keyhdr;
  }

  char base[512];
  /* The notice's scope names the query, so one keyword's disclosure does not
   * overwrite another's (the sink upserts a notice on source + scope). */
  char scopebuf[120];
  const char *scope;
  if (c->entity && *c->entity) {
    char *q = urlenc(c->entity);
    if (!q) return -1;
    snprintf(base, sizeof base,
             "https://services.nvd.nist.gov/rest/json/cpes/2.0"
             "?keywordSearch=%s", q);
    free(q);
    snprintf(scopebuf, sizeof scopebuf, "keyword:%s", c->entity);
    scope = scopebuf;
  } else {
    time_t now = time(NULL);
    time_t from = now - 7 * 24 * 3600;
    char a[40], b[40];
    if (!jo_time_fmt(from, "%Y-%m-%dT%H:%M:%S.000Z", a, sizeof a) ||
        !jo_time_fmt(now,  "%Y-%m-%dT%H:%M:%S.000Z", b, sizeof b)) {
      fprintf(stderr, "[nvd-cpe-dictionary] cannot render the query window "
                      "as a date\n");
      return -1;
    }
    snprintf(base, sizeof base,
             "https://services.nvd.nist.gov/rest/json/cpes/2.0"
             "?lastModStartDate=%s&lastModEndDate=%s", a, b);
    scope = "window";
  }

  int n = nvd_walk(c, s, h_key, keyed, base, scope);
  int fetched = n >= 0;
  if (n < 0) n = 0;

  if (n == 0 && (!c->entity || !*c->entity)) {
    /* NVD occasionally rejects a lastMod window (400) and a quiet week is also
     * possible; fall back to the head of the dictionary so a scheduled run
     * still carries fetched rows. That walk is the one the page ceiling bites
     * on, and nvd_walk says so in the data. */
    if (fetched) nvd_pause(keyed);
    int n2 = nvd_walk(c, s, h_key, keyed,
                      "https://services.nvd.nist.gov/rest/json/cpes/2.0", "head");
    if (n2 < 0) {
      if (!fetched) {
        fprintf(stderr, "[nvd-cpe-dictionary] fetch/parse failed\n");
        return -1;                          /* both attempts failed (R3) */
      }
      fprintf(stderr, "[nvd-cpe-dictionary] window empty, fallback failed\n");
      return 0;
    }
    n = n2;
  } else if (!fetched) {
    fprintf(stderr, "[nvd-cpe-dictionary] fetch/parse failed\n");
    return -1;
  }

  fprintf(stderr, "[nvd-cpe-dictionary] emitted %d\n", n);
  return 0;
}

static const source_def cert_nvd_cpe_def = {
  .id = "nvd-cpe-dictionary", .collector = "cyber",
  .name = "NVD CPE Product Dictionary 2.0",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://services.nvd.nist.gov/rest/json/cpes/2.0",
  .description = "NIST's product-naming dictionary with vendor/product/version, "
                 "localized titles and deprecation status - what turns a "
                 "fingerprinted product string into the CPE used to query for "
                 "its CVEs. Recently-modified window, or keyword pivot, each "
                 "walked to the end of its totalResults.",
  .license = "NIST public domain; NVD asks for reasonable request rates.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_nvd_cpe_def)
