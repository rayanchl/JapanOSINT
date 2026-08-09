/* soc_webarchive.c — web-archive infrastructure.
 *
 *  commoncrawl-index-catalog (scheduled) GET https://index.commoncrawl.org/collinfo.json
 *      Bare array, newest first. Emits one row per monthly crawl index:
 *      id, name, timegate, cdx-api endpoint, from, to. Element [0].cdx-api is
 *      the index a CDX query should be pointed at today.
 *      licence: Common Crawl Terms of Use allow free reuse including
 *               commercial; keyless.
 *  ARCHIVE_IT_TIMEMAP (entity pivot: domain)
 *      GET https://wayback.archive-it.org/all/timemap/link/http://<domain>/
 *      TRAP: this is NOT JSON — it is RFC 8288 link-format,
 *      `<uri>; rel="memento"; datetime="RFC1123"`. Parsed as such below.
 *      Emits one row per capture (the earliest, plus the most recent 60 of
 *      however many exist) with the memento URI, its RFC1123 datetime, the
 *      rel type, and the capture timestamp decoded from the memento URI's own
 *      14-digit path segment. Total capture count is carried in every row.
 *      A second, independent archive alongside the Wayback Machine: Archive-It
 *      holds curated government/NGO/conflict/election crawls.
 *      licence: public Memento TimeMap interface (RFC 7089) served by the
 *               Internet Archive's Archive-It service; keyless.
 *
 * Neither upstream returns coordinates → has_geo stays 0 (R2).
 */
#include "../../lib/osintemit.h"
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const SOC_UA[] = {
  "User-Agent: JapanOSINT-native/1.0 (OSINT research collector; +https://github.com/)",
  NULL
};

/* ---- commoncrawl-index-catalog ------------------------------------------ */

static int run_cc_catalog(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://index.commoncrawl.org/collinfo.json", SOC_UA, 25000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[commoncrawl-index-catalog] fetch/parse failed\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0, idx = 0;
  const cJSON *c;
  cJSON_ArrayForEach(c, doc) {
    const char *id = jo_sv(c, "id");
    if (!id) continue;
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "index.commoncrawl.org");
    jcopy(p, "id", c, "id");
    jcopy(p, "name", c, "name");
    jcopy(p, "timegate", c, "timegate");
    jcopy(p, "cdx_api", c, "cdx-api");
    jcopy(p, "from", c, "from");
    jcopy(p, "to", c, "to");
    /* the array is newest-first: index 0 is the index to query today */
    cJSON_AddBoolToObject(p, "is_latest", idx == 0);
    char summary[300];
    snprintf(summary, sizeof summary, "%s · crawl %s → %s",
             jo_sv(c, "name") ? jo_sv(c, "name") : "",
             jo_sv(c, "from") ? jo_sv(c, "from") : "?",
             jo_sv(c, "to") ? jo_sv(c, "to") : "?");
    n += soc_emit(sink, p, "commoncrawl-index",
                  "[\"commoncrawl\",\"web-archive\",\"index\"]",
                  id, jo_sv(c, "name") ? jo_sv(c, "name") : id, summary,
                  jo_sv(c, "cdx-api"), jo_sv(c, "from"), NULL);
    idx++;
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[commoncrawl-index-catalog] emitted %d\n", n);
  return 0;
}

