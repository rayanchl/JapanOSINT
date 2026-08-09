/* core/content_change.c — content change detection. The design, the complete
 * strip list, the DDL and the orchestrator wiring all live in
 * content_change.h; this file is the mechanics.
 *
 * Shape of a watched fetch: clean markup -> html_strip() (lib/htmlparse.c,
 * reused, not reimplemented) -> decode entities -> per-line token
 * normalization -> SHA-256 of the normalized text -> ONE indexed SELECT of the
 * previous snapshot. On the overwhelmingly common unchanged path that is where
 * it stops: no write, no transaction, no SQLite write-lock contention.
 *
 * Every allocation is bounded before it is made (JO_CCHANGE_MAX_DOC_BYTES caps
 * the document, CC_MAX_LINES caps the line arrays, CC_LCS_MAX_LINES caps the
 * diff scratch), and every SQLite error is swallowed: this hangs off
 * http_request() and must never be able to fail the fetch that called it.
 *
 * The buffer-growth arguments this file relies on, stated once:
 *   - clean_markup() output is never longer than its input. Every tag it
 *     rewrites is replaced by ONE separator byte and the shortest tag it
 *     rewrites is three bytes ("<p>"); comments and dropped elements only
 *     shrink; everything else is copied byte for byte.
 *   - ent_decode() output is never longer than its input. The shortest numeric
 *     entity is "&#9;" (4 bytes -> 1) and a 4-byte codepoint needs an entity of
 *     at least 7 ("&#65536;").
 * Normalization itself uses a growing builder, because URL canonicalization is
 * the one rule that can leave a token the same length.
 */
#include "content_change.h"
#include "evidence.h"
#include "intel.h"
#include "../source.h"
#include "../lib/htmlparse.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/sha.h>
#include <ctype.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define CC_SEP            '\x01'   /* block separator; survives html_strip()  */
#define CC_REFKEY_MAX     512
#define CC_MAX_TOKENS     512
#define CC_BODY_EXCERPT   4096
#define CC_CACHE_SLOTS    128
#define CC_CACHE_TTL      60

#define CC_DEFAULT_MAX_DOC   4194304LL   /* 4 MiB                             */
#define CC_DEFAULT_TEXT_MAX  262144LL    /* 256 KiB of normalized text stored */
#define CC_DEFAULT_KEEP      5
#define CC_DEFAULT_AGE_DAYS  90
#define CC_DEFAULT_DIFF_MAX  65536LL

/* UTF-8 written as hex escapes so this file's meaning does not depend on how an
 * editor saved it. A hex escape is never followed by a hex-digit character in
 * these literals, which would silently extend the escape. */
#define JA_YEAR   "\xe5\xb9\xb4"                         /* 年     */
#define JA_MONTH  "\xe6\x9c\x88"                         /* 月     */
#define JA_DAY    "\xe6\x97\xa5"                         /* 日     */
#define JA_HOUR   "\xe6\x99\x82"                         /* 時     */
#define JA_MIN    "\xe5\x88\x86"                         /* 分     */
#define JA_SEC    "\xe7\xa7\x92"                         /* 秒     */
#define JA_AGO    "\xe5\x89\x8d"                         /* 前     */
#define JA_MAN    "\xe4\xb8\x87"                         /* 万     */
#define JA_SEN    "\xe5\x8d\x83"                         /* 千     */
#define JA_VIEWS  "\xe9\x96\xb2\xe8\xa6\xa7"             /* 閲覧   */
#define JA_VIEWSN "\xe9\x96\xb2\xe8\xa6\xa7\xe6\x95\xb0" /* 閲覧数 */
#define JA_ACCESS "\xe3\x82\xa2\xe3\x82\xaf\xe3\x82\xbb\xe3\x82\xb9" /* アクセス */
#define JA_PLAY   "\xe5\x86\x8d\xe7\x94\x9f"             /* 再生   */
#define JA_VIEWKT "\xe3\x83\x93\xe3\x83\xa5\xe3\x83\xbc" /* ビュー */

/* ── environment (read once; these sit on the per-fetch path) ─────────────── */

static long long env_ll(const char *name, long long dflt, long long lo) {
  const char *e = getenv(name);
  if (!e || !*e) return dflt;
  long long v = strtoll(e, NULL, 10);
  return v >= lo ? v : dflt;
}
static int cc_disabled(void) {
  static int v = -1;
  if (v < 0) v = getenv("JO_NO_CONTENT_CHANGE") ? 1 : 0;
  return v;
}
static long long max_doc(void) {
  static long long v = -1;
  if (v < 0) v = env_ll("JO_CCHANGE_MAX_DOC_BYTES", CC_DEFAULT_MAX_DOC, 4096);
  return v;
}
static long long text_max(void) {
  static long long v = -1;
  if (v < 0) v = env_ll("JO_CCHANGE_TEXT_MAX", CC_DEFAULT_TEXT_MAX, 1024);
  return v;
}
static long long diff_max(void) {
  static long long v = -1;
  if (v < 0) v = env_ll("JO_CCHANGE_DIFF_MAX", CC_DEFAULT_DIFF_MAX, 512);
  return v;
}
static int keep_n(void) {
  static int v = -1;
  if (v < 0) {
    v = (int)env_ll("JO_CCHANGE_KEEP", CC_DEFAULT_KEEP, 1);
    if (v > 100) v = 100;
  }
  return v;
}
static int max_age_days(void) {
  static int v = -1;
  if (v < 0) {
    v = (int)env_ll("JO_CCHANGE_MAX_AGE_DAYS", CC_DEFAULT_AGE_DAYS, 1);
    if (v > 3650) v = 3650;
  }
  return v;
}

/* ── small shared helpers ─────────────────────────────────────────────────── */

/* Truncating copy between fixed-size buffers. Used instead of snprintf("%s")
 * so gcc cannot warn about a truncation that IS the documented behaviour. */
static void ccopy(char *dst, size_t cap, const char *src) {
  if (!cap) return;
  size_t l = src ? strlen(src) : 0;
  if (l >= cap) l = cap - 1;
  if (l) memcpy(dst, src, l);
  dst[l] = 0;
}
static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static void sha256_hex_bytes(const void *b, size_t n, char out[65]) {
  unsigned char d[SHA256_DIGEST_LENGTH];
  SHA256((const unsigned char *)b, n, d);
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    snprintf(out + i * 2, 3, "%02x", d[i]);
}
/* The normalizer version is part of the hash preimage on purpose: two hashes
 * produced by different strip rules are not comparable and must not collide.
 * Hashing the digest rather than prefixing the text avoids copying a multi-MB
 * document just to prepend eighteen bytes. */
static void sha256_norm(const char *text, char out[65]) {
  char inner[65], pre[128];
  sha256_hex_bytes(text, strlen(text), inner);
  int n = snprintf(pre, sizeof pre, "content_change/v%d\n%s",
                   CC_NORM_VERSION, inner);
  if (n < 0 || (size_t)n >= sizeof pre) n = (int)strlen(pre);
  sha256_hex_bytes(pre, (size_t)n, out);
}
static void now_utc_sql(char out[20]) {
  time_t t = time(NULL);
  struct tm g;
  gmtime_r(&t, &g);
  strftime(out, 20, "%Y-%m-%d %H:%M:%S", &g);
}
static void now_iso(char out[32]) {
  time_t t = time(NULL);
  struct tm g;
  gmtime_r(&t, &g);
  strftime(out, 32, "%Y-%m-%dT%H:%M:%SZ", &g);
}
static unsigned fnv32(const char *s) {
  unsigned h = 2166136261u;
  while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
  return h;
}
/* Case-insensitive substring searches, local so this file needs no
 * _GNU_SOURCE for strcasestr. */
static int ci_has(const char *hay, const char *needle) {
  if (!hay || !needle) return 0;
  size_t nl = strlen(needle);
  if (!nl) return 1;
  for (const char *p = hay; *p; p++)
    if (strncasecmp(p, needle, nl) == 0) return 1;
  return 0;
}
static int ci_has_n(const char *p, size_t n, const char *needle) {
  size_t nl = strlen(needle);
  if (!nl || nl > n) return 0;
  for (size_t i = 0; i + nl <= n; i++)
    if (strncasecmp(p + i, needle, nl) == 0) return 1;
  return 0;
}
/* Exact-byte substring, for the UTF-8 markers where case folding is meaningless
 * and byte equality is exactly what is wanted. */
