#include "intelapi.h"
#include "source_registry.h"
#include "../source.h"          /* registry_all/registry_count/registry_get */
#include "fts.h"
#include "breach_meta.h"
#include "breach_adapter.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include "source_trust.h"

/* explicit column order shared by item-by-uid and list-items so rows can be
 * read by fixed index (== rowToItem field access in intelStore.js). */
#define ITEM_COLS \
  "uid,source_id,title,body,summary,link,author,language," \
  "published_at,fetched_at,tags,properties,keywords,lat,lon,geom_source," \
  "geom_at,record_type,sub_source_id,cluster_id"

/* Same columns, same order, table-qualified — for the FTS branch where the
 * join to intel_items_fts makes title/body/summary/keywords ambiguous. The
 * fixed-index reads in row_to_item() stay valid (identical order). */
#define ITEM_COLS_Q \
  "intel_items.uid,intel_items.source_id,intel_items.title,intel_items.body," \
  "intel_items.summary,intel_items.link,intel_items.author," \
  "intel_items.language,intel_items.published_at,intel_items.fetched_at," \
  "intel_items.tags,intel_items.properties,intel_items.keywords," \
  "intel_items.lat,intel_items.lon,intel_items.geom_source," \
  "intel_items.geom_at,intel_items.record_type,intel_items.sub_source_id,"   "intel_items.cluster_id"

/* JSON.parse(text) → cJSON, or fallback. Node does JSON.parse then the value
 * is re-serialized by JSON.stringify; cJSON parse+print round-trips
 * identically (same key order, value formatting — validated in P5). */
