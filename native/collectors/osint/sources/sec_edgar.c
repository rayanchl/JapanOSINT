/* collectors/osint/sources/sec_edgar.c
 * OSINT service — faithful port of OSINTsaas osint_tools/sec_edgar.c
 * (handle_sec_edgar_search → sec_edgar_search). Canonical SERVICE name in
 * osint_dispatcher.c service_registry[] bound to handle_sec_edgar_search is
 * "SEC_EDGAR_SEARCH" (line 265). On-demand (interval 0); dispatcher runs it
 * with ctx->entity = a company name or numeric CIK.
 *
 * Reproduces sec_edgar_search() faithfully: a numeric query is treated as a
 * CIK (→ data.sec.gov/submissions/CIK<padded>.json company info + Form-4
 * insider trades via efts.sec.gov); otherwise a company-name full-text search
 * (efts.sec.gov/LATEST/search-index) plus per-CIK company_details and 10-K /
 * 10-Q filing searches. Builds the identical root: { query, source:"SEC
 * EDGAR", company|search_results, company_details?, insider_trades?,
 * annual_reports_10K?, quarterly_reports_10Q? }. SEC requires a descriptive
 * User-Agent, sent on every request. No API key. success=true, confidence 90.
 * Emits one osint_service_result row. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SEC_USER_AGENT "OSINTsaas/1.0 (contact@example.com)"

static const char *const SEC_HEADERS[] = {
  "User-Agent: " SEC_USER_AGENT,
  "Accept: application/json",
  NULL
};

static char *url_encode_dup(const char *in) {
  size_t n = strlen(in);
  char *out = malloc(n * 3 + 1);
  if (!out) return NULL;
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      out[w++] = (char)c;
    } else {
      sprintf(out + w, "%%%02X", c);
      w += 3;
    }
  }
  out[w] = 0;
  return out;
}

/* pad CIK to 10 digits, stripping leading zeros / non-digits (upstream pad_cik) */
static char *pad_cik(const char *cik) {
  char *padded = malloc(11);
  if (!padded) return NULL;
  const char *clean = cik;
  while (*clean && (*clean == '0' || !isdigit((unsigned char)*clean))) clean++;
  int len = (int)strlen(clean);
  if (len > 10) len = 10;
  int padding = 10 - len;
  memset(padded, '0', padding);
  strncpy(padded + padding, clean, len);
  padded[10] = '\0';
  return padded;
}

static cJSON *search_company(http_client *http, const char *company_name) {
  char *enc = url_encode_dup(company_name);
  if (!enc) return NULL;
  char url[1024];
  snprintf(url, sizeof url,
    "https://efts.sec.gov/LATEST/search-index?q=%s&dateRange=custom&category=form-cat1",
    enc);
  free(enc);

  cJSON *json = feed_get_json_h(http, url, SEC_HEADERS, 30000);
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "query", company_name);

  if (json) {
    cJSON *hits = cJSON_GetObjectItem(json, "hits");
    if (hits) {
      cJSON *hits_arr = cJSON_GetObjectItem(hits, "hits");
      if (hits_arr && cJSON_IsArray(hits_arr)) {
        cJSON *companies = cJSON_CreateArray();
        int count = cJSON_GetArraySize(hits_arr);
        for (int i = 0; i < count && i < 10; i++) {
          cJSON *hit = cJSON_GetArrayItem(hits_arr, i);
          cJSON *source = cJSON_GetObjectItem(hit, "_source");
          if (source) {
            cJSON *company = cJSON_CreateObject();
            cJSON *dn = cJSON_GetObjectItem(source, "display_names");
            cJSON *ciks = cJSON_GetObjectItem(source, "ciks");
            if (dn && cJSON_IsArray(dn) && cJSON_GetArraySize(dn) > 0) {
              cJSON *first = cJSON_GetArrayItem(dn, 0);
              if (first && first->valuestring)
                cJSON_AddStringToObject(company, "name", first->valuestring);
            }
            if (ciks && cJSON_IsArray(ciks) && cJSON_GetArraySize(ciks) > 0) {
              cJSON *first = cJSON_GetArrayItem(ciks, 0);
              if (first && first->valuestring)
                cJSON_AddStringToObject(company, "cik", first->valuestring);
            }
            cJSON_AddItemToArray(companies, company);
          }
        }
        cJSON_AddItemToObject(result, "companies", companies);
        cJSON_AddNumberToObject(result, "count", count);
      }
    }
    cJSON_Delete(json);
  }
  return result;
}

