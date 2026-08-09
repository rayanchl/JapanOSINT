/* collectors/environment/sources/jma_volcano.c
 * INTEL source — port of server/src/collectors/jmaVolcano.js.
 * JMA developer eqvol Atom feed carries earthquake AND volcano bulletins;
 * keep only volcano-related entries (火山/噴火/降灰/Volcan/Ash Fall).
 * uid = jma-volcano|<guid|link|sha1(title|pubDate)>. Honest empty on failure. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <openssl/sha.h>

#define FEED_URL "https://www.data.jma.go.jp/developer/xml/feed/eqvol.xml"

/* VOLCANO_RE = /(火山|噴火|降灰|Volcan|volcan|Ash Fall|ashfall)/i */
static int volcano_match(const char *s) {
  if (!s) return 0;
  if (strstr(s, "\xE7\x81\xAB\xE5\xB1\xB1")) return 1;       /* 火山 */
  if (strstr(s, "\xE5\x99\xB4\xE7\x81\xAB")) return 1;       /* 噴火 */
  if (strstr(s, "\xE9\x99\x8D\xE7\x81\xB0")) return 1;       /* 降灰 */
  if (strcasestr(s, "volcan")) return 1;
  if (strcasestr(s, "ash fall")) return 1;
  if (strcasestr(s, "ashfall")) return 1;
  return 0;
}

static char *dup_n(const char *s, size_t n) {
  char *o = malloc(n + 1);
  if (!o) return NULL;
  memcpy(o, s, n); o[n] = 0;
  return o;
}

/* extract <tag>...</tag> text (first match), strips nothing fancy. */
static char *tag_text(const char *from, const char *end, const char *tag) {
  char open[32], close[32];
  snprintf(open, sizeof open, "<%s", tag);
  snprintf(close, sizeof close, "</%s>", tag);
  const char *p = from;
  size_t ol = strlen(open);
  while (p < end) {
    const char *q = strcasestr(p, open);
    if (!q || q >= end) return NULL;
    char d = q[ol];
    if (d == ' ' || d == '>' || d == '\t' || d == '\n' || d == '\r') {
      const char *gt = strchr(q, '>');
      if (!gt || gt >= end) return NULL;
      const char *c = strcasestr(gt + 1, close);
      if (!c || c > end) return NULL;
      return dup_n(gt + 1, (size_t)(c - (gt + 1)));
    }
    p = q + ol;
  }
  return NULL;
}

static char *atom_link(const char *from, const char *end) {
  const char *p = strcasestr(from, "<link");
  if (!p || p >= end) return NULL;
  const char *gt = strchr(p, '>');
  if (!gt) return NULL;
  const char *h = strcasestr(p, "href=");
  if (!h || h > gt) return NULL;
  h += 5;
  char q = *h;
  if (q == '"' || q == '\'') { h++; const char *e = strchr(h, q);
    return e ? dup_n(h, (size_t)(e - h)) : NULL; }
  return NULL;
}

