/* core/isochrone.c — GTFS travel-time reachability (roadmap item 32).
 *
 * The contract, the honesty argument and the caching story live in
 * isochrone.h. What follows are the implementation notes that are not
 * obvious from the code:
 *
 * 1. A stop is identified by (org_id, feed_id, stop_id), never by stop_id
 *    alone. gtfs_stops' primary key is the triple and stop_id "1" exists in
 *    dozens of feeds; keying on stop_id would teleport a rider between
 *    operators. Ride edges therefore also bind org_id and feed_id, and only
 *    WALKING crosses operators — which is what actually happens in reality.
 * 2. Expansion is RAPTOR-shaped, not a per-node priority queue: round r rides
 *    every stop improved in round r-1, then relaxes walking transfers. With a
 *    bounded number of ride legs this settles in 3-4 rounds and issues one
 *    prepared-statement execution per frontier stop instead of one per edge.
 * 3. The walking relaxation uses an in-memory uniform grid over the loaded
 *    stops (cell = transfer radius), so a transfer lookup touches 9 cells
 *    instead of the whole array. The naive O(n^2) version dominated runtime
 *    in the prototype.
 * 4. Marching squares references crossing points by EDGE ID, not by
 *    coordinate. Two cells sharing an edge then produce the byte-identical
 *    endpoint, so ring chaining is exact integer matching and needs no
 *    floating-point tolerance.
 */
#include "isochrone.h"
#include "collcache.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── tunables ──────────────────────────────────────────────────────────── */
#define ISO_WALK_KMH_DEF   4.8
#define ISO_MAX_WALK_DEF   800.0     /* access + transfer cap, metres        */
#define ISO_XFER_RADIUS    400.0     /* stop→stop walking transfer radius    */
#define ISO_EGRESS_CAP_M   5000.0    /* ceiling on the final walk-off leg    */
#define ISO_MAX_MIN_DEF    30
#define ISO_MAX_MIN_CEIL   90
#define ISO_MAX_TRANSFERS  2
#define ISO_MAX_BANDS      6
#define ISO_MAX_QUERIES    4000      /* timetable executions per request     */
#define ISO_MAX_STOPS      60000     /* settled-node ceiling                 */
#define ISO_GRID_MAX       420       /* grid cells per side                  */
#define ISO_GRID_MIN_M     100.0     /* finest cell we will sample           */
#define ISO_CACHE_TTL_MS   21600000LL /* 6 h — a service day does not move   */
/* A transit vehicle we are willing to believe in, used ONLY to size the
 * gtfs_stops prefetch bbox. Too small and we would clip real reachability;
 * too large and we scan the whole country for a 5-minute query. */
#define ISO_MAX_KMH        140.0

#define ISO_FIELD_INF      1.0e9f

/* ── small helpers ─────────────────────────────────────────────────────── */
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
/* Same shape as aoiapi.c's qget: last-wins is not implemented, first match
 * wins, which is what every other module here does. */
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
static double hav_m(double la1, double lo1, double la2, double lo2) {
  const double R = 6371000.0, D = M_PI / 180.0;
  double p1 = la1 * D, p2 = la2 * D;
  double dp = (la2 - la1) * D, dl = (lo2 - lo1) * D;
  double a = sin(dp / 2) * sin(dp / 2) +
             cos(p1) * cos(p2) * sin(dl / 2) * sin(dl / 2);
  if (a > 1.0) a = 1.0;
  return 2.0 * R * asin(sqrt(a));
}
static long long now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long) t.tv_sec * 1000LL + t.tv_nsec / 1000000LL;
}

/* ── stop table ────────────────────────────────────────────────────────── */
typedef struct {
  char  *key;          /* "org\x1ffeed\x1fstop" — owns the storage          */
  /* Views into `key`. org and feed are NOT NUL-terminated (0x1F follows), so
   * every use must carry its length: binding them with sqlite3_bind_text's
   * -1 sentinel reads to the end of the whole composite key and silently
   * matches nothing. That cost one round of "why are there no ride edges". */
  const char *org, *feed, *stop;
  int olen, flen;
  double lat, lon;
  double best;         /* best arrival, seconds since service-day midnight  */
  int    improved;     /* set when this round lowered `best`                */
} iso_stop;

typedef struct {
  iso_stop *v;
  int n, cap;
  int *tab;            /* open-addressed key → index+1, 0 = empty           */
  int tabn;
} iso_set;

static unsigned fnv(const char *s) {
  unsigned h = 2166136261u;
  for (; *s; s++) { h ^= (unsigned char) *s; h *= 16777619u; }
  return h;
}
static int set_find(const iso_set *S, const char *key) {
  if (!S->tab) return -1;
  unsigned h = fnv(key) % (unsigned) S->tabn;
  for (int i = 0; i < S->tabn; i++) {
    int slot = S->tab[(h + (unsigned) i) % (unsigned) S->tabn];
    if (!slot) return -1;
    if (strcmp(S->v[slot - 1].key, key) == 0) return slot - 1;
  }
  return -1;
}
static void set_index(iso_set *S, int idx) {
  unsigned h = fnv(S->v[idx].key) % (unsigned) S->tabn;
  for (int i = 0; i < S->tabn; i++) {
    unsigned p = (h + (unsigned) i) % (unsigned) S->tabn;
    if (!S->tab[p]) { S->tab[p] = idx + 1; return; }
  }
}
/* Adds a stop, taking ownership of nothing: `key` is copied. Returns index or
 * -1 on OOM / capacity. */
static int set_add(iso_set *S, const char *org, const char *feed,
                   const char *stop, double lat, double lon) {
  if (S->n >= S->cap) return -1;
  size_t lo = strlen(org), lf = strlen(feed), ls = strlen(stop);
  char *k = malloc(lo + lf + ls + 3);
  if (!k) return -1;
  memcpy(k, org, lo);            k[lo] = '\x1f';
  memcpy(k + lo + 1, feed, lf);  k[lo + 1 + lf] = '\x1f';
  memcpy(k + lo + lf + 2, stop, ls + 1);
  iso_stop *e = &S->v[S->n];
  e->key = k;
  e->org = k; e->feed = k + lo + 1; e->stop = k + lo + lf + 2;
  e->olen = (int) lo; e->flen = (int) lf;
  e->lat = lat; e->lon = lon;
  e->best = INFINITY; e->improved = 0;
  set_index(S, S->n);
  return S->n++;
}
static void set_free(iso_set *S) {
  for (int i = 0; i < S->n; i++) free(S->v[i].key);
  free(S->v); free(S->tab);
  S->v = NULL; S->tab = NULL; S->n = S->cap = S->tabn = 0;
}

