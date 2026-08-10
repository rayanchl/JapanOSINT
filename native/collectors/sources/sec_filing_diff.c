/* collectors/sources/sec_filing_diff.c
 * OSINT service — SEC_RISK_FACTOR_DIFF. Entity pivot on a ticker, CIK or
 * company name. Fetches a registrant's two most recent 10-Ks, extracts Item 1A
 * "Risk Factors" from each, and emits ONE row carrying a real unified diff.
 *
 * What changed in a company's own account of what could go wrong is a signal
 * the fleet had no way to produce: the filings were already collected
 * (sec_edgar.c), but nothing compared them.
 *
 * WHY THIS DOES NOT USE THE AUTOMATIC content_change PATH
 *
 * core/content_change.c diffs the SAME URL over time, keyed (source_id,
 * ref_key), and fires from the httpclient hook. A year-over-year 10-K
 * comparison is two DIFFERENT documents fetched in one run, so that machinery
 * cannot express it. What it can lend is the hard part: content_change.h
 * exposes the normalizer and the differ as PURE functions — no database, no
 * thread-local scope, malloc'd results the caller frees. Reusing them means
 * this collector inherits the strip list (dates, epochs, opaque tokens, view
 * counters, cache-busting URL params) that is the entire reason a naive 10-K
 * diff is unreadable, and inherits the bounded LCS rather than reimplementing
 * one. See core/content_change.h's header for what normalization removes.
 *
 * Endpoints (all keyless):
 *   https://www.sec.gov/files/company_tickers.json          ticker/name → CIK
 *   https://data.sec.gov/submissions/CIK<10-digit>.json     filing index
 *   https://www.sec.gov/Archives/edgar/data/<cik>/<acc>/<doc>   the filings
 *
 * SEC USER-AGENT — VERIFIED, DO NOT "SIMPLIFY" TO JO_USER_AGENT.
 * SEC's automated-access policy wants "Sample Company Name AdminContact@domain"
 * and enforces it: measured 2026-08-09, the fleet-wide JO_USER_AGENT gets 403
 * ("Your Request Originates from an Undeclared Automated Tool") on both
 * www.sec.gov and data.sec.gov, while a UA containing an email address gets
 * 200. So this collector sends its own, overridable via SEC_CONTACT_UA so an
 * operator can put a real contact address in without a rebuild.
 *
 * No Accept-Encoding header is set: core/httpclient.c already passes
 * CURLOPT_ACCEPT_ENCODING "" (httpclient.c:246), which advertises every codec
 * curl can decode AND decodes the reply. Setting the header by hand instead
 * asks for gzip without arranging to decompress it.
 *
 * Licence: US government work, public domain. Rate limit: SEC asks for <= 10
 * req/s; this makes at most 4 requests per pivot and hostgate serialises them.
 *
 * R2: a filing carries no coordinates, so no row here sets has_geo. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../core/content_change.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default contact UA — the exact string collectors/sources/sec_edgar.c already
 * uses, because it is the one measured to work. SEC's filter is fussier than
 * "contains an email": on 2026-08-09,
 *     "OSINTsaas/1.0 (contact@example.com)"                      -> 200
 *     "OSINTsaas Research contact@osintsaas.example"             -> 200
 *     "OSINTsaas/1.0 (…@users.noreply.github.com)"               -> 403
 * so do not "improve" this string without re-measuring both www.sec.gov and
 * data.sec.gov. Operators SHOULD set SEC_CONTACT_UA to a real contact address:
 * SEC's automated-access policy asks for one, and the default is a placeholder
 * inherited for compatibility, not a real mailbox. */
#define SEC_DEFAULT_UA "OSINTsaas/1.0 (contact@example.com)"

/* Build the SEC request headers into caller-owned storage. `ua` must be at
 * least 256 bytes; `out` is a 3-slot array left NULL-terminated. */
static void sec_headers(char *ua, size_t uacap, const char *out[3]) {
  const char *env = jo_env("SEC_CONTACT_UA");
  snprintf(ua, uacap, "User-Agent: %s", env ? env : SEC_DEFAULT_UA);
  out[0] = ua;
  out[1] = "Accept: application/json";
  out[2] = NULL;
}

/* Pad a CIK to the 10 digits data.sec.gov's submissions path requires.
 * Written with memset/memcpy rather than snprintf("%0*d%s") because the
 * variable field width defeats -Wformat-truncation: the compiler cannot see
 * that width and digit count always sum to exactly 10, and warns. `out` needs
 * 11 bytes; a shorter buffer or a digit-free input yields the empty string. */
