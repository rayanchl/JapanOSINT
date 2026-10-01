/* collectors/sources/reg_fi_prh.c
 * OSINT service — FI_PRH. On-demand company pivot (entity = company name)
 * against the Finnish Patent and Registration Office (PRH) open YTJ API
 * (avoindata.prh.fi), the authoritative Finnish business register. Keyless,
 * LIVE. One intel_item per matched company, keyed by business id (Y-tunnus).
 * Emits business id, name, address, status. No matches / fetch failure →
 * emits nothing (honest empty).
 *
 * Response shape (v3): { "companies": [ { businessId:{value}, names:[{name,
 * type,endDate}], addresses:[{street,buildingNumber,postCode,postOffices:
 * [{city,languageCode}]}], companyForms:[{descriptions:[{languageCode,
 * description}]}], website:{url}, status } ] }. name type "1" = primary;
 * a name with no endDate is the current one. Descriptions languageCode "3"
 * = English. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* current primary name: first names[] entry of type "1" with no endDate,
 * else first names[] entry with no endDate, else first entry. */
static const char *fi_primary_name(const cJSON *c) {
  const cJSON *names = cJSON_GetObjectItem(c, "names");
  if (!cJSON_IsArray(names)) return NULL;
  const char *any = NULL, *live = NULL, *primary = NULL;
  const cJSON *n;
  cJSON_ArrayForEach(n, names) {
    const char *nm = jo_sv(n, "name");
    if (!nm) continue;
    if (!any) any = nm;
    int ended = jo_sv(n, "endDate") != NULL;
    const char *type = jo_sv(n, "type");
    if (!ended && !live) live = nm;
    if (!ended && type && strcmp(type, "1") == 0 && !primary) primary = nm;
  }
  return primary ? primary : (live ? live : any);
}

/* English (languageCode "3") description from a descriptions[] array,
 * else the first available. */
static const char *fi_desc(const cJSON *arr) {
  if (!cJSON_IsArray(arr)) return NULL;
  const char *any = NULL;
  const cJSON *d;
  cJSON_ArrayForEach(d, arr) {
    const char *txt = jo_sv(d, "description");
    if (!txt) continue;
    if (!any) any = txt;
    const char *lc = jo_sv(d, "languageCode");
    if (lc && strcmp(lc, "3") == 0) return txt;
  }
  return any;
}

/* Build a one-line street/city address from addresses[0]. malloc'd or NULL. */
static char *fi_address(const cJSON *c) {
  const cJSON *addrs = cJSON_GetObjectItem(c, "addresses");
  if (!cJSON_IsArray(addrs)) return NULL;
  const cJSON *a = cJSON_GetArrayItem(addrs, 0);  /* exhaustive-ok: display pick; addresses_all is attached to the record */
  if (!a) return NULL;
  /* Renders the primary address; the caller attaches addresses_all. */
  const char *street = jo_sv(a, "street");
  const char *num    = jo_sv(a, "buildingNumber");
  const char *post   = jo_sv(a, "postCode");
  const char *city   = NULL;
  const cJSON *pos = cJSON_GetObjectItem(a, "postOffices");
  if (cJSON_IsArray(pos)) {
    const cJSON *po;
    const char *anycity = NULL;
    cJSON_ArrayForEach(po, pos) {
      const char *ct = jo_sv(po, "city");
      if (!ct) continue;
      if (!anycity) anycity = ct;
      const char *lc = jo_sv(po, "languageCode");
      if (lc && strcmp(lc, "1") == 0) { city = ct; break; }  /* Finnish */
    }
    if (!city) city = anycity;
  }
  if (!street && !city && !post) return NULL;
  char buf[512];
  snprintf(buf, sizeof buf, "%s%s%s%s%s%s",
           street ? street : "", (street && num) ? " " : "", num ? num : "",
           ((street || num) && (post || city)) ? ", " : "",
           post ? post : "", city ? (post ? " " : "") : "");
  size_t l = strlen(buf);
  if (city) snprintf(buf + l, sizeof buf - l, "%s", city);
  return buf[0] ? strdup(buf) : NULL;
}