static cJSON *parse_or(cJSON *fallback, const char *txt) {
  if (txt && *txt) { cJSON *j = cJSON_Parse(txt); if (j) { cJSON_Delete(fallback); return j; } }
  return fallback;
}
static const char *ctext(sqlite3_stmt *s, int i) {
  return (sqlite3_column_type(s, i) == SQLITE_NULL)
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static void add_str_or_null(cJSON *o, const char *k, const char *v) {
  cJSON_AddItemToObject(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}

/* Node's new Date().toISOString(): YYYY-MM-DDTHH:MM:SS.mmmZ */
static void iso_now(char *buf, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  /* The %0Nd widths are minimums, not caps: to -Wformat-truncation
   * `tm_year + 1900` is a plain int worth up to 11 characters, so this
   * fixed 24-char stamp "may be truncated". The modulos are identity for
   * every value gmtime_r can return and make the 24 provable, not merely
   * true. */
  snprintf(buf, n, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
           (unsigned)(tm.tm_year + 1900) % 10000u, (unsigned)(tm.tm_mon + 1) % 100u,
           (unsigned)tm.tm_mday % 100u, (unsigned)tm.tm_hour % 100u,
           (unsigned)tm.tm_min % 100u, (unsigned)tm.tm_sec % 100u,
           (unsigned)(tv.tv_usec / 1000) % 1000u);
}

/* Buffer.from(str,'utf8').toString('base64url') — no padding, +/ → -_ */
static char *b64url(const char *in) {
  static const char T[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  size_t len = strlen(in);
  char *out = malloc(((len + 2) / 3) * 4 + 1);
  if (!out) return NULL;   /* the sibling copies in aoiapi.c and timelineapi.c already check */
  size_t o = 0;
  for (size_t i = 0; i < len; i += 3) {
    unsigned a = (unsigned char)in[i];
    unsigned b = i + 1 < len ? (unsigned char)in[i + 1] : 0;
    unsigned c = i + 2 < len ? (unsigned char)in[i + 2] : 0;
    unsigned v = (a << 16) | (b << 8) | c;
    out[o++] = T[(v >> 18) & 63];
    out[o++] = T[(v >> 12) & 63];
    if (i + 1 < len) out[o++] = T[(v >> 6) & 63];
    if (i + 2 < len) out[o++] = T[v & 63];
  }
  out[o] = 0;
  return out;
}

/* Inverse of b64url() — base64url (no padding) → malloc'd NUL-terminated
 * bytes, or NULL on an invalid char (treated like Node's bad-cursor catch:
 * caller ignores the cursor). Single-threaded httpd loop → static table ok. */
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

/* Build the rowToItem(row,{full}) object in EXACT Node key order. The stmt
 * must have stepped to a row selecting ITEM_COLS. */
static cJSON *row_to_item(sqlite3_stmt *s, int full) {
  const char *c_uid=ctext(s,0), *c_src=ctext(s,1), *c_title=ctext(s,2),
    *c_body=ctext(s,3), *c_summary=ctext(s,4), *c_link=ctext(s,5),
    *c_author=ctext(s,6), *c_lang=ctext(s,7), *c_pub=ctext(s,8),
    *c_fetch=ctext(s,9), *c_tags=ctext(s,10), *c_props=ctext(s,11),
    *c_kw=ctext(s,12), *c_gsrc=ctext(s,15), *c_gat=ctext(s,16),
    *c_rt=ctext(s,17), *c_sub=ctext(s,18), *c_cl=ctext(s,19);
  int latnull = sqlite3_column_type(s,13)==SQLITE_NULL;
  int lonnull = sqlite3_column_type(s,14)==SQLITE_NULL;
  double lat = sqlite3_column_double(s,13), lon = sqlite3_column_double(s,14);

  cJSON *tags = parse_or(cJSON_CreateArray(), c_tags);
  cJSON *props = parse_or(cJSON_CreateObject(), c_props);
  cJSON *kw = (c_kw && *c_kw) ? cJSON_Parse(c_kw) : NULL;

  /* buildProvenance */
  const src_meta *m = src_meta_get(c_src);
  cJSON *lic = cJSON_GetObjectItem(props, "license");
  if (!lic || cJSON_IsNull(lic)) lic = cJSON_GetObjectItem(props, "license_id");
  if (!lic || cJSON_IsNull(lic)) lic = cJSON_GetObjectItem(props, "license_name");
  const char *licstr = (lic && cJSON_IsString(lic)) ? lic->valuestring
                       : (m && m->license ? m->license : NULL);
  char licbuf[128] = {0};
  if (!licstr && lic && cJSON_IsNumber(lic)) { snprintf(licbuf,sizeof licbuf,"%g",lic->valuedouble); licstr=licbuf; }
  cJSON *conf = cJSON_GetObjectItem(props, "confidence");
  if (!conf) conf = cJSON_GetObjectItem(props, "_confidence");

  cJSON *pv = cJSON_CreateObject();
  add_str_or_null(pv,"source_id", c_src);
  cJSON_AddItemToObject(pv,"source_name", cJSON_CreateString(m&&m->name?m->name:(c_src?c_src:"")));
  add_str_or_null(pv,"source_name_ja", m?m->name_ja:NULL);
  add_str_or_null(pv,"category", m?m->category:NULL);
  add_str_or_null(pv,"collection_method", m?m->type:NULL);
  add_str_or_null(pv,"source_url", m?m->url:NULL);
  add_str_or_null(pv,"item_url", c_link);
  add_str_or_null(pv,"sub_source_id", c_sub);
  add_str_or_null(pv,"fetched_at", c_fetch);
  add_str_or_null(pv,"published_at", c_pub);
  add_str_or_null(pv,"license", licstr);
  cJSON_AddItemToObject(pv,"confidence",
     (conf && cJSON_IsNumber(conf)) ? cJSON_CreateNumber(conf->valuedouble) : cJSON_CreateNull());

  cJSON *out = cJSON_CreateObject();
  add_str_or_null(out,"uid", c_uid);
  add_str_or_null(out,"source_id", c_src);
  add_str_or_null(out,"title", c_title);
  add_str_or_null(out,"summary", c_summary);
  add_str_or_null(out,"link", c_link);
  add_str_or_null(out,"author", c_author);
  add_str_or_null(out,"language", c_lang);
  add_str_or_null(out,"published_at", c_pub);
  add_str_or_null(out,"fetched_at", c_fetch);
  cJSON_AddItemToObject(out,"tags", tags);
  cJSON_AddItemToObject(out,"lat", latnull?cJSON_CreateNull():cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(out,"lon", lonnull?cJSON_CreateNull():cJSON_CreateNumber(lon));
  add_str_or_null(out,"geom_source", c_gsrc);
  add_str_or_null(out,"geom_at", c_gat);
  add_str_or_null(out,"record_type", c_rt);
  add_str_or_null(out,"sub_source_id", c_sub);
  /* cluster_id (simhash.c near-duplicate cluster; the uid of the cluster's
   * earliest member) only when the row has been fingerprinted and clustered:
   * a row that is not in any cluster carries no key rather than a null, so
   * the pre-simhash contract fixtures stay byte-identical. */
  if (c_cl && *c_cl) cJSON_AddStringToObject(out,"cluster_id", c_cl);
  cJSON_AddItemToObject(out,"provenance", pv);
  if (full) {
    /* getItem → full:true : body, properties, [keywords] */
    add_str_or_null(out,"body", c_body);
    cJSON_AddItemToObject(out,"properties", props);
    if (kw) cJSON_AddItemToObject(out,"keywords", kw);
  } else {
    /* list path: properties only if non-empty, then keywords if present */
    if (cJSON_GetArraySize(props) > 0) cJSON_AddItemToObject(out,"properties", props);
    else cJSON_Delete(props);
    if (kw) cJSON_AddItemToObject(out,"keywords", kw);
  }
  return out;
}

char *intelapi_item_by_uid_tenant(db_handle *db, const char *uid,
                                  const char *tenant) {
  /* Breach records are served from breach_items via the adapter, not intel_items. */
  if (uid && strncmp(uid, "breach:", 7) == 0)
    return breach_adapter_item_by_uid(db, uid);
  /* Two statements rather than one with a `?2 IS NULL OR …` disjunction: that
   * form is a filter SQLite cannot use the uid index through as cleanly, and it
   * makes "unscoped" a value the query has to reason about instead of a
   * decision the caller already made. A miss under the tenant predicate is
   * indistinguishable from a missing uid on purpose — the caller's 404 must not
   * confirm that another tenant holds that row. */
  static const char *Q_ANY =
    "SELECT " ITEM_COLS " FROM intel_items WHERE uid=?1";
  static const char *Q_TENANT =
    "SELECT " ITEM_COLS " FROM intel_items WHERE uid=?1"
    " AND tenant_id IN (?2,'legacy')";
  int scoped = tenant && *tenant;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, scoped ? Q_TENANT : Q_ANY, -1, &s, NULL)
      != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
  if (scoped) sqlite3_bind_text(s, 2, tenant, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) { sqlite3_finalize(s); return NULL; }
  cJSON *out = row_to_item(s, 1);
  sqlite3_finalize(s);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env,"data", out);
  char *js = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return js;
}

char *intelapi_item_by_uid(db_handle *db, const char *uid) {
  return intelapi_item_by_uid_tenant(db, uid, NULL);
}

/* ── ranking support ──────────────────────────────────────────────────────
 *
 * bm25() weights, in intel_items_fts COLUMN ORDER (schema.sql:320 /
 * fts_schema.h): uid(UNINDEXED)=0, title=10, body=1, summary=3, keywords=2,
 * link=1, author=1, tags=2, props=1. A hit in the title outranks the same
 * term buried in a 40 KB body; keywords/tags are curated so they count double.
 * FTS5's bm25() is NEGATIVE for better matches, so ORDER BY rank ASC is
 * best-first and -rank is the positive score the trust rerank multiplies. */
#define BM25_EXPR "bm25(intel_items_fts,0,10,1,3,2,1,1,2,1)"
#define SNIPPET_EXPR \
  "snippet(intel_items_fts,-1,'<b>','</b>','\xE2\x80\xA6',24)"

enum { SORT_DATE = 0, SORT_RELEVANCE, SORT_TRUST };

static long env_long(const char *k, long dflt, long lo, long hi) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  long x = atol(v);
  if (x < lo) x = lo;
  if (x > hi) x = hi;
  return x;
}
static double env_double(const char *k, double dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  return atof(v);
}

/* Source-trust table, cached for JO_RERANK_TRUST_TTL seconds (default 300).
 * source_trust_load() is two GROUP BYs over fetch_log — cheap per /api/status
 * build, not cheap per search keystroke. Single-threaded httpd loop → a
 * static is safe. Read-only use of source_trust.h. */
static const source_trust *trust_table(db_handle *db, int *n_out) {
  static source_trust *tbl = NULL;
  static int n = 0;
  static time_t at = 0;
  time_t now = time(NULL);
  long ttl = env_long("JO_RERANK_TRUST_TTL", 300, 0, 86400);
  if (!tbl || now - at >= ttl) {
    free(tbl); tbl = NULL; n = 0;
    tbl = source_trust_load(db, &n);
    if (!tbl) n = 0;
    at = now;
  }
  *n_out = n;
  return tbl;
}

/* ISO-8601 → epoch seconds (UTC), or -1 when the text is not a date we can
 * read. Only the calendar part is required; a bare YYYY-MM-DD is midnight. */
static time_t parse_iso(const char *s) {
  if (!s || strlen(s) < 10) return -1;
  int y, mo, d, h = 0, mi = 0, se = 0;
  if (sscanf(s, "%4d-%2d-%2d", &y, &mo, &d) != 3) return -1;
  if (s[10] == 'T' || s[10] == ' ') sscanf(s + 11, "%2d:%2d:%2d", &h, &mi, &se);
  struct tm tm = {0};
  tm.tm_year = y - 1900; tm.tm_mon = mo - 1; tm.tm_mday = d;
  tm.tm_hour = h; tm.tm_min = mi; tm.tm_sec = se;
  return timegm(&tm);
}

/* One candidate row of the trust rerank window. */
typedef struct {
  cJSON *item;
  char   uid[512];
  char   cluster[512];
  double rank;      /* bm25, negative-is-better */
  double trust;     /* f(reliability) */
  double decay;     /* recency factor */
  double score;     /* -rank * trust * decay, higher is better */
} cand;

static int cand_cmp(const void *a, const void *b) {
  const cand *x = a, *y = b;
  if (x->score > y->score) return -1;
  if (x->score < y->score) return 1;
  return strcmp(x->uid, y->uid);
}

/* Build an error envelope the route can ship as-is. The status travels via
 * intelapi_list_items_st(); the body says the same thing in-band so a caller
 * on the status-less entry point still sees WHY it got no rows. */
static char *err_body(int *status, int code, const char *err, const char *detail) {
  if (status) *status = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", err);
  cJSON_AddStringToObject(o, "detail", detail);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

/* GET /api/intel/items — faithful port of intelStore.listItems({}):
 * structured WHERE (source/since/until/lang/tag/record_type/sub_source_id/
 * has_geom) + keyset cursor; ?q= goes through intel_items_fts (segmented
 * exactly like the write path, == entityapi_search / Node intelMirror.search,
 * caller-composed extra WHERE). ORDER/limit/cursor/envelope unchanged, so a
 * query with only `limit` set is byte-identical to the prior contract.
 *
 * SORTS. sort=date (default) is the port above. sort=relevance orders the FTS
 * hits by BM25_EXPR and keys the cursor on (rank,uid), rank carried as %.17g
 * so the decoded double compares equal to what SQLite computed. sort=trust
 * fetches the top JO_RERANK_WINDOW (default 500) hits by bm25 and reranks them
 * here by -bm25 x f(reliability) x decay(age), then pages by offset over that
 * window. The window is a BOUNDED VIEW of the match set, so meta.rerank states
 * the window size against the (capped) total — house rule 2.
 *
 * TOTALS. page.total is null unless want_total: the count is a second scan of
 * the whole filter. It is capped at JO_TOTAL_CAP (default 10000) — exact
 * below the cap, else page.total_gte=cap — so a broad query cannot turn the
 * feed into a full-table count.
 *
 * TENANCY. This is the primary feed and it carried NO tenant predicate at all,
 * while /api/export (exportapi.c:461) and /api/intel/near (nearapi.c:162) —
 * both of which read the same table through the same filter struct — carried
 * one. That asymmetry is latent rather than exploited only because every
 * intel_sink_make() hardcodes tenant_id 'legacy' today, so the corpus is a
 * single tenant's; the day a collector writes a real tenant_id, the export
 * route would scope and the feed beside it would not. The predicate is now
 * applied whenever the caller supplies Q->tenant.
 *
 * STILL UNWIRED, deliberately out of scope here: httpd.c's /api/intel/items,
 * /api/intel/search and /api/intel/items/:uid have an auth_user but never call
 * tenant_resolve(), so they pass no tenant and the predicate stays off — which
 * is exactly today's behaviour, i.e. this change cannot make a row disappear.
 * Closing it is one tenant_resolve() + `Q.tenant = tc.tenant_id` per route,
 * the same three lines every other tenant-scoped route in httpd.c already has.
 *
 * The breach branch below takes no tenant: breach_items is a global catalog
 * with no tenant column, gated instead by httpd.c's breach_gate(). */
char *intelapi_list_items_st(db_handle *db, const intel_items_query *Q,
                             int *status) {
  if (status) *status = 200;

  /* Resolve the sort FIRST: a ranking request that cannot be honoured must
   * not silently become the date feed. */
  int sort = SORT_DATE;
  if (Q && Q->sort && *Q->sort && strcmp(Q->sort, "date") != 0) {
    if      (!strcmp(Q->sort, "relevance")) sort = SORT_RELEVANCE;
    else if (!strcmp(Q->sort, "trust"))     sort = SORT_TRUST;
    else return err_body(status, 400, "unknown_sort",
             "sort must be one of date, relevance, trust");
    if (!Q->q || !*Q->q)
      return err_body(status, 400, "sort_requires_q",
        "sort=relevance and sort=trust rank full-text hits; there is nothing "
        "to rank without q. Omit sort (or sort=date) for the newest-first feed.");
  }

  /* A source-filtered request for a breach source is served from breach_items
   * via the adapter (keyset over row id). Unfiltered / non-breach requests stay
   * on intel_items, so breach volume never enters the operational feed.
   *
   * AUTHORIZATION for this branch lives in httpd.c's breach_gate(), applied to
   * /api/intel/items, /api/intel/search and /api/intel/items/:uid before they
   * reach this function. It cannot live here: this signature carries no caller
   * identity, and the same adapter is legitimately called (redacted) from the
   * alert inbox on behalf of the delivery worker, which has no auth_user at
   * all. If you add a fourth entry point to breach_adapter_*, gate it there
   * too — the whole finding was that one of four doors was locked. */
  if (Q && Q->source && *Q->source && breach_meta_is_source(db, Q->source)) {
    if (sort != SORT_DATE)
      return err_body(status, 400, "sort_not_supported_for_breach_source",
        "breach sources are served by the breach adapter, which orders by "
        "record id only; drop sort= for this source");
    return breach_adapter_list(db, Q->source, Q->q, Q->cursor, Q->limit);
  }

  int safe = Q && Q->limit > 0 ? Q->limit : 50;
  if (safe < 1) safe = 1;
  if (safe > 200) safe = 200;

  /* decode keyset cursor. date: {"p":pub_or_fetched,"u":uid}; relevance:
   * {"r":"%.17g bm25","u":uid}; trust: {"o":offset}. A cursor that does not
   * carry the keys this sort needs is IGNORED and disclosed in meta.notes —
   * a date cursor handed to sort=relevance would otherwise restart at page 1
   * without a word. A bad (undecodable) cursor is ignored as before. */
  char *cur_p = NULL, *cur_u = NULL, *cur_r = NULL; cJSON *curj = NULL;
  double cur_rank = 0; long cur_off = 0;
  int cursor_given = Q && Q->cursor && *Q->cursor, cursor_used = 0;
  if (cursor_given) {
    char *dec = b64url_decode(Q->cursor);
    if (dec) {
      curj = cJSON_Parse(dec); free(dec);
      if (curj) {
        cJSON *jp = cJSON_GetObjectItem(curj, "p");
        cJSON *ju = cJSON_GetObjectItem(curj, "u");
        cJSON *jr = cJSON_GetObjectItem(curj, "r");
        cJSON *jo = cJSON_GetObjectItem(curj, "o");
        if (sort == SORT_DATE && cJSON_IsString(jp) && cJSON_IsString(ju)) {
          cur_p = jp->valuestring; cur_u = ju->valuestring; cursor_used = 1;
        } else if (sort == SORT_RELEVANCE && cJSON_IsString(jr) && cJSON_IsString(ju)) {
          cur_r = jr->valuestring; cur_u = ju->valuestring;
          cur_rank = strtod(cur_r, NULL); cursor_used = 1;
        } else if (sort == SORT_TRUST && cJSON_IsNumber(jo)) {
          cur_off = (long)jo->valuedouble; if (cur_off < 0) cur_off = 0;
          cursor_used = 1;
        }
      }
    }
  }

  /* WHERE fragments + ordered binds — Node listItems() order verbatim. The
   * cursor predicate is kept in its OWN buffer so the total count can reuse
   * wbuf without it (a total that shrank as you paged would be a lie). */
  char wbuf[4096]; size_t wl = 0; wbuf[0] = 0;
  const char *bnd[24]; int nb = 0;
#define WPUSH(frag) do { int _n = snprintf(wbuf + wl, sizeof wbuf - wl, \
    " AND %s", (frag)); if (_n > 0) wl += (size_t)_n; \
    if (wl >= sizeof wbuf) wl = sizeof wbuf - 1; } while (0)
  /* Tenant scope FIRST, exactly as exportapi.c's build_intel_sql() does it and
   * for the same reason: wbuf is clamped, and the guard must never be the
   * fragment a clamp drops. `IN (?,'legacy')` because intel_items.tenant_id is
   * NOT NULL DEFAULT 'legacy' (schema.sql:308) — the `IS NULL OR tenant_id=?`
   * spelling used for the nullable `entities` column returns nothing here.
   * NULL/"" leaves the predicate off; see intel_items_query::tenant. */
  if (Q && Q->tenant && *Q->tenant)
                             { WPUSH("intel_items.tenant_id IN (?,'legacy')"); bnd[nb++] = Q->tenant; }
  if (Q && Q->source)        { WPUSH("intel_items.source_id = ?");        bnd[nb++] = Q->source; }
  if (Q && Q->since)         { WPUSH("COALESCE(intel_items.published_at,intel_items.fetched_at) >= ?"); bnd[nb++] = Q->since; }
  if (Q && Q->until)         { WPUSH("COALESCE(intel_items.published_at,intel_items.fetched_at) <= ?"); bnd[nb++] = Q->until; }
  if (Q && Q->lang)          { WPUSH("intel_items.language = ?");          bnd[nb++] = Q->lang; }
  if (Q && Q->tag)           { WPUSH("EXISTS (SELECT 1 FROM json_each(intel_items.tags) WHERE json_each.value = ?)"); bnd[nb++] = Q->tag; }
  if (Q && Q->record_type)   { WPUSH("intel_items.record_type = ?");       bnd[nb++] = Q->record_type; }
  if (Q && Q->sub_source_id) { WPUSH("intel_items.sub_source_id = ?");     bnd[nb++] = Q->sub_source_id; }
  if (Q && Q->has_geom && !strcmp(Q->has_geom, "yes")) WPUSH("intel_items.lat IS NOT NULL");
  if (Q && Q->has_geom && !strcmp(Q->has_geom, "no"))  WPUSH("intel_items.lat IS NULL");
#undef WPUSH
  const char *cursor_sql = "";
  if (cur_p)
    cursor_sql = " AND (COALESCE(intel_items.published_at,intel_items.fetched_at) < ? OR "
                 "(COALESCE(intel_items.published_at,intel_items.fetched_at) = ? AND intel_items.uid > ?))";
  else if (cur_r)
    cursor_sql = " AND (f.r > ? OR (f.r = ? AND intel_items.uid > ?))";

  /* Build the MATCH expression. fts_query_expr() quotes every token, so no
   * character the user types can reach the FTS5 expression parser as syntax
   * (`cam-tabi` used to mean `cam NOT tabi`, and a lone `"` failed the
   * prepare outright), and it prefix-matches the final token so a half-typed
   * word still hits.
   *
   * qAlt is the client's translated counterpart of q (iOS sends both when
   * auto-translate is on). It was parsed nowhere and silently dropped, which
   * is why bilingual search never widened a single result set. The two are
   * OR'd: "match the Japanese OR the English", each independently sanitized
   * and parenthesized so neither can bleed operators into the other.
   *
   * A non-empty q that yields no usable token (";;;" and friends) leaves
   * matchq NULL, i.e. the text filter is dropped rather than the request
   * failing — the same shape as any other unparseable filter here. */
  char *matchq = NULL;
  if (Q && Q->q && *Q->q) {
    char *ea = fts_query_expr(Q->q);
    char *eb = (Q->q_alt && *Q->q_alt) ? fts_query_expr(Q->q_alt) : NULL;
    if (ea && eb && strcmp(ea, eb) != 0) {
      size_t n = strlen(ea) + strlen(eb) + 12;
      matchq = malloc(n);
      if (matchq) snprintf(matchq, n, "(%s) OR (%s)", ea, eb);
      free(ea); free(eb);
    } else {
      matchq = ea ? ea : eb;
      if (ea && eb) free(eb);          /* identical after sanitizing */
    }
  }
  int has_q = matchq != NULL;
  char *segq = matchq;

  /* ...except for a RANKED request: "rank the hits for a query that has no
   * searchable token" has no honest answer other than refusing. */
  if (sort != SORT_DATE && !has_q) {
    if (curj) cJSON_Delete(curj);
    return err_body(status, 400, "sort_requires_q",
      "q contained no searchable token (only FTS metacharacters), so there "
      "are no full-text hits to rank");
  }

  long window = env_long("JO_RERANK_WINDOW", 500, 10, 5000);
  long total_cap = env_long("JO_TOTAL_CAP", 10000, 100, 10000000);
  int fetch_n = sort == SORT_TRUST ? (int)window : safe;

  /* FROM clause. With q the FTS hits come from an inner query that also
   * carries the rank, so ORDER BY / the keyset predicate can name `f.r`; the
   * structured WHERE still binds after the MATCH (bind ?1), as before. */
  char from[5200];
  if (has_q)
    snprintf(from, sizeof from,
      "FROM (SELECT uid, " BM25_EXPR " AS r FROM intel_items_fts "
      "WHERE intel_items_fts MATCH ?) f "
      "JOIN intel_items ON intel_items.uid = f.uid WHERE 1=1%s", wbuf);
  else
    snprintf(from, sizeof from, "FROM intel_items WHERE 1=1%s", wbuf);

  char sql[6400];
  const char *order =
    sort == SORT_DATE
      ? "ORDER BY COALESCE(intel_items.published_at,intel_items.fetched_at) DESC, intel_items.uid ASC"
      : "ORDER BY f.r ASC, intel_items.uid ASC";
  snprintf(sql, sizeof sql, "SELECT " ITEM_COLS_Q "%s %s%s %s LIMIT ?",
           has_q ? ", f.r" : ", NULL", from, cursor_sql, order);

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    free(segq); if (curj) cJSON_Delete(curj); return NULL;
  }
  int bi = 1;
  if (has_q) sqlite3_bind_text(s, bi++, segq, -1, SQLITE_TRANSIENT);
  for (int i = 0; i < nb; i++) sqlite3_bind_text(s, bi++, bnd[i], -1, SQLITE_TRANSIENT);
  if (cur_p) {
    sqlite3_bind_text(s, bi++, cur_p, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, bi++, cur_p, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, bi++, cur_u, -1, SQLITE_TRANSIENT);
  } else if (cur_r) {
    sqlite3_bind_double(s, bi++, cur_rank);
    sqlite3_bind_double(s, bi++, cur_rank);
    sqlite3_bind_text(s, bi++, cur_u, -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int(s, bi++, fetch_n + (sort == SORT_TRUST ? 1 : 0));

  /* Per-row snippet, looked up by FTS rowid through intel_items_fts_uid_map
   * (the alert_eval.c pattern): computing snippet() inside the ranked query
   * would evaluate it for every match the sorter sees, not just the page. */
  sqlite3_stmt *snip = NULL;
  if (has_q &&
      sqlite3_prepare_v2(db->h,
        "SELECT " SNIPPET_EXPR " FROM intel_items_fts "
        "WHERE intel_items_fts MATCH ?1 AND rowid = "
        "(SELECT rowid FROM intel_items_fts_uid_map WHERE uid=?2)",
        -1, &snip, NULL) != SQLITE_OK)
    snip = NULL;                      /* map missing → items carry no snippet */

  /* Trust rerank inputs, loaded once per call. */
  int ntrust = 0;
  const source_trust *ttab = sort == SORT_TRUST ? trust_table(db, &ntrust) : NULL;
  double half_life = env_double("JO_RERANK_HALFLIFE_DAYS", 90.0);
  double decay_floor = env_double("JO_RERANK_DECAY_FLOOR", 0.10);
  double unrated = env_double("JO_RERANK_UNRATED_FACTOR", 0.75);
  if (half_life <= 0) half_life = 90.0;
  time_t now = time(NULL);

  cand *cands = NULL; int nc = 0, ccap = 0;
  cJSON *data = cJSON_CreateArray();
  int n = 0, collapsed = 0;
  char last_pub[256] = {0}, last_uid[512] = {0}; double last_rank = 0;
  /* collapse: cluster ids already shown on this page (page is rank-ordered, so
   * the first member seen is the best-ranked one). */
  char *seen_cl[5000]; int nseen = 0;     /* ≤ window entries, heap strings */
#define CL_SEEN(cl) ({ int _d = 0; for (int _i = 0; _i < nseen; _i++)     if (!strcmp(seen_cl[_i], (cl))) { _d = 1; break; }     if (!_d && nseen < 5000) { char *_c = strdup(cl); if (_c) seen_cl[nseen++] = _c; }     _d; })

  while (sqlite3_step(s) == SQLITE_ROW) {
    n++;
    const char *uid = ctext(s, 0) ? ctext(s, 0) : "";
    const char *cl = ctext(s, 19);
    const char *p = ctext(s, 8); if (!p) p = ctext(s, 9);
    snprintf(last_pub, sizeof last_pub, "%s", p ? p : "");
    snprintf(last_uid, sizeof last_uid, "%s", uid);
    last_rank = sqlite3_column_double(s, 20);

    if (sort != SORT_TRUST && Q && Q->collapse && cl && *cl && CL_SEEN(cl)) {
      collapsed++; continue;
    }

    cJSON *it = row_to_item(s, 0);
    if (snip) {
      sqlite3_reset(snip);
      sqlite3_bind_text(snip, 1, segq, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(snip, 2, uid, -1, SQLITE_TRANSIENT);
      const char *sn = sqlite3_step(snip) == SQLITE_ROW
                         ? (const char *)sqlite3_column_text(snip, 0) : NULL;
      add_str_or_null(it, "snippet", sn);
    }
    if (sort == SORT_RELEVANCE) cJSON_AddNumberToObject(it, "rank", -last_rank);

    if (sort == SORT_TRUST) {
      if (nc == ccap) {
        int ncap = ccap ? ccap * 2 : 64;
        cand *nw = realloc(cands, (size_t)ncap * sizeof *nw);
        if (!nw) { cJSON_Delete(it); break; }
        cands = nw; ccap = ncap;
      }
      cand *c = &cands[nc++];
      memset(c, 0, sizeof *c);
      c->item = it;
      snprintf(c->uid, sizeof c->uid, "%s", uid);
      snprintf(c->cluster, sizeof c->cluster, "%s", cl ? cl : "");
      c->rank = last_rank;
      const source_trust *t = source_trust_find(ttab, ntrust, ctext(s, 1));
      /* f(reliability) = 0.5 + 0.5·r for a rated source (a grade-F source
       * still keeps half its text score — trust re-orders, it does not
       * censor); an UNRATED source gets the neutral midpoint, disclosed in
       * meta.rerank.unrated_factor rather than a fabricated grade. */
      c->trust = (t && t->rated) ? 0.5 + 0.5 * t->reliability : unrated;
      time_t ts = parse_iso(p);
      if (ts < 0) c->decay = 1.0;         /* no readable date: no evidence to decay on */
      else {
        double age_d = difftime(now, ts) / 86400.0;
        if (age_d < 0) age_d = 0;
        c->decay = pow(0.5, age_d / half_life);
        if (c->decay < decay_floor) c->decay = decay_floor;
      }
      c->score = -c->rank * c->trust * c->decay;
    } else {
      cJSON_AddItemToArray(data, it);
    }
  }
  sqlite3_finalize(s);
  if (snip) sqlite3_finalize(snip);

  /* Optional capped total, over the same filter WITHOUT the cursor. Exact
   * below total_cap, otherwise reported as total_gte=total_cap. */
  long total = -1;
  if ((Q && Q->want_total) || sort == SORT_TRUST) {
    char csql[6400];
    snprintf(csql, sizeof csql, "SELECT COUNT(*) FROM (SELECT 1 %s LIMIT ?)", from);
    sqlite3_stmt *cs;
    if (sqlite3_prepare_v2(db->h, csql, -1, &cs, NULL) == SQLITE_OK) {
      int ci = 1;
      if (has_q) sqlite3_bind_text(cs, ci++, segq, -1, SQLITE_TRANSIENT);
      for (int i = 0; i < nb; i++) sqlite3_bind_text(cs, ci++, bnd[i], -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(cs, ci++, (sqlite3_int64)total_cap + 1);
      if (sqlite3_step(cs) == SQLITE_ROW) total = (long)sqlite3_column_int64(cs, 0);
      sqlite3_finalize(cs);
    }
  }
  free(segq);

  cJSON *page = cJSON_CreateObject();
  int more = 0;
  if (sort == SORT_TRUST) {
    /* Rerank the window, then page it by offset. The (window+1)th row, if it
     * came back, is only evidence that the window is not the whole match set;
     * it is dropped from the ranking so the window is exactly `window` wide. */
    int in_window = nc > (int)window ? (int)window : nc;
    for (int i = in_window; i < nc; i++) cJSON_Delete(cands[i].item);
    qsort(cands, (size_t)in_window, sizeof *cands, cand_cmp);
    int shown = 0; long pos = 0;
    for (int i = 0; i < in_window; i++) {
      cand *c = &cands[i];
      if (Q && Q->collapse && c->cluster[0] && CL_SEEN(c->cluster)) {
        collapsed++; cJSON_Delete(c->item); continue;
      }
      if (pos < cur_off) { pos++; cJSON_Delete(c->item); continue; }
      if (shown >= safe) { more = 1; cJSON_Delete(c->item); continue; }
      cJSON_AddNumberToObject(c->item, "rank", -c->rank);
      cJSON *sc = cJSON_CreateObject();
      cJSON_AddNumberToObject(sc, "bm25", -c->rank);
      cJSON_AddNumberToObject(sc, "trust", c->trust);
      cJSON_AddNumberToObject(sc, "decay", c->decay);
      cJSON_AddNumberToObject(sc, "score", c->score);
      cJSON_AddItemToObject(c->item, "rerank", sc);
      cJSON_AddItemToArray(data, c->item);
      shown++; pos++;
    }
    free(cands);
    if (more) {
      char cj[64]; snprintf(cj, sizeof cj, "{\"o\":%ld}", cur_off + shown);
      char *enc = b64url(cj);
      cJSON_AddStringToObject(page, "next_cursor", enc); free(enc);
    } else cJSON_AddNullToObject(page, "next_cursor");
  } else if (n == fetch_n) {
    cJSON *cur = cJSON_CreateObject();
    if (sort == SORT_RELEVANCE) {
      char rb[40]; snprintf(rb, sizeof rb, "%.17g", last_rank);
      cJSON_AddStringToObject(cur, "r", rb);
    } else {
      cJSON_AddStringToObject(cur, "p", last_pub);
    }
    cJSON_AddStringToObject(cur, "u", last_uid);
    char *cj = cJSON_PrintUnformatted(cur);
    cJSON_Delete(cur);
    char *enc = b64url(cj);
    cJSON_AddStringToObject(page, "next_cursor", enc);
    free(cj); free(enc);
  } else {
    cJSON_AddNullToObject(page, "next_cursor");
  }
  for (int i = 0; i < nseen; i++) free(seen_cl[i]);
#undef CL_SEEN
  cJSON_AddNumberToObject(page, "limit", safe);
  if (total < 0)              cJSON_AddNullToObject(page, "total");
  else if (total <= total_cap) cJSON_AddNumberToObject(page, "total", (double)total);
  else { cJSON_AddNullToObject(page, "total");
         cJSON_AddNumberToObject(page, "total_gte", (double)total_cap); }

  cJSON *filters = cJSON_CreateObject();   /* echo applied filters (Node route
   * meta.filters); all-NULL input → all null → byte-identical to old output */
  add_str_or_null(filters, "source",        Q ? Q->source : NULL);
  add_str_or_null(filters, "q",             Q ? Q->q : NULL);
  add_str_or_null(filters, "q_alt",         Q ? Q->q_alt : NULL);
  add_str_or_null(filters, "lang",          Q ? Q->lang : NULL);
  add_str_or_null(filters, "record_type",   Q ? Q->record_type : NULL);
  add_str_or_null(filters, "sub_source_id", Q ? Q->sub_source_id : NULL);
  add_str_or_null(filters, "has_geom",      Q ? Q->has_geom : NULL);

  char ts[40]; iso_now(ts, sizeof ts);
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "fetched_at", ts);
  cJSON_AddItemToObject(meta, "filters", filters);

  /* Everything below is omitted for the plain feed so the
   * /api/intel/items?limit=10 contract fixture stays byte-identical. */
  if (sort != SORT_DATE)
    cJSON_AddStringToObject(meta, "sort", sort == SORT_TRUST ? "trust" : "relevance");
  if (sort == SORT_TRUST) {
    cJSON *rr = cJSON_CreateObject();
    cJSON_AddNumberToObject(rr, "window", (double)window);
    cJSON_AddNumberToObject(rr, "candidates", (double)(nc > (int)window ? window : nc));
    if (total >= 0 && total <= total_cap) cJSON_AddNumberToObject(rr, "of", (double)total);
    else cJSON_AddNumberToObject(rr, "of_gte", (double)(total > total_cap ? total_cap : n));
    cJSON_AddBoolToObject(rr, "bounded", total < 0 || total > window);
    cJSON_AddStringToObject(rr, "formula",
      "score = -bm25 * (rated ? 0.5+0.5*reliability : unrated_factor) "
      "* max(decay_floor, 0.5^(age_days/half_life_days))");
    cJSON_AddNumberToObject(rr, "half_life_days", half_life);
    cJSON_AddNumberToObject(rr, "decay_floor", decay_floor);
    cJSON_AddNumberToObject(rr, "unrated_factor", unrated);
    cJSON_AddNumberToObject(rr, "trust_sources_rated", (double)ntrust);
    cJSON_AddItemToObject(meta, "rerank", rr);
  }
  if (Q && Q->collapse) cJSON_AddNumberToObject(meta, "collapsed", (double)collapsed);

  /* DISCLOSE A DROPPED TEXT FILTER. Dropping it is deliberate (see the comment
   * on matchq above: an unparseable q degrades to "unfiltered" rather than to
   * a 500, the same as any other unusable filter here). The BUG was that
   * nothing said so. A q of pure FTS metacharacters — "'", "^", "((((", "~",
   * "-", "%", "_", "{" — sanitizes to no usable token, the MATCH is not
   * applied, the WHOLE CORPUS comes back, and meta.filters.q still echoes the
   * raw input, so the response is indistinguishable from a filtered one that
   * genuinely matched everything. On /api/intel/search that reads as "your
   * search found 4 things" when it means "your search was not run".
   *
   * `q_applied` therefore appears whenever a caller supplied any text at all,
   * and `notes` carries the reason code in the spelling timelineapi.c already
   * uses for "we did something other than what you asked" (cursor_ignored,
   * case_items_table_missing). The result set is NOT changed — this only makes
   * the envelope honest about what produced it.
   *
   * Both keys are omitted entirely when no q/q_alt was sent, which is also
   * what keeps the /api/intel/items?limit=10 contract fixture byte-identical:
   * a request with no text filter has nothing to disclose. */
  { int asked = (Q && ((Q->q && *Q->q) || (Q->q_alt && *Q->q_alt)));
    cJSON *notes = NULL;
    if (asked) {
      cJSON_AddBoolToObject(meta, "q_applied", has_q ? 1 : 0);
      if (!has_q) {
        notes = cJSON_CreateArray();
        cJSON_AddItemToArray(notes,
          cJSON_CreateString("q_ignored_no_searchable_token"));
      }
    }
    if (cursor_given && !cursor_used) {
      if (!notes) notes = cJSON_CreateArray();
      cJSON_AddItemToArray(notes, cJSON_CreateString("cursor_ignored"));
    }
    if (notes) cJSON_AddItemToObject(meta, "notes", notes);
  }

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", data);
  cJSON_AddItemToObject(env, "page", page);
  cJSON_AddItemToObject(env, "meta", meta);
  char *js = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  if (curj) cJSON_Delete(curj);
  return js;
}

char *intelapi_list_items(db_handle *db, const intel_items_query *Q) {
  return intelapi_list_items_st(db, Q, NULL);
}

/* GET /api/sources — getAllSources(): SELECT * FROM sources
 * ORDER BY category, name → JSON array. Columns serialized by sqlite type
 * (INTEGER/REAL → number, TEXT → string, NULL → null) to mirror
 * better-sqlite3's row objects + JSON.stringify. */
char *api_sources_list(db_handle *db) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT * FROM sources ORDER BY category, name", -1, &s, NULL)
      != SQLITE_OK)
    return NULL;
  int ncol = sqlite3_column_count(s);
  cJSON *arr = cJSON_CreateArray();
  while (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *row = cJSON_CreateObject();
    for (int i = 0; i < ncol; i++) {
      const char *name = sqlite3_column_name(s, i);
      switch (sqlite3_column_type(s, i)) {
        case SQLITE_NULL:
          cJSON_AddNullToObject(row, name); break;
        case SQLITE_INTEGER:
          cJSON_AddNumberToObject(row, name,
            (double)sqlite3_column_int64(s, i)); break;
        case SQLITE_FLOAT:
          cJSON_AddNumberToObject(row, name,
            sqlite3_column_double(s, i)); break;
        default:
          cJSON_AddStringToObject(row, name,
            (const char *)sqlite3_column_text(s, i)); break;
      }
    }
    cJSON_AddItemToArray(arr, row);
  }
  sqlite3_finalize(s);
  char *js = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return js;
}