static void sfd_pad_cik(const char *cik, char *out, size_t cap) {
  const char *p = cik;
  while (*p && !isdigit((unsigned char)*p)) p++;
  while (*p == '0' && p[1]) p++;                 /* strip leading zeros */
  char digits[11];
  size_t n = 0;
  for (; *p && isdigit((unsigned char)*p) && n < 10; p++) digits[n++] = *p;
  digits[n] = 0;
  if (cap < 11 || n == 0) { if (cap) out[0] = 0; return; }
  memset(out, '0', 10 - n);
  memcpy(out + (10 - n), digits, n);
  out[10] = 0;
}

/* Resolve a ticker or company name to a CIK via SEC's own mapping file. The
 * file is an object keyed by row index: {"0":{"cik_str":320193,"ticker":"AAPL",
 * "title":"Apple Inc."}, ...}. Exact ticker match wins; otherwise the first
 * case-insensitive substring hit on the title.
 *
 * Returns 1 on a match, 0 when the file was read but nothing matched, and -1
 * when the fetch itself failed. The three are kept distinct on purpose: when
 * they were collapsed into 0, a 403 from SEC reported itself as "no registrant
 * matched AAPL", which is a lie about the upstream and cost real debugging
 * time. R3's distinction between "fetched fine, found nothing" and "could not
 * fetch" has to survive every layer, not just the top one. */
static int sfd_resolve(const source_ctx *ctx, const char *const *hdrs,
                       const char *entity,
                       char *cik_out, size_t cap, char *name_out, size_t ncap) {
  cJSON *root = feed_get_json_h(ctx->http,
      "https://www.sec.gov/files/company_tickers.json", hdrs, 30000);
  if (!root) {
    fprintf(stderr, "[sec-risk-diff] ticker map fetch failed "
                    "(SEC 403s an unrecognised User-Agent; see SEC_CONTACT_UA)\n");
    return -1;
  }

  int found = 0;
  cJSON *row = NULL;
  /* Pass 1: exact ticker. Pass 2: title substring. */
  for (int pass = 0; pass < 2 && !found; pass++) {
    cJSON_ArrayForEach(row, root) {
      const char *tk = jo_sv(row, "ticker");
      const char *ti = jo_sv(row, "title");
      double cik = 0;
      if (!jo_num(row, "cik_str", &cik)) continue;
      int hit = 0;
      if (pass == 0) hit = (tk && strcasecmp(tk, entity) == 0);
      else           hit = (ti && jo_stristr(ti, entity) != NULL);
      if (!hit) continue;
      snprintf(cik_out, cap, "%ld", (long)cik);
      if (ti) snprintf(name_out, ncap, "%s", ti);
      else if (tk) snprintf(name_out, ncap, "%s", tk);
      found = 1;
      break;
    }
  }
  cJSON_Delete(root);
  return found;
}

/* One 10-K: the accession (no dashes), primary document name and filing date. */
typedef struct { char acc[32], doc[128], date[16], report[16]; } sfd_filing;

/* Fill up to `max` most-recent 10-K entries from the submissions index.
 * filings.recent is a struct-of-arrays: form[], accessionNumber[],
 * primaryDocument[], filingDate[], reportDate[] — all index-aligned, newest
 * first. Returns how many were filled. */
static int sfd_recent_10k(cJSON *subs, sfd_filing *out, int max) {
  cJSON *recent = cJSON_GetObjectItem(cJSON_GetObjectItem(subs, "filings"), "recent");
  cJSON *form = cJSON_GetObjectItem(recent, "form");
  cJSON *acc  = cJSON_GetObjectItem(recent, "accessionNumber");
  cJSON *pdoc = cJSON_GetObjectItem(recent, "primaryDocument");
  cJSON *fdat = cJSON_GetObjectItem(recent, "filingDate");
  cJSON *rdat = cJSON_GetObjectItem(recent, "reportDate");
  if (!cJSON_IsArray(form) || !cJSON_IsArray(acc) || !cJSON_IsArray(pdoc))
    return 0;

  int n = 0, total = cJSON_GetArraySize(form);
  for (int i = 0; i < total && n < max; i++) {
    const char *f = jo_vstr(cJSON_GetArrayItem(form, i));
    /* Exact "10-K" only: 10-K/A is an amendment and 10-KSB a different form,
     * and diffing across form types compares documents with different shapes. */
    if (!f || strcmp(f, "10-K") != 0) continue;
    const char *a = jo_vstr(cJSON_GetArrayItem(acc, i));
    const char *d = jo_vstr(cJSON_GetArrayItem(pdoc, i));
    if (!a || !d || !d[0]) continue;

    sfd_filing *s = &out[n];
    size_t j = 0;
    for (const char *p = a; *p && j < sizeof s->acc - 1; p++)
      if (*p != '-') s->acc[j++] = *p;
    s->acc[j] = 0;
    snprintf(s->doc, sizeof s->doc, "%s", d);
    const char *fd = jo_vstr(cJSON_GetArrayItem(fdat, i));
    const char *rd = jo_vstr(cJSON_GetArrayItem(rdat, i));
    snprintf(s->date, sizeof s->date, "%s", fd ? fd : "");
    snprintf(s->report, sizeof s->report, "%s", rd ? rd : "");
    n++;
  }
  return n;
}

