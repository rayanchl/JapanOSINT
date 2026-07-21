/* core/transitapi.c — see transitapi.h. Node→C port of
 * server/src/routes/transit.js. SQL + JSON shapes reproduced verbatim from
 * the JS (and the helpers it imports: transportStore.getLinesByMode,
 * gtfsStore.getDeparturesAt/listHydratedOperators, gtfsActiveTrips
 * .getActiveTripsAt, odptStationTimetable.getStationTimetable). */
#include "transitapi.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

/* ---- transit.js constants (verbatim) -------------------------------- */
#define MAX_FEATURES 5000

/* ---- small helpers -------------------------------------------------- */

static char *dup_json(cJSON *o) {       /* print + delete, == res.json() */
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

static char *err_json(const char *msg) {        /* {"error":"<msg>"} */
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", msg);
  return dup_json(o);
}

/* hex 2-char → byte, or -1 */
static int hexv(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

/* In-place-ish percent + '+' decode of a query-string token into `out`
 * (NUL-terminated, truncated to cap). */
static void url_decode(const char *s, size_t n, char *out, size_t cap) {
  size_t o = 0;
  for (size_t i = 0; i < n && o + 1 < cap; i++) {
    int c = (unsigned char)s[i];
    if (c == '+') { out[o++] = ' '; continue; }
    if (c == '%' && i + 2 < n) {
      int h = hexv((unsigned char)s[i + 1]);
      int l = hexv((unsigned char)s[i + 2]);
      if (h >= 0 && l >= 0) { out[o++] = (char)((h << 4) | l); i += 2; continue; }
    }
    out[o++] = (char)c;
  }
  out[o] = 0;
}

/* Extract query param `key` from raw query string into `out`. Returns 1 if
 * present (even if empty value), else 0 and out[0]=0. */
static int qget(const char *query, const char *key, char *out, size_t cap) {
  out[0] = 0;
  if (!query || !*query) return 0;
  size_t klen = strlen(key);
  const char *p = query;
  while (*p) {
    const char *amp = strchr(p, '&');
    const char *seg_end = amp ? amp : p + strlen(p);
    const char *eq = memchr(p, '=', (size_t)(seg_end - p));
    const char *kstart = p;
    size_t kn = eq ? (size_t)(eq - p) : (size_t)(seg_end - p);
    if (kn == klen && memcmp(kstart, key, klen) == 0) {
      const char *vstart = eq ? eq + 1 : seg_end;
      url_decode(vstart, (size_t)(seg_end - vstart), out, cap);
      return 1;
    }
    if (!amp) break;
    p = amp + 1;
  }
  return 0;
}

/* Math.min(hi, Math.max(lo, Number(raw)||def)) — JS verbatim. Note the
 * `|| def` is JS falsy: Number("")===0, 0||def===def; NaN||def===def. */
static int qint(const char *query, const char *key, int def, int lo, int hi);
static double js_number(const char *s);
static int qint(const char *query, const char *key, int def, int lo, int hi) {
  char buf[32];
  double v = def;
  if (qget(query, key, buf, sizeof buf) && buf[0]) {
    double n = js_number(buf);
    v = (n != n || n == 0.0) ? (double)def : n;    /* Number(x) || def */
  }
  if (v < lo) v = lo;
  if (v > hi) v = hi;
  return (int)v;
}

/* bbox parse, JS parseBbox semantics. ret: 0 none, 1 ok, -1 invalid. */
typedef struct { double minLng, minLat, maxLng, maxLat; } bbox_t;
/* JS Number(str): trims whitespace, "" → 0, full-string numeric else NaN. */
static double js_number(const char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' ||
         *s == '\f' || *s == '\v') s++;
  if (!*s) return 0.0;                       /* Number("") === 0 */
  char *end = NULL;
  double d = strtod(s, &end);
  if (end == s) return NAN;
  while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' ||
         *end == '\f' || *end == '\v') end++;
  if (*end) return NAN;                      /* trailing junk → NaN */
  return d;
}

static int parse_bbox(const char *query, bbox_t *b) {
  char raw[256];
  if (!qget(query, "bbox", raw, sizeof raw) || !raw[0]) return 0;  /* !raw → null */
  /* String(raw).split(',').map(Number); length must be exactly 4 and all
   * Number.isFinite. Split strictly on ',' (== JS, empty token → Number("")
   * → 0). */
  double v[8];
  int n = 0;
  char *seg = raw;
  for (;;) {
    char *c = strchr(seg, ',');
    if (c) *c = 0;
    if (n < 8) v[n] = js_number(seg);
    n++;
    if (!c) break;
    seg = c + 1;
  }
  if (n != 4) return -1;
  for (int i = 0; i < 4; i++)
    if (!(v[i] == v[i]) || isinf(v[i])) return -1;   /* !Number.isFinite */
  b->minLng = v[0]; b->minLat = v[1]; b->maxLng = v[2]; b->maxLat = v[3];
  return 1;
}

static int sec_since_midnight_local(void) {
  time_t t = time(NULL);
  struct tm lt; localtime_r(&t, &lt);
  return lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec;
}

static const char *col_text(sqlite3_stmt *s, int i) {
  return (const char *)sqlite3_column_text(s, i);
}

/* add a column as the JS row object would (null/number/string), key `k`. */
static void add_col(cJSON *o, const char *k, sqlite3_stmt *s, int i) {
  switch (sqlite3_column_type(s, i)) {
    case SQLITE_NULL:    cJSON_AddNullToObject(o, k); break;
    case SQLITE_INTEGER: cJSON_AddNumberToObject(o, k,
                            (double)sqlite3_column_int64(s, i)); break;
    case SQLITE_FLOAT:   cJSON_AddNumberToObject(o, k,
                            sqlite3_column_double(s, i)); break;
    default:             cJSON_AddStringToObject(o, k, col_text(s, i)); break;
  }
}

/* JSON.parse(text) → cJSON array, else NULL (safeParseArray / props arrays) */
static cJSON *parse_arr(const char *text) {
  if (!text) return NULL;
  cJSON *v = cJSON_Parse(text);
  if (v && cJSON_IsArray(v)) return v;
  if (v) cJSON_Delete(v);
  return NULL;
}

static const char *arr_str_at(cJSON *arr, int i) {
  if (!arr) return NULL;
  cJSON *e = cJSON_GetArrayItem(arr, i);
  return (e && cJSON_IsString(e)) ? e->valuestring : NULL;
}

/* ===================================================================== */
/* GET /routes                                                           */
/* ===================================================================== */

static int line_intersects_bbox(cJSON *coords, const bbox_t *b) {
  cJSON *pt;
  cJSON_ArrayForEach(pt, coords) {
    cJSON *lng = cJSON_GetArrayItem(pt, 0);
    cJSON *lat = cJSON_GetArrayItem(pt, 1);
    if (!cJSON_IsNumber(lng) || !cJSON_IsNumber(lat)) continue;
    double x = lng->valuedouble, y = lat->valuedouble;
    if (x >= b->minLng && x <= b->maxLng && y >= b->minLat && y <= b->maxLat)
      return 1;
  }
  return 0;
}

static const char *mode_to_source_id(const char *mode) {
  if (!strcmp(mode, "train"))  return "unified-trains";
  if (!strcmp(mode, "subway")) return "unified-subways";
  if (!strcmp(mode, "bus"))    return "unified-buses";
  return NULL;
}

