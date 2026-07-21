/* core/station_clusterer.c — P6 sweep: Node→C port of
 * server/src/utils/stationClusterer.js + the snap math from
 * server/src/utils/transportSpatialSnap.js.
 *
 * See station_clusterer.h for the API + the constants/approximation notes.
 * The file is self-contained: stationNameFingerprint / levenshtein /
 * normName (server/src/collectors/_dedupe.js) are static fns below; nothing
 * from lib/unified.c or lib/dedupe is used.
 *
 * Layout (port pass-by-pass, mirrors the JS file order):
 *   §0 string/codepoint helpers + normName/fingerprint/levenshtein
 *   §1 equirectangular distSqM + cellKey + DSU (union-find)
 *   §2 loadStations() (intel_items reader)
 *   §3 cluster() — 3-pass DSU (wikidata / spatial+name / LLM merges)
 *   §4 materialise() — centroid, longest name, color union, sha1 cluster_uid
 *   §5 segment index + closestPointOnSegment + snapAllWaysAt (snap math)
 *   §6 computeLineDots() + write side (clusters + dots, full rebuild)
 *   §7 station_clusterer_run / linedots_fc_json (read API)
 *   §8 station_snap_stations (snapStationsToNearestLine port)
 */

#include "station_clusterer.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── Constants (verbatim from the two JS files) ──────────────────────── */
#define SPATIAL_RADIUS_M       150.0
#define LEVENSHTEIN_THRESHOLD  2
#define CLUSTER_CELL_SIZE_DEG  0.005   /* stationClusterer.js CELL_SIZE_DEG */
#define SNAP_CELL_SIZE_DEG     0.01    /* transportSpatialSnap.js CELL_SIZE_DEG */
#define METERS_PER_DEG_LAT     111320.0
#define DEFAULT_RADIUS_M       300.0
#define MIN_RAIL_SEPARATION_M  3.0
#define MIN_RAIL_SEPARATION_SQ (MIN_RAIL_SEPARATION_M * MIN_RAIL_SEPARATION_M)
#define MAX_LINE_COLORS_KEEP   5       /* sorted-by-dist cap, snap pass */

/* M_PI is not guaranteed by <math.h> under strict ISO; define if absent. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ════════════════ §0 string / codepoint helpers ════════════════════════
 *
 * The JS fingerprint pipeline (_dedupe.js):
 *   normName(raw)            = String(raw).normalize('NFKC').toLowerCase()
 *                              .replace(/駅|station|停留所|バス停|stop/gi,'')
 *                              .replace(/[\s・\-()（）、,.]/g,'').trim()
 *   stationNameFingerprint   = katakanaToHiragana( normName(s)
 *                                .replace(/(入口|前|口)$/u,'') )
 *   levenshtein              = code-unit DP over the two folded strings.
 *
 * NFKC is approximated (no ICU): lowercase + literal-substring strips +
 * katakana fold only — see header note. Levenshtein runs over Unicode
 * codepoints (== UTF-16 units for the BMP, which is all the folded form
 * contains), matching JS charCodeAt semantics for this input class.        */

/* Decode one UTF-8 codepoint at s[*i]; advance *i past it. Returns the
 * codepoint, or the raw byte on a malformed sequence (best-effort, never
 * loops). */
static unsigned cp_next(const char *s, size_t *i) {
    unsigned char c = (unsigned char)s[*i];
    if (c < 0x80) { (*i)++; return c; }
    if ((c & 0xE0) == 0xC0) {
        unsigned char c1 = (unsigned char)s[*i + 1];
        if ((c1 & 0xC0) == 0x80) {
            *i += 2;
            return ((c & 0x1F) << 6) | (c1 & 0x3F);
        }
        (*i)++; return c;
    }
    if ((c & 0xF0) == 0xE0) {
        unsigned char c1 = (unsigned char)s[*i + 1], c2 = (unsigned char)s[*i + 2];
        if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80) {
            *i += 3;
            return ((c & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
        }
        (*i)++; return c;
    }
    if ((c & 0xF8) == 0xF0) {
        unsigned char c1 = (unsigned char)s[*i + 1], c2 = (unsigned char)s[*i + 2],
                      c3 = (unsigned char)s[*i + 3];
        if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
            *i += 4;
            return ((c & 0x07) << 18) | ((c1 & 0x3F) << 12)
                 | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        }
        (*i)++; return c;
    }
    (*i)++; return c;
}