static char *sha1_20(const char *a, const char *b) {
  unsigned char d[20]; SHA_CTX c; SHA1_Init(&c);
  if (a) { SHA1_Update(&c, a, strlen(a)); SHA1_Update(&c, "|", 1); }
  if (b) { SHA1_Update(&c, b, strlen(b)); SHA1_Update(&c, "|", 1); }
  SHA1_Final(d, &c);
  char *h = malloc(21);
  for (int i = 0; i < 10; i++) sprintf(h + i*2, "%02x", d[i]);
  h[20] = 0;
  return h;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *xml = feed_get_text(ctx->http, FEED_URL, 10000);
  if (!xml) {
    fprintf(stderr, "[jma-volcano] upstream unavailable\n");
    return -1;
  }
  const char *xend = xml + strlen(xml);
  int n = 0;
  const char *cur = xml;
  for (;;) {
    const char *open = NULL; int atom = 0;
    for (const char *p = cur; (p = strchr(p, '<')) && p < xend; p++) {
      int isi = (strncasecmp(p, "<item", 5) == 0);
      int ise = (strncasecmp(p, "<entry", 6) == 0);
      if (!isi && !ise) continue;
      char d = p[isi ? 5 : 6];
      if (d == ' ' || d == '>' || d == '\t' || d == '\n' ||
          d == '\r' || d == '/') { open = p; atom = ise; break; }
    }
    if (!open || open >= xend) break;
    const char *closeTag = atom ? "</entry>" : "</item>";
    const char *cl = strcasestr(open, closeTag);
    if (!cl) break;
    const char *it = open; size_t itlen = (size_t)(cl - it);

    char *title = tag_text(it, it + itlen, "title");
    char *desc  = tag_text(it, it + itlen, atom ? "summary" : "description");
    if (!desc) desc = tag_text(it, it + itlen, "content");
    char *link  = atom ? atom_link(it, it + itlen)
                       : tag_text(it, it + itlen, "link");
    char *pub   = tag_text(it, it + itlen, "pubDate");
    if (!pub) pub = tag_text(it, it + itlen, "published");
    if (!pub) pub = tag_text(it, it + itlen, "updated");
    char *guid  = tag_text(it, it + itlen, "guid");
    if (!guid) guid = tag_text(it, it + itlen, "id");
    char *author= tag_text(it, it + itlen, "author");

    /* filter: VOLCANO_RE.test(`${title} ${description}`) */
    char hay[1024];
    snprintf(hay, sizeof hay, "%s %s", title ? title : "", desc ? desc : "");
    if (volcano_match(hay)) {
      char *rk = NULL;
      if (guid && *guid) rk = strdup(guid);
      else if (link && *link) rk = strdup(link);
      else if (title && *title) rk = sha1_20(title, pub);
      if (rk) {
        cJSON *pj = cJSON_CreateObject();
        if (guid && *guid) cJSON_AddStringToObject(pj, "guid", guid);
        else cJSON_AddNullToObject(pj, "guid");
        char *props = cJSON_PrintUnformatted(pj);

        cJSON *tags = cJSON_CreateArray();
        cJSON_AddItemToArray(tags, cJSON_CreateString("volcano"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("jma"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("hazard"));
        char *tj = cJSON_PrintUnformatted(tags);

        char summ[256] = {0};
        const char *sb = desc ? desc : (title ? title : "");
        strncpy(summ, sb, 240); summ[240] = 0;

        intel_item item = {0};
        item.remote_key = rk;
        item.title = (title && *title) ? title : NULL;
        item.body  = desc;
        item.summary = summ[0] ? summ : NULL;
        item.link  = (link && *link) ? link : NULL;
        item.author = (author && *author) ? author
                      : "\xE6\xB0\x97\xE8\xB1\xA1\xE5\xBA\x81 Japan Meteorological Agency";
        item.lang = "ja";
        item.published_at = pub;
        item.record_type = "article";
        item.properties_json = props;
        item.tags_json = tj;
        if (sink->emit(sink, &item) >= 0) n++;
        free(rk); free(props); free(tj);
        cJSON_Delete(pj); cJSON_Delete(tags);
      }
    }
    free(title); free(desc); free(link); free(pub); free(guid); free(author);
    cur = cl + strlen(closeTag);
  }
  free(xml);
  fprintf(stderr, "[jma-volcano] emitted %d\n", n);
  /* audit-09: was `n > 0 ? 0 : -1`. The JMA eqvol feed legitimately carries no
   * volcano bulletin for long stretches; that is an honest empty, and -1 marks
   * the run as errored and quarantines the source. */
  return 0;
}

static const source_def jma_volcano_def = {
  .id = "jma-volcano", .collector = "environment",
  .name = "JMA Volcanic Alerts", .name_ja = "気象庁 火山情報",
  .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(jma_volcano_def)