/* ── uniform spatial grid over the loaded stops (walking transfers) ─────── */
typedef struct {
  int nx, ny;
  double lat0, lon0, dlat, dlon;
  int *head;      /* cell → first stop index + 1 */
  int *next;      /* stop index → next stop index + 1 */
} iso_grid;

static void grid_free(iso_grid *g) { free(g->head); free(g->next); g->head = NULL; g->next = NULL; }

static int grid_build(iso_grid *g, const iso_set *S, double cell_m) {
  memset(g, 0, sizeof *g);
  if (S->n == 0) return 0;
  double mnla = 90, mxla = -90, mnlo = 180, mxlo = -180;
  for (int i = 0; i < S->n; i++) {
    if (S->v[i].lat < mnla) mnla = S->v[i].lat;
    if (S->v[i].lat > mxla) mxla = S->v[i].lat;
    if (S->v[i].lon < mnlo) mnlo = S->v[i].lon;
    if (S->v[i].lon > mxlo) mxlo = S->v[i].lon;
  }
  double clat = cos(((mnla + mxla) / 2) * M_PI / 180.0);
  if (clat < 0.05) clat = 0.05;
  g->dlat = cell_m / 111320.0;
  g->dlon = cell_m / (111320.0 * clat);
  g->lat0 = mnla; g->lon0 = mnlo;
  g->nx = (int) ((mxlo - mnlo) / g->dlon) + 2;
  g->ny = (int) ((mxla - mnla) / g->dlat) + 2;
  if (g->nx < 1) g->nx = 1;
  if (g->ny < 1) g->ny = 1;
  /* A country-wide bbox at 400 m cells is ~5M cells; that is 20 MB of int and
   * still cheaper than the O(n^2) scan it replaces, but cap it anyway. */
  if ((long long) g->nx * g->ny > 8000000LL) return -1;
  g->head = calloc((size_t) g->nx * (size_t) g->ny, sizeof(int));
  g->next = calloc((size_t) S->n, sizeof(int));
  if (!g->head || !g->next) { grid_free(g); return -1; }
  for (int i = 0; i < S->n; i++) {
    int cx = (int) ((S->v[i].lon - g->lon0) / g->dlon);
    int cy = (int) ((S->v[i].lat - g->lat0) / g->dlat);
    if (cx < 0) cx = 0;
    if (cx >= g->nx) cx = g->nx - 1;
    if (cy < 0) cy = 0;
    if (cy >= g->ny) cy = g->ny - 1;
    int c = cy * g->nx + cx;
    g->next[i] = g->head[c];
    g->head[c] = i + 1;
  }
  return 0;
}

/* ── departure parsing ─────────────────────────────────────────────────── */
/* Fills service_date "YYYYMMDD", weekday column name, and seconds-since-
 * midnight. Returns 0 on success. See isochrone.h on the JST rule. */
static int parse_depart(const char *s, char date_out[32], const char **wd_out,
                        int *sec_out, char iso_out[64]) {
  static const char *WD[7] = { "sun", "mon", "tue", "wed", "thu", "fri", "sat" };
  struct tm tm;
  memset(&tm, 0, sizeof tm);
  time_t t;

  if (!s || !*s) {
    t = time(NULL) + 9 * 3600;                       /* UTC → JST           */
  } else {
    int Y = 0, M = 0, D = 0, h = 0, mi = 0, se = 0;
    int n = sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &Y, &M, &D, &h, &mi, &se);
    if (n < 5) n = sscanf(s, "%4d-%2d-%2d %2d:%2d:%2d", &Y, &M, &D, &h, &mi, &se);
    if (n < 5) return -1;
    memset(&tm, 0, sizeof tm);
    tm.tm_year = Y - 1900; tm.tm_mon = M - 1; tm.tm_mday = D;
    tm.tm_hour = h; tm.tm_min = mi; tm.tm_sec = se;
    t = timegm(&tm);                                  /* treat as wall clock */
    if (t == (time_t) -1) return -1;
    /* An explicit zone means the caller gave us absolute time: normalise to
     * JST, which is the clock every feed in this corpus is written in. */
    size_t L = strlen(s);
    if (L && (s[L - 1] == 'Z' || s[L - 1] == 'z')) {
      t += 9 * 3600;
    } else {
      const char *pm = NULL;
      for (size_t i = 11; i < L; i++)
        if (s[i] == '+' || s[i] == '-') { pm = s + i; break; }
      if (pm) {
        int oh = 0, om = 0;
        if (sscanf(pm + 1, "%2d:%2d", &oh, &om) >= 1) {
          int off = oh * 3600 + om * 60;
          if (*pm == '+') t -= off; else t += off;
          t += 9 * 3600;
        }
      }
    }
  }
  struct tm g;
  gmtime_r(&t, &g);
  snprintf(date_out, 32, "%04d%02d%02d", g.tm_year + 1900, g.tm_mon + 1, g.tm_mday);
  *wd_out = WD[g.tm_wday % 7];
  *sec_out = g.tm_hour * 3600 + g.tm_min * 60 + g.tm_sec;
  snprintf(iso_out, 64, "%04d-%02d-%02dT%02d:%02d:%02d+09:00",
           g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
           g.tm_hour, g.tm_min, g.tm_sec);
  return 0;
}

/* ── marching squares ──────────────────────────────────────────────────── */
typedef struct { int a, b; } iso_seg;      /* two edge ids                   */

typedef struct { double *xy; int n, cap; } iso_ring;

