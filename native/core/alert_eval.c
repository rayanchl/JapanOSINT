#include "alert_eval.h"
#include "fts.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/rand.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* How often the cache may re-probe alert_rules for a generation change. The
 * cost of staleness is bounded: a rule edited now starts matching within this
 * many seconds. Mute/dedup/cap are re-read at fire time, so this delay only
 * affects which predicates are evaluated, never whether a fire is suppressed. */
#define CACHE_RECHECK_SEC 3
#define PREVIEW_MAX_DAYS  30
#define PREVIEW_SAMPLES   50
#define PREVIEW_SCAN_CAP  50000

/* Item 9. A ring past this many vertices is a traced coastline, not a
 * geofence, and it would be walked once per candidate point on the ingest
 * hot path. Same cap on the write side (aoiapi.c) and here, because they are
 * the same function. */
#define SHAPE_MAX_VERTS   10000

/* Item 21. Bound on how many mentions of ONE item are pulled into memory for
 * the entity terms. An item with more distinct entities than this is a
 * pathological extraction, and truncating is the fail-safe direction: worst
 * case a watchlist misses a match on that one item, versus the alternative of
 * an unbounded allocation on the ingest path. */
#define FACT_ENT_CAP      2000

/* Item 21 sweep. LAG_SEC: entity_mentions.created_at is second-resolution, so
 * the upper bound of a pass must sit strictly in the past — otherwise a row
 * written later in the SAME second as the watermark lands permanently behind
 * it and is never swept. BATCH × MAX_BATCH caps one pass at ~10k mentions so
 * a backfill cannot monopolise the connection. */
#define SWEEP_DEFAULT_SEC 30
#define SWEEP_LAG_SEC     2
#define SWEEP_BATCH       500
#define SWEEP_MAX_BATCH   20
#define SWEEP_CURSOR_KEY  "entity_mentions"
#define SWEEP_ROWID_MAX   9223372036854775807LL

#define JO_PI         3.14159265358979323846
#define JO_EARTH_R_M  6371008.8              /* IUGG mean Earth radius */

static void uuid4(char out[37]) {
  unsigned char b[16]; RAND_bytes(b, 16);
  b[6] = (b[6] & 0x0F) | 0x40; b[8] = (b[8] & 0x3F) | 0x80;
  snprintf(out, 37,
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
    b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
}
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
/* Tags and source ids are authored by hand in two places (the rule form and
 * the collector), so compare them case-insensitively — "Tokyo" vs "tokyo" is
 * a typo, not a different tag. Entity ids and types go through the same
 * comparator: ids are generated hex and types are a fixed lowercase
 * vocabulary, so case-folding them costs nothing and forgives a hand-typed
 * "Person". */
static int ieq(const char *a, const char *b) {
  if (!a || !b) return 0;
  for (; *a && *b; a++, b++)
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
  return *a == *b;
}

/* ── string lists ─────────────────────────────────────────────────────────── */

typedef struct { char **v; int n; int cap; } strlist;

static void strlist_free(strlist *l) {
  for (int i = 0; i < l->n; i++) free(l->v[i]);
  free(l->v); l->v = NULL; l->n = 0; l->cap = 0;
}
static int strlist_push(strlist *l, const char *v) {
  if (!v) return 0;
  if (l->n == l->cap) {
    int nc = l->cap ? l->cap * 2 : 16;
    char **nv = realloc(l->v, (size_t)nc * sizeof *nv);
    if (!nv) return 0;
    l->v = nv; l->cap = nc;
  }
  char *d = strdup(v);
  if (!d) return 0;
  l->v[l->n++] = d;
  return 1;
}
static void strlist_from_array(strlist *l, const cJSON *arr) {
  l->v = NULL; l->n = 0; l->cap = 0;
  if (!arr || !cJSON_IsArray(arr)) return;
  int cap = cJSON_GetArraySize(arr);
  if (cap <= 0) return;
  l->v = calloc((size_t)cap, sizeof *l->v);
  if (!l->v) return;
  l->cap = cap;
  const cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    if (cJSON_IsString(e) && e->valuestring && e->valuestring[0]) {
      char *d = strdup(e->valuestring);
      if (d) l->v[l->n++] = d;
    }
  }
}
static void strlist_from_json_text(strlist *l, const char *json) {
  l->v = NULL; l->n = 0; l->cap = 0;
  if (!json || !*json) return;
  cJSON *root = cJSON_Parse(json);
  if (!root) return;
  strlist_from_array(l, root);
  cJSON_Delete(root);
}
static int strlist_has(const strlist *l, const char *v) {
  for (int i = 0; i < l->n; i++) if (ieq(l->v[i], v)) return 1;
  return 0;
}
/* "does the item's set intersect the rule's list" — the any-of shape shared
 * by tags_any, entity_ids and entity_types. */
static int strlist_any(const strlist *want, const strlist *have) {
  for (int i = 0; i < want->n; i++) if (strlist_has(have, want->v[i])) return 1;
  return 0;
}

/* ── item 9: shapes ───────────────────────────────────────────────────────── */

typedef enum { SH_NONE = 0, SH_BBOX, SH_POLY, SH_CIRCLE } sh_kind;

typedef struct {
  int     kind;
  /* Prefilter box, ALWAYS populated. bw > be encodes an antimeridian
   * crossing, which is exactly what the legacy bbox test already meant. */
  double  bw, bs, be, bn;
  double *ring;  int nring;          /* SH_POLY: 2*nring lon/lat, closed */
  double  clat, clon, crad_m;        /* SH_CIRCLE */
} jo_shape;

static void shape_free(jo_shape *g) {
  free(g->ring); g->ring = NULL; g->nring = 0; g->kind = SH_NONE;
}
static int lat_ok(double v) { return isfinite(v) && v >= -90.0  && v <= 90.0;  }
static int lon_ok(double v) { return isfinite(v) && v >= -180.0 && v <= 180.0; }

/* Parse ONE shape out of an already-parsed cJSON value. Returns NULL on
 * success (and owns nothing the caller has to free beyond shape_free), else a
 * static reason string. On every failure path `out` is left shape_free-safe.
 *
 * This is the strict validator, used for polygon/circle/AOI geometry. The
 * legacy predicate `bbox` term deliberately does NOT come through here — see
 * pred_compile(). */
