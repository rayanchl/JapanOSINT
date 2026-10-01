#include "fts.h"
#include "../lib/utf8.h"      /* bounded decoder — the local one read past the
                               * allocation and stepped over the NUL; see utf8.h */
#include "../lib/jpnorm.h"    /* the ONE definition of "the same text" — applied
                               * here, before MeCab, on both write and query */
#include <mecab.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* jpTokenizer.js JP_RE: [぀-ゟ ゠-ヿ 一-鿿 㐀-䶿 ｦ-ﾟ]
 *   = U+3040-309F, U+30A0-30FF, U+4E00-9FFF, U+3400-4DBF, U+FF66-FF9F */
int fts_has_japanese(const char *s) {
  if (!s) return 0;
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    int adv;
    unsigned cp = utf8_decode(p, &adv);
    if ((cp >= 0x3040 && cp <= 0x309F) || (cp >= 0x30A0 && cp <= 0x30FF) ||
        (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
        (cp >= 0xFF66 && cp <= 0xFF9F))
      return 1;
    p += adv;
  }
  return 0;
}

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static mecab_t *g_mecab = NULL;
static int g_init_failed = 0;

static mecab_t *get_mecab(void) {
  if (g_mecab) return g_mecab;
  if (g_init_failed) return NULL;
  /* -O wakati = space-separated surface forms == kuromoji's join. dicdir/rc
   * resolved from env (MECABRC) or homebrew default; -d override if set. */
  const char *dic = getenv("MECAB_IPADIC");
  char args[512];
  if (dic && *dic)
    snprintf(args, sizeof args, "-O wakati -d %s", dic);
  else
    snprintf(args, sizeof args, "-O wakati");
  g_mecab = mecab_new2(args);
  if (!g_mecab) {
    /* retry with explicit homebrew ipadic path */
    g_mecab = mecab_new2("-O wakati -d /opt/homebrew/lib/mecab/dic/ipadic");
  }
  if (!g_mecab) { g_init_failed = 1; fprintf(stderr, "[fts] MeCab init failed\n"); }
  return g_mecab;
}

char *fts_segment(const char *raw) {
  if (!raw) return strdup("");
  /* Fold FIRST, so 「ＮＴＴドコモ㈱」, 「NTTドコモ株式会社」 and 「ｴﾇﾃｨｰﾃｨｰ
   * ﾄﾞｺﾓ」 reach MeCab as one surface and come out as one token sequence —
   * on the write path and on the query path alike (fts_query_expr calls
   * this). Latin goes through the same fold: it only changes case and width,
   * both of which unicode61 would have folded anyway, and it keeps the two
   * paths byte-identical rather than "identical unless ASCII".
   *
   * Known cost, measured on IPADIC: hiragana-folded text segments WORSE than
   * the katakana original (「ヤマダ タロウ」 stays two tokens as katakana and
   * becomes 「や まだ たろ う」 once folded). Acceptable here because the index
   * and the query see the same tokens, so matching is unaffected; readings
   * (fts_reading) run on the compat-folded, kana-preserving text instead. */
  char *text = jpnorm_fold_dup(raw);
  if (!text) return strdup("");
  if (!fts_has_japanese(text)) return text;          /* Latin passthrough */

  pthread_mutex_lock(&g_lock);
  mecab_t *m = get_mecab();
  if (!m) { pthread_mutex_unlock(&g_lock); return text; } /* fail-open */
  const char *out = mecab_sparse_tostr(m, text);
  char *res = out ? strdup(out) : NULL;
  pthread_mutex_unlock(&g_lock);
  if (!res) return text;
  free(text);

  /* `-O wakati` appends a trailing space + '\n'; kuromoji's join(' ') has
   * neither. Trim trailing whitespace/newlines for byte parity. */
  size_t n = strlen(res);
  while (n > 0 && (res[n - 1] == ' ' || res[n - 1] == '\n' || res[n - 1] == '\r'))
    res[--n] = '\0';
  return res;
}

/* --- readings ------------------------------------------------------------ */

/* A second tagger, with the DEFAULT output format so node features are
 * available; the wakati one above discards them. Same dictionary resolution
 * as get_mecab(). Guarded by the same mutex (mecab_t is not thread-safe). */
static mecab_t *g_mecab_feat = NULL;
static int g_feat_failed = 0;

static mecab_t *get_mecab_feat(void) {
  if (g_mecab_feat) return g_mecab_feat;
  if (g_feat_failed) return NULL;
  const char *dic = getenv("MECAB_IPADIC");
  char args[512];
  if (dic && *dic) snprintf(args, sizeof args, "-d %s", dic);
  else             args[0] = 0;
  g_mecab_feat = mecab_new2(args);
  if (!g_mecab_feat)
    g_mecab_feat = mecab_new2("-d /opt/homebrew/lib/mecab/dic/ipadic");
  if (!g_mecab_feat) {
    g_feat_failed = 1;
    fprintf(stderr, "[fts] MeCab (feature) init failed\n");
  }
  return g_mecab_feat;
}

