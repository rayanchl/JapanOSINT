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

/* explicit column order shared by item-by-uid and list-items so rows can be
 * read by fixed index (== rowToItem field access in intelStore.js). */
#define ITEM_COLS \
  "uid,source_id,title,body,summary,link,author,language," \
  "published_at,fetched_at,tags,properties,keywords,lat,lon,geom_source," \
  "geom_at,record_type,sub_source_id"

/* Same columns, same order, table-qualified — for the FTS branch where the
 * join to intel_items_fts makes title/body/summary/keywords ambiguous. The
 * fixed-index reads in row_to_item() stay valid (identical order). */
#define ITEM_COLS_Q \
  "intel_items.uid,intel_items.source_id,intel_items.title,intel_items.body," \
  "intel_items.summary,intel_items.link,intel_items.author," \
  "intel_items.language,intel_items.published_at,intel_items.fetched_at," \
  "intel_items.tags,intel_items.properties,intel_items.keywords," \
  "intel_items.lat,intel_items.lon,intel_items.geom_source," \
  "intel_items.geom_at,intel_items.record_type,intel_items.sub_source_id"

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
  snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000));
}

/* Buffer.from(str,'utf8').toString('base64url') — no padding, +/ → -_ */
static char *b64url(const char *in) {
  static const char T[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  size_t len = strlen(in);
  char *out = malloc(((len + 2) / 3) * 4 + 1);
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
    *c_rt=ctext(s,17), *c_sub=ctext(s,18);
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

char *intelapi_item_by_uid(db_handle *db, const char *uid) {
  /* Breach records are served from breach_items via the adapter, not intel_items. */
  if (uid && strncmp(uid, "breach:", 7) == 0)
    return breach_adapter_item_by_uid(db, uid);
  static const char *Q =
    "SELECT " ITEM_COLS " FROM intel_items WHERE uid=?1";
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, Q, -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) { sqlite3_finalize(s); return NULL; }
  cJSON *out = row_to_item(s, 1);
  sqlite3_finalize(s);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env,"data", out);
  char *js = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return js;
}

/* GET /api/intel/items — faithful port of intelStore.listItems({}):
 * structured WHERE (source/since/until/lang/tag/record_type/sub_source_id/
 * has_geom) + keyset cursor; ?q= goes through intel_items_fts (segmented
 * exactly like the write path, == entityapi_search / Node intelMirror.search,
 * caller-composed extra WHERE). ORDER/limit/cursor/envelope unchanged, so a
 * query with only `limit` set is byte-identical to the prior contract. */
char *intelapi_list_items(db_handle *db, const intel_items_query *Q) {
  /* A source-filtered request for a breach source is served from breach_items
   * via the adapter (keyset over row id). Unfiltered / non-breach requests stay
   * on intel_items, so breach volume never enters the operational feed. */
  if (Q && Q->source && *Q->source && breach_meta_is_source(db, Q->source))
    return breach_adapter_list(db, Q->source, Q->q, Q->cursor, Q->limit);

  int safe = Q && Q->limit > 0 ? Q->limit : 50;
  if (safe < 1) safe = 1;
  if (safe > 200) safe = 200;

  /* decode keyset cursor {"p":pub_or_fetched,"u":uid}; bad cursor ignored */
  char *cur_p = NULL, *cur_u = NULL; cJSON *curj = NULL;
  if (Q && Q->cursor && *Q->cursor) {
    char *dec = b64url_decode(Q->cursor);
    if (dec) {
      curj = cJSON_Parse(dec); free(dec);
      if (curj) {
        cJSON *jp = cJSON_GetObjectItem(curj, "p");
        cJSON *ju = cJSON_GetObjectItem(curj, "u");
        if (cJSON_IsString(jp)) cur_p = jp->valuestring;
        if (cJSON_IsString(ju)) cur_u = ju->valuestring;
      }
    }
  }

  /* WHERE fragments + ordered binds — Node listItems() order verbatim. */
  char wbuf[4096]; size_t wl = 0; wbuf[0] = 0;
  const char *bnd[24]; int nb = 0;
#define WPUSH(frag) do { int _n = snprintf(wbuf + wl, sizeof wbuf - wl, \
    " AND %s", (frag)); if (_n > 0) wl += (size_t)_n; \
    if (wl >= sizeof wbuf) wl = sizeof wbuf - 1; } while (0)
  if (Q && Q->source)        { WPUSH("intel_items.source_id = ?");        bnd[nb++] = Q->source; }
  if (Q && Q->since)         { WPUSH("COALESCE(intel_items.published_at,intel_items.fetched_at) >= ?"); bnd[nb++] = Q->since; }
  if (Q && Q->until)         { WPUSH("COALESCE(intel_items.published_at,intel_items.fetched_at) <= ?"); bnd[nb++] = Q->until; }
  if (Q && Q->lang)          { WPUSH("intel_items.language = ?");          bnd[nb++] = Q->lang; }
  if (Q && Q->tag)           { WPUSH("EXISTS (SELECT 1 FROM json_each(intel_items.tags) WHERE json_each.value = ?)"); bnd[nb++] = Q->tag; }
  if (Q && Q->record_type)   { WPUSH("intel_items.record_type = ?");       bnd[nb++] = Q->record_type; }
  if (Q && Q->sub_source_id) { WPUSH("intel_items.sub_source_id = ?");     bnd[nb++] = Q->sub_source_id; }
  if (Q && Q->has_geom && !strcmp(Q->has_geom, "yes")) WPUSH("intel_items.lat IS NOT NULL");
  if (Q && Q->has_geom && !strcmp(Q->has_geom, "no"))  WPUSH("intel_items.lat IS NULL");
  if (cur_p) { WPUSH("(COALESCE(intel_items.published_at,intel_items.fetched_at) < ? OR "
                     "(COALESCE(intel_items.published_at,intel_items.fetched_at) = ? AND intel_items.uid > ?))");
               bnd[nb++] = cur_p; bnd[nb++] = cur_p; bnd[nb++] = cur_u; }
