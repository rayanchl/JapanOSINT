/* collectors/sources/transparency_world.c
 * OSINT services — world political-transparency / lobbying / donations registers.
 * Three on-demand entity-pivot sources sharing one run() that switches on
 * ctx->source_id:
 *
 *   EU_TRANSPARENCY  EU Transparency Register (lobbyists / interest reps).
 *                    LIVE, keyless. The public register exposes a REST search
 *                    at ec.europa.eu/transparencyregister/public/consultation/
 *                    search.do plus a downloadable dataset; we hit the JSON
 *                    consultation search endpoint and emit one item per matched
 *                    organisation (name, registration number, category, country,
 *                    lobbying head-count / cost band when present).
 *
 *   UK_DONATIONS     UK Electoral Commission — political donations & loans.
 *                    LIVE, keyless. search.electoralcommission.org.uk exposes a
 *                    JSON search API for the donations register; we search by
 *                    donor/regulated-entity and emit one item per donation
 *                    (donor, recipient, value, accepted date, type).
 *
 *   OPENSECRETS      US OpenSecrets (CRP) lobbying / campaign-finance API.
 *                    GATED on OPENSECRETS_KEY (free key). We call the getOrgs /
 *                    orgSummary endpoints and emit one item per matched org.
 *
 * HONESTY: every branch REAL-fetches via jo_get and emits only parsed fields, or
 * honest-empty (return 0). OPENSECRETS returns 0 with a [gated] log when its key
 * is unset. Nothing is fabricated. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- small helpers ------------------------------------------------------- */

/* cJSON field as string regardless of whether it is stored as string/number.
 * malloc'd for numbers (caller frees via the *owned flag), borrowed for strings. */
static const char *tw_num_or_str(const cJSON *o, const char *k, char *buf, size_t n) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return NULL;
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) return v->valuestring;
  if (cJSON_IsNumber(v)) { snprintf(buf, n, "%g", v->valuedouble); return buf; }
  return NULL;
}

/* ---- EU Transparency Register ------------------------------------------- */

static int eu_emit_org(intel_sink *sink, const cJSON *o) {
  if (!o) return 0;
  const char *name = jo_sv(o, "name");
  if (!name) name = jo_sv(o, "originalName");
  const char *ident = jo_sv(o, "identificationCode");
  if (!ident) ident = jo_sv(o, "registrationNumber");
  if (!name && !ident) return 0;

  const char *category = jo_sv(o, "registrationCategory");
  if (!category) { const cJSON *c = cJSON_GetObjectItem(o, "category");
                   if (c) category = jo_sv(c, "name"); }
  const char *acronym  = jo_sv(o, "acronym");
  const char *country  = jo_sv(o, "headOfficeCountry");
  if (!country) { const cJSON *ho = cJSON_GetObjectItem(o, "headOffice");
                  if (ho) country = jo_sv(ho, "country"); }
  const char *web      = jo_sv(o, "webSiteURL");
  if (!web) web = jo_sv(o, "webSite");
  char nbuf[64];
  const char *fte = tw_num_or_str(o, "numberOfNaturalPersons", nbuf, sizeof nbuf);

  cJSON *data = cJSON_CreateObject();
  if (name)     cJSON_AddStringToObject(data, "name", name);
  if (acronym)  cJSON_AddStringToObject(data, "acronym", acronym);
  if (ident)    cJSON_AddStringToObject(data, "registration_number", ident);
  if (category) cJSON_AddStringToObject(data, "category", category);
  if (country)  cJSON_AddStringToObject(data, "country", country);
  if (fte)      cJSON_AddStringToObject(data, "persons_involved", fte);
  if (web)      cJSON_AddStringToObject(data, "website", web);
  cJSON_AddStringToObject(data, "source", "EU Transparency Register");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "EU_TRANSPARENCY");
  cJSON_AddStringToObject(props, "source", "EU Transparency Register");
  if (ident)    cJSON_AddStringToObject(props, "registration_number", ident);
  if (country)  cJSON_AddStringToObject(props, "country", country);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[512]; link[0] = 0;
  if (ident) snprintf(link, sizeof link,
    "https://ec.europa.eu/transparencyregister/public/consultation/displaylobbyist.do?id=%s", ident);

  intel_item it = {0};
  it.remote_key      = ident ? ident : name;
  it.title           = name ? name : ident;
  it.summary         = category;
  it.body            = bj;
  it.lang            = "en";
  it.link            = link[0] ? link : web;
  it.record_type     = "eu-transparency-org";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"EU_TRANSPARENCY\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_eu_transparency(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  char url[1024];
  snprintf(url, sizeof url,
    "https://ec.europa.eu/transparencyregister/public/consultation/search.do"
    "?action=search&searchType=simpleSearch&name=%s&resultsType=json", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "eu_transparency");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  /* Response shapes vary: {results:[...]} or {searchResults:[...]} or a bare
   * array. Tolerate all; emit one item per organisation. */
  cJSON *arr = cJSON_GetObjectItem(root, "results");
  if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(root, "searchResults");
  if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(root, "interestRepresentatives");
  int emitted = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *o; cJSON_ArrayForEach(o, arr) emitted += eu_emit_org(sink, o);
  } else if (cJSON_IsArray(root)) {
    cJSON *o; cJSON_ArrayForEach(o, root) emitted += eu_emit_org(sink, o);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[eu_transparency] emitted %d\n", emitted);
  return 0;
}

