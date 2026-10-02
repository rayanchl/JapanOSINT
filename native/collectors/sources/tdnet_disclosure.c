/* collectors/government/sources/tdnet_disclosure.c
 * Port of server/src/collectors/tdnetDisclosure.js (fetchText + regex rows).
 * TDnet (TSE Timely Disclosure) — today's filings index:
 *   https://www.release.tdnet.info/inbs/I_list_<NNN>_<YYYYMMDD>.html
 * 100 rows per page; every page of the day is walked (see run()).
 * Per <tr>: first <a href="...pdf">title</a> + all <td> stripped cells.
 * uid = tdnet-disclosure|<pdfUrl>  (== intelUid(SOURCE_ID, r.pdfUrl, …);
 * pdfUrl is always non-empty so it wins over the `${ymd}-${i}` fallback). */
#include "source.h"
#include "lib/jocore.h"
#include "lib/feedlib.h"
#include "core/httpclient.h"
#include "lib/htmlparse.h"
#include "lib/seenset.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HOST "https://www.release.tdnet.info"

/* strndup(inner,len) then html_strip; returns malloc'd (caller frees). */
static char *strip_dup(const char *inner, int len) {
  char *raw = strndup(inner, (size_t)len);
  if (!raw) return NULL;
  char *s = html_strip(raw);
  free(raw);
  return s;
}

/* JS String.prototype.slice(0,max) in code points; copies a UTF-8-safe
 * prefix of at most `max` Unicode scalar values into out. */
static void cp_slice(const char *in, size_t max, char *out, size_t outn) {
  size_t cps = 0, w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && cps < max; ) {
    size_t adv = 1;
    if (*p < 0x80) adv = 1;
    else if ((*p >> 5) == 0x6) adv = 2;
    else if ((*p >> 4) == 0xE) adv = 3;
    else if ((*p >> 3) == 0x1E) adv = 4;
    if (w + adv >= outn) break;
    for (size_t k = 0; k < adv && p[k]; k++) out[w++] = (char)p[k];
    p += adv;
    cps++;
  }
  out[w] = 0;
}

/* pdf urls already emitted this run (lib/seenset.h). TDnet lists newest
 * first, so a filing that arrives while the walk is in progress pushes every
 * row one place down: the last row of page k reappears as the first row of
 * page k+1. Without the set it would be emitted twice and fold onto one uid at
 * the sink, which the run line reports as a UID-COLLISION that is not one. */

/* The day's declared total: `<div class="kaijiSum">1～100件&nbsp;/&nbsp;全405件`.
 * Returns -1 when the page carries none (a day with no disclosures). */
static long page_total(const char *html) {
  const char *k = strstr(html, "kaijiSum");
  if (!k) return -1;
  const char *z = strstr(k, "\xE5\x85\xA8");          /* 全 */
  if (!z || z - k > 200) return -1;
  z += 3;
  if (*z < '0' || *z > '9') return -1;
  return strtol(z, NULL, 10);
}

/* The highest I_list_NNN_<ymd>.html page the pager links to (0 if none). Read
 * from every page, so a page that appears mid-walk is still reached. */
static int page_max_linked(const char *html, const char *ymd) {
  char pat[48];
  snprintf(pat, sizeof pat, "_%s.html", ymd);
  int best = 0;
  for (const char *p = strstr(html, "I_list_"); p; p = strstr(p + 7, "I_list_")) {
    const char *d = p + 7;
    if (d[0] < '0' || d[0] > '9' || d[1] < '0' || d[1] > '9' ||
        d[2] < '0' || d[2] > '9' || strncmp(d + 3, pat, strlen(pat)) != 0)
      continue;
    int v = (d[0] - '0') * 100 + (d[1] - '0') * 10 + (d[2] - '0');
    if (v > best) best = v;
  }
  return best;
}