/* Append codepoint cp to out (UTF-8). Caller guarantees >=4 bytes free. */
static size_t cp_put(char *out, size_t o, unsigned cp) {
    if (cp < 0x80) { out[o++] = (char)cp; }
    else if (cp < 0x800) {
        out[o++] = (char)(0xC0 | (cp >> 6));
        out[o++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out[o++] = (char)(0xE0 | (cp >> 12));
        out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[o++] = (char)(0x80 | (cp & 0x3F));
    } else {
        out[o++] = (char)(0xF0 | (cp >> 18));
        out[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[o++] = (char)(0x80 | (cp & 0x3F));
    }
    return o;
}

/* Codepoint array form of a UTF-8 string (for the Levenshtein DP / suffix
 * trim, which the JS does on UTF-16 units). Returns a malloc'd array; *len
 * set to the count. */
static unsigned *to_cps(const char *s, size_t *len) {
    size_t n = strlen(s);
    unsigned *cps = malloc((n + 1) * sizeof(unsigned));
    if (!cps) { *len = 0; return NULL; }
    size_t i = 0, k = 0;
    while (i < n) cps[k++] = cp_next(s, &i);
    *len = k;
    return cps;
}

/* ASCII case-fold only (JS .toLowerCase() is Unicode-aware, but the strips
 * that follow remove the Latin tokens; CJK has no case). */
static unsigned ascii_lower(unsigned cp) {
    return (cp >= 'A' && cp <= 'Z') ? cp + 32 : cp;
}

/* Does the codepoint sequence at cps[i..] start with `lit` (UTF-8)? Returns
 * the number of *codepoints* matched, else 0. Case-insensitive on ASCII (JS
 * regex flag /gi on the Latin alternatives). */
static size_t cp_match_lit(const unsigned *cps, size_t n, size_t i,
                           const char *lit) {
    size_t li = 0, ln = strlen(lit), k = 0;
    while (li < ln) {
        if (i + k >= n) return 0;
        unsigned want = cp_next(lit, &li);
        unsigned got  = cps[i + k];
        if (ascii_lower(want) != ascii_lower(got)) return 0;
        k++;
    }
    return k;
}

/* normName(raw): NFKC≈(lower) → strip 駅|station|停留所|バス停|stop (gi) →
 * strip [\s・\-()（）、,.] → trim. Result written UTF-8 into `out`
 * (caller-sized; fingerprints are short). */
static void norm_name(const char *raw, char *out, size_t outsz) {
    if (!raw || !*raw) { if (outsz) out[0] = 0; return; }
    size_t n; unsigned *cps = to_cps(raw, &n);
    if (!cps) { if (outsz) out[0] = 0; return; }

    /* The g-replaced tokens, longest-first so "station" wins before any
     * shorter prefix; JS regex alternation is left-to-right but these are
     * disjoint enough that order only matters for safety. */
    static const char *TOK[] = { "停留所", "station", "バス停", "stop", "駅" };
    /* The char-class removed singletons: \s ・ - ( ) （ ） 、 , . */
    static const unsigned DROP[] = {
        ' ', '\t', '\n', '\r', '\f', '\v',
        0x30FB /*・*/, '-', '(', ')', 0xFF08 /*（*/, 0xFF09 /*）*/,
        0x3001 /*、*/, ',', '.'
    };

    size_t o = 0, i = 0;
    while (i < n) {
        size_t m = 0;
        for (size_t t = 0; t < sizeof(TOK) / sizeof(TOK[0]); t++) {
            size_t mm = cp_match_lit(cps, n, i, TOK[t]);
            if (mm) { m = mm; break; }
        }
        if (m) { i += m; continue; }     /* token strip */
        unsigned cp = ascii_lower(cps[i]);
        int drop = 0;
        for (size_t d = 0; d < sizeof(DROP) / sizeof(DROP[0]); d++)
            if (cp == DROP[d]) { drop = 1; break; }
        if (!drop && o + 4 < outsz) o = cp_put(out, o, cp);
        i++;
    }
    out[o] = 0;
    free(cps);
    /* .trim() — the char-class already removed interior+edge whitespace, so
     * there is nothing left to strip; kept here as a structural mirror. */
}

/* stationNameFingerprint(raw): normName then strip a single trailing
 * (入口|前|口), then katakana→hiragana (BMP 0x30A1..0x30F6 minus 0x60). */
static void station_fingerprint(const char *raw, char *out, size_t outsz) {
    char nn[512];
    norm_name(raw, nn, sizeof nn);

    size_t n; unsigned *cps = to_cps(nn, &n);
    if (!cps) { if (outsz) out[0] = 0; return; }

    /* /(入口|前|口)$/u — at most one suffix removed, 入口 (2 cp) preferred. */
    if (n >= 2 && cps[n - 2] == 0x5165 /*入*/ && cps[n - 1] == 0x53E3 /*口*/)
        n -= 2;
    else if (n >= 1 && cps[n - 1] == 0x524D /*前*/)
        n -= 1;
    else if (n >= 1 && cps[n - 1] == 0x53E3 /*口*/)
        n -= 1;

    size_t o = 0;
    for (size_t k = 0; k < n; k++) {
        unsigned cp = cps[k];
        if (cp >= 0x30A1 && cp <= 0x30F6) cp -= 0x60;   /* katakana→hiragana */
        if (o + 4 < outsz) o = cp_put(out, o, cp);
    }
    out[o] = 0;
    free(cps);
}

/* levenshtein(a,b): two-row DP, exactly the JS algorithm but over Unicode
 * codepoints (JS used charCodeAt = UTF-16 units; identical for BMP, which
 * the folded fingerprint always is). */
static int levenshtein_cp(const char *a, const char *b) {
    if (strcmp(a, b) == 0) return 0;
    size_t m, nn;
    unsigned *ca = to_cps(a, &m);
    unsigned *cb = to_cps(b, &nn);
    if (!ca || !cb) { free(ca); free(cb); return (int)(m > nn ? m : nn); }
    if (m == 0)  { free(ca); free(cb); return (int)nn; }
    if (nn == 0) { free(ca); free(cb); return (int)m; }

    int *prev = malloc((nn + 1) * sizeof(int));
    int *curr = malloc((nn + 1) * sizeof(int));
    if (!prev || !curr) { free(prev); free(curr); free(ca); free(cb);
                          return (int)(m > nn ? m : nn); }
    for (size_t j = 0; j <= nn; j++) prev[j] = (int)j;
    for (size_t i = 1; i <= m; i++) {
        curr[0] = (int)i;
        for (size_t j = 1; j <= nn; j++) {
            int cost = (ca[i - 1] == cb[j - 1]) ? 0 : 1;
            int del = prev[j] + 1;
            int ins = curr[j - 1] + 1;
            int sub = prev[j - 1] + cost;
            int mn = del < ins ? del : ins;
            if (sub < mn) mn = sub;
            curr[j] = mn;
        }
        int *t = prev; prev = curr; curr = t;
    }
    int r = prev[nn];
    free(prev); free(curr); free(ca); free(cb);
    return r;
}

/* Codepoint length of a UTF-8 string (== JS .length for BMP). */
static size_t cp_len(const char *s) {
    size_t i = 0, k = 0, n = strlen(s);
    while (i < n) { cp_next(s, &i); k++; }
    return k;
}

/* ════════════════ §1 distance / cellKey / DSU ══════════════════════════ */

/* distSqM — equirectangular metres² (stationClusterer.js distSqM). */
static double dist_sq_m(double ax, double ay, double bx, double by) {
    double cosLat = cos(((ay + by) * 0.5 * M_PI) / 180.0);
    double dx = (bx - ax) * cosLat * METERS_PER_DEG_LAT;
    double dy = (by - ay) * METERS_PER_DEG_LAT;
    return dx * dx + dy * dy;
}

/* Integer cell coords. JS: Math.floor(v / CELL). */
static long cell_of(double v, double cell) {
    return (long)floor(v / cell);
}

/* Union-find: path-compression + union-by-rank, exactly makeDSU(). */
typedef struct { int *p; signed char *r; } dsu_t;

static int dsu_init(dsu_t *d, int n) {
    d->p = malloc((size_t)n * sizeof(int));
    d->r = calloc((size_t)n, 1);
    if (!d->p || !d->r) { free(d->p); free(d->r); d->p = NULL; d->r = NULL; return -1; }
    for (int i = 0; i < n; i++) d->p[i] = i;
    return 0;
}
static void dsu_free(dsu_t *d) { free(d->p); free(d->r); }

static int dsu_find(dsu_t *d, int x) {
    while (d->p[x] != x) { d->p[x] = d->p[d->p[x]]; x = d->p[x]; }
    return x;
}
static void dsu_union(dsu_t *d, int a, int b) {
    int ra = dsu_find(d, a), rb = dsu_find(d, b);
    if (ra == rb) return;
    if (d->r[ra] < d->r[rb])      d->p[ra] = rb;
    else if (d->r[ra] > d->r[rb]) d->p[rb] = ra;
    else { d->p[rb] = ra; d->r[ra]++; }
}

/* ════════════════ §2 loadStations ══════════════════════════════════════ */

typedef struct {
    char  *uid;          /* station_uid (text after first '|') */
    char  *mode;         /* "train" / "subway" */
    char  *name;         /* title */
    char  *name_ja;      /* properties.name_ja */
    char  *operator_;    /* properties.operator */
    char  *wikidata;     /* properties.wikidata */
    char  *line_color;   /* properties.line_color */
    char  *line_name;    /* properties.line || .line_name */
    char  *line_ref;     /* properties.line_ref */
    char **line_colors;  /* properties.line_colors[] (strings, non-null) */
    int    n_line_colors;
    double lat, lon;
    char   fp[512];      /* stationNameFingerprint(name || name_ja || '') */
} station_t;

static char *dup_or_null(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* properties.<key> as a non-empty string, else NULL (cJSON truthiness like
 * JS `p.x || null` — empty string is falsy in JS so treat "" as NULL). */
static char *prop_str(cJSON *p, const char *key) {
    cJSON *v = cJSON_GetObjectItem(p, key);
    if (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
        return dup_or_null(v->valuestring);
    return NULL;
}

static void station_free_one(station_t *s) {
    free(s->uid); free(s->mode); free(s->name); free(s->name_ja);
    free(s->operator_); free(s->wikidata); free(s->line_color);
    free(s->line_name); free(s->line_ref);
    for (int i = 0; i < s->n_line_colors; i++) free(s->line_colors[i]);
    free(s->line_colors);
}
static void stations_free(station_t *a, int n) {
    for (int i = 0; i < n; i++) station_free_one(&a[i]);
    free(a);
}

/* Exact port of stmtStations + loadStations(). On error returns NULL and
 * *out_n stays whatever; on success *out_n = count (may be 0). */
static station_t *load_stations(db_handle *db, int *out_n) {
    static const char *Q =
      "SELECT "
      "  substr(uid, instr(uid, '|') + 1) AS station_uid,"
      "  COALESCE(sub_source_id,"
      "           CASE WHEN source_id = 'unified-trains'  THEN 'train'"
      "                WHEN source_id = 'unified-subways' THEN 'subway'"
      "                ELSE NULL END) AS mode,"
      "  title AS name,"
      "  json_extract(properties, '$.operator') AS operator,"
      "  lat, lon, properties "
      "FROM intel_items "
      "WHERE source_id IN ('unified-trains', 'unified-subways') "
      "  AND lat IS NOT NULL "
      "  AND (geometry IS NULL OR json_extract(geometry, '$.type') = 'Point')";

    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db->h, Q, -1, &st, NULL) != SQLITE_OK) {
        *out_n = -1; return NULL;
    }

    int cap = 256, n = 0;
    station_t *arr = malloc((size_t)cap * sizeof(station_t));
    if (!arr) { sqlite3_finalize(st); *out_n = -1; return NULL; }

    while (sqlite3_step(st) == SQLITE_ROW) {
        if (n == cap) {
            int ncap = cap * 2;
            station_t *na = realloc(arr, (size_t)ncap * sizeof(station_t));
            if (!na) { stations_free(arr, n); sqlite3_finalize(st);
                       *out_n = -1; return NULL; }
            arr = na; cap = ncap;
        }
        station_t *s = &arr[n];
        memset(s, 0, sizeof *s);

        const char *c_uid  = (const char *)sqlite3_column_text(st, 0);
        const char *c_mode = (const char *)sqlite3_column_text(st, 1);
        const char *c_name = (const char *)sqlite3_column_text(st, 2);
        const char *c_oper = (const char *)sqlite3_column_text(st, 3);
        s->lat = sqlite3_column_double(st, 4);
        s->lon = sqlite3_column_double(st, 5);
        const char *c_props = (const char *)sqlite3_column_text(st, 6);

        s->uid       = dup_or_null(c_uid);
        s->mode      = dup_or_null(c_mode);
        s->name      = dup_or_null(c_name);
        s->operator_ = dup_or_null(c_oper);   /* json_extract → may be NULL */

        cJSON *p = (c_props && *c_props) ? cJSON_Parse(c_props) : NULL;
        if (p) {
            s->name_ja    = prop_str(p, "name_ja");
            s->wikidata   = prop_str(p, "wikidata");
            s->line_color = prop_str(p, "line_color");
            s->line_name  = prop_str(p, "line");
            if (!s->line_name) s->line_name = prop_str(p, "line_name");
            s->line_ref   = prop_str(p, "line_ref");
            cJSON *lc = cJSON_GetObjectItem(p, "line_colors");
            if (lc && cJSON_IsArray(lc)) {
                int m = cJSON_GetArraySize(lc);
                if (m > 0) {
                    s->line_colors = malloc((size_t)m * sizeof(char *));
                    if (s->line_colors) {
                        for (int k = 0; k < m; k++) {
                            cJSON *e = cJSON_GetArrayItem(lc, k);
                            if (e && cJSON_IsString(e) && e->valuestring)
                                s->line_colors[s->n_line_colors++] =
                                    dup_or_null(e->valuestring);
                        }
                    }
                }
            }
        }
        /* fingerprint = stationNameFingerprint(name || name_ja || '') */
        const char *fpsrc = (s->name && s->name[0]) ? s->name
                          : (s->name_ja ? s->name_ja : "");
        station_fingerprint(fpsrc, s->fp, sizeof s->fp);

        cJSON_Delete(p);
        n++;
    }
    sqlite3_finalize(st);
    *out_n = n;
    return arr;
}

/* ════════════════ §3 cluster() — 3-pass union-find ════════════════════ */

/* Open-addressing string→int hash map (small helper for the wikidata and
 * uid→index maps in the JS cluster()). */
typedef struct { char **k; int *v; int cap, n; } smap_t;

static unsigned smap_hash(const char *s) {
    unsigned h = 2166136261u;
    for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
    return h;
}
static int smap_init(smap_t *m, int cap) {
    if (cap < 16) cap = 16;
    /* round up to power of two */
    int c = 16; while (c < cap) c <<= 1;
    m->k = calloc((size_t)c, sizeof(char *));
    m->v = malloc((size_t)c * sizeof(int));
    if (!m->k || !m->v) { free(m->k); free(m->v); return -1; }
    m->cap = c; m->n = 0;
    return 0;
}
static void smap_free(smap_t *m) { free(m->k); free(m->v); }

/* Get index for key, or -1. */
static int smap_get(smap_t *m, const char *key) {
    unsigned h = smap_hash(key) & (unsigned)(m->cap - 1);
    for (int probe = 0; probe < m->cap; probe++) {
        unsigned i = (h + (unsigned)probe) & (unsigned)(m->cap - 1);
        if (!m->k[i]) return -1;
        if (strcmp(m->k[i], key) == 0) return m->v[i];
    }
    return -1;
}
/* Insert key→val only if absent (mirrors `if (map.has(k)) … else map.set`). */
static void smap_put_if_absent(smap_t *m, const char *key, int val) {
    unsigned h = smap_hash(key) & (unsigned)(m->cap - 1);
    for (int probe = 0; probe < m->cap; probe++) {
        unsigned i = (h + (unsigned)probe) & (unsigned)(m->cap - 1);
        if (!m->k[i]) {
            m->k[i] = (char *)key;   /* borrows; keys outlive the map */
            m->v[i] = val; m->n++;
            return;
        }
        if (strcmp(m->k[i], key) == 0) return;
    }
}

/* Cell bucket: linked list keyed by (cx,cy). */
typedef struct cell_s {
    long cx, cy;
    int *items; int n, cap;
    struct cell_s *next;
} cell_t;

#define CELL_BUCKETS 65536
typedef struct { cell_t *b[CELL_BUCKETS]; } cellmap_t;

static unsigned cell_hash(long cx, long cy) {
    unsigned h = (unsigned)(cx * 73856093) ^ (unsigned)(cy * 19349663);
    return h & (CELL_BUCKETS - 1);
}
static cell_t *cell_find(cellmap_t *cm, long cx, long cy) {
    for (cell_t *c = cm->b[cell_hash(cx, cy)]; c; c = c->next)
        if (c->cx == cx && c->cy == cy) return c;
    return NULL;
}
static int cell_push(cellmap_t *cm, long cx, long cy, int idx) {
    unsigned h = cell_hash(cx, cy);
    cell_t *c = NULL;
    for (c = cm->b[h]; c; c = c->next)
        if (c->cx == cx && c->cy == cy) break;
    if (!c) {
        c = calloc(1, sizeof *c);
        if (!c) return -1;
        c->cx = cx; c->cy = cy;
        c->next = cm->b[h]; cm->b[h] = c;
    }
    if (c->n == c->cap) {
        int nc = c->cap ? c->cap * 2 : 4;
        int *ni = realloc(c->items, (size_t)nc * sizeof(int));
        if (!ni) return -1;
        c->items = ni; c->cap = nc;
    }
    c->items[c->n++] = idx;
    return 0;
}
static void cellmap_free(cellmap_t *cm) {
    for (int i = 0; i < CELL_BUCKETS; i++) {
        cell_t *c = cm->b[i];
        while (c) { cell_t *nx = c->next; free(c->items); free(c); c = nx; }
        cm->b[i] = NULL;
    }
}

/* cluster(stations) → DSU populated; group collection done by caller.
 * Returns 0, or -1 on alloc/DB failure. */
static int cluster_pass(db_handle *db, station_t *S, int n, dsu_t *dsu) {
    /* Pass 1: wikidata identity merge. */
    {
        smap_t byq;
        if (smap_init(&byq, n * 2 + 16) != 0) return -1;
        for (int i = 0; i < n; i++) {
            const char *q = S[i].wikidata;
            if (!q || !q[0]) continue;
            int prev = smap_get(&byq, q);
            if (prev >= 0) dsu_union(dsu, prev, i);
            else smap_put_if_absent(&byq, q, i);
        }
        smap_free(&byq);
    }

    /* Pass 2: spatial + name merge over 0.005° cells, 3×3 neighbourhood. */
    cellmap_t cm; memset(&cm, 0, sizeof cm);
    for (int i = 0; i < n; i++) {
        long cx = cell_of(S[i].lon, CLUSTER_CELL_SIZE_DEG);
        long cy = cell_of(S[i].lat, CLUSTER_CELL_SIZE_DEG);
        if (cell_push(&cm, cx, cy, i) != 0) { cellmap_free(&cm); return -1; }
    }

    const double radiusSq = SPATIAL_RADIUS_M * SPATIAL_RADIUS_M;
    /* Iterate buckets exactly like `for (const [key,bucket] of cells)`:
     * gather candidates from the 3×3 ring, then pair bucket×candidates with
     * the j<=i skip. Result is order-independent (union is commutative). */
    for (int hb = 0; hb < CELL_BUCKETS; hb++) {
        for (cell_t *c = cm.b[hb]; c; c = c->next) {
            /* candidate list = union of the 9 neighbour buckets */
            int ccap = 0;
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++) {
                    cell_t *nb = cell_find(&cm, c->cx + dx, c->cy + dy);
                    if (nb) ccap += nb->n;
                }
            if (ccap == 0) continue;
            int *cand = malloc((size_t)ccap * sizeof(int));
            if (!cand) { cellmap_free(&cm); return -1; }
            int cn = 0;
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++) {
                    cell_t *nb = cell_find(&cm, c->cx + dx, c->cy + dy);
                    if (nb) for (int t = 0; t < nb->n; t++) cand[cn++] = nb->items[t];
                }
            for (int bi = 0; bi < c->n; bi++) {
                int i = c->items[bi];
                station_t *a = &S[i];
                for (int cj = 0; cj < cn; cj++) {
                    int j = cand[cj];
                    if (j <= i) continue;
                    station_t *b = &S[j];
                    if (!a->fp[0] || !b->fp[0]) continue;
                    if (dist_sq_m(a->lon, a->lat, b->lon, b->lat) > radiusSq)
                        continue;
                    if (strcmp(a->fp, b->fp) == 0) { dsu_union(dsu, i, j); continue; }
                    size_t la = cp_len(a->fp), lb = cp_len(b->fp);
                    size_t mn = la < lb ? la : lb;
                    if (mn >= 3 &&
                        levenshtein_cp(a->fp, b->fp) <= LEVENSHTEIN_THRESHOLD)
                        dsu_union(dsu, i, j);
                }
            }
            free(cand);
        }
    }
    cellmap_free(&cm);

    /* Pass 3: trusted LLM merges (same=1 AND confidence>=0.7). */
    {
        smap_t byuid;
        if (smap_init(&byuid, n * 2 + 16) != 0) return -1;
        for (int i = 0; i < n; i++)
            if (S[i].uid) smap_put_if_absent(&byuid, S[i].uid, i);

        sqlite3_stmt *m;
        static const char *MQ =
          "SELECT uid_a, uid_b FROM llm_station_merges "
          "WHERE same = 1 AND confidence >= 0.7";
        if (sqlite3_prepare_v2(db->h, MQ, -1, &m, NULL) == SQLITE_OK) {
            while (sqlite3_step(m) == SQLITE_ROW) {
                const char *ua = (const char *)sqlite3_column_text(m, 0);
                const char *ub = (const char *)sqlite3_column_text(m, 1);
                if (!ua || !ub) continue;
                int i = smap_get(&byuid, ua);
                int j = smap_get(&byuid, ub);
                if (i >= 0 && j >= 0) dsu_union(dsu, i, j);
            }
            sqlite3_finalize(m);
        }
        smap_free(&byuid);
    }
    return 0;
}