/* Slice the Risk Factors section out of already-NORMALIZED plain text.
 *
 * Slicing the HTML instead would mean writing a second markup parser; slicing
 * the normalized text means the section boundaries are plain words on their own
 * lines. The complication is the table of contents, which contains the same
 * "Item 1A" heading a few hundred bytes before the real one. So: consider every
 * occurrence of "item 1a", pair it with the next following "item 1b" (or
 * "item 2" when 1B is absent, which some filers omit), and keep the LONGEST
 * span. The TOC pair is a handful of bytes; the real one is the section.
 *
 * Returns a malloc'd slice (caller frees), or NULL when no plausible pair
 * exists — in which case the caller diffs the whole document and says so. */
static char *sfd_risk_section(const char *text) {
  if (!text) return NULL;
  const char *best_s = NULL, *best_e = NULL;
  size_t best_len = 0;

  for (const char *p = text; (p = jo_stristr(p, "item 1a")) != NULL; p += 7) {
    const char *e1 = jo_stristr(p + 7, "item 1b");
    const char *e2 = jo_stristr(p + 7, "item 2");
    const char *e = e1;
    if (!e || (e2 && e2 < e)) e = e2;
    if (!e) continue;
    size_t len = (size_t)(e - p);
    if (len > best_len) { best_len = len; best_s = p; best_e = e; }
  }
  /* Under ~2 KB this is a contents entry or a cross-reference, not the section. */
  if (!best_s || best_len < 2048) return NULL;

  char *out = malloc(best_len + 1);
  if (!out) return NULL;
  memcpy(out, best_s, best_len);
  out[best_len] = 0;
  (void)best_e;
  return out;
}

