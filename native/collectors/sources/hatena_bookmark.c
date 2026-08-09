/* collectors/social/sources/hatena_bookmark.c
 * Port of server/src/collectors/hatenaBookmark.js. Hatena RDF1.0 hotentry
 * feed; bespoke item scan for title/link/dc:date/hatena:bookmarkcount/
 * dc:subject. uid = hatena-bookmark|<link> (== intelUid(SOURCE_ID,link,...)).
 * summary "<n> bookmarks · <subject>" reproduced for parity. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* inner text of first <tag ...>...</tag> in [b,e); CDATA + basic entities. */
static char *tag_in(const char *b, const char *e, const char *tag) {
  char op[64]; snprintf(op, sizeof op, "<%s", tag);
  const char *p = b;
  while (p < e) {
    const char *q = strcasestr(p, op);
    if (!q || q >= e) return NULL;
    const char *gt = memchr(q, '>', (size_t)(e - q));
    if (!gt) return NULL;
    char cl[64]; snprintf(cl, sizeof cl, "</%s>", tag);
    const char *c = strcasestr(gt + 1, cl);
    if (!c || c > e) return NULL;
    size_t n = (size_t)(c - gt - 1);
    char *s = malloc(n + 1); memcpy(s, gt + 1, n); s[n] = 0;
    char *t = s;
    if (!strncmp(t, "<![CDATA[", 9)) {
      char *z = strstr(t, "]]>"); if (z) { *z = 0; t += 9; }
    }
    while (*t==' '||*t=='\n'||*t=='\r'||*t=='\t') t++;
    size_t L = strlen(t);
    while (L && (t[L-1]==' '||t[L-1]=='\n'||t[L-1]=='\r'||t[L-1]=='\t')) t[--L]=0;
    char *o = t;
    for (char *r = t; *r; ) {
      if (r[0]=='&' && r[1]=='#') {            /* numeric char ref → UTF-8 */
        char *semi = strchr(r, ';');
        if (semi && semi - r <= 9) {
          long cp = (r[2]=='x'||r[2]=='X')
                    ? strtol(r+3, NULL, 16) : strtol(r+2, NULL, 10);
          if (cp > 0) {
            if (cp < 0x80) *o++=(char)cp;
            else if (cp < 0x800) { *o++=(char)(0xC0|(cp>>6)); *o++=(char)(0x80|(cp&0x3F)); }
            else if (cp < 0x10000) { *o++=(char)(0xE0|(cp>>12)); *o++=(char)(0x80|((cp>>6)&0x3F)); *o++=(char)(0x80|(cp&0x3F)); }
            else { *o++=(char)(0xF0|(cp>>18)); *o++=(char)(0x80|((cp>>12)&0x3F)); *o++=(char)(0x80|((cp>>6)&0x3F)); *o++=(char)(0x80|(cp&0x3F)); }
            r = semi + 1; continue;
          }
        }
        *o++=*r++;
      }
      else if(!strncmp(r,"&amp;",5)){*o++='&';r+=5;}
      else if(!strncmp(r,"&lt;",4)){*o++='<';r+=4;}
      else if(!strncmp(r,"&gt;",4)){*o++='>';r+=4;}
      else if(!strncmp(r,"&quot;",6)){*o++='"';r+=6;}
      else if(!strncmp(r,"&apos;",6)){*o++='\'';r+=6;}
      else *o++=*r++;
    }
    *o=0;
    char *res = strdup(t); free(s); return res;
  }
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *xml = feed_get_text(ctx->http, "https://b.hatena.ne.jp/hotentry.rss", 15000);
  if (!xml) return -1;
  const char *xe = xml + strlen(xml);
  int n = 0;
  const char *cur = xml;
  for (;;) {
    const char *it = strcasestr(cur, "<item");
    if (!it || it >= xe) break;
    const char *end = strcasestr(it, "</item>");
    if (!end) break;
    char *title = tag_in(it, end, "title");
    char *link  = tag_in(it, end, "link");
    char *date  = tag_in(it, end, "dc:date");
    char *bmc   = tag_in(it, end, "hatena:bookmarkcount");
    char *subj  = tag_in(it, end, "dc:subject");
    if (link && *link) {
      long bm = bmc ? strtol(bmc, NULL, 10) : 0;
      char summ[256];
      if (subj && *subj) snprintf(summ, sizeof summ, "%ld bookmarks · %s", bm, subj);
      else snprintf(summ, sizeof summ, "%ld bookmarks", bm);
      cJSON *pj = cJSON_CreateObject();
      cJSON_AddNumberToObject(pj, "bookmarks", (double)bm);
      if (subj && *subj) cJSON_AddStringToObject(pj, "subject", subj);
      else cJSON_AddNullToObject(pj, "subject");
      char *pjs = cJSON_PrintUnformatted(pj);
      char tags[160];
      if (subj && *subj)
        snprintf(tags, sizeof tags, "[\"hatena\",\"trending\",\"cat:%s\"]", subj);
      else snprintf(tags, sizeof tags, "[\"hatena\",\"trending\"]");
      intel_item item = {0};
      item.remote_key = link;            /* uid = hatena-bookmark|<link> */
      item.title = title;
      item.summary = summ;
      item.link = link;
      item.lang = "ja";
      item.published_at = date;
      item.record_type = "hatena-bookmark";
      item.properties_json = pjs;
      item.tags_json = tags;
      if (sink->emit(sink, &item) >= 0) n++;
      free(pjs); cJSON_Delete(pj);
    }
    free(title); free(link); free(date); free(bmc); free(subj);
    cur = end + 7;
  }
  free(xml);
  fprintf(stderr, "[hatena-bookmark] emitted %d\n", n);
  /* run() is a STATUS code, not a row count: fetch/parse failures already
   * returned -1 above, so reaching here with zero rows is an honest empty.
   * Returning -1 here had scheduler.c quarantine the source for working. */
  return 0;
}

static const source_def hatena_bookmark_def = {
  .id = "hatena-bookmark", .collector = "social",
  .name = "Hatena Bookmark", .name_ja = "はてなブックマーク",
   .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(hatena_bookmark_def)
