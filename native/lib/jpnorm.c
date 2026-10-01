/* lib/jpnorm.c — see jpnorm.h for the contract and the list of what is and
 * is not folded. Everything here is a table lookup over one decoded
 * codepoint plus a little state (the previous kana, for dakuten composition;
 * a pending space, for whitespace collapse). */
#include "jpnorm.h"
#include "utf8.h"
#include <stdlib.h>
#include <string.h>

/* ── output buffer with bounded UTF-8 encode ──────────────────────────────── */

typedef struct {
  char  *p;
  size_t n, cap;        /* bytes written, capacity including NUL */
  size_t last_off;      /* byte offset of the last codepoint written */
  unsigned last_cp;     /* that codepoint (0 = none) */
  int    pending_sp;    /* a space is owed before the next non-space */
  int    full;          /* capacity hit: nothing more is written */
} obuf;

static void put_cp(obuf *b, unsigned cp) {
  char tmp[4]; int len;
  if (cp < 0x80)          { tmp[0] = (char)cp; len = 1; }
  else if (cp < 0x800)    { tmp[0] = (char)(0xC0 | (cp >> 6));
                            tmp[1] = (char)(0x80 | (cp & 0x3F)); len = 2; }
  else if (cp < 0x10000)  { tmp[0] = (char)(0xE0 | (cp >> 12));
                            tmp[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                            tmp[2] = (char)(0x80 | (cp & 0x3F)); len = 3; }
  else                    { tmp[0] = (char)(0xF0 | (cp >> 18));
                            tmp[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                            tmp[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
                            tmp[3] = (char)(0x80 | (cp & 0x3F)); len = 4; }
  /* A space owed by a whitespace run is paid only when text follows it, so
   * the result is trimmed at both ends and collapsed in the middle. */
  if (b->pending_sp) {
    b->pending_sp = 0;
    if (b->n > 0 && b->n + 1 < b->cap) { b->p[b->n++] = ' '; }
  }
  /* Full: drop the whole codepoint and everything after it, so the output
   * is a clean prefix rather than one with a hole in the middle. */
  if (b->full || b->n + (size_t)len >= b->cap) { b->full = 1; return; }
  memcpy(b->p + b->n, tmp, (size_t)len);
  b->last_off = b->n;
  b->last_cp = cp;
  b->n += (size_t)len;
}

/* UTF-8 literal → codepoints (the expansions are multi-byte kanji). */
static void put_str(obuf *b, const char *s) {
  const unsigned char *p = (const unsigned char *)s;
  while (*p) { int adv; unsigned cp = utf8_decode(p, &adv); put_cp(b, cp); p += adv; }
}

/* Replace the last codepoint written (used to compose a dakuten onto it). */
static void replace_last(obuf *b, unsigned cp) {
  if (!b->last_cp) return;
  b->n = b->last_off;
  b->last_cp = 0;
  put_cp(b, cp);
}

/* ── tables ──────────────────────────────────────────────────────────────── */

/* Half-width katakana U+FF61..U+FF9F → full-width (U+30xx). ﾞ ﾟ (FF9E/FF9F)
 * are handled by the caller as composing marks and map to 0 here. */
static const unsigned short hw_kana[0x3F] = {
  0x3002, 0x300C, 0x300D, 0x3001, 0x30FB, 0x30F2, 0x30A1, 0x30A3, /* FF61 */
  0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 0x30E7, 0x30C3, 0x30FC, /* FF69 */
  0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF, /* FF71 */
  0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF, /* FF79 */
  0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD, /* FF81 */
  0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF, /* FF89 */
  0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA, /* FF91 */
  0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F3, 0,      0,               /* FF99 */
};

/* Can this kana take a dakuten (returns the voiced codepoint) / handakuten?
 * Works for both scripts: hiragana and katakana rows are 0x60 apart. */
static unsigned voiced(unsigned cp) {
  unsigned base = cp >= 0x30A1 ? cp - 0x60 : cp;
  if (base < 0x3041 || base > 0x3096) return 0;
  if (base == 0x3046) return cp + 0x4E;                    /* う→ゔ, ウ→ヴ */
  /* か..ぢ (304B..3061) alternate plain/voiced with plain on odd; つ..ど
   * (3064..3069) likewise; は..ぽ (306F..307D) in triplets. */
  if ((base >= 0x304B && base <= 0x3062 && (base & 1)) ||
      (base >= 0x3064 && base <= 0x3069 && !(base & 1)))
    return cp + 1;
  if (base >= 0x306F && base <= 0x307D && (base - 0x306F) % 3 == 0)
    return cp + 1;
  return 0;
}
static unsigned semivoiced(unsigned cp) {
  unsigned base = cp >= 0x30A1 ? cp - 0x60 : cp;
  if (base >= 0x306F && base <= 0x307D && (base - 0x306F) % 3 == 0)
    return cp + 2;
  return 0;
}

static const char *const roman_lc[16] = {
  "i","ii","iii","iv","v","vi","vii","viii","ix","x","xi","xii","l","c","d","m"
};

static int is_ws_cp(unsigned cp) {
  return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f' ||
         cp == '\v' || cp == 0x3000 || cp == 0x00A0 ||
         (cp >= 0x2000 && cp <= 0x200A) || cp == 0x202F || cp == 0x205F;
}

/* Latin case fold for the ranges listed in the header. */
static unsigned latin_lower(unsigned cp) {
  if (cp >= 'A' && cp <= 'Z') return cp + 32;
  if (cp < 0xC0) return cp;
  if (cp <= 0xDE) return cp == 0xD7 ? cp : cp + 32;
  if (cp == 0x178) return 0xFF;
  if (cp >= 0x100 && cp <= 0x137) return cp | 1;             /* even upper */
  if (cp >= 0x139 && cp <= 0x148) return (cp & 1) ? cp + 1 : cp; /* odd upper */
  if (cp >= 0x14A && cp <= 0x177) return cp | 1;
  if (cp >= 0x179 && cp <= 0x17E) return (cp & 1) ? cp + 1 : cp;
  return cp;
}

/* ── the fold ────────────────────────────────────────────────────────────── */

static size_t fold_impl(const char *in, char *out, size_t n, int kana_fold) {
  if (!n) return 0;
  obuf b = { out, 0, n, 0, 0, 0, 0 };
  if (!in) { out[0] = 0; return 0; }
  const unsigned char *p = (const unsigned char *)in;
  while (*p) {
    int adv;
    unsigned cp = utf8_decode(p, &adv);
    p += adv;

    if (is_ws_cp(cp)) { b.pending_sp = 1; continue; }
    if ((cp >= 0x200B && cp <= 0x200D) || cp == 0xFEFF) continue;

    /* stage 1: compatibility */
    if (cp >= 0xFF01 && cp <= 0xFF5E) cp -= 0xFEE0;
    else if (cp >= 0xFF61 && cp <= 0xFF9F) {
      if (cp == 0xFF9E || cp == 0xFF9F) {
        unsigned v = cp == 0xFF9E ? voiced(b.last_cp) : semivoiced(b.last_cp);
        if (v) replace_last(&b, v);
        /* an orphan mark (ﾞ after nothing voiceable) is dropped: it is
         * unpronounceable and unicode61 would drop it too */
        continue;
      }
      cp = hw_kana[cp - 0xFF61];
    }
    else if (cp == 0x3099 || cp == 0x309B) {
      unsigned v = voiced(b.last_cp); if (v) replace_last(&b, v); continue;
    }
    else if (cp == 0x309A || cp == 0x309C) {
      unsigned v = semivoiced(b.last_cp); if (v) replace_last(&b, v); continue;
    }
    else if (cp == 0x301C || cp == 0x2053) cp = '~';
    else if ((cp >= 0x2010 && cp <= 0x2015) || cp == 0x2212) cp = '-';
    else if (cp == 0x3231 || cp == 0x337F) { put_str(&b, "株式会社"); continue; }
    else if (cp == 0x3232) { put_str(&b, "有限会社"); continue; }
    else if (cp == 0x3233) { put_str(&b, "社団法人"); continue; }
    else if (cp == 0x3234) { put_str(&b, "合名会社"); continue; }
    else if (cp == 0x3236) { put_str(&b, "財団法人"); continue; }
    else if (cp >= 0x32A4 && cp <= 0x32A8) {
      static const char *const pos[5] = {"上","中","下","左","右"};
      put_str(&b, pos[cp - 0x32A4]); continue;
    }
    else if (cp == 0x2116) { put_str(&b, "no"); continue; }
    else if (cp == 0x2121) { put_str(&b, "tel"); continue; }
    else if (cp == 0x24EA) { cp = '0'; }
    else if (cp >= 0x2460 && cp <= 0x249B) {
      unsigned k = (cp - 0x2460) % 20 + 1, kind = (cp - 0x2460) / 20;
      char t[8]; size_t o = 0;
      if (kind == 1) t[o++] = '(';
      if (k >= 10) t[o++] = (char)('0' + k / 10);
      t[o++] = (char)('0' + k % 10);
      if (kind == 1) t[o++] = ')';
      if (kind == 2) t[o++] = '.';
      t[o] = 0;
      put_str(&b, t); continue;
    }
    else if (cp >= 0x2160 && cp <= 0x217F) { put_str(&b, roman_lc[(cp - 0x2160) & 15]); continue; }

    /* stage 2: katakana → hiragana */
    if (kana_fold && cp >= 0x30A1 && cp <= 0x30F6) cp -= 0x60;

    /* stage 3: Latin case */
    if (cp < 0x180) cp = latin_lower(cp);

    put_cp(&b, cp);
  }
  out[b.n] = 0;
  return b.n;
}

size_t jpnorm_fold(const char *in, char *out, size_t n)   { return fold_impl(in, out, n, 1); }
size_t jpnorm_compat(const char *in, char *out, size_t n) { return fold_impl(in, out, n, 0); }

static char *dup_via(const char *in, size_t (*fn)(const char *, char *, size_t)) {
  size_t L = in ? strlen(in) : 0;
  char *o = malloc(JPNORM_OUT_CAP(L));
  if (!o) return NULL;
  fn(in, o, JPNORM_OUT_CAP(L));
  return o;
}
char *jpnorm_fold_dup(const char *in)   { return dup_via(in, jpnorm_fold); }
char *jpnorm_compat_dup(const char *in) { return dup_via(in, jpnorm_compat); }

int jpnorm_has_cjk(const char *s) {
  if (!s) return 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; ) {
    int len;
    unsigned cp = utf8_decode(p, &len);
    if ((cp >= 0x3040 && cp <= 0x30FF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
        (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFF66 && cp <= 0xFF9F))
      return 1;
    p += len;
  }
  return 0;
}

/* ── Hepburn ─────────────────────────────────────────────────────────────── */

/* U+3041..U+3096. "" marks a small kana handled by the caller. */
static const char *const hira[0x56] = {
  "a","a","i","i","u","u","e","e","o","o",                        /* ぁあぃいぅうぇえぉお */
  "ka","ga","ki","gi","ku","gu","ke","ge","ko","go",
  "sa","za","shi","ji","su","zu","se","ze","so","zo",
  "ta","da","chi","ji","","tsu","zu","te","de","to","do",         /* っ = "" */
  "na","ni","nu","ne","no",
  "ha","ba","pa","hi","bi","pi","fu","bu","pu","he","be","pe","ho","bo","po",
  "ma","mi","mu","me","mo",
  "ya","ya","yu","yu","yo","yo",                                  /* ゃやゅゆょよ */
  "ra","ri","ru","re","ro",
  "wa","wa","i","e","o","n","vu","ka","ke"                        /* ゎわゐゑをんゔゕゖ */
};

static int is_small_vowel(unsigned cp) { return cp >= 0x3041 && cp <= 0x3049 && (cp & 1); }
static int is_small_y(unsigned cp)     { return cp == 0x3083 || cp == 0x3085 || cp == 0x3087; }
static int is_vowel(char c)            { return c=='a'||c=='i'||c=='u'||c=='e'||c=='o'; }

/* Long-vowel collapse over one token, in place. */
static size_t collapse_long(char *tok, size_t len) {
  size_t w = 0;
  for (size_t i = 0; i < len; i++) {
    char c = tok[i];
    if (w && is_vowel(c)) {
      char pv = tok[w - 1];
      if ((pv == 'o' && (c == 'u' || c == 'o')) || (pv == 'u' && c == 'u') ||
          (pv == 'a' && c == 'a') || (pv == 'e' && c == 'e'))
        continue;
    }
    tok[w++] = c;
  }
  tok[w] = 0;
  return w;
}

size_t jpnorm_hepburn(const char *kana, char *out, size_t n) {
  if (!n) return 0;
  out[0] = 0;
  if (!kana) return 0;
  char *h = jpnorm_fold_dup(kana);
  if (!h) return 0;

  size_t o = 0, tok_start = 0;
  size_t syl_start = 0;       /* start of the last syllable written */
  int    gem = 0;             /* っ pending */
  const unsigned char *p = (const unsigned char *)h;
#define PUT(c) do { if (o + 1 < n) out[o++] = (c); } while (0)
  while (*p) {
    int adv; unsigned cp = utf8_decode(p, &adv); p += adv;
    if (cp == ' ') {
      o = tok_start + collapse_long(out + tok_start, o - tok_start);
      if (o && o + 1 < n) out[o++] = ' ';
      tok_start = syl_start = o; gem = 0;
      continue;
    }
    if (cp == 0x30FC) continue;                       /* ー: macron-less */
    if (cp < 0x3041 || cp > 0x3096) {                 /* not kana: verbatim */
      char t[4]; int len;
      if (cp < 0x80) { t[0] = (char)cp; len = 1; }
      else if (cp < 0x800) { t[0]=(char)(0xC0|(cp>>6)); t[1]=(char)(0x80|(cp&0x3F)); len=2; }
      else if (cp < 0x10000) { t[0]=(char)(0xE0|(cp>>12)); t[1]=(char)(0x80|((cp>>6)&0x3F)); t[2]=(char)(0x80|(cp&0x3F)); len=3; }
      else { t[0]=(char)(0xF0|(cp>>18)); t[1]=(char)(0x80|((cp>>12)&0x3F)); t[2]=(char)(0x80|((cp>>6)&0x3F)); t[3]=(char)(0x80|(cp&0x3F)); len=4; }
      if (o + (size_t)len < n) { memcpy(out + o, t, (size_t)len); o += (size_t)len; }
      syl_start = o; gem = 0;
      continue;
    }
    if (cp == 0x3063) { gem = 1; continue; }
    const char *r = hira[cp - 0x3041];
    size_t syl_len = o - syl_start;
    if (is_small_y(cp) && syl_len >= 2 && out[o - 1] == 'i') {
      /* きゃ → ky+a; しゃ/ちゃ/じゃ → sh+a / ch+a / j+a */
      o--;
      char last = out[o - 1];
      if (last != 'h' && last != 'j') PUT('y');
      PUT(r[1]);
      continue;
    }
    if (is_small_vowel(cp) && syl_len >= 1 && is_vowel(out[o - 1])) {
      /* ふぁ → fa, てぃ → ti, うぃ → wi, しぇ → she */
      o--;                                  /* strip the syllable's vowel */
      /* a bare う takes w (うぃ → wi); any other bare vowel just yields to
       * the small one (あぁ → a) */
      if (o == syl_start && out[o] == 'u') PUT('w');
      PUT(r[0]);
      continue;
    }
    syl_start = o;
    if (gem) {
      gem = 0;
      if (r[0] == 'c') PUT('t');
      else if (!is_vowel(r[0]) && r[0] != 'n') PUT(r[0]);
    }
    for (const char *q = r; *q; q++) PUT(*q);
  }
  o = tok_start + collapse_long(out + tok_start, o - tok_start);
#undef PUT
  out[o] = 0;
  free(h);
  return o;
}

char *jpnorm_hepburn_dup(const char *kana) {
  size_t L = kana ? strlen(kana) : 0;
  char *o = malloc(JPNORM_OUT_CAP(L));
  if (!o) return NULL;
  jpnorm_hepburn(kana, o, JPNORM_OUT_CAP(L));
  return o;
}
