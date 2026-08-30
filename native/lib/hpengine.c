/* lib/hpengine.c — see hpengine.h.
 *
 * Real fetch or honest empty, everywhere. The only values that ever reach the
 * sink are bytes that came back from the upstream endpoint in this run; there
 * is no default record, no cached sample, no "source found" placeholder. A row
 * that cannot fetch emits zero items and logs why. */
#include "hpengine.h"
#include "csv.h"
#include "xlsx.h"
#include "osintemit.h"   /* jo_shape_notice(): schema drift reported as data */
#include "htmlparse.h"   /* the one anchor scanner + dedupe set */
#include "../core/httpclient.h"
#include "../third_party/cJSON.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Bounds exist only to keep one pathological response from exhausting memory —
 * they are NOT an editorial filter. Per the exhaustive-use rule
 * (docs/SOURCE_EXHAUSTIVENESS.md) they are set well above any real record, and
 * whenever one of them actually bites the record says so in-band
 * (`_fields_dropped`, `_array_truncated`, `_records_truncated`) so a consumer
 * can never mistake a truncated record for a complete one. */
#define HP_MAX_PROPS     2048   /* exhaustive-ok: memory guard, stamped     */
#define HP_MAX_ARR        256   /* exhaustive-ok: memory guard, stamped     */
#define HP_MAX_DEPTH        8   /* exhaustive-ok: memory guard, stamped     */
#define HP_HTTP_TIMEOUT 20000
#define HP_PAGE_MAX_DEF    10   /* exhaustive-ok: runaway guard, stamped    */
#define HP_DETAIL_MAX_DEF  25   /* exhaustive-ok: request budget, stamped   */

/* Grown on demand rather than fixed. The fixed HP_MAX_SOURCES array silently
 * ate every row past the cap: measured 2026-08-22, 1,481 rows were dropped at
 * boot — batches 18 and 19 alone register more than 1,024 between them, so most
 * of two batches was registered into nothing. It did print a line per drop, but
 * 1,481 lines of stderr at startup is not a signal anyone receives, and
 * /api/status showed no trace of it. A registry that cannot hold the registry
 * is not a bound worth keeping. */
static const hp_source **g_specs = NULL;
static int g_nspecs = 0;
static int g_cspecs = 0;

/* ── small utilities ─────────────────────────────────────────────────────── */

static char *hp_dup(const char *s) { return s ? strdup(s) : NULL; }

static char *hp_urlenc(const char *s) {
  if (!s) s = "";
  size_t n = strlen(s);
  char *out = malloc(n * 3 + 1);
  if (!out) return NULL;
  static const char hx[] = "0123456789ABCDEF";
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out[j++] = (char)c;
    else { out[j++] = '%'; out[j++] = hx[c >> 4]; out[j++] = hx[c & 15]; }
  }
  out[j] = 0;
  return out;
}

/* base64 of "<s>:" — HTTP Basic with an empty password, which is how the
 * Companies House / Prozorro style key-as-username APIs authenticate. */
static char *hp_b64_userpass(const char *s) {
  static const char T[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  if (!s) s = "";
  size_t n = strlen(s) + 1;                 /* + the ':' */
  char *in = malloc(n + 1);
  if (!in) return NULL;
  snprintf(in, n + 1, "%s:", s);
  char *out = malloc(((n + 2) / 3) * 4 + 1);
  if (!out) { free(in); return NULL; }
  size_t o = 0;
  for (size_t i = 0; i < n; i += 3) {
    unsigned v = (unsigned char)in[i] << 16;
    if (i + 1 < n) v |= (unsigned char)in[i + 1] << 8;
    if (i + 2 < n) v |= (unsigned char)in[i + 2];
    out[o++] = T[(v >> 18) & 63];
    out[o++] = T[(v >> 12) & 63];
    out[o++] = (i + 1 < n) ? T[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < n) ? T[v & 63]        : '=';
  }
  out[o] = 0;
  free(in);
  return out;
}

/* Derived forms of the pivot entity. Every member is malloc'd or NULL. */
typedef struct {
  char *raw, *enc, *digits, *cik10, *host, *user, *lower, *upper, *nospace,
       *key, *keyb64;
} hp_vars;

static void hp_vars_free(hp_vars *v) {
  free(v->raw); free(v->enc); free(v->digits); free(v->cik10); free(v->host);
  free(v->user); free(v->lower); free(v->upper); free(v->nospace);
  free(v->key); free(v->keyb64);
  memset(v, 0, sizeof *v);
}

/* host part of "https://a.b/c", "user@a.b" or a bare host. */
static char *hp_host_of(const char *s) {
  if (!s) return NULL;
  const char *p = strstr(s, "://");
  p = p ? p + 3 : s;
  const char *at = strchr(p, '@');
  if (at) p = at + 1;
  size_t n = 0;
  while (p[n] && p[n] != '/' && p[n] != '?' && p[n] != ':' && p[n] != ' ') n++;
  if (!n) return NULL;
  char *out = malloc(n + 1);
  if (!out) return NULL;
  memcpy(out, p, n);
  out[n] = 0;
  return out;
}

static char *hp_user_of(const char *s) {
  const char *at = s ? strchr(s, '@') : NULL;
  if (!at || at == s) return NULL;
  size_t n = (size_t)(at - s);
  char *out = malloc(n + 1);
  if (!out) return NULL;
  memcpy(out, s, n);
  out[n] = 0;
  return out;
}

static char *hp_digits_of(const char *s) {
  if (!s) return NULL;
  char *out = malloc(strlen(s) + 1);
  if (!out) return NULL;
  size_t j = 0;
  for (const char *p = s; *p; p++) if (isdigit((unsigned char)*p)) out[j++] = *p;
  out[j] = 0;
  if (!j) { free(out); return NULL; }
  return out;
}

static char *hp_case_of(const char *s, int up) {
  if (!s) return NULL;
  char *out = strdup(s);
  if (!out) return NULL;
  for (char *p = out; *p; p++)
    *p = up ? (char)toupper((unsigned char)*p) : (char)tolower((unsigned char)*p);
  return out;
}

static char *hp_nospace_of(const char *s) {
  if (!s) return NULL;
  char *out = malloc(strlen(s) + 1);
  if (!out) return NULL;
  size_t j = 0;
  for (const char *p = s; *p; p++) if (!isspace((unsigned char)*p)) out[j++] = *p;
  out[j] = 0;
  return out;
}

static void hp_vars_build(hp_vars *v, const char *entity, const char *key) {
  memset(v, 0, sizeof *v);
  v->raw = hp_dup(entity);
  v->enc = hp_urlenc(entity);
  char *d = hp_digits_of(entity);
  if (d) {
    v->digits = d;
    size_t n = strlen(d);
    if (n <= 10) {
      v->cik10 = malloc(11);
      if (v->cik10) snprintf(v->cik10, 11, "%010llu", strtoull(d, NULL, 10));
    } else {
      v->cik10 = strdup(d);
    }
  }
  char *h = hp_host_of(entity); if (h) { v->host = hp_urlenc(h); free(h); }
  char *u = hp_user_of(entity); if (u) { v->user = hp_urlenc(u); free(u); }
  char *l = hp_case_of(entity, 0); if (l) { v->lower = hp_urlenc(l); free(l); }
  char *U = hp_case_of(entity, 1); if (U) { v->upper = hp_urlenc(U); free(U); }
  char *ns = hp_nospace_of(entity); if (ns) { v->nospace = hp_urlenc(ns); free(ns); }
  if (key) { v->key = hp_urlenc(key); v->keyb64 = hp_b64_userpass(key); }
}

/* {token} expansion. An unknown token is left verbatim so a typo shows up in
 * the logged URL instead of silently vanishing. `extra_name`/`extra_val`
 * inject the second-hop {v}. Returns malloc'd. */
static char *hp_expand(const char *tmpl, const hp_vars *v,
                       const char *extra_name, const char *extra_val) {
  if (!tmpl) return NULL;
  size_t cap = strlen(tmpl) + 512, len = 0;
  char *out = malloc(cap);
  if (!out) return NULL;
  for (const char *p = tmpl; *p; ) {
    const char *sub = NULL;
    size_t skip = 0;
    if (*p == '{') {
      const char *close = strchr(p, '}');
      if (close && close - p < 12) {
        size_t tn = (size_t)(close - p - 1);
        char tok[12] = {0};
        memcpy(tok, p + 1, tn);
        if      (!strcmp(tok, "q"))      sub = v->enc;
        else if (!strcmp(tok, "Q"))      sub = v->raw;
        else if (!strcmp(tok, "qd"))     sub = v->digits;
        else if (!strcmp(tok, "qc"))     sub = v->cik10;
        else if (!strcmp(tok, "qh"))     sub = v->host;
        else if (!strcmp(tok, "qu"))     sub = v->user;
        else if (!strcmp(tok, "ql"))     sub = v->lower;
        else if (!strcmp(tok, "qU"))     sub = v->upper;
        else if (!strcmp(tok, "qn"))     sub = v->nospace;
        else if (!strcmp(tok, "key"))    sub = v->key;
        else if (!strcmp(tok, "keyb64")) sub = v->keyb64;
        else if (extra_name && !strcmp(tok, extra_name)) sub = extra_val;
        if (sub) skip = tn + 2;
      }
    }
    size_t add = sub ? strlen(sub) : 1;
    if (len + add + 1 > cap) {
      cap = (len + add + 1) * 2;
      char *n = realloc(out, cap);
      if (!n) { free(out); return NULL; }
      out = n;
    }
    if (sub) { memcpy(out + len, sub, add); len += add; p += skip; }
    else     { out[len++] = *p++; }
  }
  out[len] = 0;
  return out;
}

/* Does a template need a token we could not derive? (e.g. {qh} on a person's
 * name). Such a row must not fire a request against a half-built URL. */
static int hp_needs_missing(const char *tmpl, const hp_vars *v) {
  static const char *toks[] = { "{qd}", "{qc}", "{qh}", "{qu}", NULL };
  const char *vals[] = { v->digits, v->cik10, v->host, v->user };
  if (!tmpl) return 0;
  for (int i = 0; toks[i]; i++)
    if (strstr(tmpl, toks[i]) && !vals[i]) return 1;
  return 0;
}

/* Does a template reference the pivot entity at all? Every entity token starts
 * "{q" ({q} {qd} {qc} {qh} {qu} {ql} {qU} {qn}) except the raw POST form {Q}.
 * A row whose URL and body name none of them describes a fixed endpoint, and
 * so is meaningful on a scheduled run where there is no entity — see hp_run. */
static int hp_uses_entity(const char *tmpl) {
  if (!tmpl) return 0;
  return strstr(tmpl, "{q") != NULL || strstr(tmpl, "{Q}") != NULL;
}

/* ── entity shape gate ───────────────────────────────────────────────────── */

static int hp_is_hex(const char *s, size_t n) {
  for (size_t i = 0; i < n; i++) if (!isxdigit((unsigned char)s[i])) return 0;
  return 1;
}

static int hp_matches(hp_want w, const char *e) {
  if (!e || !*e) return 0;
  size_t n = strlen(e);
  switch (w) {
    case HP_ANY: return 1;
    case HP_DOMAIN: {
      char *h = hp_host_of(e);
      int ok = h && strchr(h, '.') && !strchr(h, ' ');
      free(h);
      return ok;
    }
    case HP_IP: {
      int dots = 0, dig = 0;
      for (const char *p = e; *p; p++) {
        if (*p == '.') dots++;
        else if (isdigit((unsigned char)*p)) dig++;
        else if (*p == ':' || isxdigit((unsigned char)*p)) continue;   /* v6 */
        else return 0;
      }
      return (dots == 3 && dig >= 4) || strchr(e, ':') != NULL;
    }
    case HP_EMAIL: {
      const char *at = strchr(e, '@');
      return at && at != e && strchr(at, '.') != NULL;
    }
    case HP_NUMERIC: {
      int dig = 0;
      for (const char *p = e; *p; p++) if (isdigit((unsigned char)*p)) dig++;
      return dig >= 4;
    }
    case HP_HASH:
      return (n == 32 || n == 40 || n == 64) && hp_is_hex(e, n);
    case HP_ICAO24:
      return n == 6 && hp_is_hex(e, 6);
    case HP_ETH:
      return n == 42 && e[0] == '0' && (e[1] == 'x' || e[1] == 'X') &&
             hp_is_hex(e + 2, 40);
    case HP_BTC:
      if (n < 26 || n > 62) return 0;
      if (!strncmp(e, "bc1", 3)) return 1;
      if (e[0] != '1' && e[0] != '3') return 0;
      for (const char *p = e; *p; p++) if (!isalnum((unsigned char)*p)) return 0;
      return 1;
    case HP_ASN: {
      const char *p = e;
      if ((p[0] == 'A' || p[0] == 'a') && (p[1] == 'S' || p[1] == 's')) p += 2;
      if (!*p) return 0;
      for (; *p; p++) if (!isdigit((unsigned char)*p)) return 0;
      return 1;
    }
  }
  return 1;
}

/* ── JSON walking ────────────────────────────────────────────────────────── */

/* Follow a dotted path. Numeric segments index arrays. A segment of the form
 * `key=value` selects the first array element whose `key` equals `value`.
 * NULL when absent.
 *
 * The selector form exists because indexing a link array positionally is a
 * silent trap. STAC and other hypermedia APIs return
 *   "links": [ {"rel":"self",...}, {"rel":"next","href":...} ]
 * and the position of `next` is not stable — it is first on some servers,
 * third on others, and nothing stops a server reordering it between releases.
 * A row written as `links.1.href` that one day resolves to the `self` link
 * makes the engine refetch the same page until the page ceiling stops it:
 * every page after the first is lost, and the run looks successful because
 * records keep arriving. `links.rel=next.href` says what is meant and cannot
 * drift. */
static cJSON *hp_path(cJSON *root, const char *path) {
  if (!root || !path || !*path) return root;
  cJSON *cur = root;
  const char *p = path;
  char seg[96];
  while (*p && cur) {
    size_t n = 0;
    while (p[n] && p[n] != '.' && n < sizeof seg - 1) n++;
    memcpy(seg, p, n);
    seg[n] = 0;
    p += n;
    if (*p == '.') p++;
    if (!*seg) continue;

    char *eq = strchr(seg, '=');
    if (eq && cJSON_IsArray(cur)) {
      *eq = 0;
      const char *want = eq + 1;
      cJSON *hit = NULL, *it = NULL;
      cJSON_ArrayForEach(it, cur) {
        cJSON *f = cJSON_GetObjectItem(it, seg);
        if (f && cJSON_IsString(f) && f->valuestring &&
            !strcmp(f->valuestring, want)) { hit = it; break; }
      }
      cur = hit;
      continue;
    }

    int allnum = 1;
    for (char *q = seg; *q; q++) if (!isdigit((unsigned char)*q)) allnum = 0;
    if (allnum && cJSON_IsArray(cur)) cur = cJSON_GetArrayItem(cur, atoi(seg));
    else                              cur = cJSON_GetObjectItem(cur, seg);
  }
  return cur;
}

/* ── legacy Japanese encodings ───────────────────────────────────────────
 *
 * lib/feedlib.c transcodes a non-UTF-8 body from a .jp host, with a documented
 * rationale: customs.go.jp and soumu.go.jp serve Shift_JIS with no charset
 * header, and those bytes were being persisted verbatim into TEXT columns and
 * their FTS mirror. hpengine never used that path, so every hp row on a legacy
 * .jp host stored mojibake — which is why the 2ch-family boards (subject.txt
 * is Shift_JIS on all of them) could not be registered at all.
 *
 * The gate is duplicated rather than shared on purpose: tests/hpengine_test.c
 * links this file WITHOUT feedlib.c, precisely so the engine test does not drag
 * in OpenSSL. lib/csv.c is already on that link line, so csv_is_utf8() and
 * csv_decode_sjis() are reachable and feed_url_host_is_jp() is not.
 *
 * The host test is the whole point. csv_decode_sjis is willing to transcode
 * anything, and Latin-1 is also invalid UTF-8 — the byte pair `FC 72` in
 * "Zürich" is perfectly good Shift_JIS — so a blanket transcode would turn
 * European feeds into kanji. */
static int hp_host_is_jp(const char *url) {
  if (!url) return 0;
  const char *h = strstr(url, "://");
  h = h ? h + 3 : url;
  /* userinfo: "user.jp@evil.com" must resolve to evil.com, not to .jp */
  const char *scan = h, *at = NULL;
  while (*scan && *scan != '/' && *scan != '?' && *scan != '#') {
    if (*scan == '@') at = scan;
    scan++;
  }
  if (at) h = at + 1;
  const char *end = h;
  while (*end && *end != '/' && *end != '?' && *end != '#' && *end != ':') end++;
  size_t n = (size_t)(end - h);
  return n >= 3 && end[-3] == '.' &&
         (end[-2] == 'j' || end[-2] == 'J') &&
         (end[-1] == 'p' || end[-1] == 'P');
}

/* -> a malloc'd UTF-8 copy the caller frees, or NULL meaning "use body as-is".
 * csv_decode_sjis fails closed (it returns a verbatim copy) when the bytes are
 * not decodable, so a host serving something else is safe.
 *
 * Two ways in. A row that DECLARES `charset` is transcoded whatever its host —
 * the encoding belongs to the endpoint, and the 2ch-family boards are Japanese
 * sites on .to and .net, which the host gate rightly refuses. Everything else
 * falls back to the .jp host heuristic, which is what catches the government
 * sites that serve Shift_JIS with no charset header and never declared
 * anything. Either way the body must actually not be valid UTF-8: a correctly
 * encoded response is never touched. */
static char *hp_body_to_utf8(const hp_source *s, const char *url,
                             const char *body, size_t body_len) {
  if (!body || !*body) return NULL;
  /* The caller's length, not strlen(). A UTF-16 body is half NUL bytes, so
   * strlen() would measure it as one character and hand iconv a single glyph;
   * the Statistics Bureau's CPI calendar is UTF-16 and reads as empty that way.
   * An embedded NUL in any other body truncated it just as silently. */
  size_t n = body_len ? body_len : strlen(body);
  if (csv_is_utf8(body, n)) return NULL;
  int declared = s && s->charset && s->charset[0];
  if (!declared && !hp_host_is_jp(url)) return NULL;
  if (declared) {
    /* The sjis family is spelled several ways in practice and all of them mean
     * CP932 here (see below). Anything else is handed to iconv verbatim, so a
     * row can name EUC-JP, or UTF-16 — the Statistics Bureau ships its CPI
     * release calendar as UTF-16, which no amount of Shift_JIS handling would
     * have read. */
    const char *cs = s->charset;
    if (strcasecmp(cs, "sjis") && strcasecmp(cs, "shift_jis") &&
        strcasecmp(cs, "shift-jis") && strcasecmp(cs, "cp932") &&
        strcasecmp(cs, "windows-31j"))
      return csv_decode_charset(body, n, cs);
  }
  /* CP932, not SHIFT_JIS. glibc's "SHIFT_JIS" is strict JIS X 0208 and REJECTS
   * the NEC/IBM extension rows that Japanese web content is full of — ■ ★ ☆ ①
   * and the rest all live at 0x81A1..0x879C. csv_decode_charset fails closed,
   * so a single such byte pair anywhere in the document made the whole
   * transcode return a verbatim copy and the record stored as mojibake. That
   * is exactly what happened to the Machi BBS boards, whose thread titles are
   * decorated with ★ and ■: open2ch transcoded cleanly and machi.to did not,
   * from the same code path. CP932 is a strict superset of Shift_JIS, so this
   * can only turn a failure into a success. */
  return csv_decode_charset(body, n, "CP932");
}

/* ── array-descending path resolution ────────────────────────────────────
 *
 * hp_path() walks each segment with cJSON_GetObjectItem(), which returns NULL
 * on an ARRAY. So a document whose records live under a path that crosses an
 * array is unreachable: the JMA district forecast is a top-level array of two
 * blocks, each holding `timeSeries[]`, each holding `areas[]`, and
 * `array_path=timeSeries.areas` resolves to nothing at all. The only thing
 * hp_path could express was a positional index (`0.timeSeries.0.areas`), which
 * reaches ONE block and silently discards every other — the exact discard
 * house rule 2 forbids. 56 verified forecast offices were dropped rather than
 * registered that way.
 *
 * hp_path_multi walks the same segments, but when the current node is an array
 * and the segment is neither an index nor a `key=value` selector, it MAPS the
 * segment across every element and concatenates the results. Nothing is
 * chosen; everything at that path is taken.
 *
 * The return is a cJSON array of REFERENCES into `root` — the caller must
 * cJSON_Delete() the wrapper, and doing so does not touch the borrowed nodes.
 * hp_path() itself is deliberately left alone: it returns a borrowed pointer
 * that three call sites do not free, and it is the right function for the
 * single-node resolutions (`next_path`, `detail_path`). */
static void hp_seg_step(cJSON *node, char *seg, cJSON *out) {
  if (!node || !out) return;

  char *eq = strchr(seg, '=');
  if (eq && cJSON_IsArray(node)) {              /* key=value selector */
    *eq = 0;
    const char *want = eq + 1;
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, node) {
      cJSON *f = cJSON_GetObjectItem(it, seg);
      if (f && cJSON_IsString(f) && f->valuestring &&
          !strcmp(f->valuestring, want)) { cJSON_AddItemReferenceToArray(out, it); break; }
    }
    *eq = '=';                                  /* seg is reused by the caller */
    return;
  }

  if (cJSON_IsArray(node)) {
    int allnum = *seg ? 1 : 0;
    for (char *q = seg; *q; q++) if (!isdigit((unsigned char)*q)) allnum = 0;
    if (allnum) {                               /* positional, as hp_path */
      cJSON *x = cJSON_GetArrayItem(node, atoi(seg));
      if (x) cJSON_AddItemReferenceToArray(out, x);
    } else {                                    /* THE FIX: map across elements */
      cJSON *it = NULL;
      cJSON_ArrayForEach(it, node) hp_seg_step(it, seg, out);
    }
    return;
  }

  cJSON *hit = cJSON_GetObjectItem(node, seg);
  if (hit) cJSON_AddItemReferenceToArray(out, hit);
}

