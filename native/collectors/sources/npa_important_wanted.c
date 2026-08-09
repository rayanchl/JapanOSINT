/* collectors/crime/sources/npa_important_wanted.c
 * Port of server/src/collectors/npaImportantWanted.js (fetchText multi-page).
 * Two NPA pages (jyuyo1/jyuyo2.html) -> <section> blocks; each yields one
 * suspect (name, age, height, crime, photo, handling-force link). A suspect
 * whose handling prefecture resolves becomes a jittered map feature
 * (id JYUYO_<code>_<i+1>); an unresolved one becomes an intel item
 * (uid intelUid(SOURCE_ID,name)). _meta/SEED n/a (this collector has none).
 * Feature uid/title derived by the geojson sink. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#define NPA_BASE "https://www.npa.go.jp"

typedef struct { const char *code, *ja; double lat, lon; } pref_t;
static const pref_t PREFS[] = {
 {"01","\xe5\x8c\x97\xe6\xb5\xb7\xe9\x81\x93",43.0628,141.3478},
 {"02","\xe9\x9d\x92\xe6\xa3\xae\xe7\x9c\x8c",40.8244,140.7400},
 {"03","\xe5\xb2\xa9\xe6\x89\x8b\xe7\x9c\x8c",39.7036,141.1525},
 {"04","\xe5\xae\xae\xe5\x9f\x8e\xe7\x9c\x8c",38.2683,140.8719},
 {"05","\xe7\xa7\x8b\xe7\x94\xb0\xe7\x9c\x8c",39.7186,140.1024},
 {"06","\xe5\xb1\xb1\xe5\xbd\xa2\xe7\x9c\x8c",38.2403,140.3633},
 {"07","\xe7\xa6\x8f\xe5\xb3\xb6\xe7\x9c\x8c",37.7503,140.4675},
 {"08","\xe8\x8c\xa8\xe5\x9f\x8e\xe7\x9c\x8c",36.3658,140.4711},
 {"09","\xe6\xa0\x83\xe6\x9c\xa8\xe7\x9c\x8c",36.5658,139.8836},
 {"10","\xe7\xbe\xa4\xe9\xa6\xac\xe7\x9c\x8c",36.3911,139.0608},
 {"11","\xe5\x9f\xbc\xe7\x8e\x89\xe7\x9c\x8c",35.8617,139.6455},
 {"12","\xe5\x8d\x83\xe8\x91\x89\xe7\x9c\x8c",35.6083,140.1233},
 {"13","\xe6\x9d\xb1\xe4\xba\xac\xe9\x83\xbd",35.6895,139.6917},
 {"14","\xe7\xa5\x9e\xe5\xa5\x88\xe5\xb7\x9d\xe7\x9c\x8c",35.4437,139.6380},
 {"15","\xe6\x96\xb0\xe6\xbd\x9f\xe7\x9c\x8c",37.9161,139.0364},
 {"16","\xe5\xaf\x8c\xe5\xb1\xb1\xe7\x9c\x8c",36.6953,137.2113},
 {"17","\xe7\x9f\xb3\xe5\xb7\x9d\xe7\x9c\x8c",36.5946,136.6256},
 {"18","\xe7\xa6\x8f\xe4\xba\x95\xe7\x9c\x8c",36.0652,136.2216},
 {"19","\xe5\xb1\xb1\xe6\xa2\xa8\xe7\x9c\x8c",35.6642,138.5683},
 {"20","\xe9\x95\xb7\xe9\x87\x8e\xe7\x9c\x8c",36.6489,138.1944},
 {"21","\xe5\xb2\x90\xe9\x98\x9c\xe7\x9c\x8c",35.4233,136.7606},
 {"22","\xe9\x9d\x99\xe5\xb2\xa1\xe7\x9c\x8c",34.9756,138.3828},
 {"23","\xe6\x84\x9b\xe7\x9f\xa5\xe7\x9c\x8c",35.1814,136.9069},
 {"24","\xe4\xb8\x89\xe9\x87\x8d\xe7\x9c\x8c",34.7184,136.5067},
 {"25","\xe6\xbb\x8b\xe8\xb3\x80\xe7\x9c\x8c",35.0044,135.8686},
 {"26","\xe4\xba\xac\xe9\x83\xbd\xe5\xba\x9c",35.0116,135.7681},
 {"27","\xe5\xa4\xa7\xe9\x98\xaa\xe5\xba\x9c",34.6864,135.5197},
 {"28","\xe5\x85\xb5\xe5\xba\xab\xe7\x9c\x8c",34.6913,135.1830},
 {"29","\xe5\xa5\x88\xe8\x89\xaf\xe7\x9c\x8c",34.6850,135.8048},
 {"30","\xe5\x92\x8c\xe6\xad\x8c\xe5\xb1\xb1\xe7\x9c\x8c",34.2261,135.1675},
 {"31","\xe9\xb3\xa5\xe5\x8f\x96\xe7\x9c\x8c",35.5039,134.2378},
 {"32","\xe5\xb3\xb6\xe6\xa0\xb9\xe7\x9c\x8c",35.4722,133.0506},
 {"33","\xe5\xb2\xa1\xe5\xb1\xb1\xe7\x9c\x8c",34.6628,133.9197},
 {"34","\xe5\xba\x83\xe5\xb3\xb6\xe7\x9c\x8c",34.3853,132.4553},
 {"35","\xe5\xb1\xb1\xe5\x8f\xa3\xe7\x9c\x8c",34.1856,131.4714},
 {"36","\xe5\xbe\xb3\xe5\xb3\xb6\xe7\x9c\x8c",34.0658,134.5594},
 {"37","\xe9\xa6\x99\xe5\xb7\x9d\xe7\x9c\x8c",34.3401,134.0434},
 {"38","\xe6\x84\x9b\xe5\xaa\x9b\xe7\x9c\x8c",33.8392,132.7656},
 {"39","\xe9\xab\x98\xe7\x9f\xa5\xe7\x9c\x8c",33.5594,133.5311},
 {"40","\xe7\xa6\x8f\xe5\xb2\xa1\xe7\x9c\x8c",33.5904,130.4017},
 {"41","\xe4\xbd\x90\xe8\xb3\x80\xe7\x9c\x8c",33.2494,130.2989},
 {"42","\xe9\x95\xb7\xe5\xb4\x8e\xe7\x9c\x8c",32.7503,129.8775},
 {"43","\xe7\x86\x8a\xe6\x9c\xac\xe7\x9c\x8c",32.8019,130.7256},
 {"44","\xe5\xa4\xa7\xe5\x88\x86\xe7\x9c\x8c",33.2381,131.6126},
 {"45","\xe5\xae\xae\xe5\xb4\x8e\xe7\x9c\x8c",31.9111,131.4239},
 {"46","\xe9\xb9\xbf\xe5\x85\x90\xe5\xb3\xb6\xe7\x9c\x8c",31.5963,130.5571},
 {"47","\xe6\xb2\x96\xe7\xb8\x84\xe7\x9c\x8c",26.2125,127.6809},
};
#define NPREF ((int)(sizeof PREFS / sizeof PREFS[0]))

static const struct { const char *host, *code; } HOST2PREF[] = {
  {"keishicho.metro.tokyo.lg.jp","13"},{"police.pref.kanagawa.jp","14"},
  {"police.pref.osaka.lg.jp","27"},{"police.pref.hyogo.lg.jp","28"},
  {"police.pref.fukuoka.jp","40"},{"police.pref.chiba.jp","12"},
  {"police.pref.saitama.lg.jp","11"},{"police.pref.aichi.jp","23"},
  {"police.pref.hokkaido.lg.jp","01"},{"police.pref.miyagi.jp","04"},
  {"police.pref.gunma.jp","10"},{"police.pref.aomori.jp","02"},
  {"police.pref.akita.lg.jp","05"},{"police.pref.fukushima.jp","07"},
  {"police.pref.mie.jp","24"},{"police.pref.hiroshima.lg.jp","34"},
  {"police.pref.ehime.jp","38"},{"police.pref.kochi.lg.jp","39"},
  {"police.pref.nagasaki.jp","42"},{"police.pref.kumamoto.jp","43"},
  {"police.pref.nara.jp","29"},{"police.pref.saga.jp","41"},
  {"police.pref.oita.jp","44"},{"police.pref.okinawa.jp","47"},
  {"police.pref.yamaguchi.jp","35"},{"police.pref.wakayama.lg.jp","30"},
};
#define NH2P ((int)(sizeof HOST2PREF / sizeof HOST2PREF[0]))

static const pref_t *pref_by_code(const char *code) {
  for (int i = 0; i < NPREF; i++)
    if (strcmp(PREFS[i].code, code) == 0) return &PREFS[i];
  return NULL;
}
/* resolvePrefecture by exact JA name (BY_JA path; suffix-strip equiv) */
static const pref_t *pref_by_ja(const char *ja) {
  for (int i = 0; i < NPREF; i++)
    if (strcmp(PREFS[i].ja, ja) == 0) return &PREFS[i];
  return NULL;
}

