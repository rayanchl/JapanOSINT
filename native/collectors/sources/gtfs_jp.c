/* collectors/transport/sources/gtfs_jp.c — GTFS-JP nationwide ingest.
 *
 * Supersedes the old stops.txt-only collector. That one walked
 * api.gtfs-data.jp/v2/organizations -> /feeds -> /files/stops.txt; every one of
 * those paths now returns 404 (the v2 API was reduced to /v2/files), so it had
 * been emitting nothing at all. The surviving endpoint returns the whole
 * catalogue in a single request — ~545 feeds, each with a direct `file_url`
 * pointing at the operator's GTFS zip — which is both simpler and cheaper than
 * the old per-org walk.
 *
 * Two outputs from one download:
 *   1. GeoJSON Point features for every stop  -> the `gtfs-jp` map layer
 *      (what the previous collector produced, now nationwide rather than the
 *      first 20 orgs).
 *   2. Full relational ingest into gtfs_stops / gtfs_routes / gtfs_trips /
 *      gtfs_stop_times / gtfs_shapes / gtfs_calendar, which is what
 *      /api/transit/active-trips reads to interpolate live vehicle positions.
 *      Those tables have existed (and been empty) since the C port; the
 *      hydrate route that used to fill them is 501 here.
 *
 * Two properties of real GTFS-JP feeds drive the design — measured over a
 * 12-feed sample spread across the catalogue:
 *
 *   shape_dist_traveled is present in only ~2/12 feeds. active-trips requires
 *   it on both stop_times (to locate a vehicle between two stops) and shapes
 *   (to project that offset onto the polyline), and skips any trip missing it.
 *   So we compute it at ingest: cumulative haversine along each shape, then
 *   each stop snapped onto that polyline. Feeds that ship the column get it
 *   recomputed too — mixing an operator's units with ours inside one trip
 *   would be worse than being uniformly self-consistent.
 *
 *   ~3/12 feeds ship no shapes.txt at all (and leave trips.shape_id empty).
 *   Rather than drop a quarter of the country, those trips get a synthetic
 *   shape built from their own stop sequence, shared between every trip with
 *   the same stop pattern. Distances are then exact by construction, and
 *   vehicles travel straight lines between stops instead of following the
 *   road — visibly coarser, honestly so, and much better than the operator
 *   being absent from the map.
 *
 * CSV is parsed by a streaming in-place iterator rather than lib/csv.h's
 * csv_parse: the latter materialises a cJSON object per row, and the largest
 * feeds carry 50k+ stop_times rows (nationwide ~3.2M), which would cost GBs.
 * The iterator mutates the decompressed buffer and hands out borrowed cell
 * pointers, so peak memory is one decompressed member.
 *
 *   props order (map layer): stop_id, name, operator, feed_id, source */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../lib/zipread.h"
#include "../../core/db.h"
#include "../../third_party/sqlite3.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define CATALOG_URL "https://api.gtfs-data.jp/v2/files"
#define MAXCOL 64

/* ── tiny open-addressing string -> int map ──────────────────────────────
 * Keys are BORROWED (owned by the caller's id arrays, which outlive the map).
 * Values are indices into those arrays; -1 means absent. */
typedef struct { const char **k; int *v; int cap, n; } smap;

static unsigned long fnv1a(const char *s) {
  unsigned long h = 2166136261UL;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619UL; }
  return h;
}
static int smap_init(smap *m, int want) {
  int c = 16; while (c < want * 2) c <<= 1;
  m->k = calloc((size_t)c, sizeof *m->k);
  m->v = malloc((size_t)c * sizeof *m->v);
  if (!m->k || !m->v) { free(m->k); free(m->v); m->k = NULL; m->v = NULL; m->cap = 0; return -1; }
  for (int i = 0; i < c; i++) m->v[i] = -1;
  m->cap = c; m->n = 0;
  return 0;
}
static void smap_free(smap *m) {
  free(m->k); free(m->v);
  m->k = NULL; m->v = NULL; m->cap = m->n = 0;
}
static void smap_put(smap *m, const char *key, int val);
static void smap_grow(smap *m) {
  smap nm;
  if (smap_init(&nm, m->cap) != 0) return;      /* keep the old map on OOM */
  for (int i = 0; i < m->cap; i++)
    if (m->k[i]) smap_put(&nm, m->k[i], m->v[i]);
  free(m->k); free(m->v);
  *m = nm;
}
static void smap_put(smap *m, const char *key, int val) {
  if (!m->k || !key) return;
  if ((m->n + 1) * 10 >= m->cap * 7) smap_grow(m);
  if (!m->k) return;
  unsigned long h = fnv1a(key);
  int i = (int)(h & (unsigned long)(m->cap - 1));
  while (m->k[i]) {
    if (strcmp(m->k[i], key) == 0) { m->v[i] = val; return; }
    i = (i + 1) & (m->cap - 1);
  }
  m->k[i] = key; m->v[i] = val; m->n++;
}
static int smap_get(const smap *m, const char *key) {
  if (!m->k || !key) return -1;
  unsigned long h = fnv1a(key);
  int i = (int)(h & (unsigned long)(m->cap - 1));
  while (m->k[i]) {
    if (strcmp(m->k[i], key) == 0) return m->v[i];
    i = (i + 1) & (m->cap - 1);
  }
  return -1;
}

/* ── streaming in-place CSV iterator ─────────────────────────────────────
 * Handles quoted fields with embedded commas and "" escapes. Cells are
 * NUL-terminated slices of `buf` (unquoting only ever shrinks, so the rewrite
 * is always in-place). Borrowed pointers: valid until the next row. */
typedef struct {
  char *buf, *cur;
  char *cells[MAXCOL]; int ncell;
  char *hdr[MAXCOL];   int nhdr;
} csvit;

static char *ctrim(char *s) {
  while (*s && isspace((unsigned char)*s)) s++;
  char *e = s + strlen(s);
  while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
  return s;
}