/* -> a NEW reference array the caller must delete, holding every node the path
 * reaches. A node that is itself an array is SPLICED, so the result is a flat
 * record list rather than a list of lists — which is what array_path means. */
static cJSON *hp_path_multi(cJSON *root, const char *path) {
  cJSON *cur = cJSON_CreateArray();
  if (!cur) return NULL;
  cJSON_AddItemReferenceToArray(cur, root);

  const char *p = path;
  char seg[96];
  while (*p && cur) {
    size_t n = 0;
    while (p[n] && p[n] != '.' && n < sizeof seg - 1) n++;
    memcpy(seg, p, n);
    seg[n] = 0;
    p += n;
    if (*p == '.') p++;
    if (!*seg) continue;

    cJSON *next = cJSON_CreateArray();
    if (!next) { cJSON_Delete(cur); return NULL; }
    cJSON *node = NULL;
    cJSON_ArrayForEach(node, cur) hp_seg_step(node, seg, next);
    cJSON_Delete(cur);
    cur = next;
  }
  if (!cur) return NULL;

  cJSON *flat = cJSON_CreateArray();
  if (!flat) { cJSON_Delete(cur); return NULL; }
  cJSON *it = NULL;
  cJSON_ArrayForEach(it, cur) {
    if (cJSON_IsArray(it)) {
      cJSON *e = NULL;
      cJSON_ArrayForEach(e, it) cJSON_AddItemReferenceToArray(flat, e);
    } else {
      cJSON_AddItemReferenceToArray(flat, it);
    }
  }
  cJSON_Delete(cur);
  return flat;
}

/* Densest array of objects anywhere in the document (depth-limited). Lets a
 * row work without hardcoding a envelope key — and keeps working when the
 * upstream renames it. */
/*
 * It also REPORTS what the guess was made between. A document with one array
 * of objects leaves no choice; a document with several does, and an upstream
 * that adds a `related` block or reorders its envelope can flip the pick
 * without changing a single record count. That is why the walk records the
 * chosen path, the number of candidates and the runner-up: hp_run turns an
 * ambiguous guess into a `collector-shape-notice` (condition
 * `densest-array-fallback`) so the choice is data, not a silent heuristic. */
typedef struct {
  cJSON *best;
  int    best_n;
  char   best_path[128];
  int    cands;             /* arrays of objects seen anywhere in the doc  */
  int    alt_n;             /* size of the runner-up, 0 when there is none */
  char   alt_path[128];
} hp_arrfind;

static void hp_find_array(cJSON *node, int depth, const char *path, hp_arrfind *f) {
  if (!node || depth > 5) return;
  if (cJSON_IsArray(node)) {
    int n = 0, objs = 0;
    cJSON *it;
    cJSON_ArrayForEach(it, node) { n++; if (cJSON_IsObject(it)) objs++; }
    if (objs > 0) {
      f->cands++;
      if (n > f->best_n) {
        if (f->best) {
          f->alt_n = f->best_n;
          snprintf(f->alt_path, sizeof f->alt_path, "%s", f->best_path);
        }
        f->best = node;
        f->best_n = n;
        snprintf(f->best_path, sizeof f->best_path, "%s", path);
      } else if (n > f->alt_n) {
        f->alt_n = n;
        snprintf(f->alt_path, sizeof f->alt_path, "%s", path);
      }
    }
  }
  cJSON *ch;
  int idx = 0;
  cJSON_ArrayForEach(ch, node) {
    if (cJSON_IsObject(ch) || cJSON_IsArray(ch)) {
      char sub[128];
      if (cJSON_IsArray(node)) snprintf(sub, sizeof sub, "%s[%d]", path, idx);
      else if (*path)          snprintf(sub, sizeof sub, "%s.%s", path, ch->string ? ch->string : "?");
      else                     snprintf(sub, sizeof sub, "%s", ch->string ? ch->string : "?");
      hp_find_array(ch, depth + 1, sub, f);
    }
    idx++;
  }
}

/* Flatten every scalar under `node` into `out` with dotted keys.
 *
 * "Every" is the point: a record is emitted whole, not reduced to the handful
 * of fields a row happens to name. The three memory bounds below are set far
 * above any real record, and hitting one is recorded in `*drops`/`*trunc` so the
 * caller can stamp the record instead of shipping a quietly incomplete one. */
static void hp_flatten_c(const cJSON *node, const char *prefix, cJSON *out,
                         int depth, int *drops, int *trunc) {
  if (!node) return;
  if (depth > HP_MAX_DEPTH) { if (drops) (*drops)++; return; }
  if (cJSON_GetArraySize(out) >= HP_MAX_PROPS) { if (drops) (*drops)++; return; }
  char key[256];
  const cJSON *ch;
  int idx = 0;
  cJSON_ArrayForEach(ch, node) {
    if (cJSON_GetArraySize(out) >= HP_MAX_PROPS) { if (drops) (*drops)++; return; }
    if (cJSON_IsArray(node)) {
      if (idx >= HP_MAX_ARR) { if (trunc) (*trunc)++; return; }
      snprintf(key, sizeof key, "%s.%d", prefix, idx++);
    } else if (ch->string) {
      if (prefix && *prefix) snprintf(key, sizeof key, "%s.%s", prefix, ch->string);
      else                   snprintf(key, sizeof key, "%s", ch->string);
    } else {
      continue;
    }
    if (cJSON_IsString(ch) && ch->valuestring && ch->valuestring[0])
      cJSON_AddStringToObject(out, key, ch->valuestring);
    else if (cJSON_IsNumber(ch))
      cJSON_AddNumberToObject(out, key, ch->valuedouble);
    else if (cJSON_IsBool(ch))
      cJSON_AddBoolToObject(out, key, cJSON_IsTrue(ch));
    else if (cJSON_IsObject(ch) || cJSON_IsArray(ch))
      hp_flatten_c(ch, key, out, depth + 1, drops, trunc);
  }
}

/* Per-record accounting for the flatten bounds, so a truncated record is
 * always labelled as one. Reset by hp_flatten() at each record.
 *
 * _Thread_local, because "per-record" is only true per THREAD: pipeline.c
 * dispatches up to 16 collectors concurrently and scheduler.c runs 8 more, so
 * as plain globals two workers interleaved one's reset with the other's
 * increments. The visible consequence was the wrong one to have — a record
 * that WAS truncated could ship stamped 0, i.e. presented as complete. That
 * stamp is exactly what the exhaustive-use rule relies on. (lib/jsonlist.c:207
 * already uses _Thread_local for its scratch buffer, same reason.) */
static _Thread_local int g_flat_drops = 0, g_flat_trunc = 0;

static void hp_flatten(const cJSON *node, const char *prefix, cJSON *out, int depth) {
  if (depth == 0) { g_flat_drops = 0; g_flat_trunc = 0; }
  hp_flatten_c(node, prefix, out, depth, &g_flat_drops, &g_flat_trunc);
}

/* Value for `name` in a flattened map: exact dotted key first, then any key
 * whose last segment matches (so "siege.nom" answers a "nom" request).
 *
 * An XML attribute is flattened as `@id` / `Codelist.@agencyID` (see
 * hp_xml_attrs()), and the `@` is stripped before the last-segment compare so
 * that a row declaring `id_keys=id` finds the identity whether the upstream
 * spells it as an attribute or as a child element — which is the whole point
 * of putting attributes in the same keyspace. Writing `id_keys=@id` still
 * selects the attribute specifically, via the exact-key hit above. Exact
 * matches are tried first, so a record carrying BOTH `@id` and `id` resolves
 * `id` to the element and `@id` to the attribute, never one for the other. */
static const cJSON *hp_flat_get(const cJSON *flat, const char *name) {
  if (!flat || !name || !*name) return NULL;
  const cJSON *ex = cJSON_GetObjectItem(flat, name);
  if (ex) return ex;
  const cJSON *it;
  cJSON_ArrayForEach(it, flat) {
    if (!it->string) continue;
    const char *dot = strrchr(it->string, '.');
    const char *last = dot ? dot + 1 : it->string;
    if (*last == '@' && name[0] != '@') last++;
    if (!strcasecmp(last, name)) return it;
  }
  return NULL;
}

/* Render a scalar as text for keying purposes. Numbers are identifiers just as
 * often as strings are -- SEC's `cik`, RIPEstat's `number`, BrandMeister's `id`
 * are all JSON numbers -- and testing cJSON_IsString alone made the engine
 * blind to them. It reported "emitted 0 of 2677" for a source whose every
 * record carried a perfectly good primary key.
 *
 * The text goes in a caller-owned buffer because cJSON holds no string form of
 * a number. Returns NULL for anything with no scalar value (objects, arrays,
 * null, empty strings). */
static const char *hp_scalar_str(const cJSON *v, char *buf, size_t cap) {
  if (!v) return NULL;
  if (cJSON_IsString(v)) return v->valuestring[0] ? v->valuestring : NULL;
  if (cJSON_IsNumber(v)) {
    double d = v->valuedouble;
    if (d == (double)(long long)d) snprintf(buf, cap, "%lld", (long long)d);
    else                           snprintf(buf, cap, "%.10g", d);
    return buf[0] ? buf : NULL;
  }
  if (cJSON_IsBool(v)) { snprintf(buf, cap, "%s", cJSON_IsTrue(v) ? "true" : "false"); return buf; }
  return NULL;
}

/* The record's first non-empty scalar, in document order — the last-resort
 * identifier for a record that carries real content under names no fallback
 * list knows. Skips the engine's own `_`-prefixed annotations, which are
 * metadata about the record rather than anything the upstream said. */
static const char *hp_first_scalar(const cJSON *flat, char *buf, size_t cap) {
  const cJSON *it;
  cJSON_ArrayForEach(it, flat) {
    if (it->string && it->string[0] == '_') continue;
    const char *r = hp_scalar_str(it, buf, cap);
    if (r) return r;
  }
  return NULL;
}

/* First candidate from a comma-separated list that resolves to a non-empty
 * scalar in the flattened record. `scratch` receives the text of a numeric or
 * boolean hit and must outlive the returned pointer. */