/* ---- UK Electoral Commission donations ---------------------------------- */

static int uk_emit_donation(intel_sink *sink, const cJSON *d) {
  if (!d) return 0;
  const char *donor  = jo_sv(d, "DonorName");
  const char *recip  = jo_sv(d, "RegulatedEntityName");
  if (!donor && !recip) return 0;

  char vbuf[64];
  const char *value  = tw_num_or_str(d, "Value", vbuf, sizeof vbuf);
  const char *accepted = jo_sv(d, "AcceptedDate");
  if (!accepted) accepted = jo_sv(d, "ReceivedDate");
  const char *dtype  = jo_sv(d, "DonationType");
  const char *donortype = jo_sv(d, "DonorStatus");
  const char *nature = jo_sv(d, "NatureOfDonation");
  const char *ecref  = jo_sv(d, "ECRef");

  cJSON *data = cJSON_CreateObject();
  if (donor)     cJSON_AddStringToObject(data, "donor", donor);
  if (donortype) cJSON_AddStringToObject(data, "donor_status", donortype);
  if (recip)     cJSON_AddStringToObject(data, "recipient", recip);
  if (value)     cJSON_AddStringToObject(data, "value", value);
  if (accepted)  cJSON_AddStringToObject(data, "accepted_date", accepted);
  if (dtype)     cJSON_AddStringToObject(data, "donation_type", dtype);
  if (nature)    cJSON_AddStringToObject(data, "nature", nature);
  if (ecref)     cJSON_AddStringToObject(data, "ec_ref", ecref);
  cJSON_AddStringToObject(data, "source", "UK Electoral Commission");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "UK_DONATIONS");
  cJSON_AddStringToObject(props, "source", "UK Electoral Commission");
  if (value)    cJSON_AddStringToObject(props, "value", value);
  if (recip)    cJSON_AddStringToObject(props, "recipient", recip);
  if (ecref)    cJSON_AddStringToObject(props, "ec_ref", ecref);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char title[512];
  snprintf(title, sizeof title, "%s%s%s",
    donor ? donor : "(donor)",
    recip ? " → " : "",
    recip ? recip : "");

  char link[256]; link[0] = 0;
  if (ecref) snprintf(link, sizeof link,
    "https://search.electoralcommission.org.uk/English/Donations/%s", ecref);

  intel_item it = {0};
  it.remote_key      = ecref ? ecref : title;
  it.title           = title;
  it.summary         = value;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = accepted;
  it.link            = link[0] ? link : NULL;
  it.record_type     = "uk-political-donation";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"UK_DONATIONS\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_uk_donations(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  /* Public donations search API. `query` matches donor / regulated entity. */
  char url[1024];
  snprintf(url, sizeof url,
    "https://search.electoralcommission.org.uk/api/search/Donations"
    "?query=%s&sort=AcceptedDate&order=desc&start=0&rows=25"
    "&register=gb&register=ni&register=none"
    "&isIrishSourceYes=true&isIrishSourceNo=true&includeOutsideSection75=true", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "uk_donations");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *arr = cJSON_GetObjectItem(root, "Result");
  if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(root, "result");
  if (!cJSON_IsArray(arr)) arr = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *d; cJSON_ArrayForEach(d, arr) emitted += uk_emit_donation(sink, d);
  } else if (cJSON_IsArray(root)) {
    cJSON *d; cJSON_ArrayForEach(d, root) emitted += uk_emit_donation(sink, d);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[uk_donations] emitted %d\n", emitted);
  return 0;
}

/* ---- US OpenSecrets (gated) --------------------------------------------- */

