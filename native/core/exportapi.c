/* core/exportapi.c — see exportapi.h.
 *
 * The whole file is organised around one invariant: a row is never
 * materialised twice and the result set is never materialised at all. Every
 * kind resolves to (column descriptor array, prepared statement); every format
 * is a (prologue, per-row emitter, epilogue) triple over that pair. Adding a
 * kind is a column table plus a SQL builder; adding a format is three
 * functions. Nothing else in here knows what a "breach" or a "case" is.
 *
 * Two deliberate duplications, both documented rather than refactored because
 * the alternative was editing files this task must not touch:
 *
 *  1. The intel WHERE-clause builder mirrors intelapi.c's listItems() builder
 *     (intelapi.c:238-249) predicate for predicate, because that builder is
 *     static. The *filter struct* is shared verbatim (intel_items_query), so
 *     a new filter cannot appear on the endpoint without appearing in the
 *     struct and failing loudly here. If intelapi ever exports its builder,
 *     delete build_intel_sql() and call it.
 *
 *  2. b64url_decode() is copied from intelapi.c (also static there) so the
 *     export honours the exact same keyset cursor token the items endpoint
 *     hands out — an export can resume where a page left off.
 *
 * Number formatting for CSV uses SQLite's own text rendering of the stored
 * value (sqlite3_column_text on a REAL/INTEGER), which round-trips exactly and
 * avoids inventing a second float formatter that would disagree with the JSON
 * output for the same row.
 */
#include "exportapi.h"
#include "intelapi.h"
#include "audit.h"
#include "fts.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include "../third_party/mongoose.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* ── output stream: bounded batches through the caller's writer ─────────── */

#define OB_CAP 65536                     /* one flush == one HTTP chunk */

typedef struct {
  export_write_fn fn;
  void *ctx;
  size_t len;
  int broken;                            /* writer said stop; latch it */
  char buf[OB_CAP];
} ostream;

static int ob_flush(ostream *o) {
  if (o->broken) return 1;
  if (o->len == 0) return 0;
  if (o->fn(o->ctx, o->buf, o->len) != 0) { o->broken = 1; return 1; }
  o->len = 0;
  return 0;
}
static int ob_put(ostream *o, const char *p, size_t n) {
  if (o->broken) return 1;
  while (n) {
    size_t room = OB_CAP - o->len;
    if (room == 0) { if (ob_flush(o)) return 1; room = OB_CAP; }
    size_t take = n < room ? n : room;
    memcpy(o->buf + o->len, p, take);
    o->len += take; p += take; n -= take;
  }
  return 0;
}
static int ob_s(ostream *o, const char *s) { return ob_put(o, s, strlen(s)); }

/* ── column model ───────────────────────────────────────────────────────── */

typedef enum { CT_TEXT = 0, CT_NUM, CT_BOOL, CT_JSON, CT_AUTO } col_type;
typedef enum { GR_NONE = 0, GR_LAT, GR_LON, GR_GEOM } geo_role;
typedef struct { const char *name; col_type type; geo_role geo; } ecol;

/* Column order MUST match the SELECT list of the matching build_*_sql(). */
static const ecol COLS_INTEL[] = {
  {"uid",           CT_TEXT, GR_NONE}, {"source_id",     CT_TEXT, GR_NONE},
  {"title",         CT_TEXT, GR_NONE}, {"summary",       CT_TEXT, GR_NONE},
  {"link",          CT_TEXT, GR_NONE}, {"author",        CT_TEXT, GR_NONE},
  {"language",      CT_TEXT, GR_NONE}, {"published_at",  CT_TEXT, GR_NONE},
  {"fetched_at",    CT_TEXT, GR_NONE}, {"tags",          CT_JSON, GR_NONE},
  {"lat",           CT_NUM,  GR_LAT },  {"lon",          CT_NUM,  GR_LON },
  {"geom_source",   CT_TEXT, GR_NONE}, {"geom_at",       CT_TEXT, GR_NONE},
  {"record_type",   CT_TEXT, GR_NONE}, {"sub_source_id", CT_TEXT, GR_NONE},
  {"geometry",      CT_JSON, GR_GEOM},
};
static const ecol COLS_ENTITIES[] = {
  {"entity_id",     CT_TEXT, GR_NONE}, {"type",          CT_TEXT, GR_NONE},
  {"canonical",     CT_TEXT, GR_NONE}, {"norm_key",      CT_TEXT, GR_NONE},
  {"name_ja",       CT_TEXT, GR_NONE}, {"name_romaji",   CT_TEXT, GR_NONE},
  {"aliases",       CT_JSON, GR_NONE}, {"properties",    CT_JSON, GR_NONE},
  {"mention_count", CT_NUM,  GR_NONE}, {"first_seen_at", CT_TEXT, GR_NONE},
  {"last_seen_at",  CT_TEXT, GR_NONE},
};
/* Redacted breach projection == breach_adapter.c's field set. `hash` and every
 * secret path are absent by construction, not by filtering. */
static const ecol COLS_BREACH[] = {
  {"uid",           CT_TEXT, GR_NONE}, {"source_id",     CT_TEXT, GR_NONE},
  {"breach_name",   CT_TEXT, GR_NONE}, {"type",          CT_TEXT, GR_NONE},
  {"value",         CT_TEXT, GR_NONE}, {"has_secret",    CT_BOOL, GR_NONE},
  {"count",         CT_NUM,  GR_NONE}, {"first_seen",    CT_TEXT, GR_NONE},
};
static const ecol COLS_ALERT_EVENTS[] = {
  {"id",            CT_TEXT, GR_NONE}, {"rule_id",       CT_TEXT, GR_NONE},
  {"rule_name",     CT_TEXT, GR_NONE}, {"item_uid",      CT_TEXT, GR_NONE},
  {"matched_at",    CT_TEXT, GR_NONE}, {"delivered_channels", CT_JSON, GR_NONE},
  {"suppressed",    CT_BOOL, GR_NONE}, {"reason",        CT_TEXT, GR_NONE},
};

