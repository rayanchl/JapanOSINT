/* collectors/cyber/sources/my_jvn.c — port of server/src/collectors/myJvn.js.
 * MyJVN — IPA/JPCERT Japan Vulnerability Notes. GET the getVulnOverviewList
 * REST endpoint (atom-ish XML, no auth), hand-parse each <item> with the same
 * per-tag CDATA-aware grab() the JS uses, and emit one intel row per item:
 *   uid       = my-jvn|<sec:identifier||dc:identifier> | else my-jvn|<link>
 *   title     = <title>
 *   body      = <description>[:500]
 *   summary   = body[:240] || null
 *   link      = <link> || null
 *   language  = 'ja'
 *   published = <dcterms:issued||dc:date> || null
 *   tags      = ['advisory','jvn','cyber']
 *   props     = { jvn_id: id||null, modified: <dcterms:modified>||null }
 * The intel _meta/seed envelope is dropped (live rows only). */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FEED_URL  "https://jvndb.jvn.jp/myjvn?method=getVulnOverviewList&feed=hnd&maxCountItem=50"
#define TIMEOUT_MS 12000

/* JS grab(tag): /<tag[^>]*>(?:<![CDATA[(.*?)]]>|([\s\S]*?))<\/tag>/s on the
 * <item> block. Returns malloc'd trimmed inner text (CDATA unwrapped), or
 * NULL if the tag is absent. Caller frees. Tag matched literally incl. ':'. */
static char *grab(const char *block, size_t blen, const char *tag) {
  size_t tl = strlen(tag);
  const char *end = block + blen;
  for (const char *p = block; p + tl + 1 < end; p++) {
    if (*p != '<') continue;
    if (strncmp(p + 1, tag, tl) != 0) continue;
    const char *after = p + 1 + tl;
    /* next char must end the tag name: '>' or whitespace/attr or '/' */
    if (*after != '>' && *after != ' ' && *after != '\t' &&
        *after != '\n' && *after != '\r' && *after != '/') continue;
    const char *gt = memchr(after, '>', (size_t)(end - after));
    if (!gt) return NULL;
    if (gt > p && gt[-1] == '/') return NULL; /* self-closing: no capture */
    const char *content = gt + 1;
    /* find </tag> */
    char close[64];
    snprintf(close, sizeof close, "</%s>", tag);
    size_t cll = strlen(close);
    const char *cl = NULL;
    for (const char *q = content; q + cll <= end; q++) {
      if (strncmp(q, close, cll) == 0) { cl = q; break; }
    }
    if (!cl) return NULL;
    size_t len = (size_t)(cl - content);
    char *raw = malloc(len + 1);
    if (!raw) return NULL;
    memcpy(raw, content, len);
    raw[len] = 0;
    char *s = raw;
    /* CDATA branch: <![CDATA[ ... ]]> */
    char *cd = strstr(s, "<![CDATA[");
    if (cd) {
      char *e2 = strstr(cd + 9, "]]>");
      if (e2) {
        size_t ilen = (size_t)(e2 - (cd + 9));
        memmove(s, cd + 9, ilen);
        s[ilen] = 0;
      }
    }
    /* trim ws */
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
    size_t L = strlen(s);
    while (L && (s[L-1]==' '||s[L-1]=='\n'||s[L-1]=='\r'||s[L-1]=='\t')) s[--L]=0;
    char *res = strdup(s);
    free(raw);
    return res;
  }
  return NULL;
}

/* Cut `s` to at most `max` BYTES without splitting a UTF-8 sequence.
 * JS .slice() counts UTF-16 units and never produces invalid text; a raw
 * byte cut here does, and core/intel.c then persists bytes that SQLite/the
 * API cannot decode ("Could not decode to UTF-8"). Back up over any
 * continuation bytes (10xxxxxx) so the cut lands on a character boundary. */