static const source_def soc_cc_catalog_def = {
  .id = "commoncrawl-index-catalog", .collector = "social",
  .name = "Common Crawl Index Catalog",
  .update_interval_sec = 86400, .run = run_cc_catalog,
  .category = "media", .type = "api",
  .url = "https://index.commoncrawl.org/collinfo.json",
  .description = "Catalog of every Common Crawl monthly index with its crawl date range and CDX API endpoint — tells a CDX query which index is current instead of a hardcoded one.",
  .license = "Common Crawl Terms of Use permit free reuse including commercial; keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_cc_catalog_def)

/* ---- ARCHIVE_IT_TIMEMAP (entity pivot: domain) -------------------------- */

static int looks_like_domain(const char *s) {
  if (!s || !*s) return 0;
  if (strchr(s, '@') || strchr(s, '/') || strchr(s, ' ') || strchr(s, ':')) return 0;
  size_t n = strlen(s);
  if (n < 3 || n > 253) return 0;
  const char *dot = strrchr(s, '.');
  if (!dot || dot == s || !dot[1]) return 0;
  for (const char *p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (!(isalnum(c) || c == '-' || c == '.')) return 0;
  }
  return isalpha((unsigned char)dot[1]) ? 1 : 0;
}

/* Copy the value of attr="..." out of [from,end). Returns 1 on success. */
static int link_attr(const char *from, const char *end, const char *attr,
                     char *out, size_t n) {
  char pat[32];
  snprintf(pat, sizeof pat, "%s=\"", attr);
  const char *a = strstr(from, pat);
  if (!a || a >= end) return 0;
  a += strlen(pat);
  const char *q = strchr(a, '"');
  if (!q || q > end) return 0;
  size_t len = (size_t)(q - a);
  if (len >= n) len = n - 1;
  memcpy(out, a, len);
  out[len] = 0;
  return 1;
}

/* Wayback memento URIs embed the capture time as 14 digits: .../20051006234532/... */
static int memento_iso(const char *uri, char *out, size_t n) {
  for (const char *p = uri; *p; p++) {
    if (!isdigit((unsigned char)*p)) continue;
    int k = 0;
    while (isdigit((unsigned char)p[k])) k++;
    if (k >= 14) {
      snprintf(out, n, "%.4s-%.2s-%.2sT%.2s:%.2s:%.2sZ",
               p, p + 4, p + 6, p + 8, p + 10, p + 12);
      return 1;
    }
    p += k ? k - 1 : 0;
  }
  (void)n;
  return 0;
}

static int run_archive_it(const source_ctx *ctx, intel_sink *sink) {
  const char *dom = ctx->entity;
  if (!looks_like_domain(dom)) return 0;        /* wrong shape → no-op (R3) */
  char url[400];
  snprintf(url, sizeof url,
           "https://wayback.archive-it.org/all/timemap/link/http://%s/", dom);
  char *txt = feed_get_text(ctx->http, url, 30000);
  if (!txt) {
    fprintf(stderr, "[ARCHIVE_IT_TIMEMAP] no timemap for %s\n", dom);
    return 0;                       /* not archived / 404 → honest empty */
  }

  /* Pass 1: count mementos. RFC 8288: <uri>; rel="..."; datetime="..." */
  long total = 0;
  for (int pass = 0; pass < 2; pass++) {
    long idx = 0;
    int n = 0;
    const char *p = txt;
    while (1) {
      const char *lt = strchr(p, '<');
      if (!lt) break;
      const char *gt = strchr(lt, '>');
      if (!gt) break;
      const char *next = strchr(gt, '<');
      const char *end = next ? next : gt + strlen(gt);
      char rel[64] = {0}, dt[64] = {0};
      link_attr(gt, end, "rel", rel, sizeof rel);
      link_attr(gt, end, "datetime", dt, sizeof dt);
      p = next ? next : end;
      if (!strstr(rel, "memento")) continue;     /* skip original/timegate/self */
      size_t ulen = (size_t)(gt - lt - 1);
      if (ulen == 0 || ulen > 900) continue;
      if (pass == 0) { total++; continue; }
      idx++;
      /* keep the first capture and the 60 most recent */
      if (!(idx == 1 || idx > total - 60)) continue;
      char uri[912];
      snprintf(uri, sizeof uri, "%.*s", (int)ulen, lt + 1);
      char iso[40]; iso[0] = 0;
      memento_iso(uri, iso, sizeof iso);
      cJSON *pr = cJSON_CreateObject();
      if (!pr) continue;
      cJSON_AddStringToObject(pr, "service", "ARCHIVE_IT_TIMEMAP");
      cJSON_AddStringToObject(pr, "source", "wayback.archive-it.org");
      cJSON_AddStringToObject(pr, "entity", dom);
      cJSON_AddStringToObject(pr, "memento", uri);
      if (rel[0]) cJSON_AddStringToObject(pr, "rel", rel);
      if (dt[0])  cJSON_AddStringToObject(pr, "datetime", dt);
      if (iso[0]) cJSON_AddStringToObject(pr, "captured_at", iso);
      cJSON_AddNumberToObject(pr, "total_mementos", (double)total);
      cJSON_AddNumberToObject(pr, "index", (double)idx);
      cJSON_AddBoolToObject(pr, "success", 1);
      char title[400], summary[200], rk[960];
      snprintf(title, sizeof title, "%s archived %s", dom,
               iso[0] ? iso : (dt[0] ? dt : "(unknown date)"));
      snprintf(summary, sizeof summary, "Archive-It capture %ld of %ld · %s",
               idx, total, rel[0] ? rel : "memento");
      snprintf(rk, sizeof rk, "%s", uri);
      n += soc_emit(sink, pr, "osint_service_result",
                    "[\"osint-search\",\"ARCHIVE_IT_TIMEMAP\",\"web-archive\"]",
                    rk, title, summary, uri, iso[0] ? iso : NULL, NULL);
    }
    if (pass == 1)
      fprintf(stderr, "[ARCHIVE_IT_TIMEMAP] emitted %d of %ld mementos for %s\n",
              n, total, dom);
  }
  free(txt);
  return 0;
}

static const source_def soc_archive_it_def = {
  .id = "ARCHIVE_IT_TIMEMAP", .collector = "osint",
  .name = "Archive-It Memento TimeMap",
  .update_interval_sec = 0, .run = run_archive_it,
  .category = "media", .type = "api",
  .url = "https://wayback.archive-it.org/all/timemap/link/",
  .description = "Capture timeline for a domain from Archive-It's curated institutional collections — a second, independent web archive that often holds pages the Internet Archive missed.",
  .license = "Public Memento TimeMap interface (RFC 7089) served by the Internet Archive's Archive-It service; keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_archive_it_def)