static const char *hp_pick_s(const cJSON *flat, const char *csv_keys,
                             const char *const *fallback,
                             char *scratch, size_t cap) {
  char buf[512];
  if (csv_keys && *csv_keys) {
    snprintf(buf, sizeof buf, "%s", csv_keys);
    /* strtok_r: hp_pick runs for every record on every concurrent dispatch
     * worker, and strtok's cursor is a process-global (see jsonlist.c:246). */
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
      while (*tok == ' ') tok++;
      const char *r = hp_scalar_str(hp_flat_get(flat, tok), scratch, cap);
      if (r) return r;
    }
  }
  for (int i = 0; fallback && fallback[i]; i++) {
    const char *r = hp_scalar_str(hp_flat_get(flat, fallback[i]), scratch, cap);
    if (r) return r;
  }
  return NULL;
}

static const char *TITLE_FALLBACK[] = {
  "name", "title", "legalName", "legal_name", "companyName", "company_name",
  "fullName", "full_name", "display_name", "displayName", "label", "caption",
  "nom", "nom_complet", "navn", "nome", "nombre", "denomination", "subject",
  "headline", "description", "summary", NULL
};
static const char *ID_FALLBACK[] = {
  "id", "uid", "uuid", "number", "company_number", "companyNumber", "lei",
  "cik", "siren", "registrationNumber", "registration_number", "organisasjonsnummer",
  "businessId", "doc_id", "docId", "accession", "identifier", "key", "code", NULL
};
static const char *LINK_FALLBACK[] = {
  "url", "link", "html_url", "web_url", "permalink", "landingPage", "uri", NULL
};
static const char *DATE_FALLBACK[] = {
  "date", "published", "published_at", "publishedAt", "created", "created_at",
  "createdAt", "updated_at", "updatedAt", "last_modified", "filing_date",
  "filingDate", "registration_date", "registrationDate", "date_creation",
  "timestamp", NULL
};

/* ── emission ────────────────────────────────────────────────────────────── */

typedef struct {
  const hp_source *s;
  const source_ctx *ctx;
  intel_sink *sink;
  const hp_vars *vars;
  const char *url;
  int emitted;
  /* Exhaustive-use accounting, stamped onto every record so a consumer can
   * always tell a complete result from a bounded one. */
  int   available;        /* array slots the upstream handed over            */
  /* Slots that held no content at all — a trailing blank line in a CSV, a null
   * or a bare scalar in a JSON array. They are counted separately because they
   * are not records, and folding them into `available` made the run report a
   * shortfall that never happened: IAEA_NDS_LEVELS said "emitted 197 of 198"
   * forever, the 198th being the newline at the end of the file. 87 rows of
   * batch 19 reported exactly that phantom -1. A disclosure that cries wolf on
   * every trailing newline is one nobody will read when a real discard happens. */
  int   empty;
  int   refused;          /* the sink declined it — a discard with a cause    */
  int   filtered;         /* filter_query excluded it — the row asked for that */
  int   duplicate;        /* the same href twice on one page — one record     */
  int   truncated;        /* a declared cap or a cancel stopped the walk     */
  int   page;             /* 1-based page currently being read               */
  int   page_records;     /* records in the page just read (paging stop test) */
  /* Second-hop budget for the WHOLE run, not per page. It used to be a local
   * of hp_run_json, so it was re-initialised on every page: a row with
   * page_max 10 could fire 10 x JO_HP_DETAIL_MAX (250) detail requests, which
   * is exactly the runaway hpengine.h promises the budget prevents. */
  int   deep_left;
  char *next_url;         /* next-page URL from the response, when declared  */
  /* HTML mode: the href dedupe set spans the WHOLE walk, not one page. Per
   * page it could not tell "page 2 is new content" from "the site ignored our
   * page param and re-served page 1", so the walk had no honest stop signal. */
  html_seen hseen;
  /* uid collision guard — see hp_collision_map(). `dup_map` is one byte per
   * element of the array currently being emitted, non-zero where that record's
   * fallback key is shared with a sibling; `rec_idx` is the element being
   * emitted right now. NULL/0 everywhere else, which is the old behaviour. */
  const unsigned char *dup_map;
  int   rec_idx;
  /* The response was HTTP 200 but its BODY said the request failed — see
   * hp_json_error_doc(). Set by the mode driver, read by hp_run so the walk
   * stops and the run reports the same thing an HTTP failure reports.
   * `err_code` is the code the document carried, when it carried a numeric
   * one, so the transport rule (>=500 is a hard error, everything else is an
   * honest empty) can be applied to it unchanged. */
  int   upstream_error;
  long  err_code;

  /* ── schema-drift tally, disclosed by hp_shape_notices() ────────────────
   * Each of these is a run that "succeeded" while the upstream's shape had
   * stopped matching the row's declaration. They are counted here and emitted
   * ONCE per run per condition as a `collector-shape-notice` record — house
   * rule 2 says a shortfall is data, not a log line — and never when the
   * condition did not occur. */
  int   sh_path_missing;    /* pages where the declared array_path resolved to nothing */
  char  sh_path_kind[96];   /* what it resolved to instead ("absent", "string", …) */
  int   sh_fb_pages;        /* pages where the densest-array guess had ≥2 candidates */
  hp_arrfind sh_fb;         /* the last such guess, for the notice's numbers    */
  int   pg_n, pg_title, pg_id;   /* THIS page: records seen / declared key hits */
  int   sh_title_pages, sh_title_n;  /* pages where title_keys matched 0 of N   */
  int   sh_id_pages,    sh_id_n;     /* same for id_keys                        */
  int   sh_repeat_page;     /* 1-based page whose bytes equalled the previous page's */
  int   sh_repeat_skipped;  /* pages the walk would still have requested       */
} hp_run_state;

/* 0 = no cap (every record). A row's non-zero max_items is its author's
 * explicit choice; the engine never invents one. */
static int hp_record_cap(const hp_source *s) {
  return s->max_items > 0 ? s->max_items : 0;
}

/* How many second hops this run may make: the row's declared detail_max, else
 * every record up to the operational ceiling $JO_HP_DETAIL_MAX. */
static int hp_detail_budget(const hp_source *s) {
  if (!s->detail_url) return 0;
  if (s->detail_max > 0) return s->detail_max;
  const char *e = getenv("JO_HP_DETAIL_MAX");
  int v = (e && *e) ? atoi(e) : 0;
  return v > 0 ? v : HP_DETAIL_MAX_DEF;
}

static double hp_num(const cJSON *flat, const char *key) {
  if (!key || !*key) return 0;
  const cJSON *v = hp_flat_get(flat, key);
  if (!v) return 0;
  if (cJSON_IsNumber(v)) return v->valuedouble;
  if (cJSON_IsString(v)) return atof(v->valuestring);
  return 0;
}

/* Case-insensitive substring, for filter_query. */
static int hp_icontains(const char *hay, const char *needle) {
  if (!hay || !needle || !*needle) return 1;
  size_t nl = strlen(needle);
  for (const char *p = hay; *p; p++)
    if (!strncasecmp(p, needle, nl)) return 1;
  return 0;
}

/* Second hop: pull the full record behind a list hit and merge it in under
 * "detail.". Never silent: a missing id, a failed fetch or an un-reached record
 * is stamped on the record (`_detail_error`, `_detail_pending`) so an absent
 * detail block is distinguishable from a detail block that came back empty. */
static void hp_deepen(hp_run_state *st, cJSON *flat) {
  const hp_source *s = st->s;
  if (!s->detail_url || !s->detail_key) return;
  const cJSON *idv = hp_flat_get(flat, s->detail_key);
  if (!idv) {
    cJSON_AddStringToObject(flat, "_detail_error", "list record carries no detail key");
    return;
  }
  char idbuf[256];
  if (cJSON_IsString(idv)) snprintf(idbuf, sizeof idbuf, "%s", idv->valuestring);
  else if (cJSON_IsNumber(idv)) snprintf(idbuf, sizeof idbuf, "%.0f", idv->valuedouble);
  else return;
  if (!idbuf[0]) return;

  char *enc = hp_urlenc(idbuf);
  char *url = hp_expand(s->detail_url, st->vars, "v", enc ? enc : idbuf);
  free(enc);
  if (!url) return;

  http_response hr = {0};
  int rc = http_request(st->ctx->http, "GET", url, NULL, NULL, 0,
                        s->timeout_ms > 0 ? s->timeout_ms : HP_HTTP_TIMEOUT,
                        0, &hr);
  if (rc == 0 && hr.status == 200 && hr.body) {
    /* Same gate as the page fetch: a detail endpoint on a .jp host serving
     * Shift_JIS would otherwise merge mojibake into the record it enriches. */
    char *dutf8 = hp_body_to_utf8(s, url, hr.body, hr.body_len);
    cJSON *doc = cJSON_Parse(dutf8 ? dutf8 : hr.body);
    free(dutf8);
    if (doc) {
      cJSON *node = s->detail_path ? hp_path(doc, s->detail_path) : doc;
      if (node) hp_flatten(node, "detail", flat, 1);
      cJSON_Delete(doc);
    }
    cJSON_AddStringToObject(flat, "detail_url", url);
  } else {
    fprintf(stderr, "[hp:%s] detail status=%ld %s\n", s->id, hr.status, url);
    cJSON_AddStringToObject(flat, "_detail_error",
                            rc != 0 ? "detail fetch transport failure"
                                    : "detail fetch returned non-200");
    cJSON_AddNumberToObject(flat, "_detail_status", (double)hr.status);
  }
  http_response_free(&hr);
  free(url);
}

/* One flattened record → one intel_item. Takes ownership of nothing. */
/* FNV-1a 64, as lowercase hex.
 *
 * Deliberately not feed_hash_key(): tests/hpengine_test.c links this file
 * without lib/feedlib.c (and so without OpenSSL), and adding that dependency to
 * make an offline engine test build is a bad trade. Nothing here needs a
 * cryptographic digest — the hash only has to distinguish records inside one
 * page from each other. */
static void hp_fnv_hex(const char *s, char out[17]) {
  unsigned long long h = 1469598103934665603ULL;
  for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
    h ^= (unsigned long long)*p;
    h *= 1099511628211ULL;
  }
  snprintf(out, 17, "%016llx", h);
}

/* ── the uid collision guard ──────────────────────────────────────────────
 *
 * `records=N` in a run line counts emit() CALLS. It says nothing about how many
 * rows were STORED, because the sink upserts on remote_key — so a source whose
 * records key onto each other reports a healthy N and stores one row. That is
 * the same invisible loss as an emit-zero source, and the metric normally used
 * to find those cannot see it.
 *
 * Below, a record with no `rkey` is keyed on its TITLE, so every record sharing
 * a title collapses onto one row. Measured over a 1,197-source sweep: 46 rows
 * losing 114,795 records per pass. ECDC_RESPIRATORY emits 12,648 and stores 31;
 * four rows (three WHO_XMART_… and NY_AUTHORITY_PROCUREMENT) each emit 10,001
 * and store between 2 and 9. lib/jsonlist.c had the identical defect and this
 * mirrors the guard written there.
 *
 * The map is built by deriving every record's fallback key FIRST and flagging
 * only the ones about to collide. That precision is the point: a record whose
 * key was already unique keeps the remote_key it has, so this re-emits nothing
 * that was being stored correctly. The colliding groups do change key — but
 * n-1 of every such group was being overwritten and never stored, so the only
 * residue is one stale row per group under the old shared key.
 *
 * Records that DO carry an rkey are covered too, and the reason is worth
 * stating because the opposite looked right at first. "Two records share an id,
 * so the upstream says they are the same record" only holds when the id is the
 * upstream's. Here it is usually OURS: `id_keys` is declared in the manifest,
 * and a wrong declaration is a very ordinary mistake. ECDC_RESPIRATORY declares
 * `id_keys=country_code` on a weekly time series — a country code is a
 * DIMENSION, not a record identity — so 12,648 observations keyed onto 438
 * rows. Restricting the guard to title-keyed records left that untouched.
 *
 * What keeps this honest is that disambiguation is by CONTENT hash. Two records
 * sharing a key and byte-identical stay one row, which is real deduplication.
 * Two sharing a key while differing are two records behind a bad identity
 * declaration, and both are kept. So the engine never fabricates a distinction
 * the data does not contain, and never merges records that differ.
 *
 * Scope is one array — one page of one response. A record on page 2 that keys
 * onto one from page 1 is not caught, because page 1 is already emitted by
 * then; catching it would mean buffering the whole walk. Stated plainly rather
 * than papered over.
 *
 * ONE implementation for all three record paths. The guard was first written
 * into hp_run_json only, and CSV and XML went on collapsing: JPCERT's monthly
 * phishing-URL files list a re-confirmed URL as a legitimate second row, so
 * 202401 emitted 5,772 and stored 5,646 — 1–4 % of every file, silently. That
 * is the "other copies of the bug" trap CLAUDE.md §4b warns about, and the
 * cure is to have no copies: each path hands this function the array it is
 * about to emit plus the SAME flatten step it will use for the real emit
 * (`flat_fn`; NULL means the elements are already flat objects), and the key
 * derivation lives here once.
 *
 * The pre-pass flattens each record a second time. hp_flatten() resets its
 * depth-0 accounting globals on every call, so the real pass is unaffected. */
typedef struct { char key[17]; int idx; } hp_keyed_row;

static int hp_keyed_cmp(const void *a, const void *b) {
  return strcmp(((const hp_keyed_row *)a)->key, ((const hp_keyed_row *)b)->key);
}

/* Per-mode flatten steps. Each is used BOTH by the collision pre-pass and by
 * the real emit loop of its path, so the key the guard derives is the key the
 * emitter derives — the pre-pass cannot drift from the pass it guards. */
typedef cJSON *(*hp_flat_fn)(const hp_source *s, cJSON *rec);

static cJSON *hp_json_flat(const hp_source *s, cJSON *rec) {
  (void)s;
  cJSON *flat = cJSON_CreateObject();
  if (!flat) return NULL;
  if (cJSON_IsObject(rec) || cJSON_IsArray(rec)) hp_flatten(rec, "", flat, 0);
  else if (cJSON_IsString(rec) && rec->valuestring[0])
    cJSON_AddStringToObject(flat, "value", rec->valuestring);
  return flat;
}

static cJSON *hp_csv_flat(const hp_source *s, cJSON *row) {
  cJSON *flat = cJSON_CreateObject();
  if (!flat) return NULL;
  if (s->csv_no_header && cJSON_IsArray(row)) {
    /* Headerless: name the columns positionally, col0..colN. */
    int i = 0;
    cJSON *cell;
    char key[16];
    cJSON_ArrayForEach(cell, row) {
      snprintf(key, sizeof key, "col%d", i++);
      if (cJSON_IsString(cell) && cell->valuestring[0])
        cJSON_AddStringToObject(flat, key, cell->valuestring);
    }
  } else {
    hp_flatten(row, "", flat, 0);
  }
  return flat;
}

