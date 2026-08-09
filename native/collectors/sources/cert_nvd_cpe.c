/* cert_nvd_cpe.c — NVD CPE Product Dictionary 2.0.
 *
 * Endpoint: https://services.nvd.nist.gov/rest/json/cpes/2.0   (keyless)
 * Shape:    { totalResults, products: [ { cpe: { cpeName, cpeNameId,
 *            deprecated, created, lastModified, titles:[{title,lang}] } } ] }
 *
 * SCOPE: the dictionary is 1,792,918 entries. This collector never walks it.
 *   - Scheduled run: the recently-modified window only —
 *     ?lastModStartDate=<now-7d>&lastModEndDate=<now>&resultsPerPage=200.
 *     If NVD rejects or empties that window, it falls back to the plain
 *     ?resultsPerPage=200&startIndex=0 head of the dictionary.
 *   - OSINT pivot (ctx->entity set): ?keywordSearch=<entity>&resultsPerPage=100,
 *     which is what turns a fingerprinted product string into the CPE used to
 *     query for its CVEs.
 *
 * Emits: the localized product title, cpeName, cpeNameId (remote_key),
 * deprecation flag, created/lastModified timestamps, and the part / vendor /
 * product / version fields split out of the fetched cpe:2.3 string (local
 * computation over fetched bytes, not a lookup URL).
 *
 * NVD asks for reasonable request rates: keyless is limited to 5 requests per
 * 30 s, and this makes at most two. NVD_API_KEY is honoured when present purely
 * to raise that limit — the source works without it (R6).
 *
 * R2: no geometry — a product name has no location.
 * R3: -1 only when both the windowed and the fallback fetch fail.
 *
 * Licence: NIST, public domain. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
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

static int walk(cJSON *doc, intel_sink *s) {
  cJSON *prods = cJSON_GetObjectItem(doc, "products");
  int n = 0;
  if (cJSON_IsArray(prods)) {
    cJSON *e;
    cJSON_ArrayForEach(e, prods) n += emit_cpe(s, e);
  }
  return n;
}

static int run(const source_ctx *c, intel_sink *s) {
  const char *key = getenv("NVD_API_KEY");
  const char *h_key[] = { "accept: application/json", NULL, NULL };
  char keyhdr[128] = {0};
  if (key && *key) {
    snprintf(keyhdr, sizeof keyhdr, "apiKey: %s", key);
    h_key[1] = keyhdr;
  }

  char url[512];
  if (c->entity && *c->entity) {
    char *q = urlenc(c->entity);
    if (!q) return -1;
    snprintf(url, sizeof url,
             "https://services.nvd.nist.gov/rest/json/cpes/2.0"
             "?keywordSearch=%s&resultsPerPage=100", q);
    free(q);
  } else {
    time_t now = time(NULL);
    time_t from = now - 7 * 24 * 3600;
    struct tm tf, tn;
    gmtime_r(&from, &tf);
    gmtime_r(&now, &tn);
    char a[40], b[40];
    strftime(a, sizeof a, "%Y-%m-%dT%H:%M:%S.000Z", &tf);
    strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%S.000Z", &tn);
    snprintf(url, sizeof url,
             "https://services.nvd.nist.gov/rest/json/cpes/2.0"
             "?lastModStartDate=%s&lastModEndDate=%s&resultsPerPage=200", a, b);
  }

  cJSON *doc = feed_get_json_h(c->http, url, h_key, 30000);
  int fetched = doc != NULL;
  int n = doc ? walk(doc, s) : 0;
  if (doc) { cJSON_Delete(doc); doc = NULL; }

  if (n == 0 && (!c->entity || !*c->entity)) {
    /* NVD occasionally rejects a lastMod window (400) and a quiet week is also
     * possible; fall back to the plain head of the dictionary so a scheduled
     * run still carries fetched rows. */
    cJSON *d2 = feed_get_json_h(c->http,
      "https://services.nvd.nist.gov/rest/json/cpes/2.0"
      "?resultsPerPage=200&startIndex=0", h_key, 30000);
    if (!d2) {
      if (!fetched) {
        fprintf(stderr, "[nvd-cpe-dictionary] fetch/parse failed\n");
        return -1;                          /* both attempts failed (R3) */
      }
      fprintf(stderr, "[nvd-cpe-dictionary] window empty, fallback failed\n");
      return 0;
    }
    n = walk(d2, s);
    cJSON_Delete(d2);
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
                 "its CVEs. Recently-modified window, or keyword pivot.",
  .license = "NIST public domain; NVD asks for reasonable request rates.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_nvd_cpe_def)