static int csv_row(csvit *it) {
  if (!it->cur || !*it->cur) return 0;
  it->ncell = 0;
  char *p = it->cur;
  for (;;) {
    char *start, *dst;
    if (*p == '"') {
      p++; start = dst = p;
      while (*p) {
        if (*p == '"') {
          if (p[1] == '"') { *dst++ = '"'; p += 2; continue; }
          p++; break;                       /* closing quote */
        }
        *dst++ = *p++;
      }
      /* tolerate junk between the closing quote and the delimiter */
      while (*p && *p != ',' && *p != '\n' && *p != '\r') p++;
    } else {
      start = p;
      while (*p && *p != ',' && *p != '\n' && *p != '\r') p++;
      dst = p;
    }
    char term = *p;
    *dst = 0;                                /* dst <= p always */
    if (it->ncell < MAXCOL) it->cells[it->ncell++] = start;
    if (term == ',') { p++; continue; }
    if (term == '\r') { p++; if (*p == '\n') p++; }
    else if (term == '\n') p++;
    it->cur = p;
    return 1;
  }
}

/* Takes ownership of `text`. Reads the header row; returns 0 on success. */
static int csv_open(csvit *it, char *text) {
  memset(it, 0, sizeof *it);
  if (!text) return -1;
  it->buf = text;
  it->cur = text;
  if (!strncmp(it->cur, "\xEF\xBB\xBF", 3)) it->cur += 3;   /* UTF-8 BOM */
  if (!csv_row(it)) { free(text); it->buf = NULL; return -1; }
  it->nhdr = it->ncell;
  for (int i = 0; i < it->ncell; i++) it->hdr[i] = ctrim(it->cells[i]);
  return 0;
}
static void csv_close(csvit *it) { free(it->buf); it->buf = NULL; }

static int csv_col(const csvit *it, const char *name) {
  for (int i = 0; i < it->nhdr; i++)
    if (strcmp(it->hdr[i], name) == 0) return i;
  return -1;
}
/* Cell by column index; "" when the row is short (GTFS allows ragged tails). */
static const char *csv_at(const csvit *it, int col) {
  if (col < 0 || col >= it->ncell) return "";
  return it->cells[col];
}

/* ── geo + time helpers ──────────────────────────────────────────────────*/
#define R_EARTH 6371000.0
#define D2R (M_PI / 180.0)

static double hav_m(double la1, double lo1, double la2, double lo2) {
  double dla = (la2 - la1) * D2R, dlo = (lo2 - lo1) * D2R;
  double a = sin(dla / 2) * sin(dla / 2) +
             cos(la1 * D2R) * cos(la2 * D2R) * sin(dlo / 2) * sin(dlo / 2);
  return 2 * R_EARTH * atan2(sqrt(a), sqrt(1 - a));
}

/* "HH:MM:SS" -> seconds after midnight; hours may exceed 24 (GTFS overnight).
 * Returns -1 for an empty/unparseable field, which the caller binds as NULL. */
static long hms_sec(const char *s) {
  if (!s || !*s) return -1;
  int h = 0, m = 0, sec = 0;
  while (isspace((unsigned char)*s)) s++;
  int n = sscanf(s, "%d:%d:%d", &h, &m, &sec);
  if (n < 2 || h < 0 || m < 0 || sec < 0) return -1;
  return (long)h * 3600 + (long)m * 60 + sec;
}

static int is_num(const char *s) {
  if (!s || !*s) return 0;
  char *e; strtod(s, &e);
  return e != s;
}

typedef struct { double lat, lon, dist; } shpt;   /* dist = cumulative metres */
typedef struct { int off, n; } shref;             /* slice of the point pool */

/* Distance from (lat,lon) to segment a-b in metres, local planar
 * approximation. `t_out` receives the clamped projection parameter. */
static double seg_dist_m(double lat, double lon, const shpt *a, const shpt *b,
                         double *t_out) {
  double coslat = cos(lat * D2R);
  double ax = (a->lon - lon) * coslat, ay = a->lat - lat;
  double bx = (b->lon - lon) * coslat, by = b->lat - lat;
  double dx = bx - ax, dy = by - ay;
  double len2 = dx * dx + dy * dy;
  double t = len2 > 0 ? -(ax * dx + ay * dy) / len2 : 0;
  if (t < 0) t = 0; else if (t > 1) t = 1;
  double px = ax + t * dx, py = ay + t * dy;
  if (t_out) *t_out = t;
  return sqrt(px * px + py * py) * (R_EARTH * D2R);
}

/* Project a stop onto the polyline, searching forward only from *from so a
 * trip's distances stay monotonic (a loop revisiting the same place must
 * advance, not snap back). Returns cumulative metres. */
static double snap_dist(const shpt *p, int n, int *from, double lat, double lon) {
  if (n < 2) return 0;
  int start = *from;
  if (start < 0) start = 0;
  if (start > n - 2) start = n - 2;
  double best = 1e30, best_t = 0;
  int best_i = start;
  for (int i = start; i < n - 1; i++) {
    double t, d = seg_dist_m(lat, lon, &p[i], &p[i + 1], &t);
    if (d < best) { best = d; best_t = t; best_i = i; }
    /* Long shapes: once we have a confident hit, don't scan the whole tail. */
    if (best < 50.0 && i - best_i > 500) break;
  }
  *from = best_i;
  double seg = p[best_i + 1].dist - p[best_i].dist;
  return p[best_i].dist + best_t * seg;
}

/* ── per-feed ingest state ───────────────────────────────────────────────*/
typedef struct {
  sqlite3_stmt *stop, *route, *trip, *st, *shape, *cal;
} stmts;

typedef struct {
  int stops, routes, trips, stop_times, shapes, syn_shapes, skipped_trips;
} feedstat;

typedef struct {
  sqlite3 *h;
  stmts *s;
  const char *org, *feed;
  feedstat *fs;

  /* stops */
  char **sid; double *slat, *slon; int ns;
  smap sidmap;

  /* shapes: `pool` is seq-ordered, sliced per shape by `sref` */
  char **shid; int nsh;
  smap shmap;
  shpt *pool; shref *sref;

  /* trips (persisted after stop_times, since synthesis rewrites shape_id) */
  char **tid, **trt, **tsv, **thd;
  int *tshape, *tdir; char **tsyn;
  int nt;
  smap tmap;

  /* synthetic shapes, deduped by stop-pattern signature */
  smap synmap; char **synkey; int nsyn, syncap;

  /* snapped-distance cache, keyed "<shape_idx>|<pattern signature>" */
  smap dmap; char **dkey; double **dval; int *dlen; int nd, dcap;
} feedctx;

