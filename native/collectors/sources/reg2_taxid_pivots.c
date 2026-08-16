/* collectors/sources/reg2_taxid_pivots.c
 *
 * Three national tax-id → company-record pivots. All keyless, all on-demand
 * (update_interval_sec = 0), all emitting only fields the upstream returned.
 * None publish coordinates, so no row claims geo and no registered address is
 * ever geocoded (R2). An unknown id is a clean empty (return 0), not an error.
 *
 *  PL_VAT_WHITELIST — Poland, Ministry of Finance VAT whitelist ("biala lista")
 *    GET https://wl-api.mf.gov.pl/api/search/nip/<10-digit NIP>?date=YYYY-MM-DD
 *    (date must be today or earlier or the API returns 400; today is used.)
 *    Emits result.subject: name, nip, regon, krs, statusVat, workingAddress,
 *    residenceAddress, registrationLegalDate, accountNumbers[] and the
 *    representatives[] / partners[] director & shareholder names when present.
 *    Licence: statutory public register (Ustawa o VAT art. 96b); official MF
 *    API, no key.
 *
 *  RO_ANAF_VAT — Romania, ANAF taxpayer register
 *    POST https://webservicesp.anaf.ro/api/PlatitorTvaRest/v9/tva
 *    body [{"cui":<number>,"data":"YYYY-MM-DD"}]  (up to 100 entries per call)
 *    NOTE the path: the widely-cited /PlatitorTvaRest/api/v8/ws/tva and v9
 *    variants now 404 — only /api/PlatitorTvaRest/v9/tva answers.
 *    Response splits into found[] and notFound[]; only found[] produces rows.
 *    Emits date_generale: denumire, cui, adresa, nrRegCom, telefon,
 *    stare_inregistrare, cod_CAEN, forma_juridica, data_inregistrare,
 *    organFiscalCompetent.
 *    Licence: official ANAF public web service, no key, no stated restriction.
 *
 *  VN_TAXCODE_LOOKUP — Vietnam business tax code (MST)
 *    GET https://api.vietqr.io/v2/business/<MST>
 *    code=="00" means found; unknown tax codes return a non-00 code, not a 404.
 *    Emits data: id (MST), name, internationalName, shortName, address, status,
 *    plus metadata.source and metadata.updatedAt, which are carried into the row
 *    because the data is a redistribution with a ~2-month lag.
 *    Licence: mirror of the General Department of Taxation register; the payload
 *    self-declares source = https://www.gdt.gov.vn.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

static int tp_digits(const char *s, char *out, int cap) {
  if (!s) { out[0] = 0; return 0; }
  int nd = 0;
  for (const char *p = s; *p && nd < cap - 1; p++)
    if (isdigit((unsigned char)*p)) out[nd++] = *p;
  out[nd] = 0;
  return nd;
}

static void tp_today(char *out, size_t n) {
  time_t t = time(NULL);
  struct tm g;
  gmtime_r(&t, &g);
  strftime(out, n, "%Y-%m-%d", &g);
}

/* ------------------------------------------------------ Poland — biala lista */
static int pl_run(const source_ctx *ctx, intel_sink *sink) {
  char nip[16];
  if (tp_digits(ctx->entity, nip, (int)sizeof nip) != 10) return 0;

  char today[16];
  tp_today(today, sizeof today);
  char url[160];
  snprintf(url, sizeof url,
           "https://wl-api.mf.gov.pl/api/search/nip/%s?date=%s", nip, today);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "PL_VAT_WHITELIST");
  if (!body) return 0;
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) return 0;

  const cJSON *res = cJSON_GetObjectItem(doc, "result");
  const cJSON *sub = res ? cJSON_GetObjectItem(res, "subject") : NULL;
  if (!sub || !cJSON_IsObject(sub)) { cJSON_Delete(doc); return 0; }

  const char *name = jo_sv(sub, "name");
  if (!name) { cJSON_Delete(doc); return 0; }
  const char *status = jo_sv(sub, "statusVat");
  const char *krs    = jo_sv(sub, "krs");
  const char *regon  = jo_sv(sub, "regon");
  const char *addr   = jo_sv(sub, "workingAddress");
  if (!addr) addr    = jo_sv(sub, "residenceAddress");
  const char *since  = jo_sv(sub, "registrationLegalDate");

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "PL_VAT_WHITELIST");
  cJSON_AddStringToObject(props, "source", "wl-api.mf.gov.pl");
  cJSON_AddStringToObject(props, "nip", jo_sv(sub, "nip") ? jo_sv(sub, "nip") : nip);
  cJSON_AddStringToObject(props, "name", name);
  if (status) cJSON_AddStringToObject(props, "status_vat", status);
  if (krs)    cJSON_AddStringToObject(props, "krs", krs);
  if (regon)  cJSON_AddStringToObject(props, "regon", regon);
  if (addr)   cJSON_AddStringToObject(props, "address", addr);
  if (since)  cJSON_AddStringToObject(props, "registration_legal_date", since);
  if (jo_sv(sub, "registrationDenialDate"))
    cJSON_AddStringToObject(props, "registration_denial_date",
                            jo_sv(sub, "registrationDenialDate"));
  if (jo_sv(sub, "restorationDate"))
    cJSON_AddStringToObject(props, "restoration_date", jo_sv(sub, "restorationDate"));
  if (jo_sv(sub, "removalDate"))
    cJSON_AddStringToObject(props, "removal_date", jo_sv(sub, "removalDate"));

  const cJSON *acc = cJSON_GetObjectItem(sub, "accountNumbers");
  if (cJSON_IsArray(acc)) {
    cJSON *a = cJSON_CreateArray();
    const cJSON *v;
    cJSON_ArrayForEach(v, acc)
      if (cJSON_IsString(v) && v->valuestring)
        cJSON_AddItemToArray(a, cJSON_CreateString(v->valuestring));
    cJSON_AddItemToObject(props, "account_numbers", a);
  }

  /* representatives[] / partners[] carry director & shareholder names. */
  static const char *people_keys[2] = { "representatives", "partners" };
  static const char *people_out[2]  = { "representatives", "partners" };
  for (int k = 0; k < 2; k++) {
    const cJSON *pl = cJSON_GetObjectItem(sub, people_keys[k]);
    if (!cJSON_IsArray(pl)) continue;
    cJSON *a = cJSON_CreateArray();
    const cJSON *p;
    cJSON_ArrayForEach(p, pl) {
      const char *cn = jo_sv(p, "companyName");
      const char *fn = jo_sv(p, "firstName");
      const char *ln = jo_sv(p, "lastName");
      if (!cn && !fn && !ln) continue;
      cJSON *o = cJSON_CreateObject();
      if (cn) cJSON_AddStringToObject(o, "company_name", cn);
      if (fn) cJSON_AddStringToObject(o, "first_name", fn);
      if (ln) cJSON_AddStringToObject(o, "last_name", ln);
      if (jo_sv(p, "nip")) cJSON_AddStringToObject(o, "nip", jo_sv(p, "nip"));
      cJSON_AddItemToArray(a, o);
    }
    cJSON_AddItemToObject(props, people_out[k], a);
  }
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char summary[460];
  snprintf(summary, sizeof summary, "NIP %s%s%s%s%s%s%s",
           nip,
           status ? " · VAT " : "", status ? status : "",
           krs ? " · KRS " : "", krs ? krs : "",
           regon ? " · REGON " : "", regon ? regon : "");

  intel_item it = {0};
  it.remote_key      = nip;
  it.title           = name;
  it.summary         = summary;
  it.body            = pj;
  it.link            = url;
  it.lang            = "pl";
  it.published_at    = since;
  it.record_type     = "pl-taxpayer";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"registry\",\"poland\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(doc);
  fprintf(stderr, "[PL_VAT_WHITELIST] emitted 1 (%s)\n", nip);
  return 0;
}