static int ja_has(const char *p, size_t n, const char *seq) {
  size_t l = strlen(seq);
  if (!l || l > n) return 0;
  for (size_t i = 0; i + l <= n; i++)
    if (memcmp(p + i, seq, l) == 0) return 1;
  return 0;
}

/* ── growing string builder ───────────────────────────────────────────────── */

typedef struct { char *p; size_t n, cap; } sb;

static int sb_need(sb *b, size_t add) {
  if (b->p && b->n + add + 1 <= b->cap) return 0;
  size_t nc = b->cap ? b->cap : 512;
  while (nc < b->n + add + 1) {
    if (nc > ((size_t)1 << 30)) return -1;
    nc *= 2;
  }
  char *q = realloc(b->p, nc);
  if (!q) return -1;
  b->p = q; b->cap = nc;
  return 0;
}
static int sb_add(sb *b, const char *s, size_t n) {
  if (sb_need(b, n)) return -1;
  if (n) memcpy(b->p + b->n, s, n);
  b->n += n; b->p[b->n] = 0;
  return 0;
}
static int sb_str(sb *b, const char *s) { return sb_add(b, s, strlen(s)); }
static int sb_ch(sb *b, char c) { return sb_add(b, &c, 1); }

/* ── URL canonicalization (cache-buster / analytics parameters) ───────────── */

/* Dropped by NAME. Deliberately short: every entry is a parameter that carries
 * no content, and a parameter dropped here is a change that can never be
 * reported. "utm_" is matched by prefix. */
static const char *CC_DROP_PARAMS[] = {
  "v", "ver", "_", "t", "ts", "cb", "cachebust", "cachebuster", "cache_buster",
  "rnd", "rand", "nocache", "no_cache", "_t", "__cb", "gclid", "fbclid",
  "msclkid", "_ga", "_gl", "mc_cid", "mc_eid", NULL
};
static int param_dropped(const char *name, size_t n) {
  if (n >= 4 && strncasecmp(name, "utm_", 4) == 0) return 1;
  for (int i = 0; CC_DROP_PARAMS[i]; i++) {
    size_t l = strlen(CC_DROP_PARAMS[i]);
    if (l == n && strncasecmp(name, CC_DROP_PARAMS[i], l) == 0) return 1;
  }
  return 0;
}
/* Does this query-parameter name look like a credential? Same list evidence.c
 * uses in its redact_url(); the two paths see the SAME urls and must not
 * disagree about which of them carries a secret. */
static int cc_name_is_secret(const char *name, size_t len) {
  char b[128];
  size_t n = len < sizeof b - 1 ? len : sizeof b - 1;
  for (size_t i = 0; i < n; i++) b[i] = (char)tolower((unsigned char)name[i]);
  b[n] = 0;
  return (strstr(b, "auth") || strstr(b, "key") || strstr(b, "token") ||
          strstr(b, "secret") || strstr(b, "password") || strstr(b, "passwd") ||
          strstr(b, "cookie") || strstr(b, "session") ||
          strstr(b, "credential") || strstr(b, "signature")) ? 1 : 0;
}

/* Strips userinfo and replaces credential-looking query values with
 * "[redacted]". Malloc'd, caller frees; NULL only on OOM.
 *
 * WHY. ref_key is derived from the URL and lands in content_snapshots plus
 * intel_items.properties.ref_key/url, both readable through the API — so every
 * Japanese open-data source that passes its key in the query string was
 * storing that key in cleartext where any authenticated caller could read it
 * back. The adjacent evidence path already redacted; this one did not. Same
 * transformation, applied at the one place the URL enters this module. */
static char *cc_redact_url(const char *url) {
  if (!url) return NULL;
  size_t ul = strlen(url);
  /* Sizing. A redacted parameter is rewritten as `name=[redacted]`, i.e. it
   * GROWS by 10 bytes whenever its value is shorter than "[redacted]" — and
   * the shortest parameter that can be redacted at all is `key=` (4 bytes,
   * plus one `&`). So the worst case is `?key=&key=&…`, where every 5 input
   * bytes become 15: growth is 3x, not 2x. At 2x the ninth such parameter
   * wrote past the end of this allocation (verified: a 59-byte URL with ten
   * empty `key=` params needs 160 bytes and was given 150). 4x + slack is
   * comfortably above the 3x bound. */
  char *out = malloc(ul * 4 + 32);
  if (!out) return NULL;
  size_t o = 0;

  const char *p = url;
  const char *scheme = strstr(url, "://");
  if (scheme) {
    const char *hstart = scheme + 3;
    const char *slash = strchr(hstart, '/');
    const char *at = strchr(hstart, '@');
    if (at && (!slash || at < slash)) {
      size_t pre = (size_t)(hstart - url);
      memcpy(out + o, url, pre); o += pre;
      memcpy(out + o, "[redacted]@", 11); o += 11;
      p = at + 1;
    }
  }
  const char *q = strchr(p, '?');
  size_t head = q ? (size_t)(q - p) : strlen(p);
  memcpy(out + o, p, head); o += head;
  if (q) {
    out[o++] = '?';
    const char *k = q + 1;
    while (*k) {
      const char *amp = strchr(k, '&');
      size_t plen = amp ? (size_t)(amp - k) : strlen(k);
      const char *eq = memchr(k, '=', plen);
      size_t nlen = eq ? (size_t)(eq - k) : plen;
      if (eq && cc_name_is_secret(k, nlen)) {
        memcpy(out + o, k, nlen); o += nlen;
        memcpy(out + o, "=[redacted]", 11); o += 11;
      } else {
        memcpy(out + o, k, plen); o += plen;
      }
      if (!amp) break;
      out[o++] = '&';
      k = amp + 1;
    }
  }
  out[o] = 0;
  return out;
}

/* Drops the fragment and the cache-buster/analytics parameters and keeps every
 * other parameter in its original order. Returns malloc'd, caller frees. */
static char *canon_url(const char *url, size_t ulen) {
  if (!url) return NULL;
  char *out = malloc(ulen + 2);
  if (!out) return NULL;
  size_t o = 0;

  const char *frag = memchr(url, '#', ulen);
  size_t n = frag ? (size_t)(frag - url) : ulen;
  const char *q = memchr(url, '?', n);
  size_t head = q ? (size_t)(q - url) : n;
  if (head) memcpy(out, url, head);
  o = head;

  if (q) {
    size_t kept = 0, i = head + 1;
    while (i < n) {
      const char *k = url + i;
      const char *amp = memchr(k, '&', n - i);
      size_t plen = amp ? (size_t)(amp - k) : n - i;
      const char *eq = memchr(k, '=', plen);
      size_t nlen = eq ? (size_t)(eq - k) : plen;
      if (plen && !param_dropped(k, nlen)) {
        out[o++] = kept ? '&' : '?';
        memcpy(out + o, k, plen);
        o += plen;
        kept++;
      }
      if (!amp) break;
      i += plen + 1;
    }
  }
  out[o] = 0;
  return out;
}

/* ── stage 2: HTML entity decoding ────────────────────────────────────────── */

