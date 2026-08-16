/* collectors/social/sources/yahoo_realtime.c
 * Port of server/src/collectors/yahooRealtime.js (fetchText + regex anchors).
 * GET https://search.yahoo.co.jp/realtime/buzz ; for each
 *   /<a[^>]+href="\/realtime\/search\?p=([^"&]+)[^"]*"[^>]*>([\s\S]*?)<\/a>/g
 *   term  = decodeURIComponent(g1.replace(/\+/g,' '))
 *   label = decodeEntities(stripTags(g2))
 *   skip if !term || term.length>60 || /<svg|^http/.test(term)
 *   dedupe by term (first wins); cap 60.
 * Feature props (exact JS order): idx, rank, term, label, url, source.
 * No NATIVE_ID prop → uid = sha1(JSON.stringify{g,p})[:16]; geometry is the
 * fixed TOKYO point so deterministic. live=html.length>0 → source string.
 * SEED/_meta NOT ported (HARD RULE 8); when down html="" → 0 rows. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int hexv(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

/* term = decodeURIComponent(raw.replace(/\+/g,' ')) — '+'→' ', %XX→byte. */
static char *url_decode_plus(const char *raw) {
  size_t L = strlen(raw);
  char *o = malloc(L + 1);
  if (!o) return NULL;
  size_t w = 0;
  for (size_t i = 0; i < L; i++) {
    if (raw[i] == '+') { o[w++] = ' '; }
    else if (raw[i] == '%' && i + 2 < L) {
      int hi = hexv((unsigned char)raw[i+1]), lo = hexv((unsigned char)raw[i+2]);
      if (hi >= 0 && lo >= 0) { o[w++] = (char)((hi << 4) | lo); i += 2; }
      else o[w++] = raw[i];
    } else o[w++] = raw[i];
  }
  o[w] = 0;
  return o;
}

/* decodeEntities: &amp;&lt;&gt;&quot;&#039;&apos;&#NNN; (numeric → char,
 * matching JS String.fromCharCode → emit as UTF-8 for code points >127). */