static const char *shape_parse(const char *kind, const cJSON *g, jo_shape *out) {
  memset(out, 0, sizeof *out);
  if (!kind || !*kind)            return "geometry kind required";
  if (!g || cJSON_IsNull(g))      return "geometry required";

  if (strcmp(kind, "bbox") == 0) {
    if (!cJSON_IsArray(g) || cJSON_GetArraySize(g) != 4)
      return "bbox must be [w,s,e,n]";
    double v[4];
    for (int i = 0; i < 4; i++) {
      const cJSON *e = cJSON_GetArrayItem(g, i);
      if (!e || !cJSON_IsNumber(e)) return "bbox must be four numbers";
      v[i] = e->valuedouble;
    }
    if (!lon_ok(v[0]) || !lon_ok(v[2])) return "bbox longitude out of range";
    if (!lat_ok(v[1]) || !lat_ok(v[3])) return "bbox latitude out of range";
    /* w>e is legal (antimeridian); s>n is not — it is an empty box, i.e. a
     * geofence that can never fire, which is always an authoring mistake. */
    if (v[1] > v[3]) return "bbox south is above north";
    out->kind = SH_BBOX;
    out->bw = v[0]; out->bs = v[1]; out->be = v[2]; out->bn = v[3];
    return NULL;
  }

  if (strcmp(kind, "polygon") == 0) {
    if (!cJSON_IsArray(g)) return "polygon must be an array of [lon,lat]";
    int n = cJSON_GetArraySize(g);
    if (n < 3)               return "polygon needs at least 3 points";
    if (n > SHAPE_MAX_VERTS) return "polygon has too many vertices (max 10000)";
    double *r = malloc((size_t)(n + 1) * 2 * sizeof *r);
    if (!r) return "server error";
    int k = 0;
    for (int i = 0; i < n; i++) {
      const cJSON *pt = cJSON_GetArrayItem(g, i);
      const cJSON *jx = (pt && cJSON_IsArray(pt) && cJSON_GetArraySize(pt) == 2)
                          ? cJSON_GetArrayItem(pt, 0) : NULL;
      const cJSON *jy = jx ? cJSON_GetArrayItem(pt, 1) : NULL;
      if (!jx || !jy || !cJSON_IsNumber(jx) || !cJSON_IsNumber(jy)) {
        free(r); return "polygon point must be [lon,lat]";
      }
      if (!lon_ok(jx->valuedouble) || !lat_ok(jy->valuedouble)) {
        free(r); return "polygon point out of range";
      }
      r[2 * k] = jx->valuedouble; r[2 * k + 1] = jy->valuedouble; k++;
    }
    /* Auto-close: ray casting treats the ring as closed anyway, but carrying
     * the closing vertex explicitly keeps the stored geometry and the walked
     * geometry identical, so a round-trip through the AOI table cannot change
     * the answer. */
    if (r[0] != r[2 * (k - 1)] || r[1] != r[2 * (k - 1) + 1]) {
      r[2 * k] = r[0]; r[2 * k + 1] = r[1]; k++;
    }
    if (k < 4) { free(r); return "polygon is degenerate"; }
    /* Shoelace: zero signed area means every vertex is collinear — a line
     * dressed as an area, which would silently never match. */
    double area2 = 0;
    for (int i = 0, j = k - 1; i < k; j = i++)
      area2 += r[2 * j] * r[2 * i + 1] - r[2 * i] * r[2 * j + 1];
    if (area2 == 0.0) { free(r); return "polygon is degenerate"; }

    double w = r[0], e = r[0], s = r[1], nn = r[1];
    for (int i = 1; i < k; i++) {
      if (r[2 * i]     < w)  w  = r[2 * i];
      if (r[2 * i]     > e)  e  = r[2 * i];
      if (r[2 * i + 1] < s)  s  = r[2 * i + 1];
      if (r[2 * i + 1] > nn) nn = r[2 * i + 1];
    }
    out->kind = SH_POLY; out->ring = r; out->nring = k;
    out->bw = w; out->bs = s; out->be = e; out->bn = nn;
    return NULL;
  }

  if (strcmp(kind, "circle") == 0) {
    if (!cJSON_IsObject(g)) return "circle must be {lat,lon,radius_m}";
    const cJSON *jl = cJSON_GetObjectItem(g, "lat");
    const cJSON *jo = cJSON_GetObjectItem(g, "lon");
    const cJSON *jr = cJSON_GetObjectItem(g, "radius_m");
    if (!cJSON_IsNumber(jl) || !cJSON_IsNumber(jo) || !cJSON_IsNumber(jr))
      return "circle must be {lat,lon,radius_m}";
    if (!lat_ok(jl->valuedouble)) return "circle latitude out of range";
    if (!lon_ok(jo->valuedouble)) return "circle longitude out of range";
    double rad = jr->valuedouble;
    /* Half the Earth's circumference: past that the cap is the whole sphere
     * and the "geofence" is a no-op with a misleading name. */
    if (!isfinite(rad) || rad <= 0.0 || rad > 20015086.0)
      return "circle radius_m must be >0 and <=20015086";
    out->kind = SH_CIRCLE;
    out->clat = jl->valuedouble; out->clon = jo->valuedouble; out->crad_m = rad;

    double dlat = (rad / JO_EARTH_R_M) * (180.0 / JO_PI);
    out->bs = out->clat - dlat;
    out->bn = out->clat + dlat;
    /* Widen longitude using the cosine of the cap's POLE-MOST latitude, not
     * its centre: a cap's widest longitudinal span is at the edge nearest the
     * pole, and a prefilter narrower than the shape it guards drops true
     * matches. Over-wide is free (the exact haversine test runs next). */
    double edge = fabs(out->bs) > fabs(out->bn) ? fabs(out->bs) : fabs(out->bn);
    double cl = cos(edge * JO_PI / 180.0);
    if (out->bs <= -90.0 || out->bn >= 90.0 || cl <= 1e-9) {
      if (out->bs < -90.0) out->bs = -90.0;
      if (out->bn >  90.0) out->bn =  90.0;
      out->bw = -180.0; out->be = 180.0;      /* cap contains a pole */
    } else {
      double dlon = dlat / cl;
      if (dlon >= 180.0) { out->bw = -180.0; out->be = 180.0; }
      else {
        out->bw = out->clon - dlon;
        out->be = out->clon + dlon;
        /* Fold back into [-180,180]; bw>be then means "wraps", which is the
         * encoding bbox_hit() already understands. */
        if (out->bw < -180.0) out->bw += 360.0;
        if (out->be >  180.0) out->be -= 360.0;
      }
    }
    return NULL;
  }
  return "unknown geometry kind";
}

/* The cheap gate. Byte-for-byte the test the bbox-only evaluator used to run,
 * including the w>e antimeridian branch — every shape now reuses it as its
 * prefilter, which is what keeps the hot path a pair of comparisons. */
static int bbox_hit(const jo_shape *g, double lat, double lon) {
  if (lat < g->bs || lat > g->bn) return 0;
  return (g->bw <= g->be) ? (lon >= g->bw && lon <= g->be)
                          : (lon >= g->bw || lon <= g->be);
}
/* Even-odd ray casting. The (yi>lat)!=(yj>lat) guard is what makes the
 * division safe: it can only be reached when the edge actually straddles the
 * scan line, so yj-yi is non-zero. */