/* ── small shared helpers (house idioms) ────────────────────────────────── */

static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static void iso_now(char *buf, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000));
}
static void stamp_now(char *buf, size_t n) {          /* filename-safe */
  time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
  strftime(buf, n, "%Y%m%dT%H%M%SZ", &g);
}

/* base64url (no padding) → malloc'd bytes; NULL on an invalid char (bad
 * cursor is ignored, == intelapi.c). Copy of intelapi.c's private decoder. */
static char *b64url_decode(const char *in) {
  static signed char R[256];
  static int init = 0;
  if (!init) {
    for (int i = 0; i < 256; i++) R[i] = -1;
    const char *T =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (int i = 0; i < 64; i++) R[(unsigned char)T[i]] = (signed char)i;
    init = 1;
  }
  size_t len = strlen(in);
  char *out = malloc(len / 4 * 3 + 4);
  if (!out) return NULL;
  size_t o = 0; unsigned v = 0; int bits = 0;
  for (size_t i = 0; i < len; i++) {
    signed char d = R[(unsigned char)in[i]];
    if (d < 0) { free(out); return NULL; }
    v = (v << 6) | (unsigned)d; bits += 6;
    if (bits >= 8) { bits -= 8; out[o++] = (char)((v >> bits) & 0xFF); }
  }
  out[o] = 0;
  return out;
}

/* Wrap a user term as one quoted FTS5 phrase so it can never be parsed as
 * query syntax (== breach_adapter.c fts_phrase / breach_store.c). */
static void fts_phrase(const char *q, char *out, size_t cap) {
  size_t o = 0;
  if (cap < 3) { if (cap) out[0] = 0; return; }
  out[o++] = '"';
  for (const char *p = q; *p && o + 2 < cap - 1; p++) {
    if (*p == '"') out[o++] = '"';
    out[o++] = *p;
  }
  out[o++] = '"';
  out[o] = 0;
}

/* mg_http_get_var over a raw query string. Reusing mongoose's parser (rather
 * than a private percent-decoder) is what guarantees the export sees exactly
 * the values httpd.c's intel_items_run() sees for the same URL. */
static int qs_var(const char *qs, const char *name, char *out, size_t cap) {
  struct mg_str q = mg_str_n(qs ? qs : "", qs ? strlen(qs) : 0);
  out[0] = 0;
  return mg_http_get_var(&q, name, out, cap);
}

/* ── kinds / formats / plan caps ────────────────────────────────────────── */

typedef enum { K_BAD = -1, K_INTEL = 0, K_ENTITIES, K_BREACH, K_CASE,
               K_ALERT_EVENTS } ekind;
typedef enum { F_BAD = -1, F_CSV = 0, F_GEOJSON, F_JSON } efmt;

static ekind kind_parse(const char *k) {
  if (!k) return K_BAD;
  if (!strcmp(k, "intel"))        return K_INTEL;
  if (!strcmp(k, "entities"))     return K_ENTITIES;
  if (!strcmp(k, "breach"))       return K_BREACH;
  if (!strcmp(k, "case"))         return K_CASE;
  if (!strcmp(k, "alert_events")) return K_ALERT_EVENTS;
  return K_BAD;
}
static efmt fmt_parse(const char *f) {
  if (!f || !*f)                return F_JSON;      /* default */
  if (!strcmp(f, "csv"))        return F_CSV;
  if (!strcmp(f, "geojson"))    return F_GEOJSON;
  if (!strcmp(f, "json"))       return F_JSON;
  return F_BAD;
}
static const char *fmt_ext(efmt f) {
  return f == F_CSV ? "csv" : f == F_GEOJSON ? "geojson" : "json";
}
static const char *fmt_ctype(efmt f) {
  return f == F_CSV     ? "text/csv; charset=utf-8"
       : f == F_GEOJSON ? "application/geo+json"
                        : "application/json";
}

/* Row cap by plan. Unknown / empty plan is treated as `free`: the cap exists
 * to bound egress, so an unrecognised value must fail closed. -1 = unlimited. */
static long plan_row_cap(const char *plan) {
  if (!plan || !*plan)              return 1000;
  if (!strcmp(plan, "enterprise"))  return -1;
  if (!strcmp(plan, "team"))        return 250000;
  if (!strcmp(plan, "pro"))         return 50000;
  return 1000;                                       /* free + unknown */
}

/* role ordering: owner > admin > analyst > viewer */
static int role_rank(const char *r) {
  if (!r)                     return 0;
  if (!strcmp(r, "owner"))    return 4;
  if (!strcmp(r, "admin"))    return 3;
  if (!strcmp(r, "analyst"))  return 2;
  if (!strcmp(r, "viewer"))   return 1;
  return 0;
}

/* ── `cases` runtime column discovery ───────────────────────────────────── */

/* Any column whose name looks like it holds a credential is dropped from the
 * dynamic projection. A denylist is the wrong tool for a known schema, but
 * `cases` is owned by another module and may grow columns this file has never
 * seen; dropping a false positive costs a column, keeping a false negative
 * costs a secret. */
static int col_is_sensitive(const char *n) {
  static const char *DENY[] = { "secret", "password", "passwd", "token",
    "key", "hash", "cipher", "credential", "salt", "nonce" };
  char lo[128]; size_t i = 0;
  for (; n[i] && i < sizeof lo - 1; i++)
    lo[i] = (n[i] >= 'A' && n[i] <= 'Z') ? (char)(n[i] + 32) : n[i];
  lo[i] = 0;
  for (size_t k = 0; k < sizeof DENY / sizeof *DENY; k++)
    if (strstr(lo, DENY[k])) return 1;
  return 0;
}

/* PRAGMA table_info(cases) → malloc'd ecol array (names strdup'd), excluding
 * tenant_id (never echoed) and sensitive-looking columns. Returns 0 and
 * leaves *out NULL when the table is absent or has no tenant_id column —
 * either way the kind is unavailable. Caller frees with free_dyn_cols(). */