static unsigned char *hp_collision_map(hp_run_state *st, cJSON *arr, int n,
                                       hp_flat_fn flat_fn) {
  if (n < 2) return NULL;
  const hp_source *s = st->s;
  hp_keyed_row *k = malloc((size_t)n * sizeof *k);
  unsigned char *dup = calloc((size_t)n, 1);
  /* Out of memory must degrade to the old behaviour, not abort the run: a
   * colliding row is worse than no guard, but losing the SOURCE is worse than
   * both. */
  if (!k || !dup) { free(k); free(dup); return NULL; }

  int m = 0, i = 0;
  cJSON *rec;
  cJSON_ArrayForEach(rec, arr) {
    cJSON *flat = flat_fn ? flat_fn(s, rec) : rec;
    if (!flat) { i++; continue; }
    char sc_t[64], sc_r[64], lastbuf[64];
    const char *title = hp_pick_s(flat, s->title_keys, TITLE_FALLBACK, sc_t, sizeof sc_t);
    const char *rkey  = hp_pick_s(flat, s->id_keys,    ID_FALLBACK,    sc_r, sizeof sc_r);
    /* THIS FALLBACK MUST MIRROR hp_emit_record() EXACTLY.
     *
     * The emitter, a few dozen lines below, does `if (!title) { if (!rkey)
     * rkey = hp_first_scalar(...); }` — so a record with NEITHER a title nor an
     * id is keyed on its first non-empty scalar. This map originally computed
     * `rkey ? rkey : title` and SKIPPED the record when both were NULL, which
     * made that one shape — a row declaring neither `id_keys` nor `title_keys`
     * — the single case the guard could not see. Every such record whose first
     * scalar happened to be a dimension constant then collapsed onto one uid,
     * unguarded.
     *
     * Measured over the full registry: 1,818 registered hp rows are in that
     * shape. WHO_XMART_NCD_MORTALITY emitted 10,001 and stored 2, both keyed on
     * an indicator code, with no content-hash suffix because the map never
     * flagged them.
     *
     * A collision guard that derives its key differently from the code it is
     * guarding is not a guard. If the emitter's key derivation ever changes,
     * this must change with it. */
    if (!title && !rkey) rkey = hp_first_scalar(flat, lastbuf, sizeof lastbuf);
    const char *kp = rkey ? rkey : title;
    if (kp) { hp_fnv_hex(kp, k[m].key); k[m].idx = i; m++; }
    if (flat_fn) cJSON_Delete(flat);
    i++;
  }

  qsort(k, (size_t)m, sizeof *k, hp_keyed_cmp);
  int flagged = 0;
  for (int a = 0; a < m; ) {
    int b = a + 1;
    while (b < m && !strcmp(k[a].key, k[b].key)) b++;
    if (b - a > 1) for (int j = a; j < b; j++) { dup[k[j].idx] = 1; flagged++; }
    a = b;
  }
  free(k);
  if (!flagged) { free(dup); return NULL; }
  return dup;
}

static void hp_emit_record(hp_run_state *st, cJSON *flat, int deepen) {
  const hp_source *s = st->s;
  /* Nothing survived flattening — an array slot that held no value at all. The
   * trailing newline of a CSV is the common case: every field maps to "" and
   * hp_flatten keeps none of them. Counted, not silently returned, because the
   * caller has already added this slot to `available` and the difference read
   * as a one-record shortfall that never happened. */
  if (cJSON_GetArraySize(flat) == 0) { st->empty++; return; }

  if (s->filter_query && st->vars->raw) {
    char *txt = cJSON_PrintUnformatted(flat);
    int hit = txt ? hp_icontains(txt, st->vars->raw) : 0;
    free(txt);
    /* A filter miss is the row doing exactly what it asked for, so it is not a
     * shortfall either — but it is still not a record we kept, and lumping it
     * in with `available` overstated what the endpoint offered for this query. */
    if (!hit) { st->filtered++; return; }
  }
  if (deepen) hp_deepen(st, flat);
  else if (s->detail_url)
    /* The row has a second hop but this record was past the per-run detail
     * budget. Say so — an un-fetched detail is not an absent detail. */
    cJSON_AddBoolToObject(flat, "_detail_pending", 1);

  /* Flatten bounds are memory guards, not filters: if one bit, the record is
   * labelled incomplete rather than shipped as if it were whole. */
  if (g_flat_drops)  cJSON_AddNumberToObject(flat, "_fields_dropped", g_flat_drops);
  if (g_flat_trunc)  cJSON_AddNumberToObject(flat, "_array_truncated", g_flat_trunc);

  /* One scratch buffer per pick: the five results coexist, so they cannot
   * share one. Only a numeric or boolean hit uses its buffer at all. */
  char sc_t[64], sc_r[64], sc_d[64], sc_b[64], sc_l[64];
  /* The DECLARED keys are tried on their own first, so a page on which they
   * matched nothing is visible as such (shape notice `title-keys-unmatched` /
   * `id-keys-unmatched`). The fallback lists then run exactly as before, so a
   * record resolves to the same title and key it always did. */
  const char *title = hp_pick_s(flat, s->title_keys, NULL, sc_t, sizeof sc_t);
  const char *rkey  = hp_pick_s(flat, s->id_keys,    NULL, sc_r, sizeof sc_r);
  st->pg_n++;
  if (title) st->pg_title++;
  if (rkey)  st->pg_id++;
  if (!title) title = hp_pick_s(flat, NULL, TITLE_FALLBACK, sc_t, sizeof sc_t);
  if (!rkey)  rkey  = hp_pick_s(flat, NULL, ID_FALLBACK,    sc_r, sizeof sc_r);
  const char *date  = hp_pick_s(flat, s->date_keys,  DATE_FALLBACK,  sc_d, sizeof sc_d);
  const char *body  = hp_pick_s(flat, s->body_keys,  NULL,           sc_b, sizeof sc_b);
  const char *lnk   = hp_pick_s(flat, s->link_keys,  LINK_FALLBACK,  sc_l, sizeof sc_l);

  char linkbuf[1024] = {0};
  if (s->link_tmpl && lnk) {
    char *enc = hp_urlenc(lnk);
    char *l = hp_expand(s->link_tmpl, st->vars, "v", enc ? enc : lnk);
    free(enc);
    if (l) { snprintf(linkbuf, sizeof linkbuf, "%s", l); free(l); }
  } else if (s->link_tmpl) {
    char *l = hp_expand(s->link_tmpl, st->vars, "v", "");
    if (l) { snprintf(linkbuf, sizeof linkbuf, "%s", l); free(l); }
  } else if (lnk && !strncmp(lnk, "http", 4)) {
    snprintf(linkbuf, sizeof linkbuf, "%s", lnk);
  }

  /* A record with no content at all is shape noise, not a finding — but "no
   * CONVENTIONALLY NAMED field" is not the same as "no content", and treating
   * the two as one was silently destroying whole sources.
   *
   * The fallback lists know `name`/`title`/`id`. They do not know DataPlane's
   * headerless columns (parsed as col0..colN, so NOTHING matches and every
   * headerless CSV in the tree emitted zero, forever), CelesTrak's OBJECT_NAME,
   * RIPEstat's `prefix`, or USGS's SiteName. Measured on batch 18: 59 of 223
   * rows fetched records and stored none — 36,166 records discarded per run by
   * DATAPLANE_TELNET alone, with the run reporting success.
   *
   * So the test is now "did the upstream give us anything real", and a record
   * that did is keyed on its first non-empty scalar. That is a worse label than
   * a declared title_keys — which is why the high-volume rows also declare one
   * — but it is not a reason to throw the record away. */
  char titlebuf[512], lastbuf[64];
  if (!title) {
    if (!rkey) rkey = hp_first_scalar(flat, lastbuf, sizeof lastbuf);
    if (!rkey) { st->empty++; return; }
    snprintf(titlebuf, sizeof titlebuf, "%s %s", s->record_type ? s->record_type : "record", rkey);
    title = titlebuf;
  }

  /* The remote_key is the sink's upsert key, so anything that makes two
   * different records produce the same string silently discards one of them.
   * Two things did:
   *
   *   - `%.120s` TRUNCATES. Two ids sharing a 120-character prefix — long
   *     URI-shaped identifiers, an SDMX key, a path — collapsed onto one row.
   *     A key that is too long is now hashed instead of cut, so it stays a
   *     function of the whole identifier.
   *   - records sharing a key COLLAPSED onto one row, whether that key came
   *     from a title (no rkey) or from an `id_keys` declaration that turned out
   *     not to be a record identity. hp_collision_map() above marks exactly the
   *     ones that collide on this page, and only the marked ones get a content
   *     suffix — see the note there on why the precision, and the choice of a
   *     content hash, matter. */
  char keybuf[320];
  const char *kpart = rkey ? rkey : title;
  char khash[17];
  if (strlen(kpart) > 120) { hp_fnv_hex(kpart, khash); kpart = khash; }

  if (st->dup_map && st->dup_map[st->rec_idx]) {
    char *js = cJSON_PrintUnformatted(flat);
    char chash[17];
    hp_fnv_hex(js ? js : title, chash);
    free(js);
    snprintf(keybuf, sizeof keybuf, "%.180s|%.120s|%s", s->id, kpart, chash);
  } else {
    snprintf(keybuf, sizeof keybuf, "%.180s|%.120s", s->id, kpart);
  }

  cJSON_AddStringToObject(flat, "service", s->name ? s->name : s->id);
  cJSON_AddStringToObject(flat, "source_id", s->id);
  if (st->vars->raw) cJSON_AddStringToObject(flat, "query", st->vars->raw);
  cJSON_AddStringToObject(flat, "endpoint", st->url ? st->url : "");
  cJSON_AddBoolToObject(flat, "real_fetch", 1);
  if (st->page > 1) cJSON_AddNumberToObject(flat, "_page", st->page);

  char *props = cJSON_PrintUnformatted(flat);
  char tags[256];
  snprintf(tags, sizeof tags, "[\"osint-search\",\"high-penetrancy\"%s%s]",
           s->tags ? "," : "", s->tags ? s->tags : "");

  double lat = hp_num(flat, s->lat_key), lon = hp_num(flat, s->lon_key);

  intel_item it = {0};
  it.remote_key      = keybuf;
  it.title           = title;
  it.body            = body;
  it.summary         = body;
  it.link            = linkbuf[0] ? linkbuf : (s->portal ? s->portal : NULL);
  it.lang            = "en";
  it.published_at    = date;
  it.record_type     = s->record_type ? s->record_type : "record";
  it.properties_json = props ? props : "{}";
  it.tags_json       = tags;
  if (s->lat_key && s->lon_key && (lat != 0.0 || lon != 0.0)) {
    it.has_geo = 1; it.lat = lat; it.lon = lon;
  }
  /* A record the SINK refused is a discard too, and it was invisible: the run
   * line just showed a smaller `emitted` and looked like a shortfall with no
   * cause. Counted so the two reasons a record does not land — no content, and
   * the store declined it — can be told apart. */
  if (st->sink->emit(st->sink, &it) >= 0) st->emitted++;
  else                                    st->refused++;
  free(props);
}

/* ── per-mode drivers ────────────────────────────────────────────────────── */

/* Is this 200-OK document an ERROR REPORT rather than a payload?
 *
 * Batch 20, live: ArcGIS answers an over-quota query with HTTP 200 and a body
 * of {"error":{"code":429,"message":"..."}}. Nothing in the engine looked at
 * that. `array_path` ("features") did not resolve, hp_find_array() found no
 * array of objects — `details` is an array of STRINGS — and the "root IS the
 * record" fallback then flattened the error envelope into a record. `code`
 * matched ID_FALLBACK through hp_flat_get()'s last-segment rule, so the engine
 * filed a finding titled `airway-record 429`: a number lifted out of an error
 * message, stored and served as if the upstream had reported it as data. That
 * is fabricated content, which house rule 1 forbids outright.
 *
 * tools/probe_hp_batch.py already rejects this shape ("HTTP-200 refusals"), so
 * the batch verifier caught it and the ENGINE did not — the gap this closes.
 *
 * What counts as an error report is kept deliberately narrow, because a false
 * positive here silently deletes a real source:
 *
 *   - only a root-level `error` / `errors` / `fault` member is considered;
 *   - only when it holds an OBJECT, a non-empty ARRAY or a non-empty STRING.
 *     `"error": null`, `"error": 0` and `"error": false` are how a great many
 *     APIs spell SUCCESS, and an envelope that carries records alongside one
 *     of those must keep working exactly as before;
 *   - and the caller only asks once no records array has been found at all, so
 *     a document that did hand over records is never reclassified.
 *
 * Returns 1 and fills `msg`/`code` when the document is an error report.
 * `code` is left at -1 when the document carried no numeric code. */
static int hp_json_error_doc(cJSON *doc, char *msg, size_t cap, long *code) {
  static const char *ERR_KEYS[] = { "error", "errors", "fault", NULL };
  if (!cJSON_IsObject(doc)) return 0;
  cJSON *node = NULL;
  for (int i = 0; ERR_KEYS[i] && !node; i++) {
    cJSON *e = cJSON_GetObjectItem(doc, ERR_KEYS[i]);
    if (!e) continue;
    if (cJSON_IsObject(e) && cJSON_GetArraySize(e) > 0)                node = e;
    /* Element 0 of an `errors` array only. This reads as a first-only discard
     * to the exhaustiveness scanner and is not one: nothing from this document
     * is stored under ANY branch of this function — the whole point of it is
     * that the response carried no records — so element 0 is a representative
     * message for the log line and a place to look for a status code, not a
     * record we are choosing over its siblings. Walking the rest would add log
     * noise and change nothing about what is kept, which is nothing. */
    else if (cJSON_IsArray(e)  && cJSON_GetArraySize(e) > 0)           node = cJSON_GetArrayItem(e, 0);   /* exhaustive-ok: error report, not records — nothing here is ever stored */
    else if (cJSON_IsString(e) && e->valuestring && e->valuestring[0]) node = e;
  }
  if (!node) return 0;

  *code = -1;
  msg[0] = 0;
  if (cJSON_IsString(node)) {
    snprintf(msg, cap, "%.200s", node->valuestring);
  } else if (cJSON_IsObject(node)) {
    static const char *MSG_KEYS[] = { "message", "msg", "description", "detail",
                                      "details", "reason", "title", NULL };
    for (int i = 0; MSG_KEYS[i] && !msg[0]; i++) {
      cJSON *m = cJSON_GetObjectItem(node, MSG_KEYS[i]);
      if (m && cJSON_IsString(m) && m->valuestring[0])
        snprintf(msg, cap, "%.200s", m->valuestring);
    }
    static const char *CODE_KEYS[] = { "code", "status", "statusCode", NULL };
    for (int i = 0; CODE_KEYS[i] && *code < 0; i++) {
      cJSON *v = cJSON_GetObjectItem(node, CODE_KEYS[i]);
      if (v && cJSON_IsNumber(v))                    *code = (long)v->valuedouble;
      else if (v && cJSON_IsString(v) && isdigit((unsigned char)v->valuestring[0]))
        *code = strtol(v->valuestring, NULL, 10);
    }
  }
  if (!msg[0]) snprintf(msg, cap, "(no message)");
  return 1;
}

