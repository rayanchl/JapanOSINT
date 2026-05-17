/* collectors/cyber/sources/phishing_feeds_jp.c
 * Port of server/src/collectors/phishingFeedsJp.js — OpenPhish feed.txt +
 * PhishTank online-valid.csv, filtered to JP brands / .jp hosts, emitted as
 * intel items. PHISHTANK_APP_KEY raises the PhishTank rate limit (optional).
 * intelEnvelope _meta not ported. (feed_get_text sends no custom UA header;
 * these public feeds are UA-agnostic — content-neutral for the live path.) */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define URL_OPENPHISH "https://openphish.com/feed.txt"
#define URL_PHISHTANK "https://data.phishtank.com/data/online-valid.csv"
#define TIMEOUT_MS 15000

/* JS_KEYWORDS literal substrings (the regex-escaped \. ones reduce to literal
 * substrings; 'apple\.com.*jp' = "apple.com"..."jp" handled separately). */
static const char *KW[] = {
  "rakuten", "jcb", "mizuho", "mufg", "smbc", "japanpost", "jp-bank",
  "yamato", "sagawa", "kuroneko", "ana", "jal", "aeon", "familymart",
  "family-mart", "yodobashi", "mercari", "paypay", "docomo", "au", "softbank",
  "jcom", "nicovideo", "amazon.co.jp", "line.me",
};
#define NKW ((int)(sizeof(KW)/sizeof(KW[0])))

/* JS: /\.jp(\b|\/|:|$)/ over the lowercased url. */
static int has_dotjp(const char *u) {
  for (const char *p = strstr(u, ".jp"); p; p = strstr(p + 1, ".jp")) {
    char nx = p[3];
    if (nx == 0 || nx == '/' || nx == ':') return 1;
    /* \b: boundary between \w and non-\w. p[2]='p' is \w; boundary if nx
     * is a non-word char. */
    int w = (isalnum((unsigned char)nx) || nx == '_');
    if (!w) return 1;
  }
  return 0;
}

/* JS isJp: url lowercased; .jp boundary OR any JP keyword (case-insensitive). */
static int is_jp(const char *url) {
  if (!url || !*url) return 0;
  char low[2048];
  size_t i = 0;
  for (; url[i] && i + 1 < sizeof low; i++)
    low[i] = (char)tolower((unsigned char)url[i]);
  low[i] = 0;
  if (has_dotjp(low)) return 1;
  for (int k = 0; k < NKW; k++)
    if (strstr(low, KW[k])) return 1;
  /* 'apple\.com.*jp' — "apple.com" then "jp" later */
  const char *a = strstr(low, "apple.com");
  if (a && strstr(a + 9, "jp")) return 1;
  return 0;
}

/* new URL(u).hostname — scheme://[userinfo@]host[:port]/...  NULL on failure
 * (mirrors JS try/catch → null). Writes into out, returns out or NULL. */
static const char *url_host(const char *u, char *out, size_t n) {
  if (!u) return NULL;
  const char *p = strstr(u, "://");
  if (!p) return NULL;
  p += 3;
  const char *at = NULL;
  for (const char *q = p; *q && *q != '/' && *q != '?' && *q != '#'; q++)
    if (*q == '@') at = q;
  if (at) p = at + 1;
  size_t o = 0;
  while (*p && *p != '/' && *p != '?' && *p != '#' && *p != ':' && o + 1 < n)
    out[o++] = *p++;
  if (o == 0) return NULL;
  out[o] = 0;
  return out;
}

/* split text into trimmed non-empty lines; cb per line. */
typedef void (*line_cb)(char *line, void *ud);
static void for_lines(char *text, line_cb cb, void *ud) {
  char *s = text;
  while (s && *s) {
    char *nl = strpbrk(s, "\r\n");
    char *end = nl ? nl : s + strlen(s);
    char *e2 = end;
    char *b = s;
    while (b < e2 && (*b == ' ' || *b == '\t')) b++;
    while (e2 > b && (e2[-1] == ' ' || e2[-1] == '\t')) e2--;
    char saved = *e2; *e2 = 0;
    if (*b) cb(b, ud);
    *e2 = saved;
    if (!nl) break;
    s = (nl[0] == '\r' && nl[1] == '\n') ? nl + 2 : nl + 1;
  }
}

struct ctx_t { intel_sink *sink; int count; int cap; };

static void op_line(char *u, void *udp) {
  struct ctx_t *st = udp;
  if (st->count >= st->cap) return;
  if (!is_jp(u)) return;

  char hb[512];
  const char *host = url_host(u, hb, sizeof hb);

  char rk[24];
  const char *parts[] = { "op", u };
  feed_hash_key(rk, parts, 2);

  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("phishing"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("openphish"));
  if (host) {
    char hb2[560]; snprintf(hb2, sizeof hb2, "host:%s", host);
    cJSON_AddItemToArray(tags, cJSON_CreateString(hb2));
  }
  char *tj = cJSON_PrintUnformatted(tags);

  cJSON *p = cJSON_CreateObject();          /* feed, host, full_url */
  cJSON_AddStringToObject(p, "feed", "openphish");
  cJSON_AddItemToObject(p, "host",
    host ? cJSON_CreateString(host) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "full_url", u);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key      = rk;          /* uid = <source>|<hash> */
  it.title           = host ? host : u;
  it.summary         = "OpenPhish feed match";
  it.link            = u;
  it.lang            = "en";
  it.tags_json       = tj;
  it.properties_json = pj;
  if (st->sink->emit(st->sink, &it) >= 0) st->count++;

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
}