/* IPADIC feature CSV: 品詞,細分類1,細分類2,細分類3,活用型,活用形,原形,読み,発音.
 * Field 7 (0-based) is the reading, katakana. Unknown words carry only the
 * first seven fields, so a missing field means "no reading known". */
static const char *feature_field(const char *feat, int idx, size_t *len) {
  const char *p = feat;
  for (int i = 0; i < idx; i++) {
    p = strchr(p, ',');
    if (!p) return NULL;
    p++;
  }
  const char *e = strchr(p, ',');
  *len = e ? (size_t)(e - p) : strlen(p);
  if (*len == 1 && *p == '*') return NULL;
  return p;
}

char *fts_reading(const char *raw) {
  if (!raw) return strdup("");
  /* Compat fold only: IPADIC's entries are katakana (ドコモ), and width is
   * the part that varies between spellings, so widths are normalised but
   * kana are left as written. */
  char *text = jpnorm_compat_dup(raw);
  if (!text) return strdup("");
  if (!fts_has_japanese(text)) return text;

  pthread_mutex_lock(&g_lock);
  mecab_t *m = get_mecab_feat();
  const mecab_node_t *node = m ? mecab_sparse_tonode(m, text) : NULL;
  if (!node) { pthread_mutex_unlock(&g_lock); return text; }   /* fail-open */

  size_t cap = strlen(text) * 3 + 16, n = 0;
  char *out = malloc(cap);
  if (!out) { pthread_mutex_unlock(&g_lock); return text; }
  for (; node; node = node->next) {
    if (node->stat == MECAB_BOS_NODE || node->stat == MECAB_EOS_NODE) continue;
    size_t rl = 0;
    const char *r = node->feature ? feature_field(node->feature, 7, &rl) : NULL;
    if (!r) { r = node->surface; rl = node->length; }   /* unknown: surface */
    if (!rl) continue;
    /* One space between morphemes, so a person name yields two romaji
     * tokens downstream: 山田太郎 → ヤマダ タロウ. */
    if (n + rl + 2 > cap) {
      cap = (n + rl + 2) * 2;
      char *np = realloc(out, cap);
      if (!np) break;
      out = np;
    }
    if (n) out[n++] = ' ';
    memcpy(out + n, r, rl); n += rl;
  }
  pthread_mutex_unlock(&g_lock);
  out[n] = 0;
  free(text);
  return out;
}

/* --- query builder (see fts.h for the rationale) --- */

static int is_space(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* A token FTS5 can actually index something from. Everything >= 0x80 counts:
 * that is CJK, kana, accented Latin — anything unicode61 will tokenize. A
 * token of pure ASCII punctuation ("-", "()") tokenizes to nothing, and an
 * empty phrase is an FTS5 syntax error, so those are dropped rather than
 * quoted. */
static int has_indexable(const char *s, size_t len) {
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c >= 0x80) return 1;
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
      return 1;
  }
  return 0;
}

/* Codepoints, not bytes — the 2-char floor for prefix matching must not treat
 * a single kanji as long enough to prefix-scan the whole index. */
static size_t cp_len(const char *s, size_t len) {
  size_t n = 0;
  for (size_t i = 0; i < len; i++)
    if (((unsigned char)s[i] & 0xC0) != 0x80) n++;
  return n;
}

char *fts_query_expr(const char *raw) {
  if (!raw || !*raw) return NULL;
  char *seg = fts_segment(raw);
  if (!seg) return NULL;

  size_t inlen = strlen(seg);
  /* Worst case per input char: a doubled quote (2) plus, for a 1-char token,
   * its own quotes and " AND " separator. 8x + slack covers it with room. */
  char *out = malloc(inlen * 8 + 64);
  if (!out) { free(seg); return NULL; }
  size_t o = 0;
  int ntok = 0;
  /* Offset of the closing quote of the last token written, so the prefix `*`
   * can be appended only once we know no further token follows. */
  size_t last_close = 0, last_cplen = 0;

  size_t i = 0;
  while (i < inlen && ntok < FTS_QUERY_MAX_TOKENS) {
    while (i < inlen && is_space((unsigned char)seg[i])) i++;
    size_t start = i;
    while (i < inlen && !is_space((unsigned char)seg[i])) i++;
    size_t len = i - start;
    if (!len || !has_indexable(seg + start, len)) continue;

    if (ntok) { memcpy(out + o, " AND ", 5); o += 5; }
    out[o++] = '"';
    for (size_t k = 0; k < len; k++) {
      char c = seg[start + k];
      out[o++] = c;
      if (c == '"') out[o++] = '"';   /* the only escape FTS5 strings have */
    }
    last_close = o;
    out[o++] = '"';
    last_cplen = cp_len(seg + start, len);
    ntok++;
  }
  free(seg);

  if (!ntok) { free(out); return NULL; }
  out[o] = '\0';

  /* Prefix-match the final token so a half-typed word still matches. Skipped
   * for a single character, where the prefix scan is the whole index for
   * nearly no selectivity. `"foo"*` is FTS5's prefix-of-a-phrase form. */
  if (last_cplen >= 2 && last_close + 1 == o) {
    out[o++] = '*';
    out[o] = '\0';
  }
  return out;
}