static int hp_run_json(hp_run_state *st, const char *body) {
  const hp_source *s = st->s;
  cJSON *doc = cJSON_Parse(body);
  if (!doc) { fprintf(stderr, "[hp:%s] non-JSON body\n", s->id); return 0; }

  cJSON *arr = NULL;
  /* Set when `arr` is a hp_path_multi() reference wrapper rather than a node
   * borrowed from `doc`. It must be deleted on EVERY exit below — deleting it
   * frees the wrapper only, never the records it points at. */
  cJSON *arr_owned = NULL;
  if (s->array_path && *s->array_path) {
    cJSON *n = hp_path(doc, s->array_path);
    if (!n) {
      /* hp_path cannot cross an array. Retry with the descending walk, which
       * takes EVERY node at this path rather than one. Only reached when the
       * plain walk already failed, so no row that resolves today changes. */
      cJSON *multi = hp_path_multi(doc, s->array_path);
      if (multi && cJSON_GetArraySize(multi) > 0) { arr = arr_owned = multi; }
      else cJSON_Delete(multi);
    }
    if (!arr && n && cJSON_IsArray(n)) arr = n;
    else if (!arr && n && cJSON_IsObject(n)) {         /* single record */
      /* Count it, exactly as the root-record path below does: an endpoint that
       * answers with one object at array_path was reporting "emitted 1 of 0
       * available", which reads as a discard when nothing was discarded. */
      st->available += 1;
      cJSON *flat = cJSON_CreateObject();
      hp_flatten(n, "", flat, 0);
      hp_emit_record(st, flat, st->deep_left > 0);
      cJSON_Delete(flat);
      cJSON_Delete(doc);
      return st->emitted;
    }
  }
  /* A DECLARED array_path that did not resolve means this response is not the
   * shape the row expects, so neither of the two guesses below is safe: the
   * densest-array heuristic would mine whatever other array the document
   * happens to contain, and the root-record fallback would file the envelope
   * itself as a finding (the ArcGIS 429 above). Emit nothing and say why.
   *
   * The row that declares NO array_path is untouched — it never told us where
   * its records live, so discovery and the single-object case are the only
   * things it can be served by, and tests 3 / 11 / 12 cover them. */
  int path_declared_missing = 0;
  if (!arr && s->array_path && *s->array_path) path_declared_missing = 1;

  if (!arr && !path_declared_missing) {
    hp_arrfind f = { 0 };
    hp_find_array(doc, 0, "", &f);
    arr = f.best;
    /* Shape notice (b): the guess was made between several arrays. One
     * candidate is not a guess; a root array is the document itself. */
    if (arr && arr != doc && f.cands >= 2) {
      st->sh_fb_pages++;
      st->sh_fb = f;
    }
  }
  int max = hp_record_cap(s);

  if (!arr) {
    /* HTTP 200 with an error DOCUMENT. Checked here and not at parse time so
     * that an envelope carrying BOTH records and an `error` member keeps its
     * records: by this point we know the response handed over none. */
    char emsg[256]; long ecode = -1;
    if (hp_json_error_doc(doc, emsg, sizeof emsg, &ecode)) {
      st->upstream_error = 1;
      st->err_code = ecode;
      fprintf(stderr, "[hp:%s] HTTP 200 carrying an error document: code=%ld %s\n",
              s->id, ecode, emsg);
      cJSON_Delete(doc);
      return st->emitted;
    }
    if (path_declared_missing) {
      /* Shape notice (a). Tallied HERE, after the error-document check: a
       * refusal is a failed fetch, not drift. Say what the path DID resolve
       * to, so the notice tells "the key is gone" from "the key now holds a
       * string". (An object at the path was filed as a single record above,
       * and an array — even an empty one — resolved, so neither reaches this.) */
      cJSON *n = hp_path(doc, s->array_path);
      const char *kind = !n ? "absent"
                       : cJSON_IsString(n) ? "a string" : cJSON_IsNumber(n) ? "a number"
                       : cJSON_IsBool(n)   ? "a boolean" : cJSON_IsNull(n) ? "null"
                       : "a node of another type";
      st->sh_path_missing++;
      snprintf(st->sh_path_kind, sizeof st->sh_path_kind, "%s", kind);
      fprintf(stderr,
              "[hp:%s] array_path \"%s\" did not resolve — response is not the "
              "declared shape, emitting nothing\n", s->id, s->array_path);
      cJSON_Delete(doc);
      return st->emitted;
    }
  }

  if (!arr) {                                          /* root IS the record */
    /* Count it. The availability tally is the in-band "N of M" disclosure the
     * exhaustive-use rule requires, and this path used to skip it — a
     * single-object endpoint reported "emitted 1 of 0 available", which reads
     * as a discard when nothing was discarded. */
    st->available += 1;
    cJSON *flat = cJSON_CreateObject();
    hp_flatten(doc, "", flat, 0);
    hp_emit_record(st, flat, st->deep_left > 0);
    cJSON_Delete(flat);
    cJSON_Delete(doc);
    return st->emitted;
  }

  int arr_n = cJSON_GetArraySize(arr);
  st->available += arr_n;
  /* Flag the records whose fallback key is shared with a sibling on this page,
   * before any of them is emitted — see hp_collision_map(). */
  unsigned char *dupmap = hp_collision_map(st, arr, arr_n, hp_json_flat);
  st->dup_map = dupmap;
  cJSON *rec;
  int ri = 0;
  cJSON_ArrayForEach(rec, arr) {
    if (max && st->emitted >= max) { st->truncated = 1; break; }
    if (st->ctx->cancel && *st->ctx->cancel) { st->truncated = 1; break; }
    cJSON *flat = hp_json_flat(s, rec);
    if (!flat) { ri++; continue; }
    int before = st->emitted;
    st->rec_idx = ri;
    hp_emit_record(st, flat, st->deep_left > 0);
    if (st->emitted > before && st->deep_left > 0) st->deep_left--;
    cJSON_Delete(flat);
    ri++;
  }
  /* The map describes THIS array only; a later single-record or detail emit
   * must not read it. */
  st->dup_map = NULL;
  st->rec_idx = 0;
  free(dupmap);
  /* Hand the caller the next page URL when the row declared one, so the walk
   * continues instead of stopping at page 1. */
  if (s->next_path && !st->next_url) {
    cJSON *nx = hp_path(doc, s->next_path);
    if (nx && cJSON_IsString(nx) && nx->valuestring[0])
      st->next_url = strdup(nx->valuestring);
  }
  st->page_records = arr ? cJSON_GetArraySize(arr) : 0;
  /* The only exit that can hold a hp_path_multi wrapper: `arr_owned` is set
   * only together with `arr`, and every earlier return above it is on a path
   * where `arr` is NULL. Deleting the wrapper frees the reference array, not
   * the records inside `doc` that it points at. */
  cJSON_Delete(arr_owned);
  cJSON_Delete(doc);
  return st->emitted;
}

static int hp_run_csv(hp_run_state *st, const char *body) {
  const hp_source *s = st->s;
  /* Headerless files (OFAC's sdn.csv and friends) must NOT be parsed with
   * headers=1: the first data row would become the column names and that row
   * would vanish. Parse positionally and name the columns col0..colN instead —
   * honest about what is known, and nothing is dropped. */
  /* Named forms exist because the manifest these rows are authored in is itself
   * pipe-delimited: writing `csv_delim=|` there produces a 14-field line and
   * fails the row rather than configuring it. */
  /* The delimiter is a STRING now: "ws" (a run of blanks) and "lit:<token>"
   * (any literal, any length) are what a fixed-width table and the 2ch-family
   * subject.txt need, and neither is a character. The single-character forms
   * resolve exactly as they always did. */
  char delim[64] = ",";
  if (s->csv_delim && s->csv_delim[0]) {
    if      (!strcmp(s->csv_delim, "tab")  || !strcmp(s->csv_delim, "\\t")) snprintf(delim, sizeof delim, "\t");
    else if (!strcmp(s->csv_delim, "pipe")) snprintf(delim, sizeof delim, "|");
    else if (!strcmp(s->csv_delim, "semi")) snprintf(delim, sizeof delim, ";");
    else if (!strcmp(s->csv_delim, "ws"))   snprintf(delim, sizeof delim, "ws");
    else if (!strncmp(s->csv_delim, "lit:", 4) && s->csv_delim[4])
                                            snprintf(delim, sizeof delim, "%s", s->csv_delim + 4);
    else                                    snprintf(delim, sizeof delim, "%c", s->csv_delim[0]);
  }
  /* A title line above the header is dropped BEFORE the comment strip and the
   * parse, as physical lines: the header that follows it is still read with
   * full quoting (MEXT's header cells contain quoted line breaks). A row that
   * declares no skip enters here with body unchanged. */
  for (int i = 0; i < s->csv_skip_lines && *body; i++) {
    const char *eol = strchr(body, '\n');
    body = eol ? eol + 1 : body + strlen(body);
  }
  /* A comment banner is stripped BEFORE the parse, not filtered after it. With
   * headers=1 the parser takes row 0 as the column names, and URLhaus's row 0
   * is `# id,dateadded,url,...` — so post-filtering would have named every
   * column after a comment line, and the real header would have been read as a
   * record. */
  char *stripped = NULL;
  if (s->csv_comment && s->csv_comment[0]) {
    size_t n = strlen(body);
    stripped = malloc(n + 1);
    if (stripped) {
      size_t w = 0, clen = strlen(s->csv_comment);
      const char *p = body;
      while (*p) {
        const char *eol = strchr(p, '\n');
        size_t linelen = eol ? (size_t)(eol - p) + 1 : strlen(p);
        const char *t = p;
        while (*t == ' ' || *t == '\t') t++;
        if (strncmp(t, s->csv_comment, clen) != 0) {
          memcpy(stripped + w, p, linelen);
          w += linelen;
        }
        if (!eol) break;
        p = eol + 1;
      }
      stripped[w] = 0;
      body = stripped;
    }
  }
  cJSON *rows = csv_parse_x(body, s->csv_no_header ? 0 : 1, delim, 0, NULL);
  free(stripped);
  if (!rows) return 0;
  int max = hp_record_cap(s);
  st->available += cJSON_GetArraySize(rows);
  /* The page walk stops on `page_records <= 0`. Leaving it at 0 here meant a
   * paged CSV row read page 1 and silently discarded every page after it —
   * with no truncation notice either, since nothing set `truncated`. */
  st->page_records = cJSON_GetArraySize(rows);
  /* uid collision guard, same as the JSON path — a CSV with no row id whose
   * `id_keys` repeats (a phishing URL re-confirmed on a later date is a second
   * row in JPCERT's monthly files) must not collapse the differing rows. */
  unsigned char *dupmap = hp_collision_map(st, rows, cJSON_GetArraySize(rows), hp_csv_flat);
  st->dup_map = dupmap;
  cJSON *row;
  int ri = 0;
  cJSON_ArrayForEach(row, rows) {
    if (max && st->emitted >= max) { st->truncated = 1; break; }
    if (st->ctx->cancel && *st->ctx->cancel) { st->truncated = 1; break; }
    cJSON *flat = hp_csv_flat(s, row);
    if (!flat) { ri++; continue; }
    st->rec_idx = ri;
    hp_emit_record(st, flat, 0);
    cJSON_Delete(flat);
    ri++;
  }
  st->dup_map = NULL;
  st->rec_idx = 0;
  free(dupmap);
  cJSON_Delete(rows);
  return st->emitted;
}

/* ── XML ─────────────────────────────────────────────────────────────────── */

/* Tag name starting just past '<'. Stops at whitespace, '/' or '>'. */
static size_t hp_xml_name(const char *p, char *buf, size_t cap) {
  size_t n = 0;
  while (p[n] && !isspace((unsigned char)p[n]) && p[n] != '>' && p[n] != '/' &&
         n + 1 < cap) { buf[n] = p[n]; n++; }
  buf[n] = 0;
  return n;
}

/* Skip one `<!...>` construct that `p` points at (p[0]=='<', p[1]=='!'):
 * a CDATA section runs to its `]]>`, a comment to its `-->`, anything else
 * (DOCTYPE) to the next '>'. Returns the first byte AFTER it, or `end`.
 * Every scanner below uses this rather than a bare memchr('>') because a
 * CDATA payload is arbitrary text: NICT's feeds put HTML in it, so the first
 * '>' inside is a `<br>`'s, not the section's. */
static const char *hp_xml_skip_bang(const char *p, const char *end) {
  const char *needle; size_t nn;
  if (end - p >= 9 && !strncmp(p, "<![CDATA[", 9)) { needle = "]]>"; nn = 3; p += 9; }
  else if (end - p >= 4 && !strncmp(p, "<!--", 4))  { needle = "-->"; nn = 3; p += 4; }
  else                                              { needle = ">";   nn = 1; p += 2; }
  while (p < end) {
    const char *c = memchr(p, needle[0], (size_t)(end - p));
    if (!c || (size_t)(end - c) < nn) return end;
    if (!strncmp(c, needle, nn)) return c + nn;
    p = c + 1;
  }
  return end;
}

/* Does [p,end) contain a '<' that opens MARKUP — i.e. not one sitting inside
 * a CDATA section or a comment? Decides whether an element's content is text
 * or child elements. `<title><![CDATA[…]]></title>` used to answer yes (the
 * CDATA opener is a '<'), the recursion then treated `<![CDATA[` as a
 * declaration and skipped to the first '>', and the title was gone — the
 * record fell back to its <link> as a title on every IPA/NICT/MHLW feed. */
static int hp_xml_has_markup(const char *p, const char *end) {
  while (p < end && (p = memchr(p, '<', (size_t)(end - p))) != NULL) {
    if (p + 1 < end && p[1] == '!') { p = hp_xml_skip_bang(p, end); continue; }
    return 1;
  }
  return 0;
}