static int point_in_ring(const double *r, int n, double lon, double lat) {
  int inside = 0;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    double xi = r[2 * i], yi = r[2 * i + 1];
    double xj = r[2 * j], yj = r[2 * j + 1];
    if (((yi > lat) != (yj > lat)) &&
        (lon < (xj - xi) * (lat - yi) / (yj - yi) + xi))
      inside = !inside;
  }
  return inside;
}
static double haversine_m(double la1, double lo1, double la2, double lo2) {
  double p = JO_PI / 180.0;
  double sa = sin((la2 - la1) * p / 2.0), so = sin((lo2 - lo1) * p / 2.0);
  double a = sa * sa + cos(la1 * p) * cos(la2 * p) * so * so;
  if (a > 1.0) a = 1.0;                    /* rounding guard for antipodes */
  return 2.0 * JO_EARTH_R_M * asin(sqrt(a));
}
static int shape_contains(const jo_shape *g, double lat, double lon) {
  if (!bbox_hit(g, lat, lon)) return 0;          /* cheap test first, always */
  if (g->kind == SH_POLY)   return point_in_ring(g->ring, g->nring, lon, lat);
  if (g->kind == SH_CIRCLE)
    return haversine_m(g->clat, g->clon, lat, lon) <= g->crad_m;
  return 1;                                      /* SH_BBOX: the box IS it */
}

const char *alert_eval_shape_check(const char *kind, const char *geometry_json,
                                   double *w, double *s, double *e, double *n) {
  if (!kind || !*kind) return "geometry kind required";
  if (!geometry_json || !*geometry_json) return "geometry required";
  cJSON *g = cJSON_Parse(geometry_json);
  if (!g) return "geometry is not valid JSON";
  jo_shape sh;
  const char *why = shape_parse(kind, g, &sh);
  cJSON_Delete(g);
  if (!why) {
    if (w) *w = sh.bw;
    if (s) *s = sh.bs;
    if (e) *e = sh.be;
    if (n) *n = sh.bn;
  }
  shape_free(&sh);
  return why;
}

/* ── compiled predicate ───────────────────────────────────────────────────── */

typedef struct {
  int      usable;    /* 0 → never matches (bad JSON / malformed constraint) */
  int      llm_only;  /* mode=="llm" + nl_query → not evaluable inline       */
  char    *fts_q;     /* MeCab-segmented MATCH expression; NULL when absent  */
  strlist  src, tany, tall;
  jo_shape geo;                     /* item 9; kind==SH_NONE → no spatial term */
  strlist  eids, etypes;            /* item 21                                 */
  int      has_ent;                 /* eids.n || etypes.n                      */
} jo_pred;

static void pred_free(jo_pred *p) {
  free(p->fts_q); p->fts_q = NULL;
  strlist_free(&p->src); strlist_free(&p->tany); strlist_free(&p->tall);
  strlist_free(&p->eids); strlist_free(&p->etypes);
  shape_free(&p->geo);
  p->usable = 0; p->has_ent = 0;
}

/* Resolve predicate.aoi_id into a compiled shape. TENANT-SCOPED and fails
 * closed: an id belonging to another tenant, a deleted id, or a row whose
 * geometry no longer parses all return 0, which makes the whole rule
 * unusable. Widening a geofence to "everywhere" because its shape went missing
 * is the one outcome that must never happen here. */
static int aoi_resolve(db_handle *db, const char *tenant_id, const cJSON *ja,
                       jo_shape *out) {
  memset(out, 0, sizeof *out);
  if (!ja || !cJSON_IsString(ja) || !ja->valuestring[0]) return 0;
  if (!db || !db->h || !tenant_id || !*tenant_id) return 0;
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT kind, geometry_json FROM areas_of_interest"
        " WHERE id=?1 AND tenant_id=?2", -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, ja->valuestring, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant_id,       -1, SQLITE_TRANSIENT);
  int ok = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *k = ctext(s, 0), *gj = ctext(s, 1);
    if (k && gj) {
      cJSON *g = cJSON_Parse(gj);
      if (g) { ok = (shape_parse(k, g, out) == NULL); cJSON_Delete(g); }
    }
  }
  sqlite3_finalize(s);
  if (!ok) shape_free(out);
  return ok;
}

/* NULL/"" or "{}" compiles to a match-everything predicate (scoped to the
 * tenant by the caller). Returns p->usable; the caller must pred_free() on
 * BOTH outcomes — a failed compile may still own the pieces it parsed.
 *
 * `db`+`tenant_id` exist only for aoi_id resolution; both callers already
 * know the owning tenant (ruleset_load from the rule row, preview from the
 * request), so no path can resolve an AOI under the wrong tenant. */
static int pred_compile(db_handle *db, const char *tenant_id,
                        const char *json, jo_pred *p) {
  memset(p, 0, sizeof *p);
  p->usable = 1;
  if (!json || !*json) return 1;
  cJSON *root = cJSON_Parse(json);
  if (!root) { p->usable = 0; return 0; }
  if (!cJSON_IsObject(root)) { cJSON_Delete(root); p->usable = 0; return 0; }

  cJSON *x = cJSON_GetObjectItem(root, "mode");
  if (x && cJSON_IsString(x) && strcmp(x->valuestring, "llm") == 0) {
    cJSON *nl = cJSON_GetObjectItem(root, "nl_query");
    if (nl && cJSON_IsString(nl) && nl->valuestring[0]) p->llm_only = 1;
  }
  x = cJSON_GetObjectItem(root, "q");
  if (x && cJSON_IsString(x) && x->valuestring[0]) {
    /* Segmented exactly like the write path and /api/intel/items?q=, so the
     * same words tokenize the same way on both sides of the index. */
    p->fts_q = fts_segment(x->valuestring);
    if (!p->fts_q) p->usable = 0;            /* fail closed, never match-all */
  }
  /* An empty array is "no constraint", matching how the rule form serialises
   * an untouched field; only a non-empty list narrows. */
  strlist_from_array(&p->src,  cJSON_GetObjectItem(root, "source_ids"));
  strlist_from_array(&p->tany, cJSON_GetObjectItem(root, "tags_any"));
  strlist_from_array(&p->tall, cJSON_GetObjectItem(root, "tags_all"));

  /* ── item 9: exactly one spatial term ───────────────────────────────── */
  cJSON *sb = cJSON_GetObjectItem(root, "bbox");
  cJSON *sp = cJSON_GetObjectItem(root, "polygon");
  cJSON *sc = cJSON_GetObjectItem(root, "circle");
  cJSON *sa = cJSON_GetObjectItem(root, "aoi_id");
  if (sb && cJSON_IsNull(sb)) sb = NULL;
  if (sp && cJSON_IsNull(sp)) sp = NULL;
  if (sc && cJSON_IsNull(sc)) sc = NULL;
  if (sa && cJSON_IsNull(sa)) sa = NULL;
  if ((!!sb + !!sp + !!sc + !!sa) > 1) {
    /* Two spatial terms is never "the intersection of both" to whoever typed
     * it, and guessing which one they meant is worse than not firing. */
    p->usable = 0;
  } else if (sb) {
    /* LEGACY PARSER, PRESERVED VERBATIM. `bbox` is the one spatial term with
     * deployed rules behind it, so it keeps its original acceptance set —
     * four numbers, no range or finiteness checks, w>e means antimeridian.
     * The strict shape_parse("bbox", …) path exists for AOI rows, which are
     * validated at write time and have no legacy to preserve. */
    double v[4]; int ok = cJSON_IsArray(sb) && cJSON_GetArraySize(sb) == 4;
    for (int i = 0; ok && i < 4; i++) {
      cJSON *e = cJSON_GetArrayItem(sb, i);
      if (e && cJSON_IsNumber(e)) v[i] = e->valuedouble; else ok = 0;
    }
    if (ok) {
      p->geo.kind = SH_BBOX;
      p->geo.bw = v[0]; p->geo.bs = v[1]; p->geo.be = v[2]; p->geo.bn = v[3];
    } else p->usable = 0;
  } else if (sp) {
    if (shape_parse("polygon", sp, &p->geo) != NULL) p->usable = 0;
  } else if (sc) {
    if (shape_parse("circle", sc, &p->geo) != NULL) p->usable = 0;
  } else if (sa) {
    if (!aoi_resolve(db, tenant_id, sa, &p->geo)) p->usable = 0;
  }

  /* ── item 21: entity terms ──────────────────────────────────────────── */
  x = cJSON_GetObjectItem(root, "entity_ids");
  if (x && !cJSON_IsNull(x)) {
    if (!cJSON_IsArray(x)) p->usable = 0;
    else strlist_from_array(&p->eids, x);
  }
  x = cJSON_GetObjectItem(root, "entity_types");
  if (x && !cJSON_IsNull(x)) {
    if (!cJSON_IsArray(x)) p->usable = 0;
    else strlist_from_array(&p->etypes, x);
  }
  p->has_ent = (p->eids.n || p->etypes.n);

  cJSON_Delete(root);
  return p->usable;
}

