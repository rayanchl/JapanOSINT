/* lib/hpengine.c — see hpengine.h.
 *
 * Real fetch or honest empty, everywhere. The only values that ever reach the
 * sink are bytes that came back from the upstream endpoint in this run; there
 * is no default record, no cached sample, no "source found" placeholder. A row
 * that cannot fetch emits zero items and logs why. */
#include "hpengine.h"
#include "csv.h"
#include "htmlparse.h"   /* the one anchor scanner + dedupe set */
#include "pager.h"       /* shared with jsonlist: next-link + cursor walk */
#include "../core/httpclient.h"
#include "../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define HP_MAX_SOURCES   1024   /* exhaustive-ok: registry table size */
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

static const hp_source *g_specs[HP_MAX_SOURCES];
static int g_nspecs = 0;

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

/* Follow a dotted path; numeric segments index arrays. NULL when absent. */
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

/* First candidate from a comma-separated list that resolves to a non-empty
 * string in the flattened record. */
static const char *hp_pick(const cJSON *flat, const char *csv_keys,
                           const char *const *fallback) {
  char buf[512];
  if (csv_keys && *csv_keys) {
    snprintf(buf, sizeof buf, "%s", csv_keys);
    /* strtok_r: hp_pick runs for every record on every concurrent dispatch
     * worker, and strtok's cursor is a process-global (see jsonlist.c:246). */
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
      while (*tok == ' ') tok++;
      const cJSON *v = hp_flat_get(flat, tok);
      if (v && cJSON_IsString(v) && v->valuestring[0]) return v->valuestring;
    }
  }
  for (int i = 0; fallback && fallback[i]; i++) {
    const cJSON *v = hp_flat_get(flat, fallback[i]);
    if (v && cJSON_IsString(v) && v->valuestring[0]) return v->valuestring;
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
  int   available;        /* records the upstream actually offered           */
  int   truncated;        /* a declared cap or a cancel stopped the walk     */
  int   page;             /* 1-based page currently being read               */
  int   page_records;     /* records in the page just read (paging stop test) */
  char *next_url;         /* next-page URL from the response, when declared  */
  /* What the upstream itself said exists, from its own count field; -1 when it
   * said nothing. Without this a page-ceiling stop could only report the
   * records it had already counted, which reads as "nothing was missed". */
  long  declared_total;
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
  if (cJSON_GetArraySize(flat) == 0) return;

  if (s->filter_query && st->vars->raw) {
    char *txt = cJSON_PrintUnformatted(flat);
    int hit = txt ? hp_icontains(txt, st->vars->raw) : 0;
    free(txt);
    if (!hit) return;
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

  const char *title = hp_pick(flat, s->title_keys, TITLE_FALLBACK);
  const char *rkey  = hp_pick(flat, s->id_keys,    ID_FALLBACK);
  const char *date  = hp_pick(flat, s->date_keys,  DATE_FALLBACK);
  const char *body  = hp_pick(flat, s->body_keys,  NULL);
  const char *lnk   = hp_pick(flat, s->link_keys,  LINK_FALLBACK);

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

  /* A record with neither a title nor an id is shape noise, not a finding. */
  char titlebuf[512];
  if (!title) {
    if (!rkey) return;
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
  if (st->sink->emit(st->sink, &it) >= 0) st->emitted++;
  free(props);
}

/* ── per-mode drivers ────────────────────────────────────────────────────── */

static int hp_run_json(hp_run_state *st, const char *body) {
  const hp_source *s = st->s;
  cJSON *doc = cJSON_Parse(body);
  if (!doc) { fprintf(stderr, "[hp:%s] non-JSON body\n", s->id); return 0; }

  /* Ask the upstream what it has, once, on the first page. */
  if (st->declared_total < 0) st->declared_total = pager_declared_total(doc);

  cJSON *arr = NULL;
  if (s->array_path && *s->array_path) {
    cJSON *n = hp_path(doc, s->array_path);
    if (n && cJSON_IsArray(n)) arr = n;
    else if (n && cJSON_IsObject(n)) {                 /* single record */
      cJSON *flat = cJSON_CreateObject();
      hp_flatten(n, "", flat, 0);
      hp_emit_record(st, flat, 1);
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
  int deep_left = hp_detail_budget(s);

  if (!arr) {                                          /* root IS the record */
    /* Count it. The availability tally is the in-band "N of M" disclosure the
     * exhaustive-use rule requires, and this path used to skip it — a
     * single-object endpoint reported "emitted 1 of 0 available", which reads
     * as a discard when nothing was discarded. */
    st->available += 1;
    cJSON *flat = cJSON_CreateObject();
    hp_flatten(doc, "", flat, 0);
    hp_emit_record(st, flat, deep_left > 0);
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
    hp_emit_record(st, flat, deep_left > 0);
    if (st->emitted > before && deep_left > 0) deep_left--;
    cJSON_Delete(flat);
  }
  /* Hand the caller the next page URL, so the walk continues instead of
   * stopping at page 1. The row's own next_path wins — it names the field for
   * this upstream specifically — and where the row declared nothing we fall
   * back to the shared list of dialects every JSON API publishes a next link
   * under (lib/pager.c), which is what jsonlist has always done. */
  if (!st->next_url && s->next_path) {
    cJSON *nx = hp_path(doc, s->next_path);
    if (nx && cJSON_IsString(nx) && nx->valuestring[0])
      st->next_url = strdup(nx->valuestring);
  }
  if (!st->next_url && !s->next_path && !s->page_param)
    st->next_url = pager_next_link(doc);
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
  cJSON *rows = csv_parse(body, s->csv_no_header ? 0 : 1);
  if (!rows) return 0;
  int max = hp_record_cap(s);
  st->available += cJSON_GetArraySize(rows);
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
  html_seen seen = {0};
  html_anchor a;
  const char *p = html;
  while ((!max || st->emitted < max) && (p = html_anchor_next(p, &a)) != NULL) {
    char href[820];
    snprintf(href, sizeof href, "%.*s", (int)a.href_len, a.href);
    if (s->href_must && !strstr(href, s->href_must)) continue;
    if (a.text_len < 3) continue;
    if (s->filter_query && st->vars->raw &&
        !hp_icontains(a.text, st->vars->raw) && !hp_icontains(href, st->vars->raw))
      continue;
    st->available++;
    if (!html_seen_add(&seen, href)) continue;      /* already emitted */

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
  html_seen_free(&seen);
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
   * silent).
   *
   * A row that declares NEITHER is still walked, on the upstream's own
   * evidence only: a next link it published, or a cursor whose page-size
   * sibling is in the URL and whose page came back exactly full (lib/pager.c).
   * This used to be `page_max = 1` — one request, every later page discarded
   * without a word. jsonlist_emit_paged() fixed the same bug for the generated
   * collectors; leaving it unfixed here meant a row that gained a detail hop by
   * moving to this engine paid for it with every page after the first. */
  int out = 0, hard_error = 0;
  int page_max = s->page_max > 0 ? s->page_max : HP_PAGE_MAX_DEF;

  hp_run_state st = { .s = s, .ctx = ctx, .sink = sink, .vars = &vars,
                      .url = url, .emitted = 0, .declared_total = -1 };
  int page_start = s->page_start;
  if (!page_start && s->page_param && !strstr(s->page_param, "offset")) page_start = 1;

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
      case HP_JSON:
      default:      out = hp_run_json(&st, hr.body); break;
    }
    http_response_free(&hr);

    if (st.truncated) break;
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
      /* SET the parameter rather than appending it. Appending built
       * "…&page=2&page=3&page=4" as the walk went on and left it to the server
       * to pick a winner — on a server that takes the first, the walk re-read
       * page 2 until the ceiling. */
      nextp = pager_query_set(url, s->page_param, value);
    } else {
      /* Neither declared: the upstream's own cursor, on the rules in pager.h. */
      nextp = pager_advance(page_url, st.page_records);
    }
    free(page_url);
    page_url = nextp;
    if (page + 1 >= page_max && page_url) st.truncated = 1;   /* ceiling bit */
  }
  free(page_url);
  free(st.next_url);

  if (out > 0 || st.available > 0)
    fprintf(stderr, "[hp:%s] emitted %d of %d available across %d page(s)%s\n",
            s->id, out, st.available, st.page, st.truncated ? " (TRUNCATED)" : "");

  /* If anything WAS left on the table, say so in the data itself — a log line
   * nobody reads is not a disclosure. One upsert-keyed notice row per
   * (source, entity) records exactly what was not used and why, so a partial
   * result can never be mistaken for a complete one downstream. */
  /* Note the condition: a page-ceiling stop leaves an UNKNOWN remainder (we
   * never fetched those pages), so `available == out` there. Disclose whenever
   * the walk stopped early, not only when we can count what was missed. */
  if (st.truncated) {
    /* What "available" means here: the upstream's own declared total when it
     * published one, else the records we personally counted. The second is a
     * floor, not a total — on a ceiling stop we never fetched the rest — so it
     * is labelled as such rather than presented as the full size. */
    long avail = st.declared_total > (long)st.available ? st.declared_total
                                                       : (long)st.available;
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "source_id", s->id);
    cJSON_AddStringToObject(p, "query", vars.raw ? vars.raw : "");
    cJSON_AddNumberToObject(p, "records_used", out);
    cJSON_AddNumberToObject(p, "records_available", (double)avail);
    cJSON_AddStringToObject(p, "records_available_basis",
      st.declared_total >= 0 ? "upstream declared this total"
                             : "records counted so far — the unread pages are not included");
    cJSON_AddNumberToObject(p, "pages_read", st.page);
    cJSON_AddBoolToObject(p, "more_pages_pending", avail <= (long)out);
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
    snprintf(title, sizeof title, "%s used %d of %ld available records",
             s->id, out, avail);
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
    if (g_nspecs >= HP_MAX_SOURCES) {
      fprintf(stderr, "[hp] OVERFLOW: dropped '%s' (cap %d)\n", s->id, HP_MAX_SOURCES);
      continue;
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
