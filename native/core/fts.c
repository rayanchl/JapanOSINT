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

/* ── MeCab: one shared model, one tagger + lattice per THREAD ────────────
 *
 * This used to be one global mecab_t behind one global mutex, so every
 * segmentation in the process — 8 scheduler workers emitting, dispatch
 * workers, the HTTP thread building a query — queued on a single lock, and
 * MeCab is the dominant CPU cost of an emit. MeCab's own threading model is
 * the fix: a mecab_model_t (the dictionary) is immutable and shareable, and a
 * tagger parsing into a caller-owned lattice is thread-safe. So the model is
 * loaded once per output format, and each thread lazily gets its own
 * tagger + lattice, destroyed by the pthread key destructor at thread exit.
 * The dictionary is not duplicated per thread; a tagger and a lattice are a
 * few KB.
 *
 * Unavailable MeCab degrades exactly as before: the model load fails once,
 * that failure is remembered, and segmentation passes text through. */
typedef struct {
  mecab_t *tag;
  mecab_lattice_t *lat;
} mecab_tls;

typedef struct {
  const char *what;            /* for the init-failure message */
  const char *fmt;             /* "-O wakati " or "" */
  pthread_once_t once;
  mecab_model_t *model;        /* NULL after once => unavailable */
  pthread_key_t key;
  int key_ok;
} mecab_kind;

static mecab_kind g_wakati = { "", "-O wakati ", PTHREAD_ONCE_INIT, NULL, 0, 0 };
static mecab_kind g_feat   = { " (feature)", "", PTHREAD_ONCE_INIT, NULL, 0, 0 };

static void tls_free(void *p) {
  mecab_tls *t = p;
  if (!t) return;
  if (t->lat) mecab_lattice_destroy(t->lat);
  if (t->tag) mecab_destroy(t->tag);
  free(t);
}

/* dicdir/rc resolved from env (MECABRC) or the system default; MECAB_IPADIC
 * overrides with -d; the homebrew ipadic path is the retry. Same resolution
 * as before the split. */
static void model_load(mecab_kind *k) {
  const char *dic = getenv("MECAB_IPADIC");
  char args[512];
  if (dic && *dic) snprintf(args, sizeof args, "%s-d %s", k->fmt, dic);
  else             snprintf(args, sizeof args, "%s", k->fmt);
  k->model = mecab_model_new2(args);
  if (!k->model) {
    snprintf(args, sizeof args, "%s-d /opt/homebrew/lib/mecab/dic/ipadic", k->fmt);
    k->model = mecab_model_new2(args);
  }
  if (k->model && pthread_key_create(&k->key, tls_free) == 0) k->key_ok = 1;
  if (!k->model || !k->key_ok)
    fprintf(stderr, "[fts] MeCab%s init failed\n", k->what);
}
static void load_wakati(void) { model_load(&g_wakati); }
static void load_feat(void)   { model_load(&g_feat); }

/* This thread's tagger+lattice for `k`, created on first use. NULL when MeCab
 * is unavailable (or this thread could not get one) — callers fail open. */
static mecab_tls *mecab_for_thread(mecab_kind *k, void (*load)(void)) {
  pthread_once(&k->once, load);
  if (!k->model || !k->key_ok) return NULL;
  mecab_tls *t = pthread_getspecific(k->key);
  if (t) return t;
  t = calloc(1, sizeof *t);
  if (!t) return NULL;
  t->tag = mecab_model_new_tagger(k->model);
  t->lat = mecab_model_new_lattice(k->model);
  if (!t->tag || !t->lat || pthread_setspecific(k->key, t) != 0) {
    tls_free(t);
    return NULL;
  }
  return t;
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

  mecab_tls *m = mecab_for_thread(&g_wakati, load_wakati);
  if (!m) return text;                               /* fail-open */
  mecab_lattice_set_sentence(m->lat, text);
  const char *out = mecab_parse_lattice(m->tag, m->lat)
                  ? mecab_lattice_tostr(m->lat) : NULL;
  char *res = out ? strdup(out) : NULL;
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

/* The feature-format model (g_feat above): DEFAULT output, so node features
 * are available; the wakati one discards them. */

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

  mecab_tls *m = mecab_for_thread(&g_feat, load_feat);
  const mecab_node_t *node = NULL;
  if (m) {
    mecab_lattice_set_sentence(m->lat, text);
    if (mecab_parse_lattice(m->tag, m->lat))
      node = mecab_lattice_get_bos_node(m->lat);
  }
  if (!node) return text;                                       /* fail-open */

  /* `node` points into this thread's lattice, valid until its next parse. */
  size_t cap = strlen(text) * 3 + 16, n = 0;
  char *out = malloc(cap);
  if (!out) return text;
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