/* ── item facts (everything the non-FTS terms need) ───────────────────────── */

typedef struct {
  sqlite3    *h;              /* for the lazy entity lookup; may be NULL */
  const char *uid;            /* borrowed from the live sqlite3_stmt row */
  const char *tenant;         /* borrowed; scopes the entities join      */
  const char *source_id;      /* borrowed                                */
  const char *tags_json;      /* borrowed; parsed lazily                 */
  int         has_geo; double lat, lon;
  int         tags_ready;
  strlist     tags;           /* owned → facts_clear()                   */
  int         ents_ready;
  strlist     eids, etypes;   /* owned → facts_clear()                   */
} item_facts;

static void facts_clear(item_facts *f) {
  if (f->tags_ready) strlist_free(&f->tags);
  if (f->ents_ready) { strlist_free(&f->eids); strlist_free(&f->etypes); }
  f->tags_ready = 0; f->ents_ready = 0;
}
/* Parsed on first use only: most rules have no tag term, and the ingest path
 * pays this per item, not per rule. */
static const strlist *facts_tags(item_facts *f) {
  if (!f->tags_ready) {
    strlist_from_json_text(&f->tags, f->tags_json);
    f->tags_ready = 1;
  }
  return &f->tags;
}
/* Item 21. The item's mention set, loaded at most ONCE per item however many
 * rules ask for it, and never at all unless some rule has an entity term.
 * Rides idx_em_item.
 *
 * The join to `entities` is what scopes the term: a tenant's rule can only
 * see its own entities plus the shared/global ones (tenant_id IS NULL, which
 * is what the collector NER pod writes). Without it, one tenant could name a
 * private entity id belonging to another and learn from the alert whether it
 * appears in their corpus. */
static void facts_entities(item_facts *f) {
  if (f->ents_ready) return;
  f->ents_ready = 1;
  memset(&f->eids, 0, sizeof f->eids);
  memset(&f->etypes, 0, sizeof f->etypes);
  if (!f->h || !f->uid || !*f->uid) return;
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(f->h,
        "SELECT m.entity_id, e.type FROM entity_mentions m"
        " JOIN entities e ON e.entity_id = m.entity_id"
        " WHERE m.item_uid = ?1 AND (e.tenant_id IS NULL OR e.tenant_id = ?2)"
        " LIMIT ?3", -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, f->uid, -1, SQLITE_TRANSIENT);
  if (f->tenant && *f->tenant)
    sqlite3_bind_text(s, 2, f->tenant, -1, SQLITE_TRANSIENT);
  else
    sqlite3_bind_null(s, 2);
  sqlite3_bind_int(s, 3, FACT_ENT_CAP);
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *id = ctext(s, 0), *ty = ctext(s, 1);
    if (id && !strlist_has(&f->eids, id))   strlist_push(&f->eids, id);
    if (ty && !strlist_has(&f->etypes, ty)) strlist_push(&f->etypes, ty);
  }
  sqlite3_finalize(s);
}
static void facts_from_row(item_facts *f, sqlite3_stmt *s, sqlite3 *h,
                           const char *tenant, int c_uid, int c_src,
                           int c_lat, int c_lon, int c_tags) {
  memset(f, 0, sizeof *f);
  f->h         = h;
  f->tenant    = tenant;
  f->uid       = ctext(s, c_uid);
  f->source_id = ctext(s, c_src);
  f->tags_json = ctext(s, c_tags);
  if (sqlite3_column_type(s, c_lat) != SQLITE_NULL &&
      sqlite3_column_type(s, c_lon) != SQLITE_NULL) {
    f->has_geo = 1;
    f->lat = sqlite3_column_double(s, c_lat);
    f->lon = sqlite3_column_double(s, c_lon);
  }
}

/* THE evaluator, minus `q` — AND semantics over every present term. Shared
 * verbatim by the live path and by preview; see the header.
 *
 * Term order is cost order: in-memory string sets, then the two-stage spatial
 * test, then the entity terms — the only ones that can touch the database. */
static int facts_match(const jo_pred *p, item_facts *f) {
  if (!p->usable || p->llm_only) return 0;
  if (p->src.n && !strlist_has(&p->src, f->source_id)) return 0;
  if (p->tany.n || p->tall.n) {
    const strlist *t = facts_tags(f);
    if (p->tany.n && !strlist_any(&p->tany, t)) return 0;
    for (int i = 0; i < p->tall.n; i++)
      if (!strlist_has(t, p->tall.v[i])) return 0;
  }
  if (p->geo.kind != SH_NONE) {
    if (!f->has_geo) return 0;             /* no coordinates → never in a shape */
    if (!shape_contains(&p->geo, f->lat, f->lon)) return 0;
  }
  if (p->has_ent) {
    facts_entities(f);
    if (p->eids.n   && !strlist_any(&p->eids,   &f->eids))   return 0;
    if (p->etypes.n && !strlist_any(&p->etypes, &f->etypes)) return 0;
  }
  return 1;
}

