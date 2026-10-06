/* collectors/osint/sources/corp_identifiers.c — corporate-identifier OSINT,
 * ported from OSINTsaas company_lookup.c (handle_lei_search / handle_vat_validator)
 * onto the JapanOSINT source ABI.
 *
 *   LEI_SEARCH    — company name → GLEIF Legal Entity Identifier records (free)
 *   VAT_VALIDATOR — EU VAT number → EU VIES validation (free)
 *
 * Both hit free public APIs (no key); honest-empty on no match / error. */
#include "lib/jocore.h"
#include "source.h"
#include "core/httpclient.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── LEI_SEARCH (GLEIF) ─────────────────────────────────────────────────── */
#define LEI_PAGE_SIZE 200   /* GLEIF's own maximum page[size] */
/* Ceiling for a walk whose total GLEIF did not declare; when it did, the walk
 * is sized from that total instead. Hitting either is disclosed. */
#define LEI_GUARD_PAGES 500   /* exhaustive-ok: cursor-walk runaway guard, used only without a declared total; hitting it emits a scoped collector-truncation-notice */

/* GLEIF name search, every page.
 *
 * CURSOR paging (page[cursor]=*, then the server's links.next), not page
 * numbers: GLEIF refuses page[number] * page[size] > 10,000 with a 400, so a
 * page-number walk of "bank" (11,370 records) could never get past record
 * 10,000 — and the old walk read that 400, like ANY failure on page >= 2
 * (timeout, 429, 5xx), as "end of results" and stopped without a word. The
 * cursor walk has no such ceiling and ends where the server stops publishing a
 * next link. A page that fails before then is disclosed in a notice scoped to
 * the entity, so one entity's notice no longer overwrites another's (they all
 * shared the uid LEI_SEARCH|truncation). */
static int lei_run(const source_ctx *ctx, intel_sink *sink) {
  const char *name = ctx->entity;
  if (!name || !*name) return 0;
  char enc[256]; jo_urlencode_buf(name, enc, sizeof enc);
  int emitted = 0, pages = 0, failed = 0, capped = 0;
  long total = -1, total_seen = 0, fail_status = 0;
  char first[576];
  snprintf(first, sizeof first,
    "https://api.gleif.org/api/v1/lei-records?filter[entity.legalName]=%s"
    "&page[size]=%d&page[cursor]=*", enc, LEI_PAGE_SIZE);
  char *url = strdup(first);
  while (url) {
    long guard = total >= 0 ? total / LEI_PAGE_SIZE + 2 : LEI_GUARD_PAGES;
    if (pages >= guard) { capped = 1; break; }
    http_response hr = {0};
    int rc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 20000, 2, &hr);
    pages++;
    cJSON *j = (rc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
    long status = rc == 0 ? hr.status : 0;
    http_response_free(&hr);
    if (!j) {
      fprintf(stderr, "[LEI_SEARCH] page %d status=%ld\n", pages, status);
      if (pages == 1) { free(url); return -1; }   /* the lookup itself failed */
      failed = 1; fail_status = status;
      break;
    }
    free(url); url = NULL;
    const cJSON *pg = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "meta"), "pagination");
    const cJSON *tv = pg ? cJSON_GetObjectItem(pg, "total") : NULL;
    if (cJSON_IsNumber(tv)) total = (long)tv->valuedouble;
    cJSON *data = cJSON_GetObjectItem(j, "data");
    int n = (data && cJSON_IsArray(data)) ? cJSON_GetArraySize(data) : 0;
    total_seen += n;
    for (int i = 0; i < n; i++) {
      cJSON *rec = cJSON_GetArrayItem(data, i);
      cJSON *attr = cJSON_GetObjectItem(rec, "attributes");
      cJSON *ent = attr ? cJSON_GetObjectItem(attr, "entity") : NULL;
      cJSON *ln = ent ? cJSON_GetObjectItem(ent, "legalName") : NULL;
      const char *lei = attr ? jo_str(attr, "lei") : NULL;
      const char *legal = ln ? jo_str(ln, "name") : NULL;
      if (!lei) continue;

      cJSON *out = cJSON_CreateObject();
      cJSON_AddStringToObject(out, "source", "api.gleif.org");
      cJSON_AddStringToObject(out, "lei", lei);
      if (legal) cJSON_AddStringToObject(out, "legal_name", legal);
      cJSON *addr = ent ? cJSON_GetObjectItem(ent, "legalAddress") : NULL;
      if (addr) {
        const char *country = jo_str(addr, "country"), *city = jo_str(addr, "city");
        if (country) cJSON_AddStringToObject(out, "country", country);
        if (city) cJSON_AddStringToObject(out, "city", city);
      }
      cJSON *reg = attr ? cJSON_GetObjectItem(attr, "registration") : NULL;
      const char *status_s = reg ? jo_str(reg, "status") : NULL;
      if (status_s) cJSON_AddStringToObject(out, "registration_status", status_s);
      /* The whole GLEIF record as served (house rule 2). The five fields above
       * are a summary; the record also carries the legal form, jurisdiction,
       * registration authority and number, headquarters and other addresses,
       * other and transliterated names, entity status and category, successor
       * entities, the managing LOU, renewal and validation dates, BIC/MIC/OCID
       * and the direct/ultimate-parent relationship links — all fetched, and
       * all dropped until 2026-10-06. */
      cJSON_AddItemToObject(out, "gleif_record", cJSON_Duplicate(rec, 1));
      char *bj = cJSON_PrintUnformatted(out);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "LEI_SEARCH");
      cJSON_AddStringToObject(props, "entity", name);
      char *pj = cJSON_PrintUnformatted(props);
      char rk[128], title[256];
      snprintf(rk, sizeof rk, "lei:%s", lei);
      snprintf(title, sizeof title, "LEI %s — %s", lei, legal ? legal : name);
      intel_item it = {0};
      it.remote_key = rk; it.title = title; it.body = bj; it.summary = legal ? legal : "LEI record";
      it.record_type = "osint_service_result"; it.properties_json = pj;
      it.tags_json = "[\"osint-search\",\"LEI_SEARCH\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj); cJSON_Delete(out); cJSON_Delete(props);
    }
    const char *next = jo_str(cJSON_GetObjectItem(j, "links"), "next");
    if (n > 0 && next && *next) url = strdup(next);   /* else: the end */
    cJSON_Delete(j);
  }
  free(url);
  if (failed || capped || (total >= 0 && total_seen < total)) {
    char reason[256];
    if (failed)
      snprintf(reason, sizeof reason,
               "GLEIF page %d of the name search failed (status %ld) before the "
               "server stopped publishing a next link", pages, fail_status);
    else if (capped)
      snprintf(reason, sizeof reason,
               "GLEIF name-search cursor walk hit its runaway guard after %d pages",
               pages);
    else
      snprintf(reason, sizeof reason,
               "GLEIF declared %ld matches but its next links ran out after %ld",
               total, total_seen);
    jo_trunc_notice_scoped(sink, "LEI_SEARCH", name,
                           "api.gleif.org/api/v1/lei-records", total_seen, total,
                           reason, failed ? "re-run the lookup"
                                          : "raise LEI_GUARD_PAGES in corp_identifiers.c");
  }
  /* The ABI is rc==0 on success, <0 on failure — core/scheduler.c logs any
   * non-zero rc as status="error" and hands it to anomaly_detect(), which
   * quarantines the source. Returning the emitted COUNT (as this did) meant a
   * perfectly working LEI lookup reported rc=5 and got benched by the circuit
   * breaker. The row count reaches the scheduler through the sink, not rc. */
  (void)emitted;
  return 0;
}

