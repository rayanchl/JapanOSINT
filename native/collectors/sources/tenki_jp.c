/* collectors/environment/sources/tenki_jp.c
 * INTEL source — port of server/src/collectors/tenkiJp.js.
 * Best-effort HTML scrape of the homepage weather-telop blocks; on any
 * fetch/parse miss fall back to ONE reachability portal item.
 * uid = tenki-jp|<sha1(telop|n)[:20]> or tenki-jp|portal. */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include "_timefmt.inc"
#include <openssl/evp.h>   /* SHA-1 via EVP; see hash_key() for why not sha.h */

#define HOME "https://tenki.jp/"
/* Regional forecast page: this is where the per-city highs/lows actually live.
 * The homepage carries navigation and a map, not readable values. */
#define FORECAST "https://tenki.jp/forecast/3/16/"

/* intelHashKey(telop, n): sha1( telop "|" n "|" )[:20 hex]. Returns 1 on
 * success, 0 on failure (out21 emptied).
 *
 * Written on EVP rather than the SHA1_Init/Update/Final trio, which OpenSSL 3.0
 * deprecates along with the whole low-level SHA_CTX surface. The DIGEST IS
 * UNCHANGED: same FIPS-180 SHA-1 over the same byte stream, same "|"
 * separators, same decimal rendering of `n`. That is the whole point — this is
 * the remote_key for every tenki.jp forecast row, so a digest that shifted by a
 * byte would re-key the entire retained corpus and re-emit it as new. Proven
 * byte-for-byte against the old implementation before the swap.
 *
 * EVP introduces a failure mode SHA1_Init did not have (context allocation), so
 * the function now reports it rather than leaving the caller's buffer holding
 * whatever was on the stack. run() skips a city it cannot key — an unkeyed row
 * would collide with the next one under an arbitrary identity. */
static int hash_key(char *out21, const char *telop, int n) {
  out21[0] = 0;
  unsigned char d[EVP_MAX_MD_SIZE]; unsigned int dl = 0;
  char nb[16]; snprintf(nb, sizeof nb, "%d", n);
  EVP_MD_CTX *c = EVP_MD_CTX_new();
  if (!c) return 0;
  int ok = EVP_DigestInit_ex(c, EVP_sha1(), NULL) == 1
        && EVP_DigestUpdate(c, telop, strlen(telop)) == 1
        && EVP_DigestUpdate(c, "|", 1) == 1
        && EVP_DigestUpdate(c, nb, strlen(nb)) == 1
        && EVP_DigestUpdate(c, "|", 1) == 1
        && EVP_DigestFinal_ex(c, d, &dl) == 1;
  EVP_MD_CTX_free(c);
  if (!ok || dl < 10) return 0;
  for (int i = 0; i < 10; i++) sprintf(out21 + i*2, "%02x", d[i]);
  out21[20] = 0;
  return 1;
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
  /* collapse whitespace + trim. Bound the scratch by `out`'s own capacity as
   * well as its own, so the copy-back below is an exact-length copy that can
   * never truncate what we just decoded. */
  char tmp[1024]; size_t to = 0; int sp = 0;
  size_t tcap = cap < sizeof tmp ? cap : sizeof tmp;
  for (size_t i = 0; out[i] && to + 1 < tcap; i++) {
    unsigned char ch = (unsigned char)out[i];
    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v') {
      if (to > 0 && !sp) { tmp[to++] = ' '; sp = 1; }
    } else { tmp[to++] = (char)ch; sp = 0; }
  }
  while (to > 0 && tmp[to-1] == ' ') to--;
  tmp[to] = 0;
  memcpy(out, tmp, to + 1);      /* to + 1 <= tcap <= cap */
}

/* A strip_tags() lived here — the `m[1].replace(/<[^>]+>/g,' ')` leg of the
 * retired weather-telop scan (see run()). The scan went; this went with it and
 * nothing has called it since. Verified against the live page before deleting
 * rather than wiring it in, because a dead tag-stripper in an HTML scraper
 * usually means raw markup is reaching titles. Here it is not: every text this
 * file lifts is bounded to a single text node. tenki.jp ships each value as a
 * plain `<span class="max-temp">34</span>` / `<span class="prob-precip">10%
 * </span>`, and the city is the text node between the `<a …>` and the first
 * `<br>`, so cls_text()'s "from '>' to the next '<'" grab and the city grab
 * both stop before a tag could be captured. decode() handles the entities. */

/* Pull the inner text of the first `class="<cls>"` element at/after `from`.
 * Returns the position just past it, or NULL. tenki.jp wraps each value in a
 * plain <span class="max-temp">36</span>, so a class-scoped text grab is enough
 * and stays tolerant of attribute reordering. */