static void utf8_trunc(char *s, size_t max) {
  if (!s) return;
  size_t L = strlen(s);
  if (L <= max) return;
  size_t c = max;
  while (c > 0 && ((unsigned char)s[c] & 0xC0) == 0x80) c--;
  s[c] = 0;
}

/* JS `(x || y) ?? '' ` then `.trim()` already applied: pick first non-NULL */
static char *grab2(const char *b, size_t bl, const char *t1, const char *t2) {
  char *a = grab(b, bl, t1);
  if (a && a[0]) return a;
  free(a);
  return grab(b, bl, t2);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *xml = feed_get_text(ctx->http, FEED_URL, TIMEOUT_MS);
  if (!xml) return -1;                   /* !res.ok → 0 items */

  int n = 0;
  const char *cur = xml;
  const char *xend = xml + strlen(xml);
  for (;;) {
    const char *open = NULL;
    for (const char *p = cur; (p = strchr(p, '<')) && p < xend; p++) {
      if (strncmp(p, "<item", 5) != 0) continue;
      char d = p[5];
      if (d == ' ' || d == '>' || d == '\t' || d == '\n' ||
          d == '\r' || d == '/') { open = p; break; }
    }
    if (!open) break;
    const char *cl = strstr(open, "</item>");
    if (!cl) break;
    const char *blk = open;
    size_t blen = (size_t)(cl - open);

    char *id    = grab2(blk, blen, "sec:identifier", "dc:identifier");
    char *title = grab(blk, blen, "title");
    char *link  = grab(blk, blen, "link");
    char *pub   = grab2(blk, blen, "dcterms:issued", "dc:date");
    char *mod   = grab(blk, blen, "dcterms:modified");
    char *desc  = grab(blk, blen, "description");

    /* description.slice(0,500) — on a character boundary, not a byte one */
    utf8_trunc(desc, 500);

    /* uid = intelUid(SOURCE_ID, it.id, it.link) → first non-empty */
    char uidbuf[768];
    const char *key = (id && id[0]) ? id : ((link && link[0]) ? link : NULL);
    if (key) snprintf(uidbuf, sizeof uidbuf, "my-jvn|%s", key);

    /* summary = (desc||'').slice(0,240) || null */
    char summ[256] = {0};
    if (desc && desc[0]) {
      snprintf(summ, sizeof summ, "%s", desc);
      utf8_trunc(summ, 240);
    }

    cJSON *props = cJSON_CreateObject();   /* EXACT JS key order */
    if (id && id[0]) cJSON_AddStringToObject(props, "jvn_id", id);
    else cJSON_AddNullToObject(props, "jvn_id");
    if (mod && mod[0]) cJSON_AddStringToObject(props, "modified", mod);
    else cJSON_AddNullToObject(props, "modified");
    char *props_s = cJSON_PrintUnformatted(props);

    intel_item it = {0};
    it.uid = key ? uidbuf : NULL;
    it.title = title;
    it.body = (desc && desc[0]) ? desc : NULL;
    it.summary = (desc && desc[0]) ? summ : NULL;
    it.link = (link && link[0]) ? link : NULL;
    it.lang = "ja";
    it.published_at = (pub && pub[0]) ? pub : NULL;
    it.record_type = "article";
    it.properties_json = props_s ? props_s : "{}";
    it.tags_json = "[\"advisory\",\"jvn\",\"cyber\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(props_s);
    cJSON_Delete(props);
    free(id); free(title); free(link); free(pub); free(mod); free(desc);
    cur = cl + 7; /* strlen("</item>") */
  }

  free(xml);
  fprintf(stderr, "[my-jvn] emitted %d\n", n);
  return 0;      /* an empty advisory feed is honest, not an error (rc=-1
                  * would trip the anomaly detector and quarantine us) */
}

static const source_def my_jvn_def = {
  .id = "my-jvn", .collector = "cyber",
  .name = "JVN iPedia / MyJVN", .name_ja = "JVN iPedia / MyJVN",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(my_jvn_def)
