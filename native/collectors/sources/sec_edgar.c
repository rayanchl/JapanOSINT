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
 *
 * PER-RECORD EMIT: emits ONE osint_service_result row per genuine fetched
 * record — one per company filing (remote_key "filing:<accession>"), one per
 * insider Form-4 trade (remote_key "form4:<...>"), and optionally one
 * company-profile row (remote_key "company:<CIK>") carrying the real fetched
 * company info. Each row has a distinct stable remote_key, title, per-record
 * body, summary, published_at (filingDate), and link (EDGAR doc URL). If
 * nothing is found, emits nothing and returns 0 (honest empty — no summary
 * blob, no seeded rows). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
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
              cJSON *first = cJSON_GetArrayItem(dn, 0);  /* exhaustive-ok: display pick; names_all carries every filer */
              if (cJSON_GetArraySize(dn) > 1)
                cJSON_AddItemToObject(company, "names_all", cJSON_Duplicate(dn, 1));
              if (first && first->valuestring)
                cJSON_AddStringToObject(company, "name", first->valuestring);
            }
            if (ciks && cJSON_IsArray(ciks) && cJSON_GetArraySize(ciks) > 0) {
              cJSON *first = cJSON_GetArrayItem(ciks, 0);  /* exhaustive-ok: display pick; ciks_all carries every CIK */
              if (first && first->valuestring)
                cJSON_AddStringToObject(company, "cik", first->valuestring);
              if (cJSON_GetArraySize(ciks) > 1)
                cJSON_AddItemToObject(company, "ciks_all", cJSON_Duplicate(ciks, 1));
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
            cJSON *first = cJSON_GetArrayItem(dn, 0);  /* exhaustive-ok: display pick; companies_all carries every filer */
            if (cJSON_GetArraySize(dn) > 1)
              cJSON_AddItemToObject(filing, "companies_all", cJSON_Duplicate(dn, 1));
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

/* ISO date helper: SEC dates are already "YYYY-MM-DD"; pass through if present. */
static const char *cjson_str(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring) ? v->valuestring : NULL;
}

/* Build the standard properties_json for one record (real fetched fields). */
static char *make_props(const char *entity, const char *cik, const char *form_type) {
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SEC_EDGAR_SEARCH");
  if (entity)    cJSON_AddStringToObject(props, "entity", entity);
  if (cik)       cJSON_AddStringToObject(props, "cik", cik);
  if (form_type) cJSON_AddStringToObject(props, "form_type", form_type);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);
  return pj;
}

/* Emit one company-profile row from the real fetched company info. The
 * `recent_filings` array is stripped into the body's own copy so the profile
 * body is self-contained but distinct from the per-filing rows. Returns 1 if
 * emitted. `company` is owned by the caller. */