/* ════════════════ §4 materialise ═══════════════════════════════════════ */

typedef struct {
    char  cluster_uid[41];   /* sha1 hex (40) + NUL */
    char *name;              /* longest member name, "(unknown)" fallback */
    char *name_ja;           /* first non-null member name_ja */
    double lat, lon;
    char **member_uids; int n_members;          /* sorted asc */
    char **line_colors; char **line_names;
    char **line_refs;   char **line_modes; int n_colors;
    char **mode_set;     int n_modes;           /* sorted asc, unique */
    char **operator_set; int n_ops;             /* sorted asc, unique, non-null */
} cluster_row_t;

static int cmp_cstr(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(x, y);   /* JS Array.prototype.sort default = code-unit-ish;
                              member_uids/operator/mode are ASCII → matches */
}

/* sha1(member_uids.join('|')) hex into out[41]. */
static void sha1_join_pipe(char **uids, int n, char *out41) {
    SHA_CTX c; SHA1_Init(&c);
    for (int i = 0; i < n; i++) {
        if (i) SHA1_Update(&c, "|", 1);
        SHA1_Update(&c, uids[i], strlen(uids[i]));
    }
    unsigned char d[20]; SHA1_Final(d, &c);
    for (int i = 0; i < 20; i++) sprintf(out41 + i * 2, "%02x", d[i]);
    out41[40] = 0;
}