/* intelCatalog.js INTEL_SOURCE_SET — sources that emit kind:'intel'.
 *
 * THE one definition; statusapi.c reaches it through intelapi_is_intel_id()
 * instead of keeping the verbatim copy it used to (see intelapi.h).
 *
 * boj-stats / jcg-navarea / nict-atlas were dropped here on 2026-08-09: their
 * collectors were deleted in the 66-source removal sweep, so `is_intel:true`
 * could only ever have been reported for an id that no longer resolves in
 * either registry. Nothing else in the list changed. */
static const char *INTEL_IDS[] = {
  "certstream-jp","bird-makeup-jp","chan-5ch","fofa-jp",
  "github-leaks-jp","grayhat-buckets","greynoise-jp","hatena-bookmark",
  "houjin-bangou","mercari-trending","misskey-timeline","note-com-trending",
  "urlscan-jp","wayback-jp",
  "edinet-filings","data-go-jp-ckan","egov-laws",
  "geospatial-jp-ckan","kyodo-rss","nhk-world-rss",
  "ripestat-jp","wifi-hotspots-jcfw","wifi-hotspots-freespot",
  "jp-news-rss","nhk-news-rss","yahoo-news-jp-rss","ipa-alerts",
  "jpcert-alerts","phishing-feeds-jp","sans-isc-feeds","my-jvn",
};
int intelapi_is_intel_id(const char *id) {
  if (!id) return 0;
  for (size_t i = 0; i < sizeof INTEL_IDS / sizeof *INTEL_IDS; i++)
    if (strcmp(INTEL_IDS[i], id) == 0) return 1;
  return 0;
}