/* bind text-or-NULL: an empty GTFS field means absent. */
static void bind_txt(sqlite3_stmt *s, int i, const char *v) {
  if (v && *v) sqlite3_bind_text(s, i, v, -1, SQLITE_TRANSIENT);
  else         sqlite3_bind_null(s, i);
}

/* FNV over the stop-id sequence — identifies a stop pattern. */
static void pattern_sig(char **stop, int n, char *out, size_t cap) {
  unsigned long h = 2166136261UL;
  for (int i = 0; i < n; i++) {
    for (const char *p = stop[i]; *p; p++) { h ^= (unsigned char)*p; h *= 16777619UL; }
    h ^= 0x2c; h *= 16777619UL;
  }
  snprintf(out, cap, "%lx.%d", h, n);
}

/* Build (or reuse) a synthetic shape from this trip's own stops, and write its
 * points to gtfs_shapes. Returns the synthetic shape id (owned by ctx), or
 * NULL if the stops carry no usable coordinates. */
static const char *synth_shape(feedctx *c, char **stop, int n, double *dists) {
  /* Walk the stop sequence once, accumulating haversine distance. Stops whose
   * id isn't in stops.txt carry the running total forward unchanged. */
  double acc = 0;
  int prev = -1, usable = 0;
  for (int i = 0; i < n; i++) {
    int si = smap_get(&c->sidmap, stop[i]);
    if (si < 0) { dists[i] = acc; continue; }
    if (prev >= 0)
      acc += hav_m(c->slat[prev], c->slon[prev], c->slat[si], c->slon[si]);
    dists[i] = acc;
    prev = si;
    usable++;
  }
  if (usable < 2) return NULL;          /* can't make a polyline from one point */

  /* The synthetic id doubles as the map key, so one allocation owns both. */
  char name[80];
  char sig[64];
  pattern_sig(stop, n, sig, sizeof sig);
  snprintf(name, sizeof name, "SYN_%s", sig);

  int idx = smap_get(&c->synmap, name);
  if (idx >= 0) return c->synkey[idx];   /* pattern already materialised */

  if (c->nsyn == c->syncap) {
    int nc = c->syncap ? c->syncap * 2 : 64;
    char **nk = realloc(c->synkey, (size_t)nc * sizeof *nk);
    if (!nk) return NULL;
    c->synkey = nk; c->syncap = nc;
  }
  char *owned = strdup(name);
  if (!owned) return NULL;
  c->synkey[c->nsyn] = owned;

  int seq = 0;
  prev = -1;
  for (int i = 0; i < n; i++) {
    int si = smap_get(&c->sidmap, stop[i]);
    if (si < 0) continue;
    sqlite3_reset(c->s->shape); sqlite3_clear_bindings(c->s->shape);
    bind_txt(c->s->shape, 1, c->org); bind_txt(c->s->shape, 2, c->feed);
    bind_txt(c->s->shape, 3, owned);
    sqlite3_bind_int(c->s->shape, 4, seq++);
    sqlite3_bind_double(c->s->shape, 5, c->slat[si]);
    sqlite3_bind_double(c->s->shape, 6, c->slon[si]);
    sqlite3_bind_double(c->s->shape, 7, dists[i]);
    sqlite3_step(c->s->shape);
    c->fs->shapes++;
    prev = si;
  }
  (void)prev;

  smap_put(&c->synmap, owned, c->nsyn);   /* borrowed key, owned by synkey */
  c->fs->syn_shapes++;
  return c->synkey[c->nsyn++];
}

/* Persist one trip's stop_times, computing shape_dist_traveled. */
static void flush_group(feedctx *c, const char *trip, int n,
                        const int *seq, char **stop,
                        const long *arr, const long *dep) {
  if (n < 1) return;
  int ti = smap_get(&c->tmap, trip);
  if (ti < 0) { c->fs->skipped_trips++; return; }   /* stop_times for an unknown trip */

  double *dists = malloc((size_t)n * sizeof *dists);
  if (!dists) return;
  for (int i = 0; i < n; i++) dists[i] = 0;

  int shape_idx = c->tshape[ti];
  int have = 0;

  if (shape_idx >= 0 && c->sref && c->sref[shape_idx].n >= 2) {
    /* cache key: shape + stop pattern (trips on a route share both) */
    char sig[64], key[96];
    pattern_sig(stop, n, sig, sizeof sig);
    snprintf(key, sizeof key, "%d|%s", shape_idx, sig);
    int di = smap_get(&c->dmap, key);
    if (di >= 0 && c->dlen[di] == n) {
      memcpy(dists, c->dval[di], (size_t)n * sizeof *dists);
      have = 1;
    } else {
      const shpt *p = c->pool + c->sref[shape_idx].off;
      int np = c->sref[shape_idx].n, from = 0;
      for (int i = 0; i < n; i++) {
        int si = smap_get(&c->sidmap, stop[i]);
        if (si < 0) { dists[i] = i ? dists[i - 1] : 0; continue; }
        dists[i] = snap_dist(p, np, &from, c->slat[si], c->slon[si]);
        if (i && dists[i] < dists[i - 1]) dists[i] = dists[i - 1];  /* monotonic */
      }
      have = 1;
      /* memoise */
      if (c->nd == c->dcap) {
        int nc = c->dcap ? c->dcap * 2 : 64;
        char **nk = realloc(c->dkey, (size_t)nc * sizeof *nk);
        double **nv = realloc(c->dval, (size_t)nc * sizeof *nv);
        int *nl = realloc(c->dlen, (size_t)nc * sizeof *nl);
        if (nk && nv && nl) { c->dkey = nk; c->dval = nv; c->dlen = nl; c->dcap = nc; }
      }
      if (c->nd < c->dcap) {
        char *kd = strdup(key);
        double *vd = malloc((size_t)n * sizeof *vd);
        if (kd && vd) {
          memcpy(vd, dists, (size_t)n * sizeof *vd);
          c->dkey[c->nd] = kd; c->dval[c->nd] = vd; c->dlen[c->nd] = n;
          smap_put(&c->dmap, kd, c->nd);
          c->nd++;
        } else { free(kd); free(vd); }
      }
    }
  } else {
    /* no usable shape -> synthesise one from the stop sequence */
    const char *syn = synth_shape(c, stop, n, dists);
    if (syn) {
      if (!c->tsyn[ti]) c->tsyn[ti] = strdup(syn);
      have = 1;
    }
  }

  if (!have) { free(dists); c->fs->skipped_trips++; return; }

  for (int i = 0; i < n; i++) {
    sqlite3_reset(c->s->st); sqlite3_clear_bindings(c->s->st);
    bind_txt(c->s->st, 1, c->org); bind_txt(c->s->st, 2, c->feed);
    bind_txt(c->s->st, 3, trip);
    sqlite3_bind_int(c->s->st, 4, seq[i]);
    bind_txt(c->s->st, 5, stop[i]);
    if (arr[i] >= 0) sqlite3_bind_int64(c->s->st, 6, arr[i]);
    else             sqlite3_bind_null(c->s->st, 6);
    if (dep[i] >= 0) sqlite3_bind_int64(c->s->st, 7, dep[i]);
    else             sqlite3_bind_null(c->s->st, 7);
    sqlite3_bind_double(c->s->st, 8, dists[i]);
    sqlite3_step(c->s->st);
    c->fs->stop_times++;
  }
  free(dists);
}

