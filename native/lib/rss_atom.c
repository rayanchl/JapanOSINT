#include "rss_atom.h"
#include "../core/httpclient.h"
#include "../third_party/cJSON.h"
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* --- tiny tolerant XML helpers (sufficient for RSS/RDF/Atom feeds) --- */

static char *dup_n(const char *s, size_t n) {
  char *r = malloc(n + 1); if (!r) return NULL;
  memcpy(r, s, n); r[n] = 0; return r;
}

/* Find first <tag ...>...</tag> inside [from,end); returns inner text
 * (malloc'd, CDATA-stripped, a few entities decoded) or NULL. *after set to
 * end of close tag. Case-insensitive tag match, namespace-agnostic suffix. */
static char *tag_text(const char *from, const char *end, const char *tag,
                      const char **after) {
  size_t tl = strlen(tag);
  for (const char *p = from; p && p < end; p++) {
    if (*p != '<') continue;
    const char *q = p + 1;
    if (q < end && (*q == '/' || *q == '!' || *q == '?')) continue;
    /* match tag possibly with ns prefix: <(...:)?tag[ >/] */
    const char *nameend = q;
    while (nameend < end && *nameend != ' ' && *nameend != '>' &&
           *nameend != '/' && *nameend != '\t' && *nameend != '\n') nameend++;
    size_t namelen = (size_t)(nameend - q);
    const char *base = q;
    for (const char *c = q; c < nameend; c++) if (*c == ':') base = c + 1;
    size_t baselen = (size_t)(nameend - base);
    if (baselen != tl || strncasecmp(base, tag, tl) != 0) {
      (void)namelen; continue;
    }
    const char *gt = memchr(q, '>', (size_t)(end - q));
    if (!gt) return NULL;
    if (gt[-1] == '/') { if (after) *after = gt + 1; return dup_n("", 0); }
    const char *content = gt + 1;
    /* find matching close </...tag> */
    char close[64]; snprintf(close, sizeof close, "</");
    const char *cl = content;
    while (cl < end) {
      const char *lt = memchr(cl, '<', (size_t)(end - cl));
      if (!lt) return NULL;
      if (lt + 1 < end && lt[1] == '/') {
        const char *cn = lt + 2, *ce = cn;
        while (ce < end && *ce != '>') ce++;
        const char *cb = cn;
        for (const char *c = cn; c < ce; c++) if (*c == ':') cb = c + 1;
        if ((size_t)(ce - cb) == tl && strncasecmp(cb, tag, tl) == 0) {
          size_t len = (size_t)(lt - content);
          char *raw = dup_n(content, len);
          if (after) *after = ce + 1;
          /* strip CDATA wrapper */
          char *s = raw;
          if (!strncmp(s, "<![CDATA[", 9)) {
            char *e2 = strstr(s, "]]>");
            if (e2) { *e2 = 0; memmove(s, s + 9, strlen(s + 9) + 1); }
          }
          /* trim ws */
          while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
          size_t L = strlen(s);
          while (L && (s[L-1]==' '||s[L-1]=='\n'||s[L-1]=='\r'||s[L-1]=='\t')) s[--L]=0;
          /* decode minimal entities */
          char *o = s;
          for (char *r = s; *r; ) {
            if (!strncmp(r,"&amp;",5)){*o++='&';r+=5;}
            else if(!strncmp(r,"&lt;",4)){*o++='<';r+=4;}
            else if(!strncmp(r,"&gt;",4)){*o++='>';r+=4;}
            else if(!strncmp(r,"&quot;",6)){*o++='"';r+=6;}
            else if(!strncmp(r,"&#39;",5)||!strncmp(r,"&apos;",6)){*o++='\'';r+=(r[2]=='3')?5:6;}
            else *o++=*r++;
          }
          *o=0;
          char *res = strdup(s); free(raw); return res;
        }
      }
      cl = lt + 1;
    }
    return NULL;
  }
  return NULL;
}

/* Atom <link href="..."/> */
static char *atom_link(const char *from, const char *end) {
  for (const char *p = from; p < end; p++) {
    if (strncasecmp(p, "<link", 5) != 0) continue;
    const char *gt = memchr(p, '>', (size_t)(end - p)); if (!gt) return NULL;
    const char *h = NULL;
    for (const char *c = p; c < gt - 4; c++)
      if (strncasecmp(c, "href=", 5) == 0) { h = c + 5; break; }
    if (!h) return NULL;
    char quote = *h; if (quote != '"' && quote != '\'') return NULL;
    const char *e2 = memchr(h + 1, quote, (size_t)(gt - h));
    return e2 ? dup_n(h + 1, (size_t)(e2 - h - 1)) : NULL;
  }
  return NULL;
}