/* Does THIS uid satisfy the FTS term? Seeks the item's fts rowid through
 * intel_items_fts_uid_map instead of filtering on the (UNINDEXED, therefore
 * unsearchable) uid column, so a common word doesn't drag its whole doclist
 * into memory. A malformed MATCH expression fails the step and reports "no
 * match" — a rule whose q the user mistyped must not fire on everything. */
static int fts_probe(sqlite3 *h, const char *q, const char *uid) {
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(h,
        "SELECT 1 FROM intel_items_fts"
        " WHERE intel_items_fts MATCH ?1"
        "   AND rowid = (SELECT rowid FROM intel_items_fts_uid_map WHERE uid=?2)"
        " LIMIT 1", -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, q, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, uid, -1, SQLITE_TRANSIENT);
  int hit = sqlite3_step(s) == SQLITE_ROW;
  sqlite3_finalize(s);
  return hit;
}

/* ── refcounted rule-cache snapshot ───────────────────────────────────────── */

typedef struct { char *id; char *tenant_id; jo_pred p; } rule_c;
typedef struct { int refs; int n; rule_c *v; int any_ent; } rule_set;

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static rule_set       *g_set;          /* current snapshot; holds one ref  */
static char            g_gen[256];     /* generation string of g_set       */
static time_t          g_checked;      /* CLOCK_MONOTONIC secs of last probe */
static int             g_checked_set;

static void ruleset_free(rule_set *rs) {
  if (!rs) return;
  for (int i = 0; i < rs->n; i++) {
    free(rs->v[i].id); free(rs->v[i].tenant_id); pred_free(&rs->v[i].p);
  }
  free(rs->v); free(rs);
}

/* Cheap change detector. MAX(updated_at) catches edits (every mutation in
 * alertsapi.c stamps it), COUNT(*) catches create/delete, and MAX(id) catches
 * the pathological delete+create inside the same one-second tick that would
 * otherwise leave both of the others unchanged.
 *
 * areas_of_interest is probed the same way and as a SEPARATE statement: rules
 * now cache a resolved AOI geometry, so editing a shape must invalidate them,
 * and running it separately means a database that predates the table (prepare
 * fails) simply loses AOI-change detection instead of pinning the whole cache. */
static void read_generation(db_handle *db, char *out, size_t n) {
  static const char *Q[2] = {
    "SELECT COALESCE(MAX(updated_at),'') || '|' || COUNT(*) || '|' ||"
    " COALESCE(MAX(id),'') FROM alert_rules",
    "SELECT COALESCE(MAX(COALESCE(updated_at,created_at)),'') || '|' ||"
    " COUNT(*) || '|' || COALESCE(MAX(id),'') FROM areas_of_interest"
  };
  out[0] = '\0';
  size_t used = 0;
  for (int i = 0; i < 2; i++) {
    const char *g = "";
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db->h, Q[i], -1, &s, NULL) == SQLITE_OK) {
      if (sqlite3_step(s) == SQLITE_ROW) {
        const char *v = ctext(s, 0);
        if (v) g = v;
      }
      int w = snprintf(out + used, n - used, "%s%s", i ? "#" : "", g);
      if (w > 0) used += (size_t)w;
      if (used >= n) used = n - 1;
    }
    sqlite3_finalize(s);
  }
}

static rule_set *ruleset_load(db_handle *db) {
  rule_set *rs = calloc(1, sizeof *rs);
  if (!rs) return NULL;
  rs->refs = 1;
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT id,tenant_id,predicate_json FROM alert_rules WHERE enabled=1",
        -1, &s, NULL) != SQLITE_OK) return rs;      /* empty set, not a crash */
  int cap = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *id = ctext(s, 0), *tid = ctext(s, 1);
    if (!id || !tid) continue;
    if (rs->n == cap) {
      int ncap = cap ? cap * 2 : 16;
      rule_c *nv = realloc(rs->v, (size_t)ncap * sizeof *nv);
      if (!nv) break;
      rs->v = nv; cap = ncap;
    }
    rule_c *r = &rs->v[rs->n];
    memset(r, 0, sizeof *r);
    r->id = strdup(id); r->tenant_id = strdup(tid);
    if (!r->id || !r->tenant_id) { free(r->id); free(r->tenant_id); continue; }
    /* Predicates are compiled ONCE here — the MeCab segmentation of q, the
     * vertex ring and bounding box of a polygon, and the tenant-scoped lookup
     * of an aoi_id are all far too expensive to redo per ingested row.
     * predicate_json is read from the same row as tenant_id, so the AOI is
     * always resolved under the tenant that owns the rule. */
    pred_compile(db, tid, ctext(s, 2), &r->p);
    if (r->p.has_ent) rs->any_ent = 1;
    rs->n++;
  }
  sqlite3_finalize(s);
  return rs;
}

/* Hand out the current snapshot with a reference. Reloading happens under the
 * mutex; evaluation deliberately does not, so a slow FTS probe on one ingest
 * thread never blocks the others. */
static rule_set *ruleset_acquire(db_handle *db) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  pthread_mutex_lock(&g_mu);
  if (!g_set || !g_checked_set || ts.tv_sec - g_checked >= CACHE_RECHECK_SEC) {
    g_checked = ts.tv_sec; g_checked_set = 1;
    char gen[256];
    read_generation(db, gen, sizeof gen);
    if (!g_set || strcmp(gen, g_gen) != 0) {
      rule_set *ns = ruleset_load(db);
      if (ns) {
        rule_set *old = g_set;
        g_set = ns;
        snprintf(g_gen, sizeof g_gen, "%s", gen);
        if (old && --old->refs == 0) ruleset_free(old);
      }
    }
  }
  rule_set *rs = g_set;
  if (rs) rs->refs++;
  pthread_mutex_unlock(&g_mu);
  return rs;
}
static void ruleset_release(rule_set *rs) {
  if (!rs) return;
  pthread_mutex_lock(&g_mu);
  if (--rs->refs == 0) ruleset_free(rs);
  pthread_mutex_unlock(&g_mu);
}
static int ruleset_has_tenant(const rule_set *rs, const char *tid) {
  for (int i = 0; i < rs->n; i++)
    if (strcmp(rs->v[i].tenant_id, tid) == 0) return 1;
  return 0;
}

/* ── firing (suppression + the alert_events write) ────────────────────────── */

/* Precedence is muted → dedup → storm cap, and only the last of the three
 * still writes a row: a storm must stay VISIBLE (suppressed=1,
 * reason='storm_cap') so an operator can see the rule that ran away, whereas
 * a mute or a dedup hit is a non-event by definition. */