/* Push s (copied) into a growable string array if not NULL. */
static int arr_push(char ***arr, int *n, int *cap, const char *s) {
    if (*n == *cap) {
        int nc = *cap ? *cap * 2 : 4;
        char **na = realloc(*arr, (size_t)nc * sizeof(char *));
        if (!na) return -1;
        *arr = na; *cap = nc;
    }
    (*arr)[(*n)++] = dup_or_null(s ? s : "");
    return 0;
}

/* Sort + dedupe a string array in place (drops empty entries first if
 * `drop_empty`). */
static void sort_unique(char **a, int *n, int drop_empty) {
    int m = 0;
    for (int i = 0; i < *n; i++) {
        if (drop_empty && (!a[i] || !a[i][0])) { free(a[i]); continue; }
        a[m++] = a[i];
    }
    *n = m;
    if (m <= 1) return;
    qsort(a, (size_t)m, sizeof(char *), cmp_cstr);
    int w = 1;
    for (int i = 1; i < m; i++) {
        if (strcmp(a[i], a[w - 1]) == 0) free(a[i]);
        else a[w++] = a[i];
    }
    *n = w;
}

static void cluster_row_free(cluster_row_t *r) {
    free(r->name); free(r->name_ja);
    for (int i = 0; i < r->n_members; i++) free(r->member_uids[i]);
    free(r->member_uids);
    for (int i = 0; i < r->n_colors; i++) {
        free(r->line_colors[i]); free(r->line_names[i]);
        free(r->line_refs[i]);   free(r->line_modes[i]);
    }
    free(r->line_colors); free(r->line_names);
    free(r->line_refs);   free(r->line_modes);
    for (int i = 0; i < r->n_modes; i++) free(r->mode_set[i]);
    free(r->mode_set);
    for (int i = 0; i < r->n_ops; i++) free(r->operator_set[i]);
    free(r->operator_set);
}

/* materialise(group): group = member indices into S. Returns 0 / -1. */
static int materialise(station_t *S, const int *grp, int gn, cluster_row_t *out) {
    memset(out, 0, sizeof *out);

    /* centroid (simple mean) */
    double latSum = 0, lonSum = 0;
    for (int i = 0; i < gn; i++) { latSum += S[grp[i]].lat; lonSum += S[grp[i]].lon; }
    out->lat = latSum / gn;
    out->lon = lonSum / gn;

    /* longest name (strict >, JS uses .length = UTF-16 units → cp_len);
     * first non-null name_ja. */
    const char *best = "";
    size_t bestlen = 0;
    for (int i = 0; i < gn; i++) {
        station_t *m = &S[grp[i]];
        if (m->name && m->name[0]) {
            size_t L = cp_len(m->name);
            if (L > bestlen) { bestlen = L; best = m->name; }
        }
        if (m->name_ja && !out->name_ja) out->name_ja = dup_or_null(m->name_ja);
    }
    out->name = dup_or_null((best && best[0]) ? best : "(unknown)");

    /* line union: iterate members in group order, each member's
     * line_colors[] in order; dedupe by color; per-color introducing mode;
     * name/ref only if the color is that member's primary line_color. */
    int cap_c = 0;
    char **seen = NULL; int seen_n = 0, seen_cap = 0;   /* dedupe set */
    for (int i = 0; i < gn; i++) {
        station_t *m = &S[grp[i]];
        for (int k = 0; k < m->n_line_colors; k++) {
            const char *col = m->line_colors[k];
            if (!col || !col[0]) continue;
            int dup = 0;
            for (int s = 0; s < seen_n; s++)
                if (strcmp(seen[s], col) == 0) { dup = 1; break; }
            if (dup) continue;
            if (arr_push(&seen, &seen_n, &seen_cap, col) != 0) goto oom;

            if (arr_push(&out->line_colors, &out->n_colors, &cap_c, col) != 0) goto oom;
            int tmp1 = out->n_colors - 1, c1 = cap_c, c2 = cap_c, c3 = cap_c;
            int n2 = tmp1, n3 = tmp1, n4 = tmp1;
            int is_primary = (m->line_color && strcmp(m->line_color, col) == 0);
            if (arr_push(&out->line_modes, &n4, &c3, m->mode ? m->mode : "") != 0) goto oom;
            if (arr_push(&out->line_names, &n2, &c1,
                         is_primary ? (m->line_name ? m->line_name : "") : "") != 0) goto oom;
            if (arr_push(&out->line_refs, &n3, &c2,
                         is_primary ? (m->line_ref ? m->line_ref : "") : "") != 0) goto oom;
        }
    }
    for (int s = 0; s < seen_n; s++) free(seen[s]);
    free(seen);

    /* mode_set / operator_set: unique + sorted asc; operator drops nulls. */
    {
        int mc = 0, oc = 0;
        for (int i = 0; i < gn; i++) {
            station_t *m = &S[grp[i]];
            if (arr_push(&out->mode_set, &out->n_modes, &mc,
                         m->mode ? m->mode : "") != 0) goto oom;
            if (m->operator_ && m->operator_[0])
                if (arr_push(&out->operator_set, &out->n_ops, &oc,
                             m->operator_) != 0) goto oom;
        }
        sort_unique(out->mode_set, &out->n_modes, 0);
        sort_unique(out->operator_set, &out->n_ops, 1);
    }

    /* member_uids sorted asc; cluster_uid = sha1(join '|'). */
    {
        int uc = 0;
        for (int i = 0; i < gn; i++)
            if (arr_push(&out->member_uids, &out->n_members, &uc,
                         S[grp[i]].uid ? S[grp[i]].uid : "") != 0) goto oom;
        if (out->n_members > 1)
            qsort(out->member_uids, (size_t)out->n_members,
                  sizeof(char *), cmp_cstr);
        sha1_join_pipe(out->member_uids, out->n_members, out->cluster_uid);
    }
    return 0;

oom:
    for (int s = 0; s < seen_n; s++) free(seen[s]);
    free(seen);
    cluster_row_free(out);
    return -1;
}

/* ════════════════ §5 segment index + snap math ════════════════════════ */

typedef struct {
    double ax, ay, bx, by;
    char  *color;     /* borrowed (lives in seg_index strings pool) */
    char  *way_uid;   /* borrowed; may be NULL */
} seg_t;

typedef struct seg_cell_s {
    long cx, cy;
    seg_t **segs; int n, cap;
    struct seg_cell_s *next;
} seg_cell_t;

typedef struct {
    seg_cell_t *b[CELL_BUCKETS];
    seg_t      *pool;  int pn, pcap;       /* owns seg_t storage */
    char      **strs;  int sn, scap;       /* owns color/way_uid strings */
    double      radiusSqM;
} seg_index_t;

static char *seg_intern(seg_index_t *ix, const char *s) {
    if (!s) return NULL;
    char *d = dup_or_null(s);
    if (!d) return NULL;
    if (ix->sn == ix->scap) {
        int nc = ix->scap ? ix->scap * 2 : 64;
        char **ns = realloc(ix->strs, (size_t)nc * sizeof(char *));
        if (!ns) { free(d); return NULL; }
        ix->strs = ns; ix->scap = nc;
    }
    ix->strs[ix->sn++] = d;
    return d;
}

static int seg_cell_push(seg_index_t *ix, long cx, long cy, seg_t *sg) {
    unsigned h = cell_hash(cx, cy);
    seg_cell_t *c;
    for (c = ix->b[h]; c; c = c->next)
        if (c->cx == cx && c->cy == cy) break;
    if (!c) {
        c = calloc(1, sizeof *c);
        if (!c) return -1;
        c->cx = cx; c->cy = cy; c->next = ix->b[h]; ix->b[h] = c;
    }
    if (c->n == c->cap) {
        int nc = c->cap ? c->cap * 2 : 8;
        seg_t **ns = realloc(c->segs, (size_t)nc * sizeof(seg_t *));
        if (!ns) return -1;
        c->segs = ns; c->cap = nc;
    }
    c->segs[c->n++] = sg;
    return 0;
}

static void seg_index_free(seg_index_t *ix) {
    for (int i = 0; i < CELL_BUCKETS; i++) {
        seg_cell_t *c = ix->b[i];
        while (c) { seg_cell_t *nx = c->next; free(c->segs); free(c); c = nx; }
    }
    for (int i = 0; i < ix->sn; i++) free(ix->strs[i]);
    free(ix->strs);
    free(ix->pool);
}

/* buildSegmentIndex(lines): `lines` = cJSON array of Feature objects with
 * .properties.line_color (required) + .geometry.coordinates. Stamps each
 * consecutive coord pair into the 0.01° cell(s) its endpoints touch. */