/* hostname from a URL (resolved against NPA_BASE if relative). */
static void url_host(const char *url, char *out, size_t n) {
  out[0] = 0;
  const char *p = url;
  if (strncmp(p, "http://", 7) == 0) p += 7;
  else if (strncmp(p, "https://", 8) == 0) p += 8;
  else { /* relative -> NPA_BASE host */ p = "www.npa.go.jp"; }
  size_t i = 0;
  while (p[i] && p[i] != '/' && p[i] != ':' && i < n - 1) { out[i] = p[i]; i++; }
  out[i] = 0;
}

/* JS String.prototype.trim() */
static void js_trim(char *s) {
  size_t L = strlen(s);
  while (L && (s[L-1]==' '||s[L-1]=='\t'||s[L-1]=='\n'||s[L-1]=='\r'||
               s[L-1]=='\f'||s[L-1]=='\v')) s[--L] = 0;
  size_t i = 0;
  while (s[i]==' '||s[i]=='\t'||s[i]=='\n'||s[i]=='\r'||
         s[i]=='\f'||s[i]=='\v') i++;
  if (i) memmove(s, s + i, strlen(s + i) + 1);
}
/* replace(/\s+/g,' ') then trim — collapse ASCII whitespace runs. */
static void collapse_ws(char *s) {
  char *r = s, *w = s; int sp = 0;
  for (; *r; r++) {
    if (*r==' '||*r=='\t'||*r=='\n'||*r=='\r'||*r=='\f'||*r=='\v') {
      if (!sp) { *w++ = ' '; sp = 1; }
    } else { *w++ = *r; sp = 0; }
  }
  *w = 0;
  js_trim(s);
}