static int prep_all(sqlite3 *h, stmts *s) {
  memset(s, 0, sizeof *s);
  if (sqlite3_prepare_v2(h,
        "INSERT OR REPLACE INTO gtfs_stops(org_id,feed_id,stop_id,name,lat,lon)"
        " VALUES(?1,?2,?3,?4,?5,?6)", -1, &s->stop, NULL) != SQLITE_OK) return -1;
  if (sqlite3_prepare_v2(h,
        "INSERT OR REPLACE INTO gtfs_routes(org_id,feed_id,route_id,short_name,"
        "long_name,route_type,color,text_color) VALUES(?1,?2,?3,?4,?5,?6,?7,?8)",
        -1, &s->route, NULL) != SQLITE_OK) return -1;
  if (sqlite3_prepare_v2(h,
        "INSERT OR REPLACE INTO gtfs_trips(org_id,feed_id,trip_id,route_id,"
        "service_id,shape_id,headsign,direction_id) VALUES(?1,?2,?3,?4,?5,?6,?7,?8)",
        -1, &s->trip, NULL) != SQLITE_OK) return -1;
  if (sqlite3_prepare_v2(h,
        "INSERT OR REPLACE INTO gtfs_stop_times(org_id,feed_id,trip_id,"
        "stop_sequence,stop_id,arrival_sec,departure_sec,shape_dist_traveled)"
        " VALUES(?1,?2,?3,?4,?5,?6,?7,?8)", -1, &s->st, NULL) != SQLITE_OK) return -1;
  if (sqlite3_prepare_v2(h,
        "INSERT OR REPLACE INTO gtfs_shapes(org_id,feed_id,shape_id,seq,lat,lon,"
        "dist_m) VALUES(?1,?2,?3,?4,?5,?6,?7)", -1, &s->shape, NULL) != SQLITE_OK) return -1;
  if (sqlite3_prepare_v2(h,
        "INSERT OR REPLACE INTO gtfs_calendar(org_id,feed_id,service_id,mon,tue,"
        "wed,thu,fri,sat,sun,start_date,end_date)"
        " VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12)",
        -1, &s->cal, NULL) != SQLITE_OK) return -1;
  return 0;
}
static void fin_all(stmts *s) {
  sqlite3_finalize(s->stop);  sqlite3_finalize(s->route);
  sqlite3_finalize(s->trip);  sqlite3_finalize(s->st);
  sqlite3_finalize(s->shape); sqlite3_finalize(s->cal);
}

static char *member(const char *zip, size_t zl, const char *name) {
  size_t n = 0;
  return zip_find_entry(zip, zl, name, &n);
}

/* ---- the six files, in dependency order ------------------------------- */

static void load_stops(feedctx *c, const char *zip, size_t zl,
                       cJSON *features, const char *org_name, int *budget) {
  char *txt = member(zip, zl, "stops.txt");
  if (!txt) return;
  csvit it;
  if (csv_open(&it, txt) != 0) return;
  int c_id = csv_col(&it, "stop_id"), c_nm = csv_col(&it, "stop_name"),
      c_la = csv_col(&it, "stop_lat"), c_lo = csv_col(&it, "stop_lon");
  int cap = 0;    /* exhaustive-ok: dynamic array capacity, doubles on demand */
  while (csv_row(&it)) {
    const char *id = csv_at(&it, c_id);
    const char *la = csv_at(&it, c_la), *lo = csv_at(&it, c_lo);
    if (!*id || !is_num(la) || !is_num(lo)) continue;
    if (c->ns == cap) {
      cap = cap ? cap * 2 : 256;
      char **a = realloc(c->sid, (size_t)cap * sizeof *a);
      double *b = realloc(c->slat, (size_t)cap * sizeof *b);
      double *d = realloc(c->slon, (size_t)cap * sizeof *d);
      if (!a || !b || !d) { c->sid = a ? a : c->sid; break; }
      c->sid = a; c->slat = b; c->slon = d;
    }
    c->sid[c->ns] = strdup(id);
    c->slat[c->ns] = strtod(la, NULL);
    c->slon[c->ns] = strtod(lo, NULL);
    smap_put(&c->sidmap, c->sid[c->ns], c->ns);

    sqlite3_reset(c->s->stop); sqlite3_clear_bindings(c->s->stop);
    bind_txt(c->s->stop, 1, c->org); bind_txt(c->s->stop, 2, c->feed);
    bind_txt(c->s->stop, 3, id);
    bind_txt(c->s->stop, 4, csv_at(&it, c_nm));
    sqlite3_bind_double(c->s->stop, 5, c->slat[c->ns]);
    sqlite3_bind_double(c->s->stop, 6, c->slon[c->ns]);
    sqlite3_step(c->s->stop);
    c->fs->stops++;

    if (features && *budget > 0) {
      cJSON *f = gj_point_feature(c->slon[c->ns], c->slat[c->ns]);
      cJSON *pr = cJSON_CreateObject();
      cJSON_AddStringToObject(pr, "stop_id", id);
      const char *nm = csv_at(&it, c_nm);
      if (*nm) cJSON_AddStringToObject(pr, "name", nm);
      else cJSON_AddNullToObject(pr, "name");
      cJSON_AddStringToObject(pr, "operator", org_name ? org_name : c->org);
      cJSON_AddStringToObject(pr, "feed_id", c->feed);
      cJSON_AddStringToObject(pr, "source", "gtfs_jp");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
      (*budget)--;
    }
    c->ns++;
  }
  csv_close(&it);
}