static char *route_routes(db_handle *db, const char *query) {
  char mode[32];
  qget(query, "mode", mode, sizeof mode);
  for (char *p = mode; *p; p++) *p = (char)tolower((unsigned char)*p);
  if (strcmp(mode, "train") && strcmp(mode, "subway") && strcmp(mode, "bus"))
    return err_json("mode must be train|subway|bus");

  bbox_t bb; int bs = parse_bbox(query, &bb);
  if (bs == -1) return err_json("bbox must be minLng,minLat,maxLng,maxLat");

  const char *src = mode_to_source_id(mode);

  /* Collect candidate LineStrings (raw stored geometry — see DEVIATIONS #2:
   * no stitch/smooth pipeline). Equivalent to getLinesByMode() pre-smoothing
   * + the .filter(coords.length>=2 [&& bbox]) the route applies. */
  cJSON **cand = NULL;
  size_t ncap = 0, ncand = 0;
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT uid, geometry, properties FROM intel_items "
        "WHERE source_id=?1 AND geometry IS NOT NULL "
        "AND json_extract(geometry,'$.type')='LineString'", -1, &st, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(st, 1, src, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
      const char *gj = col_text(st, 1);
      const char *pj = col_text(st, 2);
      cJSON *geom = gj ? cJSON_Parse(gj) : NULL;
      cJSON *coords = geom ? cJSON_GetObjectItem(geom, "coordinates") : NULL;
      if (!coords || !cJSON_IsArray(coords) || cJSON_GetArraySize(coords) < 2) {
        if (geom) cJSON_Delete(geom);
        continue;
      }
      if (bs == 1 && !line_intersects_bbox(coords, &bb)) {
        cJSON_Delete(geom);
        continue;
      }
      cJSON *props = pj ? cJSON_Parse(pj) : NULL;
      if (!props || !cJSON_IsObject(props)) {
        if (props) cJSON_Delete(props);
        props = cJSON_CreateObject();
      }
      /* keep just what the feature shape needs */
      cJSON *keep = cJSON_CreateObject();
      cJSON_AddItemToObject(keep, "coordinates",
                            cJSON_Duplicate(coords, 1));
      cJSON *lu = cJSON_GetObjectItem(props, "line_uid");
      cJSON *nm = cJSON_GetObjectItem(props, "name");
      cJSON *lc = cJSON_GetObjectItem(props, "line_color");
      cJSON_AddItemToObject(keep, "route_id",
        (lu && cJSON_IsString(lu)) ? cJSON_CreateString(lu->valuestring)
                                   : cJSON_CreateNull());
      cJSON_AddItemToObject(keep, "name",
        (nm && cJSON_IsString(nm)) ? cJSON_CreateString(nm->valuestring)
                                   : cJSON_CreateNull());
      cJSON_AddItemToObject(keep, "line_color",
        (lc && cJSON_IsString(lc)) ? cJSON_CreateString(lc->valuestring)
                                   : cJSON_CreateNull());
      cJSON_Delete(geom);
      cJSON_Delete(props);
      if (ncand == ncap) {
        ncap = ncap ? ncap * 2 : 256;
        cJSON **nn = realloc(cand, ncap * sizeof *cand);
        if (!nn) { cJSON_Delete(keep); break; }
        cand = nn;
      }
      cand[ncand++] = keep;
    }
  }
  sqlite3_finalize(st);

  /* MAX_FEATURES deterministic stride sampling (verbatim). */
  int truncated = 0;
  size_t total = ncand;
  size_t picked_n = ncand;
  size_t *idx = NULL;
  if (ncand > MAX_FEATURES) {
    truncated = 1;
    picked_n = MAX_FEATURES;
    idx = malloc(picked_n * sizeof *idx);
    double stride = (double)ncand / (double)MAX_FEATURES;
    for (size_t i = 0; i < picked_n; i++)
      idx[i] = (size_t)floor((double)i * stride);
  }

  cJSON *body = cJSON_CreateObject();
  cJSON_AddStringToObject(body, "type", "FeatureCollection");
  cJSON *feats = cJSON_AddArrayToObject(body, "features");
  for (size_t i = 0; i < picked_n; i++) {
    cJSON *kc = cand[idx ? idx[i] : i];
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "LineString");
    cJSON_AddItemToObject(g, "coordinates",
      cJSON_Duplicate(cJSON_GetObjectItem(kc, "coordinates"), 1));
    cJSON_AddItemToObject(f, "geometry", g);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddItemToObject(p, "route_id",
      cJSON_Duplicate(cJSON_GetObjectItem(kc, "route_id"), 1));
    cJSON_AddItemToObject(p, "name",
      cJSON_Duplicate(cJSON_GetObjectItem(kc, "name"), 1));
    cJSON_AddItemToObject(p, "line_color",
      cJSON_Duplicate(cJSON_GetObjectItem(kc, "line_color"), 1));
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(feats, f);
  }
  if (truncated) {
    cJSON *m = cJSON_AddObjectToObject(body, "_meta");
    cJSON_AddBoolToObject(m, "truncated", 1);
    cJSON_AddNumberToObject(m, "limit", MAX_FEATURES);
    cJSON_AddNumberToObject(m, "total", (double)total);
  }

  for (size_t i = 0; i < ncand; i++) cJSON_Delete(cand[i]);
  free(cand);
  free(idx);
  return dup_json(body);
}

/* ===================================================================== */
/* POST /gtfs/hydrate/:orgId  — graceful-empty (no upstream fetch in C)   */
/* ===================================================================== */

static char *route_hydrate(const char *orgId_raw) {
  /* JS: orgId = raw.replace(/[^a-z0-9_-]/gi,''); empty → 400 bad orgId */
  char clean[128];
  size_t o = 0;
  for (size_t i = 0; orgId_raw[i] && o + 1 < sizeof clean; i++) {
    int c = (unsigned char)orgId_raw[i];
    if (isalnum(c) || c == '_' || c == '-') clean[o++] = (char)c;
  }
  clean[o] = 0;
  if (!clean[0]) return err_json("bad orgId");
  /* hydrateOperator() pulls a remote GTFS-JP zip; no network collector in
   * the C build. Same success envelope, zeroed/explicit skip. */
  cJSON *r = cJSON_CreateObject();
  cJSON_AddBoolToObject(r, "ok", 1);
  cJSON_AddStringToObject(r, "orgId", clean);
  cJSON_AddBoolToObject(r, "skipped", 1);
  cJSON_AddStringToObject(r, "reason", "hydrate_not_ported");
  return dup_json(r);
}

/* ===================================================================== */
/* GET /gtfs/stop/:stopId/departures — gtfsStore.getDeparturesAt port     */
/* ===================================================================== */

static const char *DOW_COLS[7] = {"sun","mon","tue","wed","thu","fri","sat"};

static void iso_local_to_z(char *out, size_t n) {
  /* req.query.t default is `new Date()`; .toISOString() is UTC Z. */
  time_t t = time(NULL);
  struct tm g; gmtime_r(&t, &g);
  snprintf(out, n, "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
           g.tm_year+1900, g.tm_mon+1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec);
}