/* ------------------------------------------------------- Romania — ANAF TVA */
static int ro_run(const source_ctx *ctx, intel_sink *sink) {
  char cui[16];
  int nd = tp_digits(ctx->entity, cui, (int)sizeof cui);
  if (nd < 2 || nd > 10) return 0;             /* CUI is 2..10 digits */

  char today[16];
  tp_today(today, sizeof today);
  char body[128];
  snprintf(body, sizeof body, "[{\"cui\":%s,\"data\":\"%s\"}]", cui, today);

  const char *hdrs[] = { "Content-Type: application/json",
                         "Accept: application/json", NULL };
  cJSON *doc = feed_post_json(ctx->http,
                              "https://webservicesp.anaf.ro/api/PlatitorTvaRest/v9/tva",
                              body, hdrs, 30000);
  if (!doc) return 0;                          /* upstream said nothing */

  const cJSON *found = cJSON_GetObjectItem(doc, "found");
  if (!cJSON_IsArray(found)) { cJSON_Delete(doc); return 0; }

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, found) {
    const cJSON *dg = cJSON_GetObjectItem(r, "date_generale");
    if (!cJSON_IsObject(dg)) continue;
    const char *denumire = jo_sv(dg, "denumire");
    if (!denumire) continue;

    char cuistr[24];
    const cJSON *cv = cJSON_GetObjectItem(dg, "cui");
    if (cJSON_IsNumber(cv))      snprintf(cuistr, sizeof cuistr, "%.0f", cv->valuedouble);
    else if (cJSON_IsString(cv) && cv->valuestring)
      snprintf(cuistr, sizeof cuistr, "%s", cv->valuestring);
    else snprintf(cuistr, sizeof cuistr, "%s", cui);

    const char *adresa = jo_sv(dg, "adresa");
    const char *nrreg  = jo_sv(dg, "nrRegCom");
    const char *stare  = jo_sv(dg, "stare_inregistrare");
    const char *caen   = jo_sv(dg, "cod_CAEN");
    const char *forma  = jo_sv(dg, "forma_juridica");
    const char *dinreg = jo_sv(dg, "data_inregistrare");

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "RO_ANAF_VAT");
    cJSON_AddStringToObject(props, "source", "webservicesp.anaf.ro");
    cJSON_AddStringToObject(props, "cui", cuistr);
    cJSON_AddStringToObject(props, "denumire", denumire);
    if (adresa) cJSON_AddStringToObject(props, "adresa", adresa);
    if (nrreg)  cJSON_AddStringToObject(props, "nr_reg_com", nrreg);
    if (stare)  cJSON_AddStringToObject(props, "stare_inregistrare", stare);
    if (caen)   cJSON_AddStringToObject(props, "cod_caen", caen);
    if (forma)  cJSON_AddStringToObject(props, "forma_juridica", forma);
    if (dinreg) cJSON_AddStringToObject(props, "data_inregistrare", dinreg);
    if (jo_sv(dg, "telefon"))
      cJSON_AddStringToObject(props, "telefon", jo_sv(dg, "telefon"));
    if (jo_sv(dg, "organFiscalCompetent"))
      cJSON_AddStringToObject(props, "organ_fiscal_competent",
                              jo_sv(dg, "organFiscalCompetent"));
    const cJSON *tva = cJSON_GetObjectItem(r, "inregistrare_scop_Tva");
    if (tva) {
      const cJSON *sc = cJSON_GetObjectItem(tva, "scpTVA");
      if (cJSON_IsBool(sc))
        cJSON_AddBoolToObject(props, "scop_tva", cJSON_IsTrue(sc) ? 1 : 0);
      if (jo_sv(tva, "data_inceput_ScpTVA"))
        cJSON_AddStringToObject(props, "data_inceput_scp_tva",
                                jo_sv(tva, "data_inceput_ScpTVA"));
    }
    const cJSON *inact = cJSON_GetObjectItem(r, "stare_inactiv");
    if (inact) {
      const cJSON *si = cJSON_GetObjectItem(inact, "statusInactivi");
      if (cJSON_IsBool(si))
        cJSON_AddBoolToObject(props, "inactiv", cJSON_IsTrue(si) ? 1 : 0);
    }
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char summary[460];
    snprintf(summary, sizeof summary, "CUI %s%s%s%s%s%s%s",
             cuistr,
             nrreg ? " · " : "", nrreg ? nrreg : "",
             stare ? " · " : "", stare ? stare : "",
             forma ? " · " : "", forma ? forma : "");

    intel_item it = {0};
    it.remote_key      = cuistr;
    it.title           = denumire;
    it.summary         = summary;
    it.body            = pj;
    it.link            = "https://webservicesp.anaf.ro/api/PlatitorTvaRest/v9/tva";
    it.lang            = "ro";
    it.published_at    = dinreg;
    it.record_type     = "ro-taxpayer";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"registry\",\"romania\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[RO_ANAF_VAT] emitted %d\n", n);
  return 0;
}

