/* core/timelineapi.c — GET /api/timeline: histogram + keyset-paged merge of
 * intel_items, entity_mentions and alert_events into one temporal stream.
 *
 * Design notes that are not obvious from the SQL (the full contract, the
 * tenant-isolation argument and the httpd.c wiring live in timelineapi.h):
 *
 * 1. Every timestamp is normalised with strftime('%Y-%m-%dT%H:%M:%SZ', …)
 *    before it is compared, ordered or emitted. The three tables genuinely
 *    disagree on storage format (Node ISO with 'T'/'Z' vs SQLite
 *    "YYYY-MM-DD HH:MM:SS"), and raw string comparison across them is wrong.
 *    Normalising also gives the merged keyset a single comparable sort key,
 *    which is the whole reason a cross-stream cursor can exist at all.
 * 2. The three streams are UNION ALL'd, but each arm carries its OWN
 *    "ORDER BY ts DESC, k ASC LIMIT ?" inside a subquery before the union.
 *    The global newest-N of a union is always contained in the union of the
 *    per-stream newest-N, so this is exact — and it stops SQLite from
 *    materialising and sorting an entire 7-day window (millions of rows) just
 *    to hand back 100.
 * 3. The keyset tiebreaker is `k`, a synthetic per-event key ("i:"/"m:"/"a:"
 *    prefixed), not the item uid: the same uid legitimately appears in all
 *    three streams, and a keyset cursor whose (ts,uid) pair is not unique
 *    silently drops or repeats rows at page boundaries.
 * 4. Buckets deliberately ignore cursor/limit — a histogram that reshapes as
 *    the user pages is a bug, not a feature.
 *
 * b64url()/b64url_decode() are duplicated from intelapi.c (they are static
 * there). The encoding must stay byte-identical: clients hold one cursor
 * parser for /api/intel/items and /api/timeline. */
#include "timelineapi.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* strftime() format literals, kept as SQL string literals so they can be
 * spliced into generated SQL with %s — never as a printf format, or the %Y
 * would be eaten by snprintf. */
#define TSF "'%Y-%m-%dT%H:%M:%SZ'"
#define BKH "'%Y-%m-%dT%H:00:00Z'"
#define BKD "'%Y-%m-%dT00:00:00Z'"

#define MAX_WINDOW_DAYS 366.0

static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static char *err(int *st, int code, const char *msg) {
  *st = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", msg);
  char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}
static void add_str_or_null(cJSON *o, const char *k, const char *v) {
  cJSON_AddItemToObject(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}
static void iso_now(char *buf, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000));
}