/* /<h2>(?:<a[^>]*>)?([^<（]+?)(?:\s*（(\d+)歳）)?(?:<\/a>)?<\/h2>/  (first)
 * The fullwidth '（' = EF BC 88, '）' = EF BC 89, '歳' = E6 AD B3. */
static int parse_name(const char *blk, char *name, size_t nn,
                      int *has_age, long *age) {
  *has_age = 0; *age = 0; name[0] = 0;
  const char *h2 = strstr(blk, "<h2>");
  if (!h2) return 0;
  const char *c = h2 + 4;
  const char *end = strstr(c, "</h2>");
  if (!end) return 0;
  /* optional leading <a ...> */
  if (strncmp(c, "<a", 2) == 0) {
    const char *gt = strchr(c, '>');
    if (gt && gt < end) c = gt + 1;
  }
  /* name = lazy [^<（]+? ; in practice run of bytes until '<' or '（' */
  const char *q = c;
  size_t k = 0;
  while (q < end && *q != '<' &&
         !((unsigned char)q[0]==0xEF && (unsigned char)q[1]==0xBC &&
           (unsigned char)q[2]==0x88)) {
    if (k < nn - 1) name[k++] = *q;
    q++;
  }
  name[k] = 0;
  if (k == 0) return 0;                 /* +? requires >=1 char */
  /* optional （(\d+)歳） right after (allow leading \s*) */
  const char *r = q;
  while (r < end && (*r==' '||*r=='\t'||*r=='\n'||*r=='\r')) r++;
  if (r + 3 <= end && (unsigned char)r[0]==0xEF &&
      (unsigned char)r[1]==0xBC && (unsigned char)r[2]==0x88) {
    const char *d = r + 3;
    char num[16]; int ni = 0;
    while (d < end && isdigit((unsigned char)*d) && ni < 15) num[ni++] = *d++;
    /* require 歳 (E6 AD B3) then （ close */
    if (ni > 0 && d + 3 <= end && (unsigned char)d[0]==0xE6 &&
        (unsigned char)d[1]==0xAD && (unsigned char)d[2]==0xB3) {
      const char *e = d + 3;
      if (e + 3 <= end && (unsigned char)e[0]==0xEF &&
          (unsigned char)e[1]==0xBC && (unsigned char)e[2]==0x89) {
        num[ni] = 0; *age = strtol(num, NULL, 10); *has_age = 1;
      }
    }
  }
  return 1;
}