static char *route_stop_departures(db_handle *db, const char *stopId,
                                    const char *query) {
  if (!stopId || !*stopId) return err_json("missing stopId");
  char tbuf[64];
  int has_t = qget(query, "t", tbuf, sizeof tbuf);
  /* We accept only the default-now path precisely (JS new Date()); an
   * explicit ?t= is parsed for validity but service-day math uses local
   * now exactly as JS would for "now". A malformed explicit t → 400. */
  if (has_t && tbuf[0]) {
    /* crude ISO sanity: must contain a digit and 'T' or '-' ; JS isNaN guard */
    int oky = 0; for (const char *p = tbuf; *p; p++) if (isdigit((unsigned char)*p)) { oky = 1; break; }
    if (!oky) return err_json("bad t (ISO date)");
  }
  int limit = qint(query, "limit", 5, 1, 20);

  /* lazyHydrateForStop is a no-op here (DEVIATIONS #3). */

  time_t now = time(NULL);
  struct tm lt; localtime_r(&now, &lt);
  int dow = lt.tm_wday;                       /* 0=Sun, == Date.getDay() */
  char ymd[9];
  snprintf(ymd, sizeof ymd, "%04d%02d%02d",
           lt.tm_year+1900, lt.tm_mon+1, lt.tm_mday);
  int secOfDay = lt.tm_hour*3600 + lt.tm_min*60 + lt.tm_sec;

  cJSON *deps = cJSON_CreateArray();
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT st.org_id, st.feed_id, st.trip_id, st.arrival_sec, st.departure_sec, "
        "st.stop_sequence, t.route_id, t.headsign, t.service_id, "
        "r.short_name AS route_short, r.long_name AS route_long, r.color AS route_color "
        "FROM gtfs_stop_times st "
        "JOIN gtfs_trips t ON t.org_id=st.org_id AND t.feed_id=st.feed_id AND t.trip_id=st.trip_id "
        "LEFT JOIN gtfs_routes r ON r.org_id=t.org_id AND r.feed_id=t.feed_id AND r.route_id=t.route_id "
        "WHERE st.stop_id=?1", -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, stopId, -1, SQLITE_TRANSIENT);
    sqlite3_stmt *cal = NULL;
    sqlite3_prepare_v2(db->h,
      "SELECT sun,mon,tue,wed,thu,fri,sat,start_date,end_date "
      "FROM gtfs_calendar WHERE org_id=?1 AND feed_id=?2 AND service_id=?3",
      -1, &cal, NULL);
    while (sqlite3_step(st) == SQLITE_ROW) {
      const char *org = col_text(st, 0);
      const char *feed = col_text(st, 1);
      const char *trip = col_text(st, 2);
      int arr_null = sqlite3_column_type(st, 3) == SQLITE_NULL;
      int dep_null = sqlite3_column_type(st, 4) == SQLITE_NULL;
      long arr_sec = arr_null ? 0 : sqlite3_column_int64(st, 3);
      long dep_sec = dep_null ? 0 : sqlite3_column_int64(st, 4);
      const char *svc = col_text(st, 8);

      /* isServiceActive */
      int active = 0;
      if (cal) {
        sqlite3_reset(cal);
        sqlite3_bind_text(cal, 1, org, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(cal, 2, feed, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(cal, 3, svc, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(cal) == SQLITE_ROW) {
          int runsToday = sqlite3_column_int(cal, dow) == 1;
          const char *sd = col_text(cal, 7);
          const char *ed = col_text(cal, 8);
          int inRange = (!sd || strcmp(ymd, sd) >= 0)
                     && (!ed || strcmp(ymd, ed) <= 0);
          active = inRange && runsToday;
        }
      }
      if (!active) continue;
      if (dep_null && arr_null) continue;
      long tSec = !dep_null ? dep_sec : arr_sec;   /* departure_sec ?? arrival_sec */
      long wallSec = tSec % 86400;
      long secondsUntil = wallSec - secOfDay;
      if (secondsUntil + 60 < 0) continue;
      if (secondsUntil < 0) secondsUntil += 86400;

      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "trip_id", trip ? trip : "");
      add_col(d, "route_id", st, 6);
      const char *hs = col_text(st, 7);
      if (hs) cJSON_AddStringToObject(d, "headsign", hs);
      else    cJSON_AddNullToObject(d, "headsign");
      const char *rsh = col_text(st, 9);
      const char *rlo = col_text(st, 10);
      const char *rid = col_text(st, 6);
      cJSON_AddStringToObject(d, "route_name",
        rsh ? rsh : (rlo ? rlo : (rid ? rid : "")));
      const char *rc = col_text(st, 11);
      if (rc) { char hx[64]; snprintf(hx, sizeof hx, "#%s", rc);
                cJSON_AddStringToObject(d, "route_color", hx); }
      else cJSON_AddNullToObject(d, "route_color");
      cJSON_AddNumberToObject(d, "departure_sec", (double)tSec);
      cJSON_AddNumberToObject(d, "seconds_until", (double)secondsUntil);
      cJSON_AddItemToArray(deps, d);
    }
    sqlite3_finalize(cal);
  }
  sqlite3_finalize(st);

  /* sort by seconds_until asc, slice(0,limit) */
  int n = cJSON_GetArraySize(deps);
  cJSON **a = malloc((n > 0 ? n : 1) * sizeof *a);
  for (int i = 0; i < n; i++) a[i] = cJSON_GetArrayItem(deps, i);
  for (int i = 1; i < n; i++) {
    cJSON *key = a[i];
    double kv = cJSON_GetObjectItem(key, "seconds_until")->valuedouble;
    int j = i - 1;
    while (j >= 0 &&
           cJSON_GetObjectItem(a[j], "seconds_until")->valuedouble > kv) {
      a[j+1] = a[j]; j--;
    }
    a[j+1] = key;
  }
  cJSON *sorted = cJSON_CreateArray();
  for (int i = 0; i < n && i < limit; i++)
    cJSON_AddItemToArray(sorted, cJSON_Duplicate(a[i], 1));
  free(a);
  cJSON_Delete(deps);

  char nowz[40];
  if (has_t && tbuf[0]) { snprintf(nowz, sizeof nowz, "%s", tbuf); }
  else iso_local_to_z(nowz, sizeof nowz);

  cJSON *body = cJSON_CreateObject();
  cJSON_AddStringToObject(body, "stop_id", stopId);
  cJSON_AddStringToObject(body, "now", nowz);
  cJSON_AddItemToObject(body, "departures", sorted);
  return dup_json(body);
}

/* ===================================================================== */
/* GET /gtfs/operators — listHydratedOperators()                         */
/* ===================================================================== */

static char *route_operators(db_handle *db) {
  cJSON *arr = cJSON_CreateArray();
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT org_id, org_name, hydrated_at, stop_count, trip_count "
        "FROM gtfs_operators ORDER BY hydrated_at DESC", -1, &st, NULL)
      == SQLITE_OK) {
    while (sqlite3_step(st) == SQLITE_ROW) {
      cJSON *o = cJSON_CreateObject();
      add_col(o, "org_id", st, 0);
      add_col(o, "org_name", st, 1);
      add_col(o, "hydrated_at", st, 2);
      add_col(o, "stop_count", st, 3);
      add_col(o, "trip_count", st, 4);
      cJSON_AddItemToArray(arr, o);
    }
  }
  sqlite3_finalize(st);
  cJSON *body = cJSON_CreateObject();
  cJSON_AddItemToObject(body, "operators", arr);
  return dup_json(body);
}

/* ===================================================================== */
/* GET /active-trips — gtfsActiveTrips.getActiveTripsAt port              */
/* ===================================================================== */

typedef struct { double lat, lon, dist; } shp_pt;

static int interp_shape(const shp_pt *sh, int n, double distM,
                        double *olat, double *olon) {
  if (n <= 0) return 0;
  if (n == 1 || distM <= sh[0].dist) { *olat = sh[0].lat; *olon = sh[0].lon; return 1; }
  if (distM >= sh[n-1].dist) { *olat = sh[n-1].lat; *olon = sh[n-1].lon; return 1; }
  for (int i = 1; i < n; i++) {
    if (sh[i].dist >= distM) {
      const shp_pt *a = &sh[i-1], *b = &sh[i];
      double span = b->dist - a->dist;
      if (span <= 0) { *olat = a->lat; *olon = a->lon; return 1; }
      double t = (distM - a->dist) / span;
      *olat = a->lat + t * (b->lat - a->lat);
      *olon = a->lon + t * (b->lon - a->lon);
      return 1;
    }
  }
  *olat = sh[n-1].lat; *olon = sh[n-1].lon; return 1;
}