static size_t utf8_put(unsigned cp, char *o) {
  /* Never re-inject a control byte: CC_SEP is one, and "&#1;" must not be able
   * to forge a line break in the extracted text. */
  if (cp < 0x20 && cp != '\t') { o[0] = ' '; return 1; }
  if (cp < 0x80)    { o[0] = (char)cp; return 1; }
  if (cp < 0x800)   { o[0] = (char)(0xC0 | (cp >> 6));
                      o[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
  if (cp < 0x10000) { o[0] = (char)(0xE0 | (cp >> 12));
                      o[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                      o[2] = (char)(0x80 | (cp & 0x3F)); return 3; }
  o[0] = (char)(0xF0 | (cp >> 18));
  o[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
  o[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
  o[3] = (char)(0x80 | (cp & 0x3F));
  return 4;
}

/* `out` must hold n+1 bytes; the output is never longer than the input. */
static size_t ent_decode(const char *in, size_t n, char *out) {
  static const struct { const char *name; const char *rep; } named[] = {
    {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"},
    {"nbsp", " "}, {"middot", "\xc2\xb7"}, {"hellip", "..."},
    {"mdash", "-"}, {"ndash", "-"}, {"yen", "\xc2\xa5"}, {NULL, NULL}
  };
  size_t o = 0;
  for (size_t i = 0; i < n; ) {
    if (in[i] != '&') { out[o++] = in[i++]; continue; }
    size_t window = (n - i) > 12 ? 12 : (n - i);
    const char *semi = memchr(in + i, ';', window);
    if (!semi || semi == in + i + 1) { out[o++] = in[i++]; continue; }
    const char *body = in + i + 1;
    size_t bl = (size_t)(semi - body);
    if (body[0] == '#') {
      unsigned cp = 0;
      int ok = 1;
      if (bl >= 3 && (body[1] == 'x' || body[1] == 'X')) {
        for (size_t k = 2; k < bl; k++) {
          char c = body[k];
          int d = (c >= '0' && c <= '9') ? c - '0'
                : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
          if (d < 0) { ok = 0; break; }
          cp = cp * 16 + (unsigned)d;
        }
      } else if (bl >= 2) {
        for (size_t k = 1; k < bl; k++) {
          if (!isdigit((unsigned char)body[k])) { ok = 0; break; }
          cp = cp * 10 + (unsigned)(body[k] - '0');
        }
      } else ok = 0;
      if (ok && cp && cp <= 0x10FFFF) {
        o += utf8_put(cp, out + o);
        i += bl + 2;
        continue;
      }
      out[o++] = in[i++];
      continue;
    }
    int hit = 0;
    for (int k = 0; named[k].name; k++) {
      size_t l = strlen(named[k].name);
      if (l == bl && strncasecmp(body, named[k].name, l) == 0) {
        size_t rl = strlen(named[k].rep);
        memcpy(out + o, named[k].rep, rl);
        o += rl; i += bl + 2; hit = 1;
        break;
      }
    }
    if (!hit) out[o++] = in[i++];
  }
  out[o] = 0;
  return o;
}

/* ── stage 1: markup removal ──────────────────────────────────────────────── */

/* Element AND contents dropped. See the header for why each one is here. */
static const char *DROP_TAGS[] = {
  "script", "style", "noscript", "template", "svg", "canvas", "iframe",
  "object", "embed", "ins", NULL
};
/* Replaced by a line separator so the extracted text has line structure at all
 * — html_strip() collapses every whitespace run, newlines included, and a
 * one-line document has no useful line diff. */
static const char *BLOCK_TAGS[] = {
  "p", "br", "hr", "div", "li", "tr", "td", "th", "h1", "h2", "h3", "h4", "h5",
  "h6", "section", "article", "header", "footer", "nav", "aside", "main", "ul",
  "ol", "dl", "dt", "dd", "table", "tbody", "thead", "tfoot", "blockquote",
  "pre", "figure", "figcaption", "address", "form", "fieldset", "legend",
  "option", "label", "caption", "summary", "details", NULL
};
static int tag_in(const char *const *list, const char *p, size_t n) {
  for (int i = 0; list[i]; i++) {
    size_t l = strlen(list[i]);
    if (l == n && strncasecmp(p, list[i], l) == 0) return 1;
  }
  return 0;
}
/* Index just past the '>' of the matching close tag, or n when absent. */
static size_t find_close(const char *s, size_t n, size_t from, const char *name,
                         size_t nl) {
  if (!nl) return n;
  for (size_t i = from; i + 2 < n; i++) {
    if (s[i] != '<' || s[i + 1] != '/') continue;
    if (i + 2 + nl > n) continue;
    if (strncasecmp(s + i + 2, name, nl) != 0) continue;
    char after = (i + 2 + nl < n) ? s[i + 2 + nl] : '>';
    if (after != '>' && after != ' ' && after != '\t' && after != '\n' &&
        after != '\r' && after != '/') continue;
    for (size_t j = i + 2 + nl; j < n; j++) if (s[j] == '>') return j + 1;
    return n;
  }
  return n;
}
/* `out` must hold n+2 bytes; the output is never longer than the input. */
static size_t clean_markup(const char *s, size_t n, char *out) {
  size_t o = 0;
  for (size_t i = 0; i < n; ) {
    if (s[i] != '<') { out[o++] = s[i++]; continue; }
    if (i + 3 < n && s[i + 1] == '!' && s[i + 2] == '-' && s[i + 3] == '-') {
      size_t j = i + 4;
      while (j + 2 < n && !(s[j] == '-' && s[j + 1] == '-' && s[j + 2] == '>')) j++;
      i = (j + 2 < n) ? j + 3 : n;
      out[o++] = CC_SEP;
      continue;
    }
    if (i + 1 < n && (s[i + 1] == '!' || s[i + 1] == '?')) {
      size_t j = i + 1;
      while (j < n && s[j] != '>') j++;
      i = (j < n) ? j + 1 : n;
      continue;
    }
    size_t ns = i + 1;
    int closing = 0;
    if (ns < n && s[ns] == '/') { closing = 1; ns++; }
    size_t ne = ns;
    while (ne < n && (isalnum((unsigned char)s[ne]) || s[ne] == ':')) ne++;
    if (ne == ns || ne - ns > 32) { out[o++] = s[i++]; continue; }  /* "a < b" */

    /* namespace prefix: the local name is what the tag lists are written in */
    const char *nm = s + ns;
    size_t nl = ne - ns;
    for (size_t k = 0; k < ne - ns; k++)
      if (s[ns + k] == ':') { nm = s + ns + k + 1; nl = ne - (ns + k + 1); }

    size_t gt = ne;
    while (gt < n && s[gt] != '>') gt++;
    size_t past = (gt < n) ? gt + 1 : n;

    if (nl && tag_in(DROP_TAGS, nm, nl)) {
      out[o++] = CC_SEP;
      i = closing ? past : find_close(s, n, past, nm, nl);
      continue;
    }
    if (nl && tag_in(BLOCK_TAGS, nm, nl)) { out[o++] = CC_SEP; i = past; continue; }
    /* Inline tag: copied verbatim so html_strip() turns it into a word
     * boundary instead of gluing "foo<b>bar" into "foobar". */
    memcpy(out + o, s + i, past - i);
    o += past - i;
    i = past;
  }
  out[o] = 0;
  return o;
}

/* ── stage 3: token classification ────────────────────────────────────────── */

static int all_digits(const char *p, size_t n) {
  if (!n) return 0;
  for (size_t i = 0; i < n; i++) if (!isdigit((unsigned char)p[i])) return 0;
  return 1;
}
static int has_digit(const char *p, size_t n) {
  for (size_t i = 0; i < n; i++) if (isdigit((unsigned char)p[i])) return 1;
  return 0;
}
/* Trim ASCII punctuation that is grammar, not content. '-' is never trimmed —
 * it is a date separator — and '.'/':' only from the right. */
static void tok_core(const char *p, size_t n, const char **cp, size_t *cn) {
  static const char *trim = ",;!?()[]{}\"'`*|<>";
  size_t a = 0, b = n;
  while (a < b && p[a] && strchr(trim, p[a])) a++;
  while (b > a && p[b - 1] &&
         (strchr(trim, p[b - 1]) || p[b - 1] == '.' || p[b - 1] == ':')) b--;
  *cp = p + a;
  *cn = b - a;
}
/* 2026-07-26 | 2026-07-26T14:03:11Z | 2026-07-26T14:03:11+09:00 */
static int is_iso_date(const char *p, size_t n) {
  if (n < 10) return 0;
  if (p[4] != '-' || p[7] != '-') return 0;
  if (!all_digits(p, 4) || !all_digits(p + 5, 2) || !all_digits(p + 8, 2))
    return 0;
  if (n == 10) return 1;
  return (p[10] == 'T' || p[10] == 't');
}
/* Three numeric components separated by / . or -, exactly one of which is a
 * 4-digit year. Version strings ("1.2.3") and IPv4 ("192.168.0.1") do not
 * match, which is the point. */
static int is_num_date(const char *p, size_t n) {
  if (n < 6 || n > 12) return 0;
  size_t parts[3], plen[3];
  int np = 0;
  size_t st = 0;
  char sep = 0;
  for (size_t i = 0; i <= n; i++) {
    if (i == n || p[i] == '/' || p[i] == '.' || p[i] == '-') {
      if (np >= 3) return 0;
      if (i < n) { if (sep && p[i] != sep) return 0; sep = p[i]; }
      parts[np] = st; plen[np] = i - st; np++;
      st = i + 1;
    } else if (!isdigit((unsigned char)p[i])) return 0;
  }
  if (np != 3) return 0;
  int year = 0;
  for (int k = 0; k < 3; k++) {
    if (plen[k] == 0 || plen[k] > 4 || plen[k] == 3) return 0;
    if (plen[k] == 4) {
      long v = strtol(p + parts[k], NULL, 10);
      if (v < 1900 || v > 2999) return 0;
      year++;
    }
  }
  return year == 1;
}
/* hh:mm[:ss][.frac][Z | am | pm | ±hh:mm] */
static int is_clock(const char *p, size_t n) {
  if (n < 4) return 0;
  size_t i = 0, h0 = 0;
  while (i < n && isdigit((unsigned char)p[i])) i++;
  if (i - h0 < 1 || i - h0 > 2 || i >= n || p[i] != ':') return 0;
  i++;
  size_t m0 = i;
  while (i < n && isdigit((unsigned char)p[i])) i++;
  if (i - m0 != 2) return 0;
  if (i < n && p[i] == ':') {
    i++;
    size_t s0 = i;
    while (i < n && isdigit((unsigned char)p[i])) i++;
    if (i - s0 != 2) return 0;
  }
  if (i < n && p[i] == '.') {
    i++;
    size_t f0 = i;
    while (i < n && isdigit((unsigned char)p[i])) i++;
    if (i == f0) return 0;
  }
  if (i == n) return 1;
  if (n - i == 1 && (p[i] == 'Z' || p[i] == 'z')) return 1;
  if (n - i == 2 && (strncasecmp(p + i, "am", 2) == 0 ||
                     strncasecmp(p + i, "pm", 2) == 0)) return 1;
  if ((p[i] == '+' || p[i] == '-') && (n - i == 6 || n - i == 3)) return 1;
  return 0;
}
/* Bare epochs only. A 10-digit number is an epoch ONLY inside the plausible
 * window: Japanese phone numbers are 10-11 digits and a changed phone number on
 * a contact page is exactly the sort of change this module exists to report. */
static int is_epochish(const char *p, size_t n) {
  if (!all_digits(p, n)) return 0;
  if (n == 13) return 1;
  if (n == 10) {
    long long v = strtoll(p, NULL, 10);
    return v >= 1000000000LL && v <= 2147483647LL;
  }
  return 0;
}
/* CSRF field / session id / nonce / JWT / asset fingerprint. The character set
 * excludes '/' and '.', so a URL is never swallowed whole by this rule. */
static int is_opaque(const char *p, size_t n) {
  if (n < 28) return 0;
  int letters = 0, digits = 0, hexonly = 1;
  for (size_t i = 0; i < n; i++) {
    char c = p[i];
    if (!(isalnum((unsigned char)c) || c == '_' || c == '-' || c == '=')) return 0;
    if (isdigit((unsigned char)c)) digits++;
    else if (isalpha((unsigned char)c)) letters++;
    if (!isxdigit((unsigned char)c)) hexonly = 0;
  }
  if (hexonly && n >= 32) return 1;
  return letters > 0 && digits > 0;
}
static int is_urlish(const char *p, size_t n) {
  if (n < 8) return 0;
  for (size_t i = 0; i + 3 <= n; i++)
    if (p[i] == ':' && p[i + 1] == '/' && p[i + 2] == '/') return 1;
  return 0;
}
/* 1,284 | 12.3K | 45万 — a number as a page renders one. */
static int is_numberish(const char *p, size_t n) {
  if (!n || !has_digit(p, n)) return 0;
  size_t e = n;
  if (e >= 3 && (memcmp(p + e - 3, JA_MAN, 3) == 0 ||
                 memcmp(p + e - 3, JA_SEN, 3) == 0)) e -= 3;
  else if (strchr("KkMmBb", p[e - 1])) e -= 1;
  if (!e) return 0;
  for (size_t i = 0; i < e; i++)
    if (!isdigit((unsigned char)p[i]) && p[i] != ',' && p[i] != '.') return 0;
  return 1;
}

static const char *TIME_UNITS[] = {
  "second", "seconds", "sec", "secs", "minute", "minutes", "min", "mins",
  "hour", "hours", "hr", "hrs", "day", "days", "week", "weeks", "month",
  "months", "year", "years", NULL
};
/* Absolute counters. "件" is deliberately absent — see the header. */
static const char *COUNT_UNITS[] = {
  "view", "views", "hit", "hits", "read", "reads", "download", "downloads",
  "like", "likes", "comment", "comments", "subscriber", "subscribers",
  "play", "plays", "online", "impression", "impressions", "click", "clicks",
  "pv", JA_VIEWS, JA_VIEWSN, JA_ACCESS, JA_PLAY, JA_VIEWKT, NULL
};
static int word_in(const char *const *list, const char *p, size_t n) {
  for (int i = 0; list[i]; i++) {
    size_t l = strlen(list[i]);
    if (l == n && strncasecmp(p, list[i], l) == 0) return 1;
  }
  return 0;
}
/* Only the Japanese units and "views" are matched GLUED to a number
 * ("閲覧数1,284"), because those are written without a space; matching every
 * unit that way would start eating prose. */
static int glued_counter(const char *p, size_t n) {
  if (!has_digit(p, n)) return 0;
  return ja_has(p, n, JA_VIEWS) || ja_has(p, n, JA_ACCESS) ||
         ja_has(p, n, JA_PLAY)  || ja_has(p, n, JA_VIEWKT) ||
         (n < 24 && ci_has_n(p, n, "views"));
}

/* ── stage 3/4: per-line normalization ────────────────────────────────────── */

typedef struct {
  const char *p; size_t n;
  const char *rep;   /* placeholder literal, or NULL to keep the token     */
  char *own;         /* owned replacement (canonicalized URL)              */
  int drop;          /* absorbed by a phrase rule                          */
} cctok;

/* Appends one normalized line (plus '\n') to `out`. Returns 1 when a line was
 * appended, 0 when the line normalized to nothing. */
static int norm_line(sb *out, const char *s, size_t n) {
  if (n > (size_t)CC_MAX_LINE_BYTES) n = (size_t)CC_MAX_LINE_BYTES;
  cctok t[CC_MAX_TOKENS];
  int nt = 0;
  for (size_t i = 0; i < n && nt < CC_MAX_TOKENS; ) {
    while (i < n && isspace((unsigned char)s[i])) i++;
    if (i >= n) break;
    size_t st = i;
    while (i < n && !isspace((unsigned char)s[i])) i++;
    t[nt].p = s + st; t[nt].n = i - st;
    t[nt].rep = NULL; t[nt].own = NULL; t[nt].drop = 0;
    nt++;
  }
  if (!nt) return 0;

  /* phrase pass A: "<n> <unit> ago" */
  for (int i = 0; i + 2 < nt; i++) {
    const char *c0, *c1, *c2; size_t l0, l1, l2;
    tok_core(t[i].p, t[i].n, &c0, &l0);
    tok_core(t[i + 1].p, t[i + 1].n, &c1, &l1);
    tok_core(t[i + 2].p, t[i + 2].n, &c2, &l2);
    if (is_numberish(c0, l0) && word_in(TIME_UNITS, c1, l1) &&
        l2 == 3 && strncasecmp(c2, "ago", 3) == 0) {
      t[i].rep = "[AGO]"; t[i + 1].drop = 1; t[i + 2].drop = 1;
      i += 2;
    }
  }
  /* phrase pass B: "just now", "1,284 views", "Views: 1,284" */
  for (int i = 0; i + 1 < nt; i++) {
    if (t[i].rep || t[i].drop || t[i + 1].drop) continue;
    const char *c0, *c1; size_t l0, l1;
    tok_core(t[i].p, t[i].n, &c0, &l0);
    tok_core(t[i + 1].p, t[i + 1].n, &c1, &l1);
    if (l0 == 4 && strncasecmp(c0, "just", 4) == 0 &&
        l1 == 3 && strncasecmp(c1, "now", 3) == 0) {
      t[i].rep = "[AGO]"; t[i + 1].drop = 1; i++;
    } else if (is_numberish(c0, l0) && word_in(COUNT_UNITS, c1, l1)) {
      t[i].rep = "[COUNT]"; t[i + 1].drop = 1; i++;
    } else if (word_in(COUNT_UNITS, c0, l0) && is_numberish(c1, l1)) {
      t[i + 1].rep = "[COUNT]"; i++;
    }
  }

  /* token pass */
  for (int i = 0; i < nt; i++) {
    if (t[i].rep || t[i].drop) continue;
    const char *c; size_t l;
    tok_core(t[i].p, t[i].n, &c, &l);
    if (!l) { t[i].drop = 1; continue; }
    if (is_iso_date(c, l) || is_num_date(c, l)) { t[i].rep = "[DATE]"; continue; }
    if (is_clock(c, l))                        { t[i].rep = "[TIME]"; continue; }
    if (has_digit(c, l) && ja_has(c, l, JA_AGO)) { t[i].rep = "[AGO]"; continue; }
    if (has_digit(c, l) &&
        ((ja_has(c, l, JA_YEAR)  && ja_has(c, l, JA_MONTH)) ||
         (ja_has(c, l, JA_MONTH) && ja_has(c, l, JA_DAY)))) {
      t[i].rep = "[DATE]"; continue;
    }
    if (has_digit(c, l) && ja_has(c, l, JA_HOUR) &&
        (ja_has(c, l, JA_MIN) || ja_has(c, l, JA_SEC))) {
      t[i].rep = "[TIME]"; continue;
    }
    if (glued_counter(c, l)) { t[i].rep = "[COUNT]"; continue; }
    if (is_epochish(c, l))   { t[i].rep = "[EPOCH]"; continue; }
    if (is_opaque(c, l))     { t[i].rep = "[TOKEN]"; continue; }
    if (is_urlish(c, l))     { t[i].own = canon_url(c, l); continue; }
  }

  /* join with single spaces (stage 4) */
  size_t before = out->n;
  int wrote = 0;
  for (int i = 0; i < nt; i++) {
    if (t[i].drop) continue;
    const char *v; size_t vl;
    if (t[i].rep)      { v = t[i].rep; vl = strlen(v); }
    else if (t[i].own) { v = t[i].own; vl = strlen(v); }
    else               { v = t[i].p;   vl = t[i].n;    }
    if (!vl) continue;
    if (wrote) sb_ch(out, ' ');
    sb_add(out, v, vl);
    wrote = 1;
  }
  for (int i = 0; i < nt; i++) free(t[i].own);
  if (!wrote) {
    out->n = before;
    if (out->p) out->p[before] = 0;
    return 0;
  }
  sb_ch(out, '\n');
  return 1;
}

/* ── the normalizer ───────────────────────────────────────────────────────── */

static int looks_html(const char *ct, const char *s, size_t n) {
  if (ct && *ct) {
    if (ci_has(ct, "html")) return 1;
    if (ci_has(ct, "json") || ci_has(ct, "csv") || ci_has(ct, "javascript"))
      return 0;
  }
  size_t i = 0;
  while (i < n && isspace((unsigned char)s[i])) i++;
  return i < n && s[i] == '<';
}

char *content_change_normalize(const void *body, size_t body_len,
                               const char *content_type) {
  if (!body || !body_len) return NULL;
  if ((long long)body_len > max_doc()) return NULL;

  const char *raw = (const char *)body;
  char *flat = NULL;                       /* lines separated by CC_SEP */

  if (looks_html(content_type, raw, body_len)) {
    char *cleaned = malloc(body_len + 2);
    if (!cleaned) return NULL;
    clean_markup(raw, body_len, cleaned);
    char *stripped = html_strip(cleaned);  /* reuse: lib/htmlparse.c */
    free(cleaned);
    if (!stripped) return NULL;
    size_t sl = strlen(stripped);
    flat = malloc(sl + 2);
    if (!flat) { free(stripped); return NULL; }
    ent_decode(stripped, sl, flat);
    free(stripped);
  } else {
    /* Non-markup (JSON, CSV, plain text): keep the document's own line
     * structure, and do NOT decode entities — "&amp;" inside a JSON string is
     * literal content there, not markup. */
    flat = malloc(body_len + 2);
    if (!flat) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < body_len; i++) {
      char c = raw[i];
      flat[o++] = (c == '\n' || c == '\r') ? CC_SEP : c;
    }
    flat[o] = 0;
  }

  sb out = {0};
  int lines = 0, truncated = 0;
  const char *p = flat;
  for (;;) {
    if (lines >= CC_MAX_LINES) { truncated = 1; break; }
    const char *e = strchr(p, CC_SEP);
    size_t n = e ? (size_t)(e - p) : strlen(p);
    if (n) lines += norm_line(&out, p, n);
    if (!e) break;
    p = e + 1;
  }
  free(flat);

  if (truncated) sb_str(&out, "[TRUNCATED]\n");
  if (!out.p || !out.n) { free(out.p); return NULL; }
  if (out.p[out.n - 1] == '\n') out.p[--out.n] = 0;   /* a list of lines */
  return out.p;
}

/* ── unified diff ─────────────────────────────────────────────────────────── */

typedef struct { const char *p; size_t n; } cline;
typedef struct { char op; int ia, ib; } ccop;

static int split_lines(const char *t, cline *out, int max) {
  int k = 0;
  if (!t || !*t) return 0;
  const char *s = t;
  while (k < max) {
    const char *e = strchr(s, '\n');
    size_t n = e ? (size_t)(e - s) : strlen(s);
    out[k].p = s; out[k].n = n; k++;
    if (!e) break;
    s = e + 1;
    if (!*s) break;
  }
  return k;
}
static int line_eq(const cline *a, const cline *b) {
  return a->n == b->n && (a->n == 0 || memcmp(a->p, b->p, a->n) == 0);
}
/* Appends "<pfx><line>\n"; returns -1 once the byte cap would be crossed. */
static int emit_line(sb *o, char pfx, const cline *l, size_t cap) {
  if (o->n + l->n + 2 > cap) return -1;
  sb_ch(o, pfx);
  sb_add(o, l->p, l->n);
  sb_ch(o, '\n');
  return 0;
}

char *content_change_diff(const char *label, const char *old_text,
                          const char *new_text, int *out_added,
                          int *out_removed, int *out_truncated) {
  if (out_added) *out_added = 0;
  if (out_removed) *out_removed = 0;
  if (out_truncated) *out_truncated = 0;
  if (!old_text && !new_text) return NULL;

  cline *A = malloc(sizeof(cline) * CC_MAX_LINES);
  cline *B = malloc(sizeof(cline) * CC_MAX_LINES);
  if (!A || !B) { free(A); free(B); return NULL; }
  int na = split_lines(old_text, A, CC_MAX_LINES);
  int nb = split_lines(new_text, B, CC_MAX_LINES);

  int p = 0;
  while (p < na && p < nb && line_eq(&A[p], &B[p])) p++;
  int s = 0;
  while (s < na - p && s < nb - p && line_eq(&A[na - 1 - s], &B[nb - 1 - s])) s++;
  int am = na - p - s, bm = nb - p - s;
  if (am == 0 && bm == 0) { free(A); free(B); return NULL; }

  size_t cap = (size_t)diff_max();
  sb o = {0};
  char hdr[256];
  snprintf(hdr, sizeof hdr, "--- a/%.100s\n+++ b/%.100s\n",
           label ? label : "content", label ? label : "content");
  sb_str(&o, hdr);

  int added = 0, removed = 0, trunc = 0;

  if (am <= CC_LCS_MAX_LINES && bm <= CC_LCS_MAX_LINES) {
    /* Suffix DP: L[i][j] = LCS length of A[p+i..] and B[p+j..]. uint16 is
     * sufficient because both dimensions are capped at CC_LCS_MAX_LINES. */
    size_t w = (size_t)bm + 1;
    uint16_t *L = calloc(((size_t)am + 1) * w, sizeof(uint16_t));
    ccop *ops = malloc(sizeof(ccop) * ((size_t)na + (size_t)nb + 8));
    if (!L || !ops) { free(L); free(ops); trunc = 1; goto coarse; }

    for (int i = am - 1; i >= 0; i--) {
      for (int j = bm - 1; j >= 0; j--) {
        size_t at = (size_t)i * w + (size_t)j;
        if (line_eq(&A[p + i], &B[p + j]))
          L[at] = (uint16_t)(L[(size_t)(i + 1) * w + (size_t)(j + 1)] + 1);
        else {
          uint16_t d = L[(size_t)(i + 1) * w + (size_t)j];
          uint16_t r = L[(size_t)i * w + (size_t)(j + 1)];
          L[at] = d >= r ? d : r;
        }
      }
    }

    int nops = 0, ca = 0, cb = 0;
    for (int k = 0; k < p; k++) {
      ops[nops].op = ' '; ops[nops].ia = ca++; ops[nops].ib = cb++; nops++;
    }
    {
      int i = 0, j = 0;
      while (i < am || j < bm) {
        if (i < am && j < bm && line_eq(&A[p + i], &B[p + j])) {
          ops[nops].op = ' '; ops[nops].ia = ca++; ops[nops].ib = cb++; nops++;
          i++; j++;
        } else if (i < am && (j == bm ||
                              L[(size_t)(i + 1) * w + (size_t)j] >=
                              L[(size_t)i * w + (size_t)(j + 1)])) {
          /* Deletions before additions inside a hunk — that is what diff(1)
           * and every review tool emit, and a "unified diff" that inverts it
           * reads as a different change to anyone skimming it. */
          ops[nops].op = '-'; ops[nops].ia = ca++; ops[nops].ib = cb; nops++;
          i++; removed++;
        } else {
          ops[nops].op = '+'; ops[nops].ia = ca; ops[nops].ib = cb++; nops++;
          j++; added++;
        }
      }
    }
    for (int k = 0; k < s; k++) {
      ops[nops].op = ' '; ops[nops].ia = ca++; ops[nops].ib = cb++; nops++;
    }
    free(L);

    int i = 0;
    while (i < nops) {
      int c = i;
      while (c < nops && ops[c].op == ' ') c++;
      if (c >= nops) break;
      int hs = c - CC_DIFF_CONTEXT;
      if (hs < 0) hs = 0;
      int he = c, run = 0;
      for (int j = c; j < nops; j++) {
        if (ops[j].op == ' ') { run++; if (run > 2 * CC_DIFF_CONTEXT) break; }
        else { run = 0; he = j; }
      }
      he += CC_DIFF_CONTEXT;
      if (he >= nops) he = nops - 1;

      int acnt = 0, bcnt = 0;
      for (int j = hs; j <= he; j++) {
        if (ops[j].op != '+') acnt++;
        if (ops[j].op != '-') bcnt++;
      }
      char hh[96];
      snprintf(hh, sizeof hh, "@@ -%d,%d +%d,%d @@\n",
               acnt ? ops[hs].ia + 1 : ops[hs].ia, acnt,
               bcnt ? ops[hs].ib + 1 : ops[hs].ib, bcnt);
      if (o.n + strlen(hh) > cap) { trunc = 1; break; }
      sb_str(&o, hh);
      for (int j = hs; j <= he; j++) {
        const cline *l = (ops[j].op == '+') ? &B[ops[j].ib] : &A[ops[j].ia];
        if (emit_line(&o, ops[j].op, l, cap) != 0) { trunc = 1; break; }
      }
      if (trunc) break;
      i = he + 1;
    }
    free(ops);
    goto done;
  }

coarse:
  /* Too large for an exact diff. An honest partial beats a wrong one: report
   * the changed region's extent and a bounded prefix of each side. */
  trunc = 1;
  removed = am;
  added = bm;
  {
    char hh[96];
    snprintf(hh, sizeof hh, "@@ -%d,%d +%d,%d @@\n", p + 1, am, p + 1, bm);
    sb_str(&o, hh);
    int lim = am < CC_COARSE_LINES ? am : CC_COARSE_LINES;
    for (int k = 0; k < lim; k++)
      if (emit_line(&o, '-', &A[p + k], cap) != 0) break;
    lim = bm < CC_COARSE_LINES ? bm : CC_COARSE_LINES;
    for (int k = 0; k < lim; k++)
      if (emit_line(&o, '+', &B[p + k], cap) != 0) break;
  }

done:
  if (trunc) sb_str(&o, "@@ diff truncated @@\n");
  free(A);
  free(B);
  if (out_added) *out_added = added;
  if (out_removed) *out_removed = removed;
  if (out_truncated) *out_truncated = trunc;
  return o.p;
}

/* ── schema ───────────────────────────────────────────────────────────────── */

static pthread_mutex_t g_mig_lock = PTHREAD_MUTEX_INITIALIZER;
static int             g_mig_done = 0;

void content_change_ensure_schema(db_handle *db) {
  if (!db || !db->h || g_mig_done) return;
  pthread_mutex_lock(&g_mig_lock);
  if (!g_mig_done) {
    /* (B) then (C) then (A). The index in (C) names a column that does not
     * exist until (B) has run — which is exactly why it cannot live in
     * schema.sql, a file exec'd in ONE shot BEFORE the boot-migration block.
     * The two exec()s are separate so a failure of the opportunistic index
     * cannot abandon the script before the table is created. */
    ensure_column(db, "sources", "watch_content", "INTEGER");
    char *e1 = NULL;
    sqlite3_exec(db->h,
      "CREATE INDEX IF NOT EXISTS idx_sources_watch_content"
      " ON sources(watch_content) WHERE watch_content IS NOT NULL;",
      NULL, NULL, &e1);
    sqlite3_free(e1);

    char *e2 = NULL;
    int rc = sqlite3_exec(db->h,
      "CREATE TABLE IF NOT EXISTS content_snapshots ("
      "  source_id TEXT NOT NULL, ref_key TEXT NOT NULL,"
      "  content_sha256 TEXT NOT NULL, extract_text TEXT,"
      "  captured_at TEXT NOT NULL DEFAULT (datetime('now')),"
      "  PRIMARY KEY (source_id, ref_key, content_sha256));"
      "CREATE INDEX IF NOT EXISTS idx_content_snapshots_recent"
      " ON content_snapshots(source_id, ref_key, captured_at DESC);",
      NULL, NULL, &e2);
    if (rc != SQLITE_OK)
      fprintf(stderr, "[cchange] schema: %s\n", e2 ? e2 : "?");
    else
      g_mig_done = 1;   /* retry next call on failure; never brick the caller */
    sqlite3_free(e2);
  }
  pthread_mutex_unlock(&g_mig_lock);
}

/* ── per-source gate ──────────────────────────────────────────────────────── */

typedef struct { char id[96]; int on; time_t ts; } ccc_ent;
static ccc_ent         g_ccc[CC_CACHE_SLOTS];
static pthread_mutex_t g_ccc_lock = PTHREAD_MUTEX_INITIALIZER;

/* watch_content is tri-state: NULL follows the type default (scraped /
 * web_request), 0 is a hard opt-out, 1 a hard opt-in for any type. Cached for
 * CC_CACHE_TTL seconds so the gate costs no SQLite round trip on the collector
 * hot path; the price is that an operator flipping it waits up to a minute. */
static int watch_on(db_handle *db, const char *sid) {
  time_t now = time(NULL);
  unsigned slot = fnv32(sid) % CC_CACHE_SLOTS;

  pthread_mutex_lock(&g_ccc_lock);
  if (strcmp(g_ccc[slot].id, sid) == 0 && now - g_ccc[slot].ts < CC_CACHE_TTL) {
    int cached = g_ccc[slot].on;
    pthread_mutex_unlock(&g_ccc_lock);
    return cached;
  }
  pthread_mutex_unlock(&g_ccc_lock);

  int on = -1, found = 0;
  char type[32] = {0};
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "SELECT type,watch_content FROM sources WHERE id=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, sid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      found = 1;
      ccopy(type, sizeof type, ctext(s, 0));
      if (sqlite3_column_type(s, 1) != SQLITE_NULL)
        on = sqlite3_column_int(s, 1) != 0;
    }
    sqlite3_finalize(s);
  } else if (sqlite3_prepare_v2(db->h, "SELECT type FROM sources WHERE id=?1",
                                -1, &s, NULL) == SQLITE_OK) {
    /* watch_content not migrated yet — fall back to the type default rather
     * than failing closed, so the feature works before db.c has been wired. */
    sqlite3_bind_text(s, 1, sid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      found = 1;
      ccopy(type, sizeof type, ctext(s, 0));
    }
    sqlite3_finalize(s);
  }
  if (!found) {
    /* Not every registered source has a sources row (OSINT services are
     * registry-only); source_def.type is authoritative for those. */
    const source_def *d = registry_get(sid);
    if (d && d->type) ccopy(type, sizeof type, d->type);
  }
  if (on < 0)
    on = (strcmp(type, "scraped") == 0 || strcmp(type, "web_request") == 0);

  pthread_mutex_lock(&g_ccc_lock);
  ccopy(g_ccc[slot].id, sizeof g_ccc[slot].id, sid);
  g_ccc[slot].on = on;
  g_ccc[slot].ts = now;
  pthread_mutex_unlock(&g_ccc_lock);
  return on;
}

/* ── snapshot store ───────────────────────────────────────────────────────── */

/* ON CONFLICT DO UPDATE, not DO NOTHING: the PK is (source_id, ref_key,
 * content_sha256), so a page that flips A -> B -> A collides with its own older
 * row. Refreshing captured_at is what keeps "most recent snapshot" honest and
 * what makes the NEXT A -> B transition detectable at all. */
static int snapshot_store(db_handle *db, const char *sid, const char *key,
                          const char *sha, const char *text, const char *ts) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO content_snapshots(source_id,ref_key,content_sha256,"
        "extract_text,captured_at) VALUES(?1,?2,?3,?4,?5) "
        "ON CONFLICT(source_id,ref_key,content_sha256) DO UPDATE SET "
        "captured_at=excluded.captured_at, extract_text=excluded.extract_text",
        -1, &s, NULL) != SQLITE_OK) {
    fprintf(stderr, "[cchange] snapshot prepare: %s\n", sqlite3_errmsg(db->h));
    return -1;
  }
  sqlite3_bind_text(s, 1, sid, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, key, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, sha, -1, SQLITE_TRANSIENT);
  if (text) sqlite3_bind_text(s, 4, text, -1, SQLITE_TRANSIENT);
  else      sqlite3_bind_null(s, 4);
  sqlite3_bind_text(s, 5, ts, -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "[cchange] snapshot insert: %s\n", sqlite3_errmsg(db->h));
    return -1;
  }
  return 0;
}

