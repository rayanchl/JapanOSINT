/* collectors/social/sources/hatena_bookmark_extended.c
 * Port of server/src/collectors/hatenaBookmarkExtended.js.
 * Key-free public RSS/RDF. Pulls 8 Hatena hot-entry / entrylist feeds, parses
 * <item> blocks (custom RDF parse mirroring the JS regex parser), dedup by
 * link across feeds, emits non-spatial intel. honest empty on failure.
 * uid = hatena-bookmark-extended|<link>. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SOURCE_ID "hatena-bookmark-extended"

typedef struct { const char *url; const char *cat; } feed_t;
static const feed_t FEEDS[] = {
  { "https://b.hatena.ne.jp/hotentry.rss",            "general"   },
  { "https://b.hatena.ne.jp/hotentry/it.rss",         "it"        },
  { "https://b.hatena.ne.jp/hotentry/social.rss",     "social"    },
  { "https://b.hatena.ne.jp/hotentry/economics.rss",  "economics" },
  { "https://b.hatena.ne.jp/hotentry/life.rss",       "life"      },
  { "https://b.hatena.ne.jp/hotentry/knowledge.rss",  "knowledge" },
  { "https://b.hatena.ne.jp/hotentry/fun.rss",        "fun"       },
  { "https://b.hatena.ne.jp/entrylist.rss",           "recent"    },
};
#define NF ((int)(sizeof(FEEDS)/sizeof(FEEDS[0])))

/* decodeXmlEntities + stripCdata, in place; returns malloc'd decoded copy. */
static char *clean_dup(const char *s, size_t len) {
  if (!s) return NULL;
  /* strip leading <![CDATA[ and trailing ]]> */
  const char *start = s;
  size_t n = len;
  if (n >= 9 && strncmp(start, "<![CDATA[", 9) == 0) { start += 9; n -= 9; }
  if (n >= 3 && strncmp(start + n - 3, "]]>", 3) == 0) n -= 3;

  char *out = malloc(n + 1);
  size_t o = 0;
  for (size_t i = 0; i < n; ) {
    if (start[i] == '&') {
      if (start[i+1] == '#' && (start[i+2] == 'x' || start[i+2] == 'X')) {
        unsigned cp = 0; size_t j = i + 3;
        while (j < n && start[j] != ';') {
          char c = start[j];
          cp = cp * 16 + (c >= '0' && c <= '9' ? c - '0' :
                          (c | 32) - 'a' + 10);
          j++;
        }
        if (j < n) {
          /* UTF-8 encode codepoint */
          if (cp < 0x80) out[o++] = (char)cp;
          else if (cp < 0x800) {
            out[o++] = (char)(0xC0 | (cp >> 6));
            out[o++] = (char)(0x80 | (cp & 0x3F));
          } else if (cp < 0x10000) {
            out[o++] = (char)(0xE0 | (cp >> 12));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
          } else {
            out[o++] = (char)(0xF0 | (cp >> 18));
            out[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
          }
          out = realloc(out, o + n + 1);
          i = j + 1; continue;
        }
      } else if (start[i+1] == '#') {
        unsigned cp = 0; size_t j = i + 2;
        while (j < n && start[j] >= '0' && start[j] <= '9') {
          cp = cp * 10 + (start[j] - '0'); j++;
        }
        if (j < n && start[j] == ';') {
          if (cp < 0x80) out[o++] = (char)cp;
          else if (cp < 0x800) {
            out[o++] = (char)(0xC0 | (cp >> 6));
            out[o++] = (char)(0x80 | (cp & 0x3F));
          } else if (cp < 0x10000) {
            out[o++] = (char)(0xE0 | (cp >> 12));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
          } else {
            out[o++] = (char)(0xF0 | (cp >> 18));
            out[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (cp & 0x3F));
          }
          out = realloc(out, o + n + 1);
          i = j + 1; continue;
        }
      } else if (strncmp(start + i, "&amp;", 5) == 0)  { out[o++]='&'; i+=5; continue; }
      else if (strncmp(start + i, "&lt;", 4) == 0)      { out[o++]='<'; i+=4; continue; }
      else if (strncmp(start + i, "&gt;", 4) == 0)      { out[o++]='>'; i+=4; continue; }
      else if (strncmp(start + i, "&quot;", 6) == 0)    { out[o++]='"'; i+=6; continue; }
      else if (strncmp(start + i, "&apos;", 6) == 0)    { out[o++]='\''; i+=6; continue; }
    }
    out[o++] = start[i++];
  }
  out[o] = '\0';
  /* trim() */
  char *p = out; while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
  char *e = p + strlen(p);
  while (e > p && (e[-1]==' '||e[-1]=='\n'||e[-1]=='\r'||e[-1]=='\t')) *--e='\0';
  if (p != out) memmove(out, p, strlen(p) + 1);
  return out;
}

/* get inner text of first <tag ...>...</tag> within [body,bodyEnd). */
static char *get_tag(const char *body, const char *bodyEnd, const char *tag) {
  char open[64];
  snprintf(open, sizeof open, "<%s", tag);
  const char *p = body;
  size_t tl = strlen(tag);
  while (p < bodyEnd && (p = strstr(p, open)) && p < bodyEnd) {
    const char *q = p + strlen(open);
    if (*q != '>' && *q != ' ' && *q != '\t' && *q != '\n' && *q != '/') {
      p = q; continue;  /* not a word boundary (\\b) */
    }
    const char *gt = memchr(q, '>', bodyEnd - q);
    if (!gt) return NULL;
    if (gt > q && gt[-1] == '/') { p = gt + 1; continue; }  /* self-closed */
    const char *content = gt + 1;
    char close[64];
    snprintf(close, sizeof close, "</%s>", tag);
    const char *ce = strstr(content, close);
    if (!ce || ce > bodyEnd) return NULL;
    (void)tl;
    return clean_dup(content, (size_t)(ce - content));
  }
  return NULL;
}

/* safeIso: new Date(s).toISOString() or NULL. Hatena dc:date is RFC3339
 * (e.g. 2026-05-17T09:00:00+09:00) — normalize to a Z ISO if parseable. */
static int safe_iso(const char *s, char *out, size_t cap) {
  if (!s || !*s) return 0;
  struct tm tm; memset(&tm, 0, sizeof tm);
  int y, mo, d, h, mi, se = 0, oh = 0, om = 0; char sign = '+';
  int got = sscanf(s, "%d-%d-%dT%d:%d:%d%c%d:%d",
                   &y, &mo, &d, &h, &mi, &se, &sign, &oh, &om);
  if (got < 5) {
    got = sscanf(s, "%d-%d-%d", &y, &mo, &d);
    if (got < 3) return 0;
    h = mi = se = 0; sign = '+'; oh = om = 0;
  }
  tm.tm_year = y - 1900; tm.tm_mon = mo - 1; tm.tm_mday = d;
  tm.tm_hour = h; tm.tm_min = mi; tm.tm_sec = se;
  time_t t = timegm(&tm);
  if (got >= 7) {
    int off = (oh * 3600 + om * 60) * (sign == '-' ? -1 : 1);
    t -= off;
  }
  struct tm g; gmtime_r(&t, &g);
  strftime(out, cap, "%Y-%m-%dT%H:%M:%S.000Z", &g);
  return 1;
}

static int seen_has(char **a, int n, const char *s) {
  for (int i = 0; i < n; i++) if (strcmp(a[i], s) == 0) return 1;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char **seen = NULL; int sn = 0, scap = 0, n = 0, fetched = 0;

  for (int fi = 0; fi < NF; fi++) {
    char *xml = feed_get_text(ctx->http, FEEDS[fi].url, 15000);
    if (!xml) continue;
    fetched++;
    const char *category = FEEDS[fi].cat;

    const char *p = xml;
    while ((p = strstr(p, "<item")) != NULL) {
      const char *gt = strchr(p, '>');
      if (!gt) break;
      const char *body = gt + 1;
      const char *end = strstr(body, "</item>");
      if (!end) break;

      char *title = get_tag(body, end, "title");
      char *link  = get_tag(body, end, "link");
      char *date  = get_tag(body, end, "dc:date");
      char *bmS   = get_tag(body, end, "hatena:bookmarkcount");
      char *subj  = get_tag(body, end, "dc:subject");
      char *desc  = get_tag(body, end, "description");
      int bookmarks = bmS ? atoi(bmS) : 0;

      if (link && link[0] && !seen_has(seen, sn, link)) {
        if (sn == scap) { scap = scap ? scap * 2 : 64;
          seen = realloc(seen, scap * sizeof(char *)); }
        seen[sn++] = strdup(link);

        char uidb[1024];
        snprintf(uidb, sizeof uidb, "%s|%s", SOURCE_ID, link);

        char summary[256];
        if (subj && subj[0])
          snprintf(summary, sizeof summary, "%d bookmarks · %s",
                   bookmarks, subj);
        else
          snprintf(summary, sizeof summary, "%d bookmarks", bookmarks);

        char iso[40]; int hasIso = safe_iso(date, iso, sizeof iso);

        /* tags: ['hatena','trending',`feed:${cat}`, subj?`cat:${subj}`] */
        cJSON *tags = cJSON_CreateArray();
        cJSON_AddItemToArray(tags, cJSON_CreateString("hatena"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("trending"));
        char ft[64]; snprintf(ft, sizeof ft, "feed:%s", category);
        cJSON_AddItemToArray(tags, cJSON_CreateString(ft));
        if (subj && subj[0]) {
          char ct[128]; snprintf(ct, sizeof ct, "cat:%s", subj);
          cJSON_AddItemToArray(tags, cJSON_CreateString(ct));
        }
        char *tj = cJSON_PrintUnformatted(tags);

        cJSON *props = cJSON_CreateObject();
        cJSON_AddNumberToObject(props, "bookmarks", bookmarks);
        if (subj && subj[0]) cJSON_AddStringToObject(props, "subject", subj);
        else cJSON_AddNullToObject(props, "subject");
        cJSON_AddStringToObject(props, "feed_category", category);
        cJSON_AddStringToObject(props, "source", "hatena_rss");
        char *pj = cJSON_PrintUnformatted(props);

        intel_item it = {0};
        it.uid = uidb;
        it.title = title;
        it.summary = summary;
        it.body = (desc && desc[0]) ? desc : NULL;
        it.link = link;
        it.lang = "ja";
        it.published_at = hasIso ? iso : NULL;
        it.record_type = SOURCE_ID;
        it.tags_json = tj;
        it.properties_json = pj;
        if (sink->emit(sink, &it) >= 0) n++;

        free(tj); free(pj);
        cJSON_Delete(tags); cJSON_Delete(props);
      }

      free(title); free(link); free(date);
      free(bmS); free(subj); free(desc);
      p = end + 7;
    }
    free(xml);
  }

  for (int i = 0; i < sn; i++) free(seen[i]);
  free(seen);
  fprintf(stderr, "[hatena-bookmark-extended] emitted %d (%d/%d feeds fetched)\n",
          n, fetched, NF);
  /* STATUS code, not a row count: a feed that loads but has nothing new is an
   * honest empty. Only a total fetch failure is a real error. */
  return fetched > 0 ? 0 : -1;
}

static const source_def hatena_bookmark_extended_def = {
  .id = "hatena-bookmark-extended", .collector = "social",
  .name = "Hatena Bookmark (entry list)",
  .name_ja = "はてなブックマーク エントリ",
  .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(hatena_bookmark_extended_def)
