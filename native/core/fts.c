#include "fts.h"
#include <mecab.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* --- UTF-8 → codepoint (minimal, well-formed input assumed) --- */
static unsigned utf8_next(const unsigned char *s, int *adv) {
  if (s[0] < 0x80) { *adv = 1; return s[0]; }
  if ((s[0] & 0xE0) == 0xC0) { *adv = 2; return ((s[0] & 0x1F) << 6) | (s[1] & 0x3F); }
  if ((s[0] & 0xF0) == 0xE0) { *adv = 3;
    return ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); }
  if ((s[0] & 0xF8) == 0xF0) { *adv = 4;
    return ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
           ((s[2] & 0x3F) << 6) | (s[3] & 0x3F); }
  *adv = 1; return s[0];
}

/* jpTokenizer.js JP_RE: [぀-ゟ ゠-ヿ 一-鿿 㐀-䶿 ｦ-ﾟ]
 *   = U+3040-309F, U+30A0-30FF, U+4E00-9FFF, U+3400-4DBF, U+FF66-FF9F */
int fts_has_japanese(const char *s) {
  if (!s) return 0;
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    int adv;
    unsigned cp = utf8_next(p, &adv);
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

char *fts_segment(const char *text) {
  if (!text) return strdup("");
  if (!fts_has_japanese(text)) return strdup(text); /* Latin passthrough */

  pthread_mutex_lock(&g_lock);
  mecab_t *m = get_mecab();
  if (!m) { pthread_mutex_unlock(&g_lock); return strdup(text); } /* fail-open */
  const char *out = mecab_sparse_tostr(m, text);
  char *res = out ? strdup(out) : NULL;
  pthread_mutex_unlock(&g_lock);
  if (!res) return strdup(text);

  /* `-O wakati` appends a trailing space + '\n'; kuromoji's join(' ') has
   * neither. Trim trailing whitespace/newlines for byte parity. */
  size_t n = strlen(res);
  while (n > 0 && (res[n - 1] == ' ' || res[n - 1] == '\n' || res[n - 1] == '\r'))
    res[--n] = '\0';
  return res;
}

void fts_shutdown(void) {
  pthread_mutex_lock(&g_lock);
  if (g_mecab) { mecab_destroy(g_mecab); g_mecab = NULL; }
  pthread_mutex_unlock(&g_lock);
}