/* ── VAT_VALIDATOR (EU VIES) ────────────────────────────────────────────── */
static int vat_run(const source_ctx *ctx, intel_sink *sink) {
  const char *vat = ctx->entity;
  if (!vat || strlen(vat) < 3) return 0;
  /* split leading 2-letter ISO country from the numeric part */
  char cc[3] = {0}; cc[0] = (char)toupper((unsigned char)vat[0]); cc[1] = (char)toupper((unsigned char)vat[1]);
  if (!isalpha((unsigned char)cc[0]) || !isalpha((unsigned char)cc[1])) return 0;
  char num[64]; size_t o = 0;
  for (const char *p = vat + 2; *p && o < sizeof num - 1; p++)
    if (isalnum((unsigned char)*p)) num[o++] = *p;
  num[o] = 0;
  if (!o) return 0;

  char url[256];
  snprintf(url, sizeof url,
    "https://ec.europa.eu/taxation_customs/vies/rest-api/ms/%s/vat/%s", cc, num);
  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, NULL, NULL, 0, 12000, 2, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  if (!j) return 0;

  cJSON *out = cJSON_CreateObject();
  cJSON_AddStringToObject(out, "source", "ec.europa.eu/vies");
  cJSON_AddStringToObject(out, "country_code", cc);
  cJSON_AddStringToObject(out, "vat_number", num);
  cJSON *valid = cJSON_GetObjectItem(j, "valid");
  int isvalid = valid && cJSON_IsBool(valid) && cJSON_IsTrue(valid);
  cJSON_AddBoolToObject(out, "valid", isvalid);
  const char *nm = jo_str(j, "name"), *ad = jo_str(j, "address");
  if (nm && *nm) cJSON_AddStringToObject(out, "name", nm);
  if (ad && *ad) cJSON_AddStringToObject(out, "address", ad);
  cJSON_Delete(j);
  char *bj = cJSON_PrintUnformatted(out);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "VAT_VALIDATOR");
  cJSON_AddStringToObject(props, "entity", vat);
  char *pj = cJSON_PrintUnformatted(props);
  char rk[96], title[160];
  snprintf(rk, sizeof rk, "vat:%s%s", cc, num);
  snprintf(title, sizeof title, "VAT %s%s — %s", cc, num, isvalid ? "valid" : "not valid");
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj; it.summary = title;
  it.record_type = "osint_service_result"; it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"VAT_VALIDATOR\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); cJSON_Delete(out); cJSON_Delete(props);
  /* see the note in lei_run: rc is a status, not a count. Returning 1 made
   * every successful VAT validation look like an error to the scheduler. */
  return rc >= 0 ? 0 : -1;
}

static const source_def lei_def = {
  .id = "LEI_SEARCH", .collector = "osint", .name = "LEI Search", .run = lei_run,
  .category = "investigation", .type = "api", .url = "https://api.gleif.org/",
  .description = "Company name → Legal Entity Identifier (LEI) records from GLEIF (free).",
  .free_tier = 1,
};
REGISTER_SOURCE(lei_def)

static const source_def vat_def = {
  .id = "VAT_VALIDATOR", .collector = "osint", .name = "VAT Validator", .run = vat_run,
  .category = "investigation", .type = "api",
  .url = "https://ec.europa.eu/taxation_customs/vies/",
  .description = "EU VAT number validation + registered name/address via VIES (free).",
  .free_tier = 1,
};
REGISTER_SOURCE(vat_def)