/* Emit every disclosure row on one index page. Returns rows emitted. */
static int emit_page(const char *src, const char *ymd, const char *iso,
                     seen_set *seen, intel_sink *sink) {
  int n = 0;
  const char *cur = src;
  const char *tr_inner; int tr_len;
  while ((cur = html_block(cur, "tr", &tr_inner, &tr_len)) != NULL) {
    char *row = strndup(tr_inner, (size_t)tr_len);
    if (!row) continue;

    /* linkM = first <a href="...pdf">([^<]+)</a> in the block */
    char *pdf_url = NULL, *title = NULL;
    const char *acur = row;
    const char *a_in; int a_len;
    const char *a_after;
    while ((a_after = html_block(acur, "a", &a_in, &a_len)) != NULL) {
      /* opening <a ...> tag = bytes from its '<' up to a_in (just past '>') */
      const char *lt = a_in;
      while (lt > acur && lt[-1] != '<') lt--;
      if (lt > acur) lt--;                 /* include the '<' */
      char tag_attrs[2048];
      size_t tl = (size_t)(a_in - lt);
      if (tl >= sizeof tag_attrs) tl = sizeof tag_attrs - 1;
      memcpy(tag_attrs, lt, tl);
      tag_attrs[tl] = 0;
      char href[1024];
      if (html_attr(tag_attrs, "href", href, sizeof href)) {
        size_t hl = strlen(href);
        if (hl >= 4 && strcasecmp(href + hl - 4, ".pdf") == 0) {
          if (strncmp(href, "http", 4) == 0) {
            pdf_url = strdup(href);
          } else {
            char *u = malloc(strlen(HOST) + 6 + hl + 1);
            if (u) sprintf(u, "%s/inbs/%s", HOST, href);
            pdf_url = u;
          }
          /* title = linkM[2].trim() — inner is [^<]+, strip+trim */
          title = strip_dup(a_in, a_len);
          break;
        }
      }
      acur = a_after;
    }
    if (!pdf_url) { free(title); free(row); continue; }  /* if (!linkM) continue */
    if (!seen_add(seen, pdf_url)) {                       /* already emitted this run */
      free(pdf_url); free(title); free(row); continue;
    }

    /* cells: every <td> in block, html_strip'd */
    cJSON *cells = cJSON_CreateArray();
    const char *tdcur = row;
    const char *td_in; int td_len;
    while ((tdcur = html_block(tdcur, "td", &td_in, &td_len)) != NULL) {
      char *cell = strip_dup(td_in, td_len);
      cJSON_AddItemToArray(cells, cJSON_CreateString(cell ? cell : ""));
      free(cell);
    }

    /* summary = cells.join(' · ').slice(0,240)   (' · ' = sp U+00B7 sp) */
    size_t jl = 1;
    int cn = cJSON_GetArraySize(cells);
    for (int i = 0; i < cn; i++) {
      cJSON *c = cJSON_GetArrayItem(cells, i);
      jl += strlen(cJSON_IsString(c) ? c->valuestring : "");
      if (i + 1 < cn) jl += 4;  /* " \xC2\xB7 " */
    }
    char *joined = malloc(jl);
    char summary[1024];
    summary[0] = 0;
    if (joined) {
      joined[0] = 0;
      for (int i = 0; i < cn; i++) {
        cJSON *c = cJSON_GetArrayItem(cells, i);
        strcat(joined, cJSON_IsString(c) ? c->valuestring : "");
        if (i + 1 < cn) strcat(joined, " \xC2\xB7 ");
      }
      cp_slice(joined, 240, summary, sizeof summary);
      free(joined);
    }

    cJSON *pj = cJSON_CreateObject();          /* {date: ymd, cells: [...]} */
    cJSON_AddStringToObject(pj, "date", ymd);
    cJSON_AddItemToObject(pj, "cells", cells); /* cells now owned by pj */
    char *pjs = cJSON_PrintUnformatted(pj);

    intel_item it = {0};
    it.remote_key = pdf_url;          /* uid = tdnet-disclosure|<pdfUrl> */
    it.title = title;                 /* r.title (linkM[2].trim()) */
    it.summary = summary;
    it.link = pdf_url;
    it.lang = "ja";
    it.published_at = iso;            /* new Date().toISOString() */
    it.properties_json = pjs;
    it.tags_json = "[\"disclosure\",\"tdnet\",\"tse\",\"corporate\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(pjs); cJSON_Delete(pj);      /* frees cells too */
    free(pdf_url); free(title); free(row);
  }
  return n;
}

/* GET one index page. Returns the body, or NULL with *status set (0 = the
 * exchange never completed). A 404 past the last page is the end of the day's
 * list; anything else is a failure the run has to disclose. */