static void load_shapes(feedctx *c, const char *zip, size_t zl) {
  char *txt = member(zip, zl, "shapes.txt");
  if (!txt) return;
  csvit si;
  if (csv_open(&si, txt) != 0) return;
  int k_id = csv_col(&si, "shape_id"), k_la = csv_col(&si, "shape_pt_lat"),
      k_lo = csv_col(&si, "shape_pt_lon"), k_sq = csv_col(&si, "shape_pt_sequence");

  shpt *pool = NULL; int npool = 0, poolcap = 0;
  int *pshape = NULL, *pseq = NULL;
  int shcap = 0;  /* exhaustive-ok: dynamic array capacity, doubles on demand */
  while (csv_row(&si)) {
    const char *id = csv_at(&si, k_id);
    const char *la = csv_at(&si, k_la), *lo = csv_at(&si, k_lo);
    if (!*id || !is_num(la) || !is_num(lo)) continue;
    int idx = smap_get(&c->shmap, id);
    if (idx < 0) {
      if (c->nsh == shcap) {
        shcap = shcap ? shcap * 2 : 128;
        char **a = realloc(c->shid, (size_t)shcap * sizeof *a);
        if (!a) break;
        c->shid = a;
      }
      c->shid[c->nsh] = strdup(id);
      smap_put(&c->shmap, c->shid[c->nsh], c->nsh);
      idx = c->nsh++;
    }
    if (npool == poolcap) {
      poolcap = poolcap ? poolcap * 2 : 4096;
      shpt *a = realloc(pool, (size_t)poolcap * sizeof *a);
      int *b = realloc(pshape, (size_t)poolcap * sizeof *b);
      int *d = realloc(pseq, (size_t)poolcap * sizeof *d);
      if (!a || !b || !d) { pool = a ? a : pool; break; }
      pool = a; pshape = b; pseq = d;
    }
    pool[npool].lat = strtod(la, NULL);
    pool[npool].lon = strtod(lo, NULL);
    pool[npool].dist = 0;
    pshape[npool] = idx;
    pseq[npool] = atoi(csv_at(&si, k_sq));
    npool++;
  }
  csv_close(&si);

  if (npool < 1 || c->nsh < 1) { free(pool); free(pshape); free(pseq); return; }

  /* counting-sort into per-shape slices, then order each slice by seq */
  c->sref = calloc((size_t)c->nsh, sizeof *c->sref);
  int *cnt = calloc((size_t)c->nsh, sizeof *cnt);
  int *fill = calloc((size_t)c->nsh, sizeof *fill);
  shpt *ord = malloc((size_t)npool * sizeof *ord);
  int *oseq = malloc((size_t)npool * sizeof *oseq);
  if (!c->sref || !cnt || !fill || !ord || !oseq) {
    free(cnt); free(fill); free(ord); free(oseq);
    free(pool); free(pshape); free(pseq);
    free(c->sref); c->sref = NULL;
    return;
  }
  for (int i = 0; i < npool; i++) cnt[pshape[i]]++;
  int acc = 0;
  for (int i = 0; i < c->nsh; i++) { c->sref[i].off = acc; c->sref[i].n = cnt[i]; acc += cnt[i]; }
  for (int i = 0; i < npool; i++) {
    int g = pshape[i];
    int at = c->sref[g].off + fill[g]++;
    ord[at] = pool[i];
    oseq[at] = pseq[i];
  }
  for (int g = 0; g < c->nsh; g++) {
    int o = c->sref[g].off, n = c->sref[g].n;
    for (int i = o + 1; i < o + n; i++) {           /* insertion sort by seq */
      shpt v = ord[i]; int vs = oseq[i]; int j = i - 1;
      while (j >= o && oseq[j] > vs) { ord[j + 1] = ord[j]; oseq[j + 1] = oseq[j]; j--; }
      ord[j + 1] = v; oseq[j + 1] = vs;
    }
    double m = 0;
    for (int i = o; i < o + n; i++) {
      if (i > o) m += hav_m(ord[i-1].lat, ord[i-1].lon, ord[i].lat, ord[i].lon);
      ord[i].dist = m;
      sqlite3_reset(c->s->shape); sqlite3_clear_bindings(c->s->shape);
      bind_txt(c->s->shape, 1, c->org); bind_txt(c->s->shape, 2, c->feed);
      bind_txt(c->s->shape, 3, c->shid[g]);
      sqlite3_bind_int(c->s->shape, 4, oseq[i]);
      sqlite3_bind_double(c->s->shape, 5, ord[i].lat);
      sqlite3_bind_double(c->s->shape, 6, ord[i].lon);
      sqlite3_bind_double(c->s->shape, 7, ord[i].dist);
      sqlite3_step(c->s->shape);
      c->fs->shapes++;
    }
  }
  free(cnt); free(fill); free(oseq); free(pool); free(pshape); free(pseq);
  c->pool = ord;
}