static cJSON *get_company_info(http_client *http, const char *cik) {
  char *padded = pad_cik(cik);
  if (!padded) return NULL;
  char url[256];
  snprintf(url, sizeof url, "https://data.sec.gov/submissions/CIK%s.json", padded);
  free(padded);

  cJSON *json = feed_get_json_h(http, url, SEC_HEADERS, 30000);
  if (!json) return NULL;

  cJSON *result = cJSON_CreateObject();
  struct { const char *src, *dst; } fields[] = {
    {"name","name"}, {"cik","cik"}, {"sic","sic_code"},
    {"sicDescription","industry"}, {"ein","ein"},
    {"stateOfIncorporation","state_of_incorporation"},
    {"fiscalYearEnd","fiscal_year_end"}, {"phone","phone"}, {NULL,NULL}
  };
  for (int i = 0; fields[i].src; i++) {
    cJSON *v = cJSON_GetObjectItem(json, fields[i].src);
    if (v && cJSON_IsString(v)) cJSON_AddStringToObject(result, fields[i].dst, v->valuestring);
  }

  cJSON *addresses = cJSON_GetObjectItem(json, "addresses");
  if (addresses) {
    cJSON *mailing = cJSON_GetObjectItem(addresses, "mailing");
    if (mailing) {
      cJSON *addr = cJSON_CreateObject();
      cJSON *s1 = cJSON_GetObjectItem(mailing, "street1");
      cJSON *ci = cJSON_GetObjectItem(mailing, "city");
      cJSON *st = cJSON_GetObjectItem(mailing, "stateOrCountry");
      cJSON *zp = cJSON_GetObjectItem(mailing, "zipCode");
      if (s1 && cJSON_IsString(s1)) cJSON_AddStringToObject(addr, "street", s1->valuestring);
      if (ci && cJSON_IsString(ci)) cJSON_AddStringToObject(addr, "city", ci->valuestring);
      if (st && cJSON_IsString(st)) cJSON_AddStringToObject(addr, "state", st->valuestring);
      if (zp && cJSON_IsString(zp)) cJSON_AddStringToObject(addr, "zip", zp->valuestring);
      cJSON_AddItemToObject(result, "mailing_address", addr);
    }
  }

  cJSON *filings = cJSON_GetObjectItem(json, "filings");
  if (filings) {
    cJSON *recent = cJSON_GetObjectItem(filings, "recent");
    if (recent) {
      cJSON *forms = cJSON_GetObjectItem(recent, "form");
      cJSON *dates = cJSON_GetObjectItem(recent, "filingDate");
      cJSON *accessions = cJSON_GetObjectItem(recent, "accessionNumber");
      if (forms && cJSON_IsArray(forms)) {
        cJSON *farr = cJSON_CreateArray();
        int count = cJSON_GetArraySize(forms);
        for (int i = 0; i < count && i < 20; i++) {
          cJSON *filing = cJSON_CreateObject();
          cJSON *form = cJSON_GetArrayItem(forms, i);
          if (form && cJSON_IsString(form))
            cJSON_AddStringToObject(filing, "form_type", form->valuestring);
          if (dates && cJSON_IsArray(dates)) {
            cJSON *d = cJSON_GetArrayItem(dates, i);
            if (d && cJSON_IsString(d)) cJSON_AddStringToObject(filing, "filing_date", d->valuestring);
          }
          if (accessions && cJSON_IsArray(accessions)) {
            cJSON *acc = cJSON_GetArrayItem(accessions, i);
            if (acc && cJSON_IsString(acc)) {
              cJSON_AddStringToObject(filing, "accession_number", acc->valuestring);
              char acc_no_dash[32] = {0};
              int j = 0;
              for (int k = 0; acc->valuestring[k] && j < 31; k++)
                if (acc->valuestring[k] != '-') acc_no_dash[j++] = acc->valuestring[k];
              char doc_url[512];
              snprintf(doc_url, sizeof doc_url,
                "https://www.sec.gov/Archives/edgar/data/%s/%s", cik, acc_no_dash);
              cJSON_AddStringToObject(filing, "url", doc_url);
            }
          }
          cJSON_AddItemToArray(farr, filing);
        }
        cJSON_AddItemToObject(result, "recent_filings", farr);
      }
    }
  }
  cJSON_Delete(json);
  return result;
}