/* ── cursor codec (byte-identical to intelapi.c) ───────────────────────────── */
static char *b64url(const char *in) {
  static const char T[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  size_t len = strlen(in);
  char *out = malloc(((len + 2) / 3) * 4 + 1);
  if (!out) return NULL;
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

/* ── query-string parsing ──────────────────────────────────────────────────── */
static int hexv(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
/* Percent/plus-decoding lookup over the raw query string. Returns 1 if `key`
 * was present (value may still be empty). We parse here rather than in
 * httpd.c so the whole /api/timeline parameter contract lives in one file. */
static int qs_get(const char *qs, const char *key, char *out, size_t n) {
  if (!out || n == 0) return 0;
  out[0] = 0;
  if (!qs || !*qs) return 0;
  size_t kl = strlen(key);
  const char *p = qs;
  for (;;) {
    const char *amp = strchr(p, '&');
    const char *end = amp ? amp : p + strlen(p);
    const char *eqp = memchr(p, '=', (size_t)(end - p));
    if (eqp && (size_t)(eqp - p) == kl && strncmp(p, key, kl) == 0) {
      size_t o = 0;
      for (const char *v = eqp + 1; v < end && o + 1 < n; v++) {
        if (*v == '+') { out[o++] = ' '; continue; }
        if (*v == '%' && v + 2 < end) {
          int h = hexv((unsigned char)v[1]), l = hexv((unsigned char)v[2]);
          if (h >= 0 && l >= 0) { out[o++] = (char)(h * 16 + l); v += 2; continue; }
        }
        out[o++] = *v;
      }
      out[o] = 0;
      return 1;
    }
    if (!amp) return 0;
    p = amp + 1;
  }
}

static int table_exists(sqlite3 *h, const char *name) {
  sqlite3_stmt *s; int found = 0;
  if (sqlite3_prepare_v2(h,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1",
        -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, name, -1, SQLITE_TRANSIENT);
  found = sqlite3_step(s) == SQLITE_ROW;
  sqlite3_finalize(s);
  return found;
}

/* ── typed bind list (SQL and binds are built in lockstep) ─────────────────── */
enum { BT_TEXT = 0, BT_REAL, BT_INT };
#define BMAX 64
typedef struct { int t; const char *s; double d; int i; } bindv;
static void bt(bindv *a, int *n, const char *s) {
  if (*n >= BMAX) return;
  a[*n].t = BT_TEXT; a[*n].s = s; (*n)++;
}
static void br(bindv *a, int *n, double d) {
  if (*n >= BMAX) return;
  a[*n].t = BT_REAL; a[*n].d = d; (*n)++;
}
static void bi(bindv *a, int *n, int i) {
  if (*n >= BMAX) return;
  a[*n].t = BT_INT; a[*n].i = i; (*n)++;
}
static int bind_all(sqlite3_stmt *s, const bindv *a, int n) {
  for (int i = 0; i < n; i++) {
    int rc;
    if (a[i].t == BT_TEXT)      rc = sqlite3_bind_text(s, i + 1, a[i].s, -1, SQLITE_TRANSIENT);
    else if (a[i].t == BT_REAL) rc = sqlite3_bind_double(s, i + 1, a[i].d);
    else                        rc = sqlite3_bind_int(s, i + 1, a[i].i);
    if (rc != SQLITE_OK) return -1;
  }
  return 0;
}

/* Resolved, validated request. All char* point at caller-owned storage that
 * outlives every prepared statement (binds use SQLITE_TRANSIENT anyway). */
typedef struct {
  const char *tenant;
  const char *since, *until;      /* already normalised to TSF shape */
  const char *bfmt;               /* BKH or BKD */
  const char *source;             /* NULL = unfiltered */
  int    has_bbox; double bw, bs, be, bn;
  const char *case_id;            /* NULL = unfiltered */
  const char *cur_p, *cur_u;      /* NULL = first page */
  int    limit;
} tq;

/* Per-stream SQL vocabulary. Index 0 = intel_items, 1 = entity_mentions,
 * 2 = alert_events; the arrays are parallel and the loop below is written once
 * for all three. `i` is always the intel item a row hangs off — the sole
 * source of title/source_id/lat/lon, and (for stream 1) the tenant predicate. */
static const char *TSCOL[3] = {
  "COALESCE(i.published_at,i.fetched_at)", "em.created_at", "e.matched_at" };
static const char *FROMX[3] = {
  "FROM intel_items i",
  /* INNER: a mention whose parent item is absent or belongs to another tenant
   * is dropped. Excluding beats leaking. */
  "FROM entity_mentions em "
  "JOIN intel_items i ON i.uid=em.item_uid AND i.tenant_id=?",
  /* LEFT: the event is tenant-scoped by its own column; its decorations are
   * only borrowed from an item of the SAME tenant, else they stay NULL. */
  "FROM alert_events e "
  "LEFT JOIN intel_items i ON i.uid=e.item_uid AND i.tenant_id=?" };
static const char *BASEX[3] = { "i.tenant_id=?", "1=1", "e.tenant_id=?" };
static const char *KINDX[3] = { "intel", "mention", "alert" };
static const char *UIDX[3]  = { "i.uid", "em.item_uid", "e.item_uid" };
static const char *KEYX[3]  = {
  "('i:'||i.uid)",
  "('m:'||em.entity_id||char(31)||em.item_uid||char(31)||COALESCE(em.field,''))",
  "('a:'||e.id)" };
static const char *REFX[3]  = { "NULL", "em.entity_id", "e.rule_id" };
static const char *CNTX[3]  = { "COUNT(*) AS ci,0 AS cm,0 AS ca",
                                "0 AS ci,COUNT(*) AS cm,0 AS ca",
                                "0 AS ci,0 AS cm,COUNT(*) AS ca" };

/* Build the buckets (events=0) or events (events=1) statement text plus its
 * ordered bind list. Returns 0, or -1 if anything would truncate (caller →
 * 500; a truncated SQL string must never reach sqlite3_prepare_v2). */
static int build_sql(const tq *q, int events, char *sql, size_t cap,
                     bindv *B, int *nb) {
  char arms[3][2600];
  *nb = 0;
  for (int k = 0; k < 3; k++) {
    char tsn[160], bx[160], wh[1200], tmp[600];
    size_t wl = 0; wh[0] = 0;
    if (snprintf(tsn, sizeof tsn, "strftime(%s,%s)", TSF, TSCOL[k]) >= (int)sizeof tsn)
      return -1;
    if (snprintf(bx, sizeof bx, "strftime(%s,%s)", q->bfmt, TSCOL[k]) >= (int)sizeof bx)
      return -1;

    /* tenant binds first, in SQL text order: FROM-clause join, then WHERE. */
    if (k == 1 || k == 2) bt(B, nb, q->tenant);
    if (k == 0 || k == 2) bt(B, nb, q->tenant);
    bt(B, nb, q->since);
    bt(B, nb, q->until);

#define WP(frag) do { int _n = snprintf(wh + wl, sizeof wh - wl, " AND %s", (frag)); \
    if (_n < 0 || (size_t)_n >= sizeof wh - wl) { return -1; }                       \
    wl += (size_t)_n; } while (0)
    if (q->source)   { WP("i.source_id=?"); bt(B, nb, q->source); }
    if (q->has_bbox) { WP("i.lat BETWEEN ? AND ? AND i.lon BETWEEN ? AND ?");
                       br(B, nb, q->bs); br(B, nb, q->bn);
                       br(B, nb, q->bw); br(B, nb, q->be); }
    if (q->case_id)  {
      if (snprintf(tmp, sizeof tmp,
            "%s IN (SELECT ref_id FROM case_items WHERE case_id=?)",
            UIDX[k]) >= (int)sizeof tmp) return -1;
      WP(tmp); bt(B, nb, q->case_id);
    }
    if (events && q->cur_p) {
      /* identical shape to intelapi.c's keyset predicate, with the synthetic
       * event key standing in for uid. */
      if (snprintf(tmp, sizeof tmp, "(%s < ? OR (%s = ? AND %s > ?))",
                   tsn, tsn, KEYX[k]) >= (int)sizeof tmp) return -1;
      WP(tmp); bt(B, nb, q->cur_p); bt(B, nb, q->cur_p); bt(B, nb, q->cur_u);
    }
#undef WP

    int n;
    if (events) {
      n = snprintf(arms[k], sizeof arms[k],
        "SELECT * FROM (SELECT '%s' AS kind,%s AS ts,%s AS uid,%s AS k,"
        "i.title AS title,i.source_id AS source_id,i.lat AS lat,i.lon AS lon,"
        "%s AS ref %s WHERE %s AND %s BETWEEN ? AND ?%s "
        "ORDER BY ts DESC,k ASC LIMIT ?)",
        KINDX[k], tsn, UIDX[k], KEYX[k], REFX[k], FROMX[k], BASEX[k], tsn, wh);
      bi(B, nb, q->limit);
    } else {
      n = snprintf(arms[k], sizeof arms[k],
        "SELECT %s AS b,%s %s WHERE %s AND %s BETWEEN ? AND ?%s GROUP BY b",
        bx, CNTX[k], FROMX[k], BASEX[k], tsn, wh);
    }
    if (n < 0 || (size_t)n >= sizeof arms[k]) return -1;
  }
  if (*nb >= BMAX) return -1;

  int n;
  if (events) {
    n = snprintf(sql, cap,
      "SELECT kind,ts,uid,k,title,source_id,lat,lon,ref FROM "
      "(%s UNION ALL %s UNION ALL %s) ORDER BY ts DESC,k ASC LIMIT ?",
      arms[0], arms[1], arms[2]);
    bi(B, nb, q->limit);
  } else {
    n = snprintf(sql, cap,
      "SELECT b,SUM(ci),SUM(cm),SUM(ca) FROM (%s UNION ALL %s UNION ALL %s) "
      "WHERE b IS NOT NULL GROUP BY b ORDER BY b ASC",
      arms[0], arms[1], arms[2]);
  }
  if (n < 0 || (size_t)n >= cap) return -1;
  return *nb < BMAX ? 0 : -1;
}

/* Resolve the window through SQLite itself: the same parser that will read the
 * columns decides what the bounds mean, so "2026-07-01", "now" and
 * "2026-07-01T00:00:00.000Z" are accepted or rejected consistently. Returns 0,
 * -1 on a db error, -2 if a bound is unparseable. */
static int resolve_window(sqlite3 *h, const char *since_in, const char *until_in,
                          char *so, size_t sn, char *uo, size_t un, double *span) {
  const char *SX = since_in ? "?1" : "'now','-7 days'";
  const char *UX = until_in ? "?2" : "'now'";
  char sql[320];
  snprintf(sql, sizeof sql,
    "SELECT strftime(%s,%s),strftime(%s,%s),julianday(%s)-julianday(%s)",
    TSF, SX, TSF, UX, UX, SX);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, sql, -1, &s, NULL) != SQLITE_OK) return -1;
  if (since_in) sqlite3_bind_text(s, 1, since_in, -1, SQLITE_TRANSIENT);
  if (until_in) sqlite3_bind_text(s, 2, until_in, -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(s), ret = -1;
  if (rc == SQLITE_ROW) {
    const char *a = ctext(s, 0), *b = ctext(s, 1);
    if (!a || !b || sqlite3_column_type(s, 2) == SQLITE_NULL) {
      ret = -2;
    } else {
      snprintf(so, sn, "%s", a);
      snprintf(uo, un, "%s", b);
      *span = sqlite3_column_double(s, 2);
      ret = 0;
    }
  }
  sqlite3_finalize(s);
  return ret;
}

char *timelineapi_query(db_handle *db, const tenant_ctx *t,
                        const char *query_string, int *status) {
  if (!status) return NULL;
  *status = 200;
  if (!db || !db->h || !t) return err(status, 500, "server_error");

  char v_since[96], v_until[96], v_bucket[24], v_source[192],
       v_bbox[192], v_case[96], v_limit[24], v_cursor[768];
  int has_since  = qs_get(query_string, "since",   v_since,  sizeof v_since)  && v_since[0];
  int has_until  = qs_get(query_string, "until",   v_until,  sizeof v_until)  && v_until[0];
  qs_get(query_string, "bucket",  v_bucket, sizeof v_bucket);
  qs_get(query_string, "source",  v_source, sizeof v_source);
  qs_get(query_string, "bbox",    v_bbox,   sizeof v_bbox);
  qs_get(query_string, "case_id", v_case,   sizeof v_case);
  qs_get(query_string, "limit",   v_limit,  sizeof v_limit);
  qs_get(query_string, "cursor",  v_cursor, sizeof v_cursor);

  tq q;
  memset(&q, 0, sizeof q);
  q.tenant = t->tenant_id;
  q.limit  = 100;

  q.bfmt = BKD;
  int by_hour = 0;
  if (v_bucket[0]) {
    if      (strcmp(v_bucket, "hour") == 0) { q.bfmt = BKH; by_hour = 1; }
    else if (strcmp(v_bucket, "day")  == 0) { q.bfmt = BKD; }
    else return err(status, 400, "invalid_bucket");
  }

  if (v_limit[0]) {
    char *e; long L = strtol(v_limit, &e, 10);
    if (e == v_limit || *e) return err(status, 400, "invalid_limit");
    if (L < 1)   L = 1;
    if (L > 500) L = 500;
    q.limit = (int)L;
  }

  if (v_bbox[0]) {
    double c[4]; const char *p = v_bbox; char *e;
    for (int i = 0; i < 4; i++) {
      c[i] = strtod(p, &e);
      if (e == p) return err(status, 400, "invalid_bbox");
      if (i < 3) { if (*e != ',') return err(status, 400, "invalid_bbox"); p = e + 1; }
      else if (*e) return err(status, 400, "invalid_bbox");
    }
    /* w,s,e,n — reject an inverted or out-of-range box rather than silently
     * returning nothing; a wrapped antimeridian box is not supported. */
    if (c[0] > c[2] || c[1] > c[3] ||
        c[1] < -90 || c[3] > 90 || c[0] < -180 || c[2] > 180)
      return err(status, 400, "invalid_bbox");
    q.has_bbox = 1; q.bw = c[0]; q.bs = c[1]; q.be = c[2]; q.bn = c[3];
  }

  char since_n[64] = {0}, until_n[64] = {0};
  double span = 0;
  int wr = resolve_window(db->h, has_since ? v_since : NULL,
                          has_until ? v_until : NULL,
                          since_n, sizeof since_n, until_n, sizeof until_n, &span);
  if (wr == -1) return err(status, 500, "server_error");
  if (wr == -2) return err(status, 400, "invalid_time");
  if (span < 0) return err(status, 400, "invalid_window");
  if (span > MAX_WINDOW_DAYS) return err(status, 400, "window_too_wide");
  q.since = since_n; q.until = until_n;
  if (v_source[0]) q.source = v_source;

  cJSON *notes = cJSON_CreateArray();

  /* case_id: roadmap 17 may not have landed here yet. See the header for why
   * a missing case_items degrades to "empty" and not to "unfiltered". */
  int skip_all = 0;
  if (v_case[0]) {
    if (!table_exists(db->h, "case_items")) {
      cJSON_AddItemToArray(notes, cJSON_CreateString("case_items_table_missing"));
      skip_all = 1;
    } else if (table_exists(db->h, "cases")) {
      sqlite3_stmt *s; int owned = 0;
      if (sqlite3_prepare_v2(db->h,
            "SELECT 1 FROM cases WHERE id=?1 AND tenant_id=?2", -1, &s, NULL)
          != SQLITE_OK) { cJSON_Delete(notes); return err(status, 500, "server_error"); }
      sqlite3_bind_text(s, 1, v_case, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(s, 2, t->tenant_id, -1, SQLITE_TRANSIENT);
      owned = sqlite3_step(s) == SQLITE_ROW;
      sqlite3_finalize(s);
      if (!owned) { cJSON_Delete(notes); return err(status, 404, "not_found"); }
      q.case_id = v_case;
    } else {
      /* No `cases` table to check ownership against. Every stream is still
       * tenant-filtered, so honouring the pin list cannot leak. */
      cJSON_AddItemToArray(notes, cJSON_CreateString("cases_table_missing"));
      q.case_id = v_case;
    }
  }

  /* cursor {"p":ts,"u":key}; a malformed cursor is ignored (first page), the
   * same forgiving behaviour as intelapi.c. */
  cJSON *curj = NULL;
  if (v_cursor[0]) {
    char *dec = b64url_decode(v_cursor);
    if (dec) {
      curj = cJSON_Parse(dec); free(dec);
      if (curj) {
        cJSON *jp = cJSON_GetObjectItem(curj, "p");
        cJSON *ju = cJSON_GetObjectItem(curj, "u");
        if (cJSON_IsString(jp) && cJSON_IsString(ju)) {
          q.cur_p = jp->valuestring; q.cur_u = ju->valuestring;
        }
      }
    }
    if (!q.cur_p) cJSON_AddItemToArray(notes, cJSON_CreateString("cursor_ignored"));
  }

  cJSON *buckets = cJSON_CreateArray();
  cJSON *events  = cJSON_CreateArray();
  long long tot_i = 0, tot_m = 0, tot_a = 0;
  int nrows = 0;
  char last_ts[64] = {0}, last_key[512] = {0};

  if (!skip_all) {
    char sql[9600]; bindv B[BMAX]; int nb = 0;
    sqlite3_stmt *s;

    /* 1. histogram over the whole window */
    if (build_sql(&q, 0, sql, sizeof sql, B, &nb) != 0 ||
        sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
      cJSON_Delete(notes); cJSON_Delete(buckets); cJSON_Delete(events);
      if (curj) cJSON_Delete(curj);
      return err(status, 500, "server_error");
    }
    if (bind_all(s, B, nb) != 0) {
      sqlite3_finalize(s);
      cJSON_Delete(notes); cJSON_Delete(buckets); cJSON_Delete(events);
      if (curj) cJSON_Delete(curj);
      return err(status, 500, "server_error");
    }
    while (sqlite3_step(s) == SQLITE_ROW) {
      long long ci = sqlite3_column_int64(s, 1);
      long long cm = sqlite3_column_int64(s, 2);
      long long ca = sqlite3_column_int64(s, 3);
      cJSON *b = cJSON_CreateObject();
      cJSON_AddStringToObject(b, "t", ctext(s, 0) ? ctext(s, 0) : "");
      cJSON_AddNumberToObject(b, "intel",    (double)ci);
      cJSON_AddNumberToObject(b, "mentions", (double)cm);
      cJSON_AddNumberToObject(b, "alerts",   (double)ca);
      cJSON_AddItemToArray(buckets, b);
      tot_i += ci; tot_m += cm; tot_a += ca;
    }
    sqlite3_finalize(s);

    /* 2. merged, keyset-paged event page */
    nb = 0;
    if (build_sql(&q, 1, sql, sizeof sql, B, &nb) != 0 ||
        sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
      cJSON_Delete(notes); cJSON_Delete(buckets); cJSON_Delete(events);
      if (curj) cJSON_Delete(curj);
      return err(status, 500, "server_error");
    }
    if (bind_all(s, B, nb) != 0) {
      sqlite3_finalize(s);
      cJSON_Delete(notes); cJSON_Delete(buckets); cJSON_Delete(events);
      if (curj) cJSON_Delete(curj);
      return err(status, 500, "server_error");
    }
    while (sqlite3_step(s) == SQLITE_ROW) {
      cJSON *o = cJSON_CreateObject();
      add_str_or_null(o, "kind",      ctext(s, 0));
      add_str_or_null(o, "ts",        ctext(s, 1));
      add_str_or_null(o, "uid",       ctext(s, 2));
      add_str_or_null(o, "title",     ctext(s, 4));
      add_str_or_null(o, "source_id", ctext(s, 5));
      cJSON_AddItemToObject(o, "lat", sqlite3_column_type(s, 6) == SQLITE_NULL
        ? cJSON_CreateNull() : cJSON_CreateNumber(sqlite3_column_double(s, 6)));
      cJSON_AddItemToObject(o, "lon", sqlite3_column_type(s, 7) == SQLITE_NULL
        ? cJSON_CreateNull() : cJSON_CreateNumber(sqlite3_column_double(s, 7)));
      add_str_or_null(o, "ref",       ctext(s, 8));
      add_str_or_null(o, "key",       ctext(s, 3));
      cJSON_AddItemToArray(events, o);
      snprintf(last_ts,  sizeof last_ts,  "%s", ctext(s, 1) ? ctext(s, 1) : "");
      snprintf(last_key, sizeof last_key, "%s", ctext(s, 3) ? ctext(s, 3) : "");
      nrows++;
    }
    sqlite3_finalize(s);
  }

  cJSON *data = cJSON_CreateObject();
  cJSON_AddItemToObject(data, "buckets", buckets);
  cJSON_AddItemToObject(data, "events",  events);

  cJSON *page = cJSON_CreateObject();
  if (nrows == q.limit && last_ts[0]) {
    cJSON *cur = cJSON_CreateObject();
    cJSON_AddStringToObject(cur, "p", last_ts);
    cJSON_AddStringToObject(cur, "u", last_key);
    char *cj = cJSON_PrintUnformatted(cur);
    cJSON_Delete(cur);
    char *enc = cj ? b64url(cj) : NULL;
    if (enc) cJSON_AddStringToObject(page, "next_cursor", enc);
    else     cJSON_AddNullToObject(page, "next_cursor");
    free(cj); free(enc);
  } else {
    cJSON_AddNullToObject(page, "next_cursor");
  }
  cJSON_AddNumberToObject(page, "limit", q.limit);
  cJSON_AddNullToObject(page, "total");

  cJSON *totals = cJSON_CreateObject();
  cJSON_AddNumberToObject(totals, "intel",    (double)tot_i);
  cJSON_AddNumberToObject(totals, "mentions", (double)tot_m);
  cJSON_AddNumberToObject(totals, "alerts",   (double)tot_a);

  cJSON *filters = cJSON_CreateObject();
  add_str_or_null(filters, "source",  q.source);
  add_str_or_null(filters, "bbox",    v_bbox[0] ? v_bbox : NULL);
  add_str_or_null(filters, "case_id", v_case[0] ? v_case : NULL);

  char ts[40]; iso_now(ts, sizeof ts);
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "fetched_at", ts);
  cJSON_AddStringToObject(meta, "bucket", by_hour ? "hour" : "day");
  cJSON_AddStringToObject(meta, "since", since_n);
  cJSON_AddStringToObject(meta, "until", until_n);
  cJSON_AddItemToObject(meta, "totals",  totals);
  cJSON_AddItemToObject(meta, "filters", filters);
  cJSON_AddItemToObject(meta, "notes",   notes);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", data);
  cJSON_AddItemToObject(env, "page", page);
  cJSON_AddItemToObject(env, "meta", meta);
  char *js = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  if (curj) cJSON_Delete(curj);
  if (!js) { *status = 500; return NULL; }
  return js;
}