/* getTtlMs(key): collector_ttls row or DEFAULT (15min).
 *
 * Takes an ALREADY-PREPARED statement and resets it per lookup. It is called
 * once per emitted source — the curated table, plus every orphan, plus every
 * breach catalog row (~1,600 in total) — and used to compile and finalize a
 * fresh statement each time, i.e. ~1,600 SQL compiles per request on the single
 * event-loop thread. Pass NULL to get the default without touching the DB. */
#define TTL_DEFAULT_MS (15 * 60 * 1000.0)

static double get_ttl_ms(sqlite3_stmt *s, const char *id) {
  double v = TTL_DEFAULT_MS;
  if (!s || !id) return v;
  sqlite3_reset(s);
  sqlite3_clear_bindings(s);
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) != SQLITE_NULL)
    v = sqlite3_column_double(s, 0);
  sqlite3_reset(s);
  return v;
}

typedef struct {
  char src[256];
  long item_count, geocoded, ungeocoded, awaiting_geo;
  char last_fetched[64], last_published[64];   /* "" == NULL */
  int has_lf, has_lp;
} agg_row;

/* String.prototype.localeCompare (Node/ICU root, en-US) approximation.
 * UCA-style: primary = case-insensitive letters/digits with digits ordered
 * before letters and spaces/punctuation ignorable; on a primary tie, fall
 * back to a case-insensitive then case-sensitive byte compare. Sufficient
 * for the ASCII display names + ISO/`YYYY/MM/DD` timestamp keys this sort
 * compares (verified byte-for-byte against the captured Node fixture). */