static void ring_push(iso_ring *r, double x, double y) {
  if (r->n + 1 > r->cap) {
    int nc = r->cap ? r->cap * 2 : 64;
    double *nv = realloc(r->xy, (size_t) nc * 2 * sizeof(double));
    if (!nv) return;
    r->xy = nv; r->cap = nc;
  }
  r->xy[r->n * 2] = x; r->xy[r->n * 2 + 1] = y; r->n++;
}
static double ring_area(const iso_ring *r) {          /* signed, in deg^2    */
  double a = 0;
  for (int i = 0, j = r->n - 1; i < r->n; j = i++)
    a += (r->xy[j * 2] * r->xy[i * 2 + 1]) - (r->xy[i * 2] * r->xy[j * 2 + 1]);
  return a / 2.0;
}
static int ring_contains(const iso_ring *r, double x, double y) {
  int in = 0;
  for (int i = 0, j = r->n - 1; i < r->n; j = i++) {
    double xi = r->xy[i * 2], yi = r->xy[i * 2 + 1];
    double xj = r->xy[j * 2], yj = r->xy[j * 2 + 1];
    if (((yi > y) != (yj > y)) &&
        (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) in = !in;
  }
  return in;
}

/* Grid geometry shared by the contour pass. */
typedef struct {
  int nx, ny;                  /* sample points per axis                     */
  double lon0, lat0, dlon, dlat;
  float *f;                    /* nx*ny reach field, minutes                 */
} iso_field;

/* Linear crossing position along an edge between two sample values. Where one
 * end is the unreachable sentinel there is no gradient to interpolate, so the
 * midpoint is the only defensible answer. */
static double cross_t(float v0, float v1, double thr) {
  if (v0 >= ISO_FIELD_INF * 0.5f || v1 >= ISO_FIELD_INF * 0.5f) return 0.5;
  double d = (double) v1 - (double) v0;
  if (fabs(d) < 1e-9) return 0.5;
  double t = (thr - (double) v0) / d;
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  return t;
}

/* Builds closed rings of the `field <= thr` region. Returns ring count and
 * fills *out (caller frees each ring's xy and the array). */
static int contour(const iso_field *F, double thr, iso_ring **out) {
  int nx = F->nx, ny = F->ny;
  if (nx < 2 || ny < 2) { *out = NULL; return 0; }
  long nh = (long) (nx - 1) * ny;          /* horizontal edges               */
  long nv = (long) nx * (ny - 1);          /* vertical edges                 */
  double *ex = malloc((size_t) (nh + nv) * sizeof(double));
  double *ey = malloc((size_t) (nh + nv) * sizeof(double));
  char   *eu = calloc((size_t) (nh + nv), 1);          /* edge has a crossing */
  if (!ex || !ey || !eu) { free(ex); free(ey); free(eu); *out = NULL; return 0; }

  #define IN(i,j)  (F->f[(long)(j) * nx + (i)] <= (float) thr)
  #define HID(i,j) ((long)(j) * (nx - 1) + (i))
  #define VID(i,j) (nh + (long)(j) * nx + (i))

  for (int j = 0; j < ny; j++)
    for (int i = 0; i < nx - 1; i++) {
      if (IN(i, j) == IN(i + 1, j)) continue;
      double t = cross_t(F->f[(long) j * nx + i], F->f[(long) j * nx + i + 1], thr);
      long id = HID(i, j);
      ex[id] = F->lon0 + (i + t) * F->dlon;
      ey[id] = F->lat0 + j * F->dlat;
      eu[id] = 1;
    }
  for (int j = 0; j < ny - 1; j++)
    for (int i = 0; i < nx; i++) {
      if (IN(i, j) == IN(i, j + 1)) continue;
      double t = cross_t(F->f[(long) j * nx + i], F->f[(long) (j + 1) * nx + i], thr);
      long id = VID(i, j);
      ex[id] = F->lon0 + i * F->dlon;
      ey[id] = F->lat0 + (j + t) * F->dlat;
      eu[id] = 1;
    }

  int scap = 4096, sn = 0;
  iso_seg *segs = malloc((size_t) scap * sizeof(iso_seg));
  if (!segs) { free(ex); free(ey); free(eu); *out = NULL; return 0; }
  for (int j = 0; j < ny - 1; j++)
    for (int i = 0; i < nx - 1; i++) {
      int bl = IN(i, j), br = IN(i + 1, j), tr = IN(i + 1, j + 1), tl = IN(i, j + 1);
      int cs = bl | (br << 1) | (tr << 2) | (tl << 3);
      if (cs == 0 || cs == 15) continue;
      long B = HID(i, j), T = HID(i, j + 1), L = VID(i, j), R = VID(i + 1, j);
      long p[4]; int np = 0;
      switch (cs) {
        case 1: case 14: p[np++] = L; p[np++] = B; break;
        case 2: case 13: p[np++] = B; p[np++] = R; break;
        case 3: case 12: p[np++] = L; p[np++] = R; break;
        case 4: case 11: p[np++] = R; p[np++] = T; break;
        case 6: case  9: p[np++] = B; p[np++] = T; break;
        case 7: case  8: p[np++] = L; p[np++] = T; break;
        /* Saddles: connect so the INSIDE stays one region. Either resolution
         * is topologically legal; picking consistently avoids a contour that
         * pinches itself into two rings at every diagonal. */
        case 5:  p[np++] = L; p[np++] = T; p[np++] = B; p[np++] = R; break;
        case 10: p[np++] = L; p[np++] = B; p[np++] = T; p[np++] = R; break;
        default: break;
      }
      for (int k = 0; k + 1 < np; k += 2) {
        if (!eu[p[k]] || !eu[p[k + 1]]) continue;
        if (sn >= scap) {
          int nc = scap * 2;
          iso_seg *nv2 = realloc(segs, (size_t) nc * sizeof(iso_seg));
          if (!nv2) goto chain;
          segs = nv2; scap = nc;
        }
        segs[sn].a = (int) p[k]; segs[sn].b = (int) p[k + 1]; sn++;
      }
    }

chain:;
  /* edge id → up to two incident segments */
  long ne = nh + nv;
  int *inc = malloc((size_t) ne * 2 * sizeof(int));
  if (!inc) { free(segs); free(ex); free(ey); free(eu); *out = NULL; return 0; }
  for (long i = 0; i < ne * 2; i++) inc[i] = -1;
  for (int s = 0; s < sn; s++) {
    if (inc[(long) segs[s].a * 2] < 0)      inc[(long) segs[s].a * 2] = s;
    else if (inc[(long) segs[s].a * 2 + 1] < 0) inc[(long) segs[s].a * 2 + 1] = s;
    if (inc[(long) segs[s].b * 2] < 0)      inc[(long) segs[s].b * 2] = s;
    else if (inc[(long) segs[s].b * 2 + 1] < 0) inc[(long) segs[s].b * 2 + 1] = s;
  }
  char *used = calloc((size_t) (sn ? sn : 1), 1);
  if (!used) { free(inc); free(segs); free(ex); free(ey); free(eu); *out = NULL; return 0; }

  int rcap = 32, rn = 0;
  iso_ring *rings = calloc((size_t) rcap, sizeof(iso_ring));
  if (!rings) { free(used); free(inc); free(segs); free(ex); free(ey); free(eu);
                *out = NULL; return 0; }

  for (int s0 = 0; s0 < sn; s0++) {
    if (used[s0]) continue;
    iso_ring r; memset(&r, 0, sizeof r);
    int cur = s0, entry = segs[s0].a;
    for (;;) {
      used[cur] = 1;
      int exit_e = (segs[cur].a == entry) ? segs[cur].b : segs[cur].a;
      ring_push(&r, ex[entry], ey[entry]);
      int a = inc[(long) exit_e * 2], b = inc[(long) exit_e * 2 + 1];
      int nxt = (a >= 0 && !used[a]) ? a : ((b >= 0 && !used[b]) ? b : -1);
      if (nxt < 0) { ring_push(&r, ex[exit_e], ey[exit_e]); break; }
      cur = nxt; entry = exit_e;
    }
    if (r.n >= 4) {
      if (rn >= rcap) {
        int nc = rcap * 2;
        iso_ring *nv2 = realloc(rings, (size_t) nc * sizeof(iso_ring));
        if (!nv2) { free(r.xy); break; }
        memset(nv2 + rcap, 0, (size_t) (nc - rcap) * sizeof(iso_ring));
        rings = nv2; rcap = nc;
      }
      rings[rn++] = r;
    } else free(r.xy);
  }

  free(used); free(inc); free(segs); free(ex); free(ey); free(eu);
  #undef IN
  #undef HID
  #undef VID
  *out = rings;
  return rn;
}

/* Drops points that add no shape: a vertex whose perpendicular offset from
 * the chord of its neighbours is below `tol` degrees. One pass is enough to
 * halve a marching-squares ring without visibly moving it. */
static void ring_simplify(iso_ring *r, double tol) {
  if (r->n < 5) return;
  int w = 1;
  for (int i = 1; i < r->n - 1; i++) {
    double x0 = r->xy[(w - 1) * 2], y0 = r->xy[(w - 1) * 2 + 1];
    double x1 = r->xy[i * 2],       y1 = r->xy[i * 2 + 1];
    double x2 = r->xy[(i + 1) * 2], y2 = r->xy[(i + 1) * 2 + 1];
    double dx = x2 - x0, dy = y2 - y0;
    double L = sqrt(dx * dx + dy * dy);
    double d = L < 1e-12 ? 0 : fabs(dx * (y0 - y1) - (x0 - x1) * dy) / L;
    if (d < tol) continue;
    r->xy[w * 2] = x1; r->xy[w * 2 + 1] = y1; w++;
  }
  r->xy[w * 2] = r->xy[(r->n - 1) * 2];
  r->xy[w * 2 + 1] = r->xy[(r->n - 1) * 2 + 1];
  r->n = w + 1;
}

/* ── main entry ────────────────────────────────────────────────────────── */
char *isochrone_run(db_handle *db, const char *qs, int *status) {
  long long t_start = now_ms();
  *status = 200;

  char v[128];
  if (!qget(qs, "lat", v, sizeof v) || !*v) return errj(status, 400, "lat_required");
  char *e1 = NULL; double olat = strtod(v, &e1);
  if (e1 == v || !(olat >= -90 && olat <= 90)) return errj(status, 400, "lat_invalid");
  if (!qget(qs, "lon", v, sizeof v) || !*v) return errj(status, 400, "lon_required");
  char *e2 = NULL; double olon = strtod(v, &e2);
  if (e2 == v || !(olon >= -180 && olon <= 180)) return errj(status, 400, "lon_invalid");

  int max_min = ISO_MAX_MIN_DEF;
  if (qget(qs, "max_min", v, sizeof v) && *v) max_min = atoi(v);
  if (max_min < 5) max_min = 5;
  if (max_min > ISO_MAX_MIN_CEIL) max_min = ISO_MAX_MIN_CEIL;

  double max_walk = ISO_MAX_WALK_DEF;
  if (qget(qs, "max_walk_m", v, sizeof v) && *v) max_walk = strtod(v, NULL);
  if (max_walk < 100) max_walk = 100;
  if (max_walk > 2000) max_walk = 2000;

  double walk_kmh = ISO_WALK_KMH_DEF;
  if (qget(qs, "walk_kmh", v, sizeof v) && *v) walk_kmh = strtod(v, NULL);
  if (!(walk_kmh >= 2.0)) walk_kmh = 2.0;
  if (walk_kmh > 7.0) walk_kmh = 7.0;
  const double walk_mps = walk_kmh * 1000.0 / 3600.0;

  int max_xfer = ISO_MAX_TRANSFERS;
  if (qget(qs, "max_transfers", v, sizeof v) && *v) max_xfer = atoi(v);
  if (max_xfer < 0) max_xfer = 0;
  if (max_xfer > 4) max_xfer = 4;

  int bands[ISO_MAX_BANDS], nband = 0;
  if (qget(qs, "bands", v, sizeof v) && *v) {
    char *save = NULL;
    for (char *tk = strtok_r(v, ",", &save); tk && nband < ISO_MAX_BANDS;
         tk = strtok_r(NULL, ",", &save)) {
      int b = atoi(tk);
      if (b > 0 && b <= max_min) bands[nband++] = b;
    }
  }
  if (!nband) {
    bands[nband++] = max_min / 3 > 0 ? max_min / 3 : 1;
    bands[nband++] = (max_min * 2) / 3 > bands[0] ? (max_min * 2) / 3 : bands[0] + 1;
    bands[nband++] = max_min;
  }
  for (int i = 0; i < nband; i++)                    /* sort ascending, tiny n */
    for (int j = i + 1; j < nband; j++)
      if (bands[j] < bands[i]) { int t = bands[i]; bands[i] = bands[j]; bands[j] = t; }

  char depv[64] = {0};
  qget(qs, "depart_at", depv, sizeof depv);
  char sdate[32]; const char *wd; int dep_sec; char dep_iso[64];
  if (parse_depart(depv[0] ? depv : NULL, sdate, &wd, &dep_sec, dep_iso) != 0)
    return errj(status, 400, "depart_at_invalid");

  /* ── cache lookup ─────────────────────────────────────────────────────
   * Origin is rounded to 4 decimals (~11 m) and the departure to a 15-minute
   * bucket: finer than that and the cache would never hit, coarser and it
   * would answer a different question than the one asked. */
  char ck[256], bandstr[64] = {0};
  { int o = 0;
    for (int i = 0; i < nband && o < (int) sizeof bandstr - 8; i++)
      o += snprintf(bandstr + o, sizeof bandstr - (size_t) o, "%s%d", i ? "-" : "", bands[i]); }
  snprintf(ck, sizeof ck, "iso:%.4f:%.4f:%d:%.0f:%.1f:%d:%s:%s:%d",
           olat, olon, max_min, max_walk, walk_kmh, max_xfer, bandstr, sdate,
           dep_sec / 900);
  { long long age = 0;
    char *hit = collcache_get(db, ck, &age);
    if (hit) {
      /* The stored body is a complete FeatureCollection, but it was printed
       * with meta.cached=false and meta.elapsed_ms from the run that built
       * it. Returning it unaltered would tell the caller a cached answer was
       * freshly computed. Flip the flag in place — "true " is the same width
       * as "false", and trailing space before the comma is legal JSON, so no
       * reparse of an 18 KB document is needed. elapsed_ms deliberately keeps
       * the ORIGINAL cost: that is the number an operator wants when asking
       * what a miss costs. */
      char *p = strstr(hit, "\"cached\":false");
      if (p) memcpy(p + 9, "true ", 5);
      return hit;
    }
  }

  /* ── load candidate stops ────────────────────────────────────────────── */
  double reach_m = (ISO_MAX_KMH * 1000.0 / 60.0) * max_min + max_walk + ISO_EGRESS_CAP_M;
  double clat = cos(olat * M_PI / 180.0);
  if (clat < 0.05) clat = 0.05;
  double dlat_bb = reach_m / 111320.0, dlon_bb = reach_m / (111320.0 * clat);

  iso_set S; memset(&S, 0, sizeof S);
  S.cap = ISO_MAX_STOPS;
  S.v = calloc((size_t) S.cap, sizeof(iso_stop));
  S.tabn = S.cap * 2;
  S.tab = calloc((size_t) S.tabn, sizeof(int));
  if (!S.v || !S.tab) { set_free(&S); return errj(status, 500, "oom"); }

  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT org_id,feed_id,stop_id,lat,lon FROM gtfs_stops "
        "WHERE lat BETWEEN ?1 AND ?2 AND lon BETWEEN ?3 AND ?4 "
        "AND lat IS NOT NULL AND lon IS NOT NULL", -1, &st, NULL) != SQLITE_OK) {
    set_free(&S);
    return errj(status, 500, "gtfs_unavailable");
  }
  sqlite3_bind_double(st, 1, olat - dlat_bb);
  sqlite3_bind_double(st, 2, olat + dlat_bb);
  sqlite3_bind_double(st, 3, olon - dlon_bb);
  sqlite3_bind_double(st, 4, olon + dlon_bb);
  while (sqlite3_step(st) == SQLITE_ROW) {
    const char *o = (const char *) sqlite3_column_text(st, 0);
    const char *f = (const char *) sqlite3_column_text(st, 1);
    const char *sp = (const char *) sqlite3_column_text(st, 2);
    if (!o || !f || !sp) continue;
    if (set_add(&S, o, f, sp, sqlite3_column_double(st, 3),
                sqlite3_column_double(st, 4)) < 0) break;
  }
  sqlite3_finalize(st);

  /* No timetable in this database is a real answer, not an error: say so
   * rather than returning an empty shape that looks like "nothing nearby". */
  if (S.n == 0) {
    set_free(&S);
    cJSON *fc = cJSON_CreateObject();
    cJSON_AddStringToObject(fc, "type", "FeatureCollection");
    cJSON_AddItemToObject(fc, "features", cJSON_CreateArray());
    cJSON *m = cJSON_CreateObject();
    cJSON_AddStringToObject(m, "note", "no gtfs_stops within reach of this origin");
    cJSON_AddNumberToObject(m, "stops_reached", 0);
    cJSON_AddItemToObject(fc, "meta", m);
    char *j = cJSON_PrintUnformatted(fc);
    cJSON_Delete(fc);
    return j;
  }

  iso_grid G;
  if (grid_build(&G, &S, ISO_XFER_RADIUS) != 0) {
    set_free(&S);
    return errj(status, 500, "stop_grid_too_large");
  }

  /* ── seed: walk from the origin ──────────────────────────────────────── */
  const int horizon = dep_sec + max_min * 60;
  int reached = 0;
  for (int i = 0; i < S.n; i++) {
    double d = hav_m(olat, olon, S.v[i].lat, S.v[i].lon);
    if (d > max_walk) continue;
    double t = dep_sec + d / walk_mps;
    if (t > horizon) continue;
    S.v[i].best = t; S.v[i].improved = 1; reached++;
  }

  /* ── prepared ride query ─────────────────────────────────────────────── */
  char sql[1400];
  const char *core_sql =
    "SELECT st2.stop_id, MIN(st2.arrival_sec) "
    "FROM gtfs_stop_times st1 %s "
    "JOIN gtfs_trips t ON t.org_id=st1.org_id AND t.feed_id=st1.feed_id "
    "                 AND t.trip_id=st1.trip_id "
    "JOIN gtfs_calendar cal ON cal.org_id=t.org_id AND cal.feed_id=t.feed_id "
    "                      AND cal.service_id=t.service_id "
    "JOIN gtfs_stop_times st2 ON st2.org_id=st1.org_id AND st2.feed_id=st1.feed_id "
    "                        AND st2.trip_id=st1.trip_id "
    "                        AND st2.stop_sequence>st1.stop_sequence "
    "WHERE st1.stop_id=?1 AND st1.org_id=?2 AND st1.feed_id=?3 "
    "  AND st1.departure_sec>=?4 AND st1.departure_sec<=?5 "
    "  AND st2.arrival_sec IS NOT NULL AND st2.arrival_sec<=?5 "
    "  AND cal.%s=1 "
    "  AND (cal.start_date IS NULL OR cal.start_date<=?6) "
    "  AND (cal.end_date   IS NULL OR cal.end_date  >=?6) "
    "GROUP BY st2.stop_id";
  /* INDEXED BY pins the stop_id index. Without it SQLite prefers
   * idx_gtfs_stop_times_trip_time and scans every stop_time in the FEED,
   * which measured ~5x slower per hop. Fall back if the index is absent. */
  sqlite3_stmt *ride = NULL;
  snprintf(sql, sizeof sql, core_sql, "INDEXED BY idx_gtfs_stop_times_stop", wd);
  if (sqlite3_prepare_v2(db->h, sql, -1, &ride, NULL) != SQLITE_OK) {
    ride = NULL;
    snprintf(sql, sizeof sql, core_sql, "", wd);
    if (sqlite3_prepare_v2(db->h, sql, -1, &ride, NULL) != SQLITE_OK) {
      grid_free(&G); set_free(&S);
      return errj(status, 500, "gtfs_timetable_unavailable");
    }
  }

  /* Separated from `reached` deliberately: a stop you can WALK to is reached,
   * but if no service runs the polygon is walking reach and nothing more. One
   * number cannot say both, and conflating them is how "we have transit data"
   * gets claimed for a 3 a.m. Sunday. */
  int queries = 0, rounds = 0, truncated = 0, transit_gain = 0;
  int *frontier = malloc((size_t) S.n * sizeof(int));
  int *next_fr  = malloc((size_t) S.n * sizeof(int));
  if (!frontier || !next_fr) {
    free(frontier); free(next_fr);
    sqlite3_finalize(ride); grid_free(&G); set_free(&S);
    return errj(status, 500, "oom");
  }
  int nfr = 0;
  for (int i = 0; i < S.n; i++) if (S.v[i].improved) frontier[nfr++] = i;

  /* Walking transfers off the seed set, before the first ride: an origin
   * 300 m from stop A and 900 m from stop B still reaches B by walking A→B. */
  char nodekey[512];
  for (int pass = 0; pass <= max_xfer && nfr > 0; pass++) {
    rounds++;
    int nnext = 0;
    for (int fi = 0; fi < nfr; fi++) {
      int si = frontier[fi];
      S.v[si].improved = 0;
      if (S.v[si].best >= horizon) continue;
      if (queries >= ISO_MAX_QUERIES) { truncated = 1; break; }
      queries++;
      sqlite3_reset(ride);
      sqlite3_bind_text(ride, 1, S.v[si].stop, -1, SQLITE_STATIC);
      sqlite3_bind_text(ride, 2, S.v[si].org,  S.v[si].olen, SQLITE_STATIC);
      sqlite3_bind_text(ride, 3, S.v[si].feed, S.v[si].flen, SQLITE_STATIC);
      sqlite3_bind_int (ride, 4, (int) S.v[si].best);
      sqlite3_bind_int (ride, 5, horizon);
      sqlite3_bind_text(ride, 6, sdate, -1, SQLITE_STATIC);
      while (sqlite3_step(ride) == SQLITE_ROW) {
        const char *sid = (const char *) sqlite3_column_text(ride, 0);
        if (!sid) continue;
        double arr = sqlite3_column_double(ride, 1);
        /* Same (org, feed), new stop — precision-bounded because org/feed are
         * unterminated views into the composite key. */
        snprintf(nodekey, sizeof nodekey, "%.*s\x1f%.*s\x1f%s",
                 S.v[si].olen, S.v[si].org, S.v[si].flen, S.v[si].feed, sid);
        int ti = set_find(&S, nodekey);
        if (ti < 0) continue;               /* stop outside the loaded bbox  */
        if (arr < S.v[ti].best) {
          if (!isfinite(S.v[ti].best)) { reached++; transit_gain++; }
          S.v[ti].best = arr;
          if (!S.v[ti].improved) { S.v[ti].improved = 1; next_fr[nnext++] = ti; }
        }
      }
    }
    /* Walking transfers from everything this round improved. */
    int walk_start = 0, walk_end = nnext;
    for (int wi = walk_start; wi < walk_end; wi++) {
      int si = next_fr[wi];
      double la = S.v[si].lat, lo = S.v[si].lon;
      int cx = (int) ((lo - G.lon0) / G.dlon), cy = (int) ((la - G.lat0) / G.dlat);
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          int x = cx + dx, y = cy + dy;
          if (x < 0 || y < 0 || x >= G.nx || y >= G.ny) continue;
          for (int k = G.head[y * G.nx + x]; k; k = G.next[k - 1]) {
            int tj = k - 1;
            if (tj == si) continue;
            double d = hav_m(la, lo, S.v[tj].lat, S.v[tj].lon);
            if (d > ISO_XFER_RADIUS) continue;
            double t = S.v[si].best + d / walk_mps;
            if (t > horizon || t >= S.v[tj].best) continue;
            if (!isfinite(S.v[tj].best)) reached++;
            S.v[tj].best = t;
            if (!S.v[tj].improved && nnext < S.n) {
              S.v[tj].improved = 1; next_fr[nnext++] = tj;
            }
          }
        }
    }
    memcpy(frontier, next_fr, (size_t) nnext * sizeof(int));
    nfr = nnext;
    if (truncated) break;
  }
  sqlite3_finalize(ride);
  grid_free(&G);

  /* ── reach field ─────────────────────────────────────────────────────── */
  double egress_max = walk_mps * (double) max_min * 60.0;
  if (egress_max > ISO_EGRESS_CAP_M) egress_max = ISO_EGRESS_CAP_M;

  double mnla = olat, mxla = olat, mnlo = olon, mxlo = olon;
  for (int i = 0; i < S.n; i++) {
    if (!isfinite(S.v[i].best)) continue;
    double rem = (horizon - S.v[i].best) * walk_mps;
    if (rem <= 0) continue;
    if (rem > egress_max) rem = egress_max;
    double dla = rem / 111320.0, dlo = rem / (111320.0 * clat);
    if (S.v[i].lat - dla < mnla) mnla = S.v[i].lat - dla;
    if (S.v[i].lat + dla > mxla) mxla = S.v[i].lat + dla;
    if (S.v[i].lon - dlo < mnlo) mnlo = S.v[i].lon - dlo;
    if (S.v[i].lon + dlo > mxlo) mxlo = S.v[i].lon + dlo;
  }
  { double dla = egress_max / 111320.0, dlo = egress_max / (111320.0 * clat);
    if (olat - dla < mnla) mnla = olat - dla;
    if (olat + dla > mxla) mxla = olat + dla;
    if (olon - dlo < mnlo) mnlo = olon - dlo;
    if (olon + dlo > mxlo) mxlo = olon + dlo; }

  double span_m = hav_m(mnla, mnlo, mxla, mxlo);
  double cell_m = span_m / (double) ISO_GRID_MAX;
  if (cell_m < ISO_GRID_MIN_M) cell_m = ISO_GRID_MIN_M;

  iso_field F;
  F.dlat = cell_m / 111320.0;
  F.dlon = cell_m / (111320.0 * clat);
  F.lat0 = mnla; F.lon0 = mnlo;
  F.nx = (int) ((mxlo - mnlo) / F.dlon) + 2;
  F.ny = (int) ((mxla - mnla) / F.dlat) + 2;
  if (F.nx > ISO_GRID_MAX + 2) F.nx = ISO_GRID_MAX + 2;
  if (F.ny > ISO_GRID_MAX + 2) F.ny = ISO_GRID_MAX + 2;
  if (F.nx < 2) F.nx = 2;
  if (F.ny < 2) F.ny = 2;
  F.f = malloc((size_t) F.nx * (size_t) F.ny * sizeof(float));
  if (!F.f) {
    free(frontier); free(next_fr); set_free(&S);
    return errj(status, 500, "oom");
  }
  for (long i = 0; i < (long) F.nx * F.ny; i++) F.f[i] = ISO_FIELD_INF;

  /* Stamp every reached node (and the origin itself — pure walking is a legal
   * journey) into the cells its remaining budget can walk to. */
  #define STAMP(NLAT, NLON, TMIN)                                              \
    do {                                                                       \
      double _rem = ((double) max_min - (TMIN)) * 60.0 * walk_mps;             \
      if (_rem > 0) {                                                          \
        if (_rem > egress_max) _rem = egress_max;                              \
        int _ri = (int) (_rem / cell_m) + 1;                                   \
        int _cx = (int) (((NLON) - F.lon0) / F.dlon);                          \
        int _cy = (int) (((NLAT) - F.lat0) / F.dlat);                          \
        for (int _y = _cy - _ri; _y <= _cy + _ri; _y++) {                      \
          if (_y < 0 || _y >= F.ny) continue;                                  \
          for (int _x = _cx - _ri; _x <= _cx + _ri; _x++) {                    \
            if (_x < 0 || _x >= F.nx) continue;                                \
            double _gl = F.lat0 + _y * F.dlat, _go = F.lon0 + _x * F.dlon;     \
            double _d = hav_m(NLAT, NLON, _gl, _go);                           \
            if (_d > _rem) continue;                                           \
            float _t = (float) ((TMIN) + (_d / walk_mps) / 60.0);              \
            long _id = (long) _y * F.nx + _x;                                  \
            if (_t < F.f[_id]) F.f[_id] = _t;                                  \
          }                                                                    \
        }                                                                      \
      }                                                                        \
    } while (0)

  STAMP(olat, olon, 0.0);
  for (int i = 0; i < S.n; i++) {
    if (!isfinite(S.v[i].best)) continue;
    double tmin = (S.v[i].best - dep_sec) / 60.0;
    if (tmin < 0) tmin = 0;
    if (tmin > max_min) continue;
    STAMP(S.v[i].lat, S.v[i].lon, tmin);
  }
  #undef STAMP

  /* ── bands → GeoJSON ─────────────────────────────────────────────────── */
  cJSON *feats = cJSON_CreateArray();
  double simp_tol = (F.dlat < F.dlon ? F.dlat : F.dlon) * 0.30;
  for (int bi = nband - 1; bi >= 0; bi--) {        /* largest band first     */
    double thr = bands[bi];
    int within = 0;
    for (int i = 0; i < S.n; i++)
      if (isfinite(S.v[i].best) && (S.v[i].best - dep_sec) / 60.0 <= thr) within++;

    iso_ring *rings = NULL;
    int nr = contour(&F, thr, &rings);
    if (nr <= 0) { free(rings); continue; }
    for (int i = 0; i < nr; i++) ring_simplify(&rings[i], simp_tol);

    /* Outer vs hole: a ring wholly inside another is a hole of it. Ring
     * counts here are in the tens, so the O(n^2) containment test is cheaper
     * than any index would be. */
    int *parent = malloc((size_t) nr * sizeof(int));
    if (!parent) { for (int i = 0; i < nr; i++) free(rings[i].xy); free(rings); continue; }
    for (int i = 0; i < nr; i++) {
      parent[i] = -1;
      double px = rings[i].xy[0], py = rings[i].xy[1];
      double best_a = 0;
      for (int j = 0; j < nr; j++) {
        if (i == j || rings[j].n < 4) continue;
        if (!ring_contains(&rings[j], px, py)) continue;
        double a = fabs(ring_area(&rings[j]));
        if (parent[i] < 0 || a < best_a) { parent[i] = j; best_a = a; }
      }
    }

    cJSON *polys = cJSON_CreateArray();
    for (int i = 0; i < nr; i++) {
      if (parent[i] >= 0) continue;                 /* a hole, emitted below */
      cJSON *poly = cJSON_CreateArray();
      /* exterior CCW per RFC 7946 */
      int rev = ring_area(&rings[i]) < 0;
      cJSON *ring = cJSON_CreateArray();
      for (int k = 0; k < rings[i].n; k++) {
        int idx = rev ? rings[i].n - 1 - k : k;
        cJSON *pt = cJSON_CreateArray();
        cJSON_AddItemToArray(pt, cJSON_CreateNumber(rings[i].xy[idx * 2]));
        cJSON_AddItemToArray(pt, cJSON_CreateNumber(rings[i].xy[idx * 2 + 1]));
        cJSON_AddItemToArray(ring, pt);
      }
      cJSON_AddItemToArray(poly, ring);
      for (int h = 0; h < nr; h++) {
        if (parent[h] != i) continue;
        int hrev = ring_area(&rings[h]) > 0;        /* holes CW              */
        cJSON *hr = cJSON_CreateArray();
        for (int k = 0; k < rings[h].n; k++) {
          int idx = hrev ? rings[h].n - 1 - k : k;
          cJSON *pt = cJSON_CreateArray();
          cJSON_AddItemToArray(pt, cJSON_CreateNumber(rings[h].xy[idx * 2]));
          cJSON_AddItemToArray(pt, cJSON_CreateNumber(rings[h].xy[idx * 2 + 1]));
          cJSON_AddItemToArray(hr, pt);
        }
        cJSON_AddItemToArray(poly, hr);
      }
      cJSON_AddItemToArray(polys, poly);
    }
    free(parent);
    for (int i = 0; i < nr; i++) free(rings[i].xy);
    free(rings);

    if (cJSON_GetArraySize(polys) == 0) { cJSON_Delete(polys); continue; }
    cJSON *geom = cJSON_CreateObject();
    cJSON_AddStringToObject(geom, "type", "MultiPolygon");
    cJSON_AddItemToObject(geom, "coordinates", polys);
    cJSON *props = cJSON_CreateObject();
    cJSON_AddNumberToObject(props, "band_min", thr);
    cJSON_AddNumberToObject(props, "stops_within", within);
    cJSON *ft = cJSON_CreateObject();
    cJSON_AddStringToObject(ft, "type", "Feature");
    cJSON_AddItemToObject(ft, "properties", props);
    cJSON_AddItemToObject(ft, "geometry", geom);
    cJSON_AddItemToArray(feats, ft);
  }

  free(F.f);
  free(frontier); free(next_fr);

  cJSON *fc = cJSON_CreateObject();
  cJSON_AddStringToObject(fc, "type", "FeatureCollection");
  cJSON_AddItemToObject(fc, "features", feats);
  cJSON *meta = cJSON_CreateObject();
  cJSON *org = cJSON_CreateObject();
  cJSON_AddNumberToObject(org, "lat", olat);
  cJSON_AddNumberToObject(org, "lon", olon);
  cJSON_AddItemToObject(meta, "origin", org);
  cJSON_AddStringToObject(meta, "depart_at", dep_iso);
  cJSON_AddStringToObject(meta, "service_date", sdate);
  cJSON_AddStringToObject(meta, "weekday", wd);
  cJSON *par = cJSON_CreateObject();
  cJSON_AddNumberToObject(par, "max_min", max_min);
  cJSON_AddNumberToObject(par, "max_walk_m", max_walk);
  cJSON_AddNumberToObject(par, "walk_kmh", walk_kmh);
  cJSON_AddNumberToObject(par, "transfer_radius_m", ISO_XFER_RADIUS);
  cJSON_AddNumberToObject(par, "max_transfers", max_xfer);
  cJSON *ba = cJSON_CreateArray();
  for (int i = 0; i < nband; i++) cJSON_AddItemToArray(ba, cJSON_CreateNumber(bands[i]));
  cJSON_AddItemToObject(par, "bands", ba);
  cJSON_AddItemToObject(meta, "params", par);
  cJSON_AddNumberToObject(meta, "stops_loaded", S.n);
  cJSON_AddNumberToObject(meta, "stops_reached", reached);
  cJSON_AddNumberToObject(meta, "stops_first_reached_by_transit", transit_gain);
  cJSON_AddNumberToObject(meta, "rounds", rounds);
  cJSON_AddNumberToObject(meta, "timetable_queries", queries);
  cJSON_AddBoolToObject(meta, "truncated", truncated);
  cJSON *gr = cJSON_CreateObject();
  cJSON_AddNumberToObject(gr, "cell_m", cell_m);
  cJSON_AddNumberToObject(gr, "nx", F.nx);
  cJSON_AddNumberToObject(gr, "ny", F.ny);
  cJSON_AddItemToObject(meta, "grid", gr);
  cJSON_AddBoolToObject(meta, "cached", 0);
  if (transit_gain == 0)
    cJSON_AddStringToObject(meta, "note",
      "no scheduled service departs within the budget from this origin at "
      "this time; the polygon shown is walking reach only");
  cJSON_AddNumberToObject(meta, "elapsed_ms", (double) (now_ms() - t_start));
  cJSON_AddItemToObject(fc, "meta", meta);

  set_free(&S);
  char *out = cJSON_PrintUnformatted(fc);
  cJSON_Delete(fc);
  if (out) collcache_set(db, ck, out, ISO_CACHE_TTL_MS);
  return out;
}