static char *get_page(const source_ctx *ctx, const char *url, long *status) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 12000, 2, &hr);
  *status = rc == 0 ? hr.status : 0;
  char *body = NULL;
  if (rc == 0 && hr.status >= 200 && hr.status < 300 && hr.body) {
    body = hr.body;
    hr.body = NULL;
  }
  http_response_free(&hr);
  return body;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* todayYmd(): UTC YYYYMMDD */
  /* strftime rather than snprintf("%04d%02d%02d", tm_year + 1900, …): tm_year
   * and the clock fields are ints the compiler cannot bound, so those forms can
   * overrun `ymd` and `iso` and -Wformat-truncation says so. strftime is
   * bounded by construction — it writes nothing and returns 0 rather than
   * cutting a date in half, which matters doubly for `ymd` because it is
   * substituted straight into the index URL: a half date would fetch the wrong
   * day's disclosures and look like a normal empty day. The rendering is
   * identical for every year this can see, ".000Z" included (TDnet has no
   * sub-second precision; the literal is what the JS original emitted).
   * gmtime_r's NULL return is checked too; it was not before. */
  time_t now = time(NULL);
  struct tm g;
  char ymd[16];
  char iso[40];
  if (!gmtime_r(&now, &g) ||
      !strftime(ymd, sizeof ymd, "%Y%m%d", &g) ||
      !strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%S.000Z", &g)) {
    fprintf(stderr, "[tdnet-disclosure] cannot render today as a date\n");
    return -1;
  }

  /* TDnet serves a day's index 100 rows per page — I_list_001_<ymd>.html,
   * I_list_002_…, … — and page 001 states the day's total ("全405件") and
   * links every other page. This used to fetch 001 only, so on 2026-09-30 it
   * stored 100 of 405 disclosures and said nothing: the 101st onwards were
   * lost on exactly the busy days (earnings, buybacks, M&A) that matter most.
   * Every page is walked now. The page count is read from the server — its
   * pager links and its declared total — and re-read on every page, so a page
   * that appears mid-walk is still fetched; the three-digit page number in
   * the URL is the only ceiling. */
  seen_set seen = {0};
  int n = 0, failed = 0, last_page = 1;
  long total = -1, fail_status = 0;
  for (int pg = 1; pg <= last_page && pg <= 999; pg++) {
    char url[128];
    snprintf(url, sizeof url, "%s/inbs/I_list_%03d_%s.html", HOST, pg, ymd);
    long status = 0;
    char *html = get_page(ctx, url, &status);
    if (!html) {
      if (pg == 1) {
        /* No index at all is a failed run, not an empty day — an empty day is
         * a 200 that says "に開示された情報はありません". */
        fprintf(stderr, "[tdnet-disclosure] %s: status=%ld\n", url, status);
        seen_free(&seen);
        return -1;
      }
      fprintf(stderr, "[tdnet-disclosure] page %d failed: status=%ld\n", pg, status);
      failed++;
      fail_status = status;
      continue;                       /* later pages are still worth having */
    }
    long t = page_total(html);
    if (t > total) total = t;
    int linked = page_max_linked(html, ymd);
    if (linked > last_page) last_page = linked;
    if (total > 0 && (total + 99) / 100 > last_page) last_page = (int)((total + 99) / 100);
    n += emit_page(html, ymd, iso, &seen, sink);
    free(html);
  }
  seen_free(&seen);

  if (failed || (total >= 0 && n < total)) {
    char scope[48], ep[96], reason[320];
    snprintf(scope, sizeof scope, "I_list_%s", ymd);
    snprintf(ep, sizeof ep, "%s/inbs/I_list_NNN_%s.html", HOST, ymd);
    if (failed)
      snprintf(reason, sizeof reason,
               "%d of %d index pages for %s could not be fetched (last status %ld); "
               "the rows on them are missing from this run",
               failed, last_page, ymd, fail_status);
    else
      snprintf(reason, sizeof reason,
               "TDnet declared %ld disclosures for %s but only %d rows carried "
               "a PDF link to key on", total, ymd, n);
    jo_trunc_notice_scoped(sink, "tdnet-disclosure", scope, ep, n, total, reason,
                           "re-run; a page that failed is fetched again on the "
                           "next 30-minute pass");
  }
  fprintf(stderr, "[tdnet-disclosure] emitted %d of %ld over %d page(s)%s\n",
          n, total, last_page, failed ? " (some pages failed)" : "");
  return 0;
}

static const source_def tdnet_disclosure_def = {
  .id = "tdnet-disclosure", .collector = "government",
  .name = "TDnet Timely Disclosure", .name_ja = "TDnet 適時開示",
   .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(tdnet_disclosure_def)