/* Encode one code point as UTF-8 into w; returns bytes written (1..4). */
static size_t hp_put_utf8(char *w, unsigned long cp) {
  if (cp < 0x80)         { w[0] = (char)cp; return 1; }
  if (cp < 0x800)        { w[0] = (char)(0xC0 | (cp >> 6));  w[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
  if (cp < 0x10000)      { w[0] = (char)(0xE0 | (cp >> 12)); w[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                           w[2] = (char)(0x80 | (cp & 0x3F)); return 3; }
  w[0] = (char)(0xF0 | (cp >> 18)); w[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
  w[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); w[3] = (char)(0x80 | (cp & 0x3F)); return 4;
}

/* XML text → the characters it denotes, in place. Declared in hpengine.h so
 * the hand-written feed parsers (lib/rss_atom.c has its own five-entity,
 * first-CDATA-only copy of this) can share it instead of drifting.
 *
 * What it does:
 *   - unwraps every `<![CDATA[ … ]]>` section, however many, mixed with text
 *     in any order; the payload is copied VERBATIM, because inside CDATA an
 *     `&amp;` is four literal characters;
 *   - decodes the five predefined entities and `&#NNN;` / `&#xHHH;` numeric
 *     references to UTF-8. The old version stopped at code point 127, and on
 *     14 live NDL feeds — whose titles are written entirely as `&#x6b74;…` —
 *     every title was stored as its escape sequence, byte for byte;
 *   - leaves anything it cannot decode LITERAL: an unknown entity, a
 *     reference with no ';' within 12 bytes, a code point of 0 (a NUL would
 *     truncate the string), a surrogate, or one above U+10FFFF. Literal is
 *     visible in a read-back; a guessed character is not.
 * Never grows the string (the shortest reference, `&#N;`, is 4 bytes for 1;
 * a 4-byte UTF-8 sequence needs `&#65536;` = 8), so in-place is safe.
 * Text with no '&' and no '<' is copied byte-identically. */
void hp_xml_decode(char *s) {
  if (!s || !strpbrk(s, "&<")) return;
  char *w = s;
  for (char *r = s; *r; ) {
    if (*r == '<' && !strncmp(r, "<![CDATA[", 9)) {
      char *e = strstr(r + 9, "]]>");
      if (!e) { size_t n = strlen(r + 9); memmove(w, r + 9, n); w += n; break; }
      memmove(w, r + 9, (size_t)(e - (r + 9))); w += e - (r + 9);
      r = e + 3;
      continue;
    }
    if (*r != '&') { *w++ = *r++; continue; }
    if      (!strncmp(r, "&amp;", 5))  { *w++ = '&';  r += 5; continue; }
    else if (!strncmp(r, "&lt;", 4))   { *w++ = '<';  r += 4; continue; }
    else if (!strncmp(r, "&gt;", 4))   { *w++ = '>';  r += 4; continue; }
    else if (!strncmp(r, "&quot;", 6)) { *w++ = '"';  r += 6; continue; }
    else if (!strncmp(r, "&apos;", 6)) { *w++ = '\''; r += 6; continue; }
    if (r[1] == '#') {
      int hex = (r[2] == 'x' || r[2] == 'X');
      const char *d = r + 2 + hex;
      if (hex ? isxdigit((unsigned char)*d) : isdigit((unsigned char)*d)) {
        char *end = NULL;
        errno = 0;
        unsigned long cp = strtoul(d, &end, hex ? 16 : 10);
        if (end && *end == ';' && end - r <= 12 && errno == 0 &&
            cp > 0 && cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF)) {
          w += hp_put_utf8(w, cp);
          r = end + 1;
          continue;
        }
      }
    }
    *w++ = *r++;                                  /* not decodable — literal */
  }
  *w = 0;
}

/* Flatten the ATTRIBUTES of one start tag into the same dotted keyspace its
 * child elements use, under an `@` prefix on the last segment: `@id`,
 * `Codelist.@agencyID`.
 *
 * They used to be dropped outright — hp_xml_flatten() walked child ELEMENTS
 * only — and the cost of that was a whole family of live sources. In SDMX
 * structural metadata the identity IS the attribute:
 *
 *   <str:Codelist id="CL_FREQ" agencyID="ILO" version="1.0">
 *
 * so ILO / OECD / ABS / Istat / ECB / Eurostat / IMF all parsed into records
 * that carried the human-readable <com:Name> and nothing to key it on. Two
 * live WITS endpoints were rejected in batch 20 rather than shipped as silent
 * partials, and the rest of the family was simply unreachable.
 *
 * `@` is the disambiguator because it is the one character an XML Name cannot
 * begin with, so `Codelist.@id` can never collide with a child element named
 * `id` under the same parent — the same property the JSON flattener gets from
 * dotting (`address.city` cannot collide with a member literally named
 * "address.city", since it builds every key itself). hp_flat_get() strips a
 * leading `@` when it compares last segments, so a row that declares
 * `id_keys=id` finds `@id` without knowing whether the upstream spells its
 * identity as an attribute or an element; `id_keys=@id` still selects the
 * attribute specifically.
 *
 * `p` points just past the tag NAME, `gt` at the closing '>' (or at the '/' of
 * a self-closing tag — a trailing slash carries no '=' and is skipped). */
static void hp_xml_attrs(const char *p, const char *gt, const char *prefix,
                         cJSON *flat) {
  char key[256];
  while (p < gt) {
    while (p < gt && !(isalpha((unsigned char)*p) || *p == '_' || *p == ':')) p++;
    const char *ns = p;
    while (p < gt && (isalnum((unsigned char)*p) || *p == '_' || *p == ':' ||
                      *p == '-' || *p == '.')) p++;
    size_t nl = (size_t)(p - ns);
    if (!nl) return;
    while (p < gt && isspace((unsigned char)*p)) p++;
    if (p >= gt || *p != '=') continue;         /* not an attribute — skip it */
    p++;
    while (p < gt && isspace((unsigned char)*p)) p++;
    if (p >= gt) return;
    const char *vs; size_t vl;
    if (*p == '"' || *p == '\'') {
      char q = *p++;
      const char *e = memchr(p, q, (size_t)(gt - p));
      if (!e) return;                            /* unterminated — stop here  */
      vs = p; vl = (size_t)(e - p); p = e + 1;
    } else {
      vs = p;
      while (p < gt && !isspace((unsigned char)*p) && *p != '/') p++;
      vl = (size_t)(p - vs);
    }
    if (!vl || vl >= 4096) continue;
    if (cJSON_GetArraySize(flat) > 400) return;
    if (prefix && *prefix) snprintf(key, sizeof key, "%s.@%.*s", prefix, (int)nl, ns);
    else                   snprintf(key, sizeof key, "@%.*s", (int)nl, ns);
    if (cJSON_GetObjectItem(flat, key)) continue;
    char *val = (char *)malloc(vl + 1);
    if (!val) return;
    memcpy(val, vs, vl); val[vl] = 0;
    hp_xml_decode(val);
    if (val[0]) cJSON_AddStringToObject(flat, key, val);
    free(val);
  }
}

/* Flatten one record element's children into `flat` with dotted keys, so an
 * XML record reaches hp_emit_record in exactly the shape a JSON one does and
 * every downstream field selector (title_keys, id_keys, lat_key…) works
 * unchanged. Bounded in depth and in field count. */
static void hp_xml_flatten(const char *p, const char *end, const char *prefix,
                           cJSON *flat, int depth) {
  if (depth > 4 || cJSON_GetArraySize(flat) > 400) return;
  char name[96], key[256];
  while (p < end && (p = memchr(p, '<', (size_t)(end - p))) != NULL) {
    p++;
    if (p >= end) return;
    if (*p == '!') { p = hp_xml_skip_bang(p - 1, end); continue; } /* CDATA / comment / decl */
    if (*p == '/' || *p == '?') {                          /* close / PI */
      const char *gt = memchr(p, '>', (size_t)(end - p));
      if (!gt) return;
      p = gt + 1;
      continue;
    }
    size_t nl = hp_xml_name(p, name, sizeof name);
    if (!nl) return;
    const char *gt = memchr(p, '>', (size_t)(end - p));
    if (!gt) return;
    /* Build this child's key BEFORE the self-closing test, because a
     * self-closing element is not an empty element: `<Ref id="X" agencyID="Y"/>`
     * is pure attribute payload, and skipping the tag threw all of it away. */
    if (prefix && *prefix) snprintf(key, sizeof key, "%s.%s", prefix, name);
    else                   snprintf(key, sizeof key, "%s", name);
    hp_xml_attrs(p + nl, gt, key, flat);
    if (gt > p && gt[-1] == '/') { p = gt + 1; continue; }  /* self-closing */
    char close[100];
    int cl = snprintf(close, sizeof close, "</%s>", name);
    const char *vs = gt + 1, *ve = vs;
    /* find this element's matching close, allowing one level of same-name nest */
    int nest = 1;
    while (ve < end) {
      const char *lt = memchr(ve, '<', (size_t)(end - ve));
      if (!lt) { ve = end; break; }
      if (lt + 1 < end && lt[1] == '!') {     /* a `</x>` inside CDATA is text */
        ve = hp_xml_skip_bang(lt, end);
        continue;
      }
      if (!strncmp(lt, close, (size_t)cl)) {
        if (--nest == 0) { ve = lt; break; }
        ve = lt + cl;
      } else if (lt[1] != '/' && !strncmp(lt + 1, name, nl) &&
                 (isspace((unsigned char)lt[1 + nl]) || lt[1 + nl] == '>')) {
        nest++; ve = lt + 1;
      } else {
        ve = lt + 1;
      }
    }
    if (hp_xml_has_markup(vs, ve)) {
      hp_xml_flatten(vs, ve, key, flat, depth + 1);        /* nested element */
    } else {
      size_t vl = (size_t)(ve - vs);
      while (vl && isspace((unsigned char)*vs)) { vs++; vl--; }
      while (vl && isspace((unsigned char)vs[vl - 1])) vl--;
      if (vl && vl < 4096 && !cJSON_GetObjectItem(flat, key)) {
        char *val = (char *)malloc(vl + 1);
        if (val) {
          memcpy(val, vs, vl); val[vl] = 0;
          hp_xml_decode(val);
          /* trim again AFTER decoding: `<title>\n <![CDATA[ x ]]>\n</title>`
           * carries padding inside the wrapper as well as around it */
          char *t = val;
          while (*t && isspace((unsigned char)*t)) t++;
          size_t tl = strlen(t);
          while (tl && isspace((unsigned char)t[tl - 1])) t[--tl] = 0;
          if (tl) cJSON_AddStringToObject(flat, key, t);
          free(val);
        }
      }
    }
    p = (ve < end) ? ve + cl : end;
  }
}

/* The element that repeats most often is the record. An explicit array_path
 * overrides it, because auto-detection picks the wrong element on a schema
 * whose leaf field is more numerous than its record (a list of <target>s each
 * holding many <name>s). */
static int hp_xml_record_tag(const char *body, char *out, size_t cap) {
  struct { char n[96]; int c; } tally[64];
  int nt = 0;
  char name[96];
  const char *bend = body + strlen(body);
  for (const char *p = body; (p = strchr(p, '<')) != NULL; ) {
    if (p[1] == '!') { p = hp_xml_skip_bang(p, bend); continue; } /* CDATA payload is not markup */
    p++;
    if (*p == '/' || *p == '?') continue;
    if (!hp_xml_name(p, name, sizeof name)) continue;
    int i = 0;
    for (; i < nt; i++) if (!strcmp(tally[i].n, name)) { tally[i].c++; break; }
    if (i == nt && nt < 64) { snprintf(tally[nt].n, sizeof tally[nt].n, "%s", name); tally[nt].c = 1; nt++; }
  }
  int best = -1;
  for (int i = 0; i < nt; i++)
    if (tally[i].c >= 2 && (best < 0 || tally[i].c > tally[best].c)) best = i;
  if (best < 0) return 0;
  snprintf(out, cap, "%s", tally[best].n);
  return 1;
}

static int hp_run_xml(hp_run_state *st, const char *body) {
  const hp_source *s = st->s;
  char tag[96];
  if (s->array_path && *s->array_path) snprintf(tag, sizeof tag, "%s", s->array_path);
  else if (!hp_xml_record_tag(body, tag, sizeof tag)) {
    fprintf(stderr, "[hp:%s] XML with no repeated element\n", s->id);
    return 0;
  }
  /* A cursor element the page walk continues from — OAI-PMH's
   * <resumptionToken>. hp_run_json has resolved next_path since it was added;
   * the XML path never did, so an XML row that declared it paged not at all. */
  if (s->next_path && *s->next_path && !st->next_url) {
    char nopen[100], nclose[100];
    snprintf(nopen, sizeof nopen, "<%s", s->next_path);
    snprintf(nclose, sizeof nclose, "</%s>", s->next_path);
    const char *np = strstr(body, nopen);
    const char *ngt = np ? strchr(np, '>') : NULL;
    const char *nend = ngt ? strstr(ngt, nclose) : NULL;
    if (ngt && nend && nend > ngt + 1) {
      size_t vn = (size_t)(nend - (ngt + 1));
      char *v = malloc(vn + 1);
      if (v) {
        memcpy(v, ngt + 1, vn);
        v[vn] = 0;
        hp_xml_decode(v);            /* a token with '&' arrives as &amp; */
        /* An EMPTY <resumptionToken/> is the protocol saying "that was the
         * last page". Treating it as a cursor would refetch page 1 forever. */
        if (v[0]) st->next_url = v;
        else free(v);
      }
    }
  }
  char open[100], close[100];
  int ol = snprintf(open, sizeof open, "<%s", tag);
  int cl = snprintf(close, sizeof close, "</%s>", tag);
  int max = hp_record_cap(s);
  const char *p = body;
  int found = 0;
  cJSON *flats = cJSON_CreateArray();
  if (!flats) return 0;
  while ((p = strstr(p, open)) != NULL) {
    const char *after = p + ol;
    if (*after != '>' && !isspace((unsigned char)*after) && *after != '/') { p = after; continue; }
    const char *gt = strchr(p, '>');
    if (!gt) break;
    if (gt[-1] == '/') { p = gt + 1; continue; }           /* empty record */
    const char *endrec = strstr(gt, close);
    if (!endrec) break;
    found++;
    st->available++;
    cJSON *flat = cJSON_CreateObject();
    if (flat) {
      /* The RECORD element's own attributes, before its children. In SDMX
       * these are the whole identity — <str:Codelist id=".." agencyID=".."> —
       * and hp_xml_flatten() starts at gt+1, so nothing on the start tag was
       * ever seen. First in the object as well as first on the tag, so
       * hp_first_scalar()'s last-resort key lands on the identifier rather
       * than on the first prose field. */
      hp_xml_attrs(after, gt, "", flat);
      hp_xml_flatten(gt + 1, endrec, "", flat, 0);
      /* Collected rather than emitted here: the collision guard needs every
       * record of the page keyed BEFORE the first one is emitted, exactly as
       * the JSON and CSV paths do it. */
      cJSON_AddItemToArray(flats, flat);
    }
    p = endrec + cl;
  }
  /* uid collision guard — the elements are already flat, so no flatten step. */
  int flats_n = cJSON_GetArraySize(flats);
  unsigned char *dupmap = hp_collision_map(st, flats, flats_n, NULL);
  st->dup_map = dupmap;
  cJSON *flat;
  int ri = 0;
  cJSON_ArrayForEach(flat, flats) {
    if (max && st->emitted >= max) break;   /* `found`/available already counted them */
    st->rec_idx = ri;
    if (cJSON_GetArraySize(flat) > 0)
      hp_emit_record(st, flat, st->deep_left > 0);
    ri++;
  }
  st->dup_map = NULL;
  st->rec_idx = 0;
  free(dupmap);
  cJSON_Delete(flats);
  /* Shape notice (a), XML form: the declared record element is nowhere in a
   * body that DOES repeat some other element. A body with no repeated element
   * at all is an empty page, which is the walk ending normally, not drift. */
  if (!found && s->array_path && *s->array_path) {
    char other[96];
    if (hp_xml_record_tag(body, other, sizeof other) && strcmp(other, tag)) {
      st->sh_path_missing++;
      snprintf(st->sh_path_kind, sizeof st->sh_path_kind,
               "absent; the most repeated element is <%.48s>", other);
    }
  }
  st->page_records = found;
  return st->emitted;
}

/* ── relative reference resolution (RFC 3986 §5.2) ───────────────────────
 *
 * The engine used to PREFIX a root-relative href with `base` and store every
 * other relative href as-is. A listing that links `../profile/7007006.htm`
 * (Sangiin), `./weakness.php?id=95` (EC-CUBE) or `CVE-2026-50751.html`
 * (Yamaha) therefore stored links that resolve nowhere, and batch 25 rejected
 * six live sources as RELATIVE_LINKS for that reason alone. This is the
 * resolver every browser applies, written out: scheme-relative, root-relative,
 * query-only, fragment-only and directory-relative references, with dot
 * segments removed. `out` always receives something — on a base that cannot
 * be parsed the reference is copied verbatim, never dropped. */
static void hp_remove_dots(char *path) {
  /* In place. `path` starts at the first character of the path component. */
  char *out = path;
  const char *in = path;
  size_t w = 0;
  size_t n = strlen(in);
  char *buf = malloc(n + 2);
  if (!buf) return;
  while (*in) {
    if (!strncmp(in, "../", 3))      { in += 3; continue; }
    if (!strncmp(in, "./", 2))       { in += 2; continue; }
    if (!strncmp(in, "/./", 3))      { in += 2; continue; }
    if (!strcmp(in, "/."))           { in += 1; buf[w++] = '/'; break; }
    if (!strncmp(in, "/../", 4) || !strcmp(in, "/..")) {
      in += 3;                     /* keep the '/' that follows, if any */
      while (w > 0 && buf[w - 1] != '/') w--;   /* pop the last segment */
      if (w > 0) w--;
      if (!*in) buf[w++] = '/';
      continue;
    }
    if (!strcmp(in, ".") || !strcmp(in, "..")) break;
    /* copy one segment, including its leading '/' */
    if (*in == '/') buf[w++] = *in++;
    while (*in && *in != '/') buf[w++] = *in++;
  }
  buf[w] = 0;
  memcpy(out, buf, w + 1);
  free(buf);
}

static int hp_has_scheme(const char *ref) {
  const char *p = ref;
  if (!isalpha((unsigned char)*p)) return 0;
  while (isalnum((unsigned char)*p) || *p == '+' || *p == '-' || *p == '.') p++;
  return *p == ':';
}

static void hp_url_resolve(const char *base, const char *ref, char *out, size_t cap) {
  if (!ref) ref = "";
  if (hp_has_scheme(ref) || !base || !*base || !hp_has_scheme(base)) {
    snprintf(out, cap, "%s", ref);
    return;
  }
  /* split the base: scheme "://" authority path ["?" query] ["#" fragment] */
  const char *auth = strstr(base, "://");
  const char *path = NULL, *pend = NULL;
  size_t auth_len = 0;
  if (auth) {
    auth += 3;
    path = auth;
    while (*path && *path != '/' && *path != '?' && *path != '#') path++;
    auth_len = (size_t)(path - base);          /* scheme://authority */
  } else {
    path = strchr(base, ':') + 1;
    auth_len = (size_t)(path - base);
  }
  pend = path;
  while (*pend && *pend != '?' && *pend != '#') pend++;
  const char *qend = pend;                     /* base query, up to '#' */
  while (*qend && *qend != '#') qend++;

  if (ref[0] == '/' && ref[1] == '/') {        /* scheme-relative */
    const char *colon = strchr(base, ':');
    snprintf(out, cap, "%.*s:%s", (int)(colon - base), base, ref);
    return;
  }
  /* Precisions on the tail formats: the caller's buffer is 2048 and these
   * pieces are bounded to fit it, so a pathological 4 KB href is cut, not
   * left to snprintf's truncation, which is the same result stated up front. */
  char tmp[2048];
  if (ref[0] == '/') {                          /* root-relative */
    snprintf(tmp, sizeof tmp, "%s", ref);
    char *q = tmp; while (*q && *q != '?' && *q != '#') q++;
    char tail[1024]; snprintf(tail, sizeof tail, "%.1000s", q); *q = 0;
    hp_remove_dots(tmp);
    snprintf(out, cap, "%.*s%.1500s%.500s", (int)auth_len, base, tmp, tail);
    return;
  }
  if (ref[0] == '?') {                          /* query-only: keep the path */
    snprintf(out, cap, "%.*s%s", (int)(pend - base), base, ref);
    return;
  }
  if (ref[0] == '#') {                          /* fragment-only */
    snprintf(out, cap, "%.*s%s", (int)(qend - base), base, ref);
    return;
  }
  if (!*ref) { snprintf(out, cap, "%.*s", (int)(qend - base), base); return; }
  /* directory-relative: merge with the base path up to its last '/' */
  const char *slash = NULL;
  for (const char *p = path; p < pend; p++) if (*p == '/') slash = p;
  size_t dir_len = slash ? (size_t)(slash + 1 - path) : 0;
  const char *rq = ref; while (*rq && *rq != '?' && *rq != '#') rq++;
  if (!slash && auth)                            /* empty base path: "/" + ref */
    snprintf(tmp, sizeof tmp, "/%.*s", (int)(rq - ref), ref);
  else
    snprintf(tmp, sizeof tmp, "%.*s%.*s", (int)dir_len, path, (int)(rq - ref), ref);
  hp_remove_dots(tmp);
  snprintf(out, cap, "%.*s%.1500s%.500s", (int)auth_len, base, tmp, rq);
}

/* `<base href="...">` in the page head, if any — the document's own statement
 * of what its relative links are relative to. Case-insensitive; the first one
 * wins, as in a browser. Returns 1 and fills `out`. */
static int hp_html_base_href(const char *html, char *out, size_t cap) {
  for (const char *p = html; (p = strchr(p, '<')) != NULL; p++) {
    if (strncasecmp(p, "<base", 5) || !(isspace((unsigned char)p[5]))) continue;
    const char *gt = strchr(p, '>');
    if (!gt) return 0;
    char tag[2048];
    snprintf(tag, sizeof tag, "%.*s", (int)(gt - p + 1), p);
    if (html_attr(tag, "href", out, cap) && out[0]) return 1;
    return 0;
  }
  return 0;
}

/* Label for an anchor whose parsed text came up short.
 *
 * html_anchor_next() strips tags but glues text across line breaks and keeps
 * runs of blanks, and it has no opinion about an anchor whose only label is an
 * image. Card layouts — a `<div class="card"><a href><img …><h3>\n Title\n
 * </h3></a>` — routinely fail its `text_len < 3` test that way and were
 * dropped, record and all. Tried in order, only when the parsed text is short
 * (so an anchor that already carries text is untouched):
 *
 *   1. the anchor's full descendant text, every whitespace run collapsed to
 *      one space, trimmed;
 *   2. the first <img>'s `alt`, then its `title`;
 *
 * and only then is the anchor dropped, as before. `inner`..`end` is the markup
 * between the start tag's `>` and `</a>`. Returns the label length. */
static size_t hp_anchor_label(const char *inner, const char *end, char *out, size_t cap) {
  size_t n = 0;
  int intag = 0, pend = 0;
  for (const char *q = inner; q < end && n + 1 < cap; q++) {
    if (*q == '<') { intag = 1; continue; }
    if (*q == '>') { intag = 0; continue; }
    if (intag) continue;
    if (isspace((unsigned char)*q)) { pend = n > 0; continue; }
    if (pend) { out[n++] = ' '; pend = 0; if (n + 1 >= cap) break; }
    out[n++] = *q;
  }
  out[n] = 0;
  if (n >= 3) return n;
  /* No usable text: the first image's alternative text is the label the
   * page itself offers a reader who cannot see the image. */
  for (const char *im = inner; (im = strstr(im, "<img")) != NULL && im < end; im += 4) {
    if (im[4] != ' ' && im[4] != '\t' && im[4] != '\n' && im[4] != '\r' && im[4] != '/') continue;
    const char *gt = strchr(im, '>');
    if (!gt || gt > end) break;
    size_t taglen = (size_t)(gt - im) + 1;
    char tag[1024];
    if (taglen >= sizeof tag) break;
    memcpy(tag, im, taglen);
    tag[taglen] = 0;
    if (html_attr(tag, "alt", out, cap) && strlen(out) >= 3) return strlen(out);
    if (html_attr(tag, "title", out, cap) && strlen(out) >= 3) return strlen(out);
    break;
  }
  out[0] = 0;
  return 0;
}

/* Real anchors out of a real listing page. JS-rendered or anti-bot pages
 * simply yield nothing. */
static int hp_run_html(hp_run_state *st, const char *html) {
  const hp_source *s = st->s;
  int max = hp_record_cap(s);
  /* What a relative href is relative to: the row's override, else the page's
   * own <base href>, else the URL this page was fetched from — which is the
   * per-page URL, so page 2 of a walk resolves against page 2. (The http layer
   * does not expose the post-redirect URL; a listing that redirects across
   * directories can declare `base` to say where it really lives.) */
  char base_url[2048];
  if (s->base && s->base[0]) snprintf(base_url, sizeof base_url, "%s", s->base);
  else if (!hp_html_base_href(html, base_url, sizeof base_url))
    snprintf(base_url, sizeof base_url, "%s", st->url ? st->url : "");
  /* Scanning and dedupe come from lib/htmlparse (html_anchor_next /
   * html_seen_*) — the SAME code jo_emit_anchors() uses in the registry
   * sweeps. This function is now only the engine's policy: which anchors to
   * accept, and emitting them through the engine's record path so they carry
   * the endpoint / page / truncation provenance every hp row carries. */
  html_anchor a;
  const char *p = html;
  int page_hits = 0;
  while ((!max || st->emitted < max) && (p = html_anchor_next(p, &a)) != NULL) {
    char href[820];
    snprintf(href, sizeof href, "%.*s", (int)a.href_len, a.href);
    if (s->href_must && !strstr(href, s->href_must)) continue;
    if (a.text_len < 3) {
      /* `p` is the resume point, just past `</a>`; the start tag's `>` is the
       * first one after the href value (the parser located `</a>` the same
       * way). See hp_anchor_label(). */
      const char *inner = strchr(a.href + a.href_len, '>');
      if (!inner || inner >= p - 4) continue;
      a.text_len = hp_anchor_label(inner + 1, p - 4, a.text, sizeof a.text);
      if (a.text_len < 3) continue;
    }
    if (s->filter_query && st->vars->raw &&
        !hp_icontains(a.text, st->vars->raw) && !hp_icontains(href, st->vars->raw))
      continue;
    st->available++;
    /* The same href twice on one page is one record, not a discard. Listings
     * routinely link each item from both an icon and its title, which made
     * ECMA's standards index report "emitted 295 of 590" and ITLOS "36 of 72" —
     * a perfect 50% shortfall that was really a perfect 2x duplication. */
    if (!html_seen_add(&st->hseen, href)) { st->duplicate++; continue; }
    page_hits++;

    char link[2048];
    if (!strncmp(href, "http", 4)) snprintf(link, sizeof link, "%s", href);
    else hp_url_resolve(base_url, href, link, sizeof link);

    cJSON *flat = cJSON_CreateObject();
    cJSON_AddStringToObject(flat, "title", a.text);
    cJSON_AddStringToObject(flat, "url", link);
    cJSON_AddStringToObject(flat, "id", link);
    hp_emit_record(st, flat, 0);
    cJSON_Delete(flat);
  }
  if (max && st->emitted >= max) st->truncated = 1;
  /* Same stop signal the JSON path publishes: without it the page walk read
   * page 1 and silently dropped every later page (EU_EUIPO_TRADEMARKS and
   * CA_CIPO_TRADEMARKS both declare page_param with mode = HP_HTML), and
   * `truncated` stayed 0 so not even a notice was emitted. */
  st->page_records = page_hits;
  return st->emitted;
}

/* ── the one run() every row shares ──────────────────────────────────────── */

static const hp_source *hp_lookup(const char *id) {
  if (!id) return NULL;
  for (int i = 0; i < g_nspecs; i++)
    if (!strcmp(g_specs[i]->id, id)) return g_specs[i];
  return NULL;
}

/* FNV-1a 64 over a whole response body. Only ever compared for equality
 * between consecutive pages of one walk (shape notice (d)); nothing is keyed
 * on it. */
static unsigned long long hp_body_hash(const char *body) {
  unsigned long long h = 1469598103934665603ULL;
  for (const unsigned char *p = (const unsigned char *)body; body && *p; p++)
    h = (h ^ *p) * 1099511628211ULL;
  return h;
}

/* ── schema drift, disclosed as data ─────────────────────────────────────
 *
 * Every counter below was accumulated during the walk; this turns each one
 * that is non-zero into ONE `collector-shape-notice` record for the run. A
 * counter that stayed at zero produces nothing — a notice is never invented
 * for a condition that did not occur — and the records the walk found have
 * already been emitted; a notice is in addition to them, never instead.
 * Keyed per (source, condition, day) by jo_shape_notice(), so a scheduled
 * row hitting the same drift every tick keeps one disclosure per day. */
static void hp_shape_notices(hp_run_state *st, intel_sink *sink,
                             const hp_vars *v, int emitted, int page_max) {
  const hp_source *s = st->s;
  const char *q = v->raw ? v->raw : "";
  static const char *TAGS = "[\"osint-search\",\"shape-notice\"]";
  char title[512];
  cJSON *p;

  if (st->sh_path_missing) {                                   /* (a) */
    p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "query", q);
    cJSON_AddStringToObject(p, "declared_array_path", s->array_path ? s->array_path : "");
    cJSON_AddStringToObject(p, "resolved_to", st->sh_path_kind);
    cJSON_AddNumberToObject(p, "pages_affected", st->sh_path_missing);
    cJSON_AddNumberToObject(p, "pages_read", st->page);
    cJSON_AddNumberToObject(p, "records_emitted", emitted);
    cJSON_AddStringToObject(p, "remedy",
      "the upstream's envelope changed: re-point array_path in the manifest "
      "(tools/diagnose_emit_keys.py), or the row is not a record source");
    snprintf(title, sizeof title,
             "%s: declared array_path \"%s\" resolved to %s on %d of %d page(s); "
             "%d record(s) emitted",
             s->id, s->array_path ? s->array_path : "", st->sh_path_kind,
             st->sh_path_missing, st->page, emitted);
    jo_shape_notice(sink, s->id, "array-path-missing", title, p, TAGS);
  }

  if (st->sh_fb_pages) {                                       /* (b) */
    p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "query", q);
    cJSON_AddStringToObject(p, "declared_array_path", "");
    cJSON_AddStringToObject(p, "mined_array_path", st->sh_fb.best_path);
    cJSON_AddNumberToObject(p, "mined_array_size", st->sh_fb.best_n);
    cJSON_AddNumberToObject(p, "candidate_arrays", st->sh_fb.cands);
    cJSON_AddStringToObject(p, "runner_up_path", st->sh_fb.alt_path);
    cJSON_AddNumberToObject(p, "runner_up_size", st->sh_fb.alt_n);
    cJSON_AddNumberToObject(p, "pages_affected", st->sh_fb_pages);
    cJSON_AddNumberToObject(p, "pages_read", st->page);
    cJSON_AddNumberToObject(p, "records_emitted", emitted);
    cJSON_AddStringToObject(p, "remedy",
      "declare array_path on this row so the record array is chosen by name, "
      "not by size — an upstream that grows another array will flip this guess");
    snprintf(title, sizeof title,
             "%s: no array_path declared; mined \"%s\" (%d records) out of %d "
             "candidate arrays, runner-up \"%s\" (%d), on %d of %d page(s)",
             s->id, st->sh_fb.best_path, st->sh_fb.best_n, st->sh_fb.cands,
             st->sh_fb.alt_path, st->sh_fb.alt_n, st->sh_fb_pages, st->page);
    jo_shape_notice(sink, s->id, "densest-array-fallback", title, p, TAGS);
  }

  if (st->sh_title_pages) {                                    /* (c) */
    p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "query", q);
    cJSON_AddStringToObject(p, "declared_title_keys", s->title_keys);
    cJSON_AddNumberToObject(p, "records_seen", st->sh_title_n);
    cJSON_AddNumberToObject(p, "records_matched", 0);
    cJSON_AddNumberToObject(p, "pages_affected", st->sh_title_pages);
    cJSON_AddNumberToObject(p, "pages_read", st->page);
    cJSON_AddNumberToObject(p, "records_emitted", emitted);
    cJSON_AddStringToObject(p, "remedy",
      "the records no longer carry this field: re-point title_keys in the "
      "manifest (tools/diagnose_emit_keys.py); until then records are titled "
      "from the fallback list or their first scalar");
    snprintf(title, sizeof title,
             "%s: title_keys \"%s\" matched 0 of %d record(s) on %d of %d page(s)",
             s->id, s->title_keys, st->sh_title_n, st->sh_title_pages, st->page);
    jo_shape_notice(sink, s->id, "title-keys-unmatched", title, p, TAGS);
  }

  if (st->sh_id_pages) {                                       /* (c) */
    p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "query", q);
    cJSON_AddStringToObject(p, "declared_id_keys", s->id_keys);
    cJSON_AddNumberToObject(p, "records_seen", st->sh_id_n);
    cJSON_AddNumberToObject(p, "records_matched", 0);
    cJSON_AddNumberToObject(p, "pages_affected", st->sh_id_pages);
    cJSON_AddNumberToObject(p, "pages_read", st->page);
    cJSON_AddNumberToObject(p, "records_emitted", emitted);
    cJSON_AddStringToObject(p, "remedy",
      "the records no longer carry this field: re-point id_keys in the "
      "manifest; until then records are keyed on the fallback list or their "
      "title, which is a different uid from the one they had");
    snprintf(title, sizeof title,
             "%s: id_keys \"%s\" matched 0 of %d record(s) on %d of %d page(s)",
             s->id, s->id_keys, st->sh_id_n, st->sh_id_pages, st->page);
    jo_shape_notice(sink, s->id, "id-keys-unmatched", title, p, TAGS);
  }

  if (st->sh_repeat_page) {                                    /* (d) */
    p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "query", q);
    cJSON_AddStringToObject(p, "page_param", s->page_param ? s->page_param : "");
    cJSON_AddStringToObject(p, "next_path",  s->next_path  ? s->next_path  : "");
    cJSON_AddNumberToObject(p, "repeated_page", st->sh_repeat_page);
    cJSON_AddNumberToObject(p, "pages_read", st->page);
    cJSON_AddNumberToObject(p, "pages_skipped", st->sh_repeat_skipped);
    cJSON_AddNumberToObject(p, "page_ceiling", page_max);
    cJSON_AddNumberToObject(p, "records_emitted", emitted);
    cJSON_AddStringToObject(p, "remedy",
      "the upstream ignores this page parameter: re-point page_param/next_path "
      "at one it honours (known-issue #54), or drop paging from the row");
    snprintf(title, sizeof title,
             "%s: page %d was byte-identical to page %d — page parameter "
             "ignored; stopped and skipped %d further page(s) of a %d-page "
             "ceiling; %d record(s) emitted",
             s->id, st->sh_repeat_page, st->sh_repeat_page - 1,
             st->sh_repeat_skipped, page_max, emitted);
    jo_shape_notice(sink, s->id, "page-param-ignored", title, p, TAGS);
  }
}