/* Inline retention: exact, one indexed range per key, and it is what actually
 * bounds the table for live sources. The pod only reclaims keys that stopped
 * being fetched, which inline pruning by definition never revisits. */
static void snapshot_prune(db_handle *db, const char *sid, const char *key) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "DELETE FROM content_snapshots WHERE source_id=?1 AND ref_key=?2 "
        "AND rowid NOT IN (SELECT rowid FROM content_snapshots "
        "WHERE source_id=?1 AND ref_key=?2 "
        "ORDER BY captured_at DESC, rowid DESC LIMIT ?3)",
        -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, sid, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, key, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (s, 3, keep_n());
  sqlite3_step(s);
  sqlite3_finalize(s);
}

/* ── the change event ─────────────────────────────────────────────────────── */

/* Back off to a UTF-8 boundary so a truncated excerpt is never a broken
 * sequence. Returns the byte length to keep. */
static size_t utf8_trim(const char *s, size_t want) {
  size_t l = strlen(s);
  if (l <= want) return l;
  while (want && ((unsigned char)s[want] & 0xC0) == 0x80) want--;
  return want;
}

static void emit_change(db_handle *db, const char *sid, const char *key,
                        const char *url, const char *prev_sha,
                        const char *new_sha, const char *raw_sha,
                        const char *prev_ts, const char *now_ts,
                        const char *diff, int added, int removed,
                        int diff_trunc, const char *diff_reason,
                        const char *ev_uid, size_t text_bytes) {
  /* uid = source + ref + NEW content hash + second. The content hash is what
   * makes A -> B -> A -> B within one second three distinct events instead of
   * one overwritten row; the second keeps a page that oscillates between two
   * states over hours from folding every visit to state B onto one item. */
  char uid[512], iso[32];
  now_iso(iso);
  snprintf(uid, sizeof uid, "%.200s|content_change|%08x|%.12s|%lld",
           sid, fnv32(key), new_sha, (long long)time(NULL));

  cJSON *pr = cJSON_CreateObject();
  cJSON_AddStringToObject(pr, "ref_key", key);
  cJSON_AddItemToObject(pr, "url", url ? cJSON_CreateString(url)
                                       : cJSON_CreateNull());
  cJSON_AddStringToObject(pr, "prev_sha256", prev_sha);
  cJSON_AddStringToObject(pr, "new_sha256", new_sha);
  cJSON_AddStringToObject(pr, "raw_sha256", raw_sha);
  cJSON_AddStringToObject(pr, "prev_captured_at", prev_ts);
  cJSON_AddStringToObject(pr, "captured_at", now_ts);
  cJSON_AddNumberToObject(pr, "added_lines", added);
  cJSON_AddNumberToObject(pr, "removed_lines", removed);
  cJSON_AddBoolToObject(pr, "diff_available", diff ? 1 : 0);
  cJSON_AddItemToObject(pr, "diff", diff ? cJSON_CreateString(diff)
                                         : cJSON_CreateNull());
  cJSON_AddBoolToObject(pr, "diff_truncated", diff_trunc);
  if (diff_reason)
    cJSON_AddStringToObject(pr, "diff_unavailable_reason", diff_reason);
  cJSON_AddNumberToObject(pr, "normalized_bytes", (double)text_bytes);
  cJSON_AddNumberToObject(pr, "normalizer_version", CC_NORM_VERSION);
  cJSON_AddStringToObject(pr, "evidence_item_uid", ev_uid);
  char *props = cJSON_PrintUnformatted(pr);
  cJSON_Delete(pr);

  char title[320], summary[128];
  snprintf(title, sizeof title, "Content changed: %.240s", key);
  snprintf(summary, sizeof summary, "+%d / -%d lines changed", added, removed);

  /* The body carries only an excerpt: core/intel.c runs MeCab over it in
   * fts_write(), and segmenting a 64 KiB diff on the collector thread costs
   * real time for no search value. properties.diff holds the whole thing. */
  char *excerpt = NULL;
  if (diff) {
    size_t dl = strlen(diff);
    size_t n = utf8_trim(diff, CC_BODY_EXCERPT);
    excerpt = malloc(n + 24);
    if (excerpt) {
      memcpy(excerpt, diff, n);
      excerpt[n] = 0;
      if (n < dl) memcpy(excerpt + n, "\n... (truncated)", 17);
    }
  }

  intel_sink sink = intel_sink_make(db, sid, "legacy");
  intel_item it;
  memset(&it, 0, sizeof it);
  it.uid = uid;
  it.title = title;
  it.body = excerpt ? excerpt : summary;
  it.summary = summary;
  it.link = url;
  it.published_at = iso;
  it.record_type = "content_change";
  it.properties_json = props ? props : "{}";
  it.tags_json = "[\"content_change\"]";
  if (sink.emit) sink.emit(&sink, &it);
  /* sink_state is a flat calloc'd struct owning no pointers; core/intel.c
   * exposes no destructor, and one leak per detected change is a real leak in
   * a process that runs for months. */
  free(sink.ctx);

  free(excerpt);
  free(props);
  fprintf(stderr, "[cchange] %s changed: %s (+%d/-%d)\n",
          sid, key, added, removed);
}

