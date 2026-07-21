/* collectors/government/sources/kanpo_gazette.c
 * 官報 Official Gazette — National Printing Bureau site (kanpo.go.jp), which now
 * renders each daily issue as HTML. We scrape the daily 目次 (table of contents)
 * — real structured entries (title + link), no PDF parsing. One intel_item per
 * notice; high OSINT value: 破産/免責 (bankruptcy/discharge), 相続/失踪/公示催告
 * (inheritance/disappearance/public summons), 会社 dissolutions, naturalizations,
 * government procurement notices. Fetch failure / no entries → emits nothing
 * (honest empty — no fabricated notices).
 *
 * Flow: top page → latest issue dates (YYYYMMDD.fullcontents.html) → for each
 * recent date, parse the contents page's <a href="...f.html"><span class="text">
 * TITLE</span></a> entries. uid = kanpo|sha1(date|href|title). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DATES   2
#define MAX_PER_DAY 300

/* Collect up to MAX_DATES unique YYYYMMDD strings appearing before
 * ".fullcontents.html" on the top page (newest first as listed). */
static int top_dates(const char *html, char dates[][9]) {
  int n = 0;
  const char *p = html;
  const char *needle = ".fullcontents.html";
  while (n < MAX_DATES && (p = strstr(p, needle)) != NULL) {
    /* 8 digits immediately precede the needle: ".../YYYYMMDD.fullcontents" */
    if (p - html >= 8) {
      const char *d = p - 8; int ok = 1;
      for (int i = 0; i < 8; i++) if (!isdigit((unsigned char)d[i])) { ok = 0; break; }
      if (ok) {
        char buf[9]; memcpy(buf, d, 8); buf[8] = 0;
        int dup = 0; for (int k = 0; k < n; k++) if (!strcmp(dates[k], buf)) dup = 1;
        if (!dup) { memcpy(dates[n], buf, 9); n++; }
      }
    }
    p += strlen(needle);
  }
  return n;
}

/* Copy title text from the first `class="text"` span that follows `from`
 * within `window` bytes: ...class="text">TITLE<... → out. Returns 1/0. */
static int next_title(const char *from, size_t window, char *out, size_t cap) {
  const char *m = strstr(from, "class=\"text\"");
  if (!m || (size_t)(m - from) > window) return 0;
  const char *gt = strchr(m, '>'); if (!gt) return 0;
  gt++;
  const char *lt = strchr(gt, '<'); if (!lt) return 0;
  size_t len = (size_t)(lt - gt);
  if (len == 0 || len >= cap) { if (len >= cap) len = cap - 1; else return 0; }
  memcpy(out, gt, len); out[len] = 0;
  /* trim surrounding whitespace */
  while (out[0] && (unsigned char)out[0] <= ' ') memmove(out, out+1, strlen(out));
  size_t l = strlen(out); while (l && (unsigned char)out[l-1] <= ' ') out[--l] = 0;
  return out[0] != 0;
}

static int scrape_date(const source_ctx *ctx, intel_sink *sink, const char *ymd) {
  char url[256];
  snprintf(url, sizeof url, "https://www.kanpo.go.jp/%s/%s.fullcontents.html", ymd, ymd);
  char *html = feed_get_text(ctx->http, url, 25000);
  if (!html) return 0;

  char iso[11];
  snprintf(iso, sizeof iso, "%c%c%c%c-%c%c-%c%c",
           ymd[0],ymd[1],ymd[2],ymd[3], ymd[4],ymd[5], ymd[6],ymd[7]);

  int emitted = 0;
  const char *p = html;
  while (emitted < MAX_PER_DAY && (p = strstr(p, "<a href=\"")) != NULL) {
    p += 9;
    const char *e = strchr(p, '"');
    if (!e) break;
    size_t hl = (size_t)(e - p);
    char href[256];
    if (hl == 0 || hl >= sizeof href) { p = e + 1; continue; }
    memcpy(href, p, hl); href[hl] = 0;
    const char *after = e + 1;
    p = after;

    /* gazette entries are relative links starting with the date and ending in
     * f.html; nav/extern links start with '/', 'http', 'javascript'. */
    if (!isdigit((unsigned char)href[0]) || !strstr(href, "f.html")) continue;
    if (strstr(href, ".fullcontents.html")) continue;
    if (strstr(href, "0000f.html")) continue;          /* section index page */

    char title[512];
    if (!next_title(after, 600, title, sizeof title)) continue;

    /* section code = chars after YYYYMMDD up to '/' (e.g. "h01720") */
    char code[24] = {0};
    if (strlen(href) > 8) {
      const char *c = href + 8; int j = 0;
      while (*c && *c != '/' && j < (int)sizeof code - 1) code[j++] = *c++;
      code[j] = 0;
    }

    char link[320];
    snprintf(link, sizeof link, "https://www.kanpo.go.jp/%s/%s", ymd, href);

    char uid[21];
    const char *parts[3] = { iso, href, title };
    feed_hash_key(uid, parts, 3);
    char uidf[64]; snprintf(uidf, sizeof uidf, "kanpo|%s", uid);

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "edition_code", code);
    cJSON_AddStringToObject(pr, "issue_date", iso);
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    intel_item it = {0};
    it.uid             = uidf;
    it.title           = title;
    it.lang            = "ja";
    it.published_at    = iso;
    it.link            = link;
    it.record_type     = "kanpo-notice";
    it.properties_json = pj;
    it.tags_json       = "[\"kanpo\",\"official-gazette\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);
  }
  free(html);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *top = feed_get_text(ctx->http, "https://www.kanpo.go.jp/", 25000);
  if (!top) { fprintf(stderr, "[kanpo] top page fetch failed\n"); return -1; }
  char dates[MAX_DATES][9];
  int nd = top_dates(top, dates);
  free(top);
  if (nd == 0) { fprintf(stderr, "[kanpo] no issue dates found\n"); return -1; }

  int total = 0;
  for (int i = 0; i < nd; i++) total += scrape_date(ctx, sink, dates[i]);
  fprintf(stderr, "[kanpo] emitted %d (dates=%d)\n", total, nd);
  return total > 0 ? 0 : -1;
}

static const source_def kanpo_gazette_def = {
  .id = "kanpo-gazette", .collector = "government",
  .name = "Official Gazette (Kanpo)", .name_ja = "官報",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "scraped",
  .url = "https://www.kanpo.go.jp/",
  .description = "Daily Official Gazette notices (bankruptcy/inheritance/court/company/procurement)",
  .license = "National Printing Bureau", .free_tier = 1,
};
REGISTER_SOURCE(kanpo_gazette_def)