static void load_trips(feedctx *c, const char *zip, size_t zl) {
  char *txt = member(zip, zl, "trips.txt");
  if (!txt) return;
  csvit ti;
  if (csv_open(&ti, txt) != 0) return;
  int k_t = csv_col(&ti, "trip_id"), k_r = csv_col(&ti, "route_id"),
      k_s = csv_col(&ti, "service_id"), k_sh = csv_col(&ti, "shape_id"),
      k_h = csv_col(&ti, "trip_headsign"), k_d = csv_col(&ti, "direction_id");
  int cap = 0;    /* exhaustive-ok: dynamic array capacity, doubles on demand */
  while (csv_row(&ti)) {
    const char *id = csv_at(&ti, k_t);
    if (!*id) continue;
    if (c->nt == cap) {
      cap = cap ? cap * 2 : 512;
      char **a = realloc(c->tid, (size_t)cap * sizeof *a);
      char **b = realloc(c->trt, (size_t)cap * sizeof *b);
      char **d = realloc(c->tsv, (size_t)cap * sizeof *d);
      char **e = realloc(c->thd, (size_t)cap * sizeof *e);
      int  *f = realloc(c->tshape, (size_t)cap * sizeof *f);
      int  *g = realloc(c->tdir, (size_t)cap * sizeof *g);
      char **k = realloc(c->tsyn, (size_t)cap * sizeof *k);
      if (!a || !b || !d || !e || !f || !g || !k) { c->tid = a ? a : c->tid; break; }
      c->tid = a; c->trt = b; c->tsv = d; c->thd = e;
      c->tshape = f; c->tdir = g; c->tsyn = k;
    }
    c->tid[c->nt] = strdup(id);
    const char *v;
    v = csv_at(&ti, k_r); c->trt[c->nt] = *v ? strdup(v) : NULL;
    v = csv_at(&ti, k_s); c->tsv[c->nt] = *v ? strdup(v) : NULL;
    v = csv_at(&ti, k_h); c->thd[c->nt] = *v ? strdup(v) : NULL;
    v = csv_at(&ti, k_d); c->tdir[c->nt] = *v ? atoi(v) : -1;
    v = csv_at(&ti, k_sh);
    c->tshape[c->nt] = (*v && c->nsh) ? smap_get(&c->shmap, v) : -1;
    c->tsyn[c->nt] = NULL;
    smap_put(&c->tmap, c->tid[c->nt], c->nt);
    c->nt++;
  }
  csv_close(&ti);
}

static void load_stop_times(feedctx *c, const char *zip, size_t zl) {
  char *txt = member(zip, zl, "stop_times.txt");
  if (!txt) return;
  csvit qi;
  if (csv_open(&qi, txt) != 0) return;
  int k_t = csv_col(&qi, "trip_id"), k_sq = csv_col(&qi, "stop_sequence"),
      k_si = csv_col(&qi, "stop_id"), k_a = csv_col(&qi, "arrival_time"),
      k_d = csv_col(&qi, "departure_time");

  char *cur = NULL;
  int *g_seq = NULL; char **g_stop = NULL; long *g_arr = NULL, *g_dep = NULL;
  int gn = 0, gcap = 0;

  while (csv_row(&qi)) {
    const char *t = csv_at(&qi, k_t);
    if (!*t) continue;
    if (!cur || strcmp(cur, t) != 0) {
      if (cur && gn > 0) flush_group(c, cur, gn, g_seq, g_stop, g_arr, g_dep);
      for (int i = 0; i < gn; i++) free(g_stop[i]);
      gn = 0;
      free(cur); cur = strdup(t);
    }
    if (gn == gcap) {
      gcap = gcap ? gcap * 2 : 64;
      int  *a = realloc(g_seq, (size_t)gcap * sizeof *a);
      char **b = realloc(g_stop, (size_t)gcap * sizeof *b);
      long *d = realloc(g_arr, (size_t)gcap * sizeof *d);
      long *e = realloc(g_dep, (size_t)gcap * sizeof *e);
      if (!a || !b || !d || !e) { g_seq = a ? a : g_seq; break; }
      g_seq = a; g_stop = b; g_arr = d; g_dep = e;
    }
    g_seq[gn] = atoi(csv_at(&qi, k_sq));
    g_stop[gn] = strdup(csv_at(&qi, k_si));
    g_arr[gn] = hms_sec(csv_at(&qi, k_a));
    g_dep[gn] = hms_sec(csv_at(&qi, k_d));
    gn++;
  }
  if (cur && gn > 0) flush_group(c, cur, gn, g_seq, g_stop, g_arr, g_dep);
  for (int i = 0; i < gn; i++) free(g_stop[i]);
  free(cur); free(g_seq); free(g_stop); free(g_arr); free(g_dep);
  csv_close(&qi);
}

static void save_trips(feedctx *c) {
  for (int i = 0; i < c->nt; i++) {
    sqlite3_reset(c->s->trip); sqlite3_clear_bindings(c->s->trip);
    bind_txt(c->s->trip, 1, c->org); bind_txt(c->s->trip, 2, c->feed);
    bind_txt(c->s->trip, 3, c->tid[i]);
    bind_txt(c->s->trip, 4, c->trt[i]); bind_txt(c->s->trip, 5, c->tsv[i]);
    if (c->tsyn[i])            bind_txt(c->s->trip, 6, c->tsyn[i]);
    else if (c->tshape[i] >= 0) bind_txt(c->s->trip, 6, c->shid[c->tshape[i]]);
    else                        sqlite3_bind_null(c->s->trip, 6);
    bind_txt(c->s->trip, 7, c->thd[i]);
    if (c->tdir[i] >= 0) sqlite3_bind_int(c->s->trip, 8, c->tdir[i]);
    else                 sqlite3_bind_null(c->s->trip, 8);
    sqlite3_step(c->s->trip);
    c->fs->trips++;
  }
}

static void load_routes(feedctx *c, const char *zip, size_t zl) {
  char *txt = member(zip, zl, "routes.txt");
  if (!txt) return;
  csvit ri;
  if (csv_open(&ri, txt) != 0) return;
  int k_id = csv_col(&ri, "route_id"), k_sn = csv_col(&ri, "route_short_name"),
      k_ln = csv_col(&ri, "route_long_name"), k_ty = csv_col(&ri, "route_type"),
      k_c = csv_col(&ri, "route_color"), k_tc = csv_col(&ri, "route_text_color");
  while (csv_row(&ri)) {
    const char *id = csv_at(&ri, k_id);
    if (!*id) continue;
    sqlite3_reset(c->s->route); sqlite3_clear_bindings(c->s->route);
    bind_txt(c->s->route, 1, c->org); bind_txt(c->s->route, 2, c->feed);
    bind_txt(c->s->route, 3, id);
    bind_txt(c->s->route, 4, csv_at(&ri, k_sn));
    bind_txt(c->s->route, 5, csv_at(&ri, k_ln));
    const char *ty = csv_at(&ri, k_ty);
    if (*ty) sqlite3_bind_int(c->s->route, 6, atoi(ty));
    else     sqlite3_bind_null(c->s->route, 6);
    bind_txt(c->s->route, 7, csv_at(&ri, k_c));
    bind_txt(c->s->route, 8, csv_at(&ri, k_tc));
    sqlite3_step(c->s->route);
    c->fs->routes++;
  }
  csv_close(&ri);
}