static cJSON *get_insider_trades(http_client *http, const char *cik) {
  char *padded = pad_cik(cik);
  if (!padded) return NULL;
  char url[512];
  snprintf(url, sizeof url,
    "https://efts.sec.gov/LATEST/search-index?q=*&dateRange=custom&forms=4&ciks=%s",
    padded);
  free(padded);

  cJSON *json = feed_get_json_h(http, url, SEC_HEADERS, 30000);
  if (!json) return NULL;

  cJSON *result = cJSON_CreateArray();
  cJSON *hits = cJSON_GetObjectItem(json, "hits");
  if (hits) {
    cJSON *hits_arr = cJSON_GetObjectItem(hits, "hits");
    if (hits_arr && cJSON_IsArray(hits_arr)) {
      int count = cJSON_GetArraySize(hits_arr);
      for (int i = 0; i < count && i < 50; i++) {
        cJSON *hit = cJSON_GetArrayItem(hits_arr, i);
        cJSON *source = cJSON_GetObjectItem(hit, "_source");
        if (source) {
          cJSON *trade = cJSON_CreateObject();
          cJSON *fd = cJSON_GetObjectItem(source, "file_date");
          cJSON *dn = cJSON_GetObjectItem(source, "display_names");
          if (fd && cJSON_IsString(fd)) cJSON_AddStringToObject(trade, "filing_date", fd->valuestring);
          if (dn && cJSON_IsArray(dn)) {
            for (int j = 0; j < cJSON_GetArraySize(dn); j++) {
              cJSON *name = cJSON_GetArrayItem(dn, j);
              if (name && cJSON_IsString(name) && strchr(name->valuestring, ',')) {
                cJSON_AddStringToObject(trade, "insider_name", name->valuestring);
                break;
              }
            }
          }
          cJSON_AddItemToArray(result, trade);
        }
      }
    }
  }
  cJSON_Delete(json);
  return result;
}