static int build_segment_index(seg_index_t *ix, cJSON *lines) {
    cJSON *line;
    cJSON_ArrayForEach(line, lines) {
        cJSON *props = cJSON_GetObjectItem(line, "properties");
        cJSON *colj  = props ? cJSON_GetObjectItem(props, "line_color") : NULL;
        if (!colj || !cJSON_IsString(colj) || !colj->valuestring ||
            !colj->valuestring[0]) continue;
        cJSON *geom  = cJSON_GetObjectItem(line, "geometry");
        cJSON *coords = geom ? cJSON_GetObjectItem(geom, "coordinates") : NULL;
        if (!coords || !cJSON_IsArray(coords) ||
            cJSON_GetArraySize(coords) < 2) continue;

        char *color = seg_intern(ix, colj->valuestring);
        cJSON *wuj = props ? cJSON_GetObjectItem(props, "line_uid") : NULL;
        if (!wuj || !cJSON_IsString(wuj))
            wuj = props ? cJSON_GetObjectItem(props, "way_id") : NULL;
        char *wuid = (wuj && cJSON_IsString(wuj) && wuj->valuestring)
                       ? seg_intern(ix, wuj->valuestring) : NULL;

        int cc = cJSON_GetArraySize(coords);
        for (int i = 0; i < cc - 1; i++) {
            cJSON *a = cJSON_GetArrayItem(coords, i);
            cJSON *b = cJSON_GetArrayItem(coords, i + 1);
            if (!a || !cJSON_IsArray(a) || cJSON_GetArraySize(a) < 2) continue;
            if (!b || !cJSON_IsArray(b) || cJSON_GetArraySize(b) < 2) continue;
            double ax = cJSON_GetArrayItem(a, 0)->valuedouble;
            double ay = cJSON_GetArrayItem(a, 1)->valuedouble;
            double bx = cJSON_GetArrayItem(b, 0)->valuedouble;
            double by = cJSON_GetArrayItem(b, 1)->valuedouble;

            if (ix->pn == ix->pcap) {
                int nc = ix->pcap ? ix->pcap * 2 : 1024;
                seg_t *np = realloc(ix->pool, (size_t)nc * sizeof(seg_t));
                if (!np) return -1;
                ix->pool = np; ix->pcap = nc;
            }
            seg_t *sg = &ix->pool[ix->pn++];
            sg->ax = ax; sg->ay = ay; sg->bx = bx; sg->by = by;
            sg->color = color; sg->way_uid = wuid;

            long kAx = cell_of(ax, SNAP_CELL_SIZE_DEG);
            long kAy = cell_of(ay, SNAP_CELL_SIZE_DEG);
            long kBx = cell_of(bx, SNAP_CELL_SIZE_DEG);
            long kBy = cell_of(by, SNAP_CELL_SIZE_DEG);
            if (seg_cell_push(ix, kAx, kAy, sg) != 0) return -1;
            if (!(kBx == kAx && kBy == kAy))
                if (seg_cell_push(ix, kBx, kBy, sg) != 0) return -1;
        }
    }
    return 0;
}

/* closestPointOnSegment(): writes snapped lon/lat + returns distSqM. */
static double closest_point_on_segment(double px, double py,
                                       double ax, double ay,
                                       double bx, double by,
                                       double *snapLon, double *snapLat) {
    double midLat = (ay + by) * 0.5;
    double cosLat = cos((midLat * M_PI) / 180.0);
    double ex = (bx - ax) * cosLat * METERS_PER_DEG_LAT;
    double ey = (by - ay) * METERS_PER_DEG_LAT;
    double dx = (px - ax) * cosLat * METERS_PER_DEG_LAT;
    double dy = (py - ay) * METERS_PER_DEG_LAT;
    double segLenSq = ex * ex + ey * ey;
    double t = (segLenSq == 0.0) ? 0.0 : (dx * ex + dy * ey) / segLenSq;
    if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
    *snapLon = ax + t * (bx - ax);
    *snapLat = ay + t * (by - ay);
    double ddx = dx - t * ex;
    double ddy = dy - t * ey;
    return ddx * ddx + ddy * ddy;
}

/* segmentDistSqM(): point-to-segment distance² only (snap pass). */
static double segment_dist_sq_m(double px, double py,
                                double ax, double ay,
                                double bx, double by) {
    double midLat = (ay + by) * 0.5;
    double cosLat = cos((midLat * M_PI) / 180.0);
    double ex = (bx - ax) * cosLat * METERS_PER_DEG_LAT;
    double ey = (by - ay) * METERS_PER_DEG_LAT;
    double dx = (px - ax) * cosLat * METERS_PER_DEG_LAT;
    double dy = (py - ay) * METERS_PER_DEG_LAT;
    double segLenSq = ex * ex + ey * ey;
    double t = (segLenSq == 0.0) ? 0.0 : (dx * ex + dy * ey) / segLenSq;
    if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
    double projX = t * ex, projY = t * ey;
    double ddx = dx - projX, ddy = dy - projY;
    return ddx * ddx + ddy * ddy;
}

/* One snap point. */
typedef struct { double lon, lat, distSqM; char *color; } snap_t;

/* color → nearest squared distance (snap pass per-station scratch). */
typedef struct { char *c; double dsq; } colorbest_t;

/* snapAllWaysAt(index, lon, lat): per-colour, rail A = nearest snap, rail B
 * = first snap >= MIN_RAIL_SEPARATION_M from A (screen space). Appends to
 * *out (grown); returns count or -1. Iteration over the 3×3 cell ring and
 * the per-color insertion order mirror the JS Map insertion order so the
 * (color → first segment seen) and stable sort behave identically. */
static int snap_all_ways_at(seg_index_t *ix, double lon, double lat,
                            snap_t **out, int *out_n, int *out_cap) {
    long cx = cell_of(lon, SNAP_CELL_SIZE_DEG);
    long cy = cell_of(lat, SNAP_CELL_SIZE_DEG);

    /* group snaps by color, preserving first-seen color order (JS Map). */
    typedef struct {
        char *color;
        snap_t *pts; int n, cap;
    } grp_t;
    grp_t *G = NULL; int gn = 0, gcap = 0;

    for (int dx = -1; dx <= 1; dx++)
      for (int dy = -1; dy <= 1; dy++) {
        seg_cell_t *c;
        long qx = cx + dx, qy = cy + dy;
        for (c = ix->b[cell_hash(qx, qy)]; c; c = c->next)
            if (c->cx == qx && c->cy == qy) break;
        if (!c) continue;
        for (int s = 0; s < c->n; s++) {
            seg_t *sg = c->segs[s];
            double sl, sa;
            double dsq = closest_point_on_segment(lon, lat, sg->ax, sg->ay,
                                                  sg->bx, sg->by, &sl, &sa);
            if (dsq > ix->radiusSqM) continue;
            /* find/create the color group (first-seen order) */
            int gi = -1;
            for (int g = 0; g < gn; g++)
                if (strcmp(G[g].color, sg->color) == 0) { gi = g; break; }
            if (gi < 0) {
                if (gn == gcap) {
                    int ncp = gcap ? gcap * 2 : 8;
                    grp_t *ng = realloc(G, (size_t)ncp * sizeof(grp_t));
                    if (!ng) goto oom;
                    G = ng; gcap = ncp;
                }
                G[gn].color = sg->color;
                G[gn].pts = NULL; G[gn].n = 0; G[gn].cap = 0;
                gi = gn++;
            }
            grp_t *g = &G[gi];
            if (g->n == g->cap) {
                int ncp = g->cap ? g->cap * 2 : 8;
                snap_t *np = realloc(g->pts, (size_t)ncp * sizeof(snap_t));
                if (!np) goto oom;
                g->pts = np; g->cap = ncp;
            }
            g->pts[g->n].lon = sl; g->pts[g->n].lat = sa;
            g->pts[g->n].distSqM = dsq; g->pts[g->n].color = sg->color;
            g->n++;
        }
      }

    double cosLat = cos((lat * M_PI) / 180.0);
    for (int g = 0; g < gn; g++) {
        grp_t *gp = &G[g];
        if (gp->n == 0) continue;
        /* sort by distSqM ascending (JS Array.sort numeric). */
        for (int a = 1; a < gp->n; a++) {       /* stable insertion sort */
            snap_t key = gp->pts[a];
            int b = a - 1;
            while (b >= 0 && gp->pts[b].distSqM > key.distSqM) {
                gp->pts[b + 1] = gp->pts[b]; b--;
            }
            gp->pts[b + 1] = key;
        }
        snap_t railA = gp->pts[0];
        if (*out_n == *out_cap) {
            int ncp = *out_cap ? *out_cap * 2 : 16;
            snap_t *no = realloc(*out, (size_t)ncp * sizeof(snap_t));
            if (!no) goto oom;
            *out = no; *out_cap = ncp;
        }
        (*out)[*out_n] = railA; (*out_n)++;
        for (int k = 1; k < gp->n; k++) {
            snap_t *p = &gp->pts[k];
            double ddx = (p->lon - railA.lon) * cosLat * METERS_PER_DEG_LAT;
            double ddy = (p->lat - railA.lat) * METERS_PER_DEG_LAT;
            double sqSep = ddx * ddx + ddy * ddy;
            if (sqSep < MIN_RAIL_SEPARATION_SQ) continue;
            if (*out_n == *out_cap) {
                int ncp = *out_cap ? *out_cap * 2 : 16;
                snap_t *no = realloc(*out, (size_t)ncp * sizeof(snap_t));
                if (!no) goto oom;
                *out = no; *out_cap = ncp;
            }
            (*out)[*out_n] = *p; (*out_n)++;
            break;
        }
    }
    for (int g = 0; g < gn; g++) free(G[g].pts);
    free(G);
    return *out_n;

oom:
    for (int g = 0; g < gn; g++) free(G[g].pts);
    free(G);
    return -1;
}

/* ════════════════ §6 lines reader + write side ════════════════════════ */

/* getLinesByMode(mode) (raw): pull LineString rows for the mode's
 * source_id from intel_items as a cJSON array of Feature objects
 * {properties, geometry}. (No chaikin/RDP/stitch — see header note; that
 * read-time smoothing is a sibling port.) Returns a cJSON array (caller
 * cJSON_Delete) or NULL. */