static char *route_active_trips(db_handle *db, const char *query) {
  char tbuf[64];
  int has_t = qget(query, "t", tbuf, sizeof tbuf);
  if (has_t && tbuf[0]) {
    int oky = 0; for (const char *p = tbuf; *p; p++) if (isdigit((unsigned char)*p)) { oky = 1; break; }
    if (!oky) return err_json("bad t (ISO date)");
  }
  bbox_t bb; int bs = parse_bbox(query, &bb);
  if (bs == -1) return err_json("bbox must be minLng,minLat,maxLng,maxLat");
  int limit = qint(query, "limit", 500, 1, 1000);

  time_t now = time(NULL);
  struct tm lt; localtime_r(&now, &lt);
  const char *dowc = DOW_COLS[lt.tm_wday];
  char today[9];
  snprintf(today, sizeof today, "%04d%02d%02d",
           lt.tm_year+1900, lt.tm_mon+1, lt.tm_mday);
  long nowSec = lt.tm_hour*3600 + lt.tm_min*60 + lt.tm_sec;

  char sql[1024];
  snprintf(sql, sizeof sql,
    "WITH active_services AS ("
    " SELECT org_id,feed_id,service_id FROM gtfs_calendar "
    " WHERE %s=1 AND (start_date IS NULL OR start_date<=?1) "
    "   AND (end_date IS NULL OR end_date>=?1)),"
    "active_trips AS ("
    " SELECT t.org_id,t.feed_id,t.trip_id,t.route_id,t.shape_id,t.headsign "
    " FROM gtfs_trips t JOIN active_services s "
    "  ON s.org_id=t.org_id AND s.feed_id=t.feed_id AND s.service_id=t.service_id),"
    "trip_bounds AS ("
    " SELECT st.org_id,st.feed_id,st.trip_id,"
    "  MIN(st.departure_sec) AS min_dep, MAX(st.arrival_sec) AS max_arr "
    " FROM gtfs_stop_times st JOIN active_trips a USING (org_id,feed_id,trip_id) "
    " GROUP BY st.org_id,st.feed_id,st.trip_id)"
    " SELECT a.org_id,a.feed_id,a.trip_id,a.route_id,a.shape_id,a.headsign,"
    "  tb.min_dep,tb.max_arr FROM active_trips a "
    " JOIN trip_bounds tb USING (org_id,feed_id,trip_id) "
    " WHERE tb.min_dep<=?2 AND tb.max_arr>=?2 LIMIT ?3", dowc);

  cJSON *trips = cJSON_CreateArray();
  sqlite3_stmt *st = NULL, *sStops = NULL, *sShape = NULL, *sRoute = NULL;
  if (sqlite3_prepare_v2(db->h, sql, -1, &st, NULL) == SQLITE_OK) {
    sqlite3_bind_text(st, 1, today, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, nowSec);
    sqlite3_bind_int(st, 3, limit * 4);
    sqlite3_prepare_v2(db->h,
      "SELECT stop_sequence,arrival_sec,departure_sec,shape_dist_traveled "
      "FROM gtfs_stop_times WHERE org_id=?1 AND feed_id=?2 AND trip_id=?3 "
      "ORDER BY stop_sequence ASC", -1, &sStops, NULL);
    sqlite3_prepare_v2(db->h,
      "SELECT lat,lon,dist_m FROM gtfs_shapes "
      "WHERE org_id=?1 AND feed_id=?2 AND shape_id=?3 ORDER BY seq ASC",
      -1, &sShape, NULL);
    sqlite3_prepare_v2(db->h,
      "SELECT short_name,long_name,color FROM gtfs_routes "
      "WHERE org_id=?1 AND feed_id=?2 AND route_id=?3", -1, &sRoute, NULL);

    while (sqlite3_step(st) == SQLITE_ROW) {
      if (cJSON_GetArraySize(trips) >= limit) break;
      const char *org = col_text(st, 0);
      const char *feed = col_text(st, 1);
      const char *trip = col_text(st, 2);
      const char *route_id = col_text(st, 3);
      const char *shape_id = col_text(st, 4);
      const char *headsign = col_text(st, 5);

      /* stops */
      typedef struct { int has_arr, has_dep, has_dist;
                       long arr, dep; double dist; } sr_t;
      sr_t *stops = NULL; int ns = 0, scap = 0;
      sqlite3_reset(sStops);
      sqlite3_bind_text(sStops, 1, org, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(sStops, 2, feed, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(sStops, 3, trip, -1, SQLITE_TRANSIENT);
      while (sqlite3_step(sStops) == SQLITE_ROW) {
        if (ns == scap) { scap = scap ? scap*2 : 16;
                          stops = realloc(stops, scap*sizeof *stops); }
        sr_t *r = &stops[ns++];
        r->has_arr = sqlite3_column_type(sStops,1)!=SQLITE_NULL;
        r->has_dep = sqlite3_column_type(sStops,2)!=SQLITE_NULL;
        r->has_dist= sqlite3_column_type(sStops,3)!=SQLITE_NULL;
        r->arr = r->has_arr ? sqlite3_column_int64(sStops,1) : 0;
        r->dep = r->has_dep ? sqlite3_column_int64(sStops,2) : 0;
        r->dist= r->has_dist? sqlite3_column_double(sStops,3): 0;
      }
      if (ns < 2) { free(stops); continue; }

      int prevIdx = -1;
      for (int i = 0; i < ns-1; i++) {
        int dep_ok = stops[i].has_dep || stops[i].has_arr;
        long dep = stops[i].has_dep ? stops[i].dep : stops[i].arr;
        int na_ok = stops[i+1].has_arr || stops[i+1].has_dep;
        long nextArr = stops[i+1].has_arr ? stops[i+1].arr : stops[i+1].dep;
        if (!dep_ok || !na_ok) continue;
        if (dep <= nowSec && nowSec < nextArr) { prevIdx = i; break; }
      }
      if (prevIdx < 0) { free(stops); continue; }
      sr_t *pv = &stops[prevIdx], *nx = &stops[prevIdx+1];
      long prevT = pv->has_dep ? pv->dep : pv->arr;
      long nextT = nx->has_arr ? nx->arr : nx->dep;
      double span = (nextT - prevT) > 1 ? (double)(nextT - prevT) : 1.0;
      double ratio = (double)(nowSec - prevT) / span;
      if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1;
      if (!pv->has_dist || !nx->has_dist) { free(stops); continue; }
      double distM = pv->dist + ratio * (nx->dist - pv->dist);
      free(stops);

      /* shape */
      shp_pt *sh = NULL; int nsh = 0, shcap = 0;
      sqlite3_reset(sShape);
      sqlite3_bind_text(sShape, 1, org, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(sShape, 2, feed, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(sShape, 3, shape_id ? shape_id : "", -1, SQLITE_TRANSIENT);
      while (sqlite3_step(sShape) == SQLITE_ROW) {
        if (nsh == shcap) { shcap = shcap ? shcap*2 : 64;
                            sh = realloc(sh, shcap*sizeof *sh); }
        sh[nsh].lat = sqlite3_column_double(sShape,0);
        sh[nsh].lon = sqlite3_column_double(sShape,1);
        sh[nsh].dist= sqlite3_column_double(sShape,2);
        nsh++;
      }
      if (nsh < 2) { free(sh); continue; }
      double plat, plon;
      if (!interp_shape(sh, nsh, distM, &plat, &plon)) { free(sh); continue; }
      free(sh);

      if (bs == 1) {
        if (plon < bb.minLng || plon > bb.maxLng ||
            plat < bb.minLat || plat > bb.maxLat) continue;
      }

      const char *rsh = NULL, *rlo = NULL, *rcol = NULL;
      sqlite3_reset(sRoute);
      sqlite3_bind_text(sRoute, 1, org, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(sRoute, 2, feed, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(sRoute, 3, route_id ? route_id : "", -1, SQLITE_TRANSIENT);
      if (sqlite3_step(sRoute) == SQLITE_ROW) {
        rsh = col_text(sRoute, 0); rlo = col_text(sRoute, 1);
        rcol = col_text(sRoute, 2);
        /* dup since next reset frees */
      }
      char tid[512];
      snprintf(tid, sizeof tid, "%s|%s|%s",
               org?org:"", feed?feed:"", trip?trip:"");
      cJSON *t = cJSON_CreateObject();
      cJSON_AddStringToObject(t, "trip_id", tid);
      if (route_id) cJSON_AddStringToObject(t, "route_id", route_id);
      else cJSON_AddNullToObject(t, "route_id");
      cJSON_AddStringToObject(t, "route_name",
        (rsh && *rsh) ? rsh : ((rlo && *rlo) ? rlo : (route_id?route_id:"")));
      if (rcol && *rcol) { char hx[64]; snprintf(hx,sizeof hx,"#%s",rcol);
                           cJSON_AddStringToObject(t,"route_color",hx); }
      else cJSON_AddNullToObject(t, "route_color");
      cJSON_AddStringToObject(t, "operator", org?org:"");
      cJSON_AddNumberToObject(t, "lat", plat);
      cJSON_AddNumberToObject(t, "lon", plon);
      if (headsign) cJSON_AddStringToObject(t, "headsign", headsign);
      else cJSON_AddNullToObject(t, "headsign");
      cJSON_AddItemToArray(trips, t);
    }
  }
  sqlite3_finalize(st);
  sqlite3_finalize(sStops);
  sqlite3_finalize(sShape);
  sqlite3_finalize(sRoute);

  char nowz[40];
  if (has_t && tbuf[0]) snprintf(nowz, sizeof nowz, "%s", tbuf);
  else iso_local_to_z(nowz, sizeof nowz);
  cJSON *body = cJSON_CreateObject();
  cJSON_AddStringToObject(body, "now", nowz);
  cJSON_AddNumberToObject(body, "count", cJSON_GetArraySize(trips));
  cJSON_AddItemToObject(body, "trips", trips);
  return dup_json(body);
}

/* ===================================================================== */
/* Shared GTFS departures/arrivals/alerts/odpt for the summary routes     */
/* ===================================================================== */

/* departures (dir=1) or arrivals (dir=0) for a stop_id at >= nowSec.
 * Appends rows to `arr` with the JS spread {...d, wall_sec, [member_mode]}. */
static void query_stop_times(db_handle *db, const char *stopId, int nowSec,
                             int departures, cJSON *arr,
                             const char *member_mode /* NULL = single */,
                             cJSON *routeIdSet /* string-array accumulator */) {
  const char *sql = departures ?
    "SELECT st.trip_id, st.stop_sequence, st.departure_sec, "
    " tr.headsign, tr.route_id, r.short_name AS route_short, "
    " r.long_name AS route_long, r.color AS route_color, "
    " rt.departure_delay_s AS delay_s "
    "FROM gtfs_stop_times st "
    "JOIN gtfs_trips tr ON tr.org_id=st.org_id AND tr.feed_id=st.feed_id AND tr.trip_id=st.trip_id "
    "LEFT JOIN gtfs_routes r ON r.org_id=tr.org_id AND r.feed_id=tr.feed_id AND r.route_id=tr.route_id "
    "LEFT JOIN gtfs_rt_trip_updates rt ON rt.org_id=st.org_id AND rt.trip_id=st.trip_id AND rt.stop_sequence=st.stop_sequence "
    "WHERE st.stop_id=?1 AND st.departure_sec>=?2 "
    "ORDER BY st.departure_sec ASC LIMIT 10"
   :
    "SELECT st.trip_id, st.stop_sequence, st.arrival_sec, "
    " tr.headsign, tr.route_id, r.short_name AS route_short, "
    " r.long_name AS route_long, r.color AS route_color, "
    " rt.arrival_delay_s AS delay_s "
    "FROM gtfs_stop_times st "
    "JOIN gtfs_trips tr ON tr.org_id=st.org_id AND tr.feed_id=st.feed_id AND tr.trip_id=st.trip_id "
    "LEFT JOIN gtfs_routes r ON r.org_id=tr.org_id AND r.feed_id=tr.feed_id AND r.route_id=tr.route_id "
    "LEFT JOIN gtfs_rt_trip_updates rt ON rt.org_id=st.org_id AND rt.trip_id=st.trip_id AND rt.stop_sequence=st.stop_sequence "
    "WHERE st.stop_id=?1 AND st.arrival_sec>=?2 "
    "ORDER BY st.arrival_sec ASC LIMIT 10";

  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, stopId, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 2, nowSec);
  while (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *d = cJSON_CreateObject();
    add_col(d, "trip_id", s, 0);
    add_col(d, "stop_sequence", s, 1);
    add_col(d, departures ? "departure_sec" : "arrival_sec", s, 2);
    add_col(d, "headsign", s, 3);
    add_col(d, "route_id", s, 4);
    add_col(d, "route_short", s, 5);
    add_col(d, "route_long", s, 6);
    add_col(d, "route_color", s, 7);
    add_col(d, "delay_s", s, 8);
    /* wall_sec = departure_sec / arrival_sec */
    if (sqlite3_column_type(s, 2) == SQLITE_NULL)
      cJSON_AddNullToObject(d, "wall_sec");
    else
      cJSON_AddNumberToObject(d, "wall_sec",
                              (double)sqlite3_column_int64(s, 2));
    if (member_mode) cJSON_AddStringToObject(d, "member_mode", member_mode);
    if (routeIdSet && sqlite3_column_type(s, 4) != SQLITE_NULL) {
      const char *rid = col_text(s, 4);
      int seen = 0, m = cJSON_GetArraySize(routeIdSet);
      for (int i = 0; i < m; i++) {
        cJSON *e = cJSON_GetArrayItem(routeIdSet, i);
        if (e && cJSON_IsString(e) && rid && !strcmp(e->valuestring, rid)) { seen = 1; break; }
      }
      if (!seen && rid) cJSON_AddItemToArray(routeIdSet, cJSON_CreateString(rid));
    }
    cJSON_AddItemToArray(arr, d);
  }
  sqlite3_finalize(s);
}

/* ODPT timetable rows for station >= afterHHMM, limit. dir mirrors
 * getStationTimetable (calendar=null here; afterHHMM set). */
static void query_odpt(db_handle *db, const char *stationId,
                       const char *afterHHMM, int limit,
                       cJSON *out, const char *member_mode) {
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT station_id, line_id, calendar, direction, seq, "
        "departure_time, destination_ja, destination_en, "
        "train_type, train_name, is_last, is_origin, org_id "
        "FROM odpt_station_timetable WHERE station_id=?1 "
        "ORDER BY departure_time ASC, seq ASC", -1, &s, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_text(s, 1, stationId, -1, SQLITE_TRANSIENT);
  int kept = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    if (kept >= limit) break;
    const char *dt = col_text(s, 5);
    if (afterHHMM && afterHHMM[0] && dt && strcmp(dt, afterHHMM) < 0) continue;
    cJSON *r = cJSON_CreateObject();
    add_col(r, "station_id", s, 0);
    add_col(r, "line_id", s, 1);
    add_col(r, "calendar", s, 2);
    add_col(r, "direction", s, 3);
    add_col(r, "seq", s, 4);
    add_col(r, "departure_time", s, 5);
    add_col(r, "destination_ja", s, 6);
    add_col(r, "destination_en", s, 7);
    add_col(r, "train_type", s, 8);
    add_col(r, "train_name", s, 9);
    add_col(r, "is_last", s, 10);
    add_col(r, "is_origin", s, 11);
    add_col(r, "org_id", s, 12);
    if (member_mode) cJSON_AddStringToObject(r, "member_mode", member_mode);
    cJSON_AddItemToArray(out, r);
    kept++;
  }
  sqlite3_finalize(s);
}

static int is_cluster_uid(const char *u) {
  if (!u) return 0;
  int n = 0;
  for (const char *p = u; *p; p++, n++)
    if (!isxdigit((unsigned char)*p) || isupper((unsigned char)*p)) return 0;
  return n == 40;
}

/* alerts: for each route id, gtfs_rt_alerts WHERE route_ids LIKE %"rid"%,
 * up to 10 total. `dedupe` => skip dup header|description (cluster path). */
static void collect_alerts(db_handle *db, cJSON *routeIds, int dedupe,
                           cJSON *alerts) {
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT header_text, description_text, effect FROM gtfs_rt_alerts "
        "WHERE route_ids LIKE ?1 LIMIT 10", -1, &s, NULL) != SQLITE_OK)
    return;
  cJSON *seen = dedupe ? cJSON_CreateArray() : NULL;
  int m = cJSON_GetArraySize(routeIds);
  for (int i = 0; i < m; i++) {
    if (cJSON_GetArraySize(alerts) >= 10) break;
    cJSON *re = cJSON_GetArrayItem(routeIds, i);
    if (!re || !cJSON_IsString(re)) continue;
    char needle[256];
    snprintf(needle, sizeof needle, "%%\"%s\"%%", re->valuestring);
    sqlite3_reset(s);
    sqlite3_bind_text(s, 1, needle, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(s) == SQLITE_ROW) {
      const char *h = col_text(s, 0);
      const char *d = col_text(s, 1);
      if (dedupe) {
        char key[1024];
        snprintf(key, sizeof key, "%s|%s", h?h:"", d?d:"");
        int dup = 0, sn = cJSON_GetArraySize(seen);
        for (int k = 0; k < sn; k++) {
          cJSON *e = cJSON_GetArrayItem(seen, k);
          if (e && cJSON_IsString(e) && !strcmp(e->valuestring, key)) { dup = 1; break; }
        }
        if (dup) continue;
        cJSON_AddItemToArray(seen, cJSON_CreateString(key));
      }
      cJSON *a = cJSON_CreateObject();
      add_col(a, "header_text", s, 0);
      add_col(a, "description_text", s, 1);
      add_col(a, "effect", s, 2);
      cJSON_AddItemToArray(alerts, a);
      if (cJSON_GetArraySize(alerts) >= 10) break;
    }
    if (cJSON_GetArraySize(alerts) >= 10) break;
  }
  if (seen) cJSON_Delete(seen);
  sqlite3_finalize(s);
}

static void sort_by_wall_sec(cJSON *arr) {
  int n = cJSON_GetArraySize(arr);
  if (n < 2) return;
  cJSON **a = malloc(n * sizeof *a);
  for (int i = 0; i < n; i++) a[i] = cJSON_DetachItemFromArray(arr, 0);
  for (int i = 1; i < n; i++) {
    cJSON *key = a[i];
    cJSON *kw = cJSON_GetObjectItem(key, "wall_sec");
    double kv = (kw && cJSON_IsNumber(kw)) ? kw->valuedouble : 0;
    int j = i - 1;
    while (j >= 0) {
      cJSON *jw = cJSON_GetObjectItem(a[j], "wall_sec");
      double jv = (jw && cJSON_IsNumber(jw)) ? jw->valuedouble : 0;
      if (jv <= kv) break;
      a[j+1] = a[j]; j--;
    }
    a[j+1] = key;
  }
  for (int i = 0; i < n; i++) cJSON_AddItemToArray(arr, a[i]);
  free(a);
}

/* ---- cluster summary (40-hex uid) ----------------------------------- */
static char *cluster_summary(db_handle *db, const char *clusterUid) {
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT cluster_uid,name,name_ja,lat,lon,member_uids,"
        "line_colors,line_names,line_refs,line_modes,mode_set,operator_set "
        "FROM station_clusters WHERE cluster_uid=?1", -1, &s, NULL)
      != SQLITE_OK) return err_json("internal");
  sqlite3_bind_text(s, 1, clusterUid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) {
    sqlite3_finalize(s);
    return err_json("cluster not found");
  }
  char *c_uid   = strdup(col_text(s,0) ? col_text(s,0) : "");
  char *c_name  = col_text(s,1) ? strdup(col_text(s,1)) : NULL;
  char *c_nmja  = col_text(s,2) ? strdup(col_text(s,2)) : NULL;
  double c_lat  = sqlite3_column_double(s,3);
  double c_lon  = sqlite3_column_double(s,4);
  const char *member_uids_j = col_text(s,5);
  const char *line_colors_j = col_text(s,6);
  const char *line_names_j  = col_text(s,7);
  const char *line_refs_j   = col_text(s,8);
  const char *line_modes_j  = col_text(s,9);
  const char *mode_set_j    = col_text(s,10);
  const char *operator_set_j= col_text(s,11);
  cJSON *memberUids = parse_arr(member_uids_j);
  cJSON *lineColors = parse_arr(line_colors_j);
  cJSON *lineNames  = parse_arr(line_names_j);
  cJSON *lineRefs   = parse_arr(line_refs_j);
  cJSON *lineModes  = parse_arr(line_modes_j);
  cJSON *modeSet    = parse_arr(mode_set_j);
  cJSON *operSet    = parse_arr(operator_set_j);
  sqlite3_finalize(s);

  /* members: per member_uid, intel_items suffix match */
  typedef struct { char *station_uid, *mode, *name, *operator_, *props;
                   double lat, lon; } mem_t;
  mem_t *mem = NULL; int nmem = 0;
  int mu_n = memberUids ? cJSON_GetArraySize(memberUids) : 0;
  if (mu_n) {
    mem = calloc(mu_n, sizeof *mem);
    sqlite3_stmt *q = NULL;
    sqlite3_prepare_v2(db->h,
      "SELECT substr(uid, instr(uid,'|')+1) AS station_uid, "
      "COALESCE(sub_source_id,'') AS mode, title AS name, "
      "json_extract(properties,'$.operator') AS operator, lat, lon, properties "
      "FROM intel_items WHERE record_type='station' AND uid LIKE '%|' || ?1 "
      "LIMIT 1", -1, &q, NULL);
    for (int i = 0; i < mu_n; i++) {
      cJSON *mui = cJSON_GetArrayItem(memberUids, i);
      if (!mui || !cJSON_IsString(mui)) continue;
      sqlite3_reset(q);
      sqlite3_bind_text(q, 1, mui->valuestring, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(q) == SQLITE_ROW) {
        mem_t *m = &mem[nmem++];
        m->station_uid = col_text(q,0) ? strdup(col_text(q,0)) : strdup("");
        m->mode        = col_text(q,1) ? strdup(col_text(q,1)) : strdup("");
        m->name        = col_text(q,2) ? strdup(col_text(q,2)) : NULL;
        m->operator_   = col_text(q,3) ? strdup(col_text(q,3)) : NULL;
        m->lat         = sqlite3_column_double(q,4);
        m->lon         = sqlite3_column_double(q,5);
        m->props       = col_text(q,6) ? strdup(col_text(q,6)) : NULL;
      }
    }
    sqlite3_finalize(q);
  }

  /* lazyHydrateForStop per member: no-op (DEVIATIONS #3). */

  int nowSec = sec_since_midnight_local();
  time_t now = time(NULL); struct tm lt; localtime_r(&now, &lt);
  char hhmm[6]; snprintf(hhmm, sizeof hhmm, "%02d:%02d", lt.tm_hour, lt.tm_min);

  cJSON *allDep = cJSON_CreateArray();
  cJSON *allArr = cJSON_CreateArray();
  cJSON *odptDep = cJSON_CreateArray();
  cJSON *routeIds = cJSON_CreateArray();

  for (int i = 0; i < nmem; i++) {
    cJSON *p = mem[i].props ? cJSON_Parse(mem[i].props) : NULL;
    cJSON *sid = (p && cJSON_IsObject(p)) ? cJSON_GetObjectItem(p, "stop_id") : NULL;
    const char *stopId = (sid && cJSON_IsString(sid)) ? sid->valuestring
                                                      : mem[i].station_uid;
    query_stop_times(db, stopId, nowSec, 1, allDep, mem[i].mode, routeIds);
    query_stop_times(db, stopId, nowSec, 0, allArr, mem[i].mode, routeIds);

    cJSON *stid = (p && cJSON_IsObject(p)) ? cJSON_GetObjectItem(p, "station_id") : NULL;
    if (stid && cJSON_IsString(stid) &&
        strncmp(stid->valuestring, "odpt.Station:", 13) == 0) {
      query_odpt(db, stid->valuestring, hhmm, 10, odptDep, mem[i].mode);
    }
    if (p) cJSON_Delete(p);
  }

  sort_by_wall_sec(allDep);
  sort_by_wall_sec(allArr);

  cJSON *alerts = cJSON_CreateArray();
  collect_alerts(db, routeIds, 1 /*dedupe*/, alerts);

  /* lines from cluster row arrays */
  cJSON *lines = cJSON_CreateArray();
  int lc_n = lineColors ? cJSON_GetArraySize(lineColors) : 0;
  for (int i = 0; i < lc_n; i++) {
    cJSON *ln = cJSON_CreateObject();
    const char *col = arr_str_at(lineColors, i);
    const char *nm  = arr_str_at(lineNames, i);
    const char *rf  = arr_str_at(lineRefs, i);
    const char *md  = arr_str_at(lineModes, i);
    if (col && *col) cJSON_AddStringToObject(ln,"color",col); else cJSON_AddNullToObject(ln,"color");
    if (nm  && *nm ) cJSON_AddStringToObject(ln,"name", nm ); else cJSON_AddNullToObject(ln,"name");
    if (rf  && *rf ) cJSON_AddStringToObject(ln,"ref",  rf ); else cJSON_AddNullToObject(ln,"ref");
    if (md  && *md ) cJSON_AddStringToObject(ln,"mode", md ); else cJSON_AddNullToObject(ln,"mode");
    cJSON_AddItemToArray(lines, ln);
  }

  /* mode_set.join('/'), operator_set.join(', ') || null */
  char modej[256] = {0};
  int ms_n = modeSet ? cJSON_GetArraySize(modeSet) : 0;
  for (int i = 0; i < ms_n; i++) {
    const char *v = arr_str_at(modeSet, i); if (!v) v = "";
    if (i) strncat(modej, "/", sizeof modej - strlen(modej) - 1);
    strncat(modej, v, sizeof modej - strlen(modej) - 1);
  }
  char operj[512] = {0};
  int os_n = operSet ? cJSON_GetArraySize(operSet) : 0;
  for (int i = 0; i < os_n; i++) {
    const char *v = arr_str_at(operSet, i); if (!v) v = "";
    if (i) strncat(operj, ", ", sizeof operj - strlen(operj) - 1);
    strncat(operj, v, sizeof operj - strlen(operj) - 1);
  }

  cJSON *body = cJSON_CreateObject();
  cJSON *stn = cJSON_AddObjectToObject(body, "station");
  cJSON_AddStringToObject(stn, "station_uid", c_uid);
  cJSON_AddStringToObject(stn, "cluster_uid", c_uid);
  cJSON_AddStringToObject(stn, "mode", modej);
  if (c_name) cJSON_AddStringToObject(stn, "name", c_name);
  else cJSON_AddNullToObject(stn, "name");
  if (c_nmja) cJSON_AddStringToObject(stn, "name_ja", c_nmja);
  else cJSON_AddNullToObject(stn, "name_ja");
  if (os_n > 0) cJSON_AddStringToObject(stn, "operator", operj);
  else cJSON_AddNullToObject(stn, "operator");
  cJSON_AddNumberToObject(stn, "lat", c_lat);
  cJSON_AddNumberToObject(stn, "lon", c_lon);
  cJSON_AddNumberToObject(stn, "member_count", nmem);
  cJSON_AddItemToObject(body, "lines", lines);

  /* slice(0,20) */
  cJSON *dep20 = cJSON_CreateArray(), *arr20 = cJSON_CreateArray(),
        *odpt20 = cJSON_CreateArray();
  for (int i = 0; i < cJSON_GetArraySize(allDep) && i < 20; i++)
    cJSON_AddItemToArray(dep20, cJSON_Duplicate(cJSON_GetArrayItem(allDep,i),1));
  for (int i = 0; i < cJSON_GetArraySize(allArr) && i < 20; i++)
    cJSON_AddItemToArray(arr20, cJSON_Duplicate(cJSON_GetArrayItem(allArr,i),1));
  for (int i = 0; i < cJSON_GetArraySize(odptDep) && i < 20; i++)
    cJSON_AddItemToArray(odpt20, cJSON_Duplicate(cJSON_GetArrayItem(odptDep,i),1));
  cJSON_Delete(allDep); cJSON_Delete(allArr); cJSON_Delete(odptDep);
  cJSON_AddItemToObject(body, "departures", dep20);
  cJSON_AddItemToObject(body, "arrivals", arr20);
  cJSON_AddItemToObject(body, "odpt_departures", odpt20);
  cJSON_AddItemToObject(body, "alerts", alerts);

  for (int i = 0; i < nmem; i++) {
    free(mem[i].station_uid); free(mem[i].mode); free(mem[i].name);
    free(mem[i].operator_); free(mem[i].props);
  }
  free(mem);
  free(c_uid); free(c_name); free(c_nmja);
  if (memberUids) cJSON_Delete(memberUids);
  if (lineColors) cJSON_Delete(lineColors);
  if (lineNames)  cJSON_Delete(lineNames);
  if (lineRefs)   cJSON_Delete(lineRefs);
  if (lineModes)  cJSON_Delete(lineModes);
  if (modeSet)    cJSON_Delete(modeSet);
  if (operSet)    cJSON_Delete(operSet);
  cJSON_Delete(routeIds);
  return dup_json(body);
}

