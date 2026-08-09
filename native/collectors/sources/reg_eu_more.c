/* collectors/sources/reg_eu_more.c
 * OSINT services — seven more European business registries, on-demand entity
 * pivot (ctx->entity = company name or, where noted, a national registration
 * number). One run() dispatches on ctx->source_id:
 *
 *   NL_KVK        Netherlands — KVK (Kamer van Koophandel) Search API. The
 *                 official API needs a (free) subscription key sent as
 *                 apikey: <KVK_API_KEY>; gated when unset. LIVE JSON otherwise.
 *   HR_SUDREG     Croatia — sudreg-data OPEN REST API (sudreg-data.gov.hr /
 *                 sudreg.pravosudje.hr). Keyless, LIVE JSON.
 *   PT_RCBE       Portugal — Registo Central do Beneficiário Efetivo (rcbe.
 *                 justica.gov.pt). No open machine API; scrape the search
 *                 landing for real anchors, else honest empty.
 *   HU_COMPANIES  Hungary — e-cégjegyzék (e-cegjegyzek.hu). No open JSON API;
 *                 scrape the public search page anchors, else honest empty.
 *   RS_APR        Serbia — APR (Agencija za privredne registre, pretraga2.
 *                 apr.gov.rs). No open API; scrape anchors, else honest empty.
 *   LT_REGISTRAI  Lithuania — Registrų centras (registrucentras.lt / rekvizitai
 *                 open listing). Scrape anchors, else honest empty.
 *   LU_LBR        Luxembourg — LBR / Registre de Commerce et des Sociétés
 *                 (lbr.lu). No open API; scrape anchors, else honest empty.
 *
 * HONESTY: every branch REAL-fetches (jo_get / http_request / jo_emit_anchors)
 * and emits only parsed real data, or honest empty (return 0), or gated
 * (missing key → log + return 0). Nothing is fabricated. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- generic single-record emit for the JSON registries ---------------- */