/* ── capture ──────────────────────────────────────────────────────────────── */

/* Re-entrancy guard. The intel emit reaches alert evaluation and, through it,
 * can reach an outbound request; without this the hook would recurse into
 * capture on the same thread and attribute someone else's response to this
 * ref_key. */
static __thread int g_busy = 0;

static int capture_inner(db_handle *db, const char *source_id,
                         const char *ref_key, const char *url,
                         const void *body, size_t body_len,
                         const char *content_type) {
  content_change_ensure_schema(db);

  /* Redact ONCE, here, and use the redacted form for everything this module
   * stores or emits. The raw `url` is still what evidence_capture() is given
   * further down — it does its own redaction and needs the original to dedup
   * against what evidence_http_hook() already stored. */
  char *safe_url = cc_redact_url(url);

  char key[CC_REFKEY_MAX];
  if (ref_key && *ref_key) ccopy(key, sizeof key, ref_key);
  else {
    char *cu = canon_url(safe_url ? safe_url : "",
                         safe_url ? strlen(safe_url) : 0);
    ccopy(key, sizeof key, cu);
    free(cu);
  }
  if (!key[0]) { free(safe_url); return CC_SKIP_EMPTY; }

  char *text = content_change_normalize(body, body_len, content_type);
  if (!text || !*text) { free(text); free(safe_url); return CC_SKIP_EMPTY; }
  size_t tlen = strlen(text);

  char sha[65];
  sha256_norm(text, sha);

  char prev_sha[65] = {0}, prev_ts[40] = {0};
  char *prev_text = NULL;
  int have_prev = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT content_sha256,extract_text,captured_at FROM content_snapshots "
        "WHERE source_id=?1 AND ref_key=?2 "
        "ORDER BY captured_at DESC, rowid DESC LIMIT 1", -1, &s, NULL)
      != SQLITE_OK) {
    fprintf(stderr, "[cchange] select: %s\n", sqlite3_errmsg(db->h));
    free(text); free(safe_url);
    return CC_ERR;
  }
  sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, key, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) {
    have_prev = 1;
    ccopy(prev_sha, sizeof prev_sha, ctext(s, 0));
    const char *pt = ctext(s, 1);
    if (pt) prev_text = strdup(pt);
    ccopy(prev_ts, sizeof prev_ts, ctext(s, 2));
  }
  sqlite3_finalize(s);

  /* Stored text is capped; the HASH is over the FULL normalized text, so a
   * change past the cap is still detected — only its diff is unavailable. */
  size_t store_len = tlen;
  if ((long long)store_len > text_max())
    store_len = utf8_trim(text, (size_t)text_max());
  char *stored = malloc(store_len + 1);
  if (stored) { memcpy(stored, text, store_len); stored[store_len] = 0; }

  char ts[20];
  now_utc_sql(ts);

  if (!have_prev) {
    /* A first sighting is not a change: it establishes the baseline silently. */
    snapshot_store(db, source_id, key, sha, stored, ts);
    snapshot_prune(db, source_id, key);
    free(stored); free(prev_text); free(text); free(safe_url);
    return CC_BASELINE;
  }
  if (strcmp(prev_sha, sha) == 0) {
    /* Deliberately NO write: touching captured_at every tick would put one
     * SQLite write per watched fetch on the collector thread, and this row is
     * already the most recent one for the key. */
    free(stored); free(prev_text); free(text); free(safe_url);
    return CC_UNCHANGED;
  }

  int added = 0, removed = 0, dtrunc = 0;
  const char *reason = NULL;
  char *diff = NULL;
  if (!prev_text) reason = "no_baseline_text";
  else {
    /* Diff the two STORED-length texts so both sides are cut at the same
     * offset; otherwise the tail of the new text reads as a pure addition. */
    diff = content_change_diff(key, prev_text, stored ? stored : text,
                               &added, &removed, &dtrunc);
    if (!diff) reason = "beyond_stored_text";
  }

  char raw_sha[65];
  sha256_hex_bytes(body, body_len, raw_sha);

  /* Reuse of the roadmap-17 blob store, and ONLY on a genuine change — see the
   * storage note in the header. It is a no-op unless the operator has set
   * sources.capture_evidence=1, it dedups against whatever evidence_http_hook()
   * already stored, and it can neither fail this call nor block on the network. */
  char ev_uid[288];
  snprintf(ev_uid, sizeof ev_uid, "cc:%.80s|%.180s", source_id, key);
  evidence_capture(db, ev_uid, source_id, url, "GET", NULL, 200, NULL,
                   body, body_len, content_type);

  snapshot_store(db, source_id, key, sha, stored, ts);
  snapshot_prune(db, source_id, key);

  emit_change(db, source_id, key, safe_url, prev_sha, sha, raw_sha, prev_ts, ts,
              diff, added, removed, dtrunc, reason, ev_uid, tlen);

  free(diff); free(stored); free(prev_text); free(text); free(safe_url);
  return CC_CHANGED;
}