static int emit_company(intel_sink *sink, const cJSON *c) {
  if (!c) return 0;
  const cJSON *bid = cJSON_GetObjectItem(c, "businessId");
  const char *id = bid ? jo_sv(bid, "value") : NULL;
  const char *name = fi_primary_name(c);
  if (!id && !name) return 0;

  char *addr = fi_address(c);

  /* status: top-level numeric enum (2 = registered/active). */
  const cJSON *st = cJSON_GetObjectItem(c, "status");
  char status[16] = {0};
  if (cJSON_IsNumber(st)) snprintf(status, sizeof status, "%d", (int)st->valuedouble);

  /* company form (legal type), English if available. */
  const cJSON *forms = cJSON_GetObjectItem(c, "companyForms");
  const char *form = NULL;
  if (cJSON_IsArray(forms)) {
    const cJSON *f = cJSON_GetArrayItem(forms, 0);  /* exhaustive-ok: current form; company_forms_all carries the history */
    if (f) form = fi_desc(cJSON_GetObjectItem(f, "descriptions"));
  }
  const cJSON *website = cJSON_GetObjectItem(c, "website");
  const char *url = website ? jo_sv(website, "url") : NULL;
  const char *reg = jo_sv(c, "registrationDate");

  cJSON *data = cJSON_CreateObject();
  if (name)     cJSON_AddStringToObject(data, "name", name);
  if (id)       cJSON_AddStringToObject(data, "business_id", id);
  if (addr)     cJSON_AddStringToObject(data, "address", addr);
  if (status[0])cJSON_AddStringToObject(data, "status", status);
  if (form)     cJSON_AddStringToObject(data, "company_form", form);
  if (url)      cJSON_AddStringToObject(data, "website", url);
  if (reg)      cJSON_AddStringToObject(data, "registration_date", reg);
  /* `address` and `company_form` above are display picks: PRH lists visiting AND
   * postal addresses, and companyForms is a dated history. Carry both in full
   * (docs/SOURCE_EXHAUSTIVENESS.md). */
  {
    const cJSON *all_addrs = cJSON_GetObjectItem(c, "addresses");
    if (cJSON_IsArray(all_addrs) && cJSON_GetArraySize(all_addrs) > 0)
      cJSON_AddItemToObject(data, "addresses_all", cJSON_Duplicate(all_addrs, 1));
    if (cJSON_IsArray(forms) && cJSON_GetArraySize(forms) > 0)
      cJSON_AddItemToObject(data, "company_forms_all", cJSON_Duplicate(forms, 1));
  }
  cJSON_AddStringToObject(data, "source", "PRH (Finland)");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "FI_PRH");
  cJSON_AddStringToObject(props, "source", "PRH");
  if (id)       cJSON_AddStringToObject(props, "business_id", id);
  if (addr)     cJSON_AddStringToObject(props, "address", addr);
  if (status[0])cJSON_AddStringToObject(props, "status", status);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  /* PRH profile permalink via business id. */
  char link[256] = {0};
  if (id) snprintf(link, sizeof link,
                   "https://tietopalvelu.ytj.fi/en/company/%s", id);

  intel_item it = {0};
  it.remote_key      = id ? id : name;
  it.title           = name ? name : id;
  it.summary         = addr;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = reg;
  it.link            = link[0] ? link : NULL;
  it.record_type     = "fi-prh-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"FI_PRH\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(addr);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char url[1024];
  snprintf(url, sizeof url,
    "https://avoindata.prh.fi/opendata-ytj-api/v3/companies?name=%s", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "fi_prh");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *companies = cJSON_GetObjectItem(root, "companies");
  int emitted = 0, cap = 0;
  if (cJSON_IsArray(companies)) {
    cJSON *c;
    cJSON_ArrayForEach(c, companies) {
      if (cap++ >= 25) break;         /* bound broad name matches */
      emitted += emit_company(sink, c);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[fi_prh] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def reg_fi_prh_def = {
  .id = "FI_PRH", .collector = "osint",
  .name = "Finland PRH Company Lookup", .name_ja = "フィンランド PRH 企業情報",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/reg_fi_prh",
  .description = "Finnish Patent & Registration Office (PRH) open YTJ register — company id, name, address, status",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg_fi_prh_def)