/* DUCET-ordered primary buckets for the ASCII we see in names/timestamps:
 * NUL/control ignorable < SPACE < other punctuation < digits < letters
 * (case-insensitive). Punctuation is NON-ignorable (ICU root default), so
 * "e-Stat …" < "Eight …" and "Bike Share …" < "Bike-share …". */
static int prim_w(unsigned char c) {
  if (c == ' ')             return 1;
  if (c >= '0' && c <= '9') return 4 + (c - '0');          /* 4..13   */
  if (c >= 'A' && c <= 'Z') return 20 + (c - 'A');         /* 20..45  */
  if (c >= 'a' && c <= 'z') return 20 + (c - 'a');
  if (c >= 0x80)            return 200 + c;                /* non-ASCII */
  if (c < 0x20)             return 0;                      /* control: ignorable */
  return 2;                                                /* other punctuation */
}
static int locale_cmp(const char *a, const char *b) {
  const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
  size_t i = 0, j = 0;
  for (;;) {
    while (x[i] && prim_w(x[i]) == 0) i++;
    while (y[j] && prim_w(y[j]) == 0) j++;
    if (!x[i] && !y[j]) break;
    if (!x[i]) return -1;
    if (!y[j]) return 1;
    int wa = prim_w(x[i]), wb = prim_w(y[j]);
    if (wa != wb) return wa < wb ? -1 : 1;
    i++; j++;
  }
  /* primary equal → case-insensitive then case-sensitive byte fallback */
  for (size_t k = 0;; k++) {
    unsigned char ca = a[k], cb = b[k];
    int la = ca >= 'A' && ca <= 'Z' ? ca + 32 : ca;
    int lb = cb >= 'A' && cb <= 'Z' ? cb + 32 : cb;
    if (la != lb) return la < lb ? -1 : 1;
    if (!ca) break;
  }
  return strcmp(a, b);
}