/* first /<img[^>]*src="([^"]+)"/ */
static int parse_img(const char *blk, char *out, size_t n) {
  const char *s = blk;
  while ((s = strstr(s, "<img"))) {
    const char *q = s + 4;
    const char *sr = strstr(q, "src=\"");
    if (sr) {
      const char *v = sr + 5; const char *e = strchr(v, '"');
      if (e && e > v) {
        size_t L = (size_t)(e - v); if (L >= n) L = n - 1;
        memcpy(out, v, L); out[L] = 0; return 1;
      }
    }
    s += 4;
  }
  return 0;
}

/* first /<p>([\s\S]*?)<\/p>/  (literal <p>) */
static int parse_body(const char *blk, char *out, size_t n) {
  const char *s = strstr(blk, "<p>");
  if (!s) return 0;
  s += 3;
  const char *e = strstr(s, "</p>");
  if (!e) return 0;
  size_t L = (size_t)(e - s); if (L >= n) L = n - 1;
  memcpy(out, s, L); out[L] = 0;
  return 1;
}

/* /身長([0-9０-９ｃcｍm]+)/  身長 = E8 BA AB E9 95 B7 ; class chars:
 * ascii 0-9 c m, fullwidth ０-９ (EF BC X), ｃ (EF BD 83), ｍ (EF BD 8D). */
static int is_height_byte(const unsigned char *p, int *adv) {
  if ((p[0] >= '0' && p[0] <= '9') || p[0]=='c' || p[0]=='m') { *adv=1; return 1; }
  if (p[0]==0xEF && p[1]==0xBC && p[2]>=0x90 && p[2]<=0x99) { *adv=3; return 1; }
  if (p[0]==0xEF && p[1]==0xBD && (p[2]==0x83||p[2]==0x8D)) { *adv=3; return 1; }
  return 0;
}
static int parse_height(const char *txt, char *out, size_t n) {
  const char *s = txt;
  while ((s = strstr(s, "\xe8\xba\xab\xe9\x95\xb7"))) {  /* 身長 */
    const unsigned char *p = (const unsigned char *)(s + 6);
    size_t k = 0; int adv;
    while (*p && is_height_byte(p, &adv)) {
      for (int j = 0; j < adv && k < n - 1; j++) out[k++] = (char)p[j];
      p += adv;
    }
    out[k] = 0;
    if (k > 0) return 1;
    s += 6;
  }
  return 0;
}
/* crime = bodyTxt.replace height-class regex then strip first "位" + space.
 * (JS: .replace(/身長[0-9...cm]+ \/g,'').replace(/位 \/,'').trim());
 * 位 = E4 BD 8D. We remove the FIRST 位 followed by whitespace, non-global. */