int content_change_capture(db_handle *db, const char *source_id,
                           const char *ref_key, const char *url,
                           const void *body, size_t body_len,
                           const char *content_type) {
  if (cc_disabled()) return CC_SKIP_DISABLED;
  if (!db || !db->h || !source_id || !*source_id) return CC_SKIP_DISABLED;
  if (!body || !body_len) return CC_SKIP_EMPTY;
  if ((long long)body_len > max_doc()) return CC_SKIP_TOO_BIG;
  if (g_busy) return CC_SKIP_DISABLED;
  if (!watch_on(db, source_id)) return CC_SKIP_TYPE;

  g_busy = 1;
  int rc = capture_inner(db, source_id, ref_key, url, body, body_len,
                         content_type);
  g_busy = 0;
  return rc;
}

/* ── thread-local scope + the httpclient hook ─────────────────────────────── */

typedef struct { db_handle *db; char source_id[96]; int active; } cc_scope;
static __thread cc_scope g_scope;

void content_change_scope_begin(db_handle *db, const char *source_id) {
  g_scope.active = 0;
  if (!db || !db->h || !source_id || !*source_id) return;
  ccopy(g_scope.source_id, sizeof g_scope.source_id, source_id);
  g_scope.db = db;
  g_scope.active = 1;
}

