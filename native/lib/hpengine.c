/* lib/hpengine.c — see hpengine.h.
 *
 * Real fetch or honest empty, everywhere. The only values that ever reach the
 * sink are bytes that came back from the upstream endpoint in this run; there
 * is no default record, no cached sample, no "source found" placeholder. A row
 * that cannot fetch emits zero items and logs why. */
#include "hpengine.h"
#include "csv.h"
#include "htmlparse.h"   /* the one anchor scanner + dedupe set */
#include "../core/httpclient.h"
#include "../third_party/cJSON.h"
#include <ctype.h>
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

/* Densest array of objects anywhere in the document (depth-limited). Lets a
 * row work without hardcoding a envelope key — and keeps working when the
 * upstream renames it. */
static void hp_find_array(cJSON *node, int depth, cJSON **best, int *best_n) {
  if (!node || depth > 5) return;
  if (cJSON_IsArray(node)) {
    int n = 0, objs = 0;
    cJSON *it;
    cJSON_ArrayForEach(it, node) { n++; if (cJSON_IsObject(it)) objs++; }
    if (objs > 0 && n > *best_n) { *best = node; *best_n = n; }
  }
  cJSON *ch;
  cJSON_ArrayForEach(ch, node)
    if (cJSON_IsObject(ch) || cJSON_IsArray(ch)) hp_find_array(ch, depth + 1, best, best_n);
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
 * whose last segment matches (so "siege.nom" answers a "nom" request). */
static const cJSON *hp_flat_get(const cJSON *flat, const char *name) {
  if (!flat || !name || !*name) return NULL;
  const cJSON *ex = cJSON_GetObjectItem(flat, name);
  if (ex) return ex;
  const cJSON *it;
  cJSON_ArrayForEach(it, flat) {
    if (!it->string) continue;
    const char *dot = strrchr(it->string, '.');
    const char *last = dot ? dot + 1 : it->string;
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
                        HP_HTTP_TIMEOUT, 0, &hr);
  if (rc == 0 && hr.status == 200 && hr.body) {
    cJSON *doc = cJSON_Parse(hr.body);
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
  const char *title = hp_pick_s(flat, s->title_keys, TITLE_FALLBACK, sc_t, sizeof sc_t);
  const char *rkey  = hp_pick_s(flat, s->id_keys,    ID_FALLBACK,    sc_r, sizeof sc_r);
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

  char keybuf[320];
  if (rkey) snprintf(keybuf, sizeof keybuf, "%.180s|%.120s", s->id, rkey);
  else      snprintf(keybuf, sizeof keybuf, "%.180s|%.120s", s->id, title);

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

static int hp_run_json(hp_run_state *st, const char *body) {
  const hp_source *s = st->s;
  cJSON *doc = cJSON_Parse(body);
  if (!doc) { fprintf(stderr, "[hp:%s] non-JSON body\n", s->id); return 0; }

  cJSON *arr = NULL;
  if (s->array_path && *s->array_path) {
    cJSON *n = hp_path(doc, s->array_path);
    if (n && cJSON_IsArray(n)) arr = n;
    else if (n && cJSON_IsObject(n)) {                 /* single record */
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
  if (!arr) {
    int best_n = 0;
    hp_find_array(doc, 0, &arr, &best_n);
  }
  int max = hp_record_cap(s);

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

  st->available += cJSON_GetArraySize(arr);
  cJSON *rec;
  cJSON_ArrayForEach(rec, arr) {
    if (max && st->emitted >= max) { st->truncated = 1; break; }
    if (st->ctx->cancel && *st->ctx->cancel) { st->truncated = 1; break; }
    cJSON *flat = cJSON_CreateObject();
    if (cJSON_IsObject(rec) || cJSON_IsArray(rec)) hp_flatten(rec, "", flat, 0);
    else if (cJSON_IsString(rec) && rec->valuestring[0])
      cJSON_AddStringToObject(flat, "value", rec->valuestring);
    int before = st->emitted;
    hp_emit_record(st, flat, st->deep_left > 0);
    if (st->emitted > before && st->deep_left > 0) st->deep_left--;
    cJSON_Delete(flat);
  }
  /* Hand the caller the next page URL when the row declared one, so the walk
   * continues instead of stopping at page 1. */
  if (s->next_path && !st->next_url) {
    cJSON *nx = hp_path(doc, s->next_path);
    if (nx && cJSON_IsString(nx) && nx->valuestring[0])
      st->next_url = strdup(nx->valuestring);
  }
  st->page_records = arr ? cJSON_GetArraySize(arr) : 0;
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
  char delim = ',';
  if (s->csv_delim && s->csv_delim[0]) {
    if      (!strcmp(s->csv_delim, "tab")  || !strcmp(s->csv_delim, "\\t")) delim = '\t';
    else if (!strcmp(s->csv_delim, "pipe")) delim = '|';
    else if (!strcmp(s->csv_delim, "semi")) delim = ';';
    else                                    delim = s->csv_delim[0];
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
  cJSON *rows = csv_parse_d(body, s->csv_no_header ? 0 : 1, delim);
  free(stripped);
  if (!rows) return 0;
  int max = hp_record_cap(s);
  st->available += cJSON_GetArraySize(rows);
  /* The page walk stops on `page_records <= 0`. Leaving it at 0 here meant a
   * paged CSV row read page 1 and silently discarded every page after it —
   * with no truncation notice either, since nothing set `truncated`. */
  st->page_records = cJSON_GetArraySize(rows);
  cJSON *row;
  cJSON_ArrayForEach(row, rows) {
    if (max && st->emitted >= max) { st->truncated = 1; break; }
    if (st->ctx->cancel && *st->ctx->cancel) { st->truncated = 1; break; }
    cJSON *flat = cJSON_CreateObject();
    if (s->csv_no_header && cJSON_IsArray(row)) {
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
    hp_emit_record(st, flat, 0);
    cJSON_Delete(flat);
  }
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

/* Minimal entity decode, in place. Enough for the five predefined entities and
 * numeric refs, which is what these registers actually emit. */
static void hp_xml_unescape(char *s) {
  char *w = s;
  for (char *r = s; *r; ) {
    if (*r != '&') { *w++ = *r++; continue; }
    if      (!strncmp(r, "&amp;", 5))  { *w++ = '&';  r += 5; }
    else if (!strncmp(r, "&lt;", 4))   { *w++ = '<';  r += 4; }
    else if (!strncmp(r, "&gt;", 4))   { *w++ = '>';  r += 4; }
    else if (!strncmp(r, "&quot;", 6)) { *w++ = '"';  r += 6; }
    else if (!strncmp(r, "&apos;", 6)) { *w++ = '\''; r += 6; }
    else if (r[1] == '#') {
      char *end = NULL; long v = strtol(r + 2 + (r[2] == 'x' || r[2] == 'X'),
                                        &end, (r[2] == 'x' || r[2] == 'X') ? 16 : 10);
      if (end && *end == ';' && v > 0 && v < 128) { *w++ = (char)v; r = end + 1; }
      else *w++ = *r++;
    }
    else *w++ = *r++;
  }
  *w = 0;
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
    if (*p == '/' || *p == '?' || *p == '!') {            /* close / decl / comment */
      const char *gt = memchr(p, '>', (size_t)(end - p));
      if (!gt) return;
      p = gt + 1;
      continue;
    }
    size_t nl = hp_xml_name(p, name, sizeof name);
    if (!nl) return;
    const char *gt = memchr(p, '>', (size_t)(end - p));
    if (!gt) return;
    if (gt > p && gt[-1] == '/') { p = gt + 1; continue; }  /* self-closing */
    char close[100];
    int cl = snprintf(close, sizeof close, "</%s>", name);
    const char *vs = gt + 1, *ve = vs;
    /* find this element's matching close, allowing one level of same-name nest */
    int nest = 1;
    while (ve < end) {
      const char *lt = memchr(ve, '<', (size_t)(end - ve));
      if (!lt) { ve = end; break; }
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
    if (prefix && *prefix) snprintf(key, sizeof key, "%s.%s", prefix, name);
    else                   snprintf(key, sizeof key, "%s", name);

    if (memchr(vs, '<', (size_t)(ve - vs))) {
      hp_xml_flatten(vs, ve, key, flat, depth + 1);        /* nested element */
    } else {
      size_t vl = (size_t)(ve - vs);
      while (vl && isspace((unsigned char)*vs)) { vs++; vl--; }
      while (vl && isspace((unsigned char)vs[vl - 1])) vl--;
      if (vl && vl < 4096 && !cJSON_GetObjectItem(flat, key)) {
        char *val = (char *)malloc(vl + 1);
        if (val) {
          memcpy(val, vs, vl); val[vl] = 0;
          hp_xml_unescape(val);
          cJSON_AddStringToObject(flat, key, val);
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
  for (const char *p = body; (p = strchr(p, '<')) != NULL; ) {
    p++;
    if (*p == '/' || *p == '?' || *p == '!') continue;
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
  char open[100], close[100];
  int ol = snprintf(open, sizeof open, "<%s", tag);
  int cl = snprintf(close, sizeof close, "</%s>", tag);
  int max = hp_record_cap(s);
  const char *p = body;
  int found = 0;
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
    if (!max || st->emitted < max) {
      cJSON *flat = cJSON_CreateObject();
      hp_xml_flatten(gt + 1, endrec, "", flat, 0);
      if (cJSON_GetArraySize(flat) > 0)
        hp_emit_record(st, flat, st->deep_left > 0);
      cJSON_Delete(flat);
    }
    p = endrec + cl;
  }
  st->page_records = found;
  return st->emitted;
}

/* Real anchors out of a real listing page. JS-rendered or anti-bot pages
 * simply yield nothing. */
static int hp_run_html(hp_run_state *st, const char *html) {
  const hp_source *s = st->s;
  int max = hp_record_cap(s);
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
    if (a.text_len < 3) continue;
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

    char link[900];
    if (!strncmp(href, "http", 4)) snprintf(link, sizeof link, "%s", href);
    else snprintf(link, sizeof link, "%s%s", s->base ? s->base : "", href);

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
  int paged = (s->next_path || s->page_param) ? 1 : 0;
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
      s->page_param && !strstr(s->page_param, "offset")) page_start = 1;

  char *page_url = strdup(url);
  for (int page = 0; page < page_max && page_url; page++) {
    st.page = page + 1;
    st.url  = page_url;
    st.page_records = 0;

    http_response hr = {0};
    int rc = http_request(ctx->http, body ? "POST" : "GET", page_url, hdrs,
                          body, body ? strlen(body) : 0, HP_HTTP_TIMEOUT, 1, &hr);
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
    switch (s->mode) {
      case HP_HTML: out = hp_run_html(&st, hr.body); break;
      case HP_CSV:  out = hp_run_csv(&st, hr.body);  break;
      case HP_XML:  out = hp_run_xml(&st, hr.body);  break;
      case HP_JSON:
      default:      out = hp_run_json(&st, hr.body); break;
    }
    http_response_free(&hr);

    if (!paged || st.truncated) break;
    /* Stop when this page produced nothing new — that is the upstream telling
     * us the collection is exhausted. */
    if (st.page_records <= 0) break;

    char *nextp = NULL;
    if (st.next_url) {                       /* server-provided next link */
      nextp = st.next_url;
      st.next_url = NULL;
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
      .layer               = NULL,              /* services never map-layer */
      .free_tier           = s->free_tier,
    };
    registry_add(&defs[i]);
  }
}