static cJSON *load_lines_for_mode(db_handle *db, const char *mode) {
    const char *source_id =
        (strcmp(mode, "train") == 0)  ? "unified-trains"  :
        (strcmp(mode, "subway") == 0) ? "unified-subways" : NULL;
    if (!source_id) return cJSON_CreateArray();

    static const char *Q =
      "SELECT properties, geometry FROM intel_items "
      "WHERE source_id = ?1 "
      "  AND geometry IS NOT NULL "
      "  AND json_extract(geometry, '$.type') IN "
      "      ('LineString','MultiLineString')";
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db->h, Q, -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, source_id, -1, SQLITE_STATIC);

    cJSON *arr = cJSON_CreateArray();
    if (!arr) { sqlite3_finalize(st); return NULL; }
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *pj = (const char *)sqlite3_column_text(st, 0);
        const char *gj = (const char *)sqlite3_column_text(st, 1);
        cJSON *props = (pj && *pj) ? cJSON_Parse(pj) : NULL;
        cJSON *geom  = (gj && *gj) ? cJSON_Parse(gj) : NULL;
        if (!props) props = cJSON_CreateObject();
        if (!geom)  { cJSON_Delete(props); continue; }

        /* MultiLineString → first ring as LineString (mirrors store's
         * "first ring of MultiLineString" handling). */
        cJSON *gtype = cJSON_GetObjectItem(geom, "type");
        if (gtype && cJSON_IsString(gtype) &&
            strcmp(gtype->valuestring, "MultiLineString") == 0) {
            cJSON *mc = cJSON_GetObjectItem(geom, "coordinates");
            cJSON *first = (mc && cJSON_IsArray(mc)) ?
                           cJSON_GetArrayItem(mc, 0) : NULL;
            cJSON *ng = cJSON_CreateObject();
            cJSON_AddStringToObject(ng, "type", "LineString");
            cJSON_AddItemToObject(ng, "coordinates",
                first ? cJSON_Duplicate(first, 1) : cJSON_CreateArray());
            cJSON_Delete(geom);
            geom = ng;
        }
        cJSON *feat = cJSON_CreateObject();
        cJSON_AddItemToObject(feat, "properties", props);
        cJSON_AddItemToObject(feat, "geometry", geom);
        cJSON_AddItemToArray(arr, feat);
    }
    sqlite3_finalize(st);
    return arr;
}

/* buildSnapIndexForMode(mode): colored lines → segment index, radius
 * DEFAULT_RADIUS_M. Returns 0 / -1; index pre-zeroed by caller. */
static int build_snap_index_for_mode(db_handle *db, const char *mode,
                                     seg_index_t *ix) {
    cJSON *lines = load_lines_for_mode(db, mode);
    if (!lines) return -1;
    ix->radiusSqM = DEFAULT_RADIUS_M * DEFAULT_RADIUS_M;
    int rc = build_segment_index(ix, lines);
    cJSON_Delete(lines);
    return rc;
}

/* writeClustersTx + writeDotsTx: full rebuild of both tables in one tx.
 * Returns 0 / -1. `dots` allowed NULL/0 (line-dots failure path). */
static int json_str_array(char **a, int n, char *buf, size_t bufsz) {
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++)
        cJSON_AddItemToArray(arr, cJSON_CreateString(a[i] ? a[i] : ""));
    char *s = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!s) return -1;
    snprintf(buf, bufsz, "%s", s);
    free(s);
    return 0;
}

typedef struct {
    char *cluster_uid, *way_uid, *line_color, *line_mode;
    double lon, lat;
} dot_t;