static void load_calendar(feedctx *c, const char *zip, size_t zl) {
  char *txt = member(zip, zl, "calendar.txt");
  if (!txt) return;
  csvit ci;
  if (csv_open(&ci, txt) != 0) return;
  static const char *DOW[7] = { "monday","tuesday","wednesday","thursday",
                                "friday","saturday","sunday" };
  int k_s = csv_col(&ci, "service_id");
  int k_d[7]; for (int i = 0; i < 7; i++) k_d[i] = csv_col(&ci, DOW[i]);
  int k_f = csv_col(&ci, "start_date"), k_t = csv_col(&ci, "end_date");
  while (csv_row(&ci)) {
    const char *id = csv_at(&ci, k_s);
    if (!*id) continue;
    sqlite3_reset(c->s->cal); sqlite3_clear_bindings(c->s->cal);
    bind_txt(c->s->cal, 1, c->org); bind_txt(c->s->cal, 2, c->feed);
    bind_txt(c->s->cal, 3, id);
    for (int i = 0; i < 7; i++) {
      const char *v = csv_at(&ci, k_d[i]);
      sqlite3_bind_int(c->s->cal, 4 + i, (*v && atoi(v) == 1) ? 1 : 0);
    }
    bind_txt(c->s->cal, 11, csv_at(&ci, k_f));
    bind_txt(c->s->cal, 12, csv_at(&ci, k_t));
    sqlite3_step(c->s->cal);
  }
  csv_close(&ci);
}

static void ctx_free(feedctx *c) {
  for (int i = 0; i < c->ns; i++) free(c->sid[i]);
  free(c->sid); free(c->slat); free(c->slon); smap_free(&c->sidmap);
  for (int i = 0; i < c->nsh; i++) free(c->shid[i]);
  free(c->shid); smap_free(&c->shmap); free(c->pool); free(c->sref);
  for (int i = 0; i < c->nt; i++) {
    free(c->tid[i]); free(c->trt[i]); free(c->tsv[i]); free(c->thd[i]); free(c->tsyn[i]);
  }
  free(c->tid); free(c->trt); free(c->tsv); free(c->thd);
  free(c->tshape); free(c->tdir); free(c->tsyn); smap_free(&c->tmap);
  for (int i = 0; i < c->nsyn; i++) free(c->synkey[i]);
  free(c->synkey); smap_free(&c->synmap);
  for (int i = 0; i < c->nd; i++) { free(c->dkey[i]); free(c->dval[i]); }
  free(c->dkey); free(c->dval); free(c->dlen); smap_free(&c->dmap);
}

/* One feed: parse every member and persist. Caller owns the transaction. */
static void ingest_feed(sqlite3 *h, stmts *s, const char *org, const char *feed,
                        const char *zip, size_t zl, feedstat *fs,
                        cJSON *features, const char *org_name, int *budget) {
  feedctx c;
  memset(&c, 0, sizeof c);
  c.h = h; c.s = s; c.org = org; c.feed = feed; c.fs = fs;
  smap_init(&c.sidmap, 1024);
  smap_init(&c.shmap, 256);
  smap_init(&c.tmap, 1024);
  smap_init(&c.synmap, 256);
  smap_init(&c.dmap, 256);

  load_stops(&c, zip, zl, features, org_name, budget);
  load_shapes(&c, zip, zl);
  load_trips(&c, zip, zl);
  load_stop_times(&c, zip, zl);   /* may synthesise shapes + set tsyn */
  save_trips(&c);
  load_routes(&c, zip, zl);
  load_calendar(&c, zip, zl);

  ctx_free(&c);
}

