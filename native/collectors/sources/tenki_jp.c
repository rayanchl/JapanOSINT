/* collectors/environment/sources/tenki_jp.c
 * INTEL source — port of server/src/collectors/tenkiJp.js.
 * Best-effort HTML scrape of the homepage weather-telop blocks; on any
 * fetch/parse miss fall back to ONE reachability portal item.
 * uid = tenki-jp|<sha1(telop|n)[:20]> or tenki-jp|portal. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <openssl/sha.h>

#define HOME "https://tenki.jp/"

static void iso_now(char *out, size_t n) {
  time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
  strftime(out, n, "%Y-%m-%dT%H:%M:%S.000Z", &g);
}

/* intelHashKey(telop, n): sha1( telop "|" n "|" )[:20 hex] */
static void hash_key(char *out21, const char *telop, int n) {
  unsigned char d[20]; SHA_CTX c; SHA1_Init(&c);
  SHA1_Update(&c, telop, strlen(telop)); SHA1_Update(&c, "|", 1);
  char nb[16]; snprintf(nb, sizeof nb, "%d", n);
  SHA1_Update(&c, nb, strlen(nb)); SHA1_Update(&c, "|", 1);
  SHA1_Final(d, &c);
  for (int i = 0; i < 10; i++) sprintf(out21 + i*2, "%02x", d[i]);
  out21[20] = 0;
}

/* decode(): strip tags already done by caller; entity-decode + whitespace
 * collapse + trim, mirroring the JS decode(). */
static void decode(const char *in, char *out, size_t cap) {
  size_t o = 0;
  for (const char *p = in; *p && o + 4 < cap; ) {
    if (!strncmp(p, "&amp;", 5))      { out[o++]='&'; p+=5; }
    else if (!strncmp(p, "&lt;", 4))  { out[o++]='<'; p+=4; }
    else if (!strncmp(p, "&gt;", 4))  { out[o++]='>'; p+=4; }
    else if (!strncmp(p, "&quot;", 6)){ out[o++]='"'; p+=6; }
    else if (!strncmp(p, "&#", 2)) {
      const char *q = p + 2; long code = strtol(q, (char**)&q, 10);
      if (*q == ';') { if (code > 0 && code < 128) out[o++] = (char)code; p = q + 1; }
      else out[o++] = *p++;
    } else out[o++] = *p++;
  }
  out[o] = 0;
  /* collapse whitespace + trim */
  char tmp[1024]; size_t to = 0; int sp = 0;
  for (size_t i = 0; out[i] && to + 1 < sizeof tmp; i++) {
    unsigned char ch = (unsigned char)out[i];
    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v') {
      if (to > 0 && !sp) { tmp[to++] = ' '; sp = 1; }
    } else { tmp[to++] = (char)ch; sp = 0; }
  }
  while (to > 0 && tmp[to-1] == ' ') to--;
  tmp[to] = 0;
  strncpy(out, tmp, cap - 1); out[cap-1] = 0;
}

/* strip <...> tags into spaces (m[1].replace(/<[^>]+>/g,' ')) */
static void strip_tags(const char *in, size_t len, char *out, size_t cap) {
  size_t o = 0; int intag = 0;
  for (size_t i = 0; i < len && o + 1 < cap; i++) {
    char ch = in[i];
    if (ch == '<') { intag = 1; if (o + 1 < cap) out[o++] = ' '; }
    else if (ch == '>') intag = 0;
    else if (!intag) out[o++] = ch;
  }
  out[o] = 0;
}