static int write_all_tx(db_handle *db, cluster_row_t *rows, int nrows,
                        dot_t *dots, int ndots) {
    if (sqlite3_exec(db->h, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK)
        return -1;

    int ok = 1;
    if (sqlite3_exec(db->h, "DELETE FROM station_clusters", NULL, NULL, NULL)
            != SQLITE_OK) ok = 0;
    if (ok && sqlite3_exec(db->h, "DELETE FROM station_line_dots",
                           NULL, NULL, NULL) != SQLITE_OK) ok = 0;

    sqlite3_stmt *ins = NULL, *insd = NULL;
    if (ok) {
        static const char *IQ =
          "INSERT INTO station_clusters ("
          " cluster_uid, name, name_ja, lat, lon,"
          " member_uids, line_colors, line_names, line_refs, line_modes,"
          " mode_set, operator_set) "
          "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12)";
        if (sqlite3_prepare_v2(db->h, IQ, -1, &ins, NULL) != SQLITE_OK) ok = 0;
    }
    if (ok) {
        static const char *IDQ =
          "INSERT OR REPLACE INTO station_line_dots "
          "(cluster_uid, way_uid, line_color, line_mode, lon, lat) "
          "VALUES (?1,?2,?3,?4,?5,?6)";
        if (sqlite3_prepare_v2(db->h, IDQ, -1, &insd, NULL) != SQLITE_OK) ok = 0;
    }

    /* JSON.stringify of the per-cluster arrays. Cluster names/uids are
     * short; 16 KiB is generous (largest interchange ~tens of members). */
    char b_mu[16384], b_lc[8192], b_ln[8192], b_lr[8192],
         b_lm[8192], b_ms[1024], b_os[8192];
    for (int i = 0; ok && i < nrows; i++) {
        cluster_row_t *r = &rows[i];
        if (json_str_array(r->member_uids, r->n_members, b_mu, sizeof b_mu) ||
            json_str_array(r->line_colors, r->n_colors, b_lc, sizeof b_lc) ||
            json_str_array(r->line_names, r->n_colors, b_ln, sizeof b_ln) ||
            json_str_array(r->line_refs, r->n_colors, b_lr, sizeof b_lr) ||
            json_str_array(r->line_modes, r->n_colors, b_lm, sizeof b_lm) ||
            json_str_array(r->mode_set, r->n_modes, b_ms, sizeof b_ms) ||
            json_str_array(r->operator_set, r->n_ops, b_os, sizeof b_os)) {
            ok = 0; break;
        }
        sqlite3_reset(ins);
        sqlite3_bind_text(ins, 1, r->cluster_uid, -1, SQLITE_STATIC);
        sqlite3_bind_text(ins, 2, r->name ? r->name : "", -1, SQLITE_STATIC);
        if (r->name_ja) sqlite3_bind_text(ins, 3, r->name_ja, -1, SQLITE_STATIC);
        else            sqlite3_bind_null(ins, 3);
        sqlite3_bind_double(ins, 4, r->lat);
        sqlite3_bind_double(ins, 5, r->lon);
        sqlite3_bind_text(ins, 6, b_mu, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 7, b_lc, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 8, b_ln, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 9, b_lr, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 10, b_lm, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 11, b_ms, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 12, b_os, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(ins) != SQLITE_DONE) { ok = 0; break; }
    }
    for (int i = 0; ok && i < ndots; i++) {
        dot_t *d = &dots[i];
        sqlite3_reset(insd);
        sqlite3_bind_text(insd, 1, d->cluster_uid, -1, SQLITE_STATIC);
        sqlite3_bind_text(insd, 2, d->way_uid, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insd, 3, d->line_color, -1, SQLITE_STATIC);
        sqlite3_bind_text(insd, 4, d->line_mode, -1, SQLITE_STATIC);
        sqlite3_bind_double(insd, 5, d->lon);
        sqlite3_bind_double(insd, 6, d->lat);
        if (sqlite3_step(insd) != SQLITE_DONE) { ok = 0; break; }
    }
    if (ins)  sqlite3_finalize(ins);
    if (insd) sqlite3_finalize(insd);

    if (ok) {
        if (sqlite3_exec(db->h, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
            sqlite3_exec(db->h, "ROLLBACK", NULL, NULL, NULL);
            return -1;
        }
        return 0;
    }
    sqlite3_exec(db->h, "ROLLBACK", NULL, NULL, NULL);
    return -1;
}

/* computeLineDots(clusterRows): per cluster × mode∈{train,subway},
 * snapAllWaysAt at the cluster centroid; way_uid = "<mode>:<color>:<k>"
 * with k a per-(mode,color) running index. Returns dot array (caller
 * frees each + array) via *out_dots, count via return; -1 on error.
 * On a per-mode index failure the JS just warns and continues — we mirror
 * that (skip that mode). */
static int compute_line_dots(db_handle *db, cluster_row_t *rows, int nrows,
                             dot_t **out_dots) {
    static const char *MODES[2] = { "train", "subway" };
    seg_index_t idx[2];
    int have[2] = { 0, 0 };
    for (int m = 0; m < 2; m++) {
        memset(&idx[m], 0, sizeof idx[m]);
        if (build_snap_index_for_mode(db, MODES[m], &idx[m]) == 0)
            have[m] = 1;
        else
            fprintf(stderr,
                "[stationClusterer] snap index for %s failed\n", MODES[m]);
    }

    dot_t *dots = NULL; int dn = 0, dcap = 0;
    int rc = 0;

    for (int i = 0; i < nrows && rc == 0; i++) {
        cluster_row_t *c = &rows[i];
        for (int m = 0; m < 2; m++) {
            if (!have[m]) continue;
            snap_t *snaps = NULL; int sn = 0, scap = 0;
            if (snap_all_ways_at(&idx[m], c->lon, c->lat,
                                 &snaps, &sn, &scap) < 0) {
                free(snaps); rc = -1; break;
            }
            /* per-(color) running index → way_uid suffix */
            for (int s = 0; s < sn; s++) {
                /* k = count of this color seen so far in this mode pass */
                int k = 0;
                for (int t = 0; t < s; t++)
                    if (strcmp(snaps[t].color, snaps[s].color) == 0) k++;
                if (dn == dcap) {
                    int nc = dcap ? dcap * 2 : 64;
                    dot_t *nd = realloc(dots, (size_t)nc * sizeof(dot_t));
                    if (!nd) { free(snaps); rc = -1; break; }
                    dots = nd; dcap = nc;
                }
                char wbuf[256];
                snprintf(wbuf, sizeof wbuf, "%s:%s:%d",
                         MODES[m], snaps[s].color, k);
                dot_t *d = &dots[dn++];
                d->cluster_uid = dup_or_null(c->cluster_uid);
                d->way_uid     = dup_or_null(wbuf);
                d->line_color  = dup_or_null(snaps[s].color);
                d->line_mode   = dup_or_null(MODES[m]);
                d->lon = snaps[s].lon;
                d->lat = snaps[s].lat;
            }
            free(snaps);
            if (rc) break;
        }
    }
    for (int m = 0; m < 2; m++) if (have[m]) seg_index_free(&idx[m]);

    if (rc != 0) {
        for (int i = 0; i < dn; i++) {
            free(dots[i].cluster_uid); free(dots[i].way_uid);
            free(dots[i].line_color);  free(dots[i].line_mode);
        }
        free(dots);
        *out_dots = NULL;
        return -1;
    }
    *out_dots = dots;
    return dn;
}

/* ════════════════ §7 runStationClusterer + read API ═══════════════════ */

int station_clusterer_run(db_handle *db) {
    int n = 0;
    station_t *S = load_stations(db, &n);
    if (n < 0) return -1;                 /* DB error */
    if (n == 0) { stations_free(S, 0); return 0; }

    dsu_t dsu;
    if (dsu_init(&dsu, n) != 0) { stations_free(S, n); return -1; }
    if (cluster_pass(db, S, n, &dsu) != 0) {
        dsu_free(&dsu); stations_free(S, n); return -1;
    }

    /* collect groups: root → member-index list, in ascending member order
     * (i goes 0..n-1 so each group's list is naturally sorted; member_uids
     * are re-sorted in materialise anyway). */
    smap_t root2grp;            /* "rootidx" → group slot */
    if (smap_init(&root2grp, n * 2 + 16) != 0) {
        dsu_free(&dsu); stations_free(S, n); return -1;
    }
    int **groups = NULL; int *gsz = NULL, *gcap = NULL;
    int ng = 0, gscap = 0;
    char (*rootkeys)[16] = malloc((size_t)n * sizeof *rootkeys);
    if (!rootkeys) {
        smap_free(&root2grp); dsu_free(&dsu); stations_free(S, n); return -1;
    }
    int fail = 0;
    for (int i = 0; i < n && !fail; i++) {
        int r = dsu_find(&dsu, i);
        char rk[16]; snprintf(rk, sizeof rk, "%d", r);
        int slot = smap_get(&root2grp, rk);
        if (slot < 0) {
            if (ng == gscap) {
                int nc = gscap ? gscap * 2 : 64;
                int **ng2 = realloc(groups, (size_t)nc * sizeof(int *));
                int  *gs2 = realloc(gsz,    (size_t)nc * sizeof(int));
                int  *gc2 = realloc(gcap,   (size_t)nc * sizeof(int));
                if (!ng2 || !gs2 || !gc2) {
                    free(ng2 ? ng2 : groups);
                    /* gs2/gc2 freed via originals below */
                    fail = 1; break;
                }
                groups = ng2; gsz = gs2; gcap = gc2; gscap = nc;
            }
            slot = ng++;
            groups[slot] = NULL; gsz[slot] = 0; gcap[slot] = 0;
            memcpy(rootkeys[slot < n ? slot : 0], rk, sizeof rk);
            smap_put_if_absent(&root2grp, rootkeys[slot], slot);
        }
        if (gsz[slot] == gcap[slot]) {
            int nc = gcap[slot] ? gcap[slot] * 2 : 4;
            int *ni = realloc(groups[slot], (size_t)nc * sizeof(int));
            if (!ni) { fail = 1; break; }
            groups[slot] = ni; gcap[slot] = nc;
        }
        groups[slot][gsz[slot]++] = i;
    }
    smap_free(&root2grp);
    dsu_free(&dsu);

    int result = -1;
    cluster_row_t *rows = NULL;
    if (!fail) {
        rows = malloc((size_t)ng * sizeof(cluster_row_t));
        if (!rows) fail = 1;
    }
    int built = 0;
    for (int g = 0; g < ng && !fail; g++) {
        if (materialise(S, groups[g], gsz[g], &rows[g]) != 0) { fail = 1; break; }
        built++;
    }

    dot_t *dots = NULL; int ndots = 0;
    if (!fail) {
        /* line dots — JS wraps in try/catch: a failure just warns and the
         * cluster table is still written with zero dots. */
        int dc = compute_line_dots(db, rows, ng, &dots);
        if (dc < 0) {
            fprintf(stderr, "[stationClusterer] line dots failed\n");
            dots = NULL; ndots = 0;
        } else {
            ndots = dc;
        }
        if (write_all_tx(db, rows, ng, dots, ndots) == 0)
            result = ng;
        else
            result = -1;
    }

    for (int i = 0; i < ndots; i++) {
        free(dots[i].cluster_uid); free(dots[i].way_uid);
        free(dots[i].line_color);  free(dots[i].line_mode);
    }
    free(dots);
    for (int g = 0; g < built; g++) cluster_row_free(&rows[g]);
    free(rows);
    for (int g = 0; g < ng; g++) free(groups[g]);
    free(groups); free(gsz); free(gcap); free(rootkeys);
    stations_free(S, n);
    return result;
}

/* getAllLineDotFeatures(): FeatureCollection of one Point per row of
 * station_line_dots LEFT JOIN station_clusters. */
char *station_clusterer_linedots_fc_json(db_handle *db) {
    cJSON *fc = cJSON_CreateObject();
    if (!fc) return NULL;
    cJSON_AddStringToObject(fc, "type", "FeatureCollection");
    cJSON *feats = cJSON_AddArrayToObject(fc, "features");
    if (!feats) { cJSON_Delete(fc); return NULL; }

    static const char *Q =
      "SELECT d.cluster_uid, d.way_uid, d.line_color, d.line_mode, "
      "       d.lon, d.lat, c.name, c.name_ja "
      "FROM station_line_dots d "
      "LEFT JOIN station_clusters c ON c.cluster_uid = d.cluster_uid";
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db->h, Q, -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *cu = (const char *)sqlite3_column_text(st, 0);
            const char *wu = (const char *)sqlite3_column_text(st, 1);
            const char *lc = (const char *)sqlite3_column_text(st, 2);
            const char *lm = (const char *)sqlite3_column_text(st, 3);
            double lon = sqlite3_column_double(st, 4);
            double lat = sqlite3_column_double(st, 5);
            int name_null = sqlite3_column_type(st, 6) == SQLITE_NULL;
            int nja_null  = sqlite3_column_type(st, 7) == SQLITE_NULL;
            const char *nm  = (const char *)sqlite3_column_text(st, 6);
            const char *nja = (const char *)sqlite3_column_text(st, 7);

            cJSON *f = cJSON_CreateObject();
            cJSON_AddStringToObject(f, "type", "Feature");
            cJSON *g = cJSON_AddObjectToObject(f, "geometry");
            cJSON_AddStringToObject(g, "type", "Point");
            cJSON *co = cJSON_AddArrayToObject(g, "coordinates");
            cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
            cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
            cJSON *p = cJSON_AddObjectToObject(f, "properties");
            cJSON_AddStringToObject(p, "cluster_uid", cu ? cu : "");
            cJSON_AddStringToObject(p, "station_uid", cu ? cu : "");
            cJSON_AddStringToObject(p, "way_uid", wu ? wu : "");
            if (name_null) cJSON_AddNullToObject(p, "name");
            else cJSON_AddStringToObject(p, "name", nm ? nm : "");
            if (nja_null) cJSON_AddNullToObject(p, "name_ja");
            else cJSON_AddStringToObject(p, "name_ja", nja ? nja : "");
            cJSON_AddStringToObject(p, "line_color", lc ? lc : "");
            cJSON_AddStringToObject(p, "line_mode", lm ? lm : "");
            cJSON_AddItemToArray(feats, f);
        }
        sqlite3_finalize(st);
    }
    char *js = cJSON_PrintUnformatted(fc);
    cJSON_Delete(fc);
    return js;
}

/* ════════════════ §8 snapStationsToNearestLine ════════════════════════ */

/* getStationsByMode (raw, for snap): Point rows of the mode's source_id;
 * we need station_uid (from properties) + coordinates. */
typedef struct { char *uid; double lon, lat; } snapsta_t;