/* ---- single-station summary (legacy uid) ---------------------------- */
static char *station_summary(db_handle *db, const char *stationUid) {
  if (!stationUid || !*stationUid) return err_json("missing stationUid");
  if (is_cluster_uid(stationUid)) return cluster_summary(db, stationUid);

  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT substr(uid, instr(uid,'|')+1) AS station_uid, "
        "COALESCE(sub_source_id,'') AS mode, title AS name, "
        "json_extract(properties,'$.operator') AS operator, "
        "json_extract(properties,'$.line') AS line, lat, lon, properties "
        "FROM intel_items WHERE record_type='station' "
        "AND uid LIKE '%|' || ?1 LIMIT 1", -1, &s, NULL) != SQLITE_OK)
    return err_json("internal");
  sqlite3_bind_text(s, 1, stationUid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) {
    sqlite3_finalize(s);
    return err_json("station not found");
  }
  char *st_uid = col_text(s,0) ? strdup(col_text(s,0)) : strdup("");
  char *st_mode= col_text(s,1) ? strdup(col_text(s,1)) : strdup("");
  char *st_name= col_text(s,2) ? strdup(col_text(s,2)) : NULL;
  char *st_oper= col_text(s,3) ? strdup(col_text(s,3)) : NULL;
  char *st_line= col_text(s,4) ? strdup(col_text(s,4)) : NULL;
  double st_lat = sqlite3_column_double(s,5);
  double st_lon = sqlite3_column_double(s,6);
  char *propsj  = col_text(s,7) ? strdup(col_text(s,7)) : NULL;
  sqlite3_finalize(s);

  cJSON *props = propsj ? cJSON_Parse(propsj) : NULL;
  if (props && !cJSON_IsObject(props)) { cJSON_Delete(props); props = NULL; }

  cJSON *p_stopId = props ? cJSON_GetObjectItem(props, "stop_id") : NULL;
  cJSON *p_stationId = props ? cJSON_GetObjectItem(props, "station_id") : NULL;
  const char *odptStationId = NULL;
  if (p_stationId && cJSON_IsString(p_stationId) &&
      strncmp(p_stationId->valuestring, "odpt.Station:", 13) == 0)
    odptStationId = p_stationId->valuestring;
  /* lazyHydrateForStop: no-op (DEVIATIONS #3). */

  /* lines: line_colors[]/line_names[]/line_refs[] else single-line cols */
  cJSON *lines = cJSON_CreateArray();
  cJSON *lcArr = props ? cJSON_GetObjectItem(props, "line_colors") : NULL;
  cJSON *lnArr = props ? cJSON_GetObjectItem(props, "line_names")  : NULL;
  cJSON *lrArr = props ? cJSON_GetObjectItem(props, "line_refs")   : NULL;
  int lc_n = (lcArr && cJSON_IsArray(lcArr)) ? cJSON_GetArraySize(lcArr) : 0;
  for (int i = 0; i < lc_n; i++) {
    cJSON *ln = cJSON_CreateObject();
    const char *col = arr_str_at(lcArr, i);
    const char *nm  = (lnArr && cJSON_IsArray(lnArr)) ? arr_str_at(lnArr,i) : NULL;
    const char *rf  = (lrArr && cJSON_IsArray(lrArr)) ? arr_str_at(lrArr,i) : NULL;
    if (col && *col) cJSON_AddStringToObject(ln,"color",col); else cJSON_AddNullToObject(ln,"color");
    if (nm  && *nm ) cJSON_AddStringToObject(ln,"name", nm ); else cJSON_AddNullToObject(ln,"name");
    if (rf  && *rf ) cJSON_AddStringToObject(ln,"ref",  rf ); else cJSON_AddNullToObject(ln,"ref");
    cJSON_AddItemToArray(lines, ln);
  }
  if (cJSON_GetArraySize(lines) == 0 && st_line) {
    cJSON *ln = cJSON_CreateObject();
    cJSON *lcol = props ? cJSON_GetObjectItem(props, "line_color") : NULL;
    cJSON *lref = props ? cJSON_GetObjectItem(props, "line_ref")   : NULL;
    if (lcol && cJSON_IsString(lcol)) cJSON_AddStringToObject(ln,"color",lcol->valuestring);
    else cJSON_AddNullToObject(ln,"color");
    cJSON_AddStringToObject(ln, "name", st_line);
    if (lref && cJSON_IsString(lref)) cJSON_AddStringToObject(ln,"ref",lref->valuestring);
    else cJSON_AddNullToObject(ln,"ref");
    cJSON_AddItemToArray(lines, ln);
  }

  const char *stopId = (p_stopId && cJSON_IsString(p_stopId))
                      ? p_stopId->valuestring : st_uid;
  int nowSec = sec_since_midnight_local();

  cJSON *departures = cJSON_CreateArray();
  cJSON *arrivals = cJSON_CreateArray();
  cJSON *routeIds = cJSON_CreateArray();
  query_stop_times(db, stopId, nowSec, 1, departures, NULL, routeIds);
  query_stop_times(db, stopId, nowSec, 0, arrivals,   NULL, routeIds);

  cJSON *alerts = cJSON_CreateArray();
  collect_alerts(db, routeIds, 0 /*no dedupe*/, alerts);

  cJSON *odptDep = cJSON_CreateArray();
  if (odptStationId) {
    time_t now = time(NULL); struct tm lt; localtime_r(&now, &lt);
    char hhmm[6]; snprintf(hhmm, sizeof hhmm, "%02d:%02d", lt.tm_hour, lt.tm_min);
    query_odpt(db, odptStationId, hhmm, 10, odptDep, NULL);
  }

  cJSON *body = cJSON_CreateObject();
  cJSON *stn = cJSON_AddObjectToObject(body, "station");
  cJSON_AddStringToObject(stn, "station_uid", st_uid);
  cJSON_AddStringToObject(stn, "mode", st_mode);
  if (st_name) cJSON_AddStringToObject(stn, "name", st_name);
  else cJSON_AddNullToObject(stn, "name");
  if (st_oper) cJSON_AddStringToObject(stn, "operator", st_oper);
  else cJSON_AddNullToObject(stn, "operator");
  cJSON_AddNumberToObject(stn, "lat", st_lat);
  cJSON_AddNumberToObject(stn, "lon", st_lon);
  cJSON_AddItemToObject(body, "lines", lines);
  cJSON_AddItemToObject(body, "departures", departures);
  cJSON_AddItemToObject(body, "arrivals", arrivals);
  cJSON_AddItemToObject(body, "odpt_departures", odptDep);
  if (odptStationId) cJSON_AddStringToObject(body, "odpt_station_id", odptStationId);
  else cJSON_AddNullToObject(body, "odpt_station_id");
  /* alerts.slice(0,10) — collect_alerts already caps at 10 */
  cJSON_AddItemToObject(body, "alerts", alerts);

  cJSON_Delete(routeIds);
  if (props) cJSON_Delete(props);
  free(propsj); free(st_uid); free(st_mode); free(st_name);
  free(st_oper); free(st_line);
  return dup_json(body);
}