/* Fetch one filing document and return its normalized text (caller frees). */
static char *sfd_fetch_norm(const source_ctx *ctx, const char *const *hdrs,
                            const char *cik, const sfd_filing *f,
                            char *url_out, size_t ucap) {
  snprintf(url_out, ucap, "https://www.sec.gov/Archives/edgar/data/%s/%s/%s",
           cik, f->acc, f->doc);
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url_out, hdrs, NULL, 0, 45000, 1, &hr);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[sec-risk-diff] doc status=%ld %s\n", hr.status, url_out);
    http_response_free(&hr);
    return NULL;
  }
  char *norm = content_change_normalize(hr.body, hr.body_len, "text/html");
  http_response_free(&hr);
  return norm;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  if (!jo_looks_like_keyword(ctx->entity)) return 0;

  char ua[256];
  const char *hdrs[3];
  sec_headers(ua, sizeof ua, hdrs);

  /* Resolve the entity to a CIK. A bare number is already one. */
  char cik[32] = {0}, name[256] = {0};
  int numeric = 1;
  for (const char *p = ctx->entity; *p; p++)
    if (!isdigit((unsigned char)*p)) { numeric = 0; break; }
  if (numeric) {
    snprintf(cik, sizeof cik, "%s", ctx->entity);
  } else {
    int r = sfd_resolve(ctx, hdrs, ctx->entity, cik, sizeof cik, name, sizeof name);
    if (r < 0) return -1;                       /* upstream unreachable (R3) */
    if (r == 0) {
      fprintf(stderr, "[sec-risk-diff] no registrant matched '%s'\n", ctx->entity);
      return 0;                                 /* honest empty, no row (R1) */
    }
  }

  char padded[16];
  sfd_pad_cik(cik, padded, sizeof padded);
  char subs_url[256];
  snprintf(subs_url, sizeof subs_url,
           "https://data.sec.gov/submissions/CIK%s.json", padded);
  cJSON *subs = feed_get_json_h(ctx->http, subs_url, hdrs, 30000);
  if (!subs) { fprintf(stderr, "[sec-risk-diff] submissions fetch failed\n"); return -1; }
  if (!name[0]) {
    const char *nm = jo_sv(subs, "name");
    if (nm) snprintf(name, sizeof name, "%s", nm);
  }

  sfd_filing f[2];
  memset(f, 0, sizeof f);
  int nf = sfd_recent_10k(subs, f, 2);
  cJSON_Delete(subs);
  if (nf < 2) {
    fprintf(stderr, "[sec-risk-diff] only %d 10-K(s) on file for CIK %s\n", nf, cik);
    return 0;                                   /* nothing to compare against */
  }

  /* f[0] is the newer filing (submissions.recent is newest-first). */
  char new_url[512], old_url[512];
  char *new_txt = sfd_fetch_norm(ctx, hdrs, cik, &f[0], new_url, sizeof new_url);
  char *old_txt = new_txt ? sfd_fetch_norm(ctx, hdrs, cik, &f[1], old_url, sizeof old_url)
                          : NULL;
  if (!new_txt || !old_txt) {
    free(new_txt); free(old_txt);
    return -1;                                  /* a real fetch failure (R3) */
  }

  /* Prefer the Item 1A slice; fall back to the whole document, labelled. */
  char *new_sec = sfd_risk_section(new_txt);
  char *old_sec = sfd_risk_section(old_txt);
  const char *scope = "item-1a";
  const char *a = new_sec, *b = old_sec;
  if (!new_sec || !old_sec) { scope = "full-document"; a = new_txt; b = old_txt; }

  int added = 0, removed = 0, truncated = 0;
  char label[128];
  snprintf(label, sizeof label, "10-K %s", scope);
  char *diff = content_change_diff(label, b, a, &added, &removed, &truncated);

  int emitted = 0;
  if (!diff) {
    fprintf(stderr, "[sec-risk-diff] %s unchanged between %s and %s\n",
            scope, f[1].date, f[0].date);
  } else {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "cik", cik);
    if (name[0]) cJSON_AddStringToObject(data, "registrant", name);
    cJSON_AddStringToObject(data, "scope", scope);
    cJSON_AddStringToObject(data, "form", "10-K");
    cJSON_AddStringToObject(data, "current_filed", f[0].date);
    cJSON_AddStringToObject(data, "prior_filed", f[1].date);
    if (f[0].report[0]) cJSON_AddStringToObject(data, "current_period", f[0].report);
    if (f[1].report[0]) cJSON_AddStringToObject(data, "prior_period", f[1].report);
    cJSON_AddStringToObject(data, "current_url", new_url);
    cJSON_AddStringToObject(data, "prior_url", old_url);
    cJSON_AddNumberToObject(data, "lines_added", added);
    cJSON_AddNumberToObject(data, "lines_removed", removed);
    if (truncated) cJSON_AddBoolToObject(data, "diff_truncated", 1);
    cJSON_AddNumberToObject(data, "norm_version", CC_NORM_VERSION);
    cJSON_AddStringToObject(data, "diff", diff);

    /* The body is a bounded excerpt, not the whole diff: core/intel.c runs the
     * FTS mirror (MeCab) over `body`, and a 64 KiB diff per row makes that the
     * dominant cost of the run. The full diff stays in properties. */
    char *excerpt = strdup(diff);
    if (excerpt) jo_utf8_trunc(excerpt, 4096);

    char rk[256], title[420];
    snprintf(rk, sizeof rk, "riskdiff:%s:%s:%s", cik, f[1].acc, f[0].acc);
    snprintf(title, sizeof title,
             "10-K risk factors changed — %s (%s vs %s): +%d/-%d lines",
             name[0] ? name : cik, f[0].date, f[1].date, added, removed);

    char *pj = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);
    if (pj) {
      intel_item it = {0};
      it.remote_key      = rk;
      it.title           = title;
      it.summary         = scope;
      it.link            = new_url;
      it.published_at    = f[0].date[0] ? f[0].date : NULL;
      it.lang            = "en";
      it.record_type     = "filing-diff";
      it.body            = excerpt ? excerpt : pj;
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"sec-filing\",\"filing-diff\"]";
      if (sink->emit(sink, &it) >= 0) emitted = 1;
      free(pj);
    }
    free(excerpt);
    free(diff);
  }

  free(new_sec); free(old_sec);
  free(new_txt); free(old_txt);
  fprintf(stderr, "[sec-risk-diff] emitted %d\n", emitted);
  return 0;
}

static const source_def sec_risk_diff_def = {
  .id = "SEC_RISK_FACTOR_DIFF", .collector = "osint",
  .name = "SEC 10-K Risk Factor Diff", .name_ja = "SEC 10-K リスク要因 差分",
  .update_interval_sec = 0, .run = run,
  .category = "economy", .type = "api",
  .url = "internal://osint/sec-risk-factor-diff",
  .description = "Unified diff of Item 1A 'Risk Factors' between a registrant's two "
                 "most recent 10-K filings, resolved from a ticker, CIK or company "
                 "name. Keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(sec_risk_diff_def)