static snapsta_t *load_snap_stations(db_handle *db, const char *mode,
                                     int *out_n) {
    const char *source_id =
        (strcmp(mode, "train") == 0)  ? "unified-trains"  :
        (strcmp(mode, "subway") == 0) ? "unified-subways" : NULL;
    if (!source_id) { *out_n = 0; return NULL; }

    static const char *Q =
      "SELECT lon, lat, json_extract(properties, '$.station_uid') "
      "FROM intel_items "
      "WHERE source_id = ?1 AND lat IS NOT NULL "
      "  AND (geometry IS NULL OR json_extract(geometry,'$.type')='Point')";
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db->h, Q, -1, &st, NULL) != SQLITE_OK) {
        *out_n = -1; return NULL;
    }
    sqlite3_bind_text(st, 1, source_id, -1, SQLITE_STATIC);

    int cap = 256, n = 0;
    snapsta_t *a = malloc((size_t)cap * sizeof(snapsta_t));
    if (!a) { sqlite3_finalize(st); *out_n = -1; return NULL; }
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (n == cap) {
            int nc = cap * 2;
            snapsta_t *na = realloc(a, (size_t)nc * sizeof(snapsta_t));
            if (!na) { for (int i=0;i<n;i++) free(a[i].uid); free(a);
                       sqlite3_finalize(st); *out_n=-1; return NULL; }
            a = na; cap = nc;
        }
        a[n].lon = sqlite3_column_double(st, 0);
        a[n].lat = sqlite3_column_double(st, 1);
        const char *u = (const char *)sqlite3_column_text(st, 2);
        a[n].uid = dup_or_null(u);
        n++;
    }
    sqlite3_finalize(st);
    *out_n = n;
    return a;
}

/* updateStationColorsTx-equivalent: patch one station's
 * properties.line_color + properties.line_colors[] by station_uid (uid LIKE
 * '%|<station_uid>'); skip the write when both already equal (no-op like
 * the JS). Caller wraps in a tx. Returns 1 if a row was changed, else 0. */
static int update_one_station_color(db_handle *db, const char *station_uid,
                                    const char *color,
                                    char **colors, int ncolors) {
    static const char *SEL =
      "SELECT uid, properties FROM intel_items "
      "WHERE source_id IN ('unified-trains','unified-subways') "
      "  AND uid LIKE ?1";
    char like[512];
    snprintf(like, sizeof like, "%%|%s", station_uid);

    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h, SEL, -1, &s, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(s, 1, like, -1, SQLITE_TRANSIENT);

    int changed = 0;
    while (sqlite3_step(s) == SQLITE_ROW) {
        const char *uid = (const char *)sqlite3_column_text(s, 0);
        const char *pj  = (const char *)sqlite3_column_text(s, 1);
        if (!uid) continue;
        cJSON *p = (pj && *pj) ? cJSON_Parse(pj) : NULL;
        if (!p) p = cJSON_CreateObject();

        /* no-op check: same line_color AND same line_colors[] */
        cJSON *cur = cJSON_GetObjectItem(p, "line_color");
        int same_c = cur && cJSON_IsString(cur) && cur->valuestring &&
                     strcmp(cur->valuestring, color) == 0;
        cJSON *curArr = cJSON_GetObjectItem(p, "line_colors");
        int same_arr = 0;
        if (curArr && cJSON_IsArray(curArr) &&
            cJSON_GetArraySize(curArr) == ncolors) {
            same_arr = 1;
            for (int i = 0; i < ncolors; i++) {
                cJSON *e = cJSON_GetArrayItem(curArr, i);
                if (!e || !cJSON_IsString(e) || !e->valuestring ||
                    strcmp(e->valuestring, colors[i]) != 0) { same_arr = 0; break; }
            }
        }
        if (same_c && same_arr) { cJSON_Delete(p); continue; }

        cJSON_DeleteItemFromObject(p, "line_color");
        cJSON_AddStringToObject(p, "line_color", color);
        cJSON_DeleteItemFromObject(p, "line_colors");
        cJSON *na = cJSON_AddArrayToObject(p, "line_colors");
        for (int i = 0; i < ncolors; i++)
            cJSON_AddItemToArray(na, cJSON_CreateString(colors[i]));

        char *np = cJSON_PrintUnformatted(p);
        cJSON_Delete(p);
        if (!np) continue;

        sqlite3_stmt *u;
        if (sqlite3_prepare_v2(db->h,
              "UPDATE intel_items SET properties=?1 WHERE uid=?2",
              -1, &u, NULL) == SQLITE_OK) {
            sqlite3_bind_text(u, 1, np, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(u, 2, uid, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(u) == SQLITE_DONE && sqlite3_changes(db->h) > 0)
                changed = 1;
            sqlite3_finalize(u);
        }
        free(np);
    }
    sqlite3_finalize(s);
    return changed;
}

int station_snap_stations(db_handle *db, const char *mode) {
    if (!mode || (strcmp(mode, "train") && strcmp(mode, "subway")))
        return -1;

    /* lines (colored) → segment index. */
    cJSON *lines = load_lines_for_mode(db, mode);
    if (!lines) return -1;
    seg_index_t ix; memset(&ix, 0, sizeof ix);
    const double radiusSq = DEFAULT_RADIUS_M * DEFAULT_RADIUS_M;
    ix.radiusSqM = radiusSq;
    if (build_segment_index(&ix, lines) != 0) {
        cJSON_Delete(lines); seg_index_free(&ix); return -1;
    }
    cJSON_Delete(lines);

    int sn = 0;
    snapsta_t *sta = load_snap_stations(db, mode, &sn);
    if (sn < 0) { seg_index_free(&ix); return -1; }

    /* per-station: scan 3×3, overall-nearest color + per-color nearest
     * within radius; emit color + up-to-5 colors sorted by distance. */
    typedef struct { char *uid, *color; char **colors; int ncolors; } upd_t;
    upd_t *U = NULL; int un = 0, ucap = 0;

    for (int s = 0; s < sn; s++) {
        double lon = sta[s].lon, lat = sta[s].lat;
        long cx = cell_of(lon, SNAP_CELL_SIZE_DEG);
        long cy = cell_of(lat, SNAP_CELL_SIZE_DEG);
        double bestDsq = INFINITY;
        char  *bestColor = NULL;
        /* color → nearest dsq (first-seen order, like JS Map). */
        colorbest_t *CB = NULL; int cbn = 0, cbcap = 0;

        for (int dx = -1; dx <= 1; dx++)
          for (int dy = -1; dy <= 1; dy++) {
            long qx = cx + dx, qy = cy + dy;
            seg_cell_t *c;
            for (c = ix.b[cell_hash(qx, qy)]; c; c = c->next)
                if (c->cx == qx && c->cy == qy) break;
            if (!c) continue;
            for (int t = 0; t < c->n; t++) {
                seg_t *sg = c->segs[t];
                double dsq = segment_dist_sq_m(lon, lat, sg->ax, sg->ay,
                                               sg->bx, sg->by);
                if (dsq < bestDsq) { bestDsq = dsq; bestColor = sg->color; }
                if (dsq <= radiusSq) {
                    int fi = -1;
                    for (int z = 0; z < cbn; z++)
                        if (strcmp(CB[z].c, sg->color) == 0) { fi = z; break; }
                    if (fi < 0) {
                        if (cbn == cbcap) {
                            int nc = cbcap ? cbcap * 2 : 8;
                            void *np = realloc(CB, (size_t)nc * sizeof *CB);
                            if (!np) { free(CB); goto station_oom; }
                            CB = np; cbcap = nc;
                        }
                        CB[cbn].c = sg->color; CB[cbn].dsq = dsq; cbn++;
                    } else if (dsq < CB[fi].dsq) {
                        CB[fi].dsq = dsq;
                    }
                }
            }
          }

        if (bestColor && bestDsq <= radiusSq && sta[s].uid && sta[s].uid[0]) {
            /* stable sort CB by dsq asc, take first 5 colors. */
            for (int a = 1; a < cbn; a++) {
                colorbest_t key = CB[a];
                int b = a - 1;
                while (b >= 0 && CB[b].dsq > key.dsq) { CB[b+1] = CB[b]; b--; }
                CB[b+1] = key;
            }
            int keep = cbn < MAX_LINE_COLORS_KEEP ? cbn : MAX_LINE_COLORS_KEEP;
            if (un == ucap) {
                int nc = ucap ? ucap * 2 : 64;
                upd_t *nu = realloc(U, (size_t)nc * sizeof(upd_t));
                if (!nu) { free(CB); goto station_oom; }
                U = nu; ucap = nc;
            }
            U[un].uid = dup_or_null(sta[s].uid);
            U[un].color = dup_or_null(bestColor);
            U[un].colors = malloc((size_t)(keep > 0 ? keep : 1) * sizeof(char *));
            U[un].ncolors = keep;
            for (int i = 0; i < keep; i++) U[un].colors[i] = dup_or_null(CB[i].c);
            un++;
        }
        free(CB);
    }

    /* one transactional batch of per-station UPDATEs. */
    int changed = 0;
    if (un > 0) {
        if (sqlite3_exec(db->h, "BEGIN IMMEDIATE", NULL, NULL, NULL)
                == SQLITE_OK) {
            for (int i = 0; i < un; i++)
                changed += update_one_station_color(db, U[i].uid, U[i].color,
                                                    U[i].colors, U[i].ncolors);
            sqlite3_exec(db->h, "COMMIT", NULL, NULL, NULL);
        }
    }

    for (int i = 0; i < un; i++) {
        free(U[i].uid); free(U[i].color);
        for (int k = 0; k < U[i].ncolors; k++) free(U[i].colors[k]);
        free(U[i].colors);
    }
    free(U);
    for (int i = 0; i < sn; i++) free(sta[i].uid);
    free(sta);
    seg_index_free(&ix);
    return changed;

station_oom:
    for (int i = 0; i < un; i++) {
        free(U[i].uid); free(U[i].color);
        for (int k = 0; k < U[i].ncolors; k++) free(U[i].colors[k]);
        free(U[i].colors);
    }
    free(U);
    for (int i = 0; i < sn; i++) free(sta[i].uid);
    free(sta);
    seg_index_free(&ix);
    return -1;
}