/* ===================================================================== */
/* GET /vehicle/:tripId/info                                             */
/* ===================================================================== */

static char *vehicle_info(db_handle *db, const char *tripIdRaw) {
  /* raw is already URL-decoded by the dispatcher; split org|feed|trip */
  char buf[768];
  snprintf(buf, sizeof buf, "%s", tripIdRaw ? tripIdRaw : "");
  char *org = buf;
  char *p1 = strchr(buf, '|');
  if (!p1) return err_json("bad tripId (expected org|feed|trip)");
  *p1 = 0;
  char *feed = p1 + 1;
  char *p2 = strchr(feed, '|');
  if (!p2) return err_json("bad tripId (expected org|feed|trip)");
  *p2 = 0;
  char *trip = p2 + 1;
  if (!*org || !*feed || !*trip)
    return err_json("bad tripId (expected org|feed|trip)");

  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT t.trip_id, t.route_id, t.service_id, t.headsign, "
        "r.short_name AS route_short, r.long_name AS route_long, r.color AS route_color "
        "FROM gtfs_trips t LEFT JOIN gtfs_routes r "
        "ON r.org_id=t.org_id AND r.feed_id=t.feed_id AND r.route_id=t.route_id "
        "WHERE t.org_id=?1 AND t.feed_id=?2 AND t.trip_id=?3",
        -1, &s, NULL) != SQLITE_OK) return err_json("internal");
  sqlite3_bind_text(s, 1, org, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, feed, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, trip, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) {
    sqlite3_finalize(s);
    return err_json("trip not found");
  }
  const char *route_id = col_text(s, 1);
  const char *headsign = col_text(s, 3);
  const char *rsh = col_text(s, 4);
  const char *rlo = col_text(s, 5);
  const char *rcol = col_text(s, 6);
  char *route_id_d = route_id ? strdup(route_id) : NULL;
  char *headsign_d = headsign ? strdup(headsign) : NULL;
  char *rsh_d = rsh ? strdup(rsh) : NULL;
  char *rlo_d = rlo ? strdup(rlo) : NULL;
  char *rcol_d = rcol ? strdup(rcol) : NULL;
  sqlite3_finalize(s);

  int nowSec = sec_since_midnight_local();
  cJSON *nextStop = NULL;
  sqlite3_stmt *ns = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT st.stop_sequence, st.stop_id, st.arrival_sec, st.departure_sec, "
        "rt.arrival_delay_s AS delay_s "
        "FROM gtfs_stop_times st LEFT JOIN gtfs_rt_trip_updates rt "
        "ON rt.org_id=st.org_id AND rt.trip_id=st.trip_id AND rt.stop_sequence=st.stop_sequence "
        "WHERE st.org_id=?1 AND st.feed_id=?2 AND st.trip_id=?3 "
        "AND st.arrival_sec>=?4 ORDER BY st.arrival_sec ASC LIMIT 1",
        -1, &ns, NULL) == SQLITE_OK) {
    sqlite3_bind_text(ns, 1, org, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ns, 2, feed, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ns, 3, trip, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(ns, 4, nowSec);
    if (sqlite3_step(ns) == SQLITE_ROW) {
      nextStop = cJSON_CreateObject();
      add_col(nextStop, "stop_sequence", ns, 0);
      add_col(nextStop, "stop_id", ns, 1);
      add_col(nextStop, "arrival_sec", ns, 2);
      add_col(nextStop, "departure_sec", ns, 3);
      add_col(nextStop, "delay_s", ns, 4);
    }
  }
  sqlite3_finalize(ns);

  cJSON *body = cJSON_CreateObject();
  cJSON *t = cJSON_AddObjectToObject(body, "trip");
  cJSON_AddStringToObject(t, "trip_id", trip);
  cJSON_AddStringToObject(t, "org_id", org);
  if (route_id_d) cJSON_AddStringToObject(t, "route_id", route_id_d);
  else cJSON_AddNullToObject(t, "route_id");
  if (headsign_d) cJSON_AddStringToObject(t, "headsign", headsign_d);
  else cJSON_AddNullToObject(t, "headsign");
  if (rsh_d) cJSON_AddStringToObject(t, "route_short", rsh_d);
  else cJSON_AddNullToObject(t, "route_short");
  if (rlo_d) cJSON_AddStringToObject(t, "route_long", rlo_d);
  else cJSON_AddNullToObject(t, "route_long");
  if (rcol_d && *rcol_d) { char hx[64]; snprintf(hx,sizeof hx,"#%s",rcol_d);
                           cJSON_AddStringToObject(t,"route_color",hx); }
  else cJSON_AddNullToObject(t, "route_color");
  cJSON_AddItemToObject(body, "next_stop", nextStop ? nextStop : cJSON_CreateNull());

  free(route_id_d); free(headsign_d); free(rsh_d); free(rlo_d); free(rcol_d);
  return dup_json(body);
}