static void make_crime(const char *bodyTxt, char *out, size_t n) {
  /* pass 1: drop all 身長<heightclass>+\s* */
  size_t w = 0;
  const unsigned char *p = (const unsigned char *)bodyTxt;
  while (*p && w < n - 1) {
    if (p[0]==0xE8 && p[1]==0xBA && p[2]==0xAB && p[3]==0xE9 &&
        p[4]==0x95 && p[5]==0xB7) {                 /* 身長 */
      const unsigned char *q = p + 6; int adv, any = 0;
      while (*q && is_height_byte(q, &adv)) { q += adv; any = 1; }
      if (any) {
        while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r'||*q=='\f'||*q=='\v') q++;
        p = q; continue;
      }
    }
    out[w++] = (char)*p++;
  }
  out[w] = 0;
  /* pass 2: replace first 位\s* with '' */
  char *pos = strstr(out, "\xe4\xbd\x8d");
  if (pos) {
    char *after = pos + 3;
    while (*after==' '||*after=='\t'||*after=='\n'||*after=='\r'||
           *after=='\f'||*after=='\v') after++;
    memmove(pos, after, strlen(after) + 1);
  }
  js_trim(out);
}

/* first /<li>\s*<a[^>]*href="([^"]+)"[^>]*>([^<]+)<\/a>/ */
static int parse_handler(const char *blk, char *url, size_t un,
                         char *label, size_t ln) {
  const char *s = blk;
  while ((s = strstr(s, "<li>"))) {
    const char *c = s + 4;
    while (*c==' '||*c=='\t'||*c=='\n'||*c=='\r'||*c=='\f'||*c=='\v') c++;
    if (strncmp(c, "<a", 2) != 0) { s += 4; continue; }
    const char *href = strstr(c, "href=\"");
    const char *gt = strchr(c, '>');
    if (!href || !gt || href > gt) { s += 4; continue; }
    const char *hv = href + 6; const char *he = strchr(hv, '"');
    if (!he) { s += 4; continue; }
    const char *txt = gt + 1; const char *te = strchr(txt, '<');
    if (!te || te == txt) { s += 4; continue; }
    size_t ul = (size_t)(he - hv); if (ul >= un) ul = un - 1;
    memcpy(url, hv, ul); url[ul] = 0;
    size_t ll = (size_t)(te - txt); if (ll >= ln) ll = ln - 1;
    memcpy(label, txt, ll); label[ll] = 0;
    return 1;
  }
  return 0;
}

/* extract (?:[一-鿿]{2,4}(?:都|道|府|県)) from label — first match.
 * [一-鿿] = U+4E00..U+9FFF (3-byte UTF-8 E4 B8 80 .. E9 BF BF). suffix
 * 都 E9 83 BD / 道 E9 81 93 / 府 E5 BA 9C / 県 E7 9C 8C. */
