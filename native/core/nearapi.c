/* core/nearapi.c — proximity search over intel_items (roadmap item 32).
 * Contract, index rationale and the row-shape argument are in nearapi.h. */
#include "nearapi.h"
#include "intelapi.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NEAR_RADIUS_DEF   2000.0
#define NEAR_RADIUS_MAX   200000.0     /* 200 km — beyond this the bbox
                                        * prefilter stops filtering anything */
#define NEAR_LIMIT_DEF    50
#define NEAR_LIMIT_MAX    200
/* Ceiling on rows the refine pass will inspect. A 200 km bbox over a corpus
 * with millions of geocoded rows would otherwise turn one request into a
 * full-index walk; when this trips the response says so instead of silently
 * returning the nearest of an arbitrary prefix. */
#define NEAR_SCAN_MAX     200000

/* Append to a bounded buffer, clamping the cursor. snprintf returns what it
 * WOULD have written, so an unclamped cursor can walk past the end and make
 * the next `cap - cursor` wrap to a huge size_t — writing far outside the
 * allocation. The fragments appended below are short literals and the buffer
 * is ample, so this cannot currently overflow; it is clamped anyway because
 * this is the exact shape the 2026-08-07 audit found unguarded in 12
 * collectors, and `make lint-sources` checks for it. */
static void sql_append(char *buf, size_t cap, int *o, const char *fmt, ...) {
  if (*o < 0 || (size_t) *o >= cap) return;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf + *o, cap - (size_t) *o, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  *o = ((size_t) *o + (size_t) n >= cap) ? (int) cap - 1 : *o + n;
}