static int cases_columns(db_handle *db, ecol **out, int *nout) {
  *out = NULL; *nout = 0;
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, "PRAGMA table_info(cases)", -1, &s, NULL)
      != SQLITE_OK) return 0;
  ecol *cols = NULL; int n = 0, cap = 0, has_tenant = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *nm = (const char *)sqlite3_column_text(s, 1);
    if (!nm) continue;
    if (!strcmp(nm, "tenant_id")) { has_tenant = 1; continue; }
    if (col_is_sensitive(nm)) continue;
    if (strlen(nm) > 64) continue;         /* keeps the quoted identifier bounded */
    if (n == cap) {
      int ncap = cap ? cap * 2 : 16;
      ecol *tmp = realloc(cols, (size_t)ncap * sizeof *cols);
      if (!tmp) break;
      cols = tmp; cap = ncap;
    }
    cols[n].name = strdup(nm);
    cols[n].type = CT_AUTO;
    cols[n].geo  = GR_NONE;
    if (!cols[n].name) break;
    n++;
  }
  sqlite3_finalize(s);
  if (!has_tenant || n == 0) {
    for (int i = 0; i < n; i++) free((void *)cols[i].name);
    free(cols);
    return 0;
  }
  *out = cols; *nout = n;
  return 1;
}
static void free_dyn_cols(ecol *c, int n) {
  if (!c) return;
  for (int i = 0; i < n; i++) free((void *)c[i].name);
  free(c);
}

/* ── parsed request ─────────────────────────────────────────────────────── */

typedef struct { int is_int; const char *t; long long i; } bindv;

typedef struct {
  /* raw param buffers — bind targets live here for the whole call */
  char source[160], q[256], lang[16], since[40], until[40], rt[48],
       ssid[120], hg[8], tag[120], cursor[768], type[64], rule_id[64];
  char tenant[64];
  char cur_p[256], cur_u[512];          /* decoded intel keyset cursor */
  int  has_cursor;
  long long cur_id;                     /* breach cursor (row id) */
  char *segq;                           /* malloc'd segmented FTS query */
  char mq[600];                         /* quoted FTS5 phrase (breach) */
  intel_items_query iq;                 /* THE shared filter struct */
  cJSON *filters;                       /* echo → JSON meta + audit payload */
} eparams;

static void ep_free(eparams *p) {
  free(p->segq); p->segq = NULL;
  if (p->filters) { cJSON_Delete(p->filters); p->filters = NULL; }
}
static void ep_filter(eparams *p, const char *k, const char *v) {
  cJSON_AddStringToObject(p->filters, k, v);
}

/* Parse the query string for `kind`. Only the params that kind's own endpoint
 * defines are read; unknown params are ignored rather than guessed at. */