#undef WPUSH

  int has_q = Q && Q->q && *Q->q;
  char *segq = has_q ? fts_segment(Q->q) : NULL;   /* malloc'd; passthrough-safe */

  char sql[5600];
  if (has_q) {
    /* FTS branch: intel_items_fts.uid → intel_items.uid (== entityapi_search
     * convention); MATCH is bind ?1, structured WHERE appended after. */
    snprintf(sql, sizeof sql,
      "SELECT " ITEM_COLS_Q " FROM intel_items "
      "JOIN intel_items_fts ON intel_items_fts.uid = intel_items.uid "
      "WHERE intel_items_fts MATCH ?%s "
      "ORDER BY COALESCE(intel_items.published_at,intel_items.fetched_at) DESC, "
      "intel_items.uid ASC LIMIT ?",
      wbuf);                                       /* wbuf already " AND …"/"" */
  } else {
    char wclause[4200];
    if (wl) snprintf(wclause, sizeof wclause, "WHERE%s", wbuf + 4); /* drop " AND" */
    else    wclause[0] = 0;
    snprintf(sql, sizeof sql,
      "SELECT " ITEM_COLS " FROM intel_items %s "
      "ORDER BY COALESCE(published_at,fetched_at) DESC, uid ASC LIMIT ?",
      wclause);
  }

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    free(segq); if (curj) cJSON_Delete(curj); return NULL;
  }
  int bi = 1;
  if (has_q) sqlite3_bind_text(s, bi++, segq, -1, SQLITE_TRANSIENT);
  for (int i = 0; i < nb; i++) sqlite3_bind_text(s, bi++, bnd[i], -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, bi++, safe);

  cJSON *data = cJSON_CreateArray();
  int n = 0;
  char last_pub[256] = {0}, last_uid[512] = {0};
  while (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *it = row_to_item(s, 0);
    cJSON_AddItemToArray(data, it);
    /* track last row's cursor seed: published_at ?? fetched_at, uid */
    const char *p = ctext(s, 8); if (!p) p = ctext(s, 9);
    snprintf(last_pub, sizeof last_pub, "%s", p ? p : "");
    snprintf(last_uid, sizeof last_uid, "%s", ctext(s, 0) ? ctext(s, 0) : "");
    n++;
  }
  sqlite3_finalize(s);
  free(segq);

  cJSON *page = cJSON_CreateObject();
  if (n == safe) {
    cJSON *cur = cJSON_CreateObject();
    cJSON_AddStringToObject(cur, "p", last_pub);
    cJSON_AddStringToObject(cur, "u", last_uid);
    char *cj = cJSON_PrintUnformatted(cur);
    cJSON_Delete(cur);
    char *enc = b64url(cj);
    cJSON_AddStringToObject(page, "next_cursor", enc);
    free(cj); free(enc);
  } else {
    cJSON_AddNullToObject(page, "next_cursor");
  }
  cJSON_AddNumberToObject(page, "limit", safe);
  cJSON_AddNullToObject(page, "total");

  cJSON *filters = cJSON_CreateObject();   /* echo applied filters (Node route
   * meta.filters); all-NULL input → all null → byte-identical to old output */
  add_str_or_null(filters, "source",        Q ? Q->source : NULL);
  add_str_or_null(filters, "q",             Q ? Q->q : NULL);
  add_str_or_null(filters, "q_alt",         NULL);   /* qAlt is a route concern */
  add_str_or_null(filters, "lang",          Q ? Q->lang : NULL);
  add_str_or_null(filters, "record_type",   Q ? Q->record_type : NULL);
  add_str_or_null(filters, "sub_source_id", Q ? Q->sub_source_id : NULL);
  add_str_or_null(filters, "has_geom",      Q ? Q->has_geom : NULL);

  char ts[40]; iso_now(ts, sizeof ts);
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "fetched_at", ts);
  cJSON_AddItemToObject(meta, "filters", filters);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", data);
  cJSON_AddItemToObject(env, "page", page);
  cJSON_AddItemToObject(env, "meta", meta);
  char *js = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  if (curj) cJSON_Delete(curj);
  return js;
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

/* intelCatalog.js INTEL_SOURCE_SET — sources that emit kind:'intel'. */
static const char *INTEL_IDS[] = {
  "certstream-jp","bird-makeup-jp","chan-5ch","fofa-jp",
  "github-leaks-jp","grayhat-buckets","greynoise-jp","hatena-bookmark",
  "houjin-bangou","mercari-trending","misskey-timeline","note-com-trending",
  "urlscan-jp","wayback-jp",
  "edinet-filings","boj-stats","data-go-jp-ckan","egov-laws",
  "geospatial-jp-ckan","kyodo-rss","nhk-world-rss","jcg-navarea","nict-atlas",
  "ripestat-jp","wifi-hotspots-jcfw","wifi-hotspots-freespot",
  "jp-news-rss","nhk-news-rss","yahoo-news-jp-rss","ipa-alerts",
  "jpcert-alerts","phishing-feeds-jp","sans-isc-feeds","my-jvn",
};
static int is_intel_id(const char *id) {
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
  while (sqlite3_step(s) == SQLITE_ROW) {
    if (na == cap) { cap *= 2; A = realloc(A, cap * sizeof *A); }
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
    const char *id = (pass == 0) ? defs[i]->id : src_meta_at(i)->id;
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
    cJSON_AddBoolToObject(o, "is_intel", is_intel_id(id));

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
    cJSON_AddBoolToObject(o, "is_intel", is_intel_id(id));

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