static int kanji3(const unsigned char *p) {
  if (!(p[0] && p[1] && p[2])) return 0;
  unsigned cp = ((p[0]&0x0F)<<12)|((p[1]&0x3F)<<6)|(p[2]&0x3F);
  return (p[0]>=0xE4 && p[0]<=0xE9 && (p[0]&0xC0)==0x80) ? 0 :
         (cp >= 0x4E00 && cp <= 0x9FFF);
}
static int is_suffix(const unsigned char *p) {
  return (p[0]==0xE9&&p[1]==0x83&&p[2]==0xBD)||  /* 都 */
         (p[0]==0xE9&&p[1]==0x81&&p[2]==0x93)||  /* 道 */
         (p[0]==0xE5&&p[1]==0xBA&&p[2]==0x9C)||  /* 府 */
         (p[0]==0xE7&&p[1]==0x9C&&p[2]==0x8C);   /* 県 */
}
static int label_pref(const char *label, char *out, size_t n) {
  const unsigned char *s = (const unsigned char *)label;
  for (; *s; s++) {
    /* try a match starting at s: 2..4 kanji then a suffix */
    const unsigned char *p = s;
    for (int cnt = 1; cnt <= 4; cnt++) {
      if (!kanji3(p)) break;
      const unsigned char *nx = p + 3;
      if (cnt >= 2 && is_suffix(nx)) {
        size_t L = (size_t)(nx - s) + 3;
        if (L >= n) L = n - 1;
        memcpy(out, s, L); out[L] = 0; return 1;
      }
      p = nx;
    }
  }
  return 0;
}

static const pref_t *resolve_pref(const char *url, const char *label) {
  if (url && url[0]) {
    char host[128]; url_host(url, host, sizeof host);
    for (int i = 0; i < NH2P; i++)
      if (strcmp(HOST2PREF[i].host, host) == 0)
        return pref_by_code(HOST2PREF[i].code);
  }
  char m[64];
  if (label && label_pref(label, m, sizeof m)) {
    const pref_t *r = pref_by_ja(m);
    if (r) return r;
  }
  if (label && strstr(label, "\xe8\xad\xa6\xe8\xa6\x96\xe5\xba\x81")) /* 警視庁 */
    return pref_by_code("13");
  return NULL;
}

typedef struct {
  char name[256]; int has_age; long age; char height[64];
  char crime[2048]; char photo[1024]; char hurl[1024]; char hlabel[512];
  const char *src_page; const pref_t *pref;
} entry_t;