static void parse_params(ekind k, const char *qs, const tenant_ctx *t,
                         eparams *p) {
  memset(p, 0, sizeof *p);
  p->filters = cJSON_CreateObject();
  snprintf(p->tenant, sizeof p->tenant, "%s", t->tenant_id);

  if (k == K_INTEL) {
    /* EXACTLY httpd.c intel_items_run()'s parse, filling the same struct. */
    if (qs_var(qs, "source",        p->source, sizeof p->source) > 0) p->iq.source        = p->source;
    if (qs_var(qs, "q",             p->q,      sizeof p->q     ) > 0) p->iq.q             = p->q;
    if (qs_var(qs, "lang",          p->lang,   sizeof p->lang  ) > 0) p->iq.lang          = p->lang;
    if (qs_var(qs, "since",         p->since,  sizeof p->since ) > 0) p->iq.since         = p->since;
    if (qs_var(qs, "until",         p->until,  sizeof p->until ) > 0) p->iq.until         = p->until;
    if (qs_var(qs, "record_type",   p->rt,     sizeof p->rt    ) > 0) p->iq.record_type   = p->rt;
    if (qs_var(qs, "sub_source_id", p->ssid,   sizeof p->ssid  ) > 0) p->iq.sub_source_id = p->ssid;
    if (qs_var(qs, "has_geom",      p->hg,     sizeof p->hg    ) > 0) p->iq.has_geom      = p->hg;
    if (qs_var(qs, "tag",           p->tag,    sizeof p->tag   ) > 0) p->iq.tag           = p->tag;
    if (qs_var(qs, "cursor",        p->cursor, sizeof p->cursor) > 0) p->iq.cursor        = p->cursor;
    /* `limit` is accepted (so the same URL works) and ignored: the plan row
     * cap is the only limit an export honours. */

    if (p->iq.source)        ep_filter(p, "source",        p->iq.source);
    if (p->iq.q)             ep_filter(p, "q",             p->iq.q);
    if (p->iq.lang)          ep_filter(p, "lang",          p->iq.lang);
    if (p->iq.since)         ep_filter(p, "since",         p->iq.since);
    if (p->iq.until)         ep_filter(p, "until",         p->iq.until);
    if (p->iq.record_type)   ep_filter(p, "record_type",   p->iq.record_type);
    if (p->iq.sub_source_id) ep_filter(p, "sub_source_id", p->iq.sub_source_id);
    if (p->iq.has_geom)      ep_filter(p, "has_geom",      p->iq.has_geom);
    if (p->iq.tag)           ep_filter(p, "tag",           p->iq.tag);
    if (p->iq.cursor)        ep_filter(p, "cursor",        p->iq.cursor);

    if (p->iq.cursor && *p->iq.cursor) {              /* {"p":..,"u":..} */
      char *dec = b64url_decode(p->iq.cursor);
      if (dec) {
        cJSON *cj = cJSON_Parse(dec);
        free(dec);
        if (cj) {
          cJSON *jp = cJSON_GetObjectItem(cj, "p");
          cJSON *ju = cJSON_GetObjectItem(cj, "u");
          if (cJSON_IsString(jp) && cJSON_IsString(ju)) {
            snprintf(p->cur_p, sizeof p->cur_p, "%s", jp->valuestring);
            snprintf(p->cur_u, sizeof p->cur_u, "%s", ju->valuestring);
            p->has_cursor = 1;
          }
          cJSON_Delete(cj);
        }
      }
    }
    if (p->iq.q && *p->iq.q) p->segq = fts_segment(p->iq.q);
    return;
  }

  if (k == K_ENTITIES) {
    if (qs_var(qs, "q",     p->q,     sizeof p->q    ) > 0) ep_filter(p, "q",     p->q);
    if (qs_var(qs, "type",  p->type,  sizeof p->type ) > 0) {
      for (char *c = p->type; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
      ep_filter(p, "type", p->type);
    }
    if (qs_var(qs, "since", p->since, sizeof p->since) > 0) ep_filter(p, "since", p->since);
    if (qs_var(qs, "until", p->until, sizeof p->until) > 0) ep_filter(p, "until", p->until);
    if (p->q[0]) p->segq = fts_segment(p->q);
    return;
  }

  if (k == K_BREACH) {
    if (qs_var(qs, "source", p->source, sizeof p->source) > 0) ep_filter(p, "source", p->source);
    if (qs_var(qs, "q",      p->q,      sizeof p->q     ) > 0) {
      ep_filter(p, "q", p->q);
      fts_phrase(p->q, p->mq, sizeof p->mq);
    }
    if (qs_var(qs, "cursor", p->cursor, sizeof p->cursor) > 0) {
      ep_filter(p, "cursor", p->cursor);
      p->cur_id = atoll(p->cursor);                  /* opaque decimal id */
    }
    return;
  }

  if (k == K_ALERT_EVENTS) {
    if (qs_var(qs, "rule_id", p->rule_id, sizeof p->rule_id) > 0) ep_filter(p, "rule_id", p->rule_id);
    if (qs_var(qs, "since",   p->since,   sizeof p->since  ) > 0) ep_filter(p, "since",   p->since);
    if (qs_var(qs, "until",   p->until,   sizeof p->until  ) > 0) ep_filter(p, "until",   p->until);
    return;
  }
  /* K_CASE takes no filters beyond the mandatory tenant predicate. */
}

/* ── SQL builders ───────────────────────────────────────────────────────── */

#define BT(v) do { b[nb].is_int = 0; b[nb].t = (v);  nb++; } while (0)
#define BI(v) do { b[nb].is_int = 1; b[nb].i = (v);  nb++; } while (0)

/* Append a printf'd fragment to `w` at *wl, clamping. Every WHERE builder in
 * this file goes through it so no fragment can silently half-land. */
static void wapp(char *w, size_t cap, size_t *wl, const char *fmt, ...)
  __attribute__((format(printf, 4, 5)));
static void wapp(char *w, size_t cap, size_t *wl, const char *fmt, ...) {
  if (*wl >= cap - 1) return;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(w + *wl, cap - *wl, fmt, ap);
  va_end(ap);
  if (n > 0) *wl += (size_t)n;
  if (*wl >= cap) *wl = cap - 1;
}

/* Faithful mirror of intelapi.c listItems() predicates (intelapi.c:238-249),
 * plus the tenant guard the endpoint does not (yet) apply. Bare `?`
 * throughout: SQLite numbers them in textual order, which is exactly the
 * order binds are pushed here. */
static void build_intel_sql(const eparams *p, char *sql, size_t cap,
                            bindv *b, int *nbout) {
  int nb = 0;
  int has_q = p->segq && *p->segq;
  const char *T = has_q ? "intel_items." : "";   /* FTS join makes cols ambiguous */

  char w[4096]; size_t wl = 0; w[0] = 0;

  if (has_q) BT(p->segq);                        /* MATCH is always bind #1 */

  /* Tenant guard first: it must never be the fragment a clamp drops. */
  wapp(w, sizeof w, &wl, " AND %stenant_id IN (?,'legacy')", T);
  BT(p->tenant);

  if (p->iq.source) {
    wapp(w, sizeof w, &wl, " AND %ssource_id = ?", T);           BT(p->iq.source); }
  if (p->iq.since) {
    wapp(w, sizeof w, &wl, " AND COALESCE(%spublished_at,%sfetched_at) >= ?", T, T); BT(p->iq.since); }
  if (p->iq.until) {
    wapp(w, sizeof w, &wl, " AND COALESCE(%spublished_at,%sfetched_at) <= ?", T, T); BT(p->iq.until); }
  if (p->iq.lang) {
    wapp(w, sizeof w, &wl, " AND %slanguage = ?", T);            BT(p->iq.lang); }
  if (p->iq.tag) {
    wapp(w, sizeof w, &wl,
         " AND EXISTS (SELECT 1 FROM json_each(%stags) WHERE json_each.value = ?)", T);
    BT(p->iq.tag); }
  if (p->iq.record_type) {
    wapp(w, sizeof w, &wl, " AND %srecord_type = ?", T);         BT(p->iq.record_type); }
  if (p->iq.sub_source_id) {
    wapp(w, sizeof w, &wl, " AND %ssub_source_id = ?", T);       BT(p->iq.sub_source_id); }
  if (p->iq.has_geom && !strcmp(p->iq.has_geom, "yes"))
    wapp(w, sizeof w, &wl, " AND %slat IS NOT NULL", T);
  if (p->iq.has_geom && !strcmp(p->iq.has_geom, "no"))
    wapp(w, sizeof w, &wl, " AND %slat IS NULL", T);
  if (p->has_cursor) {                           /* same keyset token as the feed */
    wapp(w, sizeof w, &wl,
         " AND (COALESCE(%spublished_at,%sfetched_at) < ? OR "
         "(COALESCE(%spublished_at,%sfetched_at) = ? AND %suid > ?))",
         T, T, T, T, T);
    BT(p->cur_p); BT(p->cur_p); BT(p->cur_u);
  }

  const char *sel = has_q
    ? "intel_items.uid,intel_items.source_id,intel_items.title,"
      "intel_items.summary,intel_items.link,intel_items.author,"
      "intel_items.language,intel_items.published_at,intel_items.fetched_at,"
      "intel_items.tags,intel_items.lat,intel_items.lon,"
      "intel_items.geom_source,intel_items.geom_at,intel_items.record_type,"
      "intel_items.sub_source_id,intel_items.geometry"
    : "uid,source_id,title,summary,link,author,language,published_at,"
      "fetched_at,tags,lat,lon,geom_source,geom_at,record_type,"
      "sub_source_id,geometry";

  if (has_q) {
    snprintf(sql, cap,
      "SELECT %s FROM intel_items "
      "JOIN intel_items_fts ON intel_items_fts.uid = intel_items.uid "
      "WHERE intel_items_fts MATCH ?%s "
      "ORDER BY COALESCE(intel_items.published_at,intel_items.fetched_at) DESC,"
      " intel_items.uid ASC LIMIT ?", sel, w);
  } else {
    snprintf(sql, cap,
      "SELECT %s FROM intel_items WHERE%s "
      "ORDER BY COALESCE(published_at,fetched_at) DESC, uid ASC LIMIT ?",
      sel, w + 4);                                     /* drop leading " AND" */
  }
  *nbout = nb;
}

static void build_entities_sql(const eparams *p, char *sql, size_t cap,
                               bindv *b, int *nbout) {
  int nb = 0;
  int has_q = p->segq && *p->segq;
  const char *T = has_q ? "entities." : "";
  char w[1024]; size_t wl = 0; w[0] = 0;

  if (has_q) BT(p->segq);
  /* entities.tenant_id is NULL for the shared graph and set for tenant-private
   * entities; both are legitimately this tenant's to export, nothing else is. */
  wapp(w, sizeof w, &wl, " AND (%stenant_id IS NULL OR %stenant_id = ?)", T, T);
  BT(p->tenant);
  if (p->type[0])  { wapp(w, sizeof w, &wl, " AND %stype = ?", T);          BT(p->type); }
  if (p->since[0]) { wapp(w, sizeof w, &wl, " AND %slast_seen_at >= ?", T); BT(p->since); }
  if (p->until[0]) { wapp(w, sizeof w, &wl, " AND %slast_seen_at <= ?", T); BT(p->until); }

  const char *sel = has_q
    ? "entities.entity_id,entities.type,entities.canonical,entities.norm_key,"
      "entities.name_ja,entities.name_romaji,entities.aliases_json,"
      "entities.properties,entities.mention_count,entities.first_seen_at,"
      "entities.last_seen_at"
    : "entity_id,type,canonical,norm_key,name_ja,name_romaji,aliases_json,"
      "properties,mention_count,first_seen_at,last_seen_at";

  if (has_q) {
    /* Same join shape as entityapi_search() so export and search agree. */
    snprintf(sql, cap,
      "SELECT %s FROM entities_fts "
      "JOIN entities ON entities.entity_id = entities_fts.uid "
      "WHERE entities_fts MATCH ?%s "
      "ORDER BY entities.mention_count DESC, entities.entity_id ASC LIMIT ?",
      sel, w);
  } else {
    snprintf(sql, cap,
      "SELECT %s FROM entities WHERE%s "
      "ORDER BY mention_count DESC, entity_id ASC LIMIT ?", sel, w + 4);
  }
  *nbout = nb;
}

static void build_breach_sql(const eparams *p, char *sql, size_t cap,
                             bindv *b, int *nbout) {
  int nb = 0;
  int has_q = p->mq[0] != 0;
  /* Projection is the redacted adapter field set. b.hash is never selected. */
  static const char *SEL =
    "'breach:'||b.keyid, b.source_id, COALESCE(m.name,m.title,''), b.type,"
    " b.value, b.has_secret, b.count, b.first_seen";
  if (has_q) {
    snprintf(sql, cap,
      "SELECT %s FROM breach_items b "
      "JOIN breach_fts ON breach_fts.rowid = b.id "
      "LEFT JOIN breach_meta m ON m.breach_id = b.source_id "
      "WHERE b.source_id = ? AND breach_fts MATCH ? AND b.id > ? "
      "ORDER BY b.id ASC LIMIT ?", SEL);
    BT(p->source); BT(p->mq); BI(p->cur_id);
  } else {
    snprintf(sql, cap,
      "SELECT %s FROM breach_items b "
      "LEFT JOIN breach_meta m ON m.breach_id = b.source_id "
      "WHERE b.source_id = ? AND b.id > ? "
      "ORDER BY b.id ASC LIMIT ?", SEL);
    BT(p->source); BI(p->cur_id);
  }
  *nbout = nb;
}

static void build_alert_events_sql(const eparams *p, char *sql, size_t cap,
                                   bindv *b, int *nbout) {
  int nb = 0;
  char w[512]; size_t wl = 0; w[0] = 0;
  BT(p->tenant);
  if (p->rule_id[0]) { wapp(w, sizeof w, &wl, " AND e.rule_id = ?");     BT(p->rule_id); }
  if (p->since[0])   { wapp(w, sizeof w, &wl, " AND e.matched_at >= ?"); BT(p->since); }
  if (p->until[0])   { wapp(w, sizeof w, &wl, " AND e.matched_at <= ?"); BT(p->until); }
  snprintf(sql, cap,
    "SELECT e.id, e.rule_id, COALESCE(r.name,''), e.item_uid, e.matched_at,"
    " e.delivered_channels_json, e.suppressed, e.reason "
    "FROM alert_events e "
    "LEFT JOIN alert_rules r ON r.id = e.rule_id AND r.tenant_id = e.tenant_id "
    "WHERE e.tenant_id = ?%s "
    "ORDER BY e.matched_at DESC, e.id ASC LIMIT ?", w);
  *nbout = nb;
}

static void build_case_sql(const eparams *p, const ecol *cols, int ncol,
                           char *sql, size_t cap, bindv *b, int *nbout) {
  int nb = 0;
  size_t o = 0;
  wapp(sql, cap, &o, "SELECT ");
  for (int i = 0; i < ncol; i++) {
    /* Identifiers come from PRAGMA table_info, not from the request, but they
     * are still quoted (and embedded quotes doubled) so a column named through
     * any future migration can never become syntax. */
    char id[136]; size_t q = 0;
    id[q++] = '"';
    for (const char *c = cols[i].name; *c && q + 3 < sizeof id; c++) {
      if (*c == '"') id[q++] = '"';
      id[q++] = *c;
    }
    id[q++] = '"'; id[q] = 0;
    wapp(sql, cap, &o, "%s%s", i ? "," : "", id);
  }
  wapp(sql, cap, &o, " FROM cases WHERE tenant_id = ? ORDER BY 1 LIMIT ?");
  BT(p->tenant);
  *nbout = nb;
}

#undef BT
#undef BI

/* ── per-row emitters ───────────────────────────────────────────────────── */

static int csv_needs_quote(const char *v) {
  for (const char *p = v; *p; p++)
    if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') return 1;
  return 0;
}
/* RFC 4180 field: quote when it contains a comma, quote or newline; double
 * every embedded quote. NULL → an empty (unquoted) field. */
static int csv_field(ostream *o, const char *v) {
  if (!v) return o->broken;
  if (!csv_needs_quote(v)) return ob_s(o, v);
  if (ob_put(o, "\"", 1)) return 1;
  const char *start = v;
  for (const char *p = v; ; p++) {
    if (*p == '"') {
      if (ob_put(o, start, (size_t)(p - start) + 1)) return 1;
      if (ob_put(o, "\"", 1)) return 1;
      start = p + 1;
    } else if (!*p) {
      if (ob_put(o, start, (size_t)(p - start))) return 1;
      break;
    }
  }
  return ob_put(o, "\"", 1);
}

static int csv_row(ostream *o, sqlite3_stmt *s, const ecol *cols, int ncol) {
  for (int i = 0; i < ncol; i++) {
    if (i && ob_put(o, ",", 1)) return 1;
    if (cols[i].type == CT_BOOL) {
      if (sqlite3_column_type(s, i) == SQLITE_NULL) continue;
      if (ob_s(o, sqlite3_column_int(s, i) ? "true" : "false")) return 1;
      continue;
    }
    if (csv_field(o, ctext(s, i))) return 1;
  }
  return ob_s(o, "\r\n");
}

static void add_col(cJSON *obj, sqlite3_stmt *s, int i, const ecol *c) {
  int t = sqlite3_column_type(s, i);
  if (t == SQLITE_NULL) { cJSON_AddNullToObject(obj, c->name); return; }
  switch (c->type) {
    case CT_BOOL:
      cJSON_AddBoolToObject(obj, c->name, sqlite3_column_int(s, i) != 0);
      return;
    case CT_NUM:
      cJSON_AddNumberToObject(obj, c->name,
        t == SQLITE_INTEGER ? (double)sqlite3_column_int64(s, i)
                            : sqlite3_column_double(s, i));
      return;
    case CT_JSON: {
      cJSON *j = cJSON_Parse((const char *)sqlite3_column_text(s, i));
      cJSON_AddItemToObject(obj, c->name, j ? j : cJSON_CreateNull());
      return;
    }
    case CT_AUTO:
      if (t == SQLITE_INTEGER) {
        cJSON_AddNumberToObject(obj, c->name, (double)sqlite3_column_int64(s, i));
        return;
      }
      if (t == SQLITE_FLOAT) {
        cJSON_AddNumberToObject(obj, c->name, sqlite3_column_double(s, i));
        return;
      }
      break;
    case CT_TEXT:
    default:
      break;
  }
  cJSON_AddStringToObject(obj, c->name, (const char *)sqlite3_column_text(s, i));
}

static cJSON *row_object(sqlite3_stmt *s, const ecol *cols, int ncol,
                         int skip_geom) {
  cJSON *o = cJSON_CreateObject();
  for (int i = 0; i < ncol; i++) {
    if (skip_geom && cols[i].geo == GR_GEOM) continue;
    add_col(o, s, i, &cols[i]);
  }
  return o;
}

/* Row → Feature, or NULL when the row carries no geometry (caller counts the
 * skip). A GR_GEOM column passes its stored GeoJSON through verbatim; failing
 * that a GR_LAT/GR_LON pair becomes a Point — the same precedence
 * dataapi.c's row_to_feature() uses. */
static cJSON *row_feature(sqlite3_stmt *s, const ecol *cols, int ncol) {
  int ilat = -1, ilon = -1, igeom = -1;
  for (int i = 0; i < ncol; i++) {
    if (cols[i].geo == GR_LAT)  ilat = i;
    if (cols[i].geo == GR_LON)  ilon = i;
    if (cols[i].geo == GR_GEOM) igeom = i;
  }
  cJSON *geom = NULL;
  if (igeom >= 0 && sqlite3_column_type(s, igeom) != SQLITE_NULL) {
    const char *g = (const char *)sqlite3_column_text(s, igeom);
    if (g && *g) geom = cJSON_Parse(g);
  }
  if (!geom && ilat >= 0 && ilon >= 0 &&
      sqlite3_column_type(s, ilat) != SQLITE_NULL &&
      sqlite3_column_type(s, ilon) != SQLITE_NULL) {
    geom = cJSON_CreateObject();
    cJSON_AddStringToObject(geom, "type", "Point");
    cJSON *co = cJSON_AddArrayToObject(geom, "coordinates");
    cJSON_AddItemToArray(co, cJSON_CreateNumber(sqlite3_column_double(s, ilon)));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(sqlite3_column_double(s, ilat)));
  }
  if (!geom) return NULL;
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON_AddItemToObject(f, "geometry", geom);
  cJSON_AddItemToObject(f, "properties", row_object(s, cols, ncol, 1));
  return f;
}