/* ── camera channel rollup ────────────────────────────────────────────────
 *
 * The JS backend had ONE camera collector, id `camera-discovery`. The C port
 * split it into 14+ registered sources (cam-camscape, cam-tabi_cam,
 * cam-scs_com_ua, cam-webcamendirect_list, shodan-cameras-jp, …) which all
 * share `layer = "cameras"` in source_registry.gen.c. camera_store.c still
 * keys every row `camera-discovery|<camera_uid>` while intel.c binds
 * source_id from the RUNNING source — so the per-source_id aggregate above
 * hands the parent ZERO items and scatters the real counts across the
 * children. In the catalogue that reads as "Unified Camera Discovery, 0
 * items" sitting next to fourteen sibling rows that are actually its own
 * channels.
 *
 * Fix it at the API boundary, not in the registry: the children are genuine
 * separate collectors with their own schedules, TTLs and Run buttons, so they
 * must stay addressable. They just are not siblings. Their totals roll up into
 * the parent and each one is stamped with parent_id so a client can nest them.
 */
#define CAMERA_PARENT_ID "camera-discovery"
#define CAMERA_CHILD_LAYER "cameras"

static int is_camera_child(const char *id) {
  if (!id || strcmp(id, CAMERA_PARENT_ID) == 0) return 0;
  const src_meta *m = src_meta_get(id);
  return m && m->layer && strcmp(m->layer, CAMERA_CHILD_LAYER) == 0;
}