/* --------------------------------------------------- Vietnam — tax code (MST) */
static int vn_run(const source_ctx *ctx, intel_sink *sink) {
  char mst[20];
  int nd = tp_digits(ctx->entity, mst, (int)sizeof mst);
  if (nd != 10 && nd != 13) return 0;          /* MST is 10 or 13 digits */

  char url[96];
  snprintf(url, sizeof url, "https://api.vietqr.io/v2/business/%s", mst);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "VN_TAXCODE_LOOKUP");
  if (!body) return 0;
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) return 0;

  const char *code = jo_sv(doc, "code");
  if (!code || strcmp(code, "00") != 0) {      /* not found is not a failure */
    cJSON_Delete(doc);
    fprintf(stderr, "[VN_TAXCODE_LOOKUP] code=%s (no record)\n", code ? code : "?");
    return 0;
  }
  const cJSON *d = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsObject(d)) { cJSON_Delete(doc); return 0; }

  const char *name = jo_sv(d, "name");
  if (!name) { cJSON_Delete(doc); return 0; }
  const char *intl   = jo_sv(d, "internationalName");
  const char *addr   = jo_sv(d, "address");
  const char *status = jo_sv(d, "status");
  const char *id     = jo_sv(d, "id");

  const cJSON *meta = cJSON_GetObjectItem(doc, "metadata");
  const char *msrc = meta ? jo_sv(meta, "source") : NULL;
  const char *mupd = meta ? jo_sv(meta, "updatedAt") : NULL;

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "VN_TAXCODE_LOOKUP");
  cJSON_AddStringToObject(props, "source",
                          msrc ? msrc : "https://www.gdt.gov.vn");
  cJSON_AddStringToObject(props, "redistributor", "api.vietqr.io");
  cJSON_AddStringToObject(props, "tax_code", id ? id : mst);
  cJSON_AddStringToObject(props, "name", name);
  if (intl)   cJSON_AddStringToObject(props, "international_name", intl);
  if (jo_sv(d, "shortName"))
    cJSON_AddStringToObject(props, "short_name", jo_sv(d, "shortName"));
  if (addr)   cJSON_AddStringToObject(props, "address", addr);
  if (status) cJSON_AddStringToObject(props, "status", status);
  if (mupd)   cJSON_AddStringToObject(props, "source_updated_at", mupd);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char summary[460];
  snprintf(summary, sizeof summary, "MST %s%s%s%s%s",
           id ? id : mst,
           status ? " · " : "", status ? status : "",
           addr ? " · " : "", addr ? addr : "");

  intel_item it = {0};
  it.remote_key      = id ? id : mst;
  it.title           = name;
  it.summary         = summary;
  it.body            = pj;
  it.link            = url;
  it.lang            = "vi";
  it.record_type     = "vn-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"registry\",\"vietnam\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(doc);
  fprintf(stderr, "[VN_TAXCODE_LOOKUP] emitted 1 (%s)\n", mst);
  return 0;
}