static int hexv(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
static void url_decode(const char *s, size_t n, char *out, size_t cap) {
  size_t o = 0;
  for (size_t i = 0; i < n && o + 1 < cap; i++) {
    int c = (unsigned char) s[i];
    if (c == '+') { out[o++] = ' '; continue; }
    if (c == '%' && i + 2 < n) {
      int h = hexv((unsigned char) s[i + 1]), l = hexv((unsigned char) s[i + 2]);
      if (h >= 0 && l >= 0) { out[o++] = (char) ((h << 4) | l); i += 2; continue; }
    }
    out[o++] = (char) c;
  }
  out[o] = 0;
}
static int qget(const char *query, const char *key, char *out, size_t cap) {
  out[0] = 0;
  if (!query || !*query) return 0;
  size_t klen = strlen(key);
  const char *p = query;
  while (*p) {
    const char *amp = strchr(p, '&');
    const char *seg_end = amp ? amp : p + strlen(p);
    const char *eq = memchr(p, '=', (size_t) (seg_end - p));
    size_t kn = eq ? (size_t) (eq - p) : (size_t) (seg_end - p);
    if (kn == klen && memcmp(p, key, klen) == 0) {
      const char *vstart = eq ? eq + 1 : seg_end;
      url_decode(vstart, (size_t) (seg_end - vstart), out, cap);
      return 1;
    }
    if (!amp) break;
    p = amp + 1;
  }
  return 0;
}
static char *errj(int *st, int code, const char *msg) {
  *st = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", msg);
  char *j = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return j;
}
/* Same formula as alert_eval.c's circle refine. */
static double hav_m(double la1, double lo1, double la2, double lo2) {
  const double R = 6371000.0, D = M_PI / 180.0;
  double p1 = la1 * D, p2 = la2 * D;
  double dp = (la2 - la1) * D, dl = (lo2 - lo1) * D;
  double a = sin(dp / 2) * sin(dp / 2) +
             cos(p1) * cos(p2) * sin(dl / 2) * sin(dl / 2);
  if (a > 1.0) a = 1.0;
  return 2.0 * R * asin(sqrt(a));
}

/* 600, matching lib/geojson.c's feature_uid() buffer — the widest uid the fleet
 * can mint. At 512 a longer uid was truncated here, intelapi_item_by_uid() then
 * found nothing, and the row was dropped from `data` while still being counted
 * in `page.total`: a proximity hit that exists, is in range, and is invisible. */
typedef struct { char uid[600]; double d; } near_hit;
static int cmp_hit(const void *a, const void *b) {
  double da = ((const near_hit *) a)->d, db = ((const near_hit *) b)->d;
  return da < db ? -1 : (da > db ? 1 : 0);
}

char *nearapi_items(db_handle *db, const char *tenant, const char *near,
                    const char *qs, int *status) {
  *status = 200;

  double lat = 0, lon = 0;
  { char buf[128];
    snprintf(buf, sizeof buf, "%s", near ? near : "");
    char *comma = strchr(buf, ',');
    if (!comma) return errj(status, 400, "near_must_be_lat,lon");
    *comma = 0;
    char *e1 = NULL, *e2 = NULL;
    lat = strtod(buf, &e1);
    lon = strtod(comma + 1, &e2);
    if (e1 == buf || e2 == comma + 1) return errj(status, 400, "near_must_be_lat,lon");
    if (!(lat >= -90 && lat <= 90))   return errj(status, 400, "near_lat_invalid");
    if (!(lon >= -180 && lon <= 180)) return errj(status, 400, "near_lon_invalid"); }

  char v[256];
  double radius = NEAR_RADIUS_DEF;
  if (qget(qs, "radius_m", v, sizeof v) && *v) radius = strtod(v, NULL);
  if (!(radius > 0)) radius = NEAR_RADIUS_DEF;
  if (radius > NEAR_RADIUS_MAX) radius = NEAR_RADIUS_MAX;

  int limit = NEAR_LIMIT_DEF;
  if (qget(qs, "limit", v, sizeof v) && *v) limit = atoi(v);
  if (limit < 1) limit = NEAR_LIMIT_DEF;
  if (limit > NEAR_LIMIT_MAX) limit = NEAR_LIMIT_MAX;

  char f_source[160] = {0}, f_rtype[64] = {0}, f_since[48] = {0}, f_until[48] = {0};
  qget(qs, "source", f_source, sizeof f_source);
  qget(qs, "record_type", f_rtype, sizeof f_rtype);
  qget(qs, "since", f_since, sizeof f_since);
  qget(qs, "until", f_until, sizeof f_until);

  /* bbox prefilter. The longitudinal half-width uses the cosine at the query
   * latitude; near the poles that blows up, so it is clamped and the refine
   * pass does the real work. */
  double dlat = radius / 111320.0;
  double c = cos(lat * M_PI / 180.0);
  if (c < 0.01) c = 0.01;
  double dlon = radius / (111320.0 * c);
  if (dlon > 180.0) dlon = 180.0;

  char sql[900];
  int o = snprintf(sql, sizeof sql,
    "SELECT uid, lat, lon FROM intel_items "
    "WHERE lat IS NOT NULL AND lat BETWEEN ?1 AND ?2 AND lon BETWEEN ?3 AND ?4 "
    /* intel_items.tenant_id is NOT NULL DEFAULT 'legacy', so the shared
     * pre-tenancy corpus is the literal 'legacy' — not NULL. This is
     * exportapi.c:461's predicate for the same table; the "IS NULL OR"
     * form used for `entities` (which IS nullable) would silently return
     * nothing here. */
    "AND tenant_id IN (?5,'legacy')");
  int bi = 6;
  int b_source = 0, b_rtype = 0, b_since = 0, b_until = 0;
  if (f_source[0]) { b_source = bi++; sql_append(sql, sizeof sql, &o, " AND source_id=?%d", b_source); }
  if (f_rtype[0])  { b_rtype  = bi++; sql_append(sql, sizeof sql, &o, " AND record_type=?%d", b_rtype); }
  if (f_since[0])  { b_since  = bi++; sql_append(sql, sizeof sql, &o, " AND COALESCE(published_at,fetched_at)>=?%d", b_since); }
  if (f_until[0])  { b_until  = bi++; sql_append(sql, sizeof sql, &o, " AND COALESCE(published_at,fetched_at)<=?%d", b_until); }
  sql_append(sql, sizeof sql, &o, " LIMIT %d", NEAR_SCAN_MAX);

  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) != SQLITE_OK)
    return errj(status, 500, "query_failed");
  sqlite3_bind_double(st, 1, lat - dlat);
  sqlite3_bind_double(st, 2, lat + dlat);
  sqlite3_bind_double(st, 3, lon - dlon);
  sqlite3_bind_double(st, 4, lon + dlon);
  sqlite3_bind_text(st, 5, tenant ? tenant : "", -1, SQLITE_TRANSIENT);
  if (b_source) sqlite3_bind_text(st, b_source, f_source, -1, SQLITE_TRANSIENT);
  if (b_rtype)  sqlite3_bind_text(st, b_rtype,  f_rtype,  -1, SQLITE_TRANSIENT);
  if (b_since)  sqlite3_bind_text(st, b_since,  f_since,  -1, SQLITE_TRANSIENT);
  if (b_until)  sqlite3_bind_text(st, b_until,  f_until,  -1, SQLITE_TRANSIENT);

  int cap = 1024, n = 0, scanned = 0;
  near_hit *hits = malloc((size_t) cap * sizeof *hits);
  if (!hits) { sqlite3_finalize(st); return errj(status, 500, "oom"); }
  while (sqlite3_step(st) == SQLITE_ROW) {
    scanned++;
    const char *uid = (const char *) sqlite3_column_text(st, 0);
    if (!uid) continue;
    double d = hav_m(lat, lon, sqlite3_column_double(st, 1),
                     sqlite3_column_double(st, 2));
    if (d > radius) continue;
    if (n >= cap) {
      int nc = cap * 2;
      near_hit *nv = realloc(hits, (size_t) nc * sizeof *hits);
      if (!nv) break;
      hits = nv; cap = nc;
    }
    snprintf(hits[n].uid, sizeof hits[n].uid, "%s", uid);
    hits[n].d = d;
    n++;
  }
  sqlite3_finalize(st);
  qsort(hits, (size_t) n, sizeof *hits, cmp_hit);

  cJSON *data = cJSON_CreateArray();
  int emitted = n < limit ? n : limit;
  for (int i = 0; i < emitted; i++) {
    /* One canonical row builder for the whole product — see nearapi.h. */
    char *one = intelapi_item_by_uid(db, hits[i].uid);
    if (!one) continue;
    cJSON *env = cJSON_Parse(one);
    free(one);
    if (!env) continue;
    cJSON *row = cJSON_DetachItemFromObject(env, "data");
    cJSON_Delete(env);
    if (!row) continue;
    cJSON_AddNumberToObject(row, "distance_m", floor(hits[i].d + 0.5));
    cJSON_AddItemToArray(data, row);
  }
  free(hits);

  cJSON *page = cJSON_CreateObject();
  cJSON_AddNullToObject(page, "next_cursor");        /* see nearapi.h on paging */
  cJSON_AddNumberToObject(page, "limit", limit);
  cJSON_AddNumberToObject(page, "total", n);

  cJSON *filters = cJSON_CreateObject();
  cJSON *ctr = cJSON_CreateObject();
  cJSON_AddNumberToObject(ctr, "lat", lat);
  cJSON_AddNumberToObject(ctr, "lon", lon);
  cJSON_AddItemToObject(filters, "near", ctr);
  cJSON_AddNumberToObject(filters, "radius_m", radius);
  if (f_source[0]) cJSON_AddStringToObject(filters, "source", f_source);
  if (f_rtype[0])  cJSON_AddStringToObject(filters, "record_type", f_rtype);
  if (f_since[0])  cJSON_AddStringToObject(filters, "since", f_since);
  if (f_until[0])  cJSON_AddStringToObject(filters, "until", f_until);

  cJSON *meta = cJSON_CreateObject();
  char ts[40];
  { time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
    snprintf(ts, sizeof ts, "%04d-%02d-%02dT%02d:%02d:%02dZ",
             g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec); }
  cJSON_AddStringToObject(meta, "fetched_at", ts);
  cJSON_AddItemToObject(meta, "filters", filters);
  cJSON_AddStringToObject(meta, "order", "distance_m ASC");
  cJSON_AddNumberToObject(meta, "bbox_scanned", scanned);
  if (scanned >= NEAR_SCAN_MAX)
    cJSON_AddStringToObject(meta, "note",
      "bbox prefilter hit its scan ceiling; narrow the radius or add a filter "
      "— results are the nearest of an incomplete candidate set");

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", data);
  cJSON_AddItemToObject(env, "page", page);
  cJSON_AddItemToObject(env, "meta", meta);
  char *out = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  if (!out) return errj(status, 500, "oom");
  return out;
}