/* PhishTank CSV: skip header line, then quote-aware comma split.
 * cols: phish_id,url,phish_detail_url,submission_time,verified,
 *       verification_time,online,target */
struct pt_ctx { struct ctx_t st; int first_skipped; };

static void pt_line(char *line, void *udp) {
  struct pt_ctx *pc = udp;
  if (!pc->first_skipped) { pc->first_skipped = 1; return; }
  if (pc->st.count >= pc->st.cap) return;

  /* split (max index we need is 7 = target) */
  char *cols[8] = {0};
  char buf[8][2048];
  int ci = 0; size_t bo = 0; int inq = 0;
  for (char *q = line; ; q++) {
    char c = *q;
    if (c == '"') { inq = !inq; continue; }
    if ((c == ',' && !inq) || c == 0) {
      if (ci < 8) { buf[ci][bo] = 0; cols[ci] = buf[ci]; }
      ci++; bo = 0;
      if (c == 0) break;
      continue;
    }
    if (ci < 8 && bo + 1 < sizeof buf[0]) buf[ci][bo++] = c;
  }
  const char *phish_id = cols[0] && cols[0][0] ? cols[0] : NULL;
  const char *url      = cols[1] && cols[1][0] ? cols[1] : NULL;
  const char *detail   = cols[2] && cols[2][0] ? cols[2] : NULL;
  const char *submitted= cols[3] && cols[3][0] ? cols[3] : NULL;
  const char *target   = cols[7] && cols[7][0] ? cols[7] : NULL;

  if (!is_jp(url ? url : "")) return;

  char hb[512];
  const char *host = url_host(url, hb, sizeof hb);

  /* uid: intelUid(SOURCE_ID, phish_id, intelHashKey('pt', url)) — first
   * non-empty candidate; phish_id wins if present, else the hash. */
  char hash[24];
  const char *hp[] = { "pt", url };
  feed_hash_key(hash, hp, 2);
  const char *rk = phish_id ? phish_id : hash;

  /* title = target ? `${target} — ${host||url}` : (host||url) */
  char title[1024];
  const char *hu = host ? host : (url ? url : "");
  if (target) snprintf(title, sizeof title, "%s \xe2\x80\x94 %s", target, hu);
  else snprintf(title, sizeof title, "%s", hu);

  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("phishing"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("phishtank"));
  if (target) {
    char tb[560]; snprintf(tb, sizeof tb, "target:%s", target);
    cJSON_AddItemToArray(tags, cJSON_CreateString(tb));
  }
  char *tj = cJSON_PrintUnformatted(tags);

  cJSON *p = cJSON_CreateObject();   /* feed,host,full_url,phish_id,target,detail */
  cJSON_AddStringToObject(p, "feed", "phishtank");
  cJSON_AddItemToObject(p, "host",
    host ? cJSON_CreateString(host) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "full_url",
    url ? cJSON_CreateString(url) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "phish_id",
    phish_id ? cJSON_CreateString(phish_id) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "target",
    target ? cJSON_CreateString(target) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "detail",
    detail ? cJSON_CreateString(detail) : cJSON_CreateNull());
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.summary         = detail;                 /* r.detail || null */
  it.link            = url;
  it.lang            = "en";
  it.published_at    = submitted;              /* r.submitted || null */
  it.tags_json       = tj;
  it.properties_json = pj;
  if (pc->st.sink->emit(pc->st.sink, &it) >= 0) pc->st.count++;

  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0;

  char *op = feed_get_text(ctx->http, URL_OPENPHISH, TIMEOUT_MS);
  if (op) {
    struct ctx_t st = { sink, 0, 250 };
    for_lines(op, op_line, &st);
    total += st.count;
    free(op);
  }

  const char *ptkey = getenv("PHISHTANK_APP_KEY");
  char pturl[256];
  if (ptkey && *ptkey)
    snprintf(pturl, sizeof pturl, "%s?app_key=%s", URL_PHISHTANK, ptkey);
  else
    snprintf(pturl, sizeof pturl, "%s", URL_PHISHTANK);
  char *pt = feed_get_text(ctx->http, pturl, TIMEOUT_MS);
  if (pt) {
    struct pt_ctx pc = { { sink, 0, 250 }, 0 };
    for_lines(pt, pt_line, &pc);
    total += pc.st.count;
    free(pt);
  }

  fprintf(stderr, "[phishing-feeds-jp] emitted %d\n", total);
  return total >= 0 ? 0 : -1;
}

static const source_def phishing_feeds_jp_def = {
  .id = "phishing-feeds-jp", .collector = "cyber",
  .name = "Phishing feeds (JP brands)",
  .name_ja = "\xe3\x83\x95\xe3\x82\xa3\xe3\x83\x83\xe3\x82\xb7\xe3\x83\xb3\xe3\x82\xb0 (\xe6\x97\xa5\xe6\x9c\xac\xe3\x83\x96\xe3\x83\xa9\xe3\x83\xb3\xe3\x83\x89)",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(phishing_feeds_jp_def)
