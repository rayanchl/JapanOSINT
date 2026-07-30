/* collectors/government/sources/egov_laws.c
 * Port of server/src/collectors/egovLaws.js (fetch + regex XML, intel items).
 * GET laws.e-gov.go.jp/api/1/lawlists/1 -> iterate <LawNameListInfo> blocks
 * (slice 0..200), per block pickTag LawId||LawNum / LawName / LawNo /
 * PromulgationDate. uid = intelUid(egov-laws, id, name) -> remote_key = first
 * non-empty of (id,name); sink prefixes "<source_id>|". SEED/_meta n/a (none).
 * pickTag = first <tag ...>INNER</tag>, .trim() (no tag-strip / entity decode
 * in JS), so we only trim ASCII whitespace. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define API_URL "https://laws.e-gov.go.jp/api/1/lawlists/1"
/* The e-Gov law list is a full catalogue; the old MAX_ITEMS=200 kept the
 * first 200 laws and dropped the rest (docs/SOURCE_EXHAUSTIVENESS.md). */

/* JS String.prototype.trim() — strip leading/trailing ASCII whitespace
 * in place. */
static void js_trim(char *s) {
  size_t L = strlen(s);
  while (L && (s[L-1]==' '||s[L-1]=='\t'||s[L-1]=='\n'||s[L-1]=='\r'||
               s[L-1]=='\f'||s[L-1]=='\v')) s[--L] = 0;
  size_t i = 0;
  while (s[i]==' '||s[i]=='\t'||s[i]=='\n'||s[i]=='\r'||
         s[i]=='\f'||s[i]=='\v') i++;
  if (i) memmove(s, s + i, strlen(s + i) + 1);
}

/* pickTag(b,tag)[0] || "" then trimmed. */
static int pick_tag(const char *block, const char *tag, char *out, size_t n) {
  if (!html_tag(block, tag, out, n)) { out[0] = 0; return 0; }
  js_trim(out);
  return out[0] ? 1 : 0;
}

/* encodeURIComponent: keep A-Za-z0-9 and -_.!~*'() ; %XX (UTF-8 bytes)
 * everything else (uppercase hex, matching JS). */
static char *enc_uri_component(const char *s) {
  static const char *unres = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz0123456789-_.!~*'()";
  size_t len = strlen(s);
  char *o = malloc(len * 3 + 1);
  if (!o) return NULL;
  size_t k = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80 && strchr(unres, (char)c)) o[k++] = (char)c;
    else k += (size_t)sprintf(o + k, "%%%02X", c);
  }
  o[k] = 0;
  return o;
}

/* date matches /^\d{8}$/ */
static int is_yyyymmdd(const char *d) {
  if (!d || strlen(d) != 8) return 0;
  for (int i = 0; i < 8; i++) if (!isdigit((unsigned char)d[i])) return 0;
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *xml = feed_get_text(ctx->http, API_URL, 10000);
  if (!xml) return -1;

  int n = 0, taken = 0;
  const char *cur = xml;
  const char *inner; int ilen;
  while ((cur = html_block(cur, "LawNameListInfo", &inner, &ilen)) != NULL) {
    char *b = strndup(inner, (size_t)ilen);
    if (!b) continue;
    taken++;

    char id[512] = {0}, name[1024] = {0}, no[512] = {0}, date[128] = {0};
    if (!pick_tag(b, "LawId", id, sizeof id))
      pick_tag(b, "LawNum", id, sizeof id);   /* id = LawId[0] || LawNum[0] */
    pick_tag(b, "LawName", name, sizeof name);
    pick_tag(b, "LawNo", no, sizeof no);
    pick_tag(b, "PromulgationDate", date, sizeof date);

    /* uid = intelUid(SOURCE_ID, id, name): first non-empty of id,name */
    const char *rk = id[0] ? id : (name[0] ? name : NULL);
    if (!rk) { free(b); continue; }            /* no key -> skip (JS would
                                                  randomBytes; non-coalescing) */

    /* title = name || id || 'law' */
    const char *title = name[0] ? name : (id[0] ? id : "law");

    char *link = NULL;
    if (id[0]) {
      char *e = enc_uri_component(id);
      if (e) {
        size_t need = strlen("https://laws.e-gov.go.jp/law/") + strlen(e) + 1;
        link = malloc(need);
        if (link) snprintf(link, need, "https://laws.e-gov.go.jp/law/%s", e);
        free(e);
      }
    }

    char pub[32] = {0};
    if (is_yyyymmdd(date))
      snprintf(pub, sizeof pub, "%.4s-%.2s-%.2sT00:00:00Z",
               date, date + 4, date + 6);

    cJSON *pj = cJSON_CreateObject();           /* {law_id, law_no} */
    if (id[0]) cJSON_AddStringToObject(pj, "law_id", id);
    else cJSON_AddNullToObject(pj, "law_id");
    if (no[0]) cJSON_AddStringToObject(pj, "law_no", no);
    else cJSON_AddNullToObject(pj, "law_no");
    char *pjs = cJSON_PrintUnformatted(pj);

    intel_item it = {0};
    it.remote_key     = rk;
    it.title          = title;
    it.summary        = no[0] ? no : NULL;      /* no || null */
    it.link           = link;                   /* id ? url : null */
    it.lang           = "ja";
    it.published_at   = pub[0] ? pub : NULL;
    it.tags_json      = "[\"law\",\"e-gov\"]";
    it.properties_json = pjs;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pjs); cJSON_Delete(pj);
    free(link);
    free(b);
  }
  free(xml);
  fprintf(stderr, "[egov-laws] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def egov_laws_def = {
  .id = "egov-laws", .collector = "government",
  .name = "e-Gov Law Search API", .name_ja = "e-Gov 法令検索 API",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(egov_laws_def)