/* The row cap as reported to clients: a number, or null for `enterprise`.
 * Emitting the internal -1 sentinel would read as "cap of minus one". */
static void add_row_cap(cJSON *o, long cap) {
  if (cap < 0) cJSON_AddNullToObject(o, "row_cap");
  else         cJSON_AddNumberToObject(o, "row_cap", (double)cap);
}

/* Print a cJSON node straight into the stream and free it. One row's worth of
 * allocation at a time — never the result set. */
static int ob_json(ostream *o, cJSON *node) {
  char *js = cJSON_PrintUnformatted(node);
  cJSON_Delete(node);
  if (!js) return ob_s(o, "null");
  int rc = ob_s(o, js);
  free(js);
  return rc;
}

/* ── entrypoints ────────────────────────────────────────────────────────── */

static int plan_fail(export_plan *out, int *status, int code, const char *msg) {
  *status = code;
  snprintf(out->error, sizeof out->error, "%s", msg);
  return 1;
}

int export_plan_request(db_handle *db, const tenant_ctx *t,
                        const char *kind, const char *format,
                        const char *query_string,
                        export_plan *out, int *status) {
  if (!out || !status) return 1;
  memset(out, 0, sizeof *out);
  *status = 200;
  if (!db || !db->h || !t) return plan_fail(out, status, 500, "server_error");

  ekind k = kind_parse(kind);
  if (k == K_BAD) return plan_fail(out, status, 400, "unknown_kind");
  efmt f = fmt_parse(format);
  if (f == F_BAD) return plan_fail(out, status, 400, "unknown_format");

  if (k == K_BREACH) {
    /* breach_adapter.h's rule: breach records are per-source only, never an
     * unfiltered feed. A bulk export must not be the way around that. */
    char src[160];
    if (qs_var(query_string, "source", src, sizeof src) <= 0)
      return plan_fail(out, status, 400, "source_required");
    if (role_rank(t->role) < 2)                       /* analyst or better */
      return plan_fail(out, status, 403, "forbidden");
  }
  if (k == K_CASE) {
    ecol *cols = NULL; int n = 0;
    if (!cases_columns(db, &cols, &n))
      return plan_fail(out, status, 501, "kind_unavailable");
    free_dyn_cols(cols, n);
  }

  snprintf(out->content_type, sizeof out->content_type, "%s", fmt_ctype(f));
  char stamp[32]; stamp_now(stamp, sizeof stamp);
  /* Both halves come from literal tables, never from the request, so the
   * Content-Disposition value cannot carry a quote or a CRLF. Echoing the raw
   * path segment here would be a header-injection hole. */
  static const char *SLUG[] = { "intel", "entities", "breach", "case",
                                "alert-events" };
  snprintf(out->filename, sizeof out->filename, "%s-%s.%s",
           SLUG[k], stamp, fmt_ext(f));
  return 0;
}