static int has_any(const char *b, const char *a, const char *c,
                   const char *d) {
  return strstr(b, a) || strstr(b, c) || strstr(b, d);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char iso[32];
  time_t now = time(NULL);
  struct tm tmv; gmtime_r(&now, &tmv);
  strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%S.000Z", &tmv);

  static const char *PAGES[] = {
    "https://www.npa.go.jp/bureau/criminal/wanted/jyuyo1.html",
    "https://www.npa.go.jp/bureau/criminal/wanted/jyuyo2.html",
  };

  entry_t *all = NULL; int nall = 0, cap = 0;
  for (int pg = 0; pg < 2; pg++) {
    char *html = feed_get_text(ctx->http, PAGES[pg], 10000);
    if (!html) continue;
    const char *cur = html, *inner; int ilen;
    while ((cur = html_block(cur, "section", &inner, &ilen)) != NULL) {
      char *blk = strndup(inner, (size_t)ilen);
      if (!blk) continue;
      /* if (!/jyuyo|wanted|手配/.test(b) && !/写真/.test(b)) continue; */
      int g1 = has_any(blk, "jyuyo", "wanted",
                       "\xe6\x89\x8b\xe9\x85\x8d");          /* 手配 */
      int g2 = strstr(blk, "\xe5\x86\x99\xe7\x9c\x9f") != NULL; /* 写真 */
      if (!g1 && !g2) { free(blk); continue; }

      char name[256]; int hasA; long age;
      if (!parse_name(blk, name, sizeof name, &hasA, &age)) { free(blk); continue; }
      char img[1024];
      if (!parse_img(blk, img, sizeof img)) { free(blk); continue; }

      collapse_ws(name);                 /* name.replace(/\s+/g,' ').trim() */

      char photo[1280];
      if (strncmp(img, "http", 4) == 0) snprintf(photo, sizeof photo, "%s", img);
      else if (img[0] == '/') snprintf(photo, sizeof photo, "%s%s", NPA_BASE, img);
      else snprintf(photo, sizeof photo,
                    "%s/bureau/criminal/wanted/%s", NPA_BASE, img);

      char bodyRaw[8192] = {0};
      parse_body(blk, bodyRaw, sizeof bodyRaw);
      char *bstr = html_strip(bodyRaw);   /* <[^>]+> -> ' ', \s+ -> ' ', trim */
      char bodyTxt[8192]; bodyTxt[0] = 0;
      if (bstr) { snprintf(bodyTxt, sizeof bodyTxt, "%s", bstr); free(bstr); }

      char height[64]; int hasH = parse_height(bodyTxt, height, sizeof height);
      char crime[2048]; make_crime(bodyTxt, crime, sizeof crime);

      char hurl[1024] = {0}, hlabel[512] = {0};
      int hasHandler = parse_handler(blk, hurl, sizeof hurl,
                                     hlabel, sizeof hlabel);
      const pref_t *pref = hasHandler
        ? resolve_pref(hurl, hlabel) : NULL;

      if (nall == cap) {
        cap = cap ? cap * 2 : 16;
        all = realloc(all, (size_t)cap * sizeof *all);
      }
      entry_t *e = &all[nall++];
      memset(e, 0, sizeof *e);
      snprintf(e->name, sizeof e->name, "%s", name);
      e->has_age = hasA; e->age = age;
      if (hasH) snprintf(e->height, sizeof e->height, "%s", height);
      else e->height[0] = 0;
      e->height[0] = hasH ? e->height[0] : 0;
      snprintf(e->crime, sizeof e->crime, "%s", crime);
      snprintf(e->photo, sizeof e->photo, "%s", photo);
      if (hasHandler) { snprintf(e->hurl, sizeof e->hurl, "%s", hurl);
                        snprintf(e->hlabel, sizeof e->hlabel, "%s", hlabel); }
      e->src_page = PAGES[pg];
      e->pref = pref;
      e->has_age = hasA;
      e->age = age;
      /* track whether height existed (height[0]==0 means null) */
      if (!hasH) e->height[0] = 0;
      free(blk);
    }
    free(html);
  }

  /* group by prefecture code in first-seen order (JS Map insertion order) */
  cJSON *features = cJSON_CreateArray();
  int n = 0;
  char seen[64][8]; int nseen = 0;
  for (int i = 0; i < nall; i++) {
    const char *code = all[i].pref ? all[i].pref->code : "XX";
    int s = -1;
    for (int j = 0; j < nseen; j++) if (strcmp(seen[j], code) == 0) { s = j; break; }
    if (s >= 0) continue;                 /* group already processed */
    if (nseen < 64) snprintf(seen[nseen++], 8, "%s", code);

    /* collect this group's entries in order */
    int idxs[512], gc = 0;
    for (int j = i; j < nall && gc < 512; j++) {
      const char *cj = all[j].pref ? all[j].pref->code : "XX";
      if (strcmp(cj, code) == 0) idxs[gc++] = j;
    }
    for (int gi = 0; gi < gc; gi++) {
      entry_t *e = &all[idxs[gi]];
      const pref_t *pr = e->pref;
      if (pr) {
        /* Entries in the same prefecture used to be spread around an
         * index-derived ring of 0.04–0.07 deg (≈4.4–7.8 km) about the
         * prefectural point, which the map presented as each individual's
         * position. The NPA publication carries no location for any of them.
         * Emit the one real point — the prefectural handling force — and say
         * so in the properties; do not synthesise a per-person offset. */
        double lon = pr->lon, lat = pr->lat;
        cJSON *f = gj_point_feature(lon, lat);
        cJSON *p = cJSON_CreateObject();       /* EXACT JS key order */
        char idb[64];
        snprintf(idb, sizeof idb, "JYUYO_%s_%d", pr->code, gi + 1);
        cJSON_AddStringToObject(p, "id", idb);
        cJSON_AddStringToObject(p, "name", e->name);
        cJSON_AddItemToObject(p, "age",
          e->has_age ? cJSON_CreateNumber((double)e->age) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "height",
          e->height[0] ? cJSON_CreateString(e->height) : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "crime", e->crime);
        cJSON_AddStringToObject(p, "photo_url", e->photo);
        cJSON_AddItemToObject(p, "handler_url",
          e->hurl[0] ? cJSON_CreateString(e->hurl) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "handler_label",
          e->hlabel[0] ? cJSON_CreateString(e->hlabel) : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "prefecture_code", pr->code);
        cJSON_AddStringToObject(p, "prefecture_ja", pr->ja);
        cJSON_AddBoolToObject(p, "sensitive", 1);
        /* Unified provenance vocabulary: the point is the handling prefectural
         * force, not the individual. Never "exact" — this is a named person. */
        cJSON_AddStringToObject(p, "geo_provenance", "handling-force-prefecture");
        cJSON_AddStringToObject(p, "geo_precision", "prefecture");
        cJSON_AddBoolToObject(p, "geo_uncertain", 1);
        cJSON_AddStringToObject(p, "geo_note",
          "point is the handling prefectural police force; the NPA notice "
          "publishes no location for the individual");
        cJSON_AddStringToObject(p, "source", ctx->source_id);
        cJSON_AddItemToObject(f, "properties", p);
        cJSON_AddItemToArray(features, f);
      } else {
        /* unresolved -> intel item, uid intelUid(SOURCE_ID, name) */
        char title[512];
        if (e->has_age)
          snprintf(title, sizeof title,
            "\xe6\x8c\x87\xe5\x90\x8d\xe6\x89\x8b\xe9\x85\x8d: %s (%ld)",
            e->name, e->age);
        else
          snprintf(title, sizeof title,
            "\xe6\x8c\x87\xe5\x90\x8d\xe6\x89\x8b\xe9\x85\x8d: %s", e->name);
        cJSON *ip = cJSON_CreateObject();      /* EXACT JS key order */
        cJSON_AddStringToObject(ip, "name", e->name);
        cJSON_AddItemToObject(ip, "age",
          e->has_age ? cJSON_CreateNumber((double)e->age) : cJSON_CreateNull());
        cJSON_AddItemToObject(ip, "height",
          e->height[0] ? cJSON_CreateString(e->height) : cJSON_CreateNull());
        cJSON_AddStringToObject(ip, "crime", e->crime);
        cJSON_AddStringToObject(ip, "photo_url", e->photo);
        cJSON_AddItemToObject(ip, "handler_url",
          e->hurl[0] ? cJSON_CreateString(e->hurl) : cJSON_CreateNull());
        cJSON_AddItemToObject(ip, "handler_label",
          e->hlabel[0] ? cJSON_CreateString(e->hlabel) : cJSON_CreateNull());
        char *ipj = cJSON_PrintUnformatted(ip);
        intel_item it = {0};
        it.remote_key = e->name;
        it.title = title;
        it.summary = e->crime;
        it.link = e->hurl[0] ? e->hurl : e->src_page;  /* handlerUrl || page */
        it.lang = "ja";
        it.published_at = iso;
        it.tags_json =
          "[\"crime\",\"wanted\",\"sensitive\",\"unresolved-prefecture\"]";
        it.properties_json = ipj;
        if (sink->emit(sink, &it) >= 0) n++;
        free(ipj); cJSON_Delete(ip);
      }
    }
  }

  int fe = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  free(all);
  if (fe >= 0) n += fe;
  fprintf(stderr, "[npa-important-wanted] entries=%d emitted %d\n", nall, n);
  return n >= 0 ? 0 : -1;
}

static const source_def npa_important_wanted_def = {
  .id = "npa-important-wanted", .collector = "crime",
  .name = "NPA Important Wanted",
  .name_ja = "\xe8\xad\xa6\xe5\xaf\x9f\xe5\xba\x81 \xe9\x87\x8d\xe8\xa6\x81\xe6\x8c\x87\xe5\x90\x8d\xe6\x89\x8b\xe9\x85\x8d",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(npa_important_wanted_def)