static void push_portal(intel_sink *sink, int reachable, const char *now,
                        int *n) {
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("weather"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("tenki-jp"));
  cJSON_AddItemToArray(tags, cJSON_CreateString(reachable ? "reachable" : "unreachable"));
  char *tj = cJSON_PrintUnformatted(tags);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddBoolToObject(p, "reachable", reachable);
  cJSON_AddBoolToObject(p, "machine_readable", 0);
  char *pj = cJSON_PrintUnformatted(p);

  intel_item it = {0};
  it.remote_key = "portal";
  it.title = "tenki.jp (\xE6\x97\xA5\xE6\x9C\xAC\xE6\xB0\x97\xE8\xB1\xA1\xE5\x8D\x94\xE4\xBC\x9A)";
  it.summary = "Japan Weather Association forecast portal";
  it.body = "No structured forecast could be scraped (no public API/RSS); portal reachability only.";
  it.link = HOME;
  it.author = "\xE6\x97\xA5\xE6\x9C\xAC\xE6\xB0\x97\xE8\xB1\xA1\xE5\x8D\x94\xE4\xBC\x9A tenki.jp";
  it.lang = "ja";
  it.published_at = now;
  it.tags_json = tj;
  it.properties_json = pj;
  if (sink->emit(sink, &it) >= 0) (*n)++;
  free(tj); free(pj);
  cJSON_Delete(tags); cJSON_Delete(p);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *html = feed_get_text(ctx->http, HOME, 10000);
  int reachable = html != NULL;
  char now[40]; iso_now(now, sizeof now);
  int n = 0;

  if (html) {
    /* /<(p|span|div)[^>]*class="[^"]*weather-telop[^"]*"[^>]*>(.*?)<\/...>/gi
       greedy-safe scan: find an opening tag whose attributes contain
       class="...weather-telop..." then capture up to the next matching close. */
    const char *cur = html;
    int matched = 0;
    while (matched < 12) {
      const char *lt = NULL;
      const char *q = cur;
      const char *open_name = NULL; int name_len = 0;
      const char *gt = NULL;
      for (;;) {
        q = strchr(q, '<');
        if (!q) { lt = NULL; break; }
        const char *nm = NULL; int nl = 0;
        if (!strncasecmp(q, "<p", 2) && !isalpha((unsigned char)q[2])) { nm="p"; nl=1; }
        else if (!strncasecmp(q, "<span", 5)) { nm="span"; nl=4; }
        else if (!strncasecmp(q, "<div", 4)) { nm="div"; nl=3; }
        if (nm) {
          const char *e = strchr(q, '>');
          if (e) {
            char attrs[1024]; size_t al = (size_t)(e - q);
            if (al >= sizeof attrs) al = sizeof attrs - 1;
            memcpy(attrs, q, al); attrs[al] = 0;
            const char *cls = strcasestr(attrs, "class=");
            if (cls && strstr(cls, "weather-telop")) {
              lt = q; open_name = nm; name_len = nl + 1; gt = e; break;
            }
          }
        }
        q++;
      }
      if (!lt) break;
      char close[16]; snprintf(close, sizeof close, "</%.*s>", name_len, open_name);
      const char *ce = strcasestr(gt + 1, close);
      if (!ce) break;
      char raw[2048]; size_t rl = (size_t)(ce - (gt + 1));
      if (rl >= sizeof raw) rl = sizeof raw - 1;
      char stripped[2048], telop[1024];
      strip_tags(gt + 1, rl, stripped, sizeof stripped);
      decode(stripped, telop, sizeof telop);
      cur = ce + strlen(close);
      if (!telop[0] || strlen(telop) > 40) continue;
      matched++;

      char hk[24]; hash_key(hk, telop, matched);
      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("weather"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("tenki-jp"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("forecast"));
      char *tj = cJSON_PrintUnformatted(tags);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "telop", telop);
      char *pj = cJSON_PrintUnformatted(p);
      char title[128];
      snprintf(title, sizeof title, "tenki.jp \xE5\xA4\xA9\xE6\xB0\x97: %s", telop);

      intel_item it = {0};
      it.remote_key = hk;
      it.title = title;
      it.summary = telop;
      it.body = telop;
      it.link = HOME;
      it.author = "\xE6\x97\xA5\xE6\x9C\xAC\xE6\xB0\x97\xE8\xB1\xA1\xE5\x8D\x94\xE4\xBC\x9A tenki.jp";
      it.lang = "ja";
      it.published_at = now;
      it.tags_json = tj;
      it.properties_json = pj;
      if (sink->emit(sink, &it) >= 0) n++;
      free(tj); free(pj);
      cJSON_Delete(tags); cJSON_Delete(p);
    }
    free(html);
  }

  if (n == 0) push_portal(sink, reachable, now, &n);

  fprintf(stderr, "[tenki-jp] emitted %d (reachable=%d)\n", n, reachable);
  return n > 0 ? 0 : -1;
}

static const source_def tenki_jp_def = {
  .id = "tenki-jp", .collector = "environment",
  .name = "tenki.jp Weather", .name_ja = "tenki.jp 天気",
  .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(tenki_jp_def)