/* listSources() comparator: freshest first (descending localeCompare), then
 * never-collected alphabetical by display name. Decorated with the original
 * index for a stable total order (V8 Array.sort is stable). */
typedef struct { cJSON *obj; const char *fresh; const char *name; int idx; } sortrow;
static int cmp_sr(const void *A, const void *B) {
  const sortrow *a = A, *b = B;
  int af = a->fresh && a->fresh[0], bf = b->fresh && b->fresh[0];
  int r;
  if (af && bf)      r = locale_cmp(b->fresh, a->fresh);   /* bFresh vs aFresh */
  else if (af)       r = -1;
  else if (bf)       r = 1;
  else               r = locale_cmp(a->name, b->name);
  return r ? r : a->idx - b->idx;
}

char *intelapi_intel_sources(db_handle *db) {
  /* 1. aggregates over intel_items (== listSources()) */
  static const char *AQ =
    "SELECT source_id,COUNT(*),"
    "SUM(CASE WHEN lat IS NOT NULL THEN 1 ELSE 0 END),"
    "SUM(CASE WHEN lat IS NULL THEN 1 ELSE 0 END),"
    "SUM(CASE WHEN lat IS NULL AND geom_source IS NULL THEN 1 ELSE 0 END),"
    "MAX(fetched_at),MAX(COALESCE(published_at,fetched_at)) "
    "FROM intel_items GROUP BY source_id";
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, AQ, -1, &s, NULL) != SQLITE_OK) return NULL;
  int cap = 64, na = 0;
  agg_row *A = malloc(cap * sizeof *A);
  if (!A) { sqlite3_finalize(s); return NULL; }
  while (sqlite3_step(s) == SQLITE_ROW) {
    /* An unchecked realloc here leaked the old block AND wrote through the
     * NULL on the very next line. */
    if (na == cap) {
      agg_row *NA = realloc(A, (size_t)cap * 2 * sizeof *A);
      if (!NA) break;
      A = NA; cap *= 2;
    }
    agg_row *r = &A[na++];
    snprintf(r->src, sizeof r->src, "%s",
             (const char *)sqlite3_column_text(s, 0));
    r->item_count   = sqlite3_column_int64(s, 1);
    r->geocoded     = sqlite3_column_int64(s, 2);
    r->ungeocoded   = sqlite3_column_int64(s, 3);
    r->awaiting_geo = sqlite3_column_int64(s, 4);
    r->has_lf = sqlite3_column_type(s, 5) != SQLITE_NULL;
    r->has_lp = sqlite3_column_type(s, 6) != SQLITE_NULL;
    snprintf(r->last_fetched, sizeof r->last_fetched, "%s",
             r->has_lf ? (const char *)sqlite3_column_text(s, 5) : "");
    snprintf(r->last_published, sizeof r->last_published, "%s",
             r->has_lp ? (const char *)sqlite3_column_text(s, 6) : "");
  }
  sqlite3_finalize(s);

  /* 1a. Camera rollup (see is_camera_child): sum every layer="cameras"
   * channel into the camera-discovery parent, whose own GROUP BY source_id
   * aggregate is always zero because camera_store.c writes those rows under
   * the child's source_id. Freshness is the newest across the channels — the
   * parent is only as stale as its most recent channel run. */
  {
    agg_row roll; memset(&roll, 0, sizeof roll);
    int any = 0;
    for (int k = 0; k < na; k++) {
      if (!is_camera_child(A[k].src)) continue;
      any = 1;
      roll.item_count   += A[k].item_count;
      roll.geocoded     += A[k].geocoded;
      roll.ungeocoded   += A[k].ungeocoded;
      roll.awaiting_geo += A[k].awaiting_geo;
      /* ISO-8601 with a fixed shape: byte order == chronological order. */
      if (A[k].has_lf && (!roll.has_lf ||
                          strcmp(A[k].last_fetched, roll.last_fetched) > 0)) {
        roll.has_lf = 1;
        snprintf(roll.last_fetched, sizeof roll.last_fetched, "%s", A[k].last_fetched);
      }
      if (A[k].has_lp && (!roll.has_lp ||
                          strcmp(A[k].last_published, roll.last_published) > 0)) {
        roll.has_lp = 1;
        snprintf(roll.last_published, sizeof roll.last_published, "%s", A[k].last_published);
      }
    }
    if (any) {
      agg_row *p = NULL;
      for (int k = 0; k < na; k++)
        if (strcmp(A[k].src, CAMERA_PARENT_ID) == 0) { p = &A[k]; break; }
      if (!p) {          /* the normal case: no row carries that source_id */
        if (na == cap) {
          agg_row *grown = realloc(A, (size_t)cap * 2 * sizeof *A);
          if (grown) { A = grown; cap *= 2; }
        }
        if (na < cap) {
          p = &A[na++];
          memset(p, 0, sizeof *p);
          snprintf(p->src, sizeof p->src, "%s", CAMERA_PARENT_ID);
        }
      }
      if (p) {           /* += so a parent that DOES have rows keeps them */
        p->item_count   += roll.item_count;
        p->geocoded     += roll.geocoded;
        p->ungeocoded   += roll.ungeocoded;
        p->awaiting_geo += roll.awaiting_geo;
        if (roll.has_lf && (!p->has_lf ||
                            strcmp(roll.last_fetched, p->last_fetched) > 0)) {
          p->has_lf = 1;
          snprintf(p->last_fetched, sizeof p->last_fetched, "%s", roll.last_fetched);
        }
        if (roll.has_lp && (!p->has_lp ||
                            strcmp(roll.last_published, p->last_published) > 0)) {
          p->has_lp = 1;
          snprintf(p->last_published, sizeof p->last_published, "%s", roll.last_published);
        }
      }
    }
  }

  /* 1b. breach catalog rows → intel sources (category "breach"). Their
   * item_count comes from breach_items (materialized), NOT the intel_items
   * aggregate above, so breach volume never touches operational counts. */
  int nb = 0;
  breach_src_row *B = breach_meta_sources(db, &nb);

  /* One prepared statement for every ttl lookup below (see get_ttl_ms). */
  sqlite3_stmt *ttl_st = NULL;
  if (sqlite3_prepare_v2(db->h, "SELECT ttl_ms FROM collector_ttls WHERE key=?1",
                         -1, &ttl_st, NULL) != SQLITE_OK)
    ttl_st = NULL;                       /* fall back to the default TTL */

  /* 2. ids = every registered source, then any curated row with no registered
   *    def, then orphan agg ids that resolve to neither.
   *
   * Enumerating only the curated table (src_meta_at, 415 rows) silently dropped
   * every source that is registered but not curated — ~260 of them, including
   * ones with real intel_items rows. The orphan pass below could not rescue
   * them either, because it skips anything src_meta_get() can resolve, and
   * src_meta_get DOES resolve them (it falls through to the runtime registry).
   * So they appeared nowhere. */
  int nregistry = registry_count();
  const source_def **defs = registry_all();
  int ncur = src_meta_count();
  int total = nregistry + ncur + na + nb;
  sortrow *SR = malloc(total * sizeof *SR);
  if (!SR) { free(A); free(B); if (ttl_st) sqlite3_finalize(ttl_st); return NULL; }
  int nout = 0;

  for (int pass = 0; pass < 2; pass++) {
    int npass = (pass == 0) ? nregistry : ncur;
    for (int i = 0; i < npass; i++) {
    const src_meta *cm = (pass == 0) ? NULL : src_meta_at(i);
    const char *id = (pass == 0) ? defs[i]->id : (cm ? cm->id : NULL);
    if (!id) continue;
    if (pass == 1 && registry_get(id)) continue;   /* emitted in pass 0 */
    const src_meta *m = src_meta_get(id);          /* meta = Map last-wins */
    if (!m) continue;
    agg_row *g = NULL;
    for (int k = 0; k < na; k++) if (strcmp(A[k].src, id) == 0) { g = &A[k]; break; }

    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "id", id);
    cJSON_AddStringToObject(o, "name", m->name ? m->name : id);
    add_str_or_null(o, "name_ja", m->name_ja);
    add_str_or_null(o, "category", m->category);
    add_str_or_null(o, "description", m->description);
    add_str_or_null(o, "url", m->url);
    cJSON_AddNumberToObject(o, "item_count",   g ? (double)g->item_count   : 0);
    cJSON_AddNumberToObject(o, "geocoded",     g ? (double)g->geocoded     : 0);
    cJSON_AddNumberToObject(o, "ungeocoded",   g ? (double)g->ungeocoded   : 0);
    cJSON_AddNumberToObject(o, "awaiting_geo", g ? (double)g->awaiting_geo : 0);
    add_str_or_null(o, "last_fetched",   g && g->has_lf ? g->last_fetched   : NULL);
    add_str_or_null(o, "last_published", g && g->has_lp ? g->last_published : NULL);
    cJSON_AddNumberToObject(o, "ttl_ms", get_ttl_ms(ttl_st, id));
    cJSON_AddBoolToObject(o, "is_intel", intelapi_is_intel_id(id));
    /* Non-null only for the camera discovery channels. Clients that don't know
     * the key keep the old flat list; ones that do nest these under the
     * parent whose counts they were just rolled into. */
    add_str_or_null(o, "parent_id",
                    is_camera_child(id) ? CAMERA_PARENT_ID : NULL);

    sortrow *sr = &SR[nout];
    sr->obj = o; sr->idx = nout;
    sr->name = cJSON_GetObjectItem(o, "name")->valuestring;
    cJSON *lf = cJSON_GetObjectItem(o, "last_fetched");
    cJSON *lp = cJSON_GetObjectItem(o, "last_published");
    sr->fresh = cJSON_IsString(lf) ? lf->valuestring
              : cJSON_IsString(lp) ? lp->valuestring : "";
    nout++;
    }
  }
  /* orphans: agg source_ids not present in the registry, agg query order */
  for (int k = 0; k < na; k++) {
    if (src_meta_get(A[k].src)) continue;
    agg_row *g = &A[k];
    const char *id = g->src;
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "id", id);
    cJSON_AddStringToObject(o, "name", id);
    cJSON_AddNullToObject(o, "name_ja");
    cJSON_AddNullToObject(o, "category");
    cJSON_AddNullToObject(o, "description");
    cJSON_AddNullToObject(o, "url");
    cJSON_AddNumberToObject(o, "item_count",   (double)g->item_count);
    cJSON_AddNumberToObject(o, "geocoded",     (double)g->geocoded);
    cJSON_AddNumberToObject(o, "ungeocoded",   (double)g->ungeocoded);
    cJSON_AddNumberToObject(o, "awaiting_geo", (double)g->awaiting_geo);
    add_str_or_null(o, "last_fetched",   g->has_lf ? g->last_fetched   : NULL);
    add_str_or_null(o, "last_published", g->has_lp ? g->last_published : NULL);
    cJSON_AddNumberToObject(o, "ttl_ms", get_ttl_ms(ttl_st, id));
    cJSON_AddBoolToObject(o, "is_intel", intelapi_is_intel_id(id));
    /* An orphan resolves to no registry metadata by definition, so it has no
     * layer and can never be a camera child — but the key is emitted anyway
     * so every row in `data` has the same shape. */
    cJSON_AddNullToObject(o, "parent_id");

    sortrow *sr = &SR[nout];
    sr->obj = o; sr->idx = nout;
    sr->name = cJSON_GetObjectItem(o, "name")->valuestring;
    cJSON *lf = cJSON_GetObjectItem(o, "last_fetched");
    cJSON *lp = cJSON_GetObjectItem(o, "last_published");
    sr->fresh = cJSON_IsString(lf) ? lf->valuestring
              : cJSON_IsString(lp) ? lp->valuestring : "";
    nout++;
  }

  /* breach catalog sources. Each breach_meta row → one source with
   * category "breach"; item_count is the materialized breach_items count,
   * falling back to the catalog pwn_count for display before ingest. */
  for (int k = 0; k < nb; k++) {
    breach_src_row *br = &B[k];
    const char *id = br->breach_id;
    if (src_meta_get(id)) continue;   /* never shadow a real registry source */

    /* Shared with statusapi.c's breach_status_row so the two source views can't
     * drift; `fresh` is NULL (not "") when unknown, hence the guards below. */
    long long count = 0;
    const char *fresh = NULL;
    char desc[192];
    breach_meta_display(br, &count, &fresh, desc, sizeof desc);
    if (!fresh) fresh = "";

    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "id", id);
    cJSON_AddStringToObject(o, "name", br->name[0] ? br->name : id);
    cJSON_AddNullToObject(o, "name_ja");
    cJSON_AddStringToObject(o, "category", "breach");
    cJSON_AddStringToObject(o, "description", desc);
    add_str_or_null(o, "url", br->domain[0] ? br->domain : NULL);
    cJSON_AddNumberToObject(o, "item_count",   (double)count);
    cJSON_AddNumberToObject(o, "geocoded",     0);
    cJSON_AddNumberToObject(o, "ungeocoded",   0);
    cJSON_AddNumberToObject(o, "awaiting_geo", 0);
    add_str_or_null(o, "last_fetched",   fresh[0] ? fresh : NULL);
    add_str_or_null(o, "last_published", fresh[0] ? fresh : NULL);
    cJSON_AddNumberToObject(o, "ttl_ms", get_ttl_ms(ttl_st, id));
    cJSON_AddBoolToObject(o, "is_intel", 1);
    cJSON_AddNullToObject(o, "parent_id");   /* breaches are never nested */

    sortrow *sr = &SR[nout];
    sr->obj = o; sr->idx = nout;
    sr->name = cJSON_GetObjectItem(o, "name")->valuestring;
    cJSON *lf = cJSON_GetObjectItem(o, "last_fetched");
    sr->fresh = cJSON_IsString(lf) ? lf->valuestring : "";
    nout++;
  }

  if (ttl_st) sqlite3_finalize(ttl_st);

  qsort(SR, nout, sizeof *SR, cmp_sr);

  cJSON *data = cJSON_CreateArray();
  for (int i = 0; i < nout; i++) cJSON_AddItemToArray(data, SR[i].obj);

  char ts[40]; iso_now(ts, sizeof ts);
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "fetched_at", ts);
  cJSON_AddNumberToObject(meta, "total", nout);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", data);
  cJSON_AddItemToObject(env, "meta", meta);
  char *js = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  free(A); free(SR); free(B);
  return js;
}