static int hp_run(const source_ctx *ctx, intel_sink *sink) {
  const hp_source *s = hp_lookup(ctx->source_id);
  if (!s) return -1;
  const char *e = ctx->entity;

  /* Two ways in. A pivot run carries an entity. A SCHEDULED run does not, and
   * used to fall straight out of this early return — which meant a row that set
   * `interval` was registered with a live update_interval_sec, was picked up by
   * the scheduler, and then did nothing on every tick, forever, reporting
   * rc=0/records=0 as though the upstream had simply been quiet. That is the
   * failure mode the no-fabrication rule exists to prevent, one level up: a
   * source asserting an honest empty it never actually went and checked.
   *
   * A scheduled run is legitimate exactly when the row names a fixed endpoint —
   * no entity token in the URL or POST body — because then there is nothing an
   * entity would have supplied. A row that does reference {q…} genuinely has
   * nothing to fetch without one and still returns the honest empty. */
  int scheduled = (!e || !*e);
  if (scheduled) {
    if (s->interval <= 0) return 0;           /* on-demand pivot, no entity */
    if (hp_uses_entity(s->url) || hp_uses_entity(s->post_body)) {
      fprintf(stderr, "[hp:%s] scheduled run but the row needs an entity — skipped\n",
              s->id);
      return 0;
    }
    e = "";
  } else if (!hp_matches(s->want, e)) {
    fprintf(stderr, "[hp:%s] entity shape mismatch — skipped\n", s->id);
    return 0;
  }
  const char *key = NULL;
  if (s->key_env) {
    key = getenv(s->key_env);
    if (!key || !*key) {
      fprintf(stderr, "[hp:%s] %s unset — honest empty\n", s->id, s->key_env);
      return 0;
    }
  }

  hp_vars vars;
  hp_vars_build(&vars, e, key);
  if (hp_needs_missing(s->url, &vars) || hp_needs_missing(s->post_body, &vars)) {
    fprintf(stderr, "[hp:%s] entity yields no value for a required token — skipped\n", s->id);
    hp_vars_free(&vars);
    return 0;
  }

  char *url  = hp_expand(s->url, &vars, NULL, NULL);
  char *body = s->post_body ? hp_expand(s->post_body, &vars, NULL, NULL) : NULL;
  if (!url) { free(body); hp_vars_free(&vars); return 0; }

  /* Headers: the row's own templates plus a JSON Accept and, when posting, a
   * content type. Bounded, NULL-terminated for http_request. */
  char *hdr_store[8] = {0};
  const char *hdrs[9];
  int nh = 0;
  for (int i = 0; i < 5 && s->headers[i] && nh < 7; i++) {
    hdr_store[nh] = hp_expand(s->headers[i], &vars, NULL, NULL);
    if (hdr_store[nh]) { hdrs[nh] = hdr_store[nh]; nh++; }
  }
  if (s->mode == HP_JSON && nh < 7) hdrs[nh++] = "Accept: application/json";
  if (body && nh < 7)
    hdrs[nh++] = s->content_type ? s->content_type : "Content-Type: application/json";
  hdrs[nh] = NULL;

  /* ── the page walk ──────────────────────────────────────────────────────
   * A paged endpoint read once has silently discarded every page after the
   * first, which is exactly what the exhaustive-use rule forbids. When a row
   * declares next_path or page_param we keep going until the upstream stops
   * producing records (or the page ceiling bites, which is stamped, not
   * silent). Rows that declare no paging do exactly one request, as before. */
  int out = 0, hard_error = 0;
  int page_max = s->page_max > 0 ? s->page_max : HP_PAGE_MAX_DEF;
  /* A `{page}` token left in the URL after entity expansion is path-segment
   * paging: the upstream numbers its pages in the path (kanpou.ai's
   * /tosan/p/N), which no query parameter can express. hp_expand leaves an
   * unknown token verbatim, so it survives to here untouched. */
  int path_paged = strstr(url, "{page}") != NULL;
  if (path_paged && s->page_param)
    fprintf(stderr, "[hp:%s] both {page} and page_param declared — {page} "
            "wins; the row should declare one\n", s->id);
  int paged = (s->next_path || s->page_param || path_paged) ? 1 : 0;
  if (!paged) page_max = 1;

  hp_run_state st = { .s = s, .ctx = ctx, .sink = sink, .vars = &vars,
                      .url = url, .emitted = 0,
                      .deep_left = hp_detail_budget(s) };
  int page_start = s->page_start;
  /* Coerce an unset page_start to 1 for page-numbered APIs — but not when the
   * row has declared the API is 0-based. Without that exemption a 0-based API
   * gets its first extra page computed as page_start(1) + 0 + 1 = 2, and page 1
   * is never fetched: a silent one-page hole in every paged read, invisible
   * because the pages either side arrive normally. */
  if (!page_start && !s->page_zero_based &&
      ((s->page_param && !strstr(s->page_param, "offset")) || path_paged))
    page_start = 1;

  char *page_url = strdup(url);
  if (path_paged) {
    char nb[24];
    snprintf(nb, sizeof nb, "%d", page_start);
    free(page_url);
    page_url = hp_expand(url, &vars, "page", nb);
  }
  unsigned long long prev_hash = 0;   /* previous page's body, for notice (d) */
  for (int page = 0; page < page_max && page_url; page++) {
    st.page = page + 1;
    st.url  = page_url;
    st.page_records = 0;

    http_response hr = {0};
    int rc = http_request(ctx->http, body ? "POST" : "GET", page_url, hdrs,
                          body, body ? strlen(body) : 0,
                          s->timeout_ms > 0 ? s->timeout_ms : HP_HTTP_TIMEOUT,
                          1, &hr);
    if (rc != 0) {
      fprintf(stderr, "[hp:%s] transport failure %s\n", s->id, page_url);
      if (page == 0) hard_error = 1;
      http_response_free(&hr);
      break;
    }
    if (hr.status != 200 || !hr.body) {
      fprintf(stderr, "[hp:%s] status=%ld %s\n", s->id, hr.status, page_url);
      if (hr.status >= 500 && page == 0) hard_error = 1;
      http_response_free(&hr);
      break;
    }
    /* Shape notice (d): the same bytes as the previous page. A server that
     * ignores the page parameter re-serves page 1 with HTTP 200, and the walk
     * used to parse it again and again up to the ceiling — every record
     * re-emitted onto its own uid, every real later page never asked for, and
     * the run looking perfectly healthy (known-issue #54: 86 rows). Identical
     * bytes cannot hold a new record, so the walk stops here and says so. */
    unsigned long long bh = hp_body_hash(hr.body);
    if (page > 0 && bh == prev_hash) {
      st.sh_repeat_page    = page + 1;
      st.sh_repeat_skipped = page_max - (page + 1);
      fprintf(stderr, "[hp:%s] page %d is byte-identical to page %d — the page "
              "parameter is being ignored, stopping the walk\n", s->id, page + 1, page);
      http_response_free(&hr);
      break;
    }
    prev_hash = bh;
    /* Transcode BEFORE the parse, not after: a Shift_JIS title that has already
     * been through the JSON/XML/CSV reader is bytes nobody can recover a string
     * from, and the record would be stored as mojibake. */
    /* An .xlsx is a zip, not text: it must bypass the transcode and be handed
     * to the converter with its byte length, never strlen. */
    char *utf8 = s->mode == HP_XLSX ? NULL
               : hp_body_to_utf8(s, page_url, hr.body, hr.body_len);
    const char *pbody = utf8 ? utf8 : hr.body;
    char *xcsv = NULL;
    switch (s->mode) {
      case HP_HTML: out = hp_run_html(&st, pbody); break;
      case HP_CSV:  out = hp_run_csv(&st, pbody);  break;
      case HP_XML:  out = hp_run_xml(&st, pbody);  break;
      case HP_XLSX: {
        char xerr[256]; size_t xn = 0;
        if (xlsx_to_csv(hr.body, hr.body_len, s->xlsx_sheet_index, s->xlsx_sheet,
                        &xcsv, &xn, xerr, sizeof xerr) != 0) {
          /* Honest failure: an error line and no records, never a partial sheet. */
          fprintf(stderr, "[hp] %s: xlsx: %s\n", s->id, xerr);
          out = -1;
          break;
        }
        /* csv_skip_lines / csv_comment / title_keys apply to the converted text. */
        out = hp_run_csv(&st, xcsv);
        break;
      }
      case HP_JSON:
      default:      out = hp_run_json(&st, pbody); break;
    }
    free(xcsv);
    free(utf8);
    http_response_free(&hr);

    /* Shape notice (c): the row DECLARED where the title / identity live and
     * this page had records, none of which carried it. The records were still
     * emitted (under the fallback lists or their first scalar), so nothing is
     * lost yet — but the declaration is dead, and that is how a rename
     * upstream turns a keyed source into a mislabelled one without a single
     * count changing. Tallied per page, disclosed once per run. */
    if (st.pg_n > 0) {
      if (s->title_keys && *s->title_keys && !st.pg_title) {
        st.sh_title_pages++; st.sh_title_n += st.pg_n;
      }
      if (s->id_keys && *s->id_keys && !st.pg_id) {
        st.sh_id_pages++; st.sh_id_n += st.pg_n;
      }
    }
    st.pg_n = st.pg_title = st.pg_id = 0;

    /* A 200 whose body is an error report is a FAILED fetch that happened to
     * arrive with a success status, so it is treated as one: the walk stops
     * (paging an endpoint that just refused us only multiplies the refusal)
     * and the run reports what the equivalent HTTP status would have reported.
     * The rule immediately above is reused verbatim rather than reinvented —
     * >=500 on the first page is a hard error, anything else is an honest
     * empty — so a `{"error":{"code":404}}` behaves like an HTTP 404 and a
     * `{"error":{"code":500}}` like an HTTP 500. A document that carried no
     * numeric code at all cannot be classified, and an unclassifiable failure
     * is reported as an error rather than as a successful empty run: that is
     * the difference between "this source found nothing" and "this source was
     * not actually checked", and collapsing the two is what house rule 1 is
     * about. Either way nothing is STORED, which is the part that matters. */
    if (st.upstream_error) {
      if (page == 0 && (st.err_code < 0 || st.err_code >= 500)) hard_error = 1;
      break;
    }

    if (!paged || st.truncated) break;
    /* Stop when this page produced nothing new — that is the upstream telling
     * us the collection is exhausted. */
    if (st.page_records <= 0) break;

    char *nextp = NULL;
    if (st.next_url) {                       /* server-provided next link */
      if (s->next_tmpl && *s->next_tmpl) {
        /* The upstream handed back a cursor, not a URL. Build the continuation
         * request from the row's template — this is what makes OAI-PMH's
         * opaque resumptionToken pageable at all. */
        char *ev = hp_urlenc(st.next_url);
        nextp = hp_expand(s->next_tmpl, &vars, "v", ev ? ev : st.next_url);
        free(ev);
        free(st.next_url);
      } else {
        nextp = st.next_url;
      }
      st.next_url = NULL;
    } else if (path_paged) {                 /* path segment: /p/<n> */
      char nb[24];
      snprintf(nb, sizeof nb, "%ld", (long)page_start + page + 1);
      nextp = hp_expand(url, &vars, "page", nb);
    } else if (s->page_param) {              /* offset/page arithmetic */
      /* Offset-style when the row declares page_size (or the param is named
       * like an offset); page-number style otherwise. */
      int step = s->page_size > 0 ? s->page_size : st.page_records;
      int offset_style = (s->page_size > 0) || strstr(s->page_param, "offset") ||
                         strstr(s->page_param, "start") || strstr(s->page_param, "skip");
      long value = offset_style ? (long)(page_start + (page + 1) * step)
                                : (long)(page_start + page + 1);
      size_t n = strlen(url) + 64;
      nextp = malloc(n);
      if (nextp)
        snprintf(nextp, n, "%s%c%s=%ld", url, strchr(url, '?') ? '&' : '?',
                 s->page_param, value);
    }
    free(page_url);
    page_url = nextp;
    if (page + 1 >= page_max && page_url) st.truncated = 1;   /* ceiling bit */
  }
  free(page_url);
  free(st.next_url);
  html_seen_free(&st.hseen);

  /* `available` counts what the upstream handed over as records — array slots
   * that held nothing are reported separately rather than as a shortfall. */
  int real_available = st.available - st.empty - st.filtered - st.duplicate;
  if (real_available < out) real_available = out;
  if (out > 0 || st.available > 0) {
    char emptynote[128] = "";
    if (st.empty || st.refused || st.filtered || st.duplicate)
      snprintf(emptynote, sizeof emptynote,
               " [%d empty, %d duplicate, %d filtered out, %d refused by sink]",
               st.empty, st.duplicate, st.filtered, st.refused);
    fprintf(stderr, "[hp:%s] emitted %d of %d available across %d page(s)%s%s\n",
            s->id, out, real_available, st.page,
            st.truncated ? " (TRUNCATED)" : "", emptynote);
  }

  /* If anything WAS left on the table, say so in the data itself — a log line
   * nobody reads is not a disclosure. One upsert-keyed notice row per
   * (source, entity) records exactly what was not used and why, so a partial
   * result can never be mistaken for a complete one downstream. */
  /* Note the condition: a page-ceiling stop leaves an UNKNOWN remainder (we
   * never fetched those pages), so `available == out` there. Disclose whenever
   * the walk stopped early, not only when we can count what was missed. */
  if (st.truncated) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "source_id", s->id);
    cJSON_AddStringToObject(p, "query", vars.raw ? vars.raw : "");
    cJSON_AddNumberToObject(p, "records_used", out);
    cJSON_AddNumberToObject(p, "records_available", real_available);
    if (st.empty > 0)
      cJSON_AddNumberToObject(p, "empty_slots_skipped", st.empty);
    cJSON_AddNumberToObject(p, "pages_read", st.page);
    cJSON_AddBoolToObject(p, "more_pages_pending", real_available <= out);
    cJSON_AddNumberToObject(p, "declared_max_items", s->max_items);
    cJSON_AddStringToObject(p, "reason",
      (s->max_items > 0 && out >= s->max_items)
        ? "the row declares max_items and the upstream offered more"
        : "the page ceiling or a cancel stopped the walk");
    cJSON_AddStringToObject(p, "remedy",
      "raise max_items/page_max on this row (lib/hpengine.h) — see "
      "docs/SOURCE_EXHAUSTIVENESS.md");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);
    char key[320], title[256];
    snprintf(key, sizeof key, "%.150s|truncation:%.120s", s->id,
             vars.raw ? vars.raw : "");
    snprintf(title, sizeof title, "%s used %d of %d available records",
             s->id, out, real_available);
    intel_item note = {0};
    note.remote_key      = key;
    note.title           = title;
    note.lang            = "en";
    note.record_type     = "collector-truncation-notice";
    note.properties_json = pj ? pj : "{}";
    note.tags_json       = "[\"osint-search\",\"truncation-notice\"]";
    sink->emit(sink, &note);
    free(pj);
  }

  /* Schema drift, if any was measured above — one record per condition, in
   * ADDITION to whatever the walk found, never instead of it. */
  hp_shape_notices(&st, sink, &vars, out, page_max);

  for (int i = 0; i < nh; i++) free(hdr_store[i]);
  free(url);
  free(body);
  hp_vars_free(&vars);
  return hard_error ? -1 : 0;      /* honest empty is not an error */
}