static int os_emit_org(intel_sink *sink, const cJSON *attr, const char *orgid) {
  if (!attr) return 0;
  const char *name = jo_sv(attr, "orgname");
  if (!name) return 0;

  char b1[64], b2[64], b3[64];
  const char *total   = tw_num_or_str(attr, "total", b1, sizeof b1);
  const char *dems    = tw_num_or_str(attr, "dems", b2, sizeof b2);
  const char *repubs  = tw_num_or_str(attr, "repubs", b3, sizeof b3);
  const char *lobbying = jo_sv(attr, "lobbying");
  const char *indivs  = jo_sv(attr, "indivs");
  const char *pacs    = jo_sv(attr, "pacs");
  const char *cid     = jo_sv(attr, "orgid");
  if (!cid) cid = orgid;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "name", name);
  if (cid)     cJSON_AddStringToObject(data, "orgid", cid);
  if (total)   cJSON_AddStringToObject(data, "total_contributions", total);
  if (dems)    cJSON_AddStringToObject(data, "to_democrats", dems);
  if (repubs)  cJSON_AddStringToObject(data, "to_republicans", repubs);
  if (lobbying)cJSON_AddStringToObject(data, "lobbying", lobbying);
  if (indivs)  cJSON_AddStringToObject(data, "from_individuals", indivs);
  if (pacs)    cJSON_AddStringToObject(data, "from_pacs", pacs);
  cJSON_AddStringToObject(data, "source", "OpenSecrets");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "OPENSECRETS");
  cJSON_AddStringToObject(props, "source", "OpenSecrets");
  if (cid)   cJSON_AddStringToObject(props, "orgid", cid);
  if (total) cJSON_AddStringToObject(props, "total_contributions", total);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[256]; link[0] = 0;
  if (cid) snprintf(link, sizeof link,
    "https://www.opensecrets.org/orgs/summary?id=%s", cid);

  intel_item it = {0};
  it.remote_key      = cid ? cid : name;
  it.title           = name;
  it.summary         = total;
  it.body            = bj;
  it.lang            = "en";
  it.link            = link[0] ? link : NULL;
  it.record_type     = "opensecrets-org";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"OPENSECRETS\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_opensecrets(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *key = jo_env("OPENSECRETS_KEY");
  if (!key) {
    fprintf(stderr, "[opensecrets] gated (no OPENSECRETS_KEY)\n");
    return 0;
  }
  char *enc = jo_urlencode(q);
  char *kenc = jo_urlencode(key);
  if (!enc || !kenc) { free(enc); free(kenc); return 0; }

  /* getOrgs: resolve an org name → CRP orgid(s). */
  char url[1024];
  snprintf(url, sizeof url,
    "https://www.opensecrets.org/api/?method=getOrgs&org=%s&apikey=%s&output=json",
    enc, kenc);
  free(enc); free(kenc);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "opensecrets");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  /* Shape: {"response":{"organization":[{"@attributes":{...}}, ...]}} or a
   * single object when only one match. */
  int emitted = 0;
  cJSON *resp = cJSON_GetObjectItem(root, "response");
  cJSON *orgs = resp ? cJSON_GetObjectItem(resp, "organization") : NULL;
  if (cJSON_IsArray(orgs)) {
    cJSON *o;
    cJSON_ArrayForEach(o, orgs) {
      cJSON *attr = cJSON_GetObjectItem(o, "@attributes");
      emitted += os_emit_org(sink, attr ? attr : o, NULL);
    }
  } else if (cJSON_IsObject(orgs)) {
    cJSON *attr = cJSON_GetObjectItem(orgs, "@attributes");
    emitted += os_emit_org(sink, attr ? attr : orgs, NULL);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[opensecrets] emitted %d\n", emitted);
  return 0;
}

/* ---- dispatcher --------------------------------------------------------- */

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *id = ctx->source_id ? ctx->source_id : "";
  if (strcmp(id, "EU_TRANSPARENCY") == 0) return run_eu_transparency(ctx, sink);
  if (strcmp(id, "UK_DONATIONS")    == 0) return run_uk_donations(ctx, sink);
  if (strcmp(id, "OPENSECRETS")     == 0) return run_opensecrets(ctx, sink);
  return -1;
}

static const source_def eu_transparency_def = {
  .id = "EU_TRANSPARENCY", .collector = "osint",
  .name = "EU Transparency Register", .name_ja = "EU 透明性登録簿",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://ec.europa.eu/transparencyregister/public/",
  .description = "EU Transparency Register — lobbyists / interest representatives (keyless): org, reg number, category, country",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(eu_transparency_def)

static const source_def uk_donations_def = {
  .id = "UK_DONATIONS", .collector = "osint",
  .name = "UK Electoral Commission Donations", .name_ja = "英国 選挙管理委員会 献金",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://search.electoralcommission.org.uk/",
  .description = "UK Electoral Commission — political donations & loans register (keyless): donor, recipient, value, date",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(uk_donations_def)

static const source_def opensecrets_def = {
  .id = "OPENSECRETS", .collector = "osint",
  .name = "OpenSecrets Campaign Finance", .name_ja = "OpenSecrets 政治資金",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://www.opensecrets.org/api/",
  .description = "US OpenSecrets (CRP) — org lobbying & campaign-finance totals (needs OPENSECRETS_KEY)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(opensecrets_def)