void content_change_scope_end(void) {
  g_scope.active = 0;
  g_scope.db = NULL;
  g_scope.source_id[0] = 0;
}

int content_change_http_hook(const char *method, const char *url, long status,
                             const void *body, size_t body_len) {
  if (!g_scope.active || !g_scope.db) return CC_SKIP_DISABLED;
  /* A POST is a query, not a page; a 404 body or a rate-limit page is not
   * "the content changed", and admitting them would fire one event on every
   * outage and a second one on every recovery. */
  if (method && strcasecmp(method, "GET") != 0) return CC_SKIP_DISABLED;
  if (status != 200) return CC_SKIP_DISABLED;
  return content_change_capture(g_scope.db, g_scope.source_id, NULL, url,
                                body, body_len, NULL);
}

/* ── retention sweep (the pod) ────────────────────────────────────────────── */

static long run_delete(db_handle *db, const char *sql, int a, int b,
                       const char *txt) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    fprintf(stderr, "[cchange] sweep prepare: %s\n", sqlite3_errmsg(db->h));
    return -1;
  }
  if (txt) sqlite3_bind_text(s, 1, txt, -1, SQLITE_TRANSIENT);
  else     sqlite3_bind_int (s, 1, a);
  sqlite3_bind_int(s, 2, b);
  long n = (sqlite3_step(s) == SQLITE_DONE) ? (long)sqlite3_changes(db->h) : -1;
  sqlite3_finalize(s);
  return n;
}