static int em_emit(intel_sink *sink, const char *service, const char *country,
                   const char *rectype, const char *lang,
                   const char *reg_no, const char *name, const char *addr,
                   const char *status, const char *link) {
  if (!name && !reg_no) return 0;

  cJSON *data = cJSON_CreateObject();
  if (name)   cJSON_AddStringToObject(data, "name", name);
  if (reg_no) cJSON_AddStringToObject(data, "registration_number", reg_no);
  if (addr)   cJSON_AddStringToObject(data, "address", addr);
  if (status) cJSON_AddStringToObject(data, "status", status);
  cJSON_AddStringToObject(data, "country", country);
  cJSON_AddStringToObject(data, "source", service);
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", service);
  cJSON_AddStringToObject(props, "source", service);
  cJSON_AddStringToObject(props, "country", country);
  if (reg_no) cJSON_AddStringToObject(props, "registration_number", reg_no);
  if (status) cJSON_AddStringToObject(props, "status", status);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = reg_no ? reg_no : name;
  it.title           = name ? name : reg_no;
  it.summary         = addr ? addr : status;
  it.body            = bj;
  it.lang            = lang;
  it.link            = link;
  it.record_type     = rectype;
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* ---- NL — KVK Search API (gated on KVK_API_KEY) ------------------------- *
 * GET /api/v2/zoeken?naam=<q>  (or ?kvkNummer=<8digits>) with header
 * apikey: <key>. Response: { resultaten: [ { kvkNummer, naam, adres:{...} } ] } */
static int nl_kvk(const source_ctx *ctx, intel_sink *sink, const char *q) {
  const char *key = jo_env("KVK_API_KEY");
  if (!key) { fprintf(stderr, "[NL_KVK] gated (no KVK_API_KEY)\n"); return 0; }

  int alldig = 1, len = 0;
  for (const char *p = q; *p; p++, len++) if (!isdigit((unsigned char)*p)) alldig = 0;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  if (alldig && len == 8)
    snprintf(url, sizeof url,
      "https://api.kvk.nl/api/v2/zoeken?kvkNummer=%s", q);
  else
    snprintf(url, sizeof url,
      "https://api.kvk.nl/api/v2/zoeken?naam=%s", enc);
  free(enc);

  char hdr[256];
  snprintf(hdr, sizeof hdr, "apikey: %s", key);
  const char *hdrs[] = { hdr, "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "NL_KVK");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int emitted = 0;
  cJSON *arr = cJSON_GetObjectItem(root, "resultaten");
  if (cJSON_IsArray(arr)) {
    cJSON *r;
    cJSON_ArrayForEach(r, arr) {
      const char *kvk  = jo_sv(r, "kvkNummer");
      const char *naam = jo_sv(r, "naam");
      const char *adr  = NULL;
      cJSON *ao = cJSON_GetObjectItem(r, "adres");
      if (cJSON_IsObject(ao)) {
        cJSON *ba = cJSON_GetObjectItem(ao, "binnenlandsAdres");
        if (cJSON_IsObject(ba)) adr = jo_sv(ba, "plaats");
      }
      char link[256] = {0};
      if (kvk) snprintf(link, sizeof link,
        "https://www.kvk.nl/zoeken/?source=all&q=%s", kvk);
      emitted += em_emit(sink, "NL_KVK", "NL", "kvk-company", "nl",
                         kvk, naam, adr, NULL, kvk ? link : NULL);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[NL_KVK] emitted %d\n", emitted);
  return emitted;
}

/* ---- HR — Croatia sudreg OPEN REST API --------------------------------- *
 * The sudreg-data open API exposes /javni/subjekt?tvrtka_naziv=<q> style
 * lookups. Field names vary across versions, so we tolerate several: an array
 * (or an object with a data/subjekti array) of subjects with OIB / MBS /
 * name / seat. Keyless, LIVE. */
/* tiny case-insensitive substring (needle short) */
static const char *sw_ci(const char *h, const char *n) {
  if (!h || !n || !*n) return h;
  size_t nl = strlen(n);
  for (; *h; h++)
    if (tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
      size_t k = 1;
      while (k < nl && h[k] && tolower((unsigned char)h[k]) == tolower((unsigned char)n[k])) k++;
      if (k == nl) return h;
    }
  return NULL;
}
static int hr_walk(intel_sink *sink, const cJSON *arr, const char *q) {
  static const char *KN[] = { "tvrtka_naziv", "ime", "naziv", "tvrtka", NULL };
  static const char *KO[] = { "oib", "OIB", NULL };
  static const char *KM[] = { "mbs", "MBS", "maticni_broj", NULL };
  static const char *KA[] = { "sjediste", "adresa", "mjesto", NULL };
  int emitted = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    if (!cJSON_IsObject(r)) continue;
    const char *name = jo_first_sv(r, KN);
    const char *oib  = jo_first_sv(r, KO);
    const char *mbs  = jo_first_sv(r, KM);
    const char *addr = jo_first_sv(r, KA);
    /* honest filter: keep only records whose name really matches the query */
    if (name && q && *q && !sw_ci(name, q)) continue;
    const char *regno = mbs ? mbs : oib;
    char link[256] = {0};
    if (oib) snprintf(link, sizeof link, "https://sudreg-data.gov.hr/api/javni/subjekt?oib=%s", oib);
    emitted += em_emit(sink, "HR_SUDREG", "HR", "sudreg-subject", "hr",
                       regno, name, addr, NULL, oib ? link : NULL);
  }
  return emitted;
}
static int hr_sudreg(const source_ctx *ctx, intel_sink *sink, const char *q) {
  int alldig = 1, len = 0;
  for (const char *p = q; *p; p++, len++) if (!isdigit((unsigned char)*p)) alldig = 0;
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  if (alldig && len == 11)   /* OIB is 11 digits */
    snprintf(url, sizeof url,
      "https://sudreg-data.gov.hr/api/javni/subjekt?oib=%s", q);
  else
    snprintf(url, sizeof url,
      "https://sudreg-data.gov.hr/api/javni/subjekt?tvrtka_naziv=%s", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json", "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "HR_SUDREG");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  int emitted = 0;
  if (cJSON_IsArray(root)) emitted += hr_walk(sink, root, q);
  else if (cJSON_IsObject(root)) {
    cJSON *arr = cJSON_GetObjectItem(root, "data");
    if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(root, "subjekti");
    if (cJSON_IsArray(arr)) emitted += hr_walk(sink, arr, q);
    else {   /* tolerate a single subject object at the top level */
      static const char *KN[] = { "tvrtka_naziv", "ime", "naziv", "tvrtka", NULL };
      static const char *KO[] = { "oib", "OIB", NULL };
      emitted += em_emit(sink, "HR_SUDREG", "HR", "sudreg-subject", "hr",
                         jo_first_sv(root, KO), jo_first_sv(root, KN),
                         NULL, NULL, NULL);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[HR_SUDREG] emitted %d\n", emitted);
  return emitted;
}

/* ---- scrape-only registries (no open API) ------------------------------ *
 * We fetch the real public search results page and emit one item per genuine
 * anchor whose text/href matches the query. JS-rendered pages yield 0
 * (honest empty) rather than anything invented. */
static int scrape_reg(const source_ctx *ctx, intel_sink *sink, const char *q,
                      const char *tag, const char *service,
                      const char *rectype, const char *base,
                      const char *url_fmt, const char *href_must) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url, url_fmt, enc);
  free(enc);
  int n = jo_emit_anchors(ctx, sink, url, href_must, service, rectype,
                          base, q, 25, tag);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  if (strlen(q) < 2) { fprintf(stderr, "[reg_eu_more] entity too short\n"); return 0; }
  const char *id = ctx->source_id;

  if (strcmp(id, "NL_KVK") == 0)       { nl_kvk(ctx, sink, q); return 0; }
  if (strcmp(id, "HR_SUDREG") == 0)    { hr_sudreg(ctx, sink, q); return 0; }

  if (strcmp(id, "PT_RCBE") == 0) {
    scrape_reg(ctx, sink, q, "PT_RCBE", "PT_RCBE", "rcbe-entity",
      "https://rcbe.justica.gov.pt",
      "https://rcbe.justica.gov.pt/Home/Pesquisar?termo=%s", NULL);
    return 0;
  }
  if (strcmp(id, "HU_COMPANIES") == 0) {
    scrape_reg(ctx, sink, q, "HU_COMPANIES", "HU_COMPANIES", "hu-company",
      "https://www.e-cegjegyzek.hu",
      "https://www.e-cegjegyzek.hu/?cegnev=%s", NULL);
    return 0;
  }
  if (strcmp(id, "RS_APR") == 0) {
    scrape_reg(ctx, sink, q, "RS_APR", "RS_APR", "apr-entity",
      "https://pretraga2.apr.gov.rs",
      "https://pretraga2.apr.gov.rs/unifiedentitysearch?searchTerm=%s", NULL);
    return 0;
  }
  if (strcmp(id, "LT_REGISTRAI") == 0) {
    scrape_reg(ctx, sink, q, "LT_REGISTRAI", "LT_REGISTRAI", "lt-company",
      "https://rekvizitai.vz.lt",
      "https://rekvizitai.vz.lt/en/search/?q=%s", NULL);
    return 0;
  }
  if (strcmp(id, "LU_LBR") == 0) {
    scrape_reg(ctx, sink, q, "LU_LBR", "LU_LBR", "lbr-entity",
      "https://www.lbr.lu",
      "https://www.lbr.lu/mjrcs/jsp/DisplayConsultDocumentsActionNotSecured.action?fileNumber=&companyName=%s",
      NULL);
    return 0;
  }
  return -1;
}

#define EUM_DEF(SYM, ID, NAME, NAMEJA, TYPE, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", \
    .name = NAME, .name_ja = NAMEJA, .update_interval_sec = 0, .run = run, \
    .category = "government", .type = TYPE, .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

EUM_DEF(nl_kvk_def, "NL_KVK",
  "Netherlands KVK Registry", "オランダ KVK 企業登記", "api",
  "https://www.kvk.nl",
  "Netherlands KVK company registry Search API (needs KVK_API_KEY): name, KvK number, city");
EUM_DEF(pt_rcbe_def, "PT_RCBE",
  "Portugal RCBE Registry", "ポルトガル RCBE 実質的支配者登記", "scraped",
  "https://rcbe.justica.gov.pt",
  "Portugal Central Register of Beneficial Ownership (RCBE) — public search scrape");
EUM_DEF(hu_companies_def, "HU_COMPANIES",
  "Hungary e-cegjegyzek Registry", "ハンガリー e-cegjegyzek 企業登記", "scraped",
  "https://www.e-cegjegyzek.hu",
  "Hungary e-cégjegyzék company registry — public search scrape");
EUM_DEF(hr_sudreg_def, "HR_SUDREG",
  "Croatia sudreg Registry", "クロアチア sudreg 企業登記", "api",
  "https://sudreg-data.gov.hr",
  "Croatia court/company registry (sudreg-data open API, keyless): OIB/MBS, name, seat");
EUM_DEF(rs_apr_def, "RS_APR",
  "Serbia APR Registry", "セルビア APR 企業登記", "scraped",
  "https://pretraga2.apr.gov.rs",
  "Serbia Business Registers Agency (APR) unified entity search — public scrape");
EUM_DEF(lt_registrai_def, "LT_REGISTRAI",
  "Lithuania Registrai", "リトアニア企業登記", "scraped",
  "https://www.registrucentras.lt",
  "Lithuania Registrų centras company data (rekvizitai listing) — public search scrape");
EUM_DEF(lu_lbr_def, "LU_LBR",
  "Luxembourg LBR Registry", "ルクセンブルク LBR 商業登記", "scraped",
  "https://www.lbr.lu",
  "Luxembourg Registre de Commerce et des Sociétés (LBR) — public search scrape");