void hp_register(const hp_source *specs, int n, source_def *defs) {
  for (int i = 0; i < n; i++) {
    const hp_source *s = &specs[i];
    if (!s->id || !s->url) continue;
    if (g_nspecs >= g_cspecs) {
      int nc = g_cspecs ? g_cspecs * 2 : 2048;
      const hp_source **ns = realloc((void *)g_specs, (size_t)nc * sizeof *ns);
      if (!ns) {
        /* Out of memory is the only reason a row is dropped now, and it is a
         * real failure rather than a configured limit — say so once per row. */
        fprintf(stderr, "[hp] OUT OF MEMORY registering '%s' at %d rows\n",
                s->id, g_nspecs);
        continue;
      }
      g_specs = ns;
      g_cspecs = nc;
    }
    g_specs[g_nspecs++] = s;
    defs[i] = (source_def){
      .id                  = s->id,
      .collector           = s->collector ? s->collector : "osint",
      .name                = s->name,
      .name_ja             = s->name_ja,
      .update_interval_sec = s->interval,
      .run                 = hp_run,
      .category            = s->category ? s->category : "investigation",
      .type                = s->type ? s->type : (s->mode == HP_HTML ? "scraped" : "api"),
      .url                 = s->portal,
      .description         = s->description,
      .license             = NULL,
      /* NULL (the default) still means "not a map layer" — right for the
       * entity-pivot services that dominate this registry. A row may now
       * opt in by declaring hp_source.layer (see hpengine.h); the field is
       * passed through untouched, never invented. */
      .layer               = s->layer,
      .free_tier           = s->free_tier,
    };
    registry_add(&defs[i]);
  }
}