long content_change_sweep(db_handle *db, int limit, volatile int *cancel) {
  if (!db || !db->h) return -1;
  content_change_ensure_schema(db);
  int lim = (limit > 0 && limit <= 200000) ? limit : 20000;
  long total = 0;

  char age[32];
  snprintf(age, sizeof age, "-%d days", max_age_days());
  long n = run_delete(db,
      "DELETE FROM content_snapshots WHERE rowid IN ("
      " SELECT rowid FROM content_snapshots"
      "  WHERE captured_at < datetime('now', ?1) LIMIT ?2)", 0, lim, age);
  if (n < 0) return -1;
  total += n;
  if (cancel && *cancel) return total;

  /* Global retention. Inline pruning already bounds live keys; this reclaims
   * keys nothing fetches any more (a source removed, a URL that 404s now). */
  n = run_delete(db,
      "DELETE FROM content_snapshots WHERE rowid IN ("
      " SELECT rid FROM (SELECT rowid AS rid, ROW_NUMBER() OVER ("
      "   PARTITION BY source_id, ref_key"
      "   ORDER BY captured_at DESC, rowid DESC) AS rn"
      "  FROM content_snapshots) WHERE rn > ?1 LIMIT ?2)",
      keep_n(), lim, NULL);
  if (n < 0) return total;
  total += n;

  if (total) fprintf(stderr, "[cchange] sweep pruned %ld snapshot(s)\n", total);
  return total;
}