/* ── binary GET (zip payloads; feed_get_text would truncate at a NUL) ─────*/
static char *http_bytes(http_client *hc, const char *url, size_t *len) {
  http_response r = {0};
  if (http_request(hc, "GET", url, NULL, NULL, 0, 60000, 1, &r) != 0 ||
      r.status < 200 || r.status >= 300) { http_response_free(&r); return NULL; }
  char *out = r.body; *len = r.body_len;
  r.body = NULL;                      /* transferred to caller */
  http_response_free(&r);
  return out;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->db || !ctx->db->h) {
    fprintf(stderr, "[gtfs-jp] no db handle\n");
    return -1;
  }
  sqlite3 *h = ctx->db->h;

  long feed_limit = 0;                /* 0 = every feed in the catalogue */
  long stop_cap = 50000;              /* map-layer feature budget */
  const char *e1 = getenv("GTFS_JP_FEED_LIMIT");
  if (e1 && e1[0]) feed_limit = strtol(e1, NULL, 10);
  const char *e2 = getenv("GTFS_JP_STOP_CAP");
  if (e2 && e2[0]) stop_cap = strtol(e2, NULL, 10);

  cJSON *wrap = feed_get_json(ctx->http, CATALOG_URL, 30000);
  cJSON *body = wrap ? cJSON_GetObjectItem(wrap, "body") : NULL;
  if (!body || !cJSON_IsArray(body) || cJSON_GetArraySize(body) == 0) {
    if (wrap) cJSON_Delete(wrap);
    fprintf(stderr, "[gtfs-jp] catalogue unavailable\n");
    return -1;
  }

  stmts s;
  if (prep_all(h, &s) != 0) {
    fin_all(&s); cJSON_Delete(wrap);
    fprintf(stderr, "[gtfs-jp] prepare failed: %s\n", sqlite3_errmsg(h));
    return -1;
  }
  sqlite3_stmt *sfeed = NULL, *sop = NULL;
  sqlite3_prepare_v2(h,
    "INSERT OR REPLACE INTO gtfs_feeds(feed_id,ag_id,ag_name,pref_code,"
    "feed_name,fixed_current_url,license_name,license_url,feed_end_date,"
    "last_refreshed_at) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,datetime('now'))",
    -1, &sfeed, NULL);
  sqlite3_prepare_v2(h,
    "INSERT INTO gtfs_operators(org_id,org_name,hydrated_at,feed_ids,"
    "stop_count,trip_count) VALUES(?1,?2,datetime('now'),'[]',?3,?4) "
    "ON CONFLICT(org_id) DO UPDATE SET org_name=excluded.org_name,"
    "hydrated_at=excluded.hydrated_at,"
    "stop_count=gtfs_operators.stop_count+excluded.stop_count,"
    "trip_count=gtfs_operators.trip_count+excluded.trip_count",
    -1, &sop, NULL);

  cJSON *features = cJSON_CreateArray();
  int budget = (int)stop_cap;
  long done = 0, failed = 0;
  feedstat tot; memset(&tot, 0, sizeof tot);

  cJSON *row;
  cJSON_ArrayForEach(row, body) {
    if (feed_limit > 0 && done >= feed_limit) break;
    if (ctx->cancel && *ctx->cancel) break;

    const char *org = jo_sv(row, "organization_id");
    const char *feed = jo_sv(row, "feed_id");
    const char *url = jo_sv(row, "file_url");
    if (!org || !feed || !url) continue;
    const char *org_name = jo_sv(row, "organization_name");
    const char *feed_name = jo_sv(row, "feed_name");

    size_t zl = 0;
    char *zip = http_bytes(ctx->http, url, &zl);
    if (!zip || zl < 22) { free(zip); failed++; continue; }

    /* One transaction per feed: a mid-feed failure can't leave a half-ingested
     * operator behind, and 545 commits is far cheaper than 3.2M. */
    sqlite3_exec(h, "BEGIN", NULL, NULL, NULL);

    /* Replace this feed's previous ingest rather than merging into it —
     * timetables get reissued and stale trips must not linger. */
    static const char *DEL[] = {
      "DELETE FROM gtfs_stop_times WHERE org_id=?1 AND feed_id=?2",
      "DELETE FROM gtfs_shapes     WHERE org_id=?1 AND feed_id=?2",
      "DELETE FROM gtfs_trips      WHERE org_id=?1 AND feed_id=?2",
      "DELETE FROM gtfs_routes     WHERE org_id=?1 AND feed_id=?2",
      "DELETE FROM gtfs_stops      WHERE org_id=?1 AND feed_id=?2",
      "DELETE FROM gtfs_calendar   WHERE org_id=?1 AND feed_id=?2",
    };
    for (size_t i = 0; i < sizeof DEL / sizeof *DEL; i++) {
      sqlite3_stmt *d = NULL;
      if (sqlite3_prepare_v2(h, DEL[i], -1, &d, NULL) == SQLITE_OK) {
        sqlite3_bind_text(d, 1, org, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(d, 2, feed, -1, SQLITE_TRANSIENT);
        sqlite3_step(d);
      }
      sqlite3_finalize(d);
    }

    feedstat fs;
    memset(&fs, 0, sizeof fs);
    ingest_feed(h, &s, org, feed, zip, zl, &fs, features, org_name, &budget);
    free(zip);

    /* catalogue bookkeeping. gtfs_feeds.feed_id is keyed "<org>*<feed>" — the
     * same composite gtfs-data.jp uses in its own permalinks — because feed
     * ids ("communitybus") repeat across operators and the column is a bare
     * PRIMARY KEY. Nothing joins this table; the position engine keys off
     * (org_id, feed_id) in the data tables. */
    if (sfeed) {
      char composite[256];
      snprintf(composite, sizeof composite, "%s*%s", org, feed);
      sqlite3_reset(sfeed); sqlite3_clear_bindings(sfeed);
      bind_txt(sfeed, 1, composite);
      bind_txt(sfeed, 2, org);
      bind_txt(sfeed, 3, org_name);
      cJSON *pref = cJSON_GetObjectItem(row, "feed_pref_id");
      if (pref && cJSON_IsNumber(pref)) {
        char pc[8]; snprintf(pc, sizeof pc, "%02d", (int)pref->valuedouble);
        bind_txt(sfeed, 4, pc);
      } else sqlite3_bind_null(sfeed, 4);
      bind_txt(sfeed, 5, feed_name);
      bind_txt(sfeed, 6, url);
      bind_txt(sfeed, 7, jo_sv(row, "feed_license_id"));
      bind_txt(sfeed, 8, jo_sv(row, "feed_license_url"));
      bind_txt(sfeed, 9, jo_sv(row, "file_to_date"));
      sqlite3_step(sfeed);
    }
    if (sop) {
      sqlite3_reset(sop); sqlite3_clear_bindings(sop);
      bind_txt(sop, 1, org);
      bind_txt(sop, 2, org_name);
      sqlite3_bind_int(sop, 3, fs.stops);
      sqlite3_bind_int(sop, 4, fs.trips);
      sqlite3_step(sop);
    }

    if (sqlite3_exec(h, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
      sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);
      failed++;
      continue;
    }

    tot.stops += fs.stops; tot.routes += fs.routes; tot.trips += fs.trips;
    tot.stop_times += fs.stop_times; tot.shapes += fs.shapes;
    tot.syn_shapes += fs.syn_shapes; tot.skipped_trips += fs.skipped_trips;
    done++;
    if (done % 25 == 0)
      fprintf(stderr, "[gtfs-jp] %ld feeds: %d stops, %d trips, %d stop_times\n",
              done, tot.stops, tot.trips, tot.stop_times);
  }

  sqlite3_finalize(sfeed);
  sqlite3_finalize(sop);
  fin_all(&s);
  cJSON_Delete(wrap);

  fprintf(stderr,
    "[gtfs-jp] ingested %ld feeds (%ld failed): %d stops, %d routes, %d trips, "
    "%d stop_times, %d shape pts (%d synthetic shapes), %d trips skipped\n",
    done, failed, tot.stops, tot.routes, tot.trips, tot.stop_times,
    tot.shapes, tot.syn_shapes, tot.skipped_trips);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[gtfs-jp] emitted %d stop features\n", n);
  return (done > 0) ? 0 : -1;
}

static const source_def gtfs_jp_def = {
  .id = "gtfs-jp", .collector = "transport",
  .name = "GTFS-JP Nationwide Bus", .name_ja = "GTFS-JP Nationwide Bus",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(gtfs_jp_def)