static void fire(db_handle *db, const char *rule_id, const char *tenant_id,
                 const char *uid) {
  sqlite3 *h = db->h;
  sqlite3_stmt *s = NULL;

  /* Re-read live, not from the cache: a mute issued five seconds ago must
   * hold now, and this path only runs on an actual match. */
  if (sqlite3_prepare_v2(h,
        "SELECT enabled, dedup_window_sec, storm_cap_per_hour,"
        " COALESCE(datetime(muted_until) > datetime('now'),0)"
        " FROM alert_rules WHERE id=?1 AND tenant_id=?2", -1, &s, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_text(s, 1, rule_id,  -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant_id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) { sqlite3_finalize(s); return; }
  int  enabled = sqlite3_column_int(s, 0);
  long dedup   = (long)sqlite3_column_int64(s, 1);
  long cap     = (long)sqlite3_column_int64(s, 2);
  int  muted   = sqlite3_column_int(s, 3);
  sqlite3_finalize(s);
  if (!enabled || muted) return;

  if (dedup > 0) {
    char win[48];
    snprintf(win, sizeof win, "-%ld seconds", dedup);
    if (sqlite3_prepare_v2(h,
          "SELECT 1 FROM alert_events WHERE rule_id=?1 AND tenant_id=?2"
          " AND item_uid=?3 AND matched_at >= datetime('now',?4) LIMIT 1",
          -1, &s, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(s, 1, rule_id,   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, tenant_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, uid,       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 4, win,       -1, SQLITE_TRANSIENT);
    int seen = sqlite3_step(s) == SQLITE_ROW;
    sqlite3_finalize(s);
    if (seen) return;
  }

  /* The cap counts DELIVERABLE events (suppressed=0). Counting the suppressed
   * rows too would make the cap self-sustaining: the storm markers alone would
   * hold the rule over its limit for a further hour. */
  int capped = 0;
  if (cap > 0) {
    if (sqlite3_prepare_v2(h,
          "SELECT COUNT(*) FROM alert_events WHERE rule_id=?1 AND tenant_id=?2"
          " AND suppressed=0 AND matched_at >= datetime('now','-1 hour')",
          -1, &s, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(s, 1, rule_id,   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, tenant_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW &&
        (long)sqlite3_column_int64(s, 0) >= cap) capped = 1;
    sqlite3_finalize(s);
  }

  char eid[37]; uuid4(eid);
  if (sqlite3_prepare_v2(h,
        "INSERT INTO alert_events (id,tenant_id,rule_id,item_uid,matched_at,"
        "delivered_channels_json,suppressed,reason)"
        " VALUES (?1,?2,?3,?4,datetime('now'),'[]',?5,?6)",
        -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, eid,       -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, tenant_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, rule_id,   -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 4, uid,       -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (s, 5, capped);
  if (capped) sqlite3_bind_text(s, 6, "storm_cap", -1, SQLITE_TRANSIENT);
  else        sqlite3_bind_null(s, 6);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

/* ── public: live path ────────────────────────────────────────────────────── */

void alert_eval_init(db_handle *db) {
  if (!db || !db->h) return;
  ruleset_release(ruleset_acquire(db));
}

/* Evaluate ONE item against a held snapshot. The single place a rule and an
 * item meet — the ingest path and the entity sweep both come through here, so
 * there is no second matcher to drift.
 *
 * `entity_only` restricts the pass to rules that carry an entity term. The
 * sweep sets it because every other rule already had its chance at ingest;
 * re-running them here would double-fire any rule with dedup_window_sec=0. */
static void eval_item(db_handle *db, rule_set *rs, const char *tenant_hint,
                      const char *uid, int entity_only) {
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT uid,source_id,lat,lon,tags,tenant_id FROM intel_items"
        " WHERE uid=?1", -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) { sqlite3_finalize(s); return; }

  /* The stored tenant_id wins over the caller's hint: rules are selected by
   * the tenant that actually owns the row, so no argument mix-up upstream can
   * show one tenant's items to another's alert rule. It is also what makes
   * the sweep's tenant-less watermark safe. */
  const char *row_tid = ctext(s, 5);
  const char *tid = (row_tid && *row_tid) ? row_tid
                  : ((tenant_hint && *tenant_hint) ? tenant_hint : "legacy");

  item_facts f;
  facts_from_row(&f, s, db->h, tid, 0, 1, 2, 3, 4);

  for (int i = 0; i < rs->n; i++) {
    const rule_c *r = &rs->v[i];
    if (strcmp(r->tenant_id, tid) != 0) continue;
    if (entity_only && !r->p.has_ent) continue;
    if (!facts_match(&r->p, &f)) continue;            /* in-memory terms first */
    if (r->p.fts_q && !fts_probe(db->h, r->p.fts_q, uid)) continue;
    fire(db, r->id, tid, uid);
  }

  facts_clear(&f);
  sqlite3_finalize(s);      /* keeps f's borrowed column pointers valid above */
}

void alert_eval_on_item(db_handle *db, const char *tenant_id,
                        const char *item_uid) {
  if (!db || !db->h || !item_uid || !*item_uid) return;
  rule_set *rs = ruleset_acquire(db);
  if (!rs) return;
  /* The overwhelmingly common case — a tenant with no enabled rules — costs
   * one mutex and a strcmp loop, and touches the database not at all. */
  if (rs->n == 0 ||
      (tenant_id && *tenant_id && !ruleset_has_tenant(rs, tenant_id))) {
    ruleset_release(rs); return;
  }
  eval_item(db, rs, tenant_id, item_uid, 0);
  ruleset_release(rs);
}

/* ── public: item 21, the entity-mention reconciling sweep ────────────────── */

/* The watermark is (created_at, rowid). created_at alone is second-resolution
 * and not unique, so advancing on it would either re-scan or skip a second's
 * worth of mentions; SQLite orders index entries by (key…, rowid), so pairing
 * it with rowid gives a total order that idx_em_created already produces. */
static int cursor_read(db_handle *db, char *ts, size_t ts_n, long long *rid) {
  ts[0] = '\0'; *rid = 0;
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT v FROM alert_eval_cursor WHERE k=?1", -1, &s, NULL) != SQLITE_OK)
    return 0;
  sqlite3_bind_text(s, 1, SWEEP_CURSOR_KEY, -1, SQLITE_TRANSIENT);
  int got = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *v = ctext(s, 0);
    if (v) {
      const char *bar = strchr(v, '|');
      if (bar && bar > v) {
        size_t tn = (size_t)(bar - v);
        if (tn < ts_n) { memcpy(ts, v, tn); ts[tn] = '\0'; *rid = atoll(bar + 1); got = 1; }
      }
    }
  }
  sqlite3_finalize(s);
  return got;
}
static void cursor_write(db_handle *db, const char *ts, long long rid) {
  char v[64];
  snprintf(v, sizeof v, "%s|%lld", ts, rid);
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO alert_eval_cursor (k,v,updated_at)"
        " VALUES (?1,?2,datetime('now'))"
        " ON CONFLICT(k) DO UPDATE SET v=excluded.v,"
        " updated_at=excluded.updated_at", -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, SWEEP_CURSOR_KEY, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, v, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
}

int alert_eval_sweep_entities(db_handle *db) {
  if (!db || !db->h) return 0;

  /* Upper bound of this pass, lagged into the past — see SWEEP_LAG_SEC. */
  char hi[32] = {0};
  {
    char lag[24];
    snprintf(lag, sizeof lag, "-%d seconds", SWEEP_LAG_SEC);
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db->h,
          "SELECT strftime('%Y-%m-%d %H:%M:%S', datetime('now',?1))",
          -1, &s, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(s, 1, lag, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *v = ctext(s, 0);
      if (v) snprintf(hi, sizeof hi, "%s", v);
    }
    sqlite3_finalize(s);
  }
  if (!hi[0]) return 0;

  char lo[32]; long long lo_rid = 0;
  if (!cursor_read(db, lo, sizeof lo, &lo_rid) || !lo[0]) {
    /* First pass in this database. Start watching NOW rather than replaying
     * however much history the corpus holds: the live path never backfires
     * either, and a watchlist that retro-fires on a year of archive is an
     * inbox no one will open again. POST /api/alerts/preview answers "what
     * would this have matched" without writing anything. */
    cursor_write(db, hi, SWEEP_ROWID_MAX);
    return 0;
  }
  if (strcmp(lo, hi) >= 0 && lo_rid == SWEEP_ROWID_MAX) return 0;

  rule_set *rs = ruleset_acquire(db);
  if (!rs || !rs->any_ent) {
    /* Nothing watches entities right now. Advance anyway: leaving a backlog
     * would turn the FIRST watchlist someone creates into a retro-storm over
     * every mention written since the process started. */
    if (rs) ruleset_release(rs);
    cursor_write(db, hi, SWEEP_ROWID_MAX);
    return 0;
  }

  int seen = 0, drained = 0;
  char cur_ts[32]; snprintf(cur_ts, sizeof cur_ts, "%s", lo);
  long long cur_rid = lo_rid;

  for (int b = 0; b < SWEEP_MAX_BATCH; b++) {
    /* Keyset over (created_at, rowid). The created_at range is written as two
     * plain comparisons so idx_em_created drives the scan; the rowid clause is
     * only a tiebreak filter inside the first second. */
    sqlite3_stmt *q = NULL;
    if (sqlite3_prepare_v2(db->h,
          "SELECT item_uid, created_at, rowid FROM entity_mentions"
          " WHERE created_at >= ?1 AND created_at <= ?2"
          "   AND (created_at > ?1 OR rowid > ?3)"
          " ORDER BY created_at ASC, rowid ASC LIMIT ?4",
          -1, &q, NULL) != SQLITE_OK) break;
    sqlite3_bind_text (q, 1, cur_ts, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (q, 2, hi,     -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(q, 3, cur_rid);
    sqlite3_bind_int  (q, 4, SWEEP_BATCH);

    /* Read the whole batch out before evaluating anything: eval_item() runs
     * its own statements (and fire() writes alert_events) on this same
     * connection, and stepping a cursor across those writes is exactly the
     * interleaving that makes a scan skip rows. */
    strlist uids; memset(&uids, 0, sizeof uids);
    int rows = 0;
    char last_ts[32] = {0}; long long last_rid = cur_rid;
    while (sqlite3_step(q) == SQLITE_ROW) {
      const char *u = ctext(q, 0), *ts = ctext(q, 1);
      if (!u || !ts) continue;
      rows++;
      snprintf(last_ts, sizeof last_ts, "%s", ts);
      last_rid = sqlite3_column_int64(q, 2);
      /* One item usually arrives with a dozen mentions in the same batch;
       * collapsing them here saves a dozen identical evaluations. Across
       * batches a repeat is possible and harmless — it is the same re-fire
       * the re-ingest path already relies on dedup_window_sec to collapse. */
      if (!strlist_has(&uids, u)) strlist_push(&uids, u);
    }
    sqlite3_finalize(q);

    for (int i = 0; i < uids.n; i++) eval_item(db, rs, NULL, uids.v[i], 1);
    seen += uids.n;
    strlist_free(&uids);

    if (rows) { snprintf(cur_ts, sizeof cur_ts, "%s", last_ts); cur_rid = last_rid; }
    if (rows < SWEEP_BATCH) { drained = 1; break; }
  }
  ruleset_release(rs);

  /* Only claim the whole window when the whole window was actually consumed;
   * otherwise persist exactly how far we got and resume there next tick. */
  if (drained) cursor_write(db, hi, SWEEP_ROWID_MAX);
  else         cursor_write(db, cur_ts, cur_rid);
  return seen;
}

static pthread_t g_sweep_thread;
static int       g_sweep_running;
/* _Atomic, not volatile — see the note on g_gc_stop in core/evidence.c: TSan
 * reports the stop/poll pair as a data race (core/alert_eval.c:1027 vs :1053)
 * and volatile provides neither atomicity nor ordering. */
static _Atomic int g_sweep_stop;

/* Own connection: a sqlite transaction belongs to a connection, not a thread,
 * so sharing the server handle let this sweep's writes interleave with the
 * event loop's and the scheduler's transactions. */
static void *sweep_thread(void *arg) {
  (void)arg;
  db_handle own = {0};
  if (db_attach(&own, NULL) != 0) {
    fprintf(stderr, "[alert-sweep] cannot open its own DB connection; sweep off\n");
    return NULL;
  }
  int interval = SWEEP_DEFAULT_SEC;
  const char *e = getenv("JO_ALERT_SWEEP_SEC");
  if (e && *e) { int v = atoi(e); if (v > 0) interval = v; }
  while (!g_sweep_stop) {
    alert_eval_sweep_entities(&own);
    for (int i = 0; i < interval && !g_sweep_stop; i++) sleep(1);
  }
  db_close(&own);
  return NULL;
}

void alert_eval_sweep_start(db_handle *db) {
  if (g_sweep_running) return;
  if (!db || !db->h) return;
  if (getenv("JO_NO_ALERT_SWEEP")) {
    fprintf(stderr, "[alert-sweep] disabled (JO_NO_ALERT_SWEEP); "
                    "entity watchlists will not fire\n");
    return;
  }
  g_sweep_stop = 0;
  if (pthread_create(&g_sweep_thread, NULL, sweep_thread, db) == 0) {
    g_sweep_running = 1;
    fprintf(stderr, "[alert-sweep] entity-mention sweep started\n");
  } else {
    fprintf(stderr, "[alert-sweep] pthread_create failed; "
                    "entity watchlists will not fire\n");
  }
}

void alert_eval_sweep_stop(void) {
  if (!g_sweep_running) return;
  g_sweep_stop = 1;
  pthread_join(g_sweep_thread, NULL);
  g_sweep_running = 0;
  fprintf(stderr, "[alert-sweep] entity-mention sweep stopped\n");
}

/* ── public: preview / backtest ───────────────────────────────────────────── */

static char *preview_envelope(int *status, long matches, long scanned,
                              int truncated, const char *since_used,
                              const char *skipped, cJSON *samples) {
  cJSON *w = cJSON_CreateObject();
  cJSON_AddItemToObject(w, "data", samples ? samples : cJSON_CreateArray());
  cJSON *pg = cJSON_CreateObject();
  cJSON_AddNumberToObject(pg, "limit", PREVIEW_SAMPLES);
  cJSON_AddBoolToObject(pg, "has_more", matches > PREVIEW_SAMPLES);
  cJSON_AddItemToObject(w, "page", pg);
  cJSON *m = cJSON_CreateObject();
  cJSON_AddNumberToObject(m, "match_count", (double)matches);
  cJSON_AddNumberToObject(m, "scanned", (double)scanned);
  cJSON_AddBoolToObject(m, "truncated", truncated);
  cJSON_AddStringToObject(m, "since", since_used ? since_used : "");
  cJSON_AddNumberToObject(m, "max_window_days", PREVIEW_MAX_DAYS);
  cJSON_AddBoolToObject(m, "writes_events", 0);
  cJSON_AddItemToObject(m, "skipped",
    skipped ? cJSON_CreateString(skipped) : cJSON_CreateNull());
  cJSON_AddItemToObject(w, "meta", m);
  char *out = cJSON_PrintUnformatted(w);
  cJSON_Delete(w);
  *status = 200;
  return out ? out : strdup("{\"data\":[]}");
}

char *alert_eval_preview(db_handle *db, const char *tenant_id,
                         const char *predicate_json, const char *since,
                         int *status) {
  if (!db || !db->h || !tenant_id || !*tenant_id)
    return err(status, 400, "tenant_required");

  jo_pred p;
  /* Same tenant the request resolved to, so a preview can no more geofence on
   * another tenant's AOI than a live rule can. */
  if (!pred_compile(db, tenant_id, predicate_json, &p)) {
    pred_free(&p);
    return err(status, 400, "invalid_predicate");
  }

  /* Clamp the window in SQL and emit it in the writer's exact timestamp shape
   * ("…THH:MM:SS.sssZ"), so the comparison against intel_items.fetched_at is a
   * plain indexed string range — datetime(fetched_at) per row would be correct
   * but would forfeit idx_intel_items_tenant_fetched. */
  char bound[64]; bound[0] = '\0';
  int bad_since = 0;
  char maxwin[32];
  snprintf(maxwin, sizeof maxwin, "-%d days", PREVIEW_MAX_DAYS);
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT ?1 IS NOT NULL AND datetime(?1) IS NULL,"
        " strftime('%Y-%m-%dT%H:%M:%fZ',"
        "   MAX(COALESCE(datetime(?1), datetime('now','-7 days')),"
        "       datetime('now',?2)))", -1, &s, NULL) == SQLITE_OK) {
    if (since && *since) sqlite3_bind_text(s, 1, since, -1, SQLITE_TRANSIENT);
    else                 sqlite3_bind_null(s, 1);
    sqlite3_bind_text(s, 2, maxwin, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      bad_since = sqlite3_column_int(s, 0);
      const char *b = ctext(s, 1);
      if (b) snprintf(bound, sizeof bound, "%s", b);
    }
    sqlite3_finalize(s);
  }
  if (bad_since) { pred_free(&p); return err(status, 400, "invalid_since"); }
  if (!bound[0]) { pred_free(&p); return err(status, 500, "server_error"); }

  if (p.llm_only) {
    /* Same verdict the live path reaches, for the same reason — reporting a
     * match count computed from a predicate MINUS its nl_query would be a
     * preview of a rule that does not exist. */
    pred_free(&p);
    return preview_envelope(status, 0, 0, 0, bound,
                            "llm_mode_unsupported", NULL);
  }

  /* Same eight columns, same order, both drivers: with q, FTS drives and the
   * MATCH string is the very one the live path probes with; without q, the
   * tenant+window index drives. Everything after this point is identical. */
  static const char *SQL_FTS =
    "SELECT intel_items.uid, intel_items.source_id, intel_items.title,"
    " intel_items.link, intel_items.published_at, intel_items.lat,"
    " intel_items.lon, intel_items.tags"
    " FROM intel_items"
    " JOIN intel_items_fts ON intel_items_fts.uid = intel_items.uid"
    " WHERE intel_items_fts MATCH ?1 AND intel_items.tenant_id=?2"
    "   AND intel_items.fetched_at >= ?3"
    " ORDER BY intel_items.fetched_at DESC LIMIT ?4";
  static const char *SQL_PLAIN =
    "SELECT uid, source_id, title, link, published_at, lat, lon, tags"
    " FROM intel_items WHERE tenant_id=?1 AND fetched_at >= ?2"
    " ORDER BY fetched_at DESC LIMIT ?3";

  s = NULL;
  if (sqlite3_prepare_v2(db->h, p.fts_q ? SQL_FTS : SQL_PLAIN, -1, &s, NULL)
        != SQLITE_OK) {
    pred_free(&p);
    return err(status, 500, "server_error");
  }
  int i = 1;
  if (p.fts_q) sqlite3_bind_text(s, i++, p.fts_q, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, i++, tenant_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, i++, bound,     -1, SQLITE_TRANSIENT);
  sqlite3_bind_int (s, i,   PREVIEW_SCAN_CAP);

  cJSON *samples = cJSON_CreateArray();
  long matches = 0, scanned = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    scanned++;
    item_facts f;
    /* uid is column 0 in BOTH drivers, which is what lets the entity terms
     * (the only ones that need to go back to the database per row) work
     * unchanged in preview. */
    facts_from_row(&f, s, db->h, tenant_id, 0, 1, 5, 6, 7);
    int hit = facts_match(&p, &f);
    facts_clear(&f);
    if (!hit) continue;
    matches++;
    if (matches <= PREVIEW_SAMPLES) {
      cJSON *o = cJSON_CreateObject();
      const char *uid = ctext(s,0), *src = ctext(s,1), *ti = ctext(s,2),
                 *lk = ctext(s,3), *pb = ctext(s,4);
      cJSON_AddStringToObject(o, "uid", uid ? uid : "");
      cJSON_AddItemToObject(o, "source_id",
        src ? cJSON_CreateString(src) : cJSON_CreateNull());
      cJSON_AddItemToObject(o, "title",
        ti ? cJSON_CreateString(ti) : cJSON_CreateNull());
      cJSON_AddItemToObject(o, "link",
        lk ? cJSON_CreateString(lk) : cJSON_CreateNull());
      cJSON_AddItemToObject(o, "published_at",
        pb ? cJSON_CreateString(pb) : cJSON_CreateNull());
      cJSON_AddItemToArray(samples, o);
    }
  }
  sqlite3_finalize(s);
  pred_free(&p);
  return preview_envelope(status, matches, scanned,
                          scanned >= PREVIEW_SCAN_CAP, bound, NULL, samples);
}