static void put_cp(char *o, size_t *w, unsigned cp) {
  if (cp < 0x80) o[(*w)++] = (char)cp;
  else if (cp < 0x800) {
    o[(*w)++] = (char)(0xC0 | (cp >> 6));
    o[(*w)++] = (char)(0x80 | (cp & 0x3F));
  } else {
    o[(*w)++] = (char)(0xE0 | (cp >> 12));
    o[(*w)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
    o[(*w)++] = (char)(0x80 | (cp & 0x3F));
  }
}

static char *decode_entities(const char *s) {
  size_t L = strlen(s);
  char *o = malloc(L * 3 + 1);
  if (!o) return NULL;
  size_t w = 0;
  for (size_t i = 0; i < L; ) {
    if (s[i] == '&') {
      if (!strncmp(s + i, "&amp;", 5))  { o[w++] = '&'; i += 5; continue; }
      if (!strncmp(s + i, "&lt;", 4))   { o[w++] = '<'; i += 4; continue; }
      if (!strncmp(s + i, "&gt;", 4))   { o[w++] = '>'; i += 4; continue; }
      if (!strncmp(s + i, "&quot;", 6)) { o[w++] = '"'; i += 6; continue; }
      if (!strncmp(s + i, "&#039;", 6)) { o[w++] = '\''; i += 6; continue; }
      if (!strncmp(s + i, "&apos;", 6)) { o[w++] = '\''; i += 6; continue; }
      if (!strncmp(s + i, "&#", 2)) {
        size_t j = i + 2; unsigned v = 0; int any = 0;
        while (s[j] >= '0' && s[j] <= '9') { v = v * 10 + (s[j]-'0'); j++; any = 1; }
        if (any && s[j] == ';') { put_cp(o, &w, v); i = j + 1; continue; }
      }
    }
    o[w++] = s[i++];
  }
  o[w] = 0;
  return o;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *html = feed_get_text(ctx->http,
    "https://search.yahoo.co.jp/realtime/buzz", 15000);
  /* JS: live = html.length>0. On fetch failure html='' → 0 features. */
  if (!html || !html[0]) { free(html); cJSON *e = cJSON_CreateArray();
    int z = geojson_emit_features(sink, ctx->source_id, e);
    cJSON_Delete(e); return z >= 0 ? 0 : -1; }
  int live = (int)(strlen(html) > 0);

  cJSON *features = cJSON_CreateArray();
  /* seen-term dedupe set */
  char **seen = NULL; int seen_n = 0, seen_cap = 0;
  int rank = 0;

  const char *p = html;
  /* /<a[^>]+href="\/realtime\/search\?p=([^"&]+)[^"]*"[^>]*>([\s\S]*?)<\/a>/g */
  while (rank < 60) {
    const char *needle = strstr(p, "href=\"/realtime/search?p=");
    if (!needle) break;
    /* require it sits inside an <a ...> tag: backtrack to a '<' that opens 'a' */
    const char *lt = needle;
    while (lt > html && lt[-1] != '<') lt--;
    if (!(lt > html) || !(lt[0] == 'a' || lt[0] == 'A')) { p = needle + 1; continue; }

    const char *g1 = needle + strlen("href=\"/realtime/search?p=");
    const char *g1e = g1;
    while (*g1e && *g1e != '"' && *g1e != '&') g1e++;       /* ([^"&]+) */
    const char *qclose = g1e;
    while (*qclose && *qclose != '"') qclose++;             /* [^"]*" */
    if (*qclose != '"') { p = needle + 1; continue; }
    const char *gt = strchr(qclose, '>');                   /* [^>]*> */
    if (!gt) { p = needle + 1; continue; }
    const char *end = strstr(gt + 1, "</a>");               /* ([\s\S]*?)</a> */
    if (!end) { p = needle + 1; continue; }

    char raw_term[256];
    size_t tl = (size_t)(g1e - g1);
    if (tl >= sizeof raw_term) tl = sizeof raw_term - 1;
    memcpy(raw_term, g1, tl); raw_term[tl] = 0;

    /* term = decodeURIComponent(raw.replace(/\+/g,' ')) */
    char *term = url_decode_plus(raw_term);
    /* label = decodeEntities(stripTags(g2)) */
    char *inner = strndup(gt + 1, (size_t)(end - (gt + 1)));
    char *stripped = inner ? html_strip(inner) : NULL;
    char *label = stripped ? decode_entities(stripped) : NULL;
    free(inner); free(stripped);

    p = end + 4;

    /* if (!term || term.length>60 || /<svg|^http/.test(term)) continue */
    if (!term || !term[0] || strlen(term) > 60
        || strstr(term, "<svg") || strncmp(term, "http", 4) == 0) {
      free(term); free(label); continue;
    }
    /* if (seen.has(term)) continue; seen.add(term) */
    int dup = 0;
    for (int i = 0; i < seen_n; i++) if (!strcmp(seen[i], term)) { dup = 1; break; }
    if (dup) { free(term); free(label); continue; }
    if (seen_n == seen_cap) {
      seen_cap = seen_cap ? seen_cap * 2 : 16;
      seen = realloc(seen, sizeof(char *) * seen_cap);
    }
    seen[seen_n++] = strdup(term);

    int idx = rank;       /* JS: idx i (0-based), rank = i+1 */
    rank++;

    /* url = .../search?p=encodeURIComponent(term). encodeURIComponent of an
     * already-decoded term re-encodes; mirror by re-encoding the decoded term
     * RFC3986-unreserved (A-Za-z0-9-_.!~*'()), space→%20. */
    char enc[768]; size_t ew = 0;
    for (const unsigned char *t = (const unsigned char *)term; *t && ew + 3 < sizeof enc; t++) {
      unsigned char c = *t;
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c=='-'||c=='_'||c=='.'||c=='!'||c=='~'||
          c=='*'||c=='\''||c=='('||c==')') {
        enc[ew++] = (char)c;
      } else {
        static const char hx[] = "0123456789ABCDEF";
        enc[ew++] = '%'; enc[ew++] = hx[c >> 4]; enc[ew++] = hx[c & 0xF];
      }
    }
    enc[ew] = 0;
    char url[1024];
    snprintf(url, sizeof url,
      "https://search.yahoo.co.jp/realtime/search?p=%s", enc);

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    /* NO GEOMETRY. Every row here used to be emitted at Tokyo Station
     * (35.6895, 139.6917). What this source reports has no location,
     * and stacking every row on one pin is the fabrication the
     * 2026-07-31 audit deleted from cisa-kev-jp, poc-in-github and
     * peeringdb-jp. Confirmed live by tests/contract/source_contract.py.
     * lib/geojson.c handles an absent geometry correctly — do NOT
     * reintroduce a fallback coordinate. */

    cJSON *pr = cJSON_CreateObject();   /* EXACT JS key order */
    cJSON_AddItemToObject(pr, "idx", cJSON_CreateNumber((double)idx));
    cJSON_AddItemToObject(pr, "rank", cJSON_CreateNumber((double)(idx + 1)));
    cJSON_AddStringToObject(pr, "term", term);
    /* label || term */
    cJSON_AddStringToObject(pr, "label", (label && label[0]) ? label : term);
    cJSON_AddStringToObject(pr, "url", url);
    cJSON_AddStringToObject(pr, "source",
      live ? "yahoo_realtime_buzz" : "yahoo_realtime_seed");
    cJSON_AddItemToObject(f, "properties", pr);
    cJSON_AddItemToArray(features, f);

    free(term); free(label);
  }
  for (int i = 0; i < seen_n; i++) free(seen[i]);
  free(seen);
  free(html);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[yahoo-realtime] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def yahoo_realtime_def = {
  .id = "yahoo-realtime", .collector = "social",
  .name = "Yahoo! Realtime (buzz)", .name_ja = "Yahoo!リアルタイム検索",
   .update_interval_sec = 600, .run = run,
};
REGISTER_SOURCE(yahoo_realtime_def)