/* ===================================================================== */
/* GET /station-boundaries — graceful-empty (Overpass collector absent)   */
/* ===================================================================== */

static char *route_station_boundaries(const char *query) {
  bbox_t bb; int bs = parse_bbox(query, &bb);
  if (bs == -1) return err_json("bbox must be minLng,minLat,maxLng,maxLat");
  /* osmTransportStationBoundaries collector (Overpass) not ported → same
   * shape as an Overpass miss / empty cache. */
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "type", "FeatureCollection");
  cJSON_AddItemToObject(o, "features", cJSON_CreateArray());
  return dup_json(o);
}

/* ===================================================================== */
/* Dispatcher                                                            */
/* ===================================================================== */

/* match "/seg" exactly; return tail pointer if path == prefix, else NULL */
static const char *path_eq(const char *path, const char *want) {
  return strcmp(path, want) == 0 ? path + strlen(path) : NULL;
}

char *transitapi(db_handle *db, const char *path, const char *query) {
  if (!db || !path) return NULL;
  if (!*path) path = "/";

  /* GET /routes */
  if (path_eq(path, "/routes")) return route_routes(db, query);

  /* GET /gtfs/operators */
  if (path_eq(path, "/gtfs/operators")) return route_operators(db);

  /* GET /active-trips */
  if (path_eq(path, "/active-trips")) return route_active_trips(db, query);

  /* GET /station-boundaries */
  if (path_eq(path, "/station-boundaries"))
    return route_station_boundaries(query);

  /* POST /gtfs/hydrate/:orgId */
  if (strncmp(path, "/gtfs/hydrate/", 14) == 0) {
    const char *org = path + 14;
    if (!*org || strchr(org, '/')) return NULL;   /* shape mismatch → 404 */
    return route_hydrate(org);
  }

  /* GET /gtfs/stop/:stopId/departures */
  if (strncmp(path, "/gtfs/stop/", 11) == 0) {
    const char *rest = path + 11;
    const char *tail = strstr(rest, "/departures");
    if (!tail || tail[11] != 0 || tail == rest) return NULL;
    char stopId[512];
    size_t n = (size_t)(tail - rest);
    if (n >= sizeof stopId) n = sizeof stopId - 1;
    char enc[512];
    if (n >= sizeof enc) n = sizeof enc - 1;
    memcpy(enc, rest, n); enc[n] = 0;
    url_decode(enc, n, stopId, sizeof stopId);
    return route_stop_departures(db, stopId, query);
  }

  /* GET /station/:stationUid/summary */
  if (strncmp(path, "/station/", 9) == 0) {
    const char *rest = path + 9;
    const char *tail = strstr(rest, "/summary");
    if (!tail || tail[8] != 0 || tail == rest) return NULL;
    char enc[256], uid[256];
    size_t n = (size_t)(tail - rest);
    if (n >= sizeof enc) n = sizeof enc - 1;
    memcpy(enc, rest, n); enc[n] = 0;
    url_decode(enc, n, uid, sizeof uid);
    return station_summary(db, uid);
  }

  /* GET /vehicle/:tripId/info */
  if (strncmp(path, "/vehicle/", 9) == 0) {
    const char *rest = path + 9;
    const char *tail = strstr(rest, "/info");
    if (!tail || tail[5] != 0 || tail == rest) return NULL;
    char enc[768], tid[768];
    size_t n = (size_t)(tail - rest);
    if (n >= sizeof enc) n = sizeof enc - 1;
    memcpy(enc, rest, n); enc[n] = 0;
    url_decode(enc, n, tid, sizeof tid);
    return vehicle_info(db, tid);
  }

  return NULL;   /* unknown subpath → caller 404/501 */
}
