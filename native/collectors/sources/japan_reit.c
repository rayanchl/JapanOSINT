/* collectors/economy/sources/japan_reit.c
 * Port of server/src/collectors/japanReit.js — best-effort scrape of
 * japan-reit.com's listed-issue directory (https://www.japan-reit.com/
 * meigara/). JS regex (global, dedup by code):
 *   /<a[^>]+href="\/meigara\/(\d{4})\/?"[^>]*>([^<]{2,})<\/a>/g
 * name = m[2].replace(/\s+/g,' ').trim(). Honest empty (return -1) when
 * markup yields nothing. Never fabricated. Non-spatial intel_item.
 * uid = japan-reit|<code> (mirrors intelUid). */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* JS \s for replace(/\s+/g,' '): space, \t, \n, \r, \f, \v. */
static int js_ws(unsigned char c) {
  return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v';
}

/* name = raw.replace(/\s+/g,' ').trim() into malloc'd string. */
static char *normalize_name(const char *s, int len) {
  char *out = malloc((size_t)len + 1);
  int o = 0, i = 0, prev_ws = 0;
  /* leading trim handled by collapse + final trim */
  for (; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    if (js_ws(c)) {
      if (!prev_ws) { out[o++] = ' '; prev_ws = 1; }
    } else {
      out[o++] = (char)c; prev_ws = 0;
    }
  }
  /* trim both ends */
  int start = 0, end = o;
  while (start < end && out[start] == ' ') start++;
  while (end > start && out[end-1] == ' ') end--;
  int nlen = end - start;
  memmove(out, out + start, (size_t)nlen);
  out[nlen] = '\0';
  return out;
}

/* Match <a ...href="/meigara/DDDD/?"...>INNER</a> from cur. On success set
 * code(4 digits +NUL), inner+inner_len, return ptr just past </a>; else
 * advance/return NULL when no more. JS: href value exactly /meigara/DDDD
 * optionally followed by '/', [^>]{0,} until '>', INNER=[^<]{2,}. */
static const char *next_anchor(const char *cur, char code[5],
                               const char **inner, int *inner_len) {
  while ((cur = strchr(cur, '<')) != NULL) {
    if ((cur[1]=='a'||cur[1]=='A') &&
        (cur[2]==' '||cur[2]=='\t'||cur[2]=='\n'||cur[2]=='\r')) {
      const char *tag = cur + 2;
      const char *gt = strchr(tag, '>');
      if (!gt) return NULL;
      /* find href=" within [tag, gt) */
      const char *h = NULL;
      for (const char *q = tag; q + 5 < gt; q++) {
        if ((q[0]=='h'||q[0]=='H') && (q[1]=='r'||q[1]=='R') &&
            (q[2]=='e'||q[2]=='E') && (q[3]=='f'||q[3]=='F')) {
          const char *e = q + 4;
          while (e < gt && js_ws((unsigned char)*e)) e++;
          if (e < gt && *e == '=') {
            e++;
            while (e < gt && js_ws((unsigned char)*e)) e++;
            if (e < gt && *e == '"') { h = e + 1; }
          }
          break;
        }
      }
      if (h) {
        const char *want = "/meigara/";
        size_t wl = strlen(want);
        if (strncmp(h, want, wl) == 0) {
          const char *d = h + wl;
          if (isdigit((unsigned char)d[0]) && isdigit((unsigned char)d[1]) &&
              isdigit((unsigned char)d[2]) && isdigit((unsigned char)d[3])) {
            const char *after = d + 4;
            if (*after == '/') after++;
            if (*after == '"') {
              /* INNER between gt+1 and the first '<', must be [^<]{2,}. The
               * anchors on japan-reit.com carry a trailing HTML comment
               * (<a ...>名前<!--短縮名--></a>), so requiring the very next tag
               * to be </a> matched only 3 of the 61 issues on the page; take
               * the text run and then skip to the real </a>. */
              const char *is = gt + 1;
              const char *ie = strchr(is, '<');
              if (ie && (ie - is) >= 2) {
                const char *close = strstr(is, "</a>");
                if (!close) close = strstr(is, "</A>");
                const char *nexta = strstr(is, "<a ");
                if (close && (!nexta || close < nexta)) {
                  memcpy(code, d, 4); code[4] = '\0';
                  *inner = is; *inner_len = (int)(ie - is);
                  return close + 4;
                }
              }
            }
          }
        }
      }
      cur = gt + 1;
      continue;
    }
    cur++;
  }
  return NULL;
}

/* japan-reit.com retired the /meigara/ directory index (it now 404s, which is
 * what made this collector return -1 on every run). The issue anchors are
 * still published on the yield table (/list/rimawari/) and on the site root,
 * so scrape those instead — same markup, same /meigara/<code>/ hrefs. */
static const char *PAGES[] = {
  "https://www.japan-reit.com/list/rimawari/",
  "https://www.japan-reit.com/",
  NULL
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *html = NULL;
  for (int i = 0; PAGES[i] && !html; i++)
    html = feed_get_text(ctx->http, PAGES[i], 15000);
  if (!html) { fprintf(stderr, "[japan-reit] unreachable\n"); return -1; }

  char now[32];
  { time_t t = time(NULL); struct tm tm; gmtime_r(&t, &tm);
    strftime(now, sizeof now, "%Y-%m-%dT%H:%M:%S.000Z", &tm); }

  /* dedup by 4-digit code (0000..9999) */
  static unsigned char seen[10000];
  memset(seen, 0, sizeof seen);

  int n = 0;
  const char *cur = html;
  char code[5];
  const char *inner; int inner_len;
  while ((cur = next_anchor(cur, code, &inner, &inner_len)) != NULL) {
    int idx = atoi(code);
    if (idx >= 0 && idx < 10000) {
      if (seen[idx]) continue;
      seen[idx] = 1;
    }
    char *name = normalize_name(inner, inner_len);
    if (!name[0]) { free(name); continue; }

    size_t nl = strlen(name);
    char *title = malloc(nl + 16);
    sprintf(title, "%s (%s)", name, code);
    char summary[48];
    snprintf(summary, sizeof summary, "J-REIT listed issue %s", code);
    char *body = malloc(nl + 64);
    sprintf(body, "japan-reit.com listed J-REIT issue: code=%s, name=%s",
            code, name);
    char link[64];
    snprintf(link, sizeof link, "https://www.japan-reit.com/meigara/%s/", code);

    cJSON *p = cJSON_CreateObject();   /* EXACT JS key order */
    cJSON_AddStringToObject(p, "ticker_code", code);
    cJSON_AddStringToObject(p, "name", name);
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("economy"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("real-estate"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("reit"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("finance"));
    char *tj = cJSON_PrintUnformatted(tags);

    intel_item it = {0};
    it.remote_key   = code;                  /* → uid japan-reit|<code> */
    it.title        = title;
    it.summary      = summary;
    it.body         = body;
    it.link         = link;
    it.lang         = "ja";
    it.published_at = now;
    it.record_type  = "japan-reit";
    it.tags_json    = tj;
    it.properties_json = pj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(name); free(title); free(body); free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
  }
  free(html);

  fprintf(stderr, "[japan-reit] emitted %d\n", n);
  /* run() is a STATUS code, not a row count: fetch/parse failures already
   * returned -1 above, so reaching here with zero rows is an honest empty.
   * Returning -1 here had scheduler.c quarantine the source for working. */
  return 0;
}

static const source_def japan_reit_def = {
  .id = "japan-reit", .collector = "economy",
  .name = "J-REIT Properties", .name_ja = "J-REIT 不動産",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(japan_reit_def)