static const char *cls_text(const char *from, const char *cls,
                            char *out, size_t cap) {
  char needle[64];
  snprintf(needle, sizeof needle, "class=\"%s\"", cls);
  const char *h = strstr(from, needle);
  if (!h) return NULL;
  const char *gt = strchr(h, '>');
  if (!gt) return NULL;
  const char *lt = strchr(gt + 1, '<');
  if (!lt) return NULL;
  size_t rl = (size_t)(lt - (gt + 1));
  char raw[512];
  if (rl >= sizeof raw) rl = sizeof raw - 1;
  memcpy(raw, gt + 1, rl); raw[rl] = 0;
  decode(raw, out, cap);
  return lt;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *html = feed_get_text(ctx->http, HOME, 10000);
  int reachable = html != NULL;
  char now[40]; jo_now_iso_ms(now, sizeof now);
  int n = 0;

  /* tenki.jp's homepage no longer carries `weather-telop` anywhere — the site
   * moved to `forecast-point-city-name` + `max-temp`/`min-temp` on the regional
   * forecast pages, so the old scan matched nothing and the collector fell
   * through to a bare portal row every run. Read the real values instead: a
   * city plus its forecast high/low is a measurement, a telop string was only
   * a label. */
  if (html) {
    char *fc = feed_get_text(ctx->http, FORECAST, 10000);
    if (fc) {
      /* Each municipality is one <a id="forecast-map-entry-NNNNN" …> carrying
       * the city name, a weather telop in the icon's alt=, max/min temp and a
       * precipitation probability — a complete observation per row. */
      const char *cur = fc;
      while (n < 60) {
        const char *a = strstr(cur, "id=\"forecast-map-entry-");
        if (!a) break;
        /* back up to the '<a' that owns this id */
        const char *tagstart = a;
        while (tagstart > fc && *tagstart != '<') tagstart--;
        const char *gt = strchr(a, '>');
        const char *end = gt ? strstr(gt, "</a>") : NULL;
        if (!gt || !end) break;
        cur = end + 4;

        /* city: the text node immediately after the opening tag, up to <br> */
        char city[128] = {0};
        {
          const char *br = strstr(gt + 1, "<");
          size_t cl = br ? (size_t)(br - (gt + 1)) : 0;
          char raw[256];
          if (cl >= sizeof raw) cl = sizeof raw - 1;
          if (cl) { memcpy(raw, gt + 1, cl); raw[cl] = 0; decode(raw, city, sizeof city); }
        }
        if (!city[0]) continue;

        char hi[16] = {0}, lo[16] = {0}, pp[16] = {0}, telop[96] = {0}, href[256] = {0};
        cls_text(gt, "max-temp",    hi,  sizeof hi);
        cls_text(gt, "min-temp",    lo,  sizeof lo);
        cls_text(gt, "prob-precip", pp,  sizeof pp);
        {   /* icon alt= is the human-readable forecast ("晴のち曇") */
          const char *al = strstr(gt, "alt=\"");
          if (al && al < end) {
            const char *ae = strchr(al + 5, '"');
            size_t tl = ae ? (size_t)(ae - (al + 5)) : 0;
            if (tl && tl < sizeof telop) { memcpy(telop, al + 5, tl); telop[tl] = 0; }
          }
          const char *hr = strstr(tagstart, "href=\"");
          if (hr && hr < gt) {
            const char *he = strchr(hr + 6, '"');
            size_t hl = he ? (size_t)(he - (hr + 6)) : 0;
            if (hl && hl < sizeof href - 24)
              snprintf(href, sizeof href, "https://tenki.jp%.*s", (int)hl, hr + 6);
          }
        }
        /* Require a real number: the page ships "---" placeholders for blocks
         * it fills in client-side, and a placeholder is not an observation. */
        if (!isdigit((unsigned char)hi[0]) || !isdigit((unsigned char)lo[0])) continue;

        char hk[24];
        if (!hash_key(hk, city, 0)) continue;   /* no key → no honest row */
        cJSON *tags = cJSON_CreateArray();
        cJSON_AddItemToArray(tags, cJSON_CreateString("weather"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("tenki-jp"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("forecast"));
        char *tj = cJSON_PrintUnformatted(tags);
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "city", city);
        if (telop[0]) cJSON_AddStringToObject(p, "telop", telop);
        cJSON_AddNumberToObject(p, "max_temp_c", atof(hi));
        cJSON_AddNumberToObject(p, "min_temp_c", atof(lo));
        if (pp[0]) cJSON_AddStringToObject(p, "precip_prob", pp);
        char *pj = cJSON_PrintUnformatted(p);
        char title[256], summ[160];
        snprintf(title, sizeof title, "%s %s %s\xE2\x84\x83/%s\xE2\x84\x83",
                 city, telop[0] ? telop : "", hi, lo);
        snprintf(summ, sizeof summ,
                 "%s\xE2\x80\x94 \xE6\x9C\x80\xE9\xAB\x98%s\xE2\x84\x83 "
                 "\xE6\x9C\x80\xE4\xBD\x8E%s\xE2\x84\x83%s%s",
                 telop[0] ? telop : "", hi, lo,
                 pp[0] ? " \xE9\x99\x8D\xE6\xB0\xB4\xE7\xA2\xBA\xE7\x8E\x87" : "",
                 pp[0] ? pp : "");

        intel_item it = {0};
        it.remote_key = hk;
        it.title = title;
        it.summary = summ;
        it.link = href[0] ? href : FORECAST;
        it.author = "\xE6\x97\xA5\xE6\x9C\xAC\xE6\xB0\x97\xE8\xB1\xA1\xE5\x8D\x94\xE4\xBC\x9A tenki.jp";
        it.lang = "ja";
        it.published_at = now[0] ? now : NULL;
        it.record_type = "weather-observation";
        it.tags_json = tj;
        it.properties_json = pj;
        if (sink->emit(sink, &it) >= 0) n++;
        free(tj); free(pj);
        cJSON_Delete(tags); cJSON_Delete(p);
      }
      free(fc);
    }
  }

  free(html);   /* the only free() used to sit inside a dead `if (0)` copy of
                 * the retired weather-telop scan, so every run leaked the
                 * whole homepage. The scan itself is gone; the free is not. */

  /* No portal-probe fallback: a row that only says "the site answered" is not
   * an observation (contract R1), and it was the last `service-portal` row in
   * the fleet. A run that scrapes nothing now emits nothing. */

  fprintf(stderr, "[tenki-jp] emitted %d (reachable=%d)\n", n, reachable);
  return 0;              /* audit-09: an empty result set is not a run error */
}

static const source_def tenki_jp_def = {
  .id = "tenki-jp", .collector = "environment",
  .name = "tenki.jp Weather", .name_ja = "tenki.jp 天気",
  .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(tenki_jp_def)