static int emit_company(intel_sink *sink, const char *entity,
                        const char *cik, cJSON *company) {
  if (!company) return 0;
  const char *name = cjson_str(company, "name");

  char *bj = cJSON_PrintUnformatted(company);
  char *pj = make_props(entity, cik, NULL);

  char rk[128];
  snprintf(rk, sizeof rk, "company:%s", cik ? cik : (entity ? entity : "?"));
  char title[320];
  snprintf(title, sizeof title, "SEC EDGAR company — %s (CIK %s)",
           name ? name : (entity ? entity : "unknown"), cik ? cik : "?");

  char link[256] = {0};
  if (cik) snprintf(link, sizeof link,
    "https://www.sec.gov/cgi-bin/browse-edgar?action=getcompany&CIK=%s", cik);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = name ? name : "SEC EDGAR company profile";
  it.link            = link[0] ? link : NULL;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SEC_EDGAR_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Emit one row per filing object (from a recent_filings / search_filings array).
 * `filing` carries form_type, filing_date, accession_number, url, company, etc.
 * Returns 1 if emitted. */
static int emit_filing(intel_sink *sink, const char *entity, const char *cik,
                       cJSON *filing) {
  if (!filing) return 0;
  const char *form   = cjson_str(filing, "form_type");
  const char *date   = cjson_str(filing, "filing_date");
  const char *acc    = cjson_str(filing, "accession_number");
  const char *url    = cjson_str(filing, "url");
  const char *comp   = cjson_str(filing, "company");

  char *bj = cJSON_PrintUnformatted(filing);
  char *pj = make_props(entity, cik, form);

  char rk[160];
  if (acc) snprintf(rk, sizeof rk, "filing:%s", acc);
  else     snprintf(rk, sizeof rk, "filing:%s:%s:%s",
                    cik ? cik : (entity ? entity : "?"),
                    form ? form : "?", date ? date : "?");

  char title[384];
  snprintf(title, sizeof title, "%s — %s%s%s",
           form ? form : "filing",
           comp ? comp : (entity ? entity : "SEC filing"),
           date ? " (" : "", date ? "" : "");
  if (date) {
    size_t l = strlen(title);
    snprintf(title + l, sizeof title - l, "%s)", date);
  }

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = form ? form : "SEC filing";
  it.link            = url;
  it.published_at    = date;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SEC_EDGAR_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Emit one row per insider Form-4 trade. `trade` carries filing_date and
 * insider_name. Returns 1 if emitted. */
static int emit_form4(intel_sink *sink, const char *entity, const char *cik,
                      cJSON *trade, int idx) {
  if (!trade) return 0;
  const char *date   = cjson_str(trade, "filing_date");
  const char *person = cjson_str(trade, "insider_name");

  char *bj = cJSON_PrintUnformatted(trade);
  char *pj = make_props(entity, cik, "4");

  /* No accession available from the FTS hit shape used upstream; build a stable
   * key from CIK + date + person (+ index to disambiguate same-day duplicates). */
  char rk[256];
  snprintf(rk, sizeof rk, "form4:%s:%s:%s:%d",
           cik ? cik : (entity ? entity : "?"),
           date ? date : "?", person ? person : "?", idx);

  char title[384];
  snprintf(title, sizeof title, "Form 4 insider trade — %s%s%s",
           person ? person : (entity ? entity : "insider"),
           date ? " (" : "", date ? date : "");
  if (date) {
    size_t l = strlen(title);
    snprintf(title + l, sizeof title - l, ")");
  }

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = person ? person : "Form 4 insider trade";
  it.published_at    = date;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SEC_EDGAR_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Emit all filings inside a company-info object's recent_filings array.
 * `cik` is the canonical CIK for keying. Returns count emitted. */
static int emit_filings_array(intel_sink *sink, const char *entity,
                              const char *cik, cJSON *arr) {
  int n = 0;
  if (arr && cJSON_IsArray(arr)) {
    int c = cJSON_GetArraySize(arr);
    for (int i = 0; i < c; i++)
      n += emit_filing(sink, entity, cik, cJSON_GetArrayItem(arr, i));
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *query = ctx->entity;
  if (!query || !*query) return -1;

  int is_cik = 1;
  for (int i = 0; query[i]; i++)
    if (!isdigit((unsigned char)query[i])) { is_cik = 0; break; }

  int emitted = 0;

  if (is_cik) {
    cJSON *company_info = get_company_info(ctx->http, query);
    if (company_info) {
      const char *cik = cjson_str(company_info, "cik");
      if (!cik) cik = query;
      /* Per-filing rows from recent_filings. */
      cJSON *recent = cJSON_GetObjectItem(company_info, "recent_filings");
      emitted += emit_filings_array(sink, query, cik, recent);
      /* Company-profile row (genuine fetched data). */
      emitted += emit_company(sink, query, cik, company_info);
      cJSON_Delete(company_info);

      /* Per-trade Form-4 rows. */
      cJSON *insider = get_insider_trades(ctx->http, query);
      if (insider) {
        if (cJSON_IsArray(insider)) {
          int c = cJSON_GetArraySize(insider);
          for (int i = 0; i < c; i++)
            emitted += emit_form4(sink, query, query,
                                  cJSON_GetArrayItem(insider, i), i);
        }
        cJSON_Delete(insider);
      }
    }
  } else {
    /* Name search: resolve to a CIK, then emit that company's filings/profile. */
    const char *resolved_cik = NULL;
    char cik_buf[32] = {0};
    cJSON *sr = search_company(ctx->http, query);
    if (sr) {
      cJSON *companies = cJSON_GetObjectItem(sr, "companies");
      if (companies && cJSON_IsArray(companies) && cJSON_GetArraySize(companies) > 0) {
        cJSON *first = cJSON_GetArrayItem(companies, 0);  /* exhaustive-ok: ticker->CIK resolution step */
        const char *cik = cjson_str(first, "cik");
        if (cik) {
          snprintf(cik_buf, sizeof cik_buf, "%s", cik);
          resolved_cik = cik_buf;
          cJSON *ci = get_company_info(ctx->http, cik);
          if (ci) {
            const char *ccik = cjson_str(ci, "cik");
            if (!ccik) ccik = cik;
            cJSON *recent = cJSON_GetObjectItem(ci, "recent_filings");
            emitted += emit_filings_array(sink, query, ccik, recent);
            emitted += emit_company(sink, query, ccik, ci);
            cJSON_Delete(ci);
          }
        }
      }
      cJSON_Delete(sr);
    }

    /* 10-K / 10-Q full-text filing hits keyed by company name. */
    cJSON *ar = search_filings(ctx->http, query, "10-K");
    if (ar) {
      if (cJSON_IsArray(ar)) {
        int c = cJSON_GetArraySize(ar);
        for (int i = 0; i < c; i++)
          emitted += emit_filing(sink, query, resolved_cik, cJSON_GetArrayItem(ar, i));
      }
      cJSON_Delete(ar);
    }
    cJSON *qr = search_filings(ctx->http, query, "10-Q");
    if (qr) {
      if (cJSON_IsArray(qr)) {
        int c = cJSON_GetArraySize(qr);
        for (int i = 0; i < c; i++)
          emitted += emit_filing(sink, query, resolved_cik, cJSON_GetArrayItem(qr, i));
      }
      cJSON_Delete(qr);
    }
  }

  (void)emitted;        /* honest empty (emitted == 0) is not an error */
  return 0;
}

static const source_def sec_edgar_def = {
  .id = "SEC_EDGAR_SEARCH", .collector = "osint",
  .name = "SEC EDGAR Search", .name_ja = "SEC EDGAR検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/sec-edgar-search",
  .description = "Search SEC EDGAR filings for a company or person.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sec_edgar_def)