static cJSON *search_filings(http_client *http, const char *company_name, const char *form_type) {
  char *enc = url_encode_dup(company_name);
  if (!enc) return NULL;
  char url[1024];
  snprintf(url, sizeof url,
    "https://efts.sec.gov/LATEST/search-index?q=%s&forms=%s&dateRange=custom",
    enc, form_type);
  free(enc);

  cJSON *json = feed_get_json_h(http, url, SEC_HEADERS, 30000);
  if (!json) return NULL;

  cJSON *result = cJSON_CreateArray();
  cJSON *hits = cJSON_GetObjectItem(json, "hits");
  if (hits) {
    cJSON *hits_arr = cJSON_GetObjectItem(hits, "hits");
    if (hits_arr && cJSON_IsArray(hits_arr)) {
      int count = cJSON_GetArraySize(hits_arr);
      for (int i = 0; i < count && i < 20; i++) {
        cJSON *hit = cJSON_GetArrayItem(hits_arr, i);
        cJSON *source = cJSON_GetObjectItem(hit, "_source");
        if (source) {
          cJSON *filing = cJSON_CreateObject();
          cJSON *form = cJSON_GetObjectItem(source, "form");
          cJSON *fd = cJSON_GetObjectItem(source, "file_date");
          cJSON *dn = cJSON_GetObjectItem(source, "display_names");
          cJSON *fn = cJSON_GetObjectItem(source, "file_num");
          if (form && cJSON_IsString(form)) cJSON_AddStringToObject(filing, "form_type", form->valuestring);
          if (fd && cJSON_IsString(fd)) cJSON_AddStringToObject(filing, "filing_date", fd->valuestring);
          if (dn && cJSON_IsArray(dn) && cJSON_GetArraySize(dn) > 0) {
            cJSON *first = cJSON_GetArrayItem(dn, 0);
            if (first && first->valuestring) cJSON_AddStringToObject(filing, "company", first->valuestring);
          }
          if (fn && cJSON_IsString(fn)) cJSON_AddStringToObject(filing, "file_number", fn->valuestring);
          cJSON_AddItemToArray(result, filing);
        }
      }
    }
  }
  cJSON_Delete(json);
  return result;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *query = ctx->entity;
  if (!query || !*query) return -1;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", query);
  cJSON_AddStringToObject(root, "source", "SEC EDGAR");

  int is_cik = 1;
  for (int i = 0; query[i]; i++)
    if (!isdigit((unsigned char)query[i])) { is_cik = 0; break; }

  if (is_cik) {
    cJSON *company_info = get_company_info(ctx->http, query);
    if (company_info) {
      cJSON_AddItemToObject(root, "company", company_info);
      cJSON *insider = get_insider_trades(ctx->http, query);
      if (insider) cJSON_AddItemToObject(root, "insider_trades", insider);
    } else {
      cJSON_AddStringToObject(root, "error", "CIK not found");
    }
  } else {
    cJSON *sr = search_company(ctx->http, query);
    if (sr) {
      cJSON_AddItemToObject(root, "search_results", sr);
      cJSON *companies = cJSON_GetObjectItem(sr, "companies");
      if (companies && cJSON_IsArray(companies) && cJSON_GetArraySize(companies) > 0) {
        cJSON *first = cJSON_GetArrayItem(companies, 0);
        cJSON *cik = cJSON_GetObjectItem(first, "cik");
        if (cik && cJSON_IsString(cik)) {
          cJSON *ci = get_company_info(ctx->http, cik->valuestring);
          if (ci) cJSON_AddItemToObject(root, "company_details", ci);
        }
      }
    }
    cJSON *ar = search_filings(ctx->http, query, "10-K");
    if (ar && cJSON_GetArraySize(ar) > 0) cJSON_AddItemToObject(root, "annual_reports_10K", ar);
    else if (ar) cJSON_Delete(ar);
    cJSON *qr = search_filings(ctx->http, query, "10-Q");
    if (qr && cJSON_GetArraySize(qr) > 0) cJSON_AddItemToObject(root, "quarterly_reports_10Q", qr);
    else if (qr) cJSON_Delete(qr);
  }

  char *bj = cJSON_PrintUnformatted(root);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SEC_EDGAR_SEARCH");
  cJSON_AddStringToObject(props, "entity", query);
  cJSON_AddBoolToObject(props, "success", 1);          /* upstream success=true */
  cJSON_AddNumberToObject(props, "confidence", 90);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "secedgar:%s", query);
  char title[320];
  snprintf(title, sizeof title, "SEC_EDGAR_SEARCH — %s", query);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "SEC EDGAR search";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SEC_EDGAR_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  cJSON_Delete(root);
  return rc >= 0 ? 0 : -1;
}

static const source_def sec_edgar_def = {
  .id = "SEC_EDGAR_SEARCH", .collector = "osint",
  .name = "SEC EDGAR Search", .name_ja = "SEC EDGAR検索",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(sec_edgar_def)