int export_run(db_handle *db, const tenant_ctx *t, const char *kind,
               const char *format, const char *query_string,
               export_write_fn write, void *write_ctx,
               long *out_rows, int *status) {
  int st = 200;
  if (out_rows) *out_rows = 0;
  if (status) *status = 200;
  if (!db || !db->h || !t || !write) { if (status) *status = 500; return 1; }

  export_plan plan;
  if (export_plan_request(db, t, kind, format, query_string, &plan, &st) != 0) {
    if (status) *status = st;
    return 1;                                          /* nothing written */
  }

  ekind k = kind_parse(kind);
  efmt  f = fmt_parse(format);
  const char *fname = fmt_ext(f);

  eparams p;
  parse_params(k, query_string, t, &p);

  /* columns: static per kind, or discovered for `case` */
  const ecol *cols = NULL; int ncol = 0;
  ecol *dyn = NULL; int ndyn = 0;
  switch (k) {
    case K_INTEL:        cols = COLS_INTEL;        ncol = (int)(sizeof COLS_INTEL / sizeof *COLS_INTEL); break;
    case K_ENTITIES:     cols = COLS_ENTITIES;     ncol = (int)(sizeof COLS_ENTITIES / sizeof *COLS_ENTITIES); break;
    case K_BREACH:       cols = COLS_BREACH;       ncol = (int)(sizeof COLS_BREACH / sizeof *COLS_BREACH); break;
    case K_ALERT_EVENTS: cols = COLS_ALERT_EVENTS; ncol = (int)(sizeof COLS_ALERT_EVENTS / sizeof *COLS_ALERT_EVENTS); break;
    case K_CASE:
      if (!cases_columns(db, &dyn, &ndyn)) {
        ep_free(&p);
        if (status) *status = 501;
        return 1;
      }
      cols = dyn; ncol = ndyn;
      break;
    default:
      ep_free(&p);
      if (status) *status = 400;
      return 1;
  }

  char sql[8192]; bindv b[24]; int nb = 0;
  switch (k) {
    case K_INTEL:        build_intel_sql(&p, sql, sizeof sql, b, &nb); break;
    case K_ENTITIES:     build_entities_sql(&p, sql, sizeof sql, b, &nb); break;
    case K_BREACH:       build_breach_sql(&p, sql, sizeof sql, b, &nb); break;
    case K_ALERT_EVENTS: build_alert_events_sql(&p, sql, sizeof sql, b, &nb); break;
    case K_CASE:         build_case_sql(&p, cols, ncol, sql, sizeof sql, b, &nb); break;
    default: sql[0] = 0; break;
  }

  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    free_dyn_cols(dyn, ndyn); ep_free(&p);
    if (status) *status = 500;
    return 1;
  }
  for (int i = 0; i < nb; i++) {
    if (b[i].is_int) sqlite3_bind_int64(s, i + 1, b[i].i);
    else             sqlite3_bind_text(s, i + 1, b[i].t, -1, SQLITE_TRANSIENT);
  }
  /* LIMIT cap+1: fetching one row past the cap is how truncation is *known*
   * rather than assumed. SQLite treats LIMIT -1 as unlimited (enterprise). */
  long cap = plan_row_cap(t->plan);
  sqlite3_bind_int64(s, nb + 1, cap < 0 ? -1 : (long long)cap + 1);

  ostream *o = calloc(1, sizeof *o);
  if (!o) {
    sqlite3_finalize(s); free_dyn_cols(dyn, ndyn); ep_free(&p);
    if (status) *status = 500;
    return 1;
  }
  o->fn = write; o->ctx = write_ctx;

  long rows = 0, skipped = 0, scanned = 0;
  int truncated = 0, aborted = 0;

  /* ── prologue ── */
  if (f == F_CSV) {
    ob_put(o, "\xEF\xBB\xBF", 3);          /* UTF-8 BOM: Excel + Japanese */
    for (int i = 0; i < ncol; i++) {
      if (i) ob_put(o, ",", 1);
      csv_field(o, cols[i].name);
    }
    ob_s(o, "\r\n");
  } else if (f == F_GEOJSON) {
    ob_s(o, "{\"type\":\"FeatureCollection\",\"features\":[");
  } else {
    ob_s(o, "{\"data\":[");
  }

  /* ── rows ── */
  while (!o->broken && sqlite3_step(s) == SQLITE_ROW) {
    /* The cap counts rows SCANNED, not rows emitted. GeoJSON drops rows with
     * no geometry, so counting emitted features would let the statement hit
     * LIMIT cap+1 with rows < cap and truncate silently — exactly the failure
     * mode this whole block exists to prevent. Seeing row cap+1 at all is the
     * proof that more exist. */
    scanned++;
    if (cap >= 0 && scanned > cap) { truncated = 1; break; }
    if (f == F_CSV) {
      csv_row(o, s, cols, ncol);
      rows++;
    } else if (f == F_GEOJSON) {
      cJSON *feat = row_feature(s, cols, ncol);
      if (!feat) { skipped++; continue; }
      if (rows) ob_put(o, ",", 1);
      ob_json(o, feat);
      rows++;
    } else {
      if (rows) ob_put(o, ",", 1);
      ob_json(o, row_object(s, cols, ncol, 0));
      rows++;
    }
  }
  sqlite3_finalize(s);
  if (o->broken) aborted = 1;

  /* ── epilogue ── */
  if (!o->broken) {
    char ts[40]; iso_now(ts, sizeof ts);
    if (f == F_CSV) {
      if (truncated) {
        /* RFC 4180 has no comment syntax; a visible trailing row is still far
         * better than a file that is silently short. */
        char note[224];
        snprintf(note, sizeof note,
          "# truncated=true; plan=\"%s\" row_cap=%ld reached; "
          "this export is incomplete\r\n",
          t->plan[0] ? t->plan : "free", cap);
        ob_s(o, note);
      }
    } else if (f == F_GEOJSON) {
      /* `properties` on a FeatureCollection is a GeoJSON foreign member
       * (RFC 7946 §6.1 permits them) — the agreed home for export metadata. */
      cJSON *m = cJSON_CreateObject();
      cJSON_AddStringToObject(m, "kind", kind);
      cJSON_AddStringToObject(m, "format", fname);
      cJSON_AddStringToObject(m, "generated_at", ts);
      cJSON_AddStringToObject(m, "plan", t->plan[0] ? t->plan : "free");
      add_row_cap(m, cap);
      cJSON_AddNumberToObject(m, "rows", (double)rows);
      cJSON_AddNumberToObject(m, "skipped_no_geometry", (double)skipped);
      cJSON_AddBoolToObject(m, "truncated", truncated);
      cJSON_AddItemToObject(m, "filters", cJSON_Duplicate(p.filters, 1));
      ob_s(o, "],\"properties\":");
      ob_json(o, m);
      ob_s(o, "}");
    } else {
      cJSON *env = cJSON_CreateObject();
      cJSON *page = cJSON_AddObjectToObject(env, "page");
      cJSON_AddNullToObject(page, "next_cursor");      /* export ran to end */
      cJSON_AddNullToObject(page, "limit");
      cJSON_AddNumberToObject(page, "total", (double)rows);
      cJSON *meta = cJSON_AddObjectToObject(env, "meta");
      cJSON_AddStringToObject(meta, "fetched_at", ts);
      cJSON_AddItemToObject(meta, "filters", cJSON_Duplicate(p.filters, 1));
      cJSON *ex = cJSON_AddObjectToObject(meta, "export");
      cJSON_AddStringToObject(ex, "kind", kind);
      cJSON_AddStringToObject(ex, "format", fname);
      cJSON_AddStringToObject(ex, "plan", t->plan[0] ? t->plan : "free");
      add_row_cap(ex, cap);
      cJSON_AddNumberToObject(ex, "rows", (double)rows);
      cJSON_AddBoolToObject(ex, "truncated", truncated);
      /* Splice the trailer in without re-printing `data`: print the envelope
       * and drop its outer braces. */
      char *js = cJSON_PrintUnformatted(env);
      cJSON_Delete(env);
      ob_s(o, "],");
      if (js) {
        size_t jl = strlen(js);
        if (jl >= 2) ob_put(o, js + 1, jl - 2);
        free(js);
      }
      ob_s(o, "}");
    }
    ob_flush(o);
    if (o->broken) aborted = 1;
  }

  /* ── audit AFTER the run so `rows` is the truth, and on the aborted path
   *    too: a half-delivered export is still an egress event. ── */
  {
    cJSON *pay = cJSON_CreateObject();
    cJSON_AddStringToObject(pay, "kind", kind);
    cJSON_AddStringToObject(pay, "format", fname);
    cJSON_AddStringToObject(pay, "plan", t->plan[0] ? t->plan : "free");
    add_row_cap(pay, cap);
    cJSON_AddNumberToObject(pay, "rows", (double)rows);
    cJSON_AddBoolToObject(pay, "truncated", truncated);
    cJSON_AddNumberToObject(pay, "skipped_no_geometry", (double)skipped);
    cJSON_AddBoolToObject(pay, "aborted", aborted);
    cJSON_AddItemToObject(pay, "filters", cJSON_Duplicate(p.filters, 1));
    char *pj = cJSON_PrintUnformatted(pay);
    cJSON_Delete(pay);
    char target[96];
    snprintf(target, sizeof target, "%s/%s", kind, fname);
    audit_write(db, t->tenant_id, t->user_id, "export.run", target, pj);
    free(pj);
  }

  int broken = o->broken;
  free(o);
  free_dyn_cols(dyn, ndyn);
  ep_free(&p);
  if (out_rows) *out_rows = rows;
  if (status) *status = 200;
  return broken ? 1 : 0;
}