static const source_def reg2_pl_vat_def = {
  .id = "PL_VAT_WHITELIST", .collector = "osint",
  .name = "Poland MF VAT taxpayer whitelist (biala lista)",
  .update_interval_sec = 0, .run = pl_run,
  .category = "economy", .type = "api",
  .url = "https://wl-api.mf.gov.pl/api/search/nip/",
  .description = "Polish Ministry of Finance whitelist: NIP → legal name, KRS "
                 "and REGON numbers, registered address, VAT status, declared "
                 "bank accounts and named representatives/partners. Keyless.",
  .license = "Statutory public register (Ustawa o VAT art. 96b); official MF "
             "API, no key.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_pl_vat_def)

static const source_def reg2_ro_anaf_def = {
  .id = "RO_ANAF_VAT", .collector = "osint",
  .name = "Romania ANAF taxpayer register (CUI lookup)",
  .update_interval_sec = 0, .run = ro_run,
  .category = "economy", .type = "api",
  .url = "https://webservicesp.anaf.ro/api/PlatitorTvaRest/v9/tva",
  .description = "Romanian tax authority lookup: CUI → registered company name, "
                 "address, trade-register number (nrRegCom), legal form, CAEN "
                 "activity code and VAT status. Keyless.",
  .license = "Official ANAF public web service, no key, no stated restriction.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_ro_anaf_def)

static const source_def reg2_vn_taxcode_def = {
  .id = "VN_TAXCODE_LOOKUP", .collector = "osint",
  .name = "Vietnam business tax-code lookup",
  .update_interval_sec = 0, .run = vn_run,
  .category = "economy", .type = "api",
  .url = "https://api.vietqr.io/v2/business/",
  .description = "Vietnamese tax code (MST) → registered enterprise name, "
                 "head-office address and operating status — one of the few "
                 "keyless routes into the Vietnamese company register.",
  .license = "Mirror of the General Department of Taxation register; payload "
             "self-declares source = https://www.gdt.gov.vn with a ~2-month lag.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_vn_taxcode_def)