static char *sha1_20(const char *a, const char *b) {
  unsigned char d[20]; SHA_CTX c; SHA1_Init(&c);
  if (a) { SHA1_Update(&c, a, strlen(a)); SHA1_Update(&c, "|", 1); }
  if (b) { SHA1_Update(&c, b, strlen(b)); SHA1_Update(&c, "|", 1); }
  SHA1_Final(d, &c);
  char *h = malloc(41);
  for (int i = 0; i < 20; i++) sprintf(h + i*2, "%02x", d[i]);
  h[40] = 0; h[20] = 0;            /* intelHashKey slices to 20 hex chars */
  return h;
}

int rss_collect(const source_ctx *ctx, intel_sink *sink,
                const char *url, const char *lang, const char *tags_json) {
  http_response r = {0};
  const char *hdrs[] = { "Accept: application/rss+xml,*/*",
                         "User-Agent: japanosint-collector", NULL };
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 8000, 2, &r);
  if (rc != 0 || r.status < 200 || r.status >= 300 || !r.body) {
    http_response_free(&r); return -1;
  }
  const char *xml = r.body, *xend = r.body + r.body_len;
  int n = 0;
  const char *cur = xml;
  for (;;) {
    /* next <item ...>/<entry ...> block. Require a delimiter after the name
     * so the RDF <items> table-of-contents (Seq) is NOT matched. */
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
    const char *it = NULL; size_t itlen = 0; const char *blkend = NULL;
    const char *closeTag = atom ? "</entry>" : "</item>";
    const char *cl = strcasestr(open, closeTag);
    if (!cl) break;
    it = open; blkend = cl; itlen = (size_t)(blkend - it);
    const char *a;
    char *title = tag_text(it, it + itlen, "title", &a);
    char *desc  = tag_text(it, it + itlen, atom ? "summary" : "description", &a);
    if (!desc) desc = tag_text(it, it + itlen, "content", &a);
    char *link  = atom ? atom_link(it, it + itlen)
                       : tag_text(it, it + itlen, "link", &a);
    char *pub   = tag_text(it, it + itlen, "pubDate", &a);
    if (!pub) pub = tag_text(it, it + itlen, "date", &a);     /* dc:date */
    if (!pub) pub = tag_text(it, it + itlen, "published", &a);
    if (!pub) pub = tag_text(it, it + itlen, "updated", &a);
    char *guid  = tag_text(it, it + itlen, "guid", &a);
    if (!guid) guid = tag_text(it, it + itlen, "id", &a);
    char *author= tag_text(it, it + itlen, "author", &a);

    /* uid precedence: guid → link → sha1(title|pubDate) (== intelUid) */
    char *rk = NULL;
    if (guid && *guid) rk = strdup(guid);
    else if (link && *link) rk = strdup(link);
    else if (title && *title) rk = sha1_20(title, pub);
    if (rk) {
      char props[512] = "{}";
      if (guid && *guid) {
        cJSON *pj = cJSON_CreateObject();
        cJSON_AddStringToObject(pj, "guid", guid);
        char *s = cJSON_PrintUnformatted(pj);
        snprintf(props, sizeof props, "%s", s); free(s); cJSON_Delete(pj);
      }
      char summ[256] = {0};
      if (desc) { strncpy(summ, desc, 240); summ[240] = 0; }
      intel_item item = {0};
      item.remote_key = rk;
      item.title = title; item.body = desc;
      item.summary = desc ? summ : NULL;
      item.link = link; item.author = author;
      item.lang = lang ? lang : "ja";
      item.published_at = pub;                 /* RFC822→ISO norm: P5 refine */
      item.record_type = "article";
      item.properties_json = props;
      item.tags_json = tags_json;
      if (sink->emit(sink, &item) >= 0) n++;
      free(rk);
    }
    free(title); free(desc); free(link); free(pub); free(guid); free(author);
    cur = blkend + strlen(closeTag);
  }
  http_response_free(&r);
  fprintf(stderr, "[rss] %s emitted %d\n", ctx->source_id, n);
  return n;
}
